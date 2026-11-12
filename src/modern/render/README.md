# src/modern/render/ -- the modern build's renderers

A renderer core, and the backends under it that make it a render DLL the
engine can load in place of `d3d.ren` (MODERN.md, M-P0-04):

| DLL | backend | device |
|---|---|---|
| `opengl.ren` | `gl/` | fixed-function OpenGL 1.x plus a few extensions, which is what the Mac renderer uses and what Wine maps to any host's OpenGL. What of the really-close objects is nearer than the near plane (`rd_NearDepth`) it draws by recording each batch in a display list and drawing it twice: as it is, unclamped, which keeps the part beyond the near plane; then under the projection's depth from 0.01 to the near plane, into the front of the depth buffer |
| `d3d9.ren` | `d3d9/` | the Direct3D 9 fixed-function pipeline, which Proton runs on Vulkan (DXVK): `rdev.h`'s GL-shaped state kept by the backend and handed over when something is drawn, the projection moved to Direct3D's depth range and pixel centres, immediate-mode vertices drawn with `DrawPrimitiveUP`, textures made at their first use from the levels uploaded. Direct3D 9 has no depth clamping, which the really-close objects are drawn with: while it is on, the backend transforms the vertices itself, clamps their depth, and draws them pre-transformed; and what of them is nearer than the near plane it keeps in order, in the front of the depth buffer (`rd_NearDepth`) where clamping would give all of it the one depth |

The core is new code, written from the Mac release's OpenGL renderer, which
is the same renderer for the same engine version and whose functions all
kept their names (`ext/tools/pef.py` reads it), and from what the engine
expects of `d3d.ren` on the PC, which is what it aims to look like. It
draws through `rdev.h`, a small fixed-function device in the shape of
OpenGL 1.x (state, texture units with combiners, matrix stacks, immediate
primitives); a backend implements that and names its DLL.

They are built by `build/ext.sh` with `zig c++` (no VC6: nothing here has
to match anything), into `out/ext/opengl.ren` and `out/ext/d3d9.ren`.
Both are installed in the modern game directories, where `d3d9.ren` is the
default renderer and our `d3d.ren` (DirectX 7, with the modern fixes) is
installed as `d3d7.ren`, so the three can be compared (MODERN.md, M-P0-05).

## Using it

Pick one like any renderer: `"RenderDll" "opengl.ren"` (and `"CardDesc"
"OpenGL"`; for `d3d9.ren`, `"Direct3D9"`) in `autoexec.cfg`,
`+RenderDll opengl.ren` on the command line, or the launcher, which lists
them as "LithTech OpenGL Renderer" and "LithTech Direct3D 9 Renderer". A
playtest:

    printf '"RenderDll" "opengl.ren"\n"CardDesc" "OpenGL"\n' > out/gl.cfg
    PLAYTEST_CFG=out/gl.cfg build/playtest.sh out/nolf-modern-release -

Console variables it reads besides the engine's own (the names and defaults
`d3d.ren` and the Mac renderer share): `Bilinear`, `Trilinear`,
`Anisotropic`, `MipmapOffset`, `GroupOffset0`..`9`, `FilterOptimized`,
`MaxTextureSize`, `S3TCEnable`, `LightMap`, `Saturate`, `DetailTextures`, `DetailTextureAdd`, `DetailTextureScale`, `DrawWorld`, `DrawSky`, `DrawWorldModels`, `DrawAll`, `DrawSorted`, `SkyScale`, `FixTJunc`, `LightAddPoly`, the `Fog*` and `SkyFog*` variables, `DrawModels`, `DrawGuns`, `TextureModels`, `ModelSaturation`, `MaxModelLights`, `ModelLODOffset`, `32BitTextures`, `RenderDebug` (3 and up also writes
`opengl.log` or `d3d9.log` beside the game, 4 logs every 2D call), and, ours, `VSync` and `Antialiasing` (0, 2, 4 or 8 samples per pixel: the renderers draw into a multisampled target and resolve it before a flip or a read).
`RenderCommand info` prints the device in use.

## Status

