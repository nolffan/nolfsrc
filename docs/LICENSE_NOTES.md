# What is whose

This repository mixes code from different origins under different terms.
This file says which is which; it is not legal advice and grants nothing.

## In this repository

| Path | Origin | Terms |
|---|---|---|
| `src/game/` | **Monolith Productions**: *No One Lives Forever Source Code v1.003* (May 2001), unmodified except for the Game of the Year Edition's changes (reconstructed for this project from the retail GOTY `CShell.dll` and `Object.lto`, made in place and marked with `GOTY:` comments) and changes inside `#ifdef NOLFSRC_MODERN` blocks (the modern build's fixes and features, written for this project; the release build compiles none of them), including its Win32 `flex.exe` and `bison.exe` (GNU tools, used to generate the animation parser) | Monolith's end-user licence for the source release, reproduced in full in `src/MONOLITH-README.txt` (section IV). Read it: among other things it restricts redistribution of the source and requires add-ons to say they are not made or supported by Monolith. |
| `src/LT2/` | **Monolith Productions**: the LithTech 2 SDK and shared-library headers from the same release, unmodified (the Miles and Bink binaries that came with it are left out; its prebuilt libraries are in `progress/reference/`) | as `src/game/` |
| `src/MONOLITH-README.txt` | Monolith's readme for that release | as `src/game/` |
| `progress/reference/LT2/lithshared/libs/` | **Monolith Productions**: the prebuilt lithshared libraries of the same release (Release and Debug builds), unmodified; never built into anything, only compared against by `make progress` (`progress/reference/README.md`) | as `src/game/` |
| `assets/modern/` (the files `assets/modern/MODERN-DATA.txt` lists) | **Monolith Productions** / Fox Interactive / Sierra: retail game data files of the Game of the Year Edition that the modern build changes, made from the retail files by the recipes of `src/modern/data/` (`build/moderndata.py`). These changes are bug fixes, and changes as a mod makes them: attribute files, a few textures and a model, and `defaults.cfg` and `defctrls.cfg`. They are committed as fixes, as Monolith's prebuilt libraries in `progress/reference/` are, and the modern build's `nolfsrc.rez` and public archive carry them. No unchanged retail file is committed; the game's other data comes only from the player's discs (below) | Monolith's: the game's end-user licence. Monolith encouraged mods of the game; the terms of its source release (`src/MONOLITH-README.txt`, section IV) also apply to add-ons |
| `assets/art/` | **Monolith Productions**: the retail art and text that our binaries are compiled with, as the retail binaries' resources hold them: the launcher's splash bitmap, palette, music, icon and two cursors (`NOLF.exe`), the dedicated server's two icons and its setup wizard's bitmap (`NolfServ.exe`), the engine's icon and console font (`lithtech.exe`), and the Game of the Year strings and credits text (the GOTY `CRES.dll`). About 0.5 MB. They are unchanged. They are committed so that every build, also one without the discs, compiles binaries that are complete; `make art` makes them from the retail binaries, and every build with the discs checks them (`assets/README.md`). Our binaries, and thus the public archives, carry them | Monolith's: the game's end-user licence |
| `assets/modern/` (the other files) | written for this project: the jukebox's track list and two sprite files; the two Cate models are links into `ext/nolf1-modernizer` (below) | no licence is declared |
| `src/lithshared/` | the LithTech shared libraries. Much of it (`stdlith/`, `CryptMgr/`, `RegMgr/`, `MFCStub/`, the `ButeMgr/` parser, `lith/`) is adapted from Monolith's later public source release for *No One Lives Forever 2*; the rest was written or reconstructed for this project, including `ztools/`, `rezmgr/` and `gamespy/` (a stand-in for the GameSpy middleware). `zlib/` is zlib | Monolith's code under the terms of the release it came from; zlib under the zlib licence; the rest as `src/engine/` |
| `src/engine/` | the LithTech engine (`lithtech.exe`, `server.dll`, `d3d.ren`), the `NOLF.exe` launcher, the `NolfServ.exe` dedicated server, `ltmsg.dll` and `sres.dll`: written for this project, reconstructed from the retail binaries so that it compiles back to them. It implements Monolith's engine and interfaces, and `launcher.rc` / `nolfserv.rc` / `ltmsg.rc` reproduce the retail resource scripts (dialogs and strings). `NolfServ.exe` also compiles the game's `src/game/Shared/NetDefs.cpp` (Monolith's) and links the static MFC of the toolchain | written for this project; no licence is declared |
| `src/nolfsrc/`, `src/modern/`, `build/`, `ext/tools/`, `docker/`, `progress/`, `Makefile`, the documentation | written for this project | no licence is declared |
| `ext/openmiles` (submodule) | **OpenMiles**, a clean-room reimplementation of the Miles Sound System, upstream at github.com/maci0/openmiles; our fork, github.com/nolffan/openmiles (branch `miles6-fixes`: upstream's release with our fixes, GPL-3.0-only as the code they change); vendors miniaudio and TinySoundFont | GPL-3.0-only (its vendored libraries: MIT, MIT-0, zlib); see its `LICENSE` and `deps/README.md` |
| `ext/ffmpeg` (submodule) | **FFmpeg**, upstream at github.com/FFmpeg/FFmpeg | LGPL-2.1-or-later as the modern build configures it (no GPL or non-free parts enabled); see its `LICENSE.md` |
| `ext/nolf1-modernizer` (submodule) | **NOLF Modernizer** by HeyThereCoffeee (haekb), upstream at github.com/haekb/nolf1-modernizer. The modern build uses two of its files, the seasonal main menus' Cate models (`ASSETS/CHARS/MODELS/INTHERO_CASUAL.abc`, `INTHERO_WINTER.abc`, linked from `assets/modern/rez/`), which the modern build's `nolfsrc.rez` carries; none of its code | no licence of its own: its readme says it remains under Monolith's end-user licence for the v1.003 source release, as `src/game/` |

## The retail data in this repository

This repository holds three sets of retail data, and no other:

* `assets/modern/`: the retail data files that the modern build changes
  (bug fixes, and changes as a mod makes them).
* `assets/art/`: the retail art and text that our binaries are compiled
  with (icons, the launcher's splash bitmap and sound, the dedicated
  server's wizard bitmap, the console font, the Game of the Year strings
  and credits text).
* `progress/reference/`: Monolith's prebuilt lithshared libraries from the
  v1.003 source release, for comparison only.

All other game data comes from the player's own discs.

## Not in this repository, taken from the retail game at build time

The build takes these from the Game of the Year Edition as
`build/discs.py` installs it from your own images of its two discs
(`docs/BUILDING.md`, "What you need").

| What | Origin | Where it goes |
|---|---|---|
| the game data: `.rez` archives, configs, text, movies (but the changed files of `assets/modern/`, above) | Monolith Productions / Fox Interactive / Sierra | `out/nolf-legacy-release/` |
| `mss32.dll` and its `.flt`, `.m3d`, `.asi`, `.as6` providers | Miles Sound System, RAD Game Tools | `out/nolf-legacy-release/` (the only binaries in the game not built from source) |
| `binkw32.dll` | Bink Video, RAD Game Tools | `out/nolf-legacy-release/` |

`make progress` also extracts the retail `lithtech.exe`, `server.dll`,
`d3d.ren`, `NOLF.exe`, `NolfServ.exe` and `ltmsg.dll`, and the Game of the Year `CShell.dll`, `Object.lto`
and `CRes.dll` (from `NOLFGOTY.REZ`), and `sres.dll` (from `NOLF2.REZ`), into `out/reference/`, to compare our
build against; they are never run and never go into the game directory.

The retail game DLLs inside the `.rez` archives are overwritten with zeros
when the archives are copied; the retail executables are never copied into
the game directory.

## Not in this repository, provided by you

The compilers in `ext/toolchains/` (Microsoft Visual C++ 6.0, Intel C/C++ 4.0;
see `ext/toolchains/README.md`), under their own licences.
`ext/toolchains/directmusic-5.3.1.904/` also holds Microsoft's DirectMusic DLLs
from the DirectX 9.0c redistributable, which the modern build installs in
its game directory (below).

The docker image is built from Ubuntu 24.04 packages (Wine, Python, Xvfb,
ImageMagick, ...), each under its own licence, and Zig (MIT), downloaded
from ziglang.org.

## The modern build's middleware

The modern game directory (`out/nolf-modern-release/`) has no RAD binaries.
In their place it has `mss32.dll`, built from OpenMiles (GPL-3.0-only), and
`binkw32.dll`, our player (`src/modern/binkw32/`) statically linked with
FFmpeg (LGPL-2.1-or-later), both built by `build/ext.sh` from the `ext/`
submodules. This repository only points at those projects; the terms matter
when a built game directory is passed on. The GPL in particular may not sit
well with a directory whose other binaries are built from Monolith's source
under its end-user licence; work that out before distributing a modern
build. The LGPL asks that whoever receives `binkw32.dll` can rebuild it
against a modified FFmpeg, which `build/ext.sh` and the source here allow.

It also has Microsoft's DirectMusic DLLs (`ms_dmime.dll` and the rest, from
`ext/toolchains/directmusic-5.3.1.904/`), under the licence of Microsoft's DirectX
End-User Runtime they come from. Check its terms before passing on a game
directory that carries them. `lithtech.exe.manifest`, which points the game
at them, was written for this project.
