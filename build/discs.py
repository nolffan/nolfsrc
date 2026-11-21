#!/usr/bin/env python3
"""Install the retail game from your own two disc images, and check it.

  build/discs.py install <outdir> [image ...]
  build/discs.py verify  <outdir>
  build/discs.py find    [image ...]

The retail game is The Operative: No One Lives Forever, Game of the Year
Edition (Europe), on two discs. No part of it is in this repository.
build/retail-files.txt is the list of its files: for each file, its size,
its SHA-256, and where its bytes are on which disc. This script uses that
list and nothing else to install the game, as the discs' own setup does.

The disc images are the files you give on the command line. If you give no
files, the script looks in $NOLF_ISOS, or else in isos/ at the root of the
repository. Each image can be:

  .iso   a plain image, 2048-byte sectors (your own rip, for example)
  .bin   a raw image, 2352-byte MODE1 sectors (for example a redump dump)
  .cue   a cue sheet: the .bin it names is used
  .zip   a zip that holds one such .bin or .iso (it is read in place;
         nothing is unpacked to the disk)

A disc is recognised by the volume label and the volume size in its file
system. The image does not have to be identical to a known image: every
file that the install writes is checked against the list (its size and its
SHA-256). Thus any correct copy of the discs works.

`install` writes every file of the list into <outdir> (the build uses
out/retail/nolf-eu-goty/). A file that is already correct stays. Each file
is written to <name>.part, checked, and then renamed. At the end the
script writes <outdir>/.retail-files, a copy of the list that it installed.
`verify` checks every file of <outdir> against the list: all the files must
be there, with the correct size and SHA-256. Exit status 0 if they are, 1
if not. `find` shows which image is which disc.

Exit status 3 if a disc image is missing: the message tells you what to
supply.

build/mkinstall.py reads the same list: the install scripts of a public
archive copy the same byte ranges out of the player's disc images.
"""
import hashlib
import os
import struct
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIST = os.path.join(REPO, "build", "retail-files.txt")
SECTOR = 2048
RAW_SECTOR = 2352
RAW_HEAD = 16                               # sync (12) + header (4), MODE1
SYNC = b"\x00" + b"\xff" * 10 + b"\x00"
CHUNK = 1 << 20
EXTS = (".iso", ".bin", ".cue", ".zip")


class Disc:
    """One disc of the list."""
    def __init__(self, n, label, blocks, size, sha, name):
        self.n, self.label, self.blocks = n, label, blocks
        self.size, self.sha, self.name = size, sha, name


class File:
    """One file of the install."""
    def __init__(self, path, size, sha, disc, off, dpath):
        self.path, self.size, self.sha = path, size, sha
        self.disc, self.off, self.dpath = disc, off, dpath


def load(path=LIST):
    """({disc number: Disc}, [File]) from the list."""
    discs, files = {}, []
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        f = line.split("\t")
        if f[0] == "D":
            discs[int(f[1])] = Disc(int(f[1]), f[2], int(f[3]), int(f[4]), f[5], f[6])
        elif f[0] == "F":
            files.append(File(f[1], int(f[2]), f[3], int(f[4]), int(f[5]), f[6]))
        else:
            sys.exit(f"discs.py: {path}: a line that is not D or F: {line[:60]}")
    return discs, files


# --- disc images ------------------------------------------------------------