| stage | what | state |
|---|---|---|
| 1 | device and 2D: the WGL context on the engine's window (windowed, or full screen through `ChangeDisplaySettings`), the mode list, clear and flip, the engine's surfaces and every blit (keyed, scaled, warped, Optimized-2D colours and the seven blend modes), the screen lock and `FillScreenRect`, `BlitFromScreen`, screenshots (F8), DTX texture upload (32-bit, S3TC compressed or decoded) | done: menus' 2D, the console and the HUD's 2D draw |
| 2 | the world: BSP polygons with their textures and lightmaps, visibility, world models, the sky, fog | done but for portal views (`docs/world.md`): the engine's own visibility query (the PVS walk, with the renderer's leaf, object and frustum callbacks), the main world and world models (solid, chromakey, and translucent ones back to front through their BSP) with textures, vertex colours, lightmaps (static and animated, shadow-map animations included, in 512x512 atlases) and detail textures, at the retail PC's brightness (`Saturate`: lightmaps and vertex colours doubled); the sky objects' world models from the squashed sky camera; fog; the global light scale and the camera's light add |
| 3 | objects: models (with the engine's animation code compiled in, as `d3d.ren` does), sprites, particles, poly grids, line systems, canvases, dynamic lights, shadows | done (`docs/objects.md`): sprites (billboards, rotatable, glow, sprite bias, lit from the world's light grid and dynamic lights), models (the engine's trackers through our copy of its transform maker, CPU skinning, model-space lighting from the light grid, the sun where the model sees the sky towards it (a ray through the world's BSP, `d3d.ren`'s own segment test), the visible dynamic lights, the world's static lights and spot lights from the world tree, and the game's model hook, in the NormalRef node's frame where the model has one; chrome (`EnvMapEnable`); LOD; translucent, additive and chromakey ones), particle systems, poly grids, line systems and canvases (the game's `ILTCustomDraw` callbacks) draw, and really-close objects (the player's weapon, the menus' 3D parts) in camera space, in the scene's depth buffer as `d3d.ren` draws them (depth clamping stands in for its 0.01 near plane; in `d3d9.ren` what is nearer than the scene's near plane goes into the front of the depth buffer, in order, as `d3d.ren`'s depths below 0 are; `opengl.ren` draws each of their batches twice, the part beyond the near plane as the scene and the part nearer under a projection of its own, into that front). Dynamic lights light the world: added into the lit polys' lightmaps texel by texel (`r_LightData`), and into gouraud polys' vertex colours, on the main world and world models. The main menu and M03S01's cabin match `d3d.ren`, the sky through the windows included. Environment maps on the world (`EnvMapWorld`, a texture's linked `EnvMap` added signed, as `d3d.ren`) and on poly grids (`EnvMapPolyGrids`, the water). The sky-pan texture (the clouds' shadows) on panning-sky polys. Model shadows: `d3d.ren`'s projected blob, cast down from a point above the model onto the world polys under it (`MaxModelShadows`, `ModelShadowAlpha`, `ModelShadowOffset`); a world model that has moved takes its shadow in its own place. Not needed: `d3d.ren`'s mirror and portal pass (no surface in any of the game's 60 worlds is a portal or a mirror) and its specular pass (no model sets `SpecularEnable`). A sweep of all 60 single-player worlds against `d3d.ren` shows no other difference |
| 4 | movies | done: the engine's screen-mode Bink and Smacker players ask the renderer for DirectDraw (`GetHook("LPDIRECTDRAW")`, `"BACKBUFFER"`); `mr_movie.cpp` answers with small stand-ins in DirectDraw's vtable layout, a system-memory surface to decode into and a back buffer whose `Blt` draws the frame. The intro movies match `d3d.ren` |
| 5 | Linux | the renderer is one part, and its own system calls are behind `mrplatform.h` (display modes, the window; `mr_window.cpp` on Windows) and the GL backend (the context and the swap, WGL in `gl/gl_device.cpp`): a native build supplies those and the rest is plain OpenGL. `lithtech.exe` still needs Win32 for its window, input, sound and networking (the Mac engine shows where those seams are) |

## Files

