# opengl.ren — the device / 2D half of the engine contract

This spec is for writing a new render DLL ("opengl.ren") that lithtech.exe loads in place of d3d.ren.
It covers the RenderStruct slots +0x6c..+0xfc, the structures crossing the boundary, the 2D/surface
path, texture binding, mode enumeration and selection, exports and console variables.
The 3D half (RenderScene, the world/context) is covered only to the extent of what the engine
passes and expects back.

**How claims are cited:**
* `file:line` means a file in the reconstruction (`src/engine/lithtech/` = **E/**, `src/engine/d3d/` = **D/**).
* `0x004xxxxx` is a lithtech.exe address. `0x100xxxxx` is a retail d3d.ren address.
* `Mac [0x1xxxxx]` is a code offset in the Mac engine (`ext/rez/games/nolf-mac-usa/NOLF/LithTech`).
  Read it with `ext/tools/pef.py dis - <name>`.
* "(inferred)" marks anything not read directly off code.

All engine↔renderer calls are **cdecl**: the caller cleans the stack, every site passes 32-bit
arguments, and returns come back in eax. `LTBOOL` is `unsigned int` (ltbasetypes.h:19).
`HLTCOLOR` is `uint32 0x00RRGGBB`, with bit 31 = `COLOR_TRANSPARENCY_MASK` (ltbasedefs.h:101-116).

---------------------------------------------------------------------------------------------------

## 0. Summary

**What is mandatory:**
* Export three plain-C names: `RenderDLLSetup`, `GetSupportedModes`, `FreeModeList`.
* `Init` must write `0xd5d` into `RenderStructInit.m_nVersion` and return 0.
* Fill **every** slot +0x6c..+0xfc. Some slots are called without a null test (see §1).

**What the engine assumes:**
* Off-screen surfaces live in the **screen pixel format** you report through +0xc4.
* Surfaces must be lockable to a CPU pointer with a constant byte pitch.
* The engine does software blits into them (and into the screen via LockScreen) with its own converter.

**The model to copy is the Mac OpenGL renderer:**
* surfaces are CPU memory in the screen format;
* they are uploaded to GL as 64x64 tiles when drawn (dirty on LockSurface);
* every BlitToScreen is a textured quad with the colour key turned into alpha 0;
* Optimized-2D just sets GL blend state.

Things the Mac version stubs that NOLF on Windows needs:
* LockScreen/UnlockScreen (the Mac returns 0), which the game needs for FillRect on the screen;
* BlitFromScreen;
* WarpToScreen (the Mac returns 1 and draws nothing).

**What can be NULL:**
* GetRenderObject (+0xb8) may answer NULL for "LPDIRECTDRAW"/"BACKBUFFER".
* The only consequence is that Bink/Smacker **full-screen (non-texture) video** refuses to start (§1, +0xb8).

---------------------------------------------------------------------------------------------------

## 1. The RenderStruct and the lifecycle

### 1.1 Layout (one global, 0x150 bytes, at 0x004b4358)

Source: E/clientrender.cpp:59-215 and 284-339.

| off | who writes | what |
|---|---|---|
| +0x00..+0x38 | engine (r_InitRenderStruct 0x00462110) | 15 callbacks for the renderer, §6 |
| +0x3c uint32 m_Width | engine, = RMode.m_Width, **before** Init (0x004624ff) | screen width. The renderer may rewrite it in Init (d3d divides it by a scale divisor of 1, w26_10020fd0_osoiqip.cpp:805; the Mac stores w, Mac r_Init) |
| +0x40 uint32 m_Height | same | screen height |
| +0x44 uint32 m_bInited | engine: 1 after a full good init (0x004625c1), 0 on failure and at term | every `LT_NOTINITIALIZED` gate tests it. clientmgr.h calls it m_nCanOptimizeSurfaces, a wrong name (E/clientrender.cpp:1316) |
| +0x48..+0x54 | renderer | d3d writes byte counters at +0x48/+0x4c/+0x50. The Mac adds texture bytes to +0x50 (Mac [0x180cc0], [0x17fb84]). The engine never reads them |
| +0x58..+0x68 | renderer | 5 statistics counters the console shows: TagVisibleLeaves, FlushObjectQueues, Models, WorldModels, Translucent |
| +0x6c..+0xfc | renderer (RenderDLLSetup) | 37 function pointers, §2 |
| +0x100 | ? | not referenced by the client |
| +0x104 SharedTexture* | engine | environment map ("EnvironmentMap" console command, 0x0041e0fd) |
| +0x108 PanInfo[2] | engine | global pan info (sky pan / fog), 0x28 bytes |
| +0x130 LTVector | engine | global light direction, default normalised (0,-2,-1) |
| +0x13c LTVector | engine | global light colour |
| +0x148, +0x14c | engine | misc (zeroed at init) |

### 1.2 Load and init sequence

r_InitRender 0x004623d0, E/clientrender.cpp:1154-1279:

1. If `m_bInInitRender` is set, return LT_OK. Otherwise set it.
2. `r_TermRender(mgr, FALSE)`: tears down any renderer already loaded (§1.3).
3. `ShowWindow(hWnd, SW_RESTORE)`.
4. Reset the engine's TextureData list, with a 10,000,000-byte budget.
5. Build a 0x21c-byte `RenderStructInit` on the stack:
   * memset to 0;
   * `m_hWnd` = main window;
   * `m_Mode` = *pMode (§3.1).
6. `LoadLibrary(pMode->m_RenderDLL)`. This is a bare file name, so the normal DLL search order applies.
   On failure: `LT_ERRORLOADINGRENDERDLL` (0x33).
7. `GetProcAddress("RenderDLLSetup")`. If it is missing, FreeLibrary and return 0x33.
8. `RenderDLLSetup(&g_RenderStruct)`. Store the pointer; fill +0x6c..+0xfc.
9. `m_Width/m_Height` = mode.
10. `if (Init(&init) != 0 || init.m_nVersion != 0xd5d)`: FreeLibrary and return 0x33.
    The Mac does the same check [0xec978-0xec988].
11. The console is restarted on the top half of the screen (CClientMgr 0x004105c0).
12. **r_PublishRenderStruct** 0x0040eeb0 (E/clientrender.cpp:978-988):
    * `g_pRender = &g_RenderStruct`;
    * the screen CSurface gets w/h from +0x3c/+0x40;
    * **calls +0xc4 GetScreenFormat(&g_ScreenFormat)**, then `g_nBytesPerPixel = g_ScreenFormat.GetBytesPerPixel()`;
    * then `surface_RestoreAll` (0x0040eef0) re-creates every live engine surface through the new renderer (§4.6).

    If any re-creation fails: FreeLibrary, return `LT_UNABLETORESTOREVIDEO`.
    **Note: no Term is called on that path. It frees the library directly.**
13. `m_bInited = 1`; `OnEvent(LTEVENT_RENDERALMOSTINITTED)`.
14. `CreateRenderWorld` (+0x7c, only if a world is loaded and none is bound yet; E/clientmgr2.cpp:1670-1693).
    If +0x7c returns NULL: m_bInited = 0, FreeLibrary, return `LT_ERRORBINDINGWORLD`, again without calling Term.
15. `SetFocus(hWnd)`.
16. `CClientMgr::BindAllTextures` (0x00410540, E/clientmgr2.cpp:671-685). clientrender.cpp mislabels it SetGlobals.
    * Calls **+0xf8 ReadConsoleVariables** (only if m_bInited and the slot is non-null).
    * Then calls **+0x74 BindTexture(tex, FALSE)** for every live SharedTexture.
17. `g_CurRenderMode = init.m_Mode`. **The renderer may rewrite the mode copy** (d3d writes the chosen device's
    name and description into m_InternalName/m_Description, w26_10020fd0_osoiqip.cpp:754-755).
18. The console prints "CardDesc %s" (m_InternalName), RenderDLL, ScreenWidth, ScreenHeight, BitDepth.
19. `OnEvent(LTEVENT_RENDERINIT)`.

### 1.3 Shutdown

r_TermRender 0x004627d0, E/clientrender.cpp:1094-1147. This only runs if m_bInited:

1. `OnEvent(LTEVENT_RENDERTERM)`.
2. **r_UnpublishRenderStruct** (0x0040f060, E/clientmgr2.cpp:2584-2594):
   * the screen PFormat and bytes-per-pixel are copied to a shadow;
   * **every engine surface is copied out to system memory**: +0xd4 LockSurface, a row memcpy of `w*bpp` bytes,
     +0xd8 UnlockSurface, then **+0xcc DeleteSurface**;
   * then `g_pRender = NULL`.
3. Unbind and release all texture data (`FreeList1110Contents` → for each SharedTexture: **+0x78 UnbindTexture**,
   then the engine frees its TextureData).
4. **+0x80 DestroyWorld(ctx)** if a world object exists.
5. **+0x70 Term()**, then FreeLibrary.
6. All engine TextureData is freed.
7. m_bInited = 0; RenderStruct +0x00..+0xff is cleared and the callbacks are reinstalled.

So by the time Term runs, every surface and texture has already been released through the slots.
Term only has to destroy the device/context and window state.

### 1.4 Restart triggers

Init/Term must be fully repeatable; the DLL is unloaded and reloaded each time. A re-init happens on:
* **Focus loss and regain.** Unless `NullRender`, WM_ACTIVATEAPP with wParam=0 runs r_TermRender (unloads the DLL),
  and regaining focus runs `r_InitRender(&g_CurRenderMode)` (E/clientwin.cpp:1028-1075).
* `ILTClient::SetRenderMode` (§7.3).
* The console commands `RestartRender [dll]` and `ResizeScreen w h` (E/ccclient.cpp:538-600).

### 1.5 Frame shape

How the game drives a frame (InterfaceMgr.cpp:885-898 and others):

```
ClearScreen(rect or NULL, CLEARSCREEN_SCREEN|CLEARSCREEN_RENDER)   (+0x84)
Start3D()                                                           (+0x88)
  RenderCamera / RenderObjects -> RenderScene(SceneDesc*)           (+0xb0)
  StartOptimized2D()                                                (+0x94)
    DrawSurfaceToSurface*(screen, surf, ...) -> BlitToScreen         (+0xec)
    SetOptimized2DBlend / SetOptimized2DColor                       (+0xa0/+0xa8)
    FillRect(screen, ...) -> LockScreen/UnlockScreen                 (+0xe4/+0xe8)
  EndOptimized2D()                                                  (+0x98)
End3D()                                                             (+0x8c)
FlipScreen(flags)                                                   (+0xbc)
```

Blits to the screen also happen **outside** Start3D/End3D and outside Optimized2D (loading screens, the
console, menus that skip the 3D bracket). d3d handles this with a DirectDraw Blt; the Mac sets its own GL state.
**Every 2D entry must work in all three states.**

---------------------------------------------------------------------------------------------------

## 2. The 37 renderer slots

**Sources for this section:**
* d3d's table: D/w26_10020fd0_osoiqip.cpp:187-238 and 469-510.
* Mac table: `glRenderDLLSetup` [0x14c6a0]. It fills exactly these 37 slots; the given mapping was verified.
* Engine call sites: E/clientworldinfo.cpp:95-122, which has the absolute-reference count per slot.

The "null" column says whether the engine tests the pointer before calling it.
Where it does not, the slot **must** be filled.

| slot | C signature (cdecl) | null-tested? |
|---|---|---|
| +0x6c | `int Init(RenderStructInit*)` | no |
| +0x70 | `void Term(void)` | no (called only when the HMODULE is non-null) |
| +0x74 | `void BindTexture(SharedTexture*, LTBOOL bChanged)` | no (gated on m_bInited) |
| +0x78 | `void UnbindTexture(SharedTexture*)` | no (gated on m_bInited) |
| +0x7c | `void* CreateContext(RenderContextInit*)` (PC name CreateWorld) | no |
| +0x80 | `void DeleteContext(void* ctx)` (DestroyWorld) | no |
| +0x84 | `void Clear(LTRect*, uint32 flags)` | no |
| +0x88 | `LTBOOL Start3D(void)` | no |
| +0x8c | `LTBOOL End3D(void)` | no |
| +0x90 | `LTBOOL IsIn3D(void)` | no |
| +0x94 | `LTBOOL StartOptimized2D(void)` | no |
| +0x98 | `void EndOptimized2D(void)` | no |
| +0x9c | `LTBOOL IsInOptimized2D(void)` | never called |
| +0xa0 | `LTBOOL SetOptimized2DBlend(LTSurfaceBlend)` | no |
| +0xa4 | `LTBOOL GetOptimized2DBlend(LTSurfaceBlend*)` | never called |
| +0xa8 | `LTBOOL SetOptimized2DColor(HLTCOLOR)` | no |
| +0xac | `LTBOOL GetOptimized2DColor(HLTCOLOR*)` | never called |
| +0xb0 | `LTBOOL RenderScene(SceneDesc*)` | no |
| +0xb4 | `void RenderCommand(int argc, char** argv)` | no |
| +0xb8 | `void* GetHook(const char* name)` (PC: GetRenderObject) | no |
| +0xbc | `void SwapBuffers(uint32 flags)` (PC: FlipScreen) | no |
| +0xc0 | `uint32 GetInfoFlags(void)` | never called |
| +0xc4 | `void GetScreenFormat(PFormat*)` | no |
| +0xc8 | `void* CreateSurface(uint32 w, uint32 h)` | no |
| +0xcc | `void DeleteSurface(void*)` | no |
| +0xd0 | `void GetSurfaceInfo(void*, uint32* w, uint32* h, uint32* pitch)` | no |
| +0xd4 | `void* LockSurface(void*)` | no |
| +0xd8 | `void UnlockSurface(void*)` | no |
| +0xdc | `LTBOOL OptimizeSurface(void*, uint32 transColor)` | no (several sites gate on m_bInited) |
| +0xe0 | `void UnoptimizeSurface(void*)` | no (gated on m_bInited) |
| +0xe4 | `LTBOOL LockScreen(int l, int t, int r, int b, void** ppBits, uint32* pPitch)` | no |
| +0xe8 | `void UnlockScreen(void)` | no |
| +0xec | `void BlitToScreen(RenderBlitRequest*)` | **yes** (NULL means the engine's software path) |
| +0xf0 | `LTBOOL WarpToScreen(RenderBlitRequest*)` | **yes** |
| +0xf4 | `void MakeScreenShot(const char* filename)` | **no, and not gated on m_bInited** (F8, E/clientwin.cpp:1137-1156) |
| +0xf8 | `void ReadConsoleVariables(void)` | yes |
| +0xfc | `void BlitFromScreen(RenderBlitRequest*)` | **yes** |

### Per slot: who calls it, what it means, what d3d.ren and the Mac do

**+0x6c Init(RenderStructInit\* p)** → 0 = success, anything else = failure.
* **Called** once per r_InitRender (§1.2 step 10).
* **Requirements:**
  * Store `0xd5d` in `p->m_nVersion` (offset 0). d3d does it as its first instruction
    (w26_10020fd0_osoiqip.cpp:689; ship 0x100215b0); so does the Mac [0x149e68]. The incoming value is never read.
  * Return 0.
  * May rewrite `p->m_Mode` (copied to g_CurRenderMode afterwards); see §7.4 on keeping names consistent.
  * May rewrite RenderStruct +0x3c/+0x40.
* **Inputs:**
  * `p->m_Mode.m_Width/m_Height/m_BitDepth` (+0x208/+0x20c/+0x210 in the init block);
  * `p->m_Mode.m_InternalName` (+0x108), which picks the device;
  * `p->m_hWnd` (+0x218), the engine's main window, already created.

  **The renderer owns window sizing and style for the mode** (d3d: SetWindowPos, centring and
  AdjustWindowRect(WS_OVERLAPPEDWINDOW) for windowed; HWND_TOPMOST for fullscreen; w26_10020fd0_osoiqip.cpp:764-794).
* **d3d (0x100215a0, w26_10020fd0_osoiqip.cpp:678-814):**
  1. version; register then read cvars;
  2. "windowed" (string, atoi == 1), "SysMem", "nullrender";
  3. enumerate DirectDraw devices (failure → 10); pick the device whose name matches m_InternalName
     case-insensitively, else the first one that initialises;
  4. copy the name and description back into the mode;
  5. read the primary's pixel format into its PFormat;
  6. set up the world-side VisibleSet.

  Returns 0, 1 or 10.
* **Mac (r_Init [0x149e40]):**
  1. version; "windowed"; create then read cvars;
  2. AGL context (RGBA, double buffer, depth 32 or DepthBufferBits, stencil 8 with a retry without stencil);
  3. glViewport; filters; swap; capability strings;
  4. `glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.1666)`;
  5. writes +0x3c/+0x40;
  6. VisibleSet::Init; client arrays; ortho projection; texture manager; start states.

  Returns 0 or 1.

**+0x70 Term()**
* **Called** from r_TermRender after all surfaces and textures have been released (§1.3).
  Also call it yourself on your own Init failure paths, as d3d does.
* **d3d:** frees the device list and tears down D3D/DDraw.
* **Mac:** glFlush/glFinish, ShowCursor, frees draw modules and the texture manager, destroys the GL context.

**+0x74 BindTexture(SharedTexture\* t, LTBOOL bChanged)**: see §5.
* **Called:**
  * when a SharedTexture is created or resolved and has no render data (0x0041fe60 tail; tm_ReloadTextures 0x004202f0, E/clienttexmgr.cpp:569-585);
  * for every texture after renderer init and on the `RebindTextures` console command (bChanged = FALSE);
  * after ILTTexMod edits a texture (lttexmod.cpp 0x00488911, bChanged = TRUE).
* The wrapper 0x004628e0 calls it only while m_bInited.

**+0x78 UnbindTexture(SharedTexture\* t)**
* **Called** immediately before the engine frees a texture's data (0x00462900, E/clientrender.cpp:1037-1050), only while m_bInited.
* **Must** release render data and set `t->m_pRenderData` (+0x0c) to NULL.

**+0x7c CreateContext(RenderContextInit\* p)** → context pointer; NULL = failure.
* **Called** from CClientShell::CreateRenderWorld (0x00414860, E/clientmgr2.cpp:1670-1693) when a world is
  loaded and the renderer is up (also r_InitRender step 14).
* The engine stores the result at CClientMgr+0x1c8 and hands it back in `SceneDesc+0x34` and to +0x80.
* `p` points at a one-dword struct. On the PC its field is filled from `CClientShell::m_pClientMgr`
  (E/clientmgr2.cpp:1685). The Mac agent read the field as "the world" (Mac [0x14a1f0], inferred name).
  **The 3D half must settle what the first dword is** before using it.
* **NULL fails the whole renderer init** with LT_ERRORBINDINGWORLD.
* **d3d (0x10033160, D/d3ddraw.cpp:247-279):** allocates 0x10 bytes; +8 = *p; +0xc (uint16) = 0xffff;
  builds lightmap pages unless the NoLMPages cvar is set.
* **Mac:** identical shape (dalloc_z(0x10)).

**+0x80 DeleteContext(void\* ctx)**
* **Called** from DestroyRenderWorld (0x004148a0) and r_TermRender. The engine tests ctx != NULL first.
* d3d and the Mac both accept NULL as well, tear down lightmap pages and free.

**+0x84 Clear(LTRect\* r, uint32 flags)**
* **Called** from ci_ClearScreen (0x00408dc0, E/clientinterface.cpp:1214-1257) only. It is **not** gated on IsIn3D.
* The engine always passes a non-NULL rect:
  * for a NULL request, `{0,0,W,H}`;
  * otherwise each edge clamped to [0,W]/[0,H].
* If the `ForceClear` console variable is set, CLEARSCREEN_SCREEN is ORed into flags.
* **Flags (ltcodes.h:142-143):** `1` = CLEARSCREEN_SCREEN (clear colour), `2` = CLEARSCREEN_RENDER (clear Z and stencil).
* The answer is ignored.
* **d3d (0x100262f0):** colour = black (0xff000000), z = 1.0.
  * Full screen with flag 1: colour-fill Blt of the back buffer.
  * Sub-rect: D3D Clear of one rect.
  * Marks the rect dirty.
* **Mac:** glScissor for a sub-rect, but **without flipping Y** (latent bug, inferred). Colour (0,0,0,1), depth 1, stencil 0.
* **For GL:** scissor at `(l, H-b, r-l, b-t)`.

**+0x88 Start3D()** → TRUE if entered. The engine ignores the answer.
* **Called** from ci_Start3D, which refuses with LT_ALREADYIN3D if IsIn3D() is already TRUE (E/clientinterface.cpp:2747-2757).
* **d3d:** BeginScene; sets g_bIn3D.
* **Mac:** g_bIn3D = 1; return 1.

**+0x8c End3D()** → ignored.
* **Called** from ci_End3D (only if IsIn3D()).
* **d3d (D/d3ddraw.cpp:215-236):** if Optimized2D is still open, EndOptimized2D; clear the flag; EndScene; on surface loss restore surfaces.
* **Mac:** flag = 0, return 1.
* Recommend closing an open Optimized2D bracket here too, as d3d does.

**+0x90 IsIn3D()** → the flag.
* **Called** 8 times as guards (E/clientworldinfo.cpp:174-188).
* ci_Start3D refuses when it is TRUE. RenderCamera, RenderObjects, StartOptimized2D, EndOptimized2D,
  SetOptimized2DBlend, SetOptimized2DColor and End3D refuse with LT_NOTIN3D when it is FALSE.
* So **Optimized-2D only exists inside a 3D bracket.**

**+0x94 StartOptimized2D()** → the engine ignores the answer. See §4.4.

**+0x98 EndOptimized2D()** See §4.4.

**+0x9c IsInOptimized2D()** Never called by lithtech.exe. d3d returns its flag (0x10034c60); so does the Mac.

**+0xa0 SetOptimized2DBlend(LTSurfaceBlend e)** → TRUE for 0..6, else FALSE.
* The engine returns `!answer` as the LTRESULT (ci_SetOptimized2DBlend 0x004091e0, E/clientworldinfo.cpp:383-392).
  **The answer is used**: return nonzero on success.
* The mapping is in §4.4.

**+0xa4 GetOptimized2DBlend(LTSurfaceBlend\*)** Never called (ILTClient's entry is never wired).
d3d and the Mac store the value and return 1.

**+0xa8 SetOptimized2DColor(HLTCOLOR c)** → nonzero.
* The engine returns `!answer` (E/clientinterface.cpp:2771-2780).
* d3d and the Mac store `(c & 0x00ffffff) | 0xff000000` (D/d3ddraw_os.cpp:147; Mac [0x14d0a0]).

**+0xac GetOptimized2DColor(HLTCOLOR\*)** Never called. Store and return 1.

**+0xb0 RenderScene(SceneDesc\*)** → ignored.
* **Called** from 0x00411700 (RenderCamera/RenderObjects), only if m_bInited.
* The SceneDesc layout is written out in E/clientblit.cpp:419-478 (3D half).
* d3d and the Mac refuse with an error message when Optimized2D is open: "Error: tried to render 3D while in optimized 2D mode."
* Both always return 0.

**+0xb4 RenderCommand(int argc, char\*\* argv)**
* **Called** from the console commands "RenderCommand" and "RCom" (cc_RenderCommand 0x0041de80, E/ccclient.cpp:517-523).
  argv[0] is the first word after the command.
* **d3d (0x10032d00):** LISTDEVICES, LISTTEXTUREFORMATS, LISTDEVICECAPS, FREETEXTURES, LOADGAMMA, PortalFile.
* **Mac:** gl_strings, ListTextureFormats, CheckAllHeaps, info, gamma \<percent\>, glstate.
* Anything is fine; doing nothing is legal.

**+0xb8 GetHook(const char\* name)** → pointer or NULL.

Every engine caller (absolute refs 0x00401dff, 0x00401e33, 0x004022f2, 0x0048adcf, 0x0048b453):

| caller | name asked | on NULL |
|---|---|---|
| BinkVideoInst::InitScreen 0x00401dd0 (E/binkvideo.cpp:1269) | "LPDIRECTDRAW" | skips setup, returns LT_NOTINITIALIZED (E/binkvideo.cpp:1356). StartVideo then releases the video and fails (E/binkvideo.cpp:982-991). Restore() Terms it |
| same function, nested (E/binkvideo.cpp:1276) | "BACKBUFFER" | only reached if LPDIRECTDRAW was non-NULL; passed to BinkIsSoftwareCursor |
| BinkVideoInst::UpdateOnScreen 0x004022e0 (E/binkvideo.cpp:1592) | "BACKBUFFER" | returns LT_NOTINITIALIZED ("GetHook(BACKBUFFER) failed") |
| SmackVideoInst::InitScreen 0x0048ada0 (E/smackvideo.cpp:601) | "LPDIRECTDRAW" | same as Bink: fails with LT_NOTINITIALIZED |
| SmackVideoInst::DrawFrame/UpdateOnScreen 0x0048b440 (E/smackvideo.cpp:781) | "BACKBUFFER" | LT_NOTINITIALIZED |
| **modern build only**, ci_ModernFillScreen (E/warpblit.cpp:935-960) | "FillScreenRect" | falls back to the retail software fill (LockScreen) |

All five retail callers are the **screen-mode** video players and expect real DirectDraw COM objects
(GetDisplayMode, CreateSurface, Blt).

**An OpenGL renderer must return NULL.** Any non-NULL value would be called through as an IDirectDraw7 or
IDirectDrawSurface7. With NULL, on-screen movies fail to start and the game moves on.

Texture-mode video (`InitTexture`) does not use the hook. Playing movies under GL would need a texture-video path; that is game/engine work.

**Optional modern hook:** `GetHook("FillScreenRect")` → `LTBOOL __cdecl FillScreenRect(LTRect* pRect, uint32 nPixel)`.
* `nPixel` is already converted to the screen format (low 16 bits used on a 16-bit screen).
* Return FALSE to make the engine do the software fill.
* d3d's modern implementation is in D/d3dblit.cpp. MODERN.md M-P0-03.

Implementing it in GL (a scissored glClear or a solid quad) avoids a full-screen LockScreen per fill.

What the hooks answer:
* d3d: "LPDIRECTDRAW" → IDirectDraw7, "BACKBUFFER" → back-buffer surface, otherwise NULL (D/w26_10020fd0_osoiqip.cpp:359-373).
* Mac: "BRECT", "DTEXT", "HWND", "CGRAFPTR"; nothing in the PC engine asks for them.

**+0xbc SwapBuffers(uint32 flags)**
* **Called** from CClientMgr::FlipScreen 0x00410930 (E/clientblit.cpp:315-321), only if m_bInited.
  The flags are ILTClient::FlipScreen's, unchanged.
* **Flags (ltcodes.h:136-139):** 1 FLIPSCREEN_CANDRAWCONSOLE, 2 FLIPSCREEN_COPY, 4 FLIPSCREEN_DIRTY.
* **d3d (0x100371e0):**
  * DIRTY: blits only dirty rects;
  * windowed: Blt the render surface to the client area;
  * COPY: Blt back to front;
  * otherwise: Flip;
  * CANDRAWCONSOLE ignored.
* **Mac:** ignores the flags; optional FPS text; glFinish; aglSwapBuffers.
* **For GL:** SwapBuffers. DIRTY/COPY can both be treated as a full swap, provided the back buffer content is not
  relied on afterwards. **(inferred)** The engine always repaints before the next flip.

**+0xc0 GetInfoFlags()** → **0**. Never called by the engine. d3d `xor eax,eax; ret` (D/d3dmisc.cpp:73); Mac returns 0.

**+0xc4 GetScreenFormat(PFormat\* pf)**
* **Called** once per init, from r_PublishRenderStruct with `&g_ScreenFormat` (0x004b1758).
* The engine immediately calls `GetBytesPerPixel()` on it. See §3.4 for the exact fill.

**+0xc8..+0xe0 surfaces**: see §4.

**+0xe4 LockScreen / +0xe8 UnlockScreen**: see §4.5.

**+0xec BlitToScreen, +0xf0 WarpToScreen, +0xfc BlitFromScreen**: see §4.3, §4.5.

**+0xf4 MakeScreenShot(const char\* filename)**
* **Called** on the WM_KEYUP F8 handler (E/clientwin.cpp:1137-1156) with "Screenshot%d.bmp".
  The first index 0..2999 whose file does not exist is used; the base name is set by the "ScreenShot" console command.
* **Not null-tested and not gated on m_bInited.**
* **d3d (0x10036e40):** locks the back buffer, writes a bottom-up 24-bit BI_RGB **.bmp**, reports through +0x14.
* **Mac:** writes a 24-bit TGA despite the name.
* **For GL:** glReadPixels(GL_BACK or GL_FRONT, GL_BGR) → 24-bit BMP, which is bottom-up exactly as GL returns it.
  If the context is not up, return without doing anything.

**+0xf8 ReadConsoleVariables()**
* **Called** from BindAllTextures (§1.2 step 16): after every renderer init and on the "RebindTextures" command.
  d3d also calls it from its own Init.
* Re-reads every renderer cvar and applies filter, fog and dither state (§8).
* Note: the engine does **not** call it when a cvar changes at run time. (Changes apply after RebindTextures or a renderer restart.)

---------------------------------------------------------------------------------------------------

## 3. Structures

### 3.1 RMode (0x214 bytes), ltbasedefs.h:332-342

```c
struct RMode {                    // off
    LTBOOL  m_bHardware;          // 0x000
    char    m_RenderDLL[256];     // 0x004
    char    m_InternalName[128];  // 0x104   "what the DLLs use to identify a card"
    char    m_Description[128];   // 0x184   friendly name
    uint32  m_Width;              // 0x204
    uint32  m_Height;             // 0x208
    uint32  m_BitDepth;           // 0x20c   16 or 32
    RMode*  m_pNext;              // 0x210
};                                // 0x214
#define RDLL_DESCRIPTION_STRINGID 5
```

The Mac `_GetRModeFromConsoleVariables` [0x011d90] and r_InitRender's echo [0xecb1c-0xecba4] confirm every offset.

### 3.2 RenderStructInit (0x21c bytes)

E/clientrender.cpp:265-282; D/w26_10020fd0_osoiqip.cpp:136-141.

```c
struct RenderStructInit {
    uint32 m_nVersion;   // 0x000  OUT: renderer must store 0xd5d (RENDERSTRUCT_VERSION)
    RMode  m_Mode;       // 0x004  IN/OUT (copied to g_CurRenderMode after a good init)
    HWND   m_hWnd;       // 0x218  IN: engine main window (Mac: 0)
};
```

### 3.3 LTRect, LTWarpPt, LTSurfaceBlend

```c
struct LTRect   { int left, top, right, bottom; };          // right/bottom exclusive (engine sizes = right-left)
struct LTWarpPt { float source_x, source_y, dest_x, dest_y; };  // ltbasetypes.h:94-98
enum LTSurfaceBlend { LTSURFACEBLEND_ALPHA=0, SOLID=1, ADD=2, MULTIPLY=3,
                      MULTIPLY2=4, MASK=5, MASKADD=6 };      // ltbasedefs.h:348-357
```

### 3.4 PFormat: what +0xc4 must fill (0x38 bytes)

Sources: E/lttexmod.cpp:96-130 and 1232-1273; E/clientmgr2.cpp:2568-2577.

```c
class PFormat {                // engine object at 0x004b1758, vptr 0x004a1ab4 (one virtual)
  // +0x00 vptr: slot 0 = void __thiscall Init(uint32 type, uint32 a, uint32 r, uint32 g, uint32 b)  (0x0045f0e0)
  uint32 m_nType;        // +0x04  BPPIdent: 0 8P, 1 8-bit, 2 16-bit, 3 32-bit, 4/5/6 DXT1/3/5, 7 32P
  uint32 m_Masks[4];     // +0x08  A, R, G, B
  uint32 m_nBits[4];     // +0x18  bit count of each mask
  uint32 m_nShifts[4];   // +0x28  index of lowest set bit (a zero mask gives shift 32 and 0 bits)
};
```

**Two ways to fill it:**
* **Copy the fields.** d3d copies its own PFormat's bytes +0x04..+0x37 into the engine object and leaves the vptr
  alone (0x10036df0, D/d3dblit.cpp:991). Its own PFormat was filled by its Init from DDPIXELFORMAT:
  type 2 if 16-bit, else 3 (w26_10020fd0_osoiqip.cpp:65-76).
* **Call the engine's virtual.** The Mac calls `pf->Init(2, 0, 0xf800, 0x07e0, 0x001f)` (Mac [0x14c060]).
  `Init` stores the five fields and recomputes bits and shifts.

  From a C++ DLL that is a call through `(*(void***)pf)[0]` as `__thiscall` with five dword arguments.

Either way, **the bits and shifts must be right**. The engine's converters read them, and the copy method does not
recompute them.

**What the engine supports as a screen format:** type **2** (16-bit) or **3** (32-bit) only.
* The software blitters (E/cissurf.cpp:609-648, 1206-1260, 1388-1430), FillRect and the colour-conversion tables
  handle only these two. Bytes per pixel come from {1,1,2,4,0,0,0,1}[type] (E/fmconvert.cpp:2094).
* `DrawSurfaceMasked` implements only type 2 (E/cissurf.cpp:1622). Retail d3d at 32 bpp has the same gap.

**Recommended:**
* 32-bit mode: `type 3, A=0, R=0x00ff0000, G=0x0000ff00, B=0x000000ff` (X8R8G8B8, which is what DirectDraw reports).
* 16-bit mode: `type 2, A=0, 0xf800, 0x07e0, 0x001f`.

**Use A=0 (inferred, why it matters).** Colour keys are compared as whole pixel values:
* the engine converts an `HLTCOLOR` (0x00RRGGBB) to the screen format with
  `FMConvertMgr::PValueToFormatColor(&g_ScreenFormat, hColor, &out)` (0x0045f5e0);
* the transparent software blit does `if (*src != key) *dst = *src` on raw 32-bit words (E/cissurf.cpp:1406).

With an alpha mask, pixels written by the converter could carry alpha bits that keys built elsewhere do not.

**A screen format is also the surface format.** The engine:
* fills surfaces with its converter into `g_ScreenFormat` (cis_DrawDIBToSurface, E/cissurf.cpp:851-871);
* copies them row by row as `w * g_nBytesPerPixel` bytes (surface_CopyOut, E/clientmgr2.cpp:2522-2545);
* reads and writes them in the transparent, solid and masked blitters.

### 3.5 The engine's CSurface (HSURFACE), for reference

E/cissurf.cpp:73-87. Allocated by the engine from a 0x2c-byte struct bank.

```c
struct CSurface {
  CSurface* m_pPrev;      // 0x00
  CSurface* m_pNext;      // 0x04  circular list of live surfaces
  void*     m_pSysMem;    // 0x08  engine-side copy, only while no renderer is loaded
  void*     m_pRenderData;// 0x0c  = what +0xc8 CreateSurface returned  (renderer-owned)
  uint32    m_nWidth;     // 0x10  from +0xd0
  uint32    m_nHeight;    // 0x14  from +0xd0
  uint32    m_nPitch;     // 0x18  BYTES, from +0xd0 -- the engine uses this, never asks Lock for a pitch
  uint32    m_nFlags;     // 0x1c  bit0 = is the screen, bit1 = "optimized" (OptimizeSurface was asked),
                          //       bit2 = dirty since last optimize
  HLTCOLOR  m_hTransColor;// 0x20  0xffffffff = no key, else 0x00RRGGBB (bit 31 stripped)
  void*     m_pUserData;  // 0x24
  float     m_fAlpha;     // 0x28  0..1, ILTClient::SetSurfaceAlpha (clamped), default 1.0
};
// The screen is the static CSurface at 0x004b1620 (m_nFlags = 1); "is this the screen" is a pointer compare.
```

### 3.6 RenderBlitRequest (+0xec / +0xf0 / +0xfc)

Built on the caller's stack. Sources: E/cissurf.cpp:1052-1074; E/clientinterface.cpp:2042-2071;
E/clientblit.cpp:338-399. d3d's view is D/w6_10034f10_osoi.cpp; the Mac's is r_BlitToScreen [0x14e020].

```c
struct RenderBlitRequest {
  void*     m_pSurfaceData; // 0x00  renderer data of the NON-screen surface:
                            //       source for +0xec/+0xf0, destination for +0xfc
  uint32    m_bTransparent; // 0x04  bit 0: honour the colour key (only 0 or 1 is ever stored)
  uint32    m_TransColor;   // 0x08  the key, ALREADY IN SCREEN PIXEL FORMAT (engine g_TransColor 0x004b161c).
                            //       Not constructed: garbage-free only because every caller stores it.
                            //       16-bit screen: low 16 bits (the Mac reads the halfword).
  LTRect*   m_pSrcRect;     // 0x0c  source rect: surface coords for +0xec/+0xf0, SCREEN coords for +0xfc
  LTRect*   m_pDestRect;    // 0x10  dest rect: screen coords for +0xec, surface coords for +0xfc
  float     m_fAlpha;       // 0x14  CSurface::m_fAlpha of the source (1.0 for +0xfc)
  LTWarpPt* m_pPoints;      // 0x18  +0xf0 only
  uint32    m_nPoints;      // 0x1c  +0xf0 only, always 4
};
```

**Rects are never NULL**, and they are already clipped by the engine:
* cis_ClipBlit 0x0043a2a0 for plain blits;
* cis_ClipScaledBlit 0x0043a0e0 for scaled blits.

**src and dest sizes differ for ScaleSurfaceToSurface\***: the renderer must **stretch**.

**Where the key and m_bTransparent come from:**
* `DrawSurfaceToSurfaceTransparent` / `ScaleSurfaceToSurfaceTransparent` / `TransformSurfaceToSurfaceTransparent`
  convert their HLTCOLOR argument into g_TransColor first (E/clientinterface.cpp:2579-2599, 2729-2740).
* For a non-transparent blit, m_TransColor is whatever g_TransColor last held, and m_bTransparent = 0.
* The key is the *per-call* colour, **not** the surface's `m_hTransColor` given to OptimizeSurface.
  They usually agree, but a renderer must key on the request's colour.

### 3.7 SharedTexture (engine, 0x34 bytes)

Sources: E/clienttexmgr.cpp:79-156; E/clientrender.cpp:250-260; E/lttexmod.cpp:259-276.

```c
struct SharedTexture {
  void*        m_BankLink[2];   // 0x00
  TextureData* m_pEngineData;   // 0x08  engine's system-memory copy (may be NULL; get it via callback +0x08)
  void*        m_pRenderData;   // 0x0c  RENDERER-OWNED. d3d/Mac store their texture object here; the engine
                                //       tests it for NULL to decide whether to call BindTexture.
  LTLink       m_Link;          // 0x10  on the client manager's texture list
  uint32       m_Unknown1c[2];  // 0x1c
  void*        m_pFile;         // 0x24  file identifier
  SharedTexture* m_pLinkedTexture; // 0x28  DetailTex / EnvMap / EnvMapAlpha target (set when data loads)
  uint32       m_nLinkType;     // 0x2c  0 detail, 1 envmap, 2 envmap-in-alpha (E/clientrender.cpp:651-685)
  uint16       m_nFrameCode;    // 0x30
  uint16       m_nRefCount:15, m_bMarked:1; // 0x32
};
```

d3d and the Mac both reach a texture's own RTexture chain "via +0x30" (Mac rgl_BindTexture). Here that means
fields of their RTexture, not of SharedTexture **(inferred; ignore)**.

### 3.8 TextureData (engine, 0x18c bytes) and the DTX data as a renderer reads it

Sources: E/clientdsi.cpp:921-1035 (classes), 1122-1220 (dtx_Alloc), 1239-1320 (dtx_Create).

```c
struct DtxHeader {                 // 0xa4 bytes, at TextureData+0x08 (the file header verbatim)
  uint32 m_ResType;                // +0x08  0
  int32  m_Version;                // +0x0c  -5
  uint16 m_BaseWidth;              // +0x10
  uint16 m_BaseHeight;             // +0x12
  uint16 m_nMipmaps;               // +0x14  1..15 accepted by the loader, but only 8 mip records exist (clamp!)
  uint16 m_nSections;              // +0x16
  uint32 m_IFlags;                 // +0x18  DTX flags: 1 FULLBRITE, 2 PREFER16BIT, 0x40 = TDFLAG_TEXMOD (engine
                                   //        keeps the data), 8 = sections fixed, 0x80 PREFER4444, 0x100 PREFER5551
                                   //        (Mac r_SetupTextureSurface reads all of these at TD+0x18, 0x180628/0x180c6c)
  uint32 m_UserFlags;              // +0x1c
  uint8  m_Extra[12];              // +0x20  [0] texture group (0..9 -> GroupOffsetN cvar)
                                   //        [1] mipmaps to use (0 = 4)
                                   //        [2] BPPIdent: 0 or 3 = 32-bit ARGB; 4/5/6 = DXT1/3/5
                                   //        [3] extra mip offset (when no S3TC)  [4] UI mip offset
                                   //        +0x24/+0x25 mip scale/LOD, +0x26 float detail scale, +0x2a int16 detail angle
  char   m_CommandString[128];     // +0x2c  parsed for "DetailTex x", "EnvMap x", "EnvMapAlpha x" (engine),
                                   //        "ColorKey r g b" (honoured only when AlphaTest==0), "AlphaRef n" (renderer)
};
struct TextureMip {                // 0x18 bytes
  uint32 m_Width, m_Height;        // +0x00/+0x04
  uint8* m_pData;                  // +0x08
  uint32 m_Pitch;                  // +0x0c  bytes: w*4 for 32-bit, w for type 7, 0 for DXT (block data is packed)
  uint8* m_pBump; uint32 m_BumpPitch; // +0x10/+0x14
};
struct TextureData {
  void*         vptr;              // +0x00  one virtual: SetupPFormat(PFormat*) -- type 3 gives ARGB 8888
  uint32        m_Unknown04;       // +0x04
  DtxHeader     m_Header;          // +0x08..+0xab
  LTLink        m_Link;            // +0xac  engine's TextureData list
  uint32        m_AllocSize;       // +0xb8
  void*         m_pSections;       // +0xbc  (not loaded by the render path: bLoadSections = FALSE)
  SharedTexture* m_pSharedTexture; // +0xc0
  uint32        m_Unknownc4;       // +0xc4
  void*         m_pDataBuffer;     // +0xc8  one dalloc'd block holding all levels
  TextureMip    m_Mips[8];         // +0xcc
};
```

**32-bit pixel layout:** 32-bit DTX pixels are **ARGB 8888** stored as little-endian uint32: A=0xff000000,
R=0x00ff0000, G=0x0000ff00, B=0x000000ff (PFormat::InitPValue, E/lttexmod.cpp:1250; dtx_SetupFormat,
E/clientdsi.cpp:1356-1370). In memory the bytes are B,G,R,A, so the GL upload is `GL_BGRA, GL_UNSIGNED_BYTE`.

The Mac masks are big-endian and do not carry over.

**DXT levels:** the data is the raw S3TC block stream, `fm_GetImageDataSize` bytes per level.
Upload with `glCompressedTexImage2D(GL_COMPRESSED_RGB[A]_S3TC_DXT{1,3,5}_EXT)`, or decompress yourself.
* d3d honours "S3TCEnable".
* The Mac disables DXT outright (CheckDXTFormats [0x17f050]).

**Palettised types:** 0 (8P) and 7 (32P) need a palette from the sections, which this path does not load.
Neither renderer has a palette path. Type 1 (8-bit) is created by the engine only for bump maps.
**Treat 0/1/7 as unsupported.** Bind fails, the texture is left without render data, and the 3D side draws it untextured.

**Ownership:**
* The engine allocates, owns and frees TextureData (dalloc; freed by sub_461dc0, E/clientrender.cpp:533-541).
* The renderer must never free it. It only asks for it (+0x08) and releases the engine's copy (+0x0c).

---------------------------------------------------------------------------------------------------

## 4. The 2D path

### 4.1 Surface creation and info (+0xc8, +0xcc, +0xd0)

**`void* CreateSurface(uint32 w, uint32 h)`** → opaque renderer pointer, or NULL.
* **Called** by cis_CreateSurface 0x0040c130 (E/cissurf.cpp:907-958):
  * after the engine checks `0 < w,h <= 5000` and `g_pRender != NULL`;
  * also by surface_CopyIn on renderer restart (§4.6).
* Immediately afterwards the engine calls **GetSurfaceInfo(p, &w, &h, &pitch)** and stores all three in the CSurface.
  A renderer may round w/h up, but the engine then believes the new size; d3d and the Mac return the requested size.
* **The renderer allocates the pixel memory, with its own allocator.**
* **Format:** pixels are in the screen PFormat, rows top-down, `pitch` bytes apart (pitch ≥ w*bpp).
  * d3d: a SYSTEMMEMORY DirectDraw surface in the display format (D/d3dblit.cpp:412).
  * Mac: `malloc(2*w*h+0x2f)`, 565, pitch = 2w, pixels at +0x2c (Mac r_CreateSurface [0x14cca0]).
* Contents are undefined at creation (the Mac zeroes them). The engine fills them before use: DIB load, font
  rendering or FillRect.

**`void DeleteSurface(void*)`**
* **Called** by ILTClient::DeleteSurface and by surface_CopyOut / cis_FreeAllSurfaces (cis_FreeRenderData 0x0040c5a0).
* Free the pixels and any GL textures.

**`void GetSurfaceInfo(void* p, uint32* w, uint32* h, uint32* pitch)`**
* Pitch is **in bytes**.
* d3d and the Mac null-test each out pointer (D/d3dmisc.cpp:99; Mac [0x14cf50]).

### 4.2 Lock, unlock, optimize (+0xd4..+0xe0)

**`void* LockSurface(void* p)`** → pointer to pixel (0,0), or NULL.
* No pitch is returned; the engine uses CSurface+0x18.
* It is called for both reads and writes, by:
  * the inline cis_LockSurface (E/cissurf.cpp:498-529);
  * the font module (fntdraw.cpp 0x00488f90);
  * the warp blitters (E/diblit.cpp:1210-1300);
  * FillRect (E/warpblit.cpp:888);
  * surface_CopyOut/CopyIn;
  * ILTClient::LockSurface / GetPixel / SetPixel (inferred from the same inline).
* **Mark the surface dirty here.** The Mac sets its dirty byte on every lock (Mac [0x14cfc0]), so the next draw
  re-uploads. The engine also sets its own flag bit 2 on write locks and re-calls OptimizeSurface before the next
  accelerated blit (sub_40d7d0, E/clientblit.cpp:353-361). Relying on LockSurface is simpler and catches read-only locks too.

**`void UnlockSurface(void* p)`** Pairs every +0xd4. The Mac does nothing here.

**`LTBOOL OptimizeSurface(void* p, uint32 transColor)`**
* **Called by:**
  * `ILTClient::OptimizeSurface(hSurf, hTrans)` (E/clientinterface.cpp:3124-3147);
  * the re-optimize gate sub_40d7d0 before each accelerated blit, when flag bit 1 and bit 2 are both set (answer ignored);
  * surface_CopyIn after a renderer restart, for surfaces that had been optimized.
* **The key argument** is an **HLTCOLOR 0x00RRGGBB, not screen format**:
  * `0xffffffff` = no key;
  * otherwise the caller's key with bit 31 stripped.
  * A colour passed to ILTClient without bit 31 (i.e. not SETRGB_T) becomes 0xffffffff.
* **Return value:** nonzero = success. On zero, ILTClient::OptimizeSurface returns LT_ERROR, though bit 1 stays set.
* **d3d (0x10034580, D/w53_10033f30_osoiqip.cpp:662):**
  * returns FALSE unless the cvar **"OptimizeSurfaces"** is nonzero (default 0!);
  * otherwise builds 64x64 D3D texture tiles in a 1555/4444 "interface" format with alpha = 0 at the key, and returns TRUE;
  * re-run, it refills the tiles.
* **Mac:** returns 1 and does nothing. Keying is done per blit from the request.
* **Recommendation:** return TRUE and either do nothing (the Mac way) or pre-upload. Keep the HLTCOLOR key only as a
  hint (convert with your own PFormat to compare).

**`void UnoptimizeSurface(void* p)`**
* **Called** by ILTClient::UnoptimizeSurface when bit 1 was set (E/clientinterface.cpp:1982-1997).
* d3d frees the tiles; the Mac does nothing.

### 4.3 BlitToScreen (+0xec) and WarpToScreen (+0xf0)

**When the engine calls BlitToScreen.** This is the only route by which game 2D reaches the screen without a
CPU lock. It requires the slot to be non-null and the **destination to be the screen surface**, from two places:
* **cis_DoDrawSurfaceToSurface** 0x0040d290 (E/cissurf.cpp:1098-1205), behind:
  * `DrawSurfaceToSurface` (opaque, m_bTransparent = 0);
  * `DrawSurfaceToSurfaceTransparent` (keyed, 1).

  Never for DrawSurfaceSolidColor or DrawSurfaceMasked, which always go through the software path and LockScreen.
  Rects are the same size here.
* **sub_40dcc0** 0x0040dcc0 (E/clientinterface.cpp:2074-2170), behind:
  * `ScaleSurfaceToSurface` (opaque);
  * `ScaleSurfaceToSurfaceTransparent` (keyed).

  Rects may differ in size, so the blit is a **stretch**. ScaleSurfaceToSurfaceSolidColor never comes here.

**Before calling**, the engine:
* re-optimizes the source if it is dirty (§4.2);
* fills the request (§3.6);
* calls the slot, **ignores the result**, and sets the screen's flag bit 2.

There is no fallback, so **anything not drawn is lost**.

**d3d (0x10036cf0, D/d3dblit.cpp:842):**
* **Textured path:** used when the surface has tiles and Optimized2D and 3D are both open.
  * A TL quad per tile, scaled src→dest.
  * Diffuse = Optimized2D colour with alpha = `m_fAlpha*255`, stage 0 = tex × diffuse.
  * Keying comes from the tile alpha; the request's flag is ignored.
  * If the blend is not ALPHA and the surface is un-optimized, it optimizes on the fly.
* **DirectDraw path** otherwise: ends the scene if in 3D; `SetColorKey` with the request key when m_bTransparent;
  stretching Blt; BeginScene. **Alpha, colour and blend are ignored.**

**Mac (r_BlitToScreen [0x14e020] + r_DrawImage [0x14d890] + r_UpdateTexImage [0x14d380]):**
1. Push GL state.
2. Outside Optimized2D: disable depth and fog, depth mask 0, enable blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA.
3. If the key changed, mark the surface dirty.
4. If dirty, or tiles are missing or stale (a context-generation counter): convert the whole surface to RGBA8,
   with key pixels → 0x00000000 when transparent, and re-upload.
   * Tiles: one power-of-two tile if both sides are < 64, else 64x64 tiles.
5. Per tile, clipped to the src rect:
   * GL_NEAREST, GL_CLAMP, MODULATE;
   * `glColor4f(opt2DColor or white, m_fAlpha)`, where the Optimized2D colour applies only while in Optimized2D;
   * one GL_QUAD mapped by the src→dest scale.
6. Pop state.

**Recommendation for GL:**
* Keep a per-surface GL texture set, rebuilt when dirty or when the key or transparency changes.
* Convert from the screen format to RGBA8 with A = 0 for key pixels when `m_bTransparent & 1`, else A = 255.
* Draw with:
  * an ortho projection `glOrtho(0,W,H,0,-1,1)`;
  * depth test off;
  * colour = (Optimized2D colour if in Optimized2D, else white) × alpha `m_fAlpha`;
  * the current Optimized2D blend if in Optimized2D, else SRC_ALPHA/ONE_MINUS_SRC_ALPHA;
  * an alpha test (or rely on blend) so key pixels vanish.
* **Use GL_NEAREST** for 1:1 blits. Text and HUD art are pixel-exact. Linear filtering bleeds the key colour's
  neighbours unless you also clamp (the Mac uses NEAREST).
* Use GL_CLAMP_TO_EDGE.
* **Fix the Mac's colour-conversion problems:**
  * its 565→RGBA8 drops green's low bit;
  * it leaves tile padding uninitialised.
* Fill padding with A=0 or clamp UVs to the valid region.

**`LTBOOL WarpToScreen(RenderBlitRequest*)`**
* **Called** only by sub_40e4d0 (TransformSurfaceToSurface[Transparent], E/clientinterface.cpp:2608-2722) when the
  destination is the screen.
* The request carries 4 LTWarpPts:
  * dest = the rotated and scaled corners;
  * source = the source corners (0,0)…(w-1,h-1), clamped.
* srcRect = {0,0,w-1,h-1}; destRect = {0,0,screenW-1,screenH-1}.
* **TRUE** means drawn; the engine marks the screen and returns LT_OK. **FALSE** means the engine falls back to its
  software warp onto the screen through LockScreen.
* The plain `WarpSurfaceToSurface*` entries never call +0xf0; they are always software.
* **d3d:** TRUE only if the surface is optimized and both 3D and Optimized2D are open. Draws the whole surface as a
  parallelogram from points 0, 1 and 3.
* **Mac:** returns 1 and draws nothing (a bug: transformed sprites vanish).
* **For GL:** draw a textured quad with corners `pts[i].dest_*` and UVs `pts[i].source_*/texsize` (+0.5 texel), and
  return TRUE. Honour the key, alpha and Optimized2D state as in BlitToScreen. NOLF's game code never calls Transform*
  (src/game has no call site), so returning FALSE is an acceptable first step.

### 4.4 Optimized 2D

**What it is:** a bracket **inside** Start3D/End3D (§2 +0x90) in which surface blits are drawn as blended,
colour-modulated textured quads instead of copies. The SDK says: "SetOptimized2DBlend … defaults to
LTSURFACEBLEND_ALPHA; drawing a surface with a blend mode other than _ALPHA will automatically optimize the surface"
(iltclient.h:166-176).

**How the game uses it** (src/game/ClientShellDLL):
* Every HUD and menu draw is wrapped in Start/EndOptimized2D: InterfaceMgr.cpp (about 10 sites),
  GameClientShell.cpp:7034, LoadingScreen.cpp:460.
* **Drop shadows** (MessageMgr.cpp:389-395, Subtitle.cpp:210-216):
  1. `SetOptimized2DBlend(MASK)` + `SetOptimized2DColor(back)` + draw text;
  2. `SetOptimized2DBlend(ADD)` + `SetOptimized2DColor(fore)` + draw text;
  3. reset to white and ALPHA.
* Blend use across the game: ADD ×12, ALPHA ×13, MASK ×12, MASKADD ×1.
* **Fades and letterboxing** use `SetSurfaceAlpha` (m_fAlpha) on a surface blitted to the screen
  (InterfaceMgr.cpp:466, 4680-4701, 4841).

**+0x94 StartOptimized2D():**
* d3d (D/d3dopt2d.cpp:248):
  * returns TRUE if already open; FALSE with no device or not in 3D;
  * fog/Z off;
  * point filtering unless `FilterOptimized`;
  * stage 0 tex×diffuse for colour and alpha;
  * blend SRCALPHA/INVSRCALPHA.
* Mac: set flag; push FOG bit; 2D ortho (r_Go2D); depth test off, depth mask 0; blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA.
* **Both reset the hardware blend to ALPHA, but leave the stored blend value unchanged** (inferred from both bodies).

**+0x98 EndOptimized2D():** restore Z, fog, filters; blend off; leave 2D projection.

**+0xa0 blend mapping.** Identical in d3d (D/d3dopt2d.cpp:409) and the Mac (r_SetOptimized2DBlend [0x14d1f0]).
Both store the value even when it is invalid, apply it immediately, and return TRUE for 0..6.

| e | name | glBlendFunc(src, dst) |
|---|---|---|
| 0 | ALPHA | SRC_ALPHA, ONE_MINUS_SRC_ALPHA |
| 1 | SOLID | ONE, ZERO |
| 2 | ADD | ONE, ONE |
| 3 | MULTIPLY | DST_COLOR, ZERO |
| 4 | MULTIPLY2 | DST_COLOR, SRC_COLOR |
| 5 | MASK | ZERO, ONE_MINUS_SRC_COLOR |
| 6 | MASKADD | ONE, ONE_MINUS_SRC_COLOR |

Notes on the blend table:
* With the colour key turned into alpha 0, the non-ALPHA modes ignore source alpha. Key pixels must therefore also
  be **black** in RGB (or be dropped by an alpha test, as the Mac's `glAlphaFunc(GL_GREATER, 0.1666)`), or ADD/MASK
  would draw the key colour.
* d3d's tiles store the converted key colour with alpha cleared; NOLF's key is black for text surfaces (inferred).
* **Safest: glAlphaTest(GREATER, 0) plus RGB = 0 at key pixels.**

**+0xa8 colour:** HLTCOLOR 0x00RRGGBB → modulate RGB by (R,G,B)/255. Per-draw alpha = `m_fAlpha` from the request.
* The Mac applies the colour only inside Optimized2D (r_DrawImage).
* d3d applies it only on its textured (Optimized2D) path.

### 4.5 Screen lock (+0xe4/+0xe8) and BlitFromScreen (+0xfc)

**`LTBOOL LockScreen(int l, int t, int r, int b, void** ppBits, uint32* pPitch)`**
* The engine **always passes (0, 0, W, H)**: every site in cissurf.cpp, diblit.cpp, fntdraw.cpp and warpblit.cpp.
* It expects `*ppBits` = pixel (0,0) of a **top-down** buffer in the **screen PFormat**, with `*pPitch` in **bytes**.
* Nonzero = success. The engine sets `*pPitch = 0` before the call.
* **Callers.** Any software 2D operation whose destination (or source) is the screen surface:
  * `FillRect(screen, …)`: E/warpblit.cpp:888. The game does this for screen shading and fades
    (InterfaceMgr.cpp:1141, 1326) and meters (InterfaceMeter.cpp:120-140);
  * `DrawSurfaceSolidColor` / `DrawSurfaceMasked` / `ScaleSurfaceToSurfaceSolidColor` / `WarpSurfaceToSurface*` onto the screen;
  * `DrawBitmapToSurface(screen,…)`;
  * font text drawn straight to the screen (fntdraw.cpp:618);
  * ILTClient::LockSurface/GetPixel/SetPixel on the screen;
  * every DrawSurfaceToSurface* when +0xec/+0xfc is NULL.
* It reads and modifies pixels, so the buffer must hold the **current** back-buffer contents.
* **d3d (0x100369d0):** DirectDraw Lock of the back buffer (DDLOCK_WAIT). FALSE if there is no buffer, it is already
  locked, or the lock failed.
* **Mac:** returns 0. All of the above silently fail on the Mac; the cis_ helpers return LT_ERROR.
* **For GL:**
  1. `glFinish`; `glReadPixels(0,0,W,H)` from GL_BACK into a CPU buffer, in the screen format
     (32-bit: GL_BGRA/GL_UNSIGNED_BYTE; 16-bit: GL_RGB/GL_UNSIGNED_SHORT_5_6_5);
  2. flip the rows (GL is bottom-up);
  3. return that buffer.

  This is expensive (a full read-back per lock), but correct. Optimisations:
  * the modern `FillScreenRect` hook (§2 +0xb8) removes the most frequent caller;
  * reading back only on the first lock per frame is unsafe unless nothing else drew in between (inferred).

**`void UnlockScreen(void)`**
* No arguments; pairs every successful LockScreen.
* d3d unlocks and adds the rect to the dirty list.
* **For GL:** write the whole buffer back. Use glDrawPixels, or (faster) upload to a W×H texture and draw a quad, with
  blend, depth and alpha test off and an identity colour. Restore state.

**`void BlitFromScreen(RenderBlitRequest*)`**
* **Called** only from cis_DoDrawSurfaceToSurface when the **source is the screen**, the destination an ordinary
  surface, and the operation is a plain or keyed copy (E/cissurf.cpp:1162-1176). The answer is ignored.
* Request: `m_pSurfaceData` = destination surface; `m_pSrcRect` = screen rect; `m_pDestRect` = surface rect, same size.
* Key in m_TransColor when m_bTransparent; it applies to the **screen** pixels: copy only where src != key.
  `m_fAlpha` = 1.0 (not set). The engine does not mark the surface dirty, so the renderer must.
* **d3d (0x10036ae0):** EndScene; `Blt(surface ← back buffer)` **without** honouring the key; BeginScene.
* **Mac:** does nothing.
* The game has no caller (no `DrawSurfaceToSurface*(…, screen, …)` in src/game). A minimal implementation:
  glReadPixels of the rect, converted to the screen format, into the surface pixels, then mark it dirty.

### 4.6 Surfaces across a renderer restart

* **On term,** surface_SaveAll copies each surface's pixels out (LockSurface, a `w*bpp` row copy using CSurface's pitch,
  UnlockSurface, DeleteSurface).
* **On init,** surface_CopyIn 0x0040ef30 (E/cissurf.cpp:1824-1866):
  1. CreateSurface(w,h); GetSurfaceInfo (stores the new pitch);
  2. LockSurface; the engine's converter copies from the **old** screen format to the **new** one; UnlockSurface;
  3. if the surface was optimized: OptimizeSurface(data, m_hTransColor).

So a 16↔32-bit mode switch converts surfaces automatically. Nothing is needed from the renderer beyond working
CreateSurface/GetSurfaceInfo/Lock/Unlock from the moment Init returns.

### 4.7 How the game's 2D reaches the renderer: summary

| path | game call | engine route |
|---|---|---|
| bitmaps | `CreateSurfaceFromBitmap(pcx)` ×69 | engine decodes the PCX to an 8-bit DIB, CreateSurface, Lock, blit 8P→screen format, Unlock |
| fonts | engine GDI font module (fntdraw.cpp) | renders text with GDI into a DIB, then converts into a surface via Lock/Unlock. Game bitmap fonts are surfaces |
| screen draws (bitmaps, HUD, menus, fonts) | `DrawSurfaceToSurface[Transparent]` ×85, `ScaleSurfaceToSurface[Transparent]` ×57 | **+0xec BlitToScreen** |
| tinting and shadows | `SetOptimized2DBlend` / `SetOptimized2DColor` | +0xa0 / +0xa8, then +0xec |
| fades and letterbox | `SetSurfaceAlpha` | carried as `m_fAlpha` in +0xec |
| fills | `FillRect(screen)` | **+0xe4/+0xe8** (or the modern FillScreenRect hook) |
| console | engine console (CConsole, started with the RenderStruct and the top-half rect, E/clientrender.cpp:556-568) | draws through the same surface API (inferred) |

---------------------------------------------------------------------------------------------------

## 5. Textures: BindTexture / UnbindTexture and the engine callbacks

**`void BindTexture(SharedTexture* t, LTBOOL bChanged)`**

d3d (0x10039fe0, D/w6_10039730_qip.cpp:675) and the Mac (rgl_BindTexture [0x182330]) agree:

* **If `t->m_pRenderData` (+0x0c) is NULL:**
  1. `TextureData* d = g_pStruct->GetTexture(t)` (callback +0x08). It returns `m_pEngineData`, loading the DTX from
     disk if it is NULL. NULL means the load failed.
  2. Create the GL texture from `d` and store your object in `t->m_pRenderData`.
  3. **Always** call `g_pStruct->FreeTexture(t)` (callback +0x0c) afterwards. It frees the engine's system copy
     (unless TDFLAG_TEXMOD). Otherwise the engine keeps every texture twice.
* **Else if bChanged:** GetTexture again, re-upload into the existing object, then FreeTexture.
* **Else:** nothing.
* `bChanged` = "the contents changed, upload again" (TRUE only from LTTexMod and video).

**Upload details:**
* Choose the first mip from `MipmapOffset + GroupOffset[group] + Extra[3]`, clamped (Mac r_SetupTextureSurface [0x180590]).
* Mip count from Extra[1] (0 → 4).
* Generate the missing small levels or set GL_TEXTURE_MAX_LEVEL.
* Wrap = REPEAT.
* Filters from Bilinear/Trilinear; anisotropy from Anisotropic.
* Honour "32BitTextures" (the 16-bit fallback formats are optional on GL).

**`void UnbindTexture(SharedTexture* t)`:** delete your texture and set `t->m_pRenderData = NULL`.
d3d also subtracts the size from RenderStruct+0x50.

**Callbacks the texture path uses** (E/clientrender.cpp:861-890):
* `SharedTexture* (*+0x04)(const char* filename)`: resolve or create a SharedTexture by name.
  The engine uses file type 0 here (CClientFileRef); used for canvas, detail and env textures.
* `TextureData* (*+0x08)(SharedTexture*)`: the engine's 0x00462270 takes one argument. The Mac declares a second
  `int*`, which is ignored; pushing an extra dword in cdecl is harmless.
* `void (*+0x0c)(SharedTexture*)`: release the engine data unless flags & 0x40. It does **not** clear anything renderer-side.

---------------------------------------------------------------------------------------------------

## 6. Engine callbacks: RenderStruct part 1

Installed by r_InitRenderStruct 0x00462110 (E/clientrender.cpp:801-833). The Mac names are from Mac [0xec6e0].
All are cdecl.

| off | signature | what | used by d3d / Mac |
|---|---|---|---|
| +0x00 | `LTObject* ProcessAttachment(LTObject*, Attachment*)` | positions an attached object, returns the child | 3D (model attachments) |
| +0x04 | `SharedTexture* GetSharedTexture(const char* name)` | load or resolve a texture by file name | canvases, detail and env maps |
| +0x08 | `TextureData* GetTexture(SharedTexture*)` | engine copy, loaded if needed | BindTexture |
| +0x0c | `void FreeTexture(SharedTexture*)` | drop the engine copy | BindTexture |
| +0x10 | `void RunConsoleString(const char*)` | **executes** a console line (con_RunString 0x0041abe0). Used to create a missing cvar: `"Name value"` | cvar registration |
| +0x14 | `void ConsolePrintf(const char* fmt, ...)` | formatted print to the console (300-byte buffer) | all logging |
| +0x18 | `HCONSOLEVAR GetParameter(const char* name)` | find a cvar; **NULL if it does not exist** | §8 |
| +0x1c | `float GetParameterValueFloat(HCONSOLEVAR)` | float value; 0.0 for NULL | §8 |
| +0x20 | `const char* GetParameterValueString(HCONSOLEVAR)` | string value; NULL for NULL | §8 |
| +0x24 | `void Ping(void)` | empty function | d3d calls it between phases |
| +0x28 / +0x2c | `void(void)` / `?(void)` | thiscall slots 0/1 of an object at CClientMgr+0x1ec. Mac names: IncObjectFrameCode / GetObjectFrameCode | 3D (object frame code) |
| +0x30 | `uint16 IncCurTextureFrameCode(void)` | CClientMgr::NextFrameCode 0x00411450 | 3D (texture stamping) |
| +0x34 | `void* Alloc(uint32)` | engine dalloc | d3d: yes |
| +0x38 | `void Free(void*)` | engine dfree | d3d: yes |

The DLL may use its own CRT for its own memory. Use +0x34/+0x38 only for memory the engine frees or allocated.
(Nothing in this contract requires that: surfaces, contexts and mode lists are all freed by the DLL itself.)

---------------------------------------------------------------------------------------------------

## 7. Which .ren and which mode

### 7.1 Exports and resources

* **d3d.ren is linked with** `/export:RenderDLLSetup /export:GetSupportedModes /export:FreeModeList`.
  There is no .def file (build/build.mk:563-568).
* **Retail d3d.ren** (ext/rez/games/nolf-eu-goty/d3d.ren = out/reference/d3d.ren):
  * exports exactly those three undecorated names (ordinals 1 FreeModeList, 2 GetSupportedModes, 3 RenderDLLSetup);
  * one resource: an RT_STRING table, language 0x409, **string 5 = "LithTech Direct3D Renderer"**.

  The reconstructed d3d.ren lacks the .rsrc.
* **opengl.ren needs:**
  * the same three `extern "C" __cdecl` exports;
  * a string table entry 5 (e.g. "LithTech OpenGL Renderer").

  The **launcher** reads that string with `LoadString(hModule, 5, …)` for its list ("file (desc)").
  The engine never reads it (dlgdisplay.cpp `CRenderer::Init` 0x402220, lines 329-400).

### 7.2 GetSupportedModes / FreeModeList

* `RMode* GetSupportedModes(void)`: a NULL-terminated singly linked list through m_pNext.
* `void FreeModeList(RMode* head)`: frees it. It receives **exactly the head pointer GetSupportedModes returned**.
  Use your own allocator for both:
  * d3d mallocs one node per mode and frees each with the CRT (w26_10020fd0_osoiqip.cpp:412-460);
  * the Mac mallocs one block of six and frees the block (Mac glGetSupportedModes [0x14c800]).

**Engine side:** ILTClient::GetRenderModes = ci_GetRenderModes 0x0042de00 (E/clientinterface.cpp:1734-1760), with
the per-DLL worker 0x0042de60 (E/clientdsi.cpp:214-249).
1. `_findfirst("*.ren")` in the **current directory**.
2. For each file: LoadLibrary, look up both exports (skip the DLL if either is missing), GetSupportedModes.
3. Copy every node into an engine `dalloc(0x214)` and **overwrite m_RenderDLL with the file name**.
4. Push the copies onto one combined list (this reverses order). Then FreeModeList(head) and FreeLibrary.

So the DLL's m_RenderDLL contents are irrelevant, and **GetSupportedModes must work without Init**: it is called with
no device or context up. Keep it free of global side effects; DllMain should do nothing heavy.

**Fields that matter:**
* `m_bHardware = 1` (the launcher uses it to choose defaults).
* `m_InternalName` **non-empty and without spaces**. The launcher drops cards with an empty name or description
  (dlgdisplay.cpp `AddCard` 0x4024d0) and writes `++CardDesc <InternalName>` unquoted.
* `m_Description` non-empty.
* `m_Width/m_Height/m_BitDepth` (16 and/or 32).

The engine never filters on m_bHardware or the bit depth.

**Who filters the list:**
* The game's display menu (FolderDisplay.cpp:264-331) keeps ≥640x480 and in retail **only 4:3** modes.
  The modern build drops 16-bit modes instead.
* The launcher keeps ≥640x480 and 4:3 (retail).

**Suggestion:** enumerate `EnumDisplaySettings` modes at 16 and 32 bpp (deduplicated), with InternalName "OpenGL"
and Description "OpenGL". The Mac uses "opengl"/"OpenGL" and a fixed 6-mode list.

### 7.3 Choosing the DLL and mode at startup

**Mode request.** r_GetRequestedMode 0x0040f810 (E/clientmgr2.cpp:2629-2645; Mac `_GetRModeFromConsoleVariables`)
builds the request from console variables, with everything else zero (m_bHardware = 0, Description empty):

| cvar | field | default |
|---|---|---|
| `RenderDLL` | m_RenderDLL | **"d3d.ren"** |
| `CardDesc` | m_InternalName | empty |
| `ScreenWidth` | m_Width | 640 |
| `ScreenHeight` | m_Height | 480 |
| `BitDepth` | m_BitDepth | 16 |

The defaults come from src/engine/server/dsi.cpp:134-136 and 250-253. Variable names are case-insensitive.

**Where the cvars come from:**
* autoexec.cfg (`-config <file>`, default autoexec.cfg; E/clientwin.cpp:940-942);
* `+Name value` command-line arguments (cmgrinit.cpp:259-273);
* the launcher's `-cmdfile nolfcmds.txt`.

**Startup flow:**
1. CClientMgr::Init passes the mode to `IClientShell::OnEngineInitialized(&mode)` (E/clientmgr3.cpp:1685).
2. **The game** calls SetRenderMode (GameClientShell.cpp:1057-1108). The fallbacks are:
   * the same DLL and card at 640x480x16 (modern build: 800x600x32);
   * then **"d3d.ren"** with an empty card;
   * then shutdown.

So a broken opengl.ren falls back to d3d.ren if one is present.

**`ILTClient::SetRenderMode`** = ci_SetRenderMode 0x0042df60 (E/clientdsi.cpp:384-410):
* r_TermRender; r_InitRender(new).
* On failure, r_InitRender(old) → `LT_KEPTSAMEMODE`, or `LT_UNABLETORESTOREVIDEO` if that also fails.

**After SetRenderMode,** the display menu runs `+RenderDll`, `+CardDesc`, `+screenwidth`, `+screenheight` and
`+bitdepth` (FolderDisplay.cpp:404-443). These persist to autoexec.cfg at exit.

**`windowed`** is read **only by the render DLL** (d3d Init, Mac r_Init): `GetVarValueString` + `atoi == 1`.

**The launcher (NOLF.exe):**
* scans `*.REN` (dlgdisplay.cpp `FindRenderers` 0x402c10);
* loads each DLL, reads string 5, enumerates modes;
* reads the `RenderDll`/`Renderer`/`CardDesc`/`ScreenWidth`/`ScreenHeight`/`BitDepth` lines of autoexec.cfg;
* starts `lithtech.exe -cmdfile nolfcmds.txt` with `++RenderDll <file> ++CardDesc <InternalName>
  ++Renderer "<Description>" ++screenwidth … ++bitdepth …` (dlgdisplay.cpp `GetCommands` 0x4033d0, lines 1146-1183;
  launcher.cpp:528-535).

**To use opengl.ren:** put it next to lithtech.exe (the current directory) and set `RenderDLL opengl.ren` (plus
`CardDesc OpenGL`), or pick it in the launcher or the display menu.

### 7.4 Keeping the current mode recognisable

* The game highlights the current mode by comparing **RenderDLL, InternalName, Description (case-insensitive), width,
  height and bit depth** of `GetRenderMode` against list entries (FolderDisplay.cpp:476-510; GameSettings.cpp:115-176).
* GetRenderMode returns `g_CurRenderMode`, i.e. `init.m_Mode` after your Init.

So **Init must write back the same m_InternalName and m_Description that GetSupportedModes reports.** This matters
especially when CardDesc was empty, as it is on the d3d fallback path. d3d copies the device name and description back
for exactly this reason.

Leave `m_RenderDLL` alone: the engine set it to the file name.

---------------------------------------------------------------------------------------------------

## 8. Console variables the renderer reads

**Mechanism** (d3d 0x10021dc0 and 0x10021e50, D/w34_10021dc0_ossf.cpp:69, D/w15_10021e50_os.cpp:64;
Mac `r_MaybeCreateCVar` [0x14e450] and `r_ReadConsoleVariables` [0x14e5a0]):
1. For each variable: `h = GetParameter(name)` (+0x18).
2. If NULL, `RunConsoleString("Name default")` (+0x10; the Mac uses `"%s %f"`), then GetParameter again.
3. On ReadConsoleVariables: `value = GetParameterValueFloat(h)` (+0x1c). Store it as a float and as an int.
4. Then apply the derived device state (filters, fog, dither, LOD bias).

The variable handle stays valid for the process (inferred: the client console keeps variables). Re-looking up by name
on each read is also fine.

**Read directly in Init:**
* `windowed`: string via +0x20, atoi == 1 (both).
* d3d also reads `SysMem` and `nullrender` (floats), and `ForceMode` (string) in device setup.

**The shared table.** These have the same names and defaults in d3d (D/big21f90.cpp:462) and the Mac
(`__sinit_common_stuff_cpp` [0x14e6d0]); this is the set a GL port should register.

The ones that matter for the device/2D half are in **bold**:

| group | variables (name = default) |
|---|---|
| debug and limits | TableFog 0, NoLMPages 0, ModelProfile 0, MaxModelLights 4, MaxTexAspectRatio 8, **32BitTextures 0**, **MaxTextureSize 16384** |
| draw | DrawGuns 1, TintModels 1, Saturate 0 (Mac 1), **Bilinear 1**, EnvMapEnable 0, FixTJunc 0, LightSaturate 1, **FilterOptimized 0** (filter 2D blits), **OptimizeSurfaces 0** (d3d's OptimizeSurface refuses while 0; a GL renderer should ignore it), ShowSkySplits 0, WarbleSpeed 25, WarbleScale 0.92, EnvMapAll 0, EnvPanSpeed 0.0005, EnvScale 1, DrawWorld 1, ShadowZRange 17, SkyScale 1, **Gamma 100**, **GroupOffset0..9 0**, TripleBuffer 0 |
| device and passes | UseDX6Commands 1, InvertHack 0, Force1Pass 0, LightAddPoly 1, ModelWarble 0, MMXRast/RGBRast/RefRast/TnLRast 0, Force2Pass 0 |
| fog | FogEnable 0, FogR/G/B 255, FogNearZ 0, FogFarZ 2000, SkyFogNearZ 0, SkyFogFarZ 2000 |
| misc | **MipmapOffset 0**, LockPVS 0, ShowFullbriteModels 0, BumpMap 0, DrawSky 1, TextureModels 1, ShowFillInfo 0, DrawAll 0, DrawSprites 1, DrawPolyGrids 1, DrawParticles 1, DrawModels 1, DrawLineSystems 1, ShowSplits 0, DynamicLight 1, FastLight 0, LightModels 1, ModelFullbrite 1, ShowTextureCounts 0, ShadowLodOffset 100, MaxModelShadows 1, LodScale 1, LodOffset 0, Wireframe 0, ModelBoxes 0, **RenderDebug 0** (log threshold), ShowPolyCounts 0, LightMap 1, DrawFlat 0, LightmapsOnly 0, **Dither 1** |

**d3d-only (registered in other files; D/ grep `CD3DConVar`):**
* **S3TCEnable 1, Trilinear 0, Anisotropic 0**, MipMapBias 0, UseD3DClip 0.
* **AlphaTest 1** (gates the DTX "ColorKey" command), RenderToFront 0.
* DrawPortals 1, PortalLightmap 1, ShowPortalBounds 0, ShowTexInfo 0, DrawTerrainSections 0, DrawWorldTree -1.
* VFog\* (VFog 0, VFogDensity 1800, VFogMax 180, VFogMaxYVal 0, VFogMinYVal 0.5, VFogMaxY 1300, VFogMinY 0).
* DetailTextures 1, DetailTextureAdd 1, DetailTextureAngle 0, DetailTextureScale 0.2, FixSparkleys 0, LMFullBright 0, EnvMapWorld 1, EnvMapPolyGrids 1.
* LMAnim 1, LMAnimStatic 0, LMDynamic 1, LMDynamicScale 2.5, LMDynamicSize 16.
* Model\*:
  * ModelShadowAlpha 230, ModelShadowOffset 0.05, ModelShadowProjRes 128, ModelShadowProjLOD 1, ModelShadowProjShow 0;
  * ModelLODOffset 0, ModelLODBlendEnable 1, ModelLODBlendDist 60;
  * ModelSunVariance 0.05, ModelApplySun 1, ModelSaturation 2, ModelCacheRigid 0, ModelUseTnL 0;
  * ModelSpecular, ModelTexture, ModelShadowProj, ModelFovTest (defaults not listed).
* LightModelSprites 0, DrawModelsRigid 1, ExtraFOVXOffset/ExtraFOVYOffset 0, SortProfile 0, DrawSorted 1.
* PSSrcBlend/PSDestBlend -1, DrawCanvases 1, DrawWorldModels 1.
* AllSkyPortals 0, PortalFlow/PortalGraph/PortalsOnly/BlockersOnly 0, DrawPolyMgr 0, TestGouraud 0, TestLightmap 1.

**Mac-only:**
* RenderID 0, **DepthBufferBits 0** (0 means 32), LodBias -2, Anisotropic 1, ATI_FSAA_Samples 1.
* FlickerFix 0, NoPackedPixels 0, NoFogCoord 0, Trilinear 0, S3TCEnable 0.
* DisableEnvCombine 0, DisableMultitexture 0, ForceTile 1 (always tile 2D surfaces at 64x64).

**Variables the game or the modern build sets:**
* `Bilinear`, `Trilinear`, `Anisotropic`: Advanced Display (MODERN.md M-P1-09).
* `optimizesurfaces 1`: forced at startup by the modern client (src/modern/cshell/p1_config.cpp:94).
* `windowed`: `++windowed 1` from linux-run.sh in the release build, or from the game's Display options.
* `MaxModelShadows`: the game, from DrawShadows.

---------------------------------------------------------------------------------------------------

## 9. Implementation checklist and pitfalls

1. **DllMain/exports:**
   * no GL work in GetSupportedModes;
   * the `RenderDLLSetup` store order is irrelevant;
   * keep the RenderStruct pointer in a global (both renderers do).
2. **Init:**
   * write 0xd5d first;
   * read `windowed`;
   * size or style the engine's HWND for the mode, and change the display mode for fullscreen
     (ChangeDisplaySettings; restore it in Term);
   * create the WGL context with depth 24/32 (+ stencil 8 for d3d's ClearScreen/stencil users);
   * copy InternalName/Description back;
   * write +0x3c/+0x40;
   * set up the state that the 2D path needs: ortho helpers, an alpha test.

   On failure, clean up and return nonzero; the engine FreeLibrary's without calling Term.
3. **Term must leave the window usable** for the next Init. Focus changes reload the DLL.
4. **Surfaces:** CPU memory in the screen format, pitch in bytes, dirty on every LockSurface.
   OptimizeSurface returns TRUE. Re-upload on draw when dirty or when the key or transparency changes.
5. **BlitToScreen must work:**
   * inside and outside 3D, and inside and outside Optimized2D;
   * stretching;
   * keying on the request's screen-format colour;
   * modulating by `m_fAlpha` (always) and the Optimized2D colour (inside the bracket);
   * blending with the current Optimized2D blend (inside) or alpha blend (outside).

   Use point sampling for 1:1.
6. **LockScreen/UnlockScreen are needed** (FillRect on the screen, software fallbacks). Implement them by read-back and
   write-back in the screen format, top-down. Optionally also export the modern `FillScreenRect` hook.
7. **GetHook returns NULL** for everything except optional new hooks. Never return fake DirectDraw objects.
8. **The screen format must be type 2 or 3** with correct masks, bits and shifts. Prefer X8R8G8B8 (alpha mask 0) at
   32 bpp and 565 at 16 bpp. Report the format matching `m_BitDepth`, since d3d's surfaces follow the display depth.
9. **Textures:**
   * BindTexture → GetTexture(+0x08) → upload (BGRA8888, or S3TC) → FreeTexture(+0x0c);
   * store your object in SharedTexture+0x0c and NULL it in UnbindTexture;
   * clamp the mip count to 8.
10. **MakeScreenShot** must be callable at any time (F8) and write a .bmp.
11. **RenderScene while Optimized2D is open** must be refused (both renderers do).
    **End3D** should close an open Optimized2D bracket.
12. **Known engine quirks to tolerate:**
    * BlitToScreen's result is ignored;
    * m_TransColor is meaningful only when m_bTransparent;
    * rects are exclusive on right/bottom except in the WarpToScreen request (inclusive w-1/h-1);
    * DrawSurfaceMasked only works at 16 bpp (engine limitation).

**Open points (not resolved by this research):**
* The exact meaning of the RenderContextInit first dword (CClientMgr* per the PC call site; "world" per the Mac reading).
* The callback +0x28/+0x2c semantics. Both are 3D-half concerns.
