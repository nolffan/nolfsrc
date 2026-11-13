# Our own data: nolfsrc.rez

What the game directory's `nolfsrc.rez` carries besides the game DLLs: data
files written for this project (or taken from an open-source one), never
retail game data. The game's data comes only from the player's two Game of
the Year discs.

`build/all.sh` (step 4) packs, with lithtools' `lithrez`
(`build/lithrez.sh`; a fixed time, 988329600, the 1.003 update readme's
date, so that every build of the same inputs makes the same bytes):

| Build | `nolfsrc.rez` |
|---|---|
| legacy (`make`, `make debug`) | the game DLLs built from source: `cres.dll`, `cshell.dll`, `object.lto`, and `sres.dll` -- plus `rez/` here, if there is one |
| modern (`make modern`, `make modern-debug`) | the same DLLs, built with `NOLFSRC_MODERN`, and `modern/rez/` here |

- `rez/`: files for both builds, at the path the game loads them from.
  None so far.
- `modern/rez/`: the modern build's: the jukebox's track list
  (`ATTRIBUTES/Jukebox.txt`, M-P4-02), the sprites of the original main
  menus (`MENU/SPR/MAINMENU1B.spr`, `NOLFB.spr`, M-P3-10), and the seasonal
  main menus' Cate models (`CHARS/MODELS/INTHERO_CASUAL.abc`,
  `INTHERO_WINTER.abc`, M-P3-11), links into the `ext/nolf1-modernizer`
  submodule (`docs/LICENSE_NOTES.md`).

A file in both `rez/` and `modern/rez/`, or in one of them and the modern
build's `nolfsrcdisc.rez`, fails the build.

The modern build's changes to retail files are not here: they are recipes
(`src/modern/data/`, `ext/tools/moddata.py`) applied to the discs' data at
build time, into `nolfsrcdisc.rez`, which the public archive's install
scripts make from the player's discs; the game directory's `MODERN-DATA.txt`
lists what they wrote.

The game loads both archives last of its own, after the discs' four:

    nolf.rez  nolf2.rez  nolfu003.rez  nolfgoty.rez  [nolfsrcdisc.rez]  nolfsrc.rez

(`build/nolfcmds.sh`; the modern launcher; the registry values the legacy
launcher reads, which `linux-run.sh` writes; `startup.txt` for the dedicated
server). A mod's archives -- the launcher's custom archives, `Custom\*.rez`
-- load after them, so a mod overrides ours.
