# Installing and playing

This guide is for players. It explains how to get from a release download to
a running game. To build the game from source instead, see
[`BUILDING.md`](BUILDING.md).

## What to download

Each release has these archives, named for its version (`v0.0.3-alpha`, for
example):

| Archive | What it is |
|---|---|
| `nolfsrc-<version>.tar.gz` | **The one you want.** The game built from source with the modern build's fixes and features. |
| `nolfsrc-debug-<version>.tar.gz` | The same with debug information. If the game crashes, it writes a backtrace to `nolfsrc.log`. |

To try changes before the next release, there is the
[nightly](https://github.com/nolffan/nolfsrc/releases/tag/nightly): the
latest `master`, rebuilt each time a change is merged and its builds pass.
Its archives are named for the commit they were built from,
`nolfsrc-nightly-<commit>.tar.gz` and `nolfsrc-debug-nightly-<commit>.tar.gz`
(the first seven digits of its hash, `nolfsrc-nightly-a16e5e8.tar.gz` for
example), and each unpacks to a directory of that name. It has had no
testing beyond the builds, so expect the occasional new bug; say in a bug
report that you are on a nightly, and which commit. It installs like any
release.

The legacy build, the game built exactly as it shipped in 2001, bugs
included, is kept for archive purposes and is not attached to releases. To
get it, build it ([`BUILDING.md`](BUILDING.md)); each build on GitHub
Actions also keeps it as an artifact for a while.

No game files are included. Each archive has an installer script that copies
the game's data from your own disc images and checks every file it writes.

**Each release is the whole game, not a patch.** Unpack it into a new, empty
directory, never over an earlier release or another installation of the
game. The installer checks this first and stops if the directory holds
anything the release does not: it lists every file of the release, and every
file the installer writes, in `SHA256SUMS`. To keep your saved games, copy
the old directory's `Save` folder into the new one.

Once the game is installed, `sha256sum -c SHA256SUMS` in its directory checks
every file of it (all but your settings, `autoexec.cfg`, which the game
rewrites).

## What you need

- **The two discs of _The Operative: No One Lives Forever_, Game of the Year
  Edition (Europe)**, as disc images. These are the images the installer
  expects:
  - <http://redump.org/disc/42823/>
  - <http://redump.org/disc/42561/>

  An image can be a plain `.iso` (your own rip), redump's `.bin` (or its
  `.cue`), or redump's `.zip` download. A `.bin` or `.zip` is first
  converted to a temporary `.iso`, which needs about 0.6 GB of free space per
  disc; it is removed at the end. Other editions and other regions' discs
  are not supported: the installer checks each image's checksum.
- **On Linux:** Wine with 32-bit support, or Proton (Steam), and `python3`
  if your images are `.bin` or `.zip`.
- **On Windows:** nothing else. The game is a 32-bit Windows program.
- **On a Mac:** CrossOver, or another Wine for macOS, to run it. The
  installer needs nothing more: it is a Perl script, and every Mac has Perl.

## Linux

1. Unpack the archive:

   ```sh
   tar xzf nolfsrc-v0.0.3-alpha.tar.gz    # the version you downloaded
   cd nolfsrc-v0.0.3-alpha
   ```

   Each archive unpacks to a directory of its own name
   (`nolfsrc-debug-v0.0.3-alpha` for the debug one).

2. Install the game data from your disc images, in either order:

   ```sh
   ./linux-install.sh /path/to/disc1.zip /path/to/disc2.zip
   ```

   With no arguments it looks for the images in the game directory and the
   directory above it. You can run it again safely: files already in place
   are checked and kept, and your saves, settings, logs and screenshots are
   left alone.

