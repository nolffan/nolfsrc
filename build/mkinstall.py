#!/usr/bin/env python3
"""The install scripts of a game directory's public archive.

NOLF1's own (build/mkinstall.py is NOLF2's, for its InstallShield
cabinets and update).

  build/mkinstall.py <gamedir> <outdir>

A game directory holds the retail game's data as well as what we built: the
four .rez archives, the movies, the text and config files of the play disc,
and the retail Miles and Bink files (legacy builds). Our binaries ship in
the archive, the game DLLs in custom/ or the modern build's nolfsrc.rez, and
so do our startup.txt, the modern build's own defaults.cfg and defctrls.cfg
and the retail files it changes (assets/modern/; their manifest rows do not
name the discs). The public archive (build/package.sh) leaves the discs'
data out, and three scripts put it back from the player's own copies of the
two discs -- the Game of the Year Edition (Europe), as .iso images or as raw
.bin/.cue images (or a .zip of one), which the scripts convert to the .iso
first:

  linux-install.sh       bash, dd, sha256sum, base64 (coreutils)
  windows-install.bat    cmd running the PowerShell it carries (5.1, as every
                         Windows 10 and 11 has)
  mac-install.command    the Perl every Mac has, with its own modules (for
                         CrossOver or another Wine: the .bat needs PowerShell,
                         which Wine does not have)

Neither reads the ISO 9660 filesystem. A disc is recognised by its size and
SHA-256 (those of the .iso: a .bin is converted, then checked), and then
every file is known: this writes each one as a list of byte ranges of the
disc images --

  C <disc> <offset> <length>   bytes of that disc image
  Z <length>                   zeros (the retail game DLLs blanked inside the
                               .rez archives: build/all.sh's `resources`)
  L <base64>                   bytes of ours (the modern build's
                               autoexec.cfg, its defaults.cfg)

-- and the file's size and SHA-256, which the script checks before it moves
the file into place. So the result is the game directory as built, byte for
byte.

This script needs neither the retail game nor a disc image. The ranges come
from two lists: build/retail-files.txt (where each retail file is on which
disc, its size and SHA-256) and build/retail-ranges.txt (the zeroed DLLs in
each archive, and the archive's SHA-256 after that). A game directory built
with the retail game is checked against them: each file the scripts write
must be in it as they write it. One built without it (NODISCS=1: it has the
file NOT-PLAYABLE.txt) has no game data to check, and gives the same scripts.

The list goes at the end of each script, after a marker line (in the Perl
one, after its __END__); the scripts' code is
build/game-scripts/linux-install.sh, windows-install.ps1 (the .bat's header
is written here) and mac-install.command.

Writes <outdir>/linux-install.sh, <outdir>/windows-install.bat,
<outdir>/mac-install.command, <outdir>/disc-files.txt, the game directory's
paths the scripts write (the archive leaves them out), and
<outdir>/disc-sums.txt, the SHA-256 of each of those but autoexec.cfg, which
the engine rewrites (build/package.sh puts them in the archive's
SHA256SUMS).
"""
import base64
import hashlib
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "ext", "tools"))
sys.path.insert(0, os.path.join(REPO, "build"))
import discs as disclist  # noqa: E402
import retailextract as rx  # noqa: E402

SCRIPTS = os.path.join(REPO, "build", "game-scripts")
SECTOR = 2048
MARKER = "__NOLF_INSTALL_DATA__"
CHUNK = 1 << 20
RAW_SECTOR = 2352
LITERAL_LINE = 3 * 1024          # raw bytes per L record (4 KB of base64)
LITERAL_MAX = 1 << 20            # more than this of ours means a wrong source
NODISCS_MARK = "NOT-PLAYABLE.txt"   # build/all.sh, NOLF_NODISCS=1


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(CHUNK), b""):
            h.update(b)
    return h.hexdigest()


