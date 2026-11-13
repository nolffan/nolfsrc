#!/usr/bin/env python3
"""The install scripts of a game directory's public archive.

NOLF1's own (build/mkinstall.py is NOLF2's, for its InstallShield
cabinets and update).

  build/mkinstall.py <gamedir> <outdir> [isodir]

A game directory holds the retail game's data as well as what we built: the
four .rez archives, the movies, the text and config files of the play disc,
the retail Miles and Bink files (legacy builds) and the modern build's data
edits made from them (nolfsrcdisc.rez, from the recipes of src/modern/data/;
our own archive, nolfsrc.rez, ships in the archive). The public
archive (build/package.sh) leaves all of that out, and three scripts put it
back from the player's own copies of the two discs -- the Game of the Year
Edition (Europe), as .iso images or as redump's .bin/.cue (or its .zip),
which the scripts convert to the .iso first:

  linux-install.sh       bash, dd, sha256sum, base64 (coreutils)
  windows-install.bat    cmd running the PowerShell it carries (5.1, as every
                         Windows 10 and 11 has)
  mac-install.command    the Perl every Mac has, with its own modules (for
                         CrossOver or another Wine: the .bat needs PowerShell,
                         which Wine does not have)

Neither reads the ISO 9660 filesystem. A disc is recognised by its size and
SHA-256 (those of the .iso: a .bin is converted, then checked), and then
every file is known: this writes each one as a list of
byte ranges, worked out here against the images --

  C <disc> <offset> <length>   bytes of that disc image
  Z <length>                   zeros (the retail game DLLs blanked inside the
                               .rez archives: build/all.sh's `resources`)
  L <base64>                   bytes of ours (the modern data edits: a few
                               lines of a patched .txt, a palette entry;
                               nolfsrcdisc.rez's header and directory)

-- and the file's size and SHA-256, which the script checks before it moves
the file into place. So the result is the game directory as built, byte for
byte. The list goes at the end of each script, after a marker line (in the
Perl one, after its __END__); the scripts' code is
build/game-scripts/linux-install.sh, windows-install.ps1 (the .bat's header
is written here) and mac-install.command.

Writes <outdir>/linux-install.sh, <outdir>/windows-install.bat,
<outdir>/mac-install.command,
<outdir>/disc-files.txt, the game directory's paths the scripts write (the
archive leaves them out), and <outdir>/disc-sums.txt, the SHA-256 of each of
those but autoexec.cfg, which the engine rewrites (build/package.sh puts them
in the archive's SHA256SUMS). <isodir> is ext/rez/isos/ by default; the discs'
installed copy, ext/rez/games/nolf-eu-goty/, says where each retail file came
from. Exits 3 if the disc images are not there.
"""
import base64
import difflib
import hashlib
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "ext", "tools"))
sys.path.insert(0, os.path.join(REPO, "ext", "rez", "tools"))
from rezextract import Rez  # noqa: E402
from isoextract import Iso, SECTOR  # noqa: E402

RETAIL = os.path.join(REPO, "ext", "rez", "games", "nolf-eu-goty")
RECIPES = os.path.join(REPO, "src", "modern", "data")
SCRIPTS = os.path.join(REPO, "build", "game-scripts")
DISCS = {
    1: "Operative, The - No One Lives Forever (Europe) (Disc 1) (Game of the Year Edition).iso",
    2: "Operative, The - No One Lives Forever (USA, Europe) (Disc 2) (Game of the Year Edition).iso",
}
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


class Disc:
    def __init__(self, n, path):
        self.n, self.path, self.name = n, path, os.path.basename(path)
        self.size = os.path.getsize(path)
        self.sha = sha256(path)
        self.f = open(path, "rb")
        self.files = {p.lower(): (ext * SECTOR, sz) for p, ext, sz in Iso(path).walk()}

    def read(self, off, n):
        self.f.seek(off)
        return self.f.read(n)


class Source:
    """A retail file's bytes: one range of one disc image."""
    def __init__(self, disc, off, size):
        self.disc, self.off, self.size = disc, off, size

    def read(self, pos=0, n=None):
        return self.disc.read(self.off + pos, self.size - pos if n is None else n)


class Ops:
    def __init__(self):
        self.ops, self.literal = [], 0

    def copy(self, src, pos, n):
        if n <= 0:
            return
        off = src.off + pos
        last = self.ops[-1] if self.ops else None
        if last and last[0] == "C" and last[1] == src.disc.n and last[2] + last[3] == off:
            last[3] += n
        else:
            self.ops.append(["C", src.disc.n, off, n])

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

    def extend(self, other):
        """Another Ops' records after these, runs joined where they meet."""
        for op in other.ops:
            if op[0] == "C":
                last = self.ops[-1] if self.ops else None
                if last and last[0] == "C" and last[1] == op[1] and last[2] + last[3] == op[2]:
                    last[3] += op[3]
                else:
                    self.ops.append(list(op))
            elif op[0] == "Z":
                if self.ops and self.ops[-1][0] == "Z":
                    self.ops[-1][1] += op[1]
                else:
                    self.ops.append(list(op))
            else:
                self.bytes(bytes(op[1]))

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


def ops_for_bytes(sources, out):
    """ops_for, for bytes in memory (an archive's entry)."""
    best = None
    for src in sources:
        if src.size == len(out):
            ops = ops_same_chunks(src, (out[i:i + CHUNK] for i in range(0, len(out), CHUNK)))
        else:
            ops = ops_text(src, out)
        if best is None or ops.literal < best.literal:
            best = ops
    return best


