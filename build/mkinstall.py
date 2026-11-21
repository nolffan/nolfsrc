#!/usr/bin/env python3
"""The install scripts of a game directory's public archive.

NOLF1's own (build/mkinstall.py is NOLF2's, for its InstallShield
cabinets and update).

  build/mkinstall.py <gamedir> <outdir>

A game directory holds the retail game's data as well as what we built: the
four .rez archives, the movies, the text and config files of the play disc,
and the retail Miles and Bink files (legacy builds). Our game DLLs, in
custom/ or the modern build's nolfsrc.rez, ship in the archive, and so do
the modern build's own defaults.cfg and defctrls.cfg and the retail files it
changes (assets/modern/; their manifest rows do not name the discs). The
public archive (build/package.sh) leaves the discs' data out, and three
scripts put it back from the player's own copies of the two discs -- the Game of the Year
Edition (Europe), as .iso images or as raw .bin/.cue images (or a .zip of
one), which the scripts convert to the .iso first:

  linux-install.sh       bash, dd, sha256sum, base64 (coreutils)
  windows-install.bat    cmd running the PowerShell it carries (5.1, as every
                         Windows 10 and 11 has)
  mac-install.command    the Perl every Mac has, with its own modules (for
                         CrossOver or another Wine: the .bat needs PowerShell,
                         which Wine does not have)

Neither reads the ISO 9660 filesystem. A disc is recognised by its size and
SHA-256 (those of the .iso: a .bin is converted, then checked), and then
every file is known: this writes each one as a list of
byte ranges of the disc images --

  C <disc> <offset> <length>   bytes of that disc image
  Z <length>                   zeros (the retail game DLLs blanked inside the
                               .rez archives: build/all.sh's `resources`)
  L <base64>                   bytes of ours (the modern build's
                               autoexec.cfg, its defaults.cfg)

-- and the file's size and SHA-256, which the script checks before it moves
the file into place. So the result is the game directory as built, byte for
byte. The ranges come from build/retail-files.txt, the list build/discs.py
installs the retail game from (where each retail file is on which disc),
and are worked out against that install (out/retail/nolf-eu-goty/, or
$NOLF_RETAIL): this script needs no disc image. The list goes at the end of each script, after a marker line (in the
Perl one, after its __END__); the scripts' code is
build/game-scripts/linux-install.sh, windows-install.ps1 (the .bat's header
is written here) and mac-install.command.

Writes <outdir>/linux-install.sh, <outdir>/windows-install.bat,
<outdir>/mac-install.command,
<outdir>/disc-files.txt, the game directory's paths the scripts write (the
archive leaves them out), and <outdir>/disc-sums.txt, the SHA-256 of each of
those but autoexec.cfg, which the engine rewrites (build/package.sh puts them
in the archive's SHA256SUMS). Exits 3 if the retail game is not installed.
"""
import base64
import difflib
import hashlib
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "ext", "tools"))
sys.path.insert(0, os.path.join(REPO, "build"))
from rezextract import Rez  # noqa: E402
import discs as disclist  # noqa: E402

RETAIL = os.environ.get("NOLF_RETAIL") or os.path.join(REPO, "out", "retail", "nolf-eu-goty")
SCRIPTS = os.path.join(REPO, "build", "game-scripts")
SECTOR = 2048
MARKER = "__NOLF_INSTALL_DATA__"
CHUNK = 1 << 20
RAW_SECTOR = 2352
LITERAL_LINE = 3 * 1024          # raw bytes per L record (4 KB of base64)
LITERAL_MAX = 1 << 20            # more than this of ours means a wrong source


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(CHUNK), b""):
            h.update(b)
    return h.hexdigest()


class Source:
    """A retail file's bytes: one range of one disc image (`disc`, its
    number; `off`, the byte offset in its .iso), read from the installed
    copy of the file (`path`, from `base` on): the same bytes, which
    build/discs.py checked."""
    def __init__(self, disc, off, size, path, base=0):
        self.disc, self.off, self.size = disc, off, size
        self.path, self.base = path, base

    def read(self, pos=0, n=None):
        with open(self.path, "rb") as f:
            f.seek(self.base + pos)
            return f.read(self.size - pos if n is None else n)


class Ops:
    def __init__(self):
        self.ops, self.literal = [], 0

    def copy(self, src, pos, n):
        if n <= 0:
            return
        off = src.off + pos
        last = self.ops[-1] if self.ops else None
        if last and last[0] == "C" and last[1] == src.disc and last[2] + last[3] == off:
            last[3] += n
        else:
            self.ops.append(["C", src.disc, off, n])

    def bytes(self, b):
        if not b:
            return
        if b.count(0) == len(b):
            if self.ops and self.ops[-1][0] == "Z":
                self.ops[-1][1] += len(b)
            else:
                self.ops.append(["Z", len(b)])
            return
        self.literal += len(b)
        if self.ops and self.ops[-1][0] == "L":
            self.ops[-1][1] += b
        else:
            self.ops.append(["L", bytearray(b)])

    def lines(self):
        for op in self.ops:
            if op[0] == "L":
                b = bytes(op[1])
                for i in range(0, len(b), LITERAL_LINE):
                    yield "L " + base64.b64encode(b[i:i + LITERAL_LINE]).decode()
            else:
                yield " ".join(str(x) for x in op)