| file | what |
|---|---|
| `ltrender.h` | the engine <-> renderer contract: `RenderStruct` (the 15 engine callbacks and the 37 renderer slots, with the Mac renderer's names), `RenderStructInit`, `RMode`, `PFormat`, `RenderBlitRequest`, `SharedTexture`, `TextureData`; every size checked at compile time |
| `mr.h` | the core's own globals and declarations |
| `rdev.h` | the device under the core, which a backend implements |
| `mr_main.cpp` | exports, Init/Term, modes, clear/flip, console variables, the small slots |
| `mrplatform.h`, `mr_window.cpp` | what the core needs from the system (display modes, sizing the engine's window), and its Windows implementation (user32) |
| `mr_2d.cpp` | surfaces, blits, Optimized 2D, the screen lock |
| `mr_texture.cpp` | textures |
| `ltworld.h` | the engine's scene and world data as the renderer reads them: `SceneDesc`, `MainWorld`, `WorldBsp`, `Leaf`, `Node`, `WorldPoly`, `Surface` |
| `mrscene.h` | the 3D half's shared declarations: the per-world context, lightmap atlases, the camera |
| `mr_scene.cpp` | the world context, the camera and `RenderScene` |
| `mr_lightmap.cpp` | lightmaps: decoding, composing light animations, atlases |
| `mrobjects.h`, `mr_objects.cpp` | the visible set: objects by kind and the order they are drawn in |
| `mr_sprites.cpp` | sprites, and the world's light grid |
| `mr_models.cpp` | models: the transform maker, skinning, lighting |
| `mr_effects.cpp` | particle systems, poly grids, line systems, canvases |
| `mr_shadows.cpp` | model shadows |
| `mr_movie.cpp` | DirectDraw stand-ins for the movie players |
| `gl/gl_device.cpp` | `rdev.h` on OpenGL, and the WGL context |
| `gl/opengl.def`, `gl/opengl.rc` | `opengl.ren`'s three exports; the launcher's name for it (string 5) |
| `d3d9/d3d9_device.cpp` | `rdev.h` on Direct3D 9 |
| `d3d9/d3d9.def`, `d3d9/d3d9.rc` | `d3d9.ren`'s exports and launcher name |
| `docs/` | the research the renderer is written from: `device.md` (the device and 2D half of the contract), `world.md` (the world's data and how it is drawn), `objects.md` (objects, models and the engine services a render DLL reaches), `mac-cvars.txt` (the Mac renderer's console variables) |

## The Mac renderer, for reference

`ext/rez/games/nolf-mac-usa/NOLF/LithTech` (ext/rez/README.md) is one PowerPC
executable: the engine with the renderer linked in where the PC loads
`d3d.ren`. The renderer is 514 functions, 225 KB of code, from
`opengl_init.cpp` to `lightmap_dynamic.cpp`; CodeWarrior's static
initialisers (`__sinit_<file>`) mark where each source file ends:
`opengl_init`, `common_stuff`, `common_draw`, `opengl_draw`, `drawsprite`,
`setupmodel`, `drawobjects`, `tagnodes`, `drawworldmodel`,
`drawparticles_A`, `drawparticles`, `drawsky`, `opengl_drawsky`, `vb_mgr`,
`opengl_texture`, `drawmodeltris`, `drawmodel`, `drawmodelshadows`,
`drawmodelshadowsproj`, `drawpolygrid`, `draw_canvas`, `lm_dualpass`,
`lightmap_surface`, `lightmap_light`, `lightmap_dynamic`.

`glRenderDLLSetup` fills the same 37 slots as `d3d.ren`'s `RenderDLLSetup`,
at the same offsets:

| slot | Mac function | | slot | Mac function |
|---|---|---|---|---|
| +0x6c | `r_Init(RenderStructInit*)` | | +0xb8 | `r_GetHook(char*)` (PC: GetRenderObject) |
| +0x70 | `r_Term` | | +0xbc | `r_SwapBuffers(uint)` (PC: FlipScreen) |
| +0x74 | `rgl_BindTexture(SharedTexture*, uint)` | | +0xc0 | `r_GetInfoFlags` |
| +0x78 | `rgl_UnbindTexture(SharedTexture*)` | | +0xc4 | `r_GetScreenFormat(PFormat*)` |
| +0x7c | `r_CreateContext(RenderContextInit*)` (PC: CreateWorld) | | +0xc8 | `r_CreateSurface(int, int)` |
| +0x80 | `r_DeleteContext(RenderContext*)` | | +0xcc | `r_DeleteSurface(void*)` |
| +0x84 | `r_Clear(LTRect*, uint32)` | | +0xd0 | `r_GetSurfaceInfo(void*, uint32*, uint32*, int32*)` |
| +0x88 | `r_Start3D` | | +0xd4 | `r_LockSurface(void*)` |
| +0x8c | `r_End3D` | | +0xd8 | `r_UnlockSurface(void*)` |
| +0x90 | `r_IsIn3D` | | +0xdc | `r_OptimizeSurface(void*, uint32)` |
| +0x94 | `r_StartOptimized2D` | | +0xe0 | `r_UnoptimizeSurface(void*)` |
| +0x98 | `r_EndOptimized2D` | | +0xe4 | `r_LockScreen(int, int, int, int, void**, int32*)` |
| +0x9c | `r_IsInOptimized2D` | | +0xe8 | `r_UnlockScreen` |
| +0xa0 | `r_SetOptimized2DBlend(LTSurfaceBlend)` | | +0xec | `r_BlitToScreen(BlitRequest*)` |
| +0xa4 | `r_GetOptimized2DBlend(LTSurfaceBlend&)` | | +0xf0 | `r_WarpToScreen(BlitRequest*)` |
| +0xa8 | `r_SetOptimized2DColor(uint32)` | | +0xf4 | `r_MakeScreenShot(char*)` |
| +0xac | `r_GetOptimized2DColor(uint32&)` | | +0xf8 | `r_ReadConsoleVariables` |
| +0xb0 | `r_RenderScene(SceneDesc*)` | | +0xfc | `r_BlitFromScreen(BlitRequest*)` |
| +0xb4 | `r_RenderCommand(int, char**)` | | | |

The engine's own 15 callbacks at `+0x00`..`+0x38` are named by the Mac's
`r_InitRenderStruct` (in `ltrender.h`).

Where the Mac renderer and the PC engine disagree, the PC wins: the Mac
stubs the screen lock (`r_LockScreen` returns 0) and `WarpToScreen`, which
the PC game uses, and its 2D colour conversion loses a bit of green.