class Image:
    """The 2048-byte user data of one disc image, read from any of the
    accepted formats. Reads that go forward are cheap in all of them; a
    .zip goes back to the start of its member for a read that goes back."""

    def __init__(self, path):
        self.path, self.label = path, os.path.basename(path)
        self.zip = self.f = None
        src = path
        if path.lower().endswith(".cue"):
            src = self._cue(path)
        if zipfile.is_zipfile(src):
            self.zip = zipfile.ZipFile(src)
            members = [i for i in self.zip.infolist()
                       if i.filename.lower().endswith((".bin", ".iso"))]
            if len(members) != 1:
                raise ValueError("not one .bin or .iso in the zip")
            self.member = members[0]
            self.f = self.zip.open(self.member)
            self.length = self.member.file_size
        else:
            self.f = open(src, "rb")
            self.length = os.path.getsize(src)
        head = self.f.read(len(SYNC))
        self.raw = head == SYNC and self.length % RAW_SECTOR == 0
        self._rewind()

    @staticmethod
    def _cue(path):
        import re
        text = open(path, encoding="latin-1").read()
        m = re.search(r'^\s*FILE\s+"([^"]+)"', text, re.M | re.I)
        if not m or not re.search(r"TRACK\s+01\s+MODE1/(2352|2048)", text, re.I):
            raise ValueError("not a cue sheet of one MODE1 data track")
        return os.path.join(os.path.dirname(path), m.group(1))

    def _rewind(self):
        if self.zip:
            self.f.close()
            self.f = self.zip.open(self.member)
        else:
            self.f.seek(0)
        self.pos = 0

    def _seek(self, raw_off):
        if raw_off < self.pos:
            self._rewind()
        if self.zip:
            while self.pos < raw_off:      # forward: read and drop
                n = len(self.f.read(min(CHUNK, raw_off - self.pos)))
                if not n:
                    raise EOFError
                self.pos += n
        else:
            self.f.seek(raw_off)
            self.pos = raw_off

    def _read(self, n):
        b = self.f.read(n)
        self.pos += len(b)
        return b

    def chunks(self, off, n):
        """The user data bytes off .. off + n, in chunks."""
        if not self.raw:
            self._seek(off)
            while n:
                b = self._read(min(CHUNK, n))
                if not b:
                    raise EOFError
                n -= len(b)
                yield b
            return
        sec, within = divmod(off, SECTOR)
        self._seek(sec * RAW_SECTOR)
        per = CHUNK // SECTOR
        while n:
            buf = self._read(RAW_SECTOR * per)
            if not buf or len(buf) % RAW_SECTOR and len(buf) < RAW_SECTOR:
                raise EOFError
            while len(buf) % RAW_SECTOR:
                more = self._read(RAW_SECTOR - len(buf) % RAW_SECTOR)
                if not more:
                    raise EOFError
                buf += more
            data = b"".join(buf[i + RAW_HEAD:i + RAW_HEAD + SECTOR]
                            for i in range(0, len(buf), RAW_SECTOR))
            data = data[within:within + n]
            within = 0
            n -= len(data)
            yield data

    def read(self, off, n):
        return b"".join(self.chunks(off, n))

    def volume(self):
        """(volume label, volume size in blocks) of its ISO 9660 file system."""
        pvd = self.read(16 * SECTOR, SECTOR)
        if pvd[1:6] != b"CD001" or pvd[0] != 1:
            return None
        return pvd[40:72].decode("ascii", "replace").rstrip(), struct.unpack_from("<I", pvd, 80)[0]


def candidates(args):
    """The image files to look at: the arguments, or the files in
    $NOLF_ISOS or isos/ (and the directories given)."""
    if args:
        out = []
        for a in args:
            if os.path.isdir(a):
                out += candidates_in(a)
            else:
                out.append(a)
        return out
    d = os.environ.get("NOLF_ISOS") or os.path.join(REPO, "isos")
    return candidates_in(d)


def candidates_in(d):
    if not os.path.isdir(d):
        return []
    names = sorted(n for n in os.listdir(d) if n.lower().endswith(EXTS))
    # .iso first (no conversion), and a .bin before the .cue that names it
    order = {e: i for i, e in enumerate(EXTS)}
    names.sort(key=lambda n: order[os.path.splitext(n)[1].lower()])
    return [os.path.join(d, n) for n in names]


def identify(discs, paths, quiet=False):
    """{disc number: Image} for the images among `paths` that are discs of
    the list: the first image found for each disc."""
    found, seen = {}, set()
    for p in paths:
        if len(found) == len(discs):
            break
        try:
            real = os.path.realpath(Image._cue(p) if p.lower().endswith(".cue") else p)
        except (OSError, ValueError):
            real = p
        if real in seen:
            continue
        seen.add(real)
        try:
            img = Image(p)
            vol = img.volume()
        except (OSError, ValueError, EOFError, zipfile.BadZipFile) as e:
            if not quiet:
                print(f"   {os.path.basename(p)}: not read ({e})")
            continue
        for n, d in sorted(discs.items()):
            if n not in found and vol == (d.label, d.blocks):
                found[n] = img
                if not quiet:
                    print(f"   {os.path.basename(p)}: disc {n} ({d.label})")
                break
        else:
            if not quiet:
                print(f"   {os.path.basename(p)}: not a disc of this release")
    return found