def diff_runs(a, b):
    """(start, end) of the runs where equal-length a and b differ; runs
    closer than 16 bytes are joined (one literal, not three records)."""
    runs, i, n = [], 0, len(a)
    while i < n:
        if a[i] == b[i]:
            i += 1
            continue
        j = i
        while j < n and a[j] != b[j]:
            j += 1
        if runs and i - runs[-1][1] < 16:
            runs[-1][1] = j
        else:
            runs.append([i, j])
        i = j
    return runs


def gap(ops, src, pos, out, s, e):
    """Bytes the file shares with the retail one: copied, unless they are
    zeros next to zeros we write (a blanked DLL's own zero bytes), which
    join that run instead of splitting it."""
    seg = out[s:e]
    if seg and ops.ops and ops.ops[-1][0] == "Z" and seg.count(0) == len(seg):
        ops.bytes(seg)
    else:
        ops.copy(src, pos + s, e - s)


def ops_same_size(src, path):
    with open(path, "rb") as f:
        return ops_same_chunks(src, iter(lambda: f.read(CHUNK), b""))


def ops_same_chunks(src, chunks):
    """A file as long as its retail one, given as successive chunks."""
    ops = Ops()
    pos = 0
    for out in chunks:
        if pos >= src.size:
            break
        ref = src.read(pos, len(out))
        if out == ref:
            ops.copy(src, pos, len(out))
        elif out.count(0) == len(out):
            ops.bytes(out)
        else:
            done = 0
            for s, e in diff_runs(ref, out):
                gap(ops, src, pos, out, done, s)
                ops.bytes(out[s:e])
                done = e
            gap(ops, src, pos, out, done, len(out))
        pos += len(out)
    return ops


def ops_text(src, out):
    """A patched text file (its bytes): its lines matched against the
    retail file's."""
    ref = src.read()
    a, b = ref.splitlines(keepends=True), out.splitlines(keepends=True)
    apos = [0]
    for line in a:
        apos.append(apos[-1] + len(line))
    ops = Ops()
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    bpos = [0]
    for line in b:
        bpos.append(bpos[-1] + len(line))
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            ops.copy(src, apos[i1], apos[i2] - apos[i1])
        else:
            ops.bytes(out[bpos[j1]:bpos[j2]])
    return ops


