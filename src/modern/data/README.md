# src/modern/data/ -- the modern build's game-data edits

Some changes in the modern build (`make modern`, `NOLFSRC_MODERN`; see
`BUILDING.md`, "Variant builds") need game data changed as well as code. The
game data is not in this repository -- it comes from the retail game, which
the `ext/rez/` submodule installs -- so a change is committed here as a recipe,
and `ext/tools/moddata.py` applies it when the modern build is made
(`build/all.sh`, step 4b). The results are packed into the game directory's
`nolfsrcdisc.rez`, which the engine loads after the four retail `.rez`
archives (and just before `nolfsrc.rez`), so they override the retail
copies. They are made from the discs' data, so they are never committed nor
put in the public archive: its install scripts make `nolfsrcdisc.rez` from
the player's discs (`build/mkinstall.py`). The game directory's
`MODERN-DATA.txt` lists every file written. The release build never sees
any of it.

Files written for this project, which need no retail data, are not
recipes: they are in `assets/modern/rez/` and go into `nolfsrc.rez` as
they are (`assets/README.md`).

| Recipe | What it does |
|---|---|
| `<path>.patch` | a unified diff against the retail file at `<path>` (as the game loads it: the last `.rez` archive that holds it), e.g. `ATTRIBUTES/WEAPONS.txt.patch` |
| `<path>.from` | one line, `<archive> <path in it>`: that retail file, from that archive, installed at `<path>` -- for a retail file the Game of the Year archives override, wanted under another name (e.g. `INTERFACE/MAINMENU1B.dtx.from` containing `nolf2.rez INTERFACE/MAINMENU1.dtx`) |
| `<path>.bytes` | byte edits of a binary retail file, one per line: `<offset> <old hex> <new hex>` (offset in hex or decimal, negative to count from the end; `#` starts a comment); the old bytes must match, e.g. `INTERFACE/FONTS/HUD_CHOOSER.pcx.bytes` turns one palette entry black |
| `<path>.flop` | the retail texture at `<path>` (a `.dtx`, DXT1 or 32-bit) mirrored left to right, exactly (the blocks reordered, nothing re-encoded); the file's text is a comment, e.g. `TEX/OBJECTS/OBJ14/OBJ0689.dtx.flop` (M-P4-13) |
| `gamedir/...` | the same kinds for a file the engine reads from the game directory itself rather than from the archives (`gamedir/defctrls.cfg.patch`); the retail original is kept beside it as `<file>.retail` and put back if the recipe goes |

To make a patch, edit a copy of the file and let the tool write the diff
(after a `make modern`, so `out/nolf-modern-release/` exists):

```sh
SKIP="--skip nolfsrc.rez --skip nolfsrcdisc.rez"   # the retail file, not ours
python3 ext/tools/moddata.py show out/nolf-modern-release ATTRIBUTES/WEAPONS.txt $SKIP > /tmp/weapons.txt
# ... edit /tmp/weapons.txt ...
python3 ext/tools/moddata.py diff out/nolf-modern-release ATTRIBUTES/WEAPONS.txt /tmp/weapons.txt $SKIP \
    > src/modern/data/ATTRIBUTES/WEAPONS.txt.patch
```

Keep patches small -- the changed lines and the three lines of context the
tool gives them. A patch has to apply exactly, so a retail file that does not
match what it expects fails the build rather than producing something half
edited. Never commit a whole retail file (nor here, nor in `assets/`);
`MODERN.md` says which change each recipe belongs to.
