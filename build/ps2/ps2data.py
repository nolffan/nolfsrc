#!/usr/bin/env python3
"""The PS2 "Nine years ago" levels' attribute edits (MODERN.md, P8).

  ps2data.py <gamedir>

The recipes in build/ps2/data/ (unified diffs, as src/modern/data/'s) only
make sense with the converted PS2 content installed (build/ps2/ps2install.py),
so they are applied here rather than by tools/moddata.py: each one on top of
the file as the modern build leaves it -- the retail file the game loads,
with src/modern/data/'s own recipe for it, if any, applied first -- and
written into the game directory's custom/. Running it again gives the same
result. A `make modern` rewrites those files without the PS2 edits; run this
again afterwards.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, "tools"))
import moddata  # noqa: E402

DATA = os.path.join(HERE, "data")
MODERN = os.path.join(REPO, "src", "modern", "data")


def rez_stack(gamedir):
    """moddata.rez_stack, but matching the archive names whatever their
    case: the retail launcher writes nolfcmds.txt with NOLF.rez, NOLF2.rez,
    which on Linux are not the files nolf.rez, nolf2.rez."""
    names = {n.lower(): n for n in os.listdir(gamedir)}
    cmds = open(os.path.join(gamedir, "nolfcmds.txt"), errors="replace").read()
    stack = []
    for name in moddata.re.findall(r"-rez\s+(\S+)", cmds):
        real = names.get(name.lower())
        if real and real.lower().endswith(".rez") and os.path.isfile(os.path.join(gamedir, real)):
            stack.append(os.path.join(gamedir, real))
    return stack


moddata.rez_stack = rez_stack


def patches():
    for root, _, fs in os.walk(DATA):
        for f in sorted(fs):
            if f.endswith(".patch"):
                p = os.path.join(root, f)
                yield os.path.relpath(p, DATA)[:-len(".patch")].replace("\\", "/"), p


def custom_path(custom, path):
    """custom/<path>, matching existing directory and file names whatever
    their case (Wine finds only one of two names that differ in case)."""
    cur = custom
    parts = path.split("/")
    for i, part in enumerate(parts):
        hit = None
        if os.path.isdir(cur):
            for n in os.listdir(cur):
                if n.lower() == part.lower():
                    hit = n
                    break
        cur = os.path.join(cur, hit or part)
    return cur


def apply(gamedir):
    data = moddata.Data(gamedir)
    custom = os.path.join(gamedir, "custom")
    for path, recipe in patches():
        raw, real = data.read(path)
        if raw is None:
            sys.exit(f"ps2data: {path}: not in the game's rez archives")
        text = raw.decode("latin-1")
        base = os.path.join(MODERN, path + ".patch")
        if os.path.exists(base):
            text = moddata.apply_patch(text, open(base, encoding="latin-1").read(), path)
        text = moddata.apply_patch(text, open(recipe, encoding="latin-1").read(), path)
        dest = custom_path(custom, real)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        if os.path.lexists(dest):
            os.remove(dest)          # never write through a hard link
        with open(dest, "wb") as f:
            f.write(text.encode("latin-1"))
        print(f"   {os.path.relpath(dest, gamedir)}  <- {os.path.relpath(recipe, REPO)}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    apply(sys.argv[1])