def ops_for(sources, path):
    """The cheapest description of the file at `path` from one of `sources`."""
    size = os.path.getsize(path)
    best = None
    for src in sources:
        if src.size == size:
            ops = ops_same_size(src, path)
        elif size < 16 << 20:
            ops = ops_text(src, open(path, "rb").read())
        else:
            continue
        if best is None or ops.literal < best.literal:
            best = ops
    return best


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    game, outdir = sys.argv[1].rstrip("/"), sys.argv[2]
    if not os.path.isfile(os.path.join(RETAIL, ".retail-files")):
        print(f"mkinstall: the retail game is not installed in {RETAIL} -- make retail "
              "(it needs your two disc images: docs/BUILDING.md)", file=sys.stderr)
        sys.exit(3)
    discs, files = disclist.load()

    # Where each retail file is on the discs (build/retail-files.txt).
    retail = {f.path.lower(): Source(f.disc, f.off, f.size, os.path.join(RETAIL, f.path))
              for f in files}

    # The four archives' entries, as ranges of the discs.
    entries = {}                          # archive (lower) -> {path (lower): Source}
    for rez in ("nolf.rez", "nolf2.rez", "nolfgoty.rez", "nolfu003.rez"):
        src = retail[rez]
        entries[rez] = {p.lower(): Source(src.disc, src.off + pos, size, src.path, pos)
                        for p, pos, size in Rez(os.path.join(RETAIL, next(
                            n for n in os.listdir(RETAIL) if n.lower() == rez))).walk()}

    # What in the game directory came from the discs, and from which file.
    wanted = {}                           # game path -> [Source, ...]
    for line in open(os.path.join(game, "MANIFEST.txt")):
        m = re.match(r"(data|THIRD-PARTY) nolf-eu-goty:(\S+).*\t(.+)$", line.rstrip("\n"))
        if m:
            wanted[m.group(3)] = [retail[m.group(2).lower()]]
    for rel, srcs in wanted.items():
        if not srcs:
            sys.exit(f"mkinstall: {rel}: no retail file to make it from")
        if re.search(r"[\s\"'`$\\%!]", rel):
            sys.exit(f"mkinstall: {rel}: a name the scripts do not quote")

    # autoexec.cfg is the shipped defaults.cfg (build/package.sh); the
    # scripts write it only if there is none, as the engine rewrites it.
    # The modern build's defaults.cfg is ours, in the archive: its
    # autoexec.cfg is then our bytes, not the discs'.
    records, literal, sums = [], 0, []
    if "defaults.cfg" not in wanted:
        cfg = open(os.path.join(game, "defaults.cfg"), "rb").read()
        ops = Ops()
        ops.bytes(cfg)
        literal += ops.literal
        records.append(f"F autoexec.cfg {len(cfg)} {hashlib.sha256(cfg).hexdigest()} once")
        records.extend(ops.lines())
        records.append("E")
    for rel in sorted(wanted, key=str.lower):
        path = os.path.join(game, rel)
        ops = ops_for(wanted[rel], path)
        if ops is None:
            sys.exit(f"mkinstall: {rel}: not made from its retail file")
        literal += ops.literal
        size, sha = os.path.getsize(path), sha256(path)
        sums.append(f"{sha}  {rel}")
        records.append(f"F {rel} {size} {sha} -")
        records.extend(ops.lines())
        records.append("E")
        if rel == "defaults.cfg":
            records.append(f"F autoexec.cfg {size} {sha} once")
            records.extend(ops.lines())
            records.append("E")
    if literal > LITERAL_MAX:
        sys.exit(f"mkinstall: {literal} bytes would ship as literal data")

    data = ["NOLFINSTALL 1"]
    for n, d in sorted(discs.items()):
        data.append(f"D {n} {d.size} {d.sha} {d.name}")
        # The raw .bin of the known image of the disc: one MODE1/2352 track, each 2048
        # bytes of the .iso a 2352-byte sector (the scripts convert it).
        data.append(f"R {n} {d.size // SECTOR * RAW_SECTOR}")
    data += records

    os.makedirs(outdir, exist_ok=True)
    body = open(os.path.join(SCRIPTS, "linux-install.sh")).read()
    with open(os.path.join(outdir, "linux-install.sh"), "w", newline="\n") as f:
        f.write(body + MARKER + "\n" + "\n".join(data) + "\n")
    os.chmod(os.path.join(outdir, "linux-install.sh"), 0o755)

    perl = open(os.path.join(SCRIPTS, "mac-install.command")).read()
    if not perl.endswith("\n__END__\n"):
        sys.exit("mkinstall: mac-install.command must end with __END__ (the list follows it)")
    with open(os.path.join(outdir, "mac-install.command"), "w", newline="\n") as f:
        f.write(perl + MARKER + "\n" + "\n".join(data) + "\n")
    os.chmod(os.path.join(outdir, "mac-install.command"), 0o755)

    ps = open(os.path.join(SCRIPTS, "windows-install.ps1")).read()
    if not ps.isascii() or "#__NOLF_PS__" in ps or MARKER in ps:
        sys.exit("mkinstall: windows-install.ps1 must be ASCII, without the markers")
    bat = BAT_HEADER + ["#__NOLF_PS__"] + ps.splitlines() + ["#__NOLF_PS__", MARKER] + data
    with open(os.path.join(outdir, "windows-install.bat"), "w", newline="\r\n") as f:
        f.write("\n".join(bat) + "\n")

    with open(os.path.join(outdir, "disc-files.txt"), "w") as f:
        for rel in sorted(wanted, key=str.lower):
            f.write(rel + "\n")
        f.write("autoexec.cfg\n")
    with open(os.path.join(outdir, "disc-sums.txt"), "w", newline="\n") as f:
        f.write("".join(line + "\n" for line in sums))
    print(f"   install scripts: {len(wanted) + 1} files from the discs, "
          f"{literal} bytes of ours")


# cmd runs this; everything from the first #__NOLF_PS__ line on is never
# parsed by it (`exit /b` comes first). The PowerShell between the markers is
# written to a temporary .ps1 and run with -File, which passes the dropped or
# typed paths through as they are.
BAT_HEADER = r"""@echo off
rem windows-install.bat -- install the game data from your two NOLF discs.
rem
rem   Drop both disc images of The Operative: No One Lives Forever, Game of
rem   the Year Edition (Europe) onto this file, or put them in this folder
rem   and double-click it. Then start the game with NOLF.exe. An image is a
rem   plain .iso, a raw .bin (or its .cue), or a .zip of one; a
rem   .bin or .zip is first converted to a temporary .iso in this folder
rem   (about 0.6 GB a disc, removed at the end).
rem
rem It checks each image's SHA-256, copies the game's data out of them into
rem this folder and checks every file it writes. Nothing else is changed.
setlocal
set "NOLF_SELF=%~f0"
set "NOLF_PS1=%TEMP%\nolf-install-%RANDOM%%RANDOM%.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=[IO.File]::ReadAllText($env:NOLF_SELF); $m='#'+'__NOLF_PS__'; $a=$t.IndexOf($m); $b=$t.IndexOf($m,$a+1); [IO.File]::WriteAllText($env:NOLF_PS1,$t.Substring($a,$b-$a))"
if errorlevel 1 goto failed
powershell -NoProfile -ExecutionPolicy Bypass -File "%NOLF_PS1%" "%~f0" %*
set "NOLF_RC=%errorlevel%"
del "%NOLF_PS1%" 2>nul
pause
exit /b %NOLF_RC%
:failed
echo Could not start PowerShell.
pause
exit /b 1""".splitlines()

if __name__ == "__main__":
    main()
