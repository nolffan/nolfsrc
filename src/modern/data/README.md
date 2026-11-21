# src/modern/data/ -- the modern build's game-data edits

Some changes in the modern build (`make modern`, `NOLFSRC_MODERN`; see
`BUILDING.md`, "Variant builds") need changes to the retail game data as
well as to the code. These changes are bug fixes, and changes as a mod makes
them. Monolith encouraged mods of the game. Each change is a recipe here:
the source of the change, in a form that you can read and review.

The changed files themselves are committed in `assets/modern/`
(`assets/README.md`):

- `assets/modern/rez/<path>`: a file that the game loads from the rez
  archives. `build/all.sh` packs it into `nolfsrc.rez` with our other files.
  `nolfsrc.rez` loads after the four retail archives, thus our file
  overrides the retail file.
- `assets/modern/gamedir/<file>`: a file that the engine reads from the
  game directory (`defaults.cfg`, `defctrls.cfg`). `build/all.sh` copies it
  into the game directory in place of the retail file.
- `assets/modern/MODERN-DATA.txt`: the list of the files that the recipes
  make.

`build/moderndata.py` applies the recipes (with `ext/tools/moddata.py`) to
the retail game that `build/discs.py` installs from your disc images:

- `make modern-data` writes the results into `assets/modern/` again.
- `make modern-data-check` compares the results with `assets/modern/`.
  `make modern` and `make progress` also do this check. If a file is
  different, the check fails. Without the retail game (a build without the
  discs), the check does not occur, and a message says so.

Thus, when you change a recipe, run `make modern-data` and commit the
recipe and the changed files together. Do not edit a file in
`assets/modern/` that a recipe makes: change its recipe. The release build
never sees any of it.

Files that we wrote for this project, which need no retail data, are not
recipes: they are in `assets/modern/rez/` and go into `nolfsrc.rez` as
they are (`assets/README.md`).

| Recipe | What it does |
|---|---|
| `<path>.patch` | a unified diff against the retail file at `<path>` (as the game loads it: the last `.rez` archive that holds it), e.g. `ATTRIBUTES/WEAPONS.txt.patch` |
| `<path>.from` | one line, `<archive> <path in it>`: that retail file, from that archive, installed at `<path>` -- for a retail file the Game of the Year archives override, wanted under another name (e.g. `INTERFACE/MAINMENU1B.dtx.from` containing `nolf2.rez INTERFACE/MAINMENU1.dtx`) |
| `<path>.bytes` | byte edits of a binary retail file, one per line: `<offset> <old hex> <new hex>` (offset in hex or decimal, negative to count from the end; `#` starts a comment); the old bytes must match, e.g. `INTERFACE/FONTS/HUD_CHOOSER.pcx.bytes` turns one palette entry black |
| `<path>.flop` | the retail texture at `<path>` (a `.dtx`, DXT1 or 32-bit) mirrored left to right, exactly (the blocks reordered, nothing re-encoded); the file's text is a comment, e.g. `TEX/OBJECTS/OBJ14/OBJ0689.dtx.flop` (M-P4-13) |
| `gamedir/...` | the same kinds for a file the engine reads from the game directory itself rather than from the archives (`gamedir/defctrls.cfg.patch`); the result goes into `assets/modern/gamedir/` |

To make a patch, edit a copy of the retail file and let the tool write the
diff. The commands use the retail game in `out/retail/nolf-eu-goty/` (after
`make retail`) through a game directory of the four retail archives, for
example after a `make modern`, with our archive left out:

```sh
SKIP="--skip nolfsrc.rez"   # the retail file, not ours
python3 ext/tools/moddata.py show out/nolf-modern-release ATTRIBUTES/WEAPONS.txt $SKIP > /tmp/weapons.txt
# ... edit /tmp/weapons.txt ...
python3 ext/tools/moddata.py diff out/nolf-modern-release ATTRIBUTES/WEAPONS.txt /tmp/weapons.txt $SKIP \
    > src/modern/data/ATTRIBUTES/WEAPONS.txt.patch
make modern-data            # writes assets/modern/rez/ATTRIBUTES/WEAPONS.txt again
```

Keep patches small -- the changed lines and the three lines of context the
tool gives them. A patch has to apply exactly, so a retail file that does not
match what it expects stops `make modern-data` (and fails the check) rather
than producing something half edited. Commit a retail file in
`assets/modern/` only as the result of a recipe, never as an unchanged copy;
`MODERN.md` says which change each recipe belongs to.