def missing_message(discs, found):
    lines = ["error: the retail game's disc images are missing. Bring your own discs:"]
    for n, d in sorted(discs.items()):
        if n not in found:
            lines.append(f"  disc {n} (volume label \"{d.label}\"), for example {d.name}")
    lines += [
        "Put both images of The Operative: No One Lives Forever, Game of the Year",
        "Edition (Europe) in isos/ at the root of the repository (a hard link is",
        "good: ln <image> isos/), or set NOLF_ISOS=<directory>. An image can be a",
        ".iso, a .bin with its .cue, or a .zip that holds one of them.",
        "See docs/BUILDING.md, \"What you need\".",
    ]
    return "\n".join(lines)


# --- install and verify -------------------------------------------------------

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(CHUNK), b""):
            h.update(b)
    return h.hexdigest()


def correct(path, f):
    return os.path.isfile(path) and os.path.getsize(path) == f.size and \
        sha256_file(path) == f.sha


def install(outdir, args):
    discs, files = load()
    found = identify(discs, candidates(args))
    if len(found) < len(discs):
        print(missing_message(discs, found), file=sys.stderr)
        return 3
    os.makedirs(outdir, exist_ok=True)
    stamp = os.path.join(outdir, ".retail-files")
    if os.path.exists(stamp):
        os.unlink(stamp)                   # not whole until the end
    written = kept = 0
    # in disc order, forward through each image (a .zip is read once)
    for f in sorted(files, key=lambda f: (f.disc, f.off)):
        dest = os.path.join(outdir, f.path)
        if correct(dest, f):
            kept += 1
            continue
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        part = dest + ".part"
        h = hashlib.sha256()
        try:
            with open(part, "wb") as out:
                for b in found[f.disc].chunks(f.off, f.size):
                    h.update(b)
                    out.write(b)
        except EOFError:
            os.unlink(part)
            print(f"error: {found[f.disc].label} ends before {f.dpath}: the image "
                  f"is not a whole copy of disc {f.disc}", file=sys.stderr)
            return 1
        if h.hexdigest() != f.sha:
            os.unlink(part)
            print(f"error: {f.dpath} in {found[f.disc].label} is not the retail file "
                  f"(SHA-256 {h.hexdigest()}, the list has {f.sha}): the image is not "
                  f"a correct copy of disc {f.disc} of this release", file=sys.stderr)
            return 1
        os.replace(part, dest)
        written += 1
    with open(LIST, "rb") as src, open(stamp, "wb") as out:
        out.write(src.read())
    print(f"   {written} files written, {kept} already correct -> {outdir}")
    return 0


def verify(outdir):
    _discs, files = load()
    bad = []
    for f in files:
        p = os.path.join(outdir, f.path)
        if not os.path.isfile(p):
            bad.append(f"missing  {f.path}")
        elif os.path.getsize(p) != f.size or sha256_file(p) != f.sha:
            bad.append(f"changed  {f.path}")
    for line in bad:
        print(f"   {line}")
    if bad:
        return 1
    print(f"   all {len(files)} files match build/retail-files.txt")
    return 0


def main():
    a = sys.argv[1:]
    if a[:1] == ["install"] and len(a) >= 2:
        return install(a[1], a[2:])
    if a[:1] == ["verify"] and len(a) == 2:
        return verify(a[1])
    if a[:1] == ["find"]:
        discs, _files = load()
        found = identify(discs, candidates(a[1:]))
        if len(found) < len(discs):
            print(missing_message(discs, found), file=sys.stderr)
            return 3
        return 0
    sys.exit(__doc__)


if __name__ == "__main__":
    sys.exit(main())
