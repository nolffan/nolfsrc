# Building

`make` is the whole build. This file describes what it does, and how to run
parts of it by hand.

## What you need

* **Linux with Docker.** The compilers are Win32 programs and run under Wine
  in a container; the host needs nothing else (no Wine, no Python).
* **The submodules**: `git submodule update --init`.
  * `toolchains/`, the compilers (about 560 MB). See `toolchains/README.md`.
  * `rez/`, the retail game the build takes its data from. It fetches the
    redump images of the two discs of *No One Lives Forever: Game of the Year
    Edition* (Europe) from archive.org, checks them, and installs the game
    from them as its own setup would, into `rez/games/nolf-eu-goty/`, checked
    file by file against `rez/games.sha256`. `make` does this the first time
    (`make retail`); see `rez/README.md`.
  * `tools/`, the build's helpers (the scorer, the object makers, the
    playtest, ...) and the reconstruction tools and guides
    ([nolffan/tools](https://github.com/nolffan/tools)). Every build needs it.
  * `ext/openmiles` and `ext/ffmpeg`, the upstream projects the modern build
    uses in place of Miles and Bink ("The modern build's middleware" below).
    The release build does not need them.
* About 9 GB of disk: 2.6 GB for the docker image, 2.8 GB for `out/` (the
  game directory, its two .tar.gz, and the 0.6 GB Wine prefix), about 3 GB in `rez/`
  (the disc images, zipped and unzipped, and the installed game), a few
  hundred MB of objects.

## What `make` does

1. Checks the compilers are in `toolchains/` and that `rez/` is there.
2. Builds the docker image `nolfsrc-build` from `docker/Dockerfile` if
   needed (a few minutes, once).
3. `make retail`, in a container: the first time, fetches the two disc
   images and installs the retail game from them (`rez/fetch-isos.sh
   nolf-eu-goty`, `rez/install-games.sh nolf-eu-goty`); every time, checks
   the installed files against `rez/games.sha256` (the result is remembered
   in `out/.retail-verified` while the files are unchanged). See
   `build/retail.sh`.
4. Runs `build/all.sh` in a throwaway container (`--rm --init`, named
   `nolfsrc-build`), as your user, with the checkout (and so `rez/`) mounted
   at `/repo`.

`build/all.sh` then:

| Step | What | Output |
|---|---|---|
| 1. resources | game data from the retail game: the four `.rez` archives, configs, text, movies -- with the retail game DLLs inside the archives overwritten with zeros | `out/nolf-legacy-release/` |
| 2. third-party runtime | Miles Sound System (`mss32.dll` and its `.flt` / `.m3d` / `.asi` / `.as6` providers) and Bink (`binkw32.dll`) from the retail game: licensed middleware with no source, which the engine imports | `out/nolf-legacy-release/` |
| 2b. launcher, server and engine art | the splash bitmap, palette, music, icon and cursors `launcher.rc` compiles into `NOLF.exe`, read out of the retail `NOLF.exe`'s resources; the icons and wizard bitmap `nolfserv.rc` compiles into `NolfServ.exe`, out of the retail `NolfServ.exe`'s; the icon and console font `lithtech.rc` compiles into `lithtech.exe`, out of the retail `lithtech.exe`'s. The text resources (dialogs, string tables, version blocks) are `.rc` source in the repository: `launcher.rc`, `nolfserv.rc`, `lithtech.rc`, `server/server.rc`, `d3d/d3d.rc`, `ltmsg.rc`; MFC's own resources come from the toolchain's `afxres.rc` | `src/engine/launcher/res/`, `src/engine/nolfserv/res/`, `src/engine/lithtech/res/` |
| 2c. Game of the Year text | the strings of the Game of the Year content (the "Rest and Relaxation" mission) and the GOTY credits, which Monolith's v1.003 source of `CRes.dll` lacks, read out of the retail GOTY `CRES.dll` (in `NOLFGOTY.REZ`); compiled into our `CRes.dll` beside `ClientRes.rc`, so its resources are the retail ones | `src/game/ClientRes/goty/` |
| 3. compile | `build/compile.sh`: every compile and link, by `build/build.mk` (below) -- the lithshared libraries (`RegMgr` twice, as Monolith built it for each game DLL: the Release build for `NOLF.exe` and `Object.lto`, the Debug build, `RegMgr_Debug.lib`, for `CShell.dll`, which the Game of the Year `CShell.dll` linked; and `ButeMgr` twice, without MFC for the game DLLs and with it for `NolfServ.exe`); the game DLLs `CRes.dll`, `CShell.dll`, `Object.lto`; the engine, `ltmsg.dll`, `sres.dll`, `NOLF.exe` (the launcher), `NolfServ.exe` (the dedicated server), `server.dll`, `lithtech.exe`, `d3d.ren` | `src/lithshared/out/*.lib`, `src/game/*/Release`, `Final_Release`, `src/engine/*/Release` |
| 4. assemble | the built binaries into the game directory; the game DLLs and `sres.dll` go in `custom/`, the rez directory the engine searches last (and that `startup.txt`, the rez list of the dedicated server `NolfServ.exe`, gets as its last entry) | `out/nolf-legacy-release/` |
| 5. audit | every PE file in `out/nolf-legacy-release/` listed with its origin (`out/nolf-legacy-release/MANIFEST.txt`); the build fails if one is unaccounted for, or if a `.rez` archive still holds one | |
| 6. package | `out/nolf-legacy-release/` as a compressed tar, whole (`-private`), and without the disc data plus the scripts that install it from the player's disc images and a `SHA256SUMS` of every file, which they check the directory against first (`-public`; `build/package.sh`, `build/mkinstall.py`) | `out/nolf-legacy-release-private.tar.gz`, `out/nolf-legacy-release-public.tar.gz` |

Every binary in `out/nolf-legacy-release/` is built from this repository except the 35
Miles and Bink files of step 2; the audit prints them all. The steps are
idempotent: running `make` again rebuilds only what changed and leaves the
data in place.

### build/build.mk

Step 3 is one Makefile, `build/build.mk`, for every binary: the libraries,
the game DLLs and the engine are one dependency graph, so `make -j`
compiles objects of all of them at once, one job per CPU (`make JOBS=<n>`
for another number), and a rebuild redoes only what changed. An object is
rebuilt when its source, a header it includes, or its command line changes;
a binary when one of its inputs does. VC6 cannot list the headers a file
includes (`/showIncludes` came with VS2005), so `tools/mkdeps.py` finds them
the way the compiler does, and every command line is kept beside its output
(`<output>.cmd`) to be compared with the next. A one-file change rebuilds in
seconds; a full build takes about half the time it did as a sequence of
batch compiles (more on a fast disk: Wine's file access is most of a
compile). `make clean` starts from nothing.

Each compile is the command the original build scripts ran, flag for flag,
one source at a time instead of one batch per directory -- and the
binaries come out the same, byte for byte (only the time stamps the tools
write differ; `tools/buildcmp.py` compares two sets of build outputs that
way). The objects themselves are not reproducible from one build to the
next, with any build: VC6 leaves memory it never initialised in some fields
and orders some symbols and relocations differently, none of which reaches
a linked image.

The compiles run under Wine in two prefixes the Makefile keeps in the tree
it builds (`.wine/`), one for the game DLLs (the drives `S:` and `T:`,
below) and one for everything else.

## Other targets

```sh
make test         # build/playtest.sh with tools/playtest-basic.txt on
                  #   Worlds\T01S01 and Worlds\M03S01, under Xvfb; passes if the
                  #   game is still running after the script on both
make test TEST_LEVELS="M06S01"   # other levels
make progress     # score the build against the retail binaries, function by
                  #   function (and resource by resource); fails if a function
                  #   recorded as byte-identical in progress/identical.txt no
                  #   longer is, or if the text in an image's data differs from
                  #   the retail text in a way progress/data-known.txt does not
                  #   list (tools/datacheck.py)
make progress NOLF_SOURCE=<dir>  # the same, with the lithshared libraries
                  #   (<dir>: the NOLF Source Code v1.003 release)
make progress-update NOLF_SOURCE=<dir>   # record the score in progress/ and
                  #   README.md
make shell        # a shell in the build container, at /repo
make clean        # remove out/ and every build output in the source tree
make distclean    # clean, and remove the docker image
```

`make test` leaves each run in `out/play/test-<level>/`: `result.txt`,
`wine.log`, `script.log`, the screenshots in `shots/` and a contact sheet
`shots.png`.

The dedicated server has a test of its own, `tools/servertest.sh` (in
`make shell`): it starts the game directory's `NolfServ.exe` unattended
under Xvfb, hosting a deathmatch, checks it hosts and stops cleanly, and
with `SERVERTEST_CLIENT=1` has the game join it; the run, its screenshots
and logs are in `out/play/<game directory>-server/`:

```sh
SERVERTEST_CLIENT=1 tools/servertest.sh out/nolf-legacy-release 60
```

`make progress` scores what the last `make` built (it compiles nothing), so
run `make` first; the retail images it compares against are extracted from
the retail game into `out/reference/` the first time. `progress/README.md` describes
the scoring and the test.

## Running parts of the build

Inside `make shell` every script runs on its own:

```sh
build/compile.sh                      # every binary (build/build.mk all)
build/compile.sh lithtech d3d         # some (libs, game, engine, a binary, <binary>-objs,
                                      #   or any output file)
build/libs.sh stdlith                 # one library (or: all)
build/build.sh cshell final           # one game DLL (cres | cshell | object | all)
build/engine.sh server                # one engine target's objects ...
build/engine.sh server-link           # ... and its link
build/all.sh                          # the whole thing
build/reference.sh                    # the retail images, into out/reference/
build/engine.sh server verify         # compile, then match every function
                                      #   against the retail server.dll
build/engine.sh server-one smgr.cpp PlaySound   # recompile one file, match it
build/engine.sh nolfserv verify       # the dedicated server's objects, matched
build/engine.sh nolfserv-one ServerDlg.cpp OnTimer   # (likewise one file)
build/playtest.sh out/nolf-legacy-release 'Worlds\M01S01' 60   # idle for 60 s on a level
tools/servertest.sh out/nolf-legacy-release 60  # host with NolfServ.exe for 60 s
```

All of them build through `build/build.mk`, which has every compiler and
flag (they differ per binary, and for some files within one, because they
reproduce what the original build used). `build/env.sh`, which every script
sources, defines `run_cl`, `run_link`, `run_lib`, `run_rc` and `run_icl` (the
compilers under Wine) for one-off commands. `src/engine/server/` is
compiled into both `server.dll` and `lithtech.exe`; a full build rebuilds
both after a change to it, a `*-one` loop only the object it names.

The `verify` and `*-one` targets are the reconstruction loop:
`tools/guides/02-the-loop.md` explains it, and `tools/guides/` the rest of the method.

## Variant builds: debug and modern

`make` builds the release game: what the retail game was, compiled to its
exact machine code, and the only build `make progress` ever scores. Two
project defines add to it, each in a build of its own:

| | define | for | into |
|---|---|---|---|
| `make debug` | `NOLFSRC_DEBUG` | symbols, logging and crash backtraces: finding out why something goes wrong | `out/nolf-legacy-debug/` |
| `make modern` | `NOLFSRC_MODERN` | the retail bugs fixed (`RETAIL_BUGS.md`), and later new features | `out/nolf-modern-release/` |
| `make modern-debug` | both | debugging the modern build | `out/nolf-modern-debug/` |

All three are `build/variant.sh <variant>`. Each copies the source (without
any build output) into `out/build-<variant>/` and builds it there with the
same `build/all.sh`, so `src/*/Release` (what `make progress` scores),
`out/nolf-legacy-release/` and the release command lines are left exactly as they were.
The copy keeps its build outputs between runs, so a variant also rebuilds
only what changed.
`build/env.sh` passes the extra flags to the compilers only inside those
copies, and refuses them anywhere else, so neither define can reach the
release build or the scoring by accident. The first run of each variant
extracts the game data again (about 1.1 GB); later runs take as long as
what changed takes to rebuild. Each is packaged like the release game, as
`out/<directory>-private.tar.gz` and `-public.tar.gz`
(`out/nolf-modern-release-private.tar.gz`, ...); `NOLF_ARCHIVES=public
make ...` writes only the public one, and fails rather than skip it without
the disc images. Play them as the release game, from the directory the
table gives: `out/nolf-modern-release/linux-run.sh` (the launcher), or
`build/playtest.sh out/nolf-modern-release 'Worlds\T01S01'`.

The GitHub Actions workflow `.github/workflows/public-archives.yml` builds
the release game and all three variants that way on every push to `master`
and every pull request, each in a job of its own and all four at the same
time, runs `make progress` on the release build (a
function recorded as byte-identical that stops matching fails the run), and
keeps the four public archives as the run's artifacts, each the `.tar.gz`
itself rather than a zip around it. It caches the two redump zips (keyed on
`rez/isos/SHA256SUMS`) and the docker image's layers, so only the first run
downloads from archive.org.

### The modern build: `NOLFSRC_MODERN`

`make modern` compiles everything with `/D NOLFSRC_MODERN` and nothing else
changed. Every departure from the retail game -- a bug fix, later a new
feature -- goes behind it, with the retail code kept, unchanged, in the other
branch:

```cpp
#ifdef NOLFSRC_MODERN
    if (stricmp(pFilter, pName) == 0)       // fixed: report the filter that matches
#else
    if (stricmp(pFilter, pName))            // retail: reports the first that does not
#endif
```

The release build never sees such a block: `build/compile.sh` (and
`build/libs.sh`, `build/build.sh`, `build/engine.sh`) compile a copy of
`src/` in `out/release-tree/` with
every `#ifdef`/`#ifndef NOLFSRC_MODERN` (and `NOLFSRC_DEBUG`) block resolved
and its directives removed (`tools/releasesrc.py`), then copy their outputs
back into `src/`. Merely skipping the blocks is not enough: VC6 lets a file's
layout, skipped lines included, decide where some out-of-line inline
functions go (`src/modern/README.md`). So the release build compiles exactly
what it compiled before -- as long as nothing is added outside a block, not
even a blank line; `python3 tools/releasesrc.py diff master` lists anything
that is. It still matches the retail binaries and `make progress` still
passes; check that after any such change (`build/engine.sh <target>-one
<file>` shows the function still matching). Test the two macros only with
`#ifdef`/`#ifndef`: anything else stops the release build with an error.

The game DLLs are also built from the same paths wherever the checkout is:
`build/build.mk` makes the tree's `src/` Wine's drive `S:` and its
`toolchains/` drive `T:` (in a Wine prefix of their own), so their compiles
run in `S:\game\...` with
`T:\...` on the include path. VC6's output for `CShell.dll` and
`Object.lto` depends on those paths (the same source built in two
directories of different path lengths gave different binaries), so without
this a worktree, the main checkout and `out/release-tree/` would each build
their own. The engine and the libraries keep `Z:` (`/`): their output never
depended on the path, and Intel C/C++ builds a different `d3d.ren` from a
working directory on a drive of its own (`build/build.mk`). The modern build is never scored -- it differs from
the retail game on purpose. When a fix lands, mark its entry in
`RETAIL_BUGS.md` as fixed, naming the file.

### The modern build's middleware: `ext/`

The release build plays its sound through RAD's Miles Sound System 6.0i and
its movies through RAD's Bink 1.0h, both taken from the retail game
(`build/all.sh` step 2). The modern build uses neither: `build/variant.sh`
sets `NOLF_MIDDLEWARE=open`, and step 2 then runs `build/ext.sh`, which
builds

| File | From | In place of |
|---|---|---|
| `mss32.dll` | [OpenMiles](https://github.com/maci0/openmiles) (`ext/openmiles`), a clean-room Miles reimplementation, with the Miles 6.0 export table (`-Dmss-version=6.0`) | Miles 6.0i and its 25 provider files (`.m3d`, `.flt`, `.asi`, `.as6`): OpenMiles mixes, positions and decodes MP3 itself |
| `binkw32.dll` | our Bink player, `src/modern/binkw32/`, linked with a static [FFmpeg](https://ffmpeg.org/) (`ext/ffmpeg`) cut down to its Bink demuxer and decoders | Bink 1.0h |
| `d3d9.ren` | our Direct3D 9 renderer, `src/modern/render/` with its `d3d9/` backend (MODERN.md, M-P0-05) | `d3d.ren` as the default renderer (our `d3d.ren` is installed beside it as `d3d7.ren`) |
| `opengl.ren` | our OpenGL renderer, `src/modern/render/` with its `gl/` backend (MODERN.md, M-P0-04) | nothing: picked with `"RenderDll" "opengl.ren"` |

into `out/ext/` (shared by every modern variant, rebuilt only when a
submodule's commit, the build script or the renderer's source changes; the first build takes about
two minutes), and installs them in the game directory, removing any retail
Miles or Bink file a previous build left there. The audit lists them as
`EXT`. The engine is unchanged: it loads whichever `mss32.dll` and
`binkw32.dll` are beside it. Because the movies now play reliably under
Wine, the modern build's `nolfcmds.txt` leaves out `+DisableMovies 1`.

The same step installs Microsoft's DirectMusic, so the music plays under Wine
and Proton without winetricks: the nine DLLs in
`toolchains/directmusic-5.3.1.904/` (checked against its `SHA256SUMS`) go in
as `ms_<name>.dll`, with `src/modern/directmusic/lithtech.exe.manifest`,
which points the engine's DirectMusic classes at them (`src/modern/directmusic/README.md`, MODERN.md
M-P4-12). The audit lists them as `THIRD-PARTY`. Without
`toolchains/directmusic-5.3.1.904/` the build warns and installs neither.

Everything in `ext/` is compiled with [Zig](https://ziglang.org/) 0.16.0,
which the build image installs (pinned and checksummed in
`docker/Dockerfile`, the version `ext/openmiles/build.zig.zon` asks for):
OpenMiles is written in Zig, and `zig cc` is clang with its own mingw-w64
headers and libraries, so FFmpeg and the Bink player are cross-compiled for
32-bit Windows with it too. The same compiler targets Linux and macOS, which
a native port of the game would need. None of this touches the 2000-era
compilers in `toolchains/`, which still build every game and engine binary.

Each upstream project is a submodule under `ext/`, pinned to a release
(OpenMiles v0.4.0, FFmpeg n8.1.3; `ext/ffmpeg` is a shallow clone). Nothing
is written into `ext/`: the builds happen in `out/ext/`. Where upstream needs
a fix for this game, the fix is a patch in `ext/patches/<project>/`, applied
in order to a copy of the source in `out/ext/` (the submodule stays exactly
upstream's); each patch's header says what it fixes. Today there are five,
each making OpenMiles behave as Miles 6.0i does, or better:

* `0001`: `AIL_init_sample` cleared a sample handle's user data, which the
  engine relies on to recycle its voices -- the game ran out of voices after
  a few dozen sounds and went silent (dialogue stopped mid-conversation).
* `0002`: the 0..127 sample volume followed later Miles versions' much
  steeper curve, combined with the master volume and the pan as exact
  floating-point gains. Miles 6.0i's arithmetic is integer and linear,
  measured from the retail DLL (a test tone played through each at every
  volume under Wine, with ALSA's output recorded to a file): each volume
  is scaled to 0..128, a sample's level is (sample * master) >> 7, and a
  pan's far side is its distance from that edge over 64.5. OpenMiles now
  does the same, and the two agree within 0.01 dB at every volume, master
  volume and pan, and for the game's own PCM, IMA ADPCM and MP3 sounds.
  The engine lowers the volume of distant speakers, so their dialogue had
  been 10-30 dB too quiet to hear; a first, interpolated version of the
  curve was 1.5-3 dB too loud at the middle and low volumes most sounds
  use (issue #31).
* `0003`: `AIL_open_stream` did not understand the `\\<handle>` file names
  the engine streams a sound inside a `.rez` archive with (an open file
  handle, seeked to the sound), so every such sound was silent -- the
  opening credits' song in T01S01 among them, danced to in silence. It now
  reads the sound from the handle.
* `0004`: each sound is resampled to the 44100 Hz mix on its own, by
  miniaudio's pitch resampler, which miniaudio runs without its low-pass
  filter -- so a 22050 Hz sound (most effects) came out with its mirror
  image at 11-22 kHz, 30-45 dB down. The filter is now on at its highest
  order: the image is 58-84 dB down above 12 kHz. An improvement on retail
  rather than a match: Miles 6.0i avoids the image only by mixing at the
  22050 Hz NOLF asks for, which also cuts the dialogue (44100 Hz) off at
  10 kHz, where OpenMiles keeps it to 14 kHz.
* `0005`: a sample's loop block (`AIL_set_sample_loop_block`) is in bytes
  of the sample's own data, which for IMA ADPCM are compressed blocks;
  OpenMiles divided the offsets by the decoded frame size, so an ADPCM
  sound looped a stretch an eighth of the way in. The engine loops a
  sound over its WAV cue region: the crane in M05S03 (`LIFT3.wav`) looped
  66 ms of its start for as long as it moved, a rattling drill (issue
  #31). Miles 6.0i loops at the start of the block an offset falls in, as
  recorded in-game, and OpenMiles now does the same.

Send such fixes upstream too.

### The debug build: `NOLFSRC_DEBUG`

`make debug` builds the same game with debug symbols and the project's
debug-only diagnostics compiled in, for finding out why something misbehaves
or crashes.

What differs from the release build: every compile adds `/Z7 /Oy-
/D NOLFSRC_DEBUG` (debug information, frame pointers kept so a backtrace can
walk the stack, and the project's debug define; every other flag, the
optimisation included, is the release one), and every link adds `/DEBUG
/OPT:REF /INCREMENTAL:NO`. Each binary's `.pdb` and `.map` are put beside
it.

In the source, use `src/nolfsrc/nolfsrc_debug.h` (on every compiler's include
path, so any file of any target can include it):

```cpp
#include "nolfsrc_debug.h"

NOLFSRC_LOG(("loading world %s, %d objects", pName, nObjects));  // note (( ))
NOLFSRC_ASSERT(pObj != NULL);
#ifdef NOLFSRC_DEBUG
    // ... anything else that must exist only in the debug build ...
#endif
```

Without `NOLFSRC_DEBUG` the macros expand to nothing, arguments included, so
they can stay in the source; never put a side effect in their arguments. Log
lines go to `nolfsrc.log` in the game's working directory (`$NOLFSRC_LOG`
names another file), stamped with the time and source position, and to
`OutputDebugString` (`WINEDEBUG=+debugstr` shows them).

`lithtech.exe` installs a crash handler in the debug build
(`src/nolfsrc/nolfsrc_crash.h`, from `WinMain`): an unhandled exception
anywhere in the process -- engine, renderer or game DLLs -- writes a
backtrace to `nolfsrc.log`, for example

```
[ 450171572] crash: CRASH: exception 0xc0000005 at 0x025a6d1c
[ 450171940] crash:   #0  0x025a6d1c d3d.ren+0x36d1c  (0x10036d1c in its .map)
[ 450171970] crash:   #1  0x0041ac50 lithtech.exe!r_InitRender+0x130  [clientrender.cpp:1207]
[ 450171971] crash:   #2  0x00409eef lithtech.exe!ci_SetRenderMode+0x9f  [clientdsi.cpp:398]
[ 450171998] crash:   #3  0x01a8481f cshell.dll!CGameClientShell::OnEngineInitialized+0x1a0f  [GameClientShell.cpp:1037]
```

and the crash then goes on as it would have (Wine's own crash dialog, or the
playtest's CRASHED). A frame with no name is in code without debug
information -- the static C runtime, or a `d3d.ren` file compiled with
`/Qip`, for which Intel C/C++ 4.0 writes none -- and is given at the
binary's preferred base; `tools/mapaddr.py out/nolf-legacy-debug/d3d.map
0x10036d1c` names it from the map.

The playtest runs the game in `out/play/<name>/`, so a playtest's
`nolfsrc.log` is there.

## Notes

* The build is deterministic for a given checkout: rebuilding gives the same
  bytes except for the timestamps the linker writes. (The game DLLs also
  vary by a few bytes between builds made at different times; the engine
  binaries do not.)
* The game DLLs are the `Final_Release` configuration (`_REZFILE`, `_FINAL`),
  which reading data from `.rez` archives requires.
* `out/nolf-legacy-release/nolfcmds.txt` is the command file the playtest starts the
  engine with (`linux-run.sh`'s launcher writes its own): the rez stack of the retail GOTY install with
  `custom/` last, a 640x480 window, and the intro movies off (they stall
  under some Wine setups).

## Optional native mod manager component

The modern build compiles the native Mods folder and launcher bridge. To prepare
a self-contained Windows component and a separate managed installation from an
owned modern game, follow [MOD_HELPER_RELEASE.md](MOD_HELPER_RELEASE.md). Python
and PyInstaller are build-time tools only; players use NOLF.exe and its native
menu. This optional process leaves the legacy build, public disc installer and
original game input intact. No runtime binaries or retail assets are committed.
