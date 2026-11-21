# No One Lives Forever, built from source

Source code and a build for **The Operative: No One Lives Forever** (Game of
the Year Edition, PC, 2000/2001): every executable and DLL of the game --
the LithTech 2.2 engine (`lithtech.exe`, `server.dll`, the Direct3D renderer
`d3d.ren`), the `NOLF.exe` launcher, the `NolfServ.exe` dedicated server,
`ltmsg.dll`, `sres.dll` and the three game DLLs -- compiled from the source in this repository with the original
2000-era compilers.

<p align="center">
  <img src="screenshots/01-main-menu.jpg" alt="The main menu" width="49%">
  <img src="screenshots/02-unity-headquarters.jpg" alt="UNITY headquarters" width="49%">
  <img src="screenshots/03-morocco-rooftops.jpg" alt="Misfortune in Morocco: the rooftops of Marrakech" width="49%">
  <img src="screenshots/04-morocco-alley.jpg" alt="Misfortune in Morocco: a guard in an alley, with his shadow" width="49%">
  <img src="screenshots/05-berlin-by-night.jpg" alt="Berlin by Night" width="49%">
  <img src="screenshots/06-plane.jpg" alt="Unexpected Turbulence: on board the plane" width="49%">
  <img src="screenshots/07-fireplace.jpg" alt="The Dive: a room with a fireplace" width="49%">
  <img src="screenshots/08-alpine-village.jpg" alt="Alpine Intrigue: a village in the German Alps" width="49%">
</p>

<p align="center"><sub>The modern build at 1920x1080: the Direct3D 9 renderer with antialiasing, model shadows, 32-bit textures and 16x anisotropic filtering.</sub></p>

The engine source in `src/engine/` is a reconstruction: Monolith never
released it, so it was written to recompile to the retail machine code. The
game code in `src/game/` is Monolith's own NOLF Source Code v1.003 release,
which predates the Game of the Year Edition: the GOTY changes to it,
reconstructed from the retail GOTY game DLLs, are made in place and marked
`GOTY:`, and the modern build's changes sit inside `#ifdef NOLFSRC_MODERN`
blocks. See [`docs/LICENSE_NOTES.md`](docs/LICENSE_NOTES.md) for what is whose.

This project, and its modern build in particular, is not made or supported by
Monolith Productions.

**No unchanged game data is included.** The build takes the game's data
(and the Miles and Bink sound/video runtime, the only binaries not built
from source) from the retail Game of the Year Edition, which the build
installs from your own images of its two discs (bring your own: see below).
There are two exceptions, both committed in `assets/` (`assets/README.md`):
a small set of retail data files that the modern build changes (about 2.6
MB: bug fixes, and changes as a mod makes them), which go into its
`nolfsrc.rez`; and the retail art and text that our binaries are compiled
with (about 0.5 MB: the icons, the launcher's splash bitmap and sound, the
dedicated server's wizard bitmap, the console font, and the Game of the
Year strings and credits). The modern
build replaces that runtime with open-source builds from `ext/`: OpenMiles
for Miles, and our own Bink player on FFmpeg.

## Progress

How much of each binary compiles to exactly the bytes of the retail game is
in [`progress/README.md`](progress/README.md), with how it is scored.

## Build

You need Linux with Docker, the submodules, and your own images of the two
discs of the Game of the Year Edition (Europe):

1. Fetch the submodules: `git submodule update --init` (or clone with
   `--recurse-submodules`). `ext/toolchains/` is the compilers (about 560 MB,
   see `ext/toolchains/README.md`); `ext/tools/` is the build's helpers and the
   reconstruction tools and guides; `ext/openmiles` and `ext/ffmpeg` are the
   modern build's open-source middleware (about 130 MB), and
   `ext/nolf1-modernizer` the NOLF Modernizer, two of whose models the
   modern build uses (about 60 MB); `ext/lithtools` is our LithTech tools,
   whose `lithrez` packs the game's `nolfsrc.rez`.
