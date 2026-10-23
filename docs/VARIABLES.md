# Console variables

This file lists the game's console variables: what each one does, its
default, and whether it is new in the modern build. Commands are listed in
[`CONSOLE.md`](CONSOLE.md).

It is not every variable the engine knows. It leaves out internal tuning
values with no visible effect, and most renderer debug switches. `Set` (in the
console) prints every client variable and its current value; `serv
ShowGameVars` prints the server's.

## How variables work

- **Setting one.** Type `Name value` in the console, or put the same line in
  `autoexec.cfg`. On the command line, use `+Name value`.
- **Names are not case-sensitive.** `ShowFramerate` and `ShowFrameRate` are
  the same variable, and so are `LODScale` and `LodScale`.
- **Toggling.** A value of `!` toggles a variable between 0 and 1.
- **What is saved.** The game writes `autoexec.cfg` when you leave an options
  menu and when it quits. It saves only variables marked for saving:
  - those an options menu has set;
  - those set as `+Name value` (in the console, the config, or `++Name value`
    on the command line);
  - those that came from a config file.

  Creating a variable does not mark it, so a value you type without the `+`
  lasts until the game quits. `-Name` removes the mark.
- **Config files.** The game starts from `autoexec.cfg`, or from the file
  named by `-config <file>`. The launcher's "restore defaults" starts from
  `defaults.cfg`. A dedicated server reads `s_autoexec.cfg`.
- **Server variables.** Some variables live on the server. In single player
  the modern console sends every line to the server too, so these work when
  typed there; in retail, prefix them with `serv`.
- **Per-level values.** Each level sets `FarZ`, the fog variables and the
  volumetric fog itself when it loads, so changing them in the console only
  lasts until the next level.

"Modern" in a table means the variable exists, or does something, only in
the modern build.

## Modern build

| Variable | Default | What it does |
|---|---|---|
| `FramerateLimit` | 240 | The most frames a second in single player; 0 is no limit. Multiplayer is always held at 60, and loading is never limited. Interface options: "Frame rate limit" (60, 120, 144, 240, unlimited). |
| `ShowFramerate` | 0 | 1 draws a frame-rate counter at the bottom left. Interface options. |
| `UIScale` | 0.5 | HUD size, times the screen's height ratio (never smaller than retail, at most 4). Interface options: "HUD size". |
| `RestrictCinematicsTo4x3` | 0 | 1 puts black bars beside cinematics on screens wider than 4:3. Interface options. |
| `UseGotyMenu` | 1 | Main-menu art: 1 Game of the Year, 0 the original release's. Interface options. |
| `MenuFont` | 1 | 1 draws the menus' text (in English) in the Bowlby One SC TrueType font from `fonts\`, at the screen's scale; and the developer console's in Noto Sans Mono; 0 uses the original bitmap fonts, scaled, for both. Read when the menus are laid out. |
| `NoFunMenus` | 0 | 1 always shows the plain main menu. Otherwise it has a winter Cate from 15 to 31 December and a casual Cate on Fridays. |
| `WindowMode` | 1 | Only applies when `Windowed` is 1: 2 is a borderless window at the desktop's resolution, anything else a framed window. Display options: "Window mode". |
| `Windowed` | 0 | 1 runs in a window (retail variable; Display options now sets it). |
| `OldMouseLook` | 0 | 1 uses retail's mouse look (DirectInput, re-centring the cursor) instead of raw mouse input. Mouse options: "Legacy mouse look". |
| `NoRawInput` | 0 | 1 keeps the new mouse look but reads the cursor's movement instead of raw input. It is also used automatically when raw input is unavailable. |
| `CrouchToggle` | 0 | 1: one press of Duck crouches, the next stands up (Duck still works as a hold while swimming, on a ladder or on a vehicle). Game options: "Toggle crouch". |
| `QuickSwitch` | 0 | 1: next/previous weapon switch at once, without confirming. Game options: "Quick weapon switch". |
| `PadLookSpeed` | 1 | Controller: right-stick turn rate (times 180 degrees a second across, 120 up and down). Controller options. |
| `PadInvertY` | 0 | Controller: 1 inverts the right stick's up and down. Controller options. |
| `PadLookDeadZone` | 0.15 | Controller: the right stick's dead zone (0-0.9 of its travel). Controller options. |
| `PadMoveDeadZone` | 0.2 | Controller: the left stick's dead zone. Past three quarters of its travel the player runs. Controller options. |
| `PadLookCurve` | 2 | Controller: right-stick response; the deflection is raised to this power (1 = linear). |
| `VSync` | 0 | `d3d9.ren` and `opengl.ren`: 1 waits for the display's refresh. Read when the renderer starts. |
| `Antialiasing` | 0 | `d3d9.ren` and `opengl.ren`: multisample antialiasing, 0 (off), 2, 4 or 8. Read when the renderer starts. Advanced display options. |
| `EnableScreenTinting` | 1 | 0 draws screen tints with an overlay (the Intel graphics fix). Applies only with `d3d7.ren`. |
| `EnableLightScale` | 1 | 0 turns light scaling off (the Intel graphics fix). Applies only with `d3d7.ren`. |
| `ConsoleBackdrop` | 0 | The developer console's background: 0 or 1 the menus' blank tag, 2 black. |
| `BigHeadMode` | 0 | Heads drawn 1.8 times this size (0 is off). Ignored in multiplayer. Also a server variable; the console passes it on. |
| `DisplayTriggers` | 0 | Server: 1 draws trigger volumes as yellow boxes. |
| `MasterServer` | `master.openspy.net` | The master server for the internet server list and for hosted games (`host` or `host:port`; `none` turns it off). |
| `NetJoinPassword` | (none) | The password `+connect` uses to join a locked server. |
| `ModPatchNum` | 0 | The last modern config version applied (now 1.5); it drives one-time upgrades of an old config. Leave it alone. |
| `FramerateLock` | (gone) | The old on/off limit. An old config's `FramerateLock 0` becomes `FramerateLimit 0` once. |