3. Play:

   ```sh
   ./linux-run.sh
   ```

   This opens the game's launcher: click Launch. The first time, it asks
   for a performance setting first. The launcher's Display and Advanced
   buttons set the renderer, the resolution and the engine's options.

   The game uses its own Wine prefix, `~/.wine-nolf`, which is created on
   the first run, so your own prefix is left alone. Arguments go to the
   game: `./linux-run.sh ++windowed 1` starts it in a window whatever
   Options > Display says. `./linux-run.sh --help` says more.

Sound needs Wine's audio to work on your machine (`winecfg`, Audio tab). The
music needs nothing extra: the game directory has its own DirectMusic.

## Steam Deck and Proton

1. In Desktop Mode, unpack the archive and run `linux-install.sh` as above.
2. In Steam, use Games > Add a Non-Steam Game and pick the game directory's
   `NOLF.exe`.
3. In the shortcut's Properties > Compatibility, force a Proton version.

The controller works as an Xbox-style pad (see [Controller](#controller)).
No winetricks or Protontricks are needed.

## Windows

1. Unpack the archive into a new, empty folder (7-Zip and Windows 11's
   Explorer both open `.tar.gz`).
2. Drop both disc images onto `windows-install.bat`, or put them in the game
   folder and double-click it.
3. Start `NOLF.exe` and click Launch.

Windows support has had less testing than Linux. The programs are not
signed, so SmartScreen may warn before the first start. If something goes
wrong, please report it.

## Mac (CrossOver)

The game is a Windows program, so on a Mac it runs in CrossOver (or another
Wine). Its data is installed in macOS itself, with `mac-install.command`.

1. Unpack the archive into a folder of its own: double-click it in Finder.
2. Put both disc images into the game folder, then double-click
   `mac-install.command`. It runs in a Terminal window and says when it is
   done.

   macOS may refuse to open it, as it was downloaded and is not signed. Then
   allow it in System Settings > Privacy & Security ("Open Anyway"), or run
   it in Terminal, which can also take the images from elsewhere:

   ```sh
   cd ~/Downloads/nolfsrc-v0.0.3-alpha    # the game folder
   ./mac-install.command /path/to/disc1.zip /path/to/disc2.zip
   ```

3. In CrossOver, make a new bottle (Windows 10), then use the bottle's Run
   Command, browse to the game folder's `NOLF.exe` and run it; "Save command
   as a launcher" keeps it for next time. Click Launch.

The game, its movies and its music need nothing installed in the bottle.
If the picture is wrong, try `opengl.ren` or `d3d7.ren` in Options >
Display. The Mac installer has been tested on Linux, which runs the same
Perl, but not on a Mac yet, and the game has not been tried in CrossOver
here: please report how it goes.

## First start

- **Screen.** The game starts full screen, at a resolution that fits your
  screen. Choose the resolution and window mode in Options > Display (or the
  launcher's Display button). Window mode can be Full screen, Window or
  Borderless window, and the game starts as you last left it. Wide screens
  are supported.
- **Renderer.** The default renderer is `d3d9.ren` (Direct3D 9). If you see
  rendering problems, pick `opengl.ren` or `d3d7.ren` (the original DirectX 7
  renderer) in Options > Display. On Intel integrated graphics with
  `d3d7.ren`, turn on "Intel graphics fix" if the screen goes black
  underwater.
- **Frame rate.** Single player is limited to 240 frames a second by default.
  Options > Interface offers 60, 120, 144, 240 or no limit. Multiplayer always
  runs at 60.
- **Crouch.** To press Duck once to crouch and once more to stand, set Options
  > Game > "Toggle crouch" to Yes.

## Controller

An Xbox-style (XInput) controller works in the menus and in the game. That
is also what Steam Input makes of the Steam Deck's controls and of most other
pads.

- In the game, the left stick moves and the right stick looks; the triggers
  fire and zoom.
- In the menus, use the d-pad or a stick to move, A to choose and B to go
  back.
- Start opens the menu; A skips a cutscene.
- Options > Controls > Controller sets the look speed, inverted look and dead
  zones. Customize Controls binds the buttons like keys.

## Console and cheats

Press the key left of `1` (backquote), or Shift+Esc, to open the developer
console in single player. Type `Help` for the commands and `Cheats` for the
cheat codes.

- [`CONSOLE.md`](CONSOLE.md) lists every command and cheat.
- [`VARIABLES.md`](VARIABLES.md) lists the settings you can change there.

If you get stuck in the level geometry, `mpclip` lets you fly out.

## Where things are

| File | What |
|---|---|
| `autoexec.cfg` | Your settings and key bindings, rewritten when the game exits. Delete it to start from the defaults (`defaults.cfg`). |
| `Save/` | Saved games. |
| `modern.log` | The modern build's log, emptied each run. Attach it when you report a problem. |
| `NetHost.txt`, `modern-server.log` | The dedicated server's settings, and its log in the modern build (below). |

## Multiplayer

The internet server list uses the OpenSpy master server
(`master.openspy.net`) in place of GameSpy's, which is gone. The modern build
plays with retail 1.003 players and servers.

## A dedicated server

The game directory also has the game's stand-alone server, `NolfServ.exe`:
a server with no player of its own, that keeps hosting deathmatch or
H.A.R.M. vs UNITY games for others to join. Players join it from the
Multiplayer menu (on the internet list, or over a LAN), or start the game
with `+connect <address>:27888`.

- **Windows:** start `NolfServ.exe`. Its setup wizard asks for the
  session's name, the game type, the number of players, the port, the
  options and the worlds; Finish starts the server.
- **Linux:** `./linux-server.sh`. The first time it shows the same wizard;
  after that it starts straight away with the saved settings. Without a
  display (a server machine, a container) it runs under `xvfb-run`, a
  virtual display, which then needs to be installed (`xvfb`); the wizard
  then cannot be shown, so start it once on a desktop, or give the settings
  on the command line.

The settings are kept in `NetHost.txt` in the game directory. Each can also
be given on the command line as `-<setting> <value>`: `-NetSessionName`,
`-NetPort` (default 27888), `-NetMaxPlayers` (up to 16), `-NetGameType` (2
deathmatch, 1 H.A.R.M. vs UNITY), `-NetUsePassword` and `-NetPassword`, and
the game's options (`-NetEndFrags`, `-NetEndTime`, `-NetRunSpeed`, ...).
`-nowiz` starts without the wizard. For example:

```sh
./linux-server.sh -NetSessionName "My server" -NetMaxPlayers 8
```

The modern build adds to that:

- `-NetLevel <world>`, once per world, sets the deathmatch level list
  (`-NetCALevel` for H.A.R.M. vs UNITY), for example `-NetLevel
  'Worlds\Multi\DeathMatch\001THEPAD_DM'`;
- `-MasterServer <host>` names the master server the game is listed on
  (default `master.openspy.net`; `none` lists it nowhere), which the
  server's console shows when it starts;
- everything the console shows also goes to `modern-server.log`.

The server answers on UDP port 27888 (or the `-NetPort` you give it): let
it through the firewall for players on other machines. It runs until Stop
Server is clicked (or the process is ended).

In the legacy build, `-nowiz` is only read when another argument follows
it (a bug of the original server, which `linux-server.sh` works around).

## Optional native ZIP/REZ mods (modern Windows build)

A distribution prepared with the optional bundled mod component supports:
**drop ZIP/REZ into Mods -> launch NOLF.exe -> Mods -> enable -> Save choices ->
Restart game**. New packages start disabled. There is no player Python setup,
manual extraction or external application requirement. See
[MOD_MANAGER.md](MOD_MANAGER.md) for profiles, conflicts, saves and limitations.
The standard release without the optional component keeps its original launcher.
Release builders use [MOD_HELPER_RELEASE.md](MOD_HELPER_RELEASE.md); the helper
alone does not convert an arbitrary retail installation into a managed one.