class Ops:
    def __init__(self):
        self.ops, self.literal = [], 0

    def copy(self, disc, off, n):
        if n <= 0:
            return
        last = self.ops[-1] if self.ops else None
        if last and last[0] == "C" and last[1] == disc and last[2] + last[3] == off:
            last[3] += n
        else:
            self.ops.append(["C", disc, off, n])

    def zeros(self, n):
        if n > 0:
            self.ops.append(["Z", n])

    def bytes(self, b):
        if not b:
            return
        self.literal += len(b)
        self.ops.append(["L", bytes(b)])

    def lines(self):
        for op in self.ops:
            if op[0] == "L":
                b = op[1]
                for i in range(0, len(b), LITERAL_LINE):
                    yield "L " + base64.b64encode(b[i:i + LITERAL_LINE]).decode()
            else:
                yield " ".join(str(x) for x in op)


def retail_ops(f, ranges):
    """(ops, size, SHA-256) of the game directory's copy of a retail file:
    the whole file, or an archive with its game DLLs as zeros."""
    ops = Ops()
    if f.path not in ranges.zsum:
        ops.copy(f.disc, f.off, f.size)
        return ops, f.size, f.sha
    size, sha = ranges.zsum[f.path]
    if size != f.size:
        sys.exit(f"mkinstall: {f.path}: build/retail-ranges.txt and retail-files.txt "
                 "do not agree on its size")
    pos = 0
    for off, n, _entry in sorted(ranges.zero.get(f.path, [])):
        ops.copy(f.disc, f.off + pos, off - pos)
        ops.zeros(n)
        pos = off + n
    ops.copy(f.disc, f.off + pos, f.size - pos)
    return ops, size, sha


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    game, outdir = sys.argv[1].rstrip("/"), sys.argv[2]
    nodiscs = os.path.isfile(os.path.join(game, NODISCS_MARK))
    discs, files = disclist.load()
    ranges = rx.load_ranges()
    retail = {f.path.lower(): f for f in files}

    # What in the game directory came from the discs, and from which file.
    wanted = {}                           # game path -> File
    for line in open(os.path.join(game, "MANIFEST.txt")):
        m = re.match(r"(data|THIRD-PARTY) nolf-eu-goty:(\S+).*\t(.+)$", line.rstrip("\n"))
        if m:
            wanted[m.group(3)] = retail[m.group(2).lower()]
    for rel in wanted:
        if re.search(r"[\s\"'`$\\%!]", rel):
            sys.exit(f"mkinstall: {rel}: a name the scripts do not quote")

    # Each file the scripts write: (size, SHA-256, flag, ops).
    out = {}
    for rel, f in wanted.items():
        ops, size, sha = retail_ops(f, ranges)
        path = os.path.join(game, rel)
        if not nodiscs and (not os.path.isfile(path) or os.path.getsize(path) != size
                            or sha256(path) != sha):
            sys.exit(f"mkinstall: {rel} in {game} is not what the scripts would write "
                     "(build/retail-files.txt, build/retail-ranges.txt)")
        out[rel] = (size, sha, "-", ops)

    # autoexec.cfg is the shipped defaults.cfg (build/package.sh); the
    # scripts write it only if there is none, as the engine rewrites it.
    # The modern build's defaults.cfg is ours, in the archive: its
    # autoexec.cfg is then our bytes, not the discs'.
    if "defaults.cfg" in wanted:
        size, sha, _flag, ops = out["defaults.cfg"]
        out["autoexec.cfg"] = (size, sha, "once", ops)
    else:
        cfg = open(os.path.join(game, "defaults.cfg"), "rb").read()
        ops = Ops()
        ops.bytes(cfg)
        out["autoexec.cfg"] = (len(cfg), hashlib.sha256(cfg).hexdigest(), "once", ops)

    records, literal, sums = [], 0, []
    for rel in sorted(out, key=str.lower):
        size, sha, flag, ops = out[rel]
        literal += ops.literal
        if rel != "autoexec.cfg":
            sums.append(f"{sha}  {rel}")
        records.append(f"F {rel} {size} {sha} {flag}")
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
        for rel in sorted(out, key=str.lower):
            f.write(rel + "\n")
    with open(os.path.join(outdir, "disc-sums.txt"), "w", newline="\n") as f:
        f.write("".join(line + "\n" for line in sums))
    print(f"   install scripts: {len(out)} files from the discs, {literal} bytes of ours")


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