def ops_archive(parts, path):
    """An archive of ours whose entries are made from retail files
    (nolfsrcdisc.rez): each entry from its sources, the bytes between them
    (the header, the directories) literal. parts: (pos, size, sources, name)."""
    data = open(path, "rb").read()
    ops, at = Ops(), 0
    for pos, size, srcs, name in sorted(parts, key=lambda p: p[0]):
        if pos < at:
            sys.exit(f"mkinstall: {path}: entries overlap at {name}")
        ops.bytes(data[at:pos])
        best = ops_for_bytes(srcs, data[pos:pos + size])
        if best is None:
            sys.exit(f"mkinstall: {path}:{name}: not made from its retail file")
        ops.extend(best)
        at = pos + size
    ops.bytes(data[at:])
    return ops


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    game, outdir = sys.argv[1].rstrip("/"), sys.argv[2]
    isodir = sys.argv[3] if len(sys.argv) == 4 else os.path.join(REPO, "ext", "rez", "isos")
    paths = {n: os.path.join(isodir, name) for n, name in DISCS.items()}
    missing = [p for p in paths.values() if not os.path.isfile(p)]
    if missing or not os.path.isfile(os.path.join(RETAIL, "MANIFEST.txt")):
        print("mkinstall: no disc images (%s) or no ext/rez/games/nolf-eu-goty/ -- "
              "ext/rez/fetch-isos.sh nolf-eu-goty && ext/rez/install-games.sh nolf-eu-goty"
              % ", ".join(missing or paths.values()), file=sys.stderr)
        sys.exit(3)
    discs = {n: Disc(n, p) for n, p in paths.items()}
    by_name = {d.name: d for d in discs.values()}

    # Where each retail file is on the discs (ext/rez/install-games.sh's record).
    retail = {}
    for line in open(os.path.join(RETAIL, "MANIFEST.txt"), errors="replace"):
        if line.startswith("#") or "\t" not in line:
            continue
        rel, origin = line.rstrip("\n").split("\t", 1)
        iso, _, dpath = origin.partition(".iso:")
        disc = by_name[iso + ".iso"]
        off, size = disc.files[dpath.lower()]
        retail[rel.lower()] = Source(disc, off, size)

    # The four archives' entries, as ranges of the discs.
    entries = {}                          # archive (lower) -> {path (lower): Source}
    for rez in ("nolf.rez", "nolf2.rez", "nolfgoty.rez", "nolfu003.rez"):
        src = retail[rez]
        entries[rez] = {p.lower(): Source(src.disc, src.off + pos, size)
                        for p, pos, size in Rez(os.path.join(RETAIL, next(
                            n for n in os.listdir(RETAIL) if n.lower() == rez))).walk()}

    # What in the game directory came from the discs, and from which file.
    wanted = {}                           # game path -> [Source, ...]
    for line in open(os.path.join(game, "MANIFEST.txt")):
        m = re.match(r"(data|THIRD-PARTY) nolf-eu-goty:(\S+).*\t(.+)$", line.rstrip("\n"))
        if m:
            wanted[m.group(3)] = [retail[m.group(2).lower()]]
    for rel in list(wanted):
        if os.path.isfile(os.path.join(game, rel + ".retail")):
            wanted[rel + ".retail"] = wanted[rel]
    # The modern build's edits of the discs' data, MODERN-DATA.txt's list
    # (ext/tools/moddata.py), are the entries of nolfsrcdisc.rez: each made
    # from the retail file its recipe edits, or names (.from).
    archives = {}                         # game path -> [(pos, size, [Source], name)]
    made = {}                             # path in nolfsrcdisc.rez (lower) -> [Source]
    listing = os.path.join(game, "MODERN-DATA.txt")
    for rel in open(listing).read().split() if os.path.isfile(listing) else []:
        if rel.startswith("gamedir:"):
            continue                      # a MANIFEST row already
        frm = os.path.join(RECIPES, rel + ".from")
        if os.path.isfile(frm):
            rez, path = open(frm).read().split()
            made[rel.lower()] = [entries[rez.lower()][path.lower()]]
        else:
            made[rel.lower()] = [e[rel.lower()] for e in entries.values()
                                 if rel.lower() in e]
    disc_rez = os.path.join(game, "nolfsrcdisc.rez")
    if os.path.isfile(disc_rez):
        parts = []
        for name, pos, size in Rez(disc_rez).walk():
            if name.lower() not in made:
                sys.exit(f"mkinstall: nolfsrcdisc.rez:{name}: not in MODERN-DATA.txt")
            if not made[name.lower()]:
                sys.exit(f"mkinstall: nolfsrcdisc.rez:{name}: no retail file to make it from")
            parts.append((pos, size, made[name.lower()], name))
        archives["nolfsrcdisc.rez"] = parts
        wanted["nolfsrcdisc.rez"] = None
    for rel, srcs in wanted.items():
        if srcs is None:
            continue                      # an archive of ours: `archives`
        if not srcs:
            sys.exit(f"mkinstall: {rel}: no retail file to make it from")
        if re.search(r"[\s\"'`$\\%!]", rel):
            sys.exit(f"mkinstall: {rel}: a name the scripts do not quote")

    # autoexec.cfg is the shipped defaults.cfg (build/package.sh); the
    # scripts write it only if there is none, as the engine rewrites it.
    records, literal, sums = [], 0, []
    for rel in sorted(wanted, key=str.lower):
        path = os.path.join(game, rel)
        ops = ops_archive(archives[rel], path) if rel in archives else ops_for(wanted[rel], path)
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
        # The redump .bin of the disc: one MODE1/2352 track, each 2048
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
rem   plain .iso, redump's .bin (or its .cue), or redump's .zip download; a
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