2. Put your two disc images in `isos/` (a `.iso`, a `.bin` with its `.cue`,
   or a `.zip` of one; a hard link is good: `ln <image> isos/`), or set
   `NOLF_ISOS=<directory>`. The first build installs the game from them
   into `out/retail/` and checks every file. See
   [`docs/BUILDING.md`](docs/BUILDING.md), "What you need".
3. Run

```sh
make            # -> out/nolf-legacy-release/ and its -private / -public .tar.gz
                #    (about 3 minutes; the first time, also the docker image
                #    and the retail game's install from your images)
make test       # optional: plays two levels under Xvfb with scripted input
make debug      # optional: the same game with symbols and debug logging,
                #    in out/nolf-legacy-debug/
make modern     # optional: the game with the retail bugs fixed, in
                #    out/nolf-modern-release/ (both: docs/BUILDING.md, "Variant builds")
make progress   # optional: how much of it matches the retail binaries
```

`make help` lists the other targets. [`docs/BUILDING.md`](docs/BUILDING.md) explains the build in
detail.

## Play

See [`docs/INSTALL.md`](docs/INSTALL.md). What the modern build changes is
listed in [`CHANGELOG.md`](CHANGELOG.md); to play a build of your own, see
[`docs/BUILDING.md`](docs/BUILDING.md#playing-your-build).

## Contributing

The remaining work is functions that are written but not yet byte-identical,
and functions not written yet. `ext/tools/guides/` explains the method, from reading the
retail machine code to the compiler quirks that decide the last few bytes;
`make progress` (after `make`) shows where things stand and catches a change
that breaks a function that matched before. The CI has no retail game: it
builds the four variants and their public archives without it, and checks
the build against hashes that `make progress-update` records (`make ci`;
see `progress/README.md` and `docs/BUILDING.md`, "The CI build"). It cannot
find new matches, so run `make progress-update` with your disc images
before you send a change.

## Credits

This project builds on the work of others:

- **Monolith Productions**, for _No One Lives Forever_ and for releasing its
  source code (v1.003, 2001), which `src/game/` and `src/LT2/` are, and the
  _No One Lives Forever 2_ source release, from which much of
  `src/lithshared/` is adapted.
- [StenApp](https://github.com/StenApp) for generously sharing his invaluable,
  in-depth knowledge of all things NOLF and LithTech.
- **NOLF Modernizer** by HeyThereCoffeee (haekb):
  <https://github.com/haekb/nolf1-modernizer>. Most of the modern build's
  fixes and features were found and designed there; `docs/MODERN.md` names
  the Modernizer change behind each one. The seasonal main menus' Cate
  models are its own, used as they are.
- **ChthonVII's Modernizer fork**: <https://github.com/ChthonVII/nolf1-modernizer>.
  It gathers the crouch toggle and Daniel Gibson's fixes, and sorts out what
  the different "Patch 4" releases contain.
- **Daniel Gibson**, for the fixes in
  <https://github.com/haekb/nolf1-modernizer/pull/55>: Shift+Esc for the
  console, closing popups with Activate, each weapon keeping its ammo, and
  the sunglasses' zoom on wide screens.
- **The author of the "crouch toggle" Modernizer** on Mod DB, for the crouch
  toggle.
- **OpenMiles** (<https://github.com/maci0/openmiles>), the open
  reimplementation of the Miles Sound System that the modern build uses in
  place of Miles, with the miniaudio and TinySoundFont libraries it vendors.
- **FFmpeg** (<https://ffmpeg.org>), on which the modern build's Bink player
  is built; **zlib**; and **Capstone**, which the tools use to read the
  retail machine code.
- **Wine**, **Proton** and **DXVK**, which run the game on Linux and the
  Steam Deck, and **OpenSpy** (<https://openspy.net>), whose master server
  replaces GameSpy's.
- **redump.org**, for the verified disc images the installers check
  against.

The modern build reimplements the Modernizer's changes rather than copying
its code (`docs/MODERN.md`, "Rules for this work"); of its assets, it uses
the two seasonal menu models.
