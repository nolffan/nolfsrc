# Our own data: nolfsrc.rez

What the game directory's `nolfsrc.rez` carries besides the game DLLs, and
the modern build's own game-directory files. There are two kinds of file:

- data files written for this project (or taken from an open-source one);
- retail game files that the modern build changes: bug fixes, and changes
  as a mod makes them. Monolith encouraged mods of the game. Each one is
  the result of a recipe in `src/modern/data/` (`src/modern/data/README.md`),
  and `assets/modern/MODERN-DATA.txt` lists them. No unchanged retail file
  is here. The game's other data comes only from the player's two Game of
  the Year discs.

The modern build only (`make modern`, `make modern-debug`): `build/all.sh`
(step 4) packs, with lithtools' `lithrez` (`build/lithrez.sh`; a fixed
time, 988329600, the 1.003 update readme's date, so that every build of the
same inputs makes the same bytes), the game DLLs built from source --
`cres.dll`, `cshell.dll`, `object.lto` and `sres.dll` -- and `modern/rez/`
here into `nolfsrc.rez`. In step 1 it copies `modern/gamedir/` into the
game directory. The legacy build (`make`, `make debug`) is the retail game
rebuilt and starts as it did: its game DLLs are loose in the game
directory's `custom/`, which the release launcher loads, and it has no
`nolfsrc.rez`.

- `modern/rez/`: files at the path the game loads them from.
  - Written for this project, or from an open-source project: the jukebox's
    track list (`ATTRIBUTES/Jukebox.txt`, M-P4-02), the sprites of the
    original main menus (`MENU/SPR/MAINMENU1B.spr`, `NOLFB.spr`, M-P3-10),
    and the seasonal main menus' Cate models
    (`CHARS/MODELS/INTHERO_CASUAL.abc`, `INTHERO_WINTER.abc`, M-P3-11),
    links into the `ext/nolf1-modernizer` submodule
    (`docs/LICENSE_NOTES.md`).
  - Changed retail files, from the recipes: the attribute files
    `ATTRIBUTES/ATTACHMENTS.txt`, `LAYOUT.txt`, `MODELBUTES.txt` and
    `WEAPONS.txt`; the second cosmonaut style's model
    (`CHARS/MODELS/MULTI/COSMONAUT_STYLE2_MULTI.abc`, M-P4-04); the
    seasonal Cate skins (`CHARS/SKINS/INTHERO_*.dtx`, M-P3-11); the
    original main menus' textures (`MENU/SPRTEX/MAINMENU1B.dtx`,
    `NOLFB.dtx`, M-P3-10); `INTERFACE/FONTS/HUD_CHOOSER.pcx` (M-P4-10); and
    the briefing room's Union Jack (`TEX/OBJECTS/OBJ14/OBJ0689.dtx`,
    M-P4-13).
- `modern/gamedir/`: the modern build's `defaults.cfg` and `defctrls.cfg`
  (changed retail files, from the recipes: the new default controls,
  M-P4-06, and the new defaults). The public archive ships them, and its
  install scripts do not take them from the discs.
- `modern/MODERN-DATA.txt`: the list of the changed retail files above
  (`gamedir:` for a file of `modern/gamedir/`).

`build/moderndata.py` (`make modern-data`) writes the changed retail files
again from the recipes and the retail game. `make modern-data-check`, `make
modern` and `make progress` compare them with the recipes' results, and fail
if they are different. A file in both `modern/rez/` and the game DLLs fails
the build.

The modern game loads `nolfsrc.rez` last of its own, after the discs' four:

    nolf.rez  nolf2.rez  nolfu003.rez  nolfgoty.rez  nolfsrc.rez

(`build/nolfcmds.sh modern`; the modern launcher; `startup.txt` for the
dedicated server). A mod's archives -- the launcher's custom archives, `Custom\*.rez`
-- load after them, so a mod overrides ours.
