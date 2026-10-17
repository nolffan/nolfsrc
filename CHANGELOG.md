# Changelog

What the modern build changes from the retail game: _The Operative: No One
Lives Forever_, Game of the Year Edition, version 1.003.

The legacy build is the retail game rebuilt from source, unchanged, bugs
included; none of this applies to it. [`docs/MODERN.md`](docs/MODERN.md)
covers each change in detail, under its item number (M-P1-01 and so on).
[`docs/RETAIL_BUGS.md`](docs/RETAIL_BUGS.md) describes the retail bugs.

## Unreleased

Since the first public release:

- **The briefing room's Union Jack the right way round.** Every UNITY HQ
  level maps the flag mirrored, so it showed inverted; the modern build
  installs the texture mirrored, which turns it back. (M-P4-13, issue #26)
- **The dedicated server, `NolfServ.exe`**, is built from source and in the
  game directory of every build; `linux-server.sh` runs it with Wine,
  without a display if need be ([`docs/INSTALL.md`](docs/INSTALL.md), "A
  dedicated server"). In the modern build it logs to `modern-server.log`,
  starts unattended from its command line, shows its master server, and has
  four retail bugs fixed. (M-P6-11 to M-P6-14)
- **Crouch toggle.** Game options > "Toggle crouch": one press of Duck
  crouches, the next stands. It works with the keyboard, mouse and
  controller, and in multiplayer. (M-P5-19)
- **Each weapon keeps its ammo.** Switching back to a weapon loads the ammo
  you last used in it, while you have some, instead of always the "best".
  (M-P5-18)
- **Popups close with Activate or the right mouse button**, not only with
  Escape. (M-P5-17)
- **The sunglasses' zoom on wide screens** blacks out the sides, as the
  weapon scopes do. (M-P3-21)
- **Shift+Esc opens the console**, whatever your keyboard layout. (M-P2-15)
- **Frame-rate limit choices:** 60, 120, 144, 240 (the default) or none, in
  Interface options. (M-P1-25)
- **One way to start the game on Linux: `linux-run.sh` opens the launcher**,
  `NOLF.exe`, as the game starts on Windows and Steam. It replaces both the
  old `linux-run.sh`, which skipped the launcher, and `linux-run-launcher.sh`.
  It no longer forces a window: the game starts as Options > Display last
  left it (`./linux-run.sh ++windowed 1` still forces one). The archives no
  longer carry a `nolfcmds.txt`: the launcher writes its own on every Launch.
- **The installers refuse a release copied over another installation.**
  Each archive carries a `SHA256SUMS` of every file of the release and every
  file the installer writes; `linux-install.sh` and `windows-install.bat`
  check the directory against it first, and stop if a file is missing,
  changed or not part of the release.
- **Menu text in a TrueType font.** In English the menus' text is drawn
  from Bowlby One SC (SIL Open Font License, shipped in `fonts\`), made at
  the size the screen needs, so it is crisp instead of a scaled-up bitmap:
  menu items, titles, help text and the loading screen. `MenuFont 0`
  brings the bitmap fonts back. (M-P3-24)
- **The developer console in Noto Sans Mono**, a monospaced font sized for
  the screen, instead of the menus' small font. (M-P3-25)
- **Menu text grows with the screen.** The menus' text was the same size at
  every resolution, so on a large screen it was small inside a full-screen
  menu (and half that again at 4K); it is now drawn at the menu page's scale,
  as on a 640x480 screen, and check boxes beside it with it; Display
  options' values line up. Message boxes follow; the HUD's text keeps its own
  size setting. The game is also DPI-aware, so Windows' display scaling no
  longer stretches it. (M-P3-23)
- **Black side bars beside the menus** on wide screens, instead of the
  menus' green: the main menu and the other menu pages, the splash, pause,
  failure and loading screens. (M-P3-22)
- **The intro movies keep their shape** on wide screens, with black bars,
  instead of being stretched; the Sierra logo, a 2:1 movie, is no longer
  squeezed on a 4:3 screen either. (M-P1-27)
- **Ultra texture detail.** Performance > Display > Texture resolution has
  an Ultra preset above High, which puts every slider at its top: the 357
  textures that High draws at half size (character heads, skyboxes, some
  props) are drawn at full size. (M-P1-26)
- **The Friday and Christmas main menus' Cate moves again, in her own
  outfits**: the casual and winter Cates are the NOLF Modernizer's menu
  models, not the in-game ones, which stood still. (M-P3-11)
- **DirectInput no longer enumerates every device at startup**: only the
  keyboard and mouse are opened, as the controller is read through XInput.
  A Windows 11 startup crash was inside that enumeration. (M-P7-09)
- **Climbing out of water at high frame rates** works again (near the end of
  Morocco you could not get out). (M-P1-23)
- **32-bit textures by default.** (M-P1-24)
- **Water surfaces fill their pools** with `d3d9.ren` and `opengl.ren`; they
  stopped half a wave cell short of every edge (the UNITY fountain).
- **The Intel graphics fix** applies only with `d3d7.ren`, the one renderer
  it is needed for. (M-P1-10)
- **`SetPos` and `GetPos`** console commands. (M-P2-14)
- **Documentation:** [`docs/INSTALL.md`](docs/INSTALL.md),
  [`docs/CONSOLE.md`](docs/CONSOLE.md) and
  [`docs/VARIABLES.md`](docs/VARIABLES.md).

## The modern build, compared with retail

### Graphics and display

- **Wide screens.** Every resolution of at least 640x480 is offered,
  including wide ones, in the game and in the launcher. The field of view
  widens with the screen instead of being cropped (Hor+), and the player's
  weapon keeps its shape. (M-P0-01, M-P1-07, M-P1-14)
- **Window mode:** full screen, a window, or a borderless window at the
  desktop's resolution, in Display options. (M-P1-06, M-P1-22)
- **New renderers.** `d3d9.ren` (Direct3D 9), the default, and `opengl.ren`
  (OpenGL). The original DirectX 7 renderer, with fixes, ships as `d3d7.ren`.
  (M-P0-04, M-P0-05)
- **Antialiasing** (2x, 4x or 8x multisampling) with `d3d9.ren` and
  `opengl.ren`; `VSync` too. (M-P1-18)
- **Texture filtering:** none, bilinear, trilinear, or 2x to 16x anisotropic.
  (M-P1-09)
- **32-bit textures** by default. (M-P1-24)
- **Ultra texture detail**, every texture at its full size. (M-P1-26)
- **Movies keep their shape**, with black bars, on any screen. (M-P1-27)
- **Intel graphics fix** for a black screen underwater and missing screen
  tints with `d3d7.ren`. (M-P1-10)
- **Menus and the HUD at any resolution.** The menus keep their 4:3 layout,
  centred, with black bars at the sides (M-P3-22). The HUD scales with the
  screen (Interface options: HUD size). This covers the splash, loading and
  failure screens, the multiplayer menus, the password dialog, the space-suit
  helmet, the weapon and ammo chooser (which now also shows the previous and
  next weapons), and the main menu's version text. (P3)
- **Cinematics** can be limited to 4:3 with side bars. (M-P3-08)
- **Model shadows** are no longer switched off for good by the first
  cinematic or a loaded game. (M-P5-14)
- **Fonts** draw without black boxes. (M-P3-19)
- **Main menu:** the original or the Game of the Year art, and a winter Cate
  at Christmas and a casual one on Fridays (`NoFunMenus 1` turns these off).
  (M-P3-10, M-P3-11)
- **A frame-rate counter** (Interface options). (M-P3-14)
- **Safer start-up:** a failed screen mode falls back to 800x600x32 rather than
  640x480x16, and the settings that made text draw in black boxes are put
  right. (M-P1-08)

### Frame rate and timing

- **High frame rates.** Single player runs at up to 240 frames a second by
  default (60, 120, 144, 240 or unlimited); multiplayer stays at 60, so
  everyone fires at the same rate. (M-P1-01, M-P1-02, M-P1-23, M-P1-25)
- **The game keeps real time at any frame rate.** In retail, above 100
  frames a second the world ran fast. Animations, physics, particle systems,
  vehicles, and climbing out of water now behave the same at any rate.
  (M-P1-23)
- **A high-resolution clock**, and **faster loading** (the loading screen
  slept 100 ms between frames). (M-P1-05, M-P1-11)
- **Faster 2D drawing** in the menus, scopes and letterbox bars, and on-screen
  messages. (M-P0-03, M-P3-16)

### Controls

- **Raw mouse input:** smooth mouse look with no stutter. "Legacy mouse look"
  in Mouse options brings the retail one back. (M-P1-03, M-P1-04)
- **Crouch toggle** (Game options). (M-P5-19)
- **Quick weapon switch:** next and previous weapon switch at once (Game
  options). (M-P5-12)
- **Step zoom:** one key steps through the zoom levels and back out.
  (M-P5-13)
- **Each weapon keeps its ammo.** (M-P5-18)
- **New default controls:** the mouse wheel switches weapons, the middle
  button steps the zoom, and backquote opens the console. (M-P4-06)
- **Popups close with Activate or the right mouse button.** (M-P5-17)

### Controller

- **Xbox-style (XInput) controllers**, including the Steam Deck's controls
  through Steam Input. They work in the game and in the menus, and are
  rebindable like keys. (P7)
- Analog look and movement, with a speed setting, inverted look and dead
  zones in Options > Controls > Controller. (M-P7-03, M-P7-04, M-P7-06)
- The menus follow the d-pad, A and B; Start opens the menu; A skips
  cutscenes. (M-P7-05)
- DirectInput joysticks are no longer used, and DirectInput no longer
  enumerates the machine's devices at startup (M-P7-09).

### Console and cheats

- **A developer console** (backquote or Shift+Esc, single player). `Help`
  lists the commands. Engine and server commands work in it, and so do cheat
  codes. (P2) See [`docs/CONSOLE.md`](docs/CONSOLE.md).
- **`Cheats`** lists the cheat codes. Six cheats the retail build left out
  work again: `mpclip` (fly through walls, also the way out of a stuck spot),
  `mpteleport`, `mphookmeup`, `mpboyisuck`, `mpbeenthere` and `mpwhoami`.
  (M-P2-09)
- **`SetPos` and `GetPos`.** (M-P2-14)
- **Big head mode** (`BigHeadMode`) and **trigger volumes**
  (`DisplayTriggers`). (M-P2-10, M-P2-11)
- **A log file**, `modern.log`. (M-P2-07)
- **Speed-run helpers:** the mission, level and loading state at fixed
  addresses, for timers. (M-P2-12)

### Sound, music and movies

- **No proprietary middleware.** Miles is replaced by OpenMiles and Bink by
  our own FFmpeg-based player; the intro movies play.
- **Music plays out of the box** on Wine, Proton and the Steam Deck: the game
  directory carries Microsoft's DirectMusic. (M-P4-12)
- **A jukebox** in Options plays any of the game's music themes at any
  intensity. (M-P4-02)

### Game content

- The **Game of the Year** content's text is complete. (M-P4-08)
- A **second cosmonaut style** for multiplayer, and fixed character
  selections. (M-P4-04)
- The skydiving SMG has a name of its own. (M-P4-07)

### Multiplayer

- **The internet server list works again**, through OpenSpy's master server
  (`MasterServer`). (M-P6-02)
- **Server passwords are checked by the server.** Retail sent the password to
  anyone who asked. (M-P6-10)
- **Names and chat with `%` in them** can no longer crash clients and servers
  (a published LithTech format-string vulnerability). (M-P6-01)
- **Rejoining after a level-change timeout**, and a log of join attempts.
  (M-P6-05, M-P6-06)
- The server list shows compatible servers by default. (M-P6-03)
- The modern build plays with retail 1.003 players and servers. (M-P6-04)
- **The dedicated server** (`NolfServ.exe`, `linux-server.sh`):
  - logs its console to `modern-server.log`; (M-P6-11)
  - starts without its setup wizard with `-nowiz` wherever that is on the
    command line, and takes the level list as `-NetLevel <world>` (or
    `-NetCALevel`) once per world; (M-P6-12)
  - says which master server it reports to, set with `-MasterServer`;
    (M-P6-13)
  - no longer prints console text as a format (a `%` in a player's name),
    overruns its console buffer on a long line, writes past the game's
    option table, or leaks a find handle. (M-P6-14)

### Launcher and installation

- **Installers** for Linux (including the Steam Deck) and Windows copy the
  game's data from your own disc images and check every file. See
  [`docs/INSTALL.md`](docs/INSTALL.md).
- **The launcher starts the game on modern Windows**, and says why when it
  cannot. (M-P1-19)
- **No disc check**, and the Game of the Year archives are found without the
  original installer. A fresh install picks the screen's resolution
  (1280x800 on a Steam Deck). (M-P1-15, M-P1-16, M-P1-17)
- **No warning popups** before the launcher's Display and Advanced dialogs.
  (M-P1-20)
- **The launcher's art keeps its colours** under Wine when it loses focus.
  (M-P1-21)
- **Old settings are upgraded once.** For example, `d3d.ren` becomes
  `d3d9.ren`, and the controller bindings are added. (M-P1-12)

### Bug fixes

These retail bugs are fixed (details in
[`docs/RETAIL_BUGS.md`](docs/RETAIL_BUGS.md)):

- **Ones you can see:**
  - Level particle systems stopped emitting at high frame rates.
  - The world ran fast above 100 frames a second.
  - Enemies could fly off "at faster-than-light speed".
  - The player was stopped by invisible gaps between floor polygons
    (M-P5-05).
  - Model shadows were lost after cinematics.
  - Popups closed only with Escape.
  - The camera disabler made a HUD surface ten thousand icons wide.
- **Crashes:**
  - Doors and switches with no move direction.
  - A zero-length turn to face something.
  - An invalid character style.
  - Long skin and head names in player setup.
  - Window messages during shutdown (M-P1-13).
  - A dozen unchecked pointers, handles and indexes in the engine.
- **Security:**
  - Format strings in names and chat.
  - Server passwords sent in the clear.
  - The server browser trusting an index from the network.
- **Leaks and smaller errors** in the engine:
  - file handles, streams and music paths;
  - stream reads and writes;
  - the compressed-vector codec;
  - a matrix transform;
  - the box-intersect test;
  - the sound filter and reverb lookups;
  - debug messages naming the wrong function or reading a missing
    argument.

In all, 49 of the 51 retail bugs found so far are fixed. The two left alone
are explained in `docs/RETAIL_BUGS.md`.

### Not included

- Controller rumble and on-screen button prompts (M-P7-07, M-P7-08).
- The Modernizer's own art and models: the PlayStation 2 Cate, the LithTech
  tech demo, and its HD font. The game's own files are used where they can
  stand in.
