#!/usr/bin/env python3
"""The modern build's changed retail files: make them from the recipes, or
check them.

  build/moderndata.py write   (make modern-data)
  build/moderndata.py check   (make modern and make progress run it)

The modern build changes some files of the retail game: bug fixes, and
changes as a mod makes them. The changed files are committed in
assets/modern/ (assets/README.md):

  assets/modern/rez/<path>       a changed file the game loads from the rez
                                 archives; build/all.sh packs it into
                                 nolfsrc.rez with our other files
  assets/modern/gamedir/<file>   a changed file the engine reads from the
                                 game directory (defaults.cfg,
                                 defctrls.cfg); build/all.sh copies it into
                                 the game directory
  assets/modern/MODERN-DATA.txt  the list of the files above that come from
                                 the recipes (the game-directory files with
                                 the prefix "gamedir:")

The recipes in src/modern/data/ (src/modern/data/README.md) are the source of
each change, in a form that you can read. This script applies them, with
ext/tools/moddata.py, to the installed retail game (out/retail/nolf-eu-goty/,
or $NOLF_RETAIL), never to a game directory.

`write` writes the results into assets/modern/ and removes a file that the
list names but that no recipe makes now. `check` makes the results in a
temporary directory and compares them with assets/modern/: the list and
every file must be the same. It fails if they are not. If the retail game is
not installed (a build without the discs), `check` does nothing, says so,
and exits 0; `write` exits 3.
"""
import contextlib
import io
import os
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "ext", "tools"))
import moddata  # noqa: E402

RETAIL = os.environ.get("NOLF_RETAIL") or os.path.join(REPO, "out", "retail", "nolf-eu-goty")
RECIPES = os.path.join(REPO, "src", "modern", "data")
ASSETS = os.path.join(REPO, "assets", "modern")
LIST = os.path.join(ASSETS, "MODERN-DATA.txt")
# The four retail archives, as the game directory names them, in load order
# (build/retailextract.py's REZ, build/nolfcmds.sh).
REZ = ["nolf.rez", "nolf2.rez", "nolfu003.rez", "nolfgoty.rez"]


def retail_file(name):
    for n in os.listdir(RETAIL):
        if n.lower() == name.lower():
            return os.path.join(RETAIL, n)
    sys.exit(f"moderndata: no {name} in {RETAIL}")


def make(out):
    """Apply the recipes to the retail game; the results go into
    <out>/rez/ and <out>/gamedir/. Returns the list of what was made."""
    with tempfile.TemporaryDirectory(prefix="moderndata-") as g:
        # A game directory of links to the retail files: moddata.py reads
        # its archives through nolfcmds.txt, and its game-directory files.
        for name in REZ:
            os.symlink(retail_file(name), os.path.join(g, name))
        with open(os.path.join(g, "nolfcmds.txt"), "w") as f:
            f.write("".join(f"-rez {n} " for n in REZ) + "\n")
        gamedir = sorted({p for t, p, _k, _r in moddata.recipes(RECIPES) if t == "gamedir"})
        for p in gamedir:
            shutil.copyfile(retail_file(p), os.path.join(g, p))
        listing = os.path.join(g, "list.txt")
        with contextlib.redirect_stdout(io.StringIO()):   # its paths are the temporary ones
            moddata.cmd_apply(g, RECIPES, out=os.path.join(out, "rez"), listing=listing)
        made = [e for e in open(listing).read().split("\n") if e]
        for e in made:
            if e.startswith("gamedir:"):
                p = e[len("gamedir:"):]
                dest = os.path.join(out, "gamedir", p)
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                shutil.copyfile(os.path.join(g, p), dest)
    return made


def path_of(entry, root):
    if entry.startswith("gamedir:"):
        return os.path.join(root, "gamedir", entry[len("gamedir:"):])
    return os.path.join(root, "rez", entry)


def committed():
    if not os.path.isfile(LIST):
        return []
    return [e for e in open(LIST).read().split("\n") if e]


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("write", "check"):
        sys.exit(__doc__)
    mode = sys.argv[1]
    if not os.path.isfile(os.path.join(RETAIL, ".retail-files")):
        if mode == "check":
            print(f"   assets/modern/ not checked against src/modern/data/: "
                  f"the retail game is not installed in {RETAIL}")
            return 0
        print(f"moderndata: the retail game is not installed in {RETAIL} -- make retail "
              "(it needs your two disc images: docs/BUILDING.md)", file=sys.stderr)
        return 3
    with tempfile.TemporaryDirectory(prefix="moderndata-out-") as out:
        made = make(out)
        old = committed()
        if mode == "write":
            for e in old:
                if e not in made and os.path.isfile(path_of(e, ASSETS)):
                    os.remove(path_of(e, ASSETS))
                    print(f"   removed {os.path.relpath(path_of(e, ASSETS), REPO)} (no recipe makes it now)")
            for e in made:
                dest = path_of(e, ASSETS)
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                shutil.copyfile(path_of(e, out), dest)
                print(f"   {os.path.relpath(dest, REPO)}")
            with open(LIST, "w", newline="\n") as f:
                f.write("".join(e + "\n" for e in made))
            print(f"   {len(made)} file(s) -> {os.path.relpath(ASSETS, REPO)}/")
            return 0
        bad = []
        if sorted(old) != sorted(made):
            bad.append(f"{os.path.relpath(LIST, REPO)} is not the list of what the recipes make")
        for e in made:
            mine = path_of(e, ASSETS)
            if not os.path.isfile(mine):
                bad.append(f"{os.path.relpath(mine, REPO)}: missing")
            elif open(mine, "rb").read() != open(path_of(e, out), "rb").read():
                bad.append(f"{os.path.relpath(mine, REPO)}: not what its recipe makes")
        if bad:
            for b in bad:
                print(f"error: {b}", file=sys.stderr)
            print("error: assets/modern/ and src/modern/data/ differ: change both, or run "
                  "make modern-data to write assets/modern/ again from the recipes", file=sys.stderr)
            return 1
        print(f"   assets/modern/: the {len(made)} file(s) the src/modern/data/ recipes make, unchanged")
        return 0


if __name__ == "__main__":
    sys.exit(main())