## Display and window

| Variable | Default | What it does |
|---|---|---|
| `RenderDll` | `d3d9.ren` (modern), `d3d.ren` (retail) | The renderer. The modern build has `d3d9.ren` (default), `opengl.ren` and `d3d7.ren`; a config naming `d3d.ren` gets `d3d9.ren`. |
| `ScreenWidth`, `ScreenHeight` | 640, 480 | The screen mode. |
| `BitDepth` | 16 | The screen's colour depth. |
| `FovX`, `FovY` | 90, 78 | The game camera's field of view, in degrees. The modern build widens the horizontal one for wide screens. |
| `HardwareCursor` | 0 | 1 uses the hardware cursor in menus (`DisableHardwareCursor 1` forbids it). |
| `TripleBuffer` | 0 | `d3d7.ren`: 1 uses two back buffers. |
| `ForceClear` | 0 | 1 clears the whole screen every frame. |
| `LetterBox` | 0 | 1 forces the cinematic letterbox bars on. |
| `ScreenFadeAtLevelStart` | 1 | 0 skips the fade-in when a level starts. |
| `DynamicLoadScreen` | 1 | 0 shows a still loading screen instead of the animated one. |
| `PerformanceLevel` | 1 | The overall detail preset, 0-2. It also sets how many effects (such as gibs) are made. |

## Rendering

| Variable | Default | What it does |
|---|---|---|
| `32BitTextures` | 1 (modern), 0 (retail) | 1 keeps textures at 32 bits instead of 16. Display options: "Texture depth". |
| `Bilinear`, `Trilinear`, `Anisotropic` | 1, 0, 0 | Texture filtering. Advanced display options: "Texture filtering". |
| `MipmapOffset` | 0 | Skips the most detailed mip levels; higher is blurrier. |
| `MaxTextureSize` | 16384 | The largest texture side kept. |
| `GroupOffset1` ... `GroupOffset6` | 0 | Mip levels dropped from the world, weapon, character, prop, effect and sky textures; added to each texture file's own offset (0 or 1), and below 0 the result is 0. Texture resolution's sliders (slider 3 - offset); Low/Medium/High/Ultra set them. |
| `DetailTextures` | 1 | Detail textures on the world. Advanced display options. |
| `LightMap` | 1 | Lightmaps on the world. |
| `DynamicLight` | 1 | Dynamic lights. |
| `FastLight` | 0 | 1 uses cheaper dynamic lighting on the world. |
| `MaxModelLights` | 4 | Dynamic lights per model. |
| `EnvMapEnable` | 0 | Environment maps (chrome) on models. Advanced display options. |
| `EnvMapWorld` | 1 | Environment maps on the world. |
| `DrawPortals` | 1 | Mirrors. Advanced display options. |
| `DrawShadows` | 0 | Model shadows. Advanced display options sets it, together with `MaxModelShadows`. |
| `MaxModelShadows` | 1 | Model shadows on or off. Cinematics turn it off; the modern build turns it back on afterwards. |
| `ModelShadowProj` | 0 | 1 uses projected model shadows. |
| `Saturate` | 0 | 1 doubles the brightness ramps ("saturated" lighting). |
| `LightSaturate` | 1 | Lightmap brightness scale. |
| `ModelLODOffset`, `LodOffset`, `LODScale` | 0, 0, 1 | Model level-of-detail bias and scale. |
| `FarZ` | 10000 | The far clip distance. Each level sets its own. |
| `FogEnable` | 0 | Distance fog. Each level sets it, with `FogR`, `FogG`, `FogB`, `FogNearZ`, `FogFarZ`. |
| `DrawSky` | 1 | 0 hides the sky. |
| `DrawWorld`, `DrawModels`, `DrawGuns`, `DrawSprites`, `DrawParticles`, `DrawPolyGrids`, `DrawLineSystems`, `DrawCanvases`, `DrawWorldModels` | 1 | 0 hides each kind of object (`DrawGuns`: the weapon in your hands). |
| `Wireframe` | 0 | 1 draws everything as wireframe. |
| `LightmapsOnly`, `DrawFlat` | 0 | Debug views: lightmaps only; flat shading. |
| `ShowPolyCounts`, `ShowTextureCounts` | 0 | Print polygon and texture counts. |
| `OptimizeSurfaces` | 0 | `d3d7.ren`: 2D surface optimisation. The modern build sets it to 1 at start-up (text draws in black boxes otherwise). |

