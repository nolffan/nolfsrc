# Our own data, and the retail data we commit

This directory holds two sets of files:

- `modern/`: what the game directory's `nolfsrc.rez` carries besides the
  game DLLs, and the modern build's own game-directory files (below).
- `art/`: the retail art and text that our binaries are compiled with
  ("The retail art", at the end).

With Monolith's prebuilt libraries in `progress/reference/`, these are the
only retail data in this repository. All other game data comes only from
the player's two Game of the Year discs.

## modern/

There are two kinds of file in `modern/`:

- data files written for this project (or taken from an open-source one);
- retail game files that the modern build changes: bug fixes, and changes
  as a mod makes them. Monolith encouraged mods of the game. Each one is
  the result of a recipe in `src/modern/data/` (`src/modern/data/README.md`),
  and `assets/modern/MODERN-DATA.txt` lists them. No unchanged retail data
  file is in `modern/`.

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

## The retail art

`art/` holds the retail art and text that our binaries are compiled with,
unchanged, as the retail binaries' resources hold them. About 0.5 MB:

| Directory | What | Compiled into |
|---|---|---|
| `art/launcher/` | the splash bitmap (`game1.bmp`), its palette (`game1.pal`), the music (`game1.wav`), the icon (`nolf.ico`) and two cursors (`webhand.cur`, `webclick.cur`) | `NOLF.exe`, and the modern build's `NOLFLauncher.exe` |
| `art/nolfserv/` | two icons (`nolfserv.ico`, `selected.ico`) and the setup wizard's bitmap (`wizard.bmp`) | `NolfServ.exe` |
| `art/lithtech/` | the icon (`lithtech.ico`) and the console font (`confont.pcx`) | `lithtech.exe` |
| `art/goty/` | the Game of the Year strings (`goty.rc`, string IDs 30000-30095) and the credits text (`credits.txt`) | `CRes.dll` |

The source for each is a retail binary: `NOLF.exe`, `NolfServ.exe` and
`lithtech.exe`, and the Game of the Year `CRES.dll` in `NOLFGOTY.REZ`.
They are committed so that every build compiles complete binaries, also a
build without the discs (`NODISCS=1`, the CI build). Thus the binaries in
the public archives are complete.

- Every build copies them into the directories that the resource scripts
  include from (`build/all.sh`, steps 2b and 2c;
  `build/retailextract.py art install`).
- `make art` writes them again from the retail binaries
  (`build/retailextract.py art write`).
- `make art-check`, and every build with the retail game, compares them
  with the retail binaries, and fails if they are different.

Do not edit these files.
