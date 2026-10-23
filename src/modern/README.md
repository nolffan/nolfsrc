# src/modern/ -- the modern build's own code and data

Everything here is used only by the modern build (`make modern`,
`NOLFSRC_MODERN`; `BUILDING.md`, "Variant builds"), never by the release
build, which stays the retail game. `MODERN.md` is the plan and the record of
what has been done.

| Directory | What | Used by |
|---|---|---|
| `cshell/` | new game code for `CShell.dll`: every `.cpp` here is compiled into it (with `CShell.dll`'s own flags and include paths, without its precompiled header -- include `stdafx.h` yourself) | `build/build.sh cshell` |
| `object/` | the same for `Object.lto` | `build/build.sh object` |
| `engine/` | new engine code for `lithtech.exe` (the controller device): files named in `build/build.mk`'s `LITHTECH_SRC`, compiled with the engine's own flags | `build/build.mk` |
| `cres/` | new strings for `CRes.dll`: every `.rc` here is compiled into it beside `ClientRes.rc`; put the IDs in a `.h` beside it | `build/build.sh cres` |
| `data/` | game-data recipes, applied to the data from the retail game (`data/README.md`) | `build/variant.sh`, `tools/moddata.py` |
| `render/` | the modern build's renderers the engine can load instead of `d3d.ren`: a renderer core over a device backend, `opengl.ren` on OpenGL (`render/README.md`); C++ compiled with `zig c++`, not VC6 | `build/ext.sh` |
| `binkw32/` | the modern build's `binkw32.dll`: the Bink entry points `lithtech.exe` uses, playing the movies with FFmpeg (`ext/ffmpeg`); C, compiled with `zig cc`, not VC6 | `build/ext.sh` |

In a modern build `src/modern/` is on the game DLLs' include path, so retail
files reach the new code with, for example, `#include "cshell/p1_input.h"`
inside an `#ifdef NOLFSRC_MODERN` block. Neither `CShell.dll` nor
`Object.lto` needs a project-file (`.dsp`) change for a new file, and no new
string goes into `ClientRes.rc`.

Conventions, so that work done in parallel merges cleanly:

* One file per package and topic, named for it: `cshell/p1_input.cpp`,
  `cshell/p2_console.cpp`, `cres/p3.rc` + `cres/p3.h`.
* String IDs from the package's range (`MODERN.md`); wording of our own.
* In retail files, each change is an `#ifdef NOLFSRC_MODERN` block opened by
  a comment naming the item (`// M-P1-01: frame limiter`), preferably a single
  call into the package's file, with the retail code kept in the `#else`.
* Add nothing outside the block, not even a blank line: the release build
  compiles each retail file with its `NOLFSRC_MODERN` blocks taken out
  (below), so a line outside a block is a change to the release build.
  `python3 tools/releasesrc.py diff master` lists every such change.

The release build never sees these blocks at all. Skipping them is not
enough with VC6: a skipped block still moves everything below it, and the
compiler lets a file's layout decide where some out-of-line inline
functions go, so blocks from several packages in `GameClientShell.cpp`
reordered two functions in the release `CShell.dll` with every release
token unchanged (and restoring the line numbers with `#line` did not help).
So `build/libs.sh`, `build/build.sh` and `build/engine.sh` compile a copy of
`src/` in `out/release-tree/` with every `#ifdef`/`#ifndef NOLFSRC_MODERN`
(and `NOLFSRC_DEBUG`) block resolved as the release build sees it
(`tools/releasesrc.py`; `build/env.sh`, `nolfsrc_release_tree`), and copy
the build outputs back into `src/`. Test the variant macros only with
`#ifdef` or `#ifndef`; anything else (`#if defined(...)`, `#elif`) stops the
release build with an error.

## Optional native mod manager

`cshell/mod_manager.*` uses the stock NOLF controls and a generated catalog.
`launcher/mod_startup.h` and `mod_helper_launch.h` start the adjacent bundled
helper without a shell or interpreter lookup; missing components preserve the
original launcher path. The layout is a player-data recipe under `data/`.
All retail-file hooks remain inside `NOLFSRC_MODERN`. There is no gameplay
adapter dependency. See [the release contract](../../docs/MOD_HELPER_RELEASE.md)
and [validation](../../docs/MOD_MANAGER_VALIDATION.md).