## Sound and music

| Variable | Default | What it does |
|---|---|---|
| `SoundEnable`, `MusicEnable` | 1, 1 | Sound and music on or off. Sound options. |
| `SoundVolume` | 70 | Effects volume, 0-100. Sound options. |
| `MusicVolume` | -1100 | Music volume. Sound options. |
| `Sound16Bit` | 1 | Sound quality. Sound options. |
| `SoundFilters` | 0 | Reverb and filters by area. Performance options. |
| `Max3DVoices`, `MaxSWVoices` | 16, 32 | Sound voices. |
| `ReverbLevel` | | Reverb level (the launcher passes it). |
| `DisableSound`, `DisableMusic`, `NoMovies` / `DisableMovies` | 0 | 1 turns sound, music or the intro movies off (the launcher's "Advanced" options). |

## Input

| Variable | Default | What it does |
|---|---|---|
| `MouseSensitivity` | 5 | Mouse sensitivity, 0-10. Mouse options. |
| `MouseInvertY` | 0 | 1 inverts the mouse's up and down. Mouse options. |
| `MouseLook` | 1 | Mouse look. Mouse options. |
| `InputRate` | 20 | Smoothing of input read through DirectInput: the least time between samples, in ms. In the modern build that is the mouse wheel (and the mouse with `OldMouseLook`). Mouse options: "Mouse smoothing" (retail), "Wheel smoothing" (modern). |
| `LookSpring` | 0 | 1 centres the view when you stop looking. Keyboard options. |
| `NormalTurnRate`, `FastTurnRate`, `LookUpRate` | 1.5, 2.3, 2.5 | Keyboard turning and looking speeds. Keyboard options. |
| `VehicleTurnRateScale` | 1 | Vehicle steering speed. Mouse and keyboard options. |
| `RunLock` | 1 | Always run (Run then walks). Game options. |
| `JoystickDisable` | 0 | 1 turns DirectInput joysticks off (retail). The modern build uses only its own XInput controller and has no DirectInput joysticks. |
| `CursorCenter` | 1 | Re-centre the cursor every frame (retail mouse look). The modern build sets it to 0 unless `OldMouseLook` is 1. |

## Game and HUD

| Variable | Default | What it does |
|---|---|---|
| `Difficulty` | 1 | 0 easy to 3 very hard. Game options. |
| `Subtitles` | 0 | Subtitles. Game options. |
| `Gore` | 1 | Blood and gore. Game options. |
| `FadeBodies` | 0 | Bodies fade away. Game options. |
| `HeadBob`, `WeaponSway` | 1, 1 | Head bob and weapon sway, 0-1. Game options. |
| `HUDLayout` | 0 | HUD layout. Game options. |
| `PickupMessageDuration` | 5 | Seconds a pickup message stays. Game options. |
| `ObjectiveMessages` | 1 | Show a message when the objectives change. Game options. |
| `MaxNumMessages` | 5 | Message lines kept on screen. |
| `CrosshairStyle`, `CrosshairColorR/G/B`, `CrosshairAlpha`, `CrosshairDynamic` | | The crosshair. Crosshair options. |
| `DrawInterface` | 1 | 0 hides the whole HUD (for screenshots). |
| `SkipTitle` | 0 | 1 goes straight to the main menu. |
| `Tracers`, `ShellCasings`, `MuzzleLight`, `EnableWeatherFX` | 1 | Effects on or off. Effects options. |
| `DebrisFXLevel` | 2 | Debris amount, 0-2. Effects options. |
| `BulletHoles` | 300 | Bullet marks kept (the modern build makes it at least 100). |
| `BigBlood` | 0 | 1: much more blood (also the `mpexorbitantamounts` cheat). |
| `EnableDamageFX` | 1 | 0 turns damage screen effects off (poison, stun, sleep, burn). |
| `PlayerGravity` | -2000 | The player's gravity. |
| `FallDamageMinHeight`, `FallDamageMaxHeight` | 300, 500 | Falls higher than the first hurt; the damage grows up to the second. |

## Multiplayer

| Variable | Default | What it does |
|---|---|---|
| `NetPlayerName` | Player | Your name. Player setup. |
| `NetPlayerModel`, `NetPlayerSkin`, `NetPlayerHead`, `NetPlayerTeam` | | Your character and team. Player setup. |
| `UpdateRate` | 6 on first run | Server updates a second to ask for (2-60). Player setup: "Connection speed". |
| `CSendRate` | 7 | Your position updates to the server a second. |
| `AutoWeaponSwitch`, `AutoAmmoSwitch` | 1, 1 | Switch to a better weapon or ammo when you pick it up. |
| `IgnoreTaunts` | 0 | 1 neither plays nor sends taunts. |
| `connect` | | `+connect <address>` joins a server at start-up. |
| `NetPort` | 27888 | The port a hosted game uses. Host options. |
| `NetSessionName`, `NetPassword`, `NetUsePassword` | | A hosted game's name and password. Host options. |
| `NetMaxPlayers` | 8 | Players on a hosted game. |
| `NetGameType` | 2 | Hosted game type: 2 deathmatch, 1 co-op assault (H.A.R.M. vs. UNITY). |
| `NetEndFrags`, `NetEndTime`, `NetEndScore` | 25, 15, 50 | When a hosted game ends. Host options. |
| `NetFriendlyFire`, `NetWeaponsStay`, `NetHitLocation`, `NetAudioTaunts`, `NetFallDamageScale`, `NetRunSpeed`, `NetRespawnScale` | | A hosted game's rules. Host options. |
| `SendBandwidth` | 0 | Server: the bandwidth to use, set by the host options' connection choice. |
| `Say` | | Server: `serv Say <text>` sends a message from the host. |

## Debugging

| Variable | Default | What it does |
|---|---|---|
| `DebugLevel` | 0 | How much debugging text the engine and game print. |
| `ShowTickCounts` | 0 | Timing counters (also the `ShowTicks` command). |
| `ShowTiming` | 0 | Client and server timing printouts. |
| `ShowConnStats`, `ShowPing`, `ShowThruput` | 0 | Network statistics. |
| `ShowGameTime`, `ShowMemStats`, `ShowFileAccess` | 0 | Print the game time, memory use or every file opened. |
| `InputDebug` | 0 | Print input device activity. |
| `PlayerDims` | 0 | 1 draws the player's bounding box. |
| `CanShowDims` + `ShowDims` | 0 | Server: both 1 draw every object's bounding box. |
| `ShowTriggers` | 0 | Server: print trigger messages (`FilterTriggerMsgs` narrows them). |
| `DamageScale`, `HealScale` | 1, 1 | Server: multiply all damage and healing. |
| `TimeScale` | 1 | Server: game speed. |
| `WorldTime`, `WorldTimeSpeed` | -1, -1 | Server: force the hour of day, and how fast it passes. |
| `UpdateServer` | | See the `UpdateServer` command: 0 freezes the local world. |
| `alwaysfocused` | 0 | 1 keeps the game running while its window is in the background. |
| `HighPriority` | 0 | 1 runs the game at high process priority. |
| `ErrorLog`, `AlwaysFlushLog` | 0 | Write errors to a log file; flush it after every line. |
| `LatencySim`, `DropRate` | 0 | Simulate network lag and packet loss. |

## Variables that do nothing

The engine creates these, but nothing in this source reads them: `MaxFPS`
(the frame-rate limit is `FramerateLimit`), `ShowFrameRate` (the modern
`ShowFramerate`, which is the same variable, does work), `Gamma`,
`ImpactFXLevel`, `ConsoleEnable`, `ModelTransitionMS`, `Software`.
