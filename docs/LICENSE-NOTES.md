# What is whose

This repository mixes code from different origins under different terms.
This file says which is which; it is not legal advice and grants nothing.

## In this repository

| Path | Origin | Terms |
|---|---|---|
| `src/game/` | **Monolith Productions**: *No One Lives Forever Source Code v1.003* (May 2001), unmodified except for the Game of the Year Edition's changes (reconstructed for this project from the retail GOTY `CShell.dll` and `Object.lto`, made in place and marked with `GOTY:` comments) and changes inside `#ifdef NOLFSRC_MODERN` blocks (the modern build's fixes and features, written for this project; the release build compiles none of them), including its Win32 `flex.exe` and `bison.exe` (GNU tools, used to generate the animation parser) | Monolith's end-user licence for the source release, reproduced in full in `src/MONOLITH-README.txt` (section IV). Read it: among other things it restricts redistribution of the source and requires add-ons to say they are not made or supported by Monolith. |
| `src/LT2/` | **Monolith Productions**: the LithTech 2 SDK and shared-library headers from the same release, unmodified but for three `#ifdef NOLFSRC_MODERN` blocks the release build compiles none of (`sdk/inc/iltclient.h`, `lithshared/lithfontmgr/lithbitmapfont.h`, `lithshared/ltguimgr/ltguifont.h`) (Monolith's prebuilt libraries and the Miles and Bink binaries that came with it are left out) | as `src/game/` |
| `src/MONOLITH-README.txt` | Monolith's readme for that release | as `src/game/` |
| `src/lithshared/` | the LithTech shared libraries. Much of it (`stdlith/`, `CryptMgr/`, `RegMgr/`, `MFCStub/`, the `ButeMgr/` parser, `lith/`) is adapted from Monolith's later public source release for *No One Lives Forever 2*; the rest was written or reconstructed for this project, including `ztools/`, `rezmgr/` and `gamespy/` (a stand-in for the GameSpy middleware). `zlib/` is zlib | Monolith's code under the terms of the release it came from; zlib under the zlib licence; the rest as `src/engine/` |
| `src/engine/` | the LithTech engine (`lithtech.exe`, `server.dll`, `d3d.ren`), the `NOLF.exe` launcher, the `NolfServ.exe` dedicated server, `ltmsg.dll` and `sres.dll`: written for this project, reconstructed from the retail binaries so that it compiles back to them. It implements Monolith's engine and interfaces, and `launcher.rc` / `nolfserv.rc` / `ltmsg.rc` reproduce the retail resource scripts (dialogs and strings). `NolfServ.exe` also compiles the game's `src/game/Shared/NetDefs.cpp` (Monolith's) and links the static MFC of the toolchain | written for this project; no licence is declared |
| `src/nolfsrc/`, `src/modern/`, `build/`, `tools/`, `docker/`, `progress/`, `Makefile`, the documentation | written for this project | no licence is declared |
| `src/modern/fonts/` | the modern build's fonts, which its game directory and archives carry with their licences: **Bowlby One SC** by Vernon Adams (`BowlbyOneSC-Regular.ttf`, from github.com/google/fonts), the menu font, and **Noto Sans Mono** by the Noto Project (`NotoSansMono-Regular.ttf`, from github.com/notofonts), the console's | SIL Open Font License 1.1, in `BowlbyOneSC-OFL.txt` and `NotoSansMono-OFL.txt` beside them |
| `ext/openmiles` (submodule) | **OpenMiles**, a clean-room reimplementation of the Miles Sound System, upstream at github.com/maci0/openmiles; vendors miniaudio and TinySoundFont | GPL-3.0-only (its vendored libraries: MIT, MIT-0, zlib); see its `LICENSE` and `deps/README.md` |
| `ext/patches/openmiles/` | our fixes to OpenMiles, applied by `build/ext.sh` | GPL-3.0-only, as the code they change |
| `ext/ffmpeg` (submodule) | **FFmpeg**, upstream at github.com/FFmpeg/FFmpeg | LGPL-2.1-or-later as the modern build configures it (no GPL or non-free parts enabled); see its `LICENSE.md` |
| `ext/nolf1-modernizer` (submodule) | **NOLF Modernizer** by HeyThereCoffeee (haekb), upstream at github.com/haekb/nolf1-modernizer. The modern build uses two of its files, the seasonal main menus' Cate models (`ASSETS/CHARS/MODELS/INTHERO_CASUAL.abc`, `INTHERO_WINTER.abc`, linked from `src/modern/data/files/`), which the modern build's archives carry; none of its code | no licence of its own: its readme says it remains under Monolith's end-user licence for the v1.003 source release, as `src/game/` |

## Not in this repository, taken from the retail game at build time

The build takes these from the Game of the Year Edition as the `rez/`
submodule installs it, from the redump images of its two discs that it
fetches from archive.org (`rez/README.md`).

| What | Origin | Where it goes |
|---|---|---|
| the game data: `.rez` archives, configs, text, movies | Monolith Productions / Fox Interactive / Sierra | `out/nolf-legacy-release/` |
| `mss32.dll` and its `.flt`, `.m3d`, `.asi`, `.as6` providers | Miles Sound System, RAD Game Tools | `out/nolf-legacy-release/` (the only binaries in the game not built from source) |
| `binkw32.dll` | Bink Video, RAD Game Tools | `out/nolf-legacy-release/` |
| the launcher's bitmap, palette, music, icon and cursors | Monolith, from the retail `NOLF.exe`'s resources | `src/engine/launcher/res/` (gitignored), compiled into our `NOLF.exe` |
| the dedicated server's two icons and its setup wizard's bitmap | Monolith, from the retail `NolfServ.exe`'s resources | `src/engine/nolfserv/res/` (gitignored), compiled into our `NolfServ.exe` |
| the engine's icon and its console font (a PCX image) | Monolith, from the retail `lithtech.exe`'s resources | `src/engine/lithtech/res/` (gitignored), compiled into our `lithtech.exe` |
| the Game of the Year strings and credits | Monolith, from the retail GOTY `CRES.dll`'s resources | `src/game/ClientRes/goty/` (gitignored), compiled into our `CRes.dll` |

`make progress` also extracts the retail `lithtech.exe`, `server.dll`,
`d3d.ren`, `NOLF.exe`, `NolfServ.exe` and `ltmsg.dll`, and the Game of the Year `CShell.dll`, `Object.lto`
and `CRes.dll` (from `NOLFGOTY.REZ`), and `sres.dll` (from `NOLF2.REZ`), into `out/reference/`, to compare our
build against; they are never run and never go into the game directory.

The retail game DLLs inside the `.rez` archives are overwritten with zeros
when the archives are copied; the retail executables are never copied into
the game directory.

## Not in this repository, provided by you

Optionally, for `make progress`: Monolith's *NOLF Source Code v1.003*
release (`NOLF_SOURCE`), whose prebuilt lithshared libraries the scoring
compares ours against. Only those seven `.lib` files are read.

The compilers in `toolchains/` (Microsoft Visual C++ 6.0, Intel C/C++ 4.0;
see `toolchains/README.md`), under their own licences.
`toolchains/directmusic-5.3.1.904/` also holds Microsoft's DirectMusic DLLs
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
`toolchains/directmusic-5.3.1.904/`), under the licence of Microsoft's DirectX
End-User Runtime they come from. Check its terms before passing on a game
directory that carries them. `lithtech.exe.manifest`, which points the game
at them, was written for this project.
