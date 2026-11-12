# World rendering spec for opengl.ren (NOLF / LithTech, RenderStruct version 0xd5d)

This file covers everything a new OpenGL render DLL needs to draw the WORLD: the static BSP
geometry, world models (moving brushes) and the sky, using the data the engine hands it. It is
self-contained. Models, sprites, particles, poly grids, line systems and canvases are out of
scope; only their place in the draw order is given.

Sources:
- **Engine reconstruction.** `src/engine/lithtech/*.cpp`, mainly `clientrender.cpp` (RenderStruct)
  and `clientmgr2.cpp` (SceneDesc, CreateRenderWorld).
- **Retail PC renderer.** d3d.ren, reconstructed in `src/engine/d3d/*.cpp`.
- **Mac renderer.** The Mac build of the same engine version
  (`ext/rez/games/nolf-mac-usa/NOLF/LithTech`, PowerPC, names preserved, OpenGL renderer linked in).

Address conventions:
- `PC 0x004xxxxx` is lithtech.exe.
- `PC 0x10xxxxxx` is d3d.ren.
- `Mac 0xNNNNNN` is a code-section offset in the Mac PEF.
- "inferred" marks a claim that was not read directly off an instruction.

**Struct offsets are the same on PC and Mac** for every structure cross-checked (SceneDesc,
RenderContext, MainWorld fields, WorldBsp, Node, Leaf, WorldPoly, Surface, SharedTexture,
TextureData, LTObject, VisibleSet, ViewParams). No alignment differences were found: every member
is 4 bytes or smaller and there are no doubles. The one thing that differs is C++ vtable shape.
CodeWarrior vtables have an 8-byte header, and the WorldBsp slot sets differ, so **read fields
directly; do not call engine virtuals beyond the ones named here**.

Organisation: a summary and implementation recipe (section 0), then the five detailed parts:
- **A.** SceneDesc, CreateWorld/RenderContext, camera and projection, frame flow, console variables.
- **B.** World data structures: MainWorld, WorldBsp, Node, Leaf, WorldPoly, Surface, textures,
  texture coordinates, surface flags.
- **C.** Lightmaps: storage, compression, pixel format, UVs, page packing, animation, dynamic lights.
- **D.** Visibility (PVS, world tree), world models, sky.
- **E.** Draw passes and GL state: single-pass and two-pass lightmapping, gouraud, flat,
  fullbright, detail, envmap, fog, vertex formats.

---------------------------------------------------------------------------

## 0.1 Tooling

The Mac evidence was read with `ext/tools/pef.py`. Two of its early gaps, both
fixed since, mattered while this was written: `dis` stopped at the first
instruction capstone cannot decode (`fcmpo`), and `funcs` skipped names with
template arguments (`r_FullSetupPoly3<...>`, `r_CalcFog_*`, `r_InitFrustrum2`).
A function's name is the traceback string that follows its last `blr`.

The Mac renderer's console variables (name, default) are listed in
`mac-cvars.txt` beside this file, read off the `__sinit_*` `ConVar`
constructors.

## 0.2 Reconciliation notes (where the parts disagreed or need care)

- **Frame-code reset.** `r_ResetWorldFrameCode` (Mac 0x14ef90) zeroes `Leaf+0x2c` (u16; Leaf is
  0x30 bytes, array at WorldBsp+0x88, count +0x8c). It also zeroes `WorldPoly+0x42` (u16; poly
  pointers at WorldBsp+0xa0, count +0xa4). Parts B and D have this right. Part A's first draft
  swapped "poly" and "leaf", and that has been corrected in A.2 below.
- **MainWorld+0x180 flags.**
  - **0x2** means the world has vis/PVS data. It is set by MainWorld::Load and gates the PVS path
    in r_TagVisibleLeaves.
  - **0x4** means the world has static lightmaps. `MainWorld::ReadLightmapData` sets it when
    `FindLMAnim("LightAnim_BASE")` succeeds (Mac 0xd9d2c..0xd9d34: `ori r0,r0,4; stw
    r0,0x180(r15)`, verified for this merge). `r_SetPolyFunctions` routes lightmapped polys to
    gouraud unless the bit is set.

  Part B's wording ("to disable lightmapping") should be read this way.
- **What CreateWorld receives.** The request's single field is the **MainWorld\***. clientmgr2.cpp
  names the CClientShell+0x78 object "ClientMgr", but the Mac `CClientShell::GetWorld` returns that
  same field. The returned context is stored at `MainWorld+0x1c8` and comes back every frame as
  `SceneDesc+0x34`.
- **The surface vtable entry that returns the lightmap P/Q vectors** is PC slot 0 and Mac
  vtable+0xc (after the 8-byte CodeWarrior header and the destructor). Part E's "vtbl[3]" means
  byte offset 0xc on the Mac. Do not call it from a new renderer: compute P/Q from the plane
  normal and the plane-type bits (C.3 / B.4.1).
- **PC vs Mac defaults that change the look.** The retail PC uses **two-pass** lightmapping with
  **`Saturate` = 0**, giving `base × lightmap` at 1x plus world detail textures and environment
  maps. The Mac defaults to single-pass multitexture with `Saturate` = 1 (2x vertex lighting), and
  draws no world detail or envmaps on that path. Match the PC (E.0).

## 0.3 One-page implementation recipe

```c
// ---- Init (RenderStruct +0x6c) ----
//   Write 0xd5d into RenderStructInit.m_nVersion. Register console variables via the engine
//   callbacks (A.5).

// ---- CreateWorld (+0x7c): RenderContext* (RenderContextInit* p)  (A.2) ----
//   ctx = calloc(0x10)-equivalent; ctx->m_pMainWorld = p->m_pMainWorld; ctx->frameCode = 0xffff;
//   Build lightmap atlases (C.10): for each ref of FindLMAnim("LightAnim_BASE") (MainWorld LMAnim
//   array +0x08/+0x0c, 0x60 stride, compare name) decode frame 0 (RLE 5:5:5), pack into pages,
//   compute per-vertex lightmap UVs (you may store them in WorldPolyVertex +0x0c/+0x10 and the
//   page pointer in WorldPoly +0x44 -- the renderer owns those fields), upload GL_LINEAR, no mips.
//   Return ctx (NULL = "Error binding world to renderer").
// ---- DestroyWorld (+0x80): free pages, clear WorldPoly +0x44, free ctx. ----

// ---- RenderScene (+0xb0): SceneDesc* d ----
//   if d->m_DrawMode == 2: draw only d->m_pObjectList (no world). Return.
//   world = ((RenderContext*)d->m_pRenderContext)->m_pMainWorld   (d+0x34 -> +0x08)
//   camera: pos d+0xc0, quat d+0xcc (x,y,z,w), fov d+0xb4/d+0xb8 (full angles, radians,
//           independent), near 7.0, far clamp(d+0xbc, 3, 500000), viewport d+0xa4 (A.3)
//   frameCode++ (wrap at 0xffff -> reset leaf/poly codes, start at 1)
//   visible polys: PVS walk from the camera leaf of the vis BSP (D.1.2) unless !(world+0x180&2)
//                  or DrawAll, in which case take all polys; dedupe by WorldPoly+0x42;
//                  skip SURF_INVISIBLE 0x4; SURF_SKY 0x10 -> sky list (never drawn)
//   sky: if sky list non-empty and d->m_nSkyObjects (d+0xa0) > 0: draw sky objects (d+0x9c)
//        in list order from skyPos (D.3.2) with the camera rotation, depth test/write OFF
//   world polys: back-face (dot(N,cam)-D <= 0.01 -> skip), sphere/frustum cull, then by surface:
//        no texture -> untextured; 0x80 -> base*lightmap (+detail/envmap, fullbright);
//        0x8000 -> base*vertex colour*pan texture; else base*vertex colour (E.1, E.3-E.5, E.9)
//   world models (OT_WORLDMODEL objects from the world tree vis query or object lists):
//        solid/chromakey: all polys under m_Transform (LTObject+0x1f0), same pipeline as the world;
//        translucent (first surface flag 0x8): after opaque, far->near, BSP back-to-front, vertex
//        colour × object tint, alpha = object alpha, no lightmap (D.2)
//   fog: linear on radial eye distance FogNearZ..FogFarZ, colour FogR/G/B, if FogEnable (A.5, E.7)
//   post: LightScale modulate quad (d+0x50), LightAdd additive quad (d+0x5c) (A.1)
```

---------------------------------------------------------------------------



# Part A — SceneDesc, the render context, frame flow, console variables

Scope: what the engine hands the renderer each frame (SceneDesc), what it hands
it per world (CreateWorld / RenderContextInit), how the shipped renderers turn
that into a camera and a draw order, and which console variables steer world
drawing. Other parts cover the BSP data (B), lightmaps (C), visibility / world
models / sky (D) and the per-poly passes (E).

Evidence notation: `PC 0x…` = address in lithtech.exe or d3d.ren; `Mac 0x…` =
code offset in ext/rez/games/nolf-mac-usa/NOLF/LithTech (PowerPC, OpenGL renderer
linked in), disassembled with ext/tools/pef.py. "inferred" marks anything not read
directly off an instruction.

---------------------------------------------------------------------------

## A.0 Tooling notes (read first if you re-check anything here)

* `ext/tools/pef.py` decodes every instruction and names template functions (see 0.1).

---------------------------------------------------------------------------

## A.1 SceneDesc

Built on the stack by the engine's RenderCamera (PC `cm_RenderCamera`
0x00411700, src/engine/lithtech/clientmgr2.cpp:1100-1290; Mac
`cm_Render__FP10CClientMgrP14CameraInstanceiPP8LTObjecti` 0x15c40, SceneDesc
at r1+0xb0) and passed by pointer to RenderStruct slot +0xb0 (PC d3d.ren
0x1002d1c0, Mac `r_RenderScene` 0x157560). The Mac builds the struct at
exactly the same offsets as the PC (every store in Mac 0x15d50..0x15f84 checked
below), so one definition serves both. Size 0xec bytes (the 30-entry sky list
follows it on the caller's frame but is not part of it).

```c
typedef void (*ModelHookFn)(ModelHookData* pData, void* pUser); // ltbasedefs.h:988

struct SceneDesc                          // 0xec bytes
{
/*0x00*/ int32       m_DrawMode;          // 1 = DRAWMODE_NORMAL, 2 = DRAWMODE_OBJECTLIST
/*0x04*/ uint32*     m_pTicks[6];         // engine counters; unused by both renderers
/*0x1c*/ LTVector    m_ModelAdd;          // console "ModelAdd"   (models only)
/*0x28*/ LTVector    m_ModelDirAdd;       // console "ModelDirAdd" (models only)
/*0x34*/ RenderContext* m_pRenderContext; // what CreateWorld returned; NULL = no world
/*0x38*/ LTVector    m_GlobalLight38;     // dead: always (0,0,0), never read (see below)
/*0x44*/ LTVector    m_GlobalLight44;     // dead: always (0,0,0), never read
/*0x50*/ LTVector    m_GlobalLightScale;  // ILTClient::SetGlobalLightScale; default (1,1,1)
/*0x5c*/ LTVector    m_GlobalLightAdd;    // camera's LightAdd (SetCameraLightAdd)
/*0x68*/ float       m_FrameTime;         // seconds since last frame
/*0x6c*/ SkyDef      m_SkyDef;            // 0x30 bytes, see below
/*0x9c*/ HLOCALOBJ*  m_SkyObjects;        // sky objects, in AddObjectToSky index order
/*0xa0*/ int32       m_nSkyObjects;
/*0xa4*/ LTRect      m_Rect;              // left, top, right, bottom (ints, pixels; right/bottom exclusive)
/*0xb4*/ float       m_xFov;              // full horizontal FOV, radians
/*0xb8*/ float       m_yFov;              // full vertical FOV, radians
/*0xbc*/ float       m_FarZ;              // console "FarZ" (engine-side variable)
/*0xc0*/ LTVector    m_Pos;               // camera position (world)
/*0xcc*/ LTRotation  m_Rotation;          // camera orientation, quaternion {x,y,z,w}
/*0xdc*/ HLOCALOBJ*  m_pObjectList;       // DRAWMODE_OBJECTLIST only
/*0xe0*/ int32       m_ObjectListSize;
/*0xe4*/ ModelHookFn m_ModelHookFn;       // ILTClient::SetModelHook
/*0xe8*/ void*       m_ModelHookUser;
};

struct SkyDef                             // ltbasetypes.h:125, 0x30 bytes
{
    LTVector m_Min, m_Max;                // sky box corners, in SKY-world coordinates
    LTVector m_ViewMin, m_ViewMax;        // the viewer's main-world position is
                                          // squashed into this box (tiny parallax)
};
```

Field-by-field evidence and semantics:

* **+0x00 m_DrawMode.** The caller's 3rd argument. Mac `ci_RenderCamera`
  (0x4380) calls `cm_Render(mgr, cam, 1, NULL, 0)` (0x4498/0x449c `li r5,1 ;
  li r6,0`); `ci_RenderObjects` (0x4500) calls `cm_Render(mgr, cam, 2, list,
  n)` (0x462c..0x4634). Renderer side: r_InitFrame tests `== 1` (Mac 0x151568,
  PC w4_1001eb90_os.cpp:278 `if (pScene->m_DrawMode == 1)`) to pick up the
  world; r_FullDrawScene tests `== 2` (Mac 0x1547ac) to draw only the list.
  Names DRAWMODE_NORMAL/OBJECTLIST are the LithTech names (inferred; not in
  this SDK).
* **+0x04..+0x18 m_pTicks.** Six pointers to engine .bss counters (PC
  0x004b15a4, …a0, …98, …9c, …94, …90 — clientmgr2.cpp:1263-1268; Mac
  data+0x21720/1c/14/18/10/0c — Mac 0x15ec0..0x15eec). Nothing in d3d.ren's
  reconstruction or in any Mac r_* function loads them; ignore them.
* **+0x1c / +0x28.** Copied from the engine globals g_ModelAdd (PC
  0x004b3228, set by console command "ModelAdd", ccclient.cpp:1070) and
  g_ModelDirAdd (0x004b3218, "ModelDirAdd"). Mac readers: only
  `ModelDraw::SetupModelLight` (reads +0x1c..+0x24). Not used for the world.
* **+0x34 m_pRenderContext.** PC: `m_pClientShell->GetClientMgr()->
  m_pRenderWorld` (clientmgr2.cpp:1234); Mac 0x15d5c..0x15d88:
  `CClientShell::GetWorld()` (returns CClientShell+0x78) then `+0x1c8`.
  So it is the MainWorld's render context (see A.2). The renderer finds the
  world through it: `pMainWorld = pContext->m_pMainWorld` (+0x08).
* **+0x38 / +0x44.** Copied from CClientMgr+0x75c and +0x750 (PC
  clientmgr2.cpp:1262/1269, Mac 0x15e30..0x15e5c). On the Mac those six
  floats are only ever written with 0.0 (`cm_Init` 0x143b8.., 0x145cc..) and
  nothing else in the image writes them, so they are always zero. r_InitFrame
  copies them to globals (Mac data+0x29824/+0x29818, PC 0x1008be98/0x1008be8c)
  that no other code reads (scan of the whole Mac image; the PC view names
  them "light colour"/"light dir", clientmgr2.cpp names them the other way
  round — moot). The REAL global light direction/colour (used for model
  lighting) live in RenderStruct +0x130 / +0x13c (clientrender.cpp header);
  r_RenderScene stores `-RenderStruct.m_GlobalLightDir` per frame (Mac
  0x1576ec..0x15770c, PC w28_100256d0_qipni.cpp `g_vNegLightDir`).
* **+0x50 m_GlobalLightScale.** CClientMgr+0x768 (clientmgr.h:159), reset to
  (1,1,1) by `CClientMgr::Update` (Mac 0x13a28) unless a flag is set. Used two
  ways: (a) r_InitFrame keeps it as float, `*255` and as bytes (Mac
  0x151628..0x15178c; PC w4_1001eb90_os.cpp:312-330) and r_RenderScene
  rebuilds the 3x256 colour ramps (`r_FillColorMap`, factor 2.0 if
  "Saturate" else 1.0) when it moves by >0.001 (Mac 0x157724..0x157810);
  (b) after the whole scene, `r_DrawLightScalePoly(&desc->m_GlobalLightScale)`
  (Mac 0x154800 → 0x154220): if any component is outside [0.99, 1.01] draw a
  full-viewport quad, no texture/fog/depth-write, `glBlendFunc(GL_DST_COLOR,
  GL_SRC_COLOR)` with colour = scale*0.5 — i.e. framebuffer *= scale.
* **+0x5c m_GlobalLightAdd.** Camera +0x1c4 (clientmgr.h:301 `m_LightAdd`).
  After the scene `r_DrawLightAddPoly(&desc->m_GlobalLightAdd)` (Mac 0x154808
  → 0x153f20): if console "LightAddPoly" (default 1) and any component >
  0.001, full-viewport quad, `glBlendFunc(GL_ONE, GL_ONE)`, colour = LightAdd
  (0..1 floats), no texture, no fog, depth mask off, depth test off.
* **+0x68 m_FrameTime.** CClientMgr+0x1258 (PC) / +0x1850 (Mac). Renderer use:
  `g_fWarbleTime += m_FrameTime * WarbleSpeed` at the end of RenderScene (Mac
  0x157bcc, PC w28_100256d0_qipni.cpp:5196). Not used for world geometry.
* **+0x6c m_SkyDef.** CClientMgr+0x11a0 copied by value, 12 dwords
  (clientblit.cpp:464; Mac memcpy 0x30 from mgr+0x1798 at 0x15f0c). Filled
  from the server's sky message (Mac `OnSkyDef`); read by the sky code (part D).
* **+0x9c / +0xa0 sky objects.** Thirty uint16 object ids at CClientMgr+0x1160
  (cchandler.cpp:483 `m_SkyObjects[30]`), each looked up and appended if it
  exists (clientmgr2.cpp:1277-1284; Mac 0x15f30..0x15f74, `cmplwi r23,0x1e`).
  Order = sky index given to AddObjectToSky. `r_DrawSky` only runs when
  `m_nSkyObjects > 0` (Mac 0x17717c).
* **+0xa4 m_Rect.** Camera rect, or (0,0,screenW,screenH) when the camera is
  full-screen (clientmgr2.cpp:1243-1255).
* **+0xb4 / +0xb8 FOV.** Full angles in radians, independent in X and Y (the
  game sets both; no aspect is derived from the rect). See A.3.
* **+0xbc m_FarZ.** Engine console variable "FarZ" (PC g_CV_FarZ 0x004a9f00).
* **+0xcc m_Rotation.** A plain 16-byte copy of camera+0x9c. (clientblit.cpp's
  "stored x, z, y, w" is only the store ORDER of a straight copy:
  +0x9c→+0xcc, +0xa0→+0xd0, +0xa4→+0xd4, +0xa8→+0xd8; the Mac copies in
  order, 0x15db0..0x15dcc.) LTRotation is `float m_Quat[4]` = x,y,z,w
  (ltrotation.h:67).
* **+0xdc / +0xe0.** The two extra RenderCamera parameters; only meaningful
  for DRAWMODE_OBJECTLIST: `r_ProcessObjectList(desc->m_pObjectList,
  desc->m_ObjectListSize)` (Mac 0x1547c4..0x1547cc).
* **+0xe4 / +0xe8.** CClientMgr+0x1524/+0x1528. Read only by
  `ModelDraw::CallModelHook` (Mac). Signature
  `void (*)(ModelHookData*, void*)`; ModelHookData = {HLOCALOBJ m_hObject;
  uint32 m_Flags; uint32 m_ObjectFlags; LTVector* m_LightAdd; LTVector*
  m_ObjectColor} (ltbasedefs.h:293). Models only.

Engine side after the call: CClientMgr frame counter++ and RenderStruct+0x14c
= 0 (PC clientmgr2.cpp:1290, Mac 0x15fc4). The call itself is skipped (scene
still built) when engine console "RenderEnable" is 0.

---------------------------------------------------------------------------

## A.2 CreateWorld / DestroyWorld and what the renderer keeps per world

```c
struct RenderContextInit { MainWorld* m_pMainWorld; };   // 4 bytes

struct RenderContext                     // 0x10 bytes, renderer-allocated, zeroed
{
/*0x00*/ void*      m_LMPages0;          // lightmap-page bookkeeping, owned by
/*0x04*/ void*      m_LMPages4;          //   r_CreateLMPages / r_DestroyLMPages (part C)
/*0x08*/ MainWorld* m_pMainWorld;        // = init->m_pMainWorld
/*0x0c*/ uint16     m_CurFrameCode;      // 0xffff = "never drawn"
};
```

* Engine side: `CClientShell::CreateRenderWorld` (PC 0x00414860,
  clientmgr2.cpp:1663; Mac `CClientShell::BindWorlds` 0x24c30): if the
  renderer is up and `world->+0x1c8` is NULL, build `{ world }` on the stack,
  call slot +0x7c, store the answer at `world+0x1c8`; NULL answer = failure
  ("Error binding world to renderer", LT_ERRORBINDINGWORLD). Unbind
  (PC 0x004148a0, Mac `UnbindWorlds` 0x24ce0) calls slot +0x80 with it and
  clears the field. NOTE: clientmgr2.cpp calls the object at CClientShell+0x78
  "m_pClientMgr"/CClientMgrView, but the Mac names it: `CClientShell::GetWorld`
  (0x25270) returns `+0x78`, i.e. it is the **MainWorld**; the request's one
  field is the MainWorld*, and MainWorld+0x1c8 holds the RenderContext*.
  Also called after a renderer (re)init (clientrender.cpp header).
* Renderer side, identical on both: PC `d3d_CreateWorld` 0x10033160
  (d3ddraw.cpp:241), Mac `r_CreateContext` 0x14a1f0:
  ```c
  RenderContext* CreateWorld(RenderContextInit* p) {
      RenderContext* c = alloc_zeroed(0x10);  if (!c) return NULL;
      c->m_CurFrameCode = 0xffff;  c->m_pMainWorld = p->m_pMainWorld;
      if (cvar NoLMPages) print("Warning: NoLMPages is TRUE");
      else if (g_bLMPagesAvailable && !r_CreateLMPages(c)) {
          AddDebugMessage(0, "Warning: unable to create lightmap pages.  Lightmapping disabled.");
          g_bLMPagesAvailable = 0;           // so the warning is printed once
      }
      return c;
  }
  ```
  `r_CreateLMPages` walks every leaf/poly of the world, packs lightmaps into
  pages and uploads them (Mac 0x19c3e0: r_ProcessLeaf/r_AddToLMPage/
  r_UpdatePolyLightmap, then glTexImage2D) — part C.
* DestroyWorld: PC 0x10032c00 `if (c) { TermRenderWorld(c) /*0x1006d130*/; free(c); }`;
  Mac `r_DeleteContext` 0x14a2f0 `if (c) { r_DestroyLMPages(c); dfree(c); }`.
  So the only per-world renderer state is the context plus its lightmap pages
  (and per-poly render data the LM code hangs on WorldPolys — part C).

How the renderer reaches the world at render time (Mac r_InitFrame 0x151560..,
PC w4_1001eb90_os.cpp:276-300):

```c
g_pSceneDesc = desc;
if (desc->m_DrawMode == 1) {
    RenderContext* c = desc->m_pRenderContext;  if (!c) return FALSE;
    g_pMainWorld = c->m_pMainWorld;   g_pContext = c;     // Mac data+0x29838 / +0x29840
    r_IncrementFrameCode(c);           // see below
    g_CurFrameCode = c->m_CurFrameCode;                    // Mac data+0x29834 (uint16)
} else { g_pMainWorld = NULL; g_pContext = NULL; }
g_ObjectFrameCode  = RenderStruct->IncObjectFrameCode();   // slot +0x28
g_TextureFrameCode = (uint16)RenderStruct->IncCurTextureFrameCode(); // slot +0x30
```

`r_IncrementFrameCode` (Mac 0x14f060): if `c->m_CurFrameCode == 0xffff`
(first frame, or wrap) set it to 1 and reset every BSP's frame codes —
for each world model `i < MainWorld+0x190`, `bsp = (MainWorld+0x18c)[i]->+4`
(WorldBsp*): zero `leaf.+0x2c` (uint16; Leaf records are 0x30 bytes at
`bsp+0x88`, count `bsp+0x8c`) and `poly->+0x42` (uint16; WorldPoly* array at
`bsp+0xa0`, count `bsp+0xa4`) (`r_ResetWorldFrameCode` 0x14ef90; corrected in
the merge, see 0.2); otherwise `++m_CurFrameCode`. MainWorld fields used here (layout is part B's): +0x6c
the world tree object, +0x180 world flags (bit 1 = 0x2: has vis/leaf data, used by
r_TagVisibleLeaves; bit 2 = 0x4: has static lightmaps "LightAnim_BASE", used by r_SetPolyFunctions),
+0x18c `WorldData** ` world models (index 0 = the main BSP world, per
LithTech convention — inferred), +0x190 their count.

Object lists the renderer draws:
* DRAWMODE_NORMAL: objects are found by the visibility walk of the world tree
  (`r_VLTagPolies` passes `r_ProcessLeaf`, `r_ProcessObject`,
  `r_AddRenderObjects`, `r_NodeFilter`, `r_IsPortalVisible` to the tree's vis
  query — Mac 0x16f998..0x16f9e0; part D).
* DRAWMODE_OBJECTLIST: `r_ProcessObjectList(list, n)` (Mac 0x153e40) — no
  world, no visibility, just each object.
* Both dispatch per object through a module table indexed by
  `LTObject::m_ObjectType` (signed byte at +0xbc), 11 entries of 0x14 bytes
  at Mac data+0x2045c: `{Init, Term, ?, PreFrame, Process}`:
  1 OT_MODEL r_ProcessModel, 2 OT_WORLDMODEL r_ProcessWorldModel,
  3 OT_SPRITE r_ProcessSprite, 4 OT_LIGHT r_ProcessLight,
  6 OT_PARTICLESYSTEM r_ProcessParticles, 7 OT_POLYGRID r_ProcessPolyGrid,
  8 OT_LINESYSTEM r_ProcessLineSystem, 9 OT_CONTAINER r_ProcessWorldModel,
  10 OT_CANVAS r_ProcessCanvas; 0 (normal) and 5 (camera) have none.
  `r_InitObjectQueues` calls every PreFrame; `r_ReallyProcessObject` calls
  Process, then `r_ProcessAttachments` if `obj+0x98` (attachments) is set.
  Visibility filter in r_ProcessObject / r_AddRenderObjects (Mac 0x16e4b0,
  0x16e600): has a Process fn, type != 7 (poly grids come in through leaves),
  `!(obj+0x8c & 0x40)`, and `obj+0x88 & 1` (FLAG_VISIBLE) — with extra rules
  when drawing through a portal (ViewParams+0x4d4 set).

---------------------------------------------------------------------------

## A.3 Camera, ViewParams and projection

The renderer's per-frame camera is one global "ViewParams" (Mac data+0x2920c,
PC 0x1008b880, 0x4f8 bytes). Its layout is the same on both: Mac
`r_SetupFastProjection` (0x14fba0) copies +0x19c,+0x1a4,+0x1b0,+0x1b4,+0x1c4,
+0x1c8 to +0x31c..+0x330, byte-for-byte PC sub_1001e8d0 (big1ef30.cpp:811).
Full PC layout: src/engine/d3d/big1ef30.cpp:696-760 (struct D3DXForm). The
fields that matter:

```c
struct ViewBoxDef { float offX, offY, focal, halfW, halfH, nearZ, farZ; }; // 0x1c ("D3DLens")

struct ViewParams                         // 0x4f8
{
/*0x000*/ ViewBoxDef m_Lens;
/*0x01c*/ int32  m_nLeft, m_nTop, m_nRight, m_nBottom;   // rounded rect
/*0x02c*/ float  m_fLeft, m_fRight, m_fTop, m_fBottom;   // right/bottom = n-1
/*0x04c*/ float  m_fFovX, m_fFovY;
/*0x054*/ float  m_fW, m_fH, m_fHalfW, m_fHalfH, m_fInvHalfW, m_fInvHalfH,
                 m_fCentreX, m_fCentreY, m_fSx, m_fSy, m_fInvSx, m_fInvSy, m_fSxSy;
/*0x088*/ float  m_fNear, m_fFar, m_fFar2, m_fAspX, m_fAspY;
/*0x09c*/ LTMatrix m_MatCam;      // camera -> world (rotation | position)
/*0x0dc*/ LTMatrix m_MatWorld;    // world -> camera (scale * R^T * T(-pos))
/*0x11c*/ LTMatrix m_Mat11c;      // camera -> homogeneous clip (projection)
/*0x15c*/ LTMatrix m_Mat15c, m_Mat19c /*screen*depth*/, m_Mat1dc /*world->screen*/,
                   m_Mat21c, m_Mat25c /*off-centre skew*/, m_Mat29c, m_Mat2dc;
/*0x31c*/ float  m_FastProj[6];   // Mat19c [0][0],[0][2],[1][1],[1][2],[2][2],[2][3]
/*0x334*/ LTVector m_Corners[5];  // eye + 4 far corners, world space
/*0x370*/ LTPlane m_CamPlanes[6]; /*0x3d0*/ LTPlane m_WorldPlanes[6]; /*0x430*/ LTPlane m_ClipPlanes[6];
/*0x490*/ LTVector m_Up; /*0x49c*/ LTVector m_Right; /*0x4a8*/ LTVector m_Forward; // camera basis
/*0x4b4*/ LTVector m_Pos;         // camera position
/*0x4c0*/ LTVector m_SkyCamPos;   // used by r_DrawSky as the sky camera translation (part D; inferred name)
/*0x4d4*/ uint32   m_bAltVisPos;  // nonzero: vis uses m_AltVisPos instead of m_Pos (portal views)
/*0x4d8*/ LTVector m_AltVisPos;
/*0x4e4*/ LTVector m_EyePos;      // fog / LOD distance origin (PC sub_1001e360)
};
```

Per-frame setup (Mac r_InitFrame tail 0x1517d4..0x151850, PC
w4_1001eb90_os.cpp:398-405 — identical constants):

```c
r_InitFrustrum(&g_ViewParams, desc->m_xFov, desc->m_yFov, /*nearZ*/ 7.0f, desc->m_FarZ,
               (float)rect.left, (float)rect.top,
               (float)rect.right - 0.1f, (float)rect.bottom - 0.1f,
               &desc->m_Pos, &desc->m_Rotation);
```

* `r_InitFrustrum` (Mac 0x14fa70): `quat_ConvertToMatrix(rot, M)` (Mac 0x100),
  `M[0][3],M[1][3],M[2][3] = pos` (stores at frame 0x58/0x68/0x78 = matrix
  +0xc/+0x1c/+0x2c), then `r_InitFrustrum3(vp, xfov, yfov, near, far, l,t,r,b, &M)`.
* `r_InitFrustrum3` (0x14f950): `r_InitViewBox(&box, near, far, xfov, yfov)`
  then `r_InitFrustrum2(vp, &box, l,t,r,b, &M, scale=(1,1,1))`.
* `r_InitViewBox` (0x14f6b0): `box = {0, 0, 1, tan(xfov/2), tan(yfov/2),
  near, far}` (computed as cos(π/2−f/2)/sin(π/2−f/2)).
* `r_InitFrustrum2` (Mac 0x14fc10, 6 KB) = PC sub_1001ef30 (big1ef30.cpp:832):
  far clamped to [3, 500000]; `m_MatWorld = Scale * R^T * T(-pos)`;
  `aspX = focal/halfW = 1/tan(xfov/2)`, `aspY = 1/tan(yfov/2)`;
  screen mapping `sx = centreX + (halfW-1e-4)*aspX*x/z`,
  `sy = centreY - (halfH-1e-4)*aspY*y/z` (Y flipped, screen y grows down);
  depth `z' = far/(far-near) * (1 - near/z)` (D3D-style, 0 at near, 1 at far);
  `rhw = 1/z`; six camera-space planes: near z>=1 … (big1ef30.cpp:1180-1210),
  transformed to world space at +0x3d0 (used for poly/leaf culling).

quat_ConvertToMatrix (Mac 0x100, standard, q = {x,y,z,w}, s = 2/|q|²):
```
[1-s(yy+zz)   s(xy-wz)    s(xz+wy)  ]
[  s(xy+wz) 1-s(xx+zz)    s(yz-wx)  ]   columns = camera right, up, forward
[  s(xz-wy)   s(yz+wx)  1-s(xx+yy)  ]   (LTMatrix m[row][col], v' = M v)
```

Coordinate system: LithTech is left-handed, +X right, +Y up, +Z forward
(camera looks along column 2). How the Mac maps this to OpenGL: it does NOT
use GL transforms. `r_SetOpenGLTransformStates` (0x1546f0) selects
GL_MODELVIEW/GL_PROJECTION and calls `r_LoadMatrix(&vp+0xdc)` /
`r_LoadMatrix(&vp+0x11c)`, but `r_LoadMatrix` (0x1546c0) is a bare `blr`.
The matrices actually in effect are those set once in `r_Init`
(0x14a0cc..0x14a14c): `glMatrixMode(GL_PROJECTION); glLoadIdentity();
glOrtho(0, width, height, 0, 0.0, 2.0); glMatrixMode(GL_MODELVIEW);
glLoadIdentity(); glScalef(1, 1, -1)`. All world geometry is transformed,
clipped and projected in software into 0x28-byte `TLVertex`es
{x_screen, y_screen, z' (0..1), …, colour ARGB bytes at +0xc..+0xf, fog
coord +0x14, u +0x18, v +0x1c, rhw +0x24} and submitted with
`glVertexPointer(3, GL_FLOAT, 0x28)` and texture coordinates
`(u*rhw, v*rhw, 0, rhw)` (4-component, `r_DrawGLPrimitive` 0x1917e0) so GL's
per-fragment divide by q gives perspective-correct texturing.

For a new GL renderer the equivalent hardware path is (derived from the math
above; not something either shipped renderer does):
```c
glViewport(rect.left, winH - rect.bottom, rect.right - rect.left, rect.bottom - rect.top);
// projection: camera space (x right, y up, z forward) -> GL clip, w = z
P = | 1/tan(xFov/2)  0              0                 0              |
    | 0              1/tan(yFov/2)  0                 0              |
    | 0              0              (f+n)/(f-n)      -2fn/(f-n)      |
    | 0              0              1                 0              |
// with n = 7.0 (hard-coded by both renderers), f = clamp(desc->m_FarZ, 3, 500000)
// modelview = R^T * T(-pos)   where R = quat_ConvertToMatrix(desc->m_Rotation)
```
GL's NDC is itself left-handed with y up, so no axis flip is needed; screen-Y
inversion falls out of the viewport. Winding as seen on screen is mirrored
relative to D3D's y-down screen space; the shipped renderers reject back faces
themselves (plane test, part D/E), so GL face culling can be left off for
world polys.

---------------------------------------------------------------------------

## A.4 Frame flow

Mac `r_RenderScene` (0x157560), PC `d3d_RenderScene` 0x1002d1c0
(w28_100256d0_qipni.cpp:4918) — same order:

```c
BOOL r_RenderScene(SceneDesc* d) {
  if (!d || !g_bIn3D) return 0;
  if (g_bInOptimized2D) { AddDebugMessage(0, "Error: tried to render 3D while in optimized 2D mode."); return 0; }
  g_bPortalsLive = cvar DrawPortals && r_CanDrawPortals();       // mirror/portal pass possible
  if (LightSaturate changed) r_InitSaturateTable(LightSaturate);
  envPanU = d->m_Pos.x*EnvPanSpeed + .5; envPanV = d->m_Pos.z*EnvPanSpeed + .5;
  envScaleU = envScaleV = 0.5/EnvScale;                           // world env-map coords (part E)
  g_vNegLightDir = -RenderStruct.m_GlobalLightDir;                // models
  if (r_InitFrame(d, scratch, 0x7000 /*PC 0x5000*/)) {            // A.2 + A.3, re-reads all cvars
     r_SetupFogStuff();                                           // A.5
     if (|LightScale - last| > .001 in any axis) rebuild colour ramps (r_FillColorMap, x2 if Saturate);
     zero stats; RenderStruct+0x48 = +0x4c = 0;
     if (Wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
     if (!ShowPortalBounds) { g_pPortalCB = r_DrawPortalsCB; r_FullDrawScene(d); g_pPortalCB = 0; }
     glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
     debug prints (ShowTextureCounts, ShowPolyCounts, ShowFillInfo, ShowTexInfo);
  }
  if (DrawWorldTree >= 0 && world) r_DrawWorldTree(&world->+0x6c, n);   // debug
  if (DrawTerrainSections && world) r_DrawTerrainSections(world);       // empty on Mac
  if (WarbleScale changed) r_SetupWarbleTable();
  warbleTime += d->m_FrameTime*WarbleSpeed; warbleFrac = frac(warbleTime);
  frameCount++;  return 0;                                        // answer ignored by engine
}

void r_FullDrawScene(SceneDesc* d) {                              // Mac 0x154780
  r_SetOpenGLTransformStates();                                   // no-op in effect, see A.3
  r_InitObjectQueues();                                           // each module's PreFrame
  r_InitLitPolyList();
  if (d->m_DrawMode == 2) { VisibleSet.Clear(); r_ProcessObjectList(d->m_pObjectList, d->m_ObjectListSize); }
  else {
     r_StartDrawWorld(0);      // r_SetPolyFunctions(mode) + r_SetLightmapStates
     r_TagVisibleLeaves();     // visibility walk; draws sky; queues world polys by kind; queues objects
     r_EndDrawWorld(0);        // draws the queued world polys (below)
  }
  r_FlushObjectQueues();       // world models, models, ... translucents (below)
  r_ClearLitPolyList();
  r_DrawLightScalePoly(&d->m_GlobalLightScale);
  r_DrawLightAddPoly(&d->m_GlobalLightAdd);
}
```

`r_TagVisibleLeaves` (0x16fc40): `r_VLTagPolies(!(world+0x180 & 2) || DrawAll)`.
`r_VLTagPolies(bNoVis)` (0x16f860): with `bNoVis` it draws the whole vis BSP
(`r_DrawBSP(world->GetVisBSP())`); otherwise it runs the world tree's vis query
from `m_Pos` (or `m_AltVisPos`) with r_ProcessLeaf / r_ProcessObject /
r_AddRenderObjects / r_IsPortalVisible / r_NodeFilter; "LockPVS" skips the
walk and reuses last frame's set. Then **`r_DrawSky()`** (sky first, before
any world poly is drawn; part D), `r_DrawLights()`, and, if "DrawWorld", for
each collected visible world poly: back-face test
`dot(plane.n, camPos) - plane.d > 0.01` (plane at `poly+0x28 → +0x34`),
sphere-vs-3-frustum-plane-pairs test against `vp+0x3d0` (poly centre +0x18,
radius +0x24), skip if surface flag 0x4, then dispatch to one of four handlers
chosen by `r_SetPolyFunctions` (no texture → flat; surface flag 0x80 →
lightmap handler; 0x8000 → panning-sky handler; else gouraud). Details and the
exact flag names: parts D and E.

`r_SetPolyFunctions(mode)` (0x1539b0) chooses those handlers:
`flat = r_QueueFlatPoly; gouraud = r_QueueGouraudPoly;
lm = singlePassMT ? r_QueueLMSinglePass : r_DrawLMDualPass;
panSky = singlePassMT ? r_QueueLMSinglePass : r_QueuePanningSkyPoly;`
then overrides — ShowFillInfo: all → r_CalcFillInfo; DrawFlat: all → flat;
if `!LightMap || !g_bLMPagesAvailable || !(world+0x180 & 4)` or `mode == 1`:
lm and panSky → gouraud; if no sky-shadow pan texture
(`RenderStruct+0x108` PanInfo[0].texture == NULL): panSky → gouraud.
(`singlePassMT` = Mac data+0x2a104, set by r_CheckCapabilities; inferred
name.)

`r_EndDrawWorld` (0x1545e0): `r_DrawQueuedLightmapPolies(); r_DrawPanningSkyPolies();
singlePassMT ? r_DrawLMPolies() : r_DrawDualLMPolies(); r_UnsetLightmapStates();
if (gouraud queued) { r_SetGouraudStates; r_DrawGouraudPolies; r_UnsetGouraudStates; }
if (flat queued) { r_SetFlatStates; r_DrawFlatPolies; r_UnsetFlatStates; }`

`r_FlushObjectQueues` (0x16a450) — the order of everything after the static
world (RenderStruct slot +0x24 "RendererPing", a no-op, is called between
steps):
1. r_DrawSolidWorldModels, r_DrawSolidModels, r_DrawSolidPolyGrids,
   r_DrawSolidCanvases;
2. r_SetChromakeyStates; r_DrawChromakeyModels; r_DrawChromakeyWorldModels;
   r_UnsetChromakeyStates;
3. the portal/mirror callback if installed (`r_DrawPortalsCB` →
   `r_DrawPortals(scene, visibleSet)`, which re-enters r_FullDrawScene for
   the view through each portal) — optional, safe to omit at first;
4. r_SetTranslucentObjectStates(0); r_DrawTranslucentParticles,
   r_DrawTranslucentPolyGrids, r_DrawLineSystems, r_DrawTranslucentWorldModels,
   r_DrawTranslucentModels, r_DrawTranslucentCanvases,
   r_DrawTranslucentSprites — these queue into one ObjectDrawList that is then
   sorted (`ObjectDrawList::Sort(vp)`, back to front) and drawn
   (`ObjectDrawList::Draw`); then r_DrawNoZSprites;
   r_UnsetTranslucentObjectStates(1).

---------------------------------------------------------------------------

## A.5 Console variables

### How they reach the renderer

The renderer keeps a linked list of records, one static object per variable
(Mac `ConVar::ConVar(char* name, float def, int* pInt, float* pFloat)`
0x14e340, PC CD3DConVar, big21f90.cpp:464..):

```c
struct ConVar {                 // 0x20
/*0x00*/ int32   m_nVal;        // round(float value)
/*0x04*/ float   m_fVal;
/*0x08*/ float   m_fDefault;
/*0x0c*/ int32*  m_pIntDest;    // optional global that receives m_nVal
/*0x10*/ float*  m_pFloatDest;  // optional global that receives m_fVal
/*0x14*/ char*   m_pName;
/*0x18*/ HCONSOLEVAR m_hVar;
/*0x1c*/ ConVar* m_pNext;
};
```
* At Init: `r_CreateConsoleVariables` (Mac 0x14e520) →
  `r_MaybeCreateCVar(name, def)` (0x14e450): `h = Struct->GetParameter(name)`
  (slot +0x18); if absent, `Struct->RunConsoleString(sprintf("%s %f", name,
  def))` (slot +0x10) and look it up again; store h in +0x18.
* Every frame (top of r_InitFrame) and whenever the engine calls slot +0xf8
  (`CClientMgr::SetGlobals`): `r_ReadConsoleVariables` (Mac 0x14e5a0, PC
  0x10021e50 w15_10021e50_os.cpp:66): `f = Struct->GetParameterValueFloat(h)`
  (slot +0x1c); `m_fVal = f; m_nVal = (int)f` (Mac `fctiwz` = truncate; PC
  rounds); copy to the two destinations. Then the API-specific part: PC
  0x100316e0 (fog / dither / filter device states, and LightMap forced 0 when
  the device can't do it); Mac `r_ReadExtraConsoleVariables` (fog, filters,
  LOD bias, FSAA).

RenderStruct callbacks the engine installs (named from Mac
`r_InitRenderStruct` 0xec6e0, matching clientrender.cpp's PC table):
+0x00 r_ProcessAttachment, +0x04 r_GetSharedTexture(const char*),
+0x08 r_GetTexture(SharedTexture*) → TextureData* (loads system copy if
`tex+0x08` is NULL), +0x0c r_FreeTexture(SharedTexture*), +0x10
RunConsoleString(char*), +0x14 ConsolePrint(fmt, …), +0x18
GetParameter(name) → HCONSOLEVAR, +0x1c GetParameterValueFloat(h), +0x20
GetParameterValueString(h), +0x24 RendererPing() (empty), +0x28
IncObjectFrameCode() → new code, +0x2c GetObjectFrameCode(), +0x30
IncCurTextureFrameCode() → new code, +0x34 Alloc, +0x38 Free.

Renderer slots (Mac `glRenderDLLSetup`, for completeness): +0x6c Init, +0x70
Term, +0x74 BindTexture, +0x78 UnbindTexture, +0x7c CreateContext, +0x80
DeleteContext, +0x84 Clear(LTRect*, flags), +0x88 Start3D, +0x8c End3D, +0x90
IsIn3D, +0x94 StartOptimized2D, +0x98 EndOptimized2D, +0x9c
IsInOptimized2D, +0xa0/+0xa4 Set/GetOptimized2DBlend, +0xa8/+0xac
Set/GetOptimized2DColor, +0xb0 RenderScene, +0xb4 RenderCommand(argc,
argv), +0xb8 GetHook(name), +0xbc SwapBuffers(flags), +0xc0 GetInfoFlags,
+0xc4 GetScreenFormat, +0xc8 CreateSurface(w,h), +0xcc DeleteSurface, +0xd0
GetSurfaceInfo, +0xd4/+0xd8 Lock/UnlockSurface, +0xdc/+0xe0
Optimize/UnoptimizeSurface, +0xe4/+0xe8 Lock/UnlockScreen, +0xec
BlitToScreen, +0xf0 WarpToScreen, +0xf4 MakeScreenShot(name), +0xf8
ReadConsoleVariables, +0xfc BlitFromScreen.

### Variables that affect world drawing

Name, default (Mac; PC identical unless noted, big21f90.cpp:464-549 and
src/engine/d3d CConVar lines), what it does. "obj" = value read from the
ConVar record itself.

| Variable | Default | Effect (evidence) |
|---|---|---|
| DrawWorld | 1 | gates the visible-world-poly loop in r_VLTagPolies (Mac 0x16f9f8) |
| DrawSky | 1 | gates r_DrawSky (Mac 0x177160) |
| DrawAll | 0 | r_TagVisibleLeaves: skip vis, draw the whole BSP |
| LockPVS | 0 | reuse last frame's visible set (Mac 0x16f90c) |
| LightMap | 1 | 0 → lightmapped/panning-sky polys drawn as gouraud (r_SetPolyFunctions); PC forces 0 when the device can't |
| NoLMPages | 0 | CreateWorld skips lightmap pages ("Warning: NoLMPages is TRUE") |
| LightmapsOnly | 0 | draw only lightmaps (part E) |
| LMFullBright | 0 | lightmap pass ignores lightmaps (part E; Mac obj data+0x2f3f8) |
| LMAnim / LMAnimStatic | 1 / 0 | animated lightmaps (part C) |
| LMDynamic / LMDynamicScale / LMDynamicSize | 1 / 2.5 / 16 | dynamic lights rendered into lightmaps (part C) |
| LMUOffset / LMVOffset | 0 / 0 | lightmap texcoord nudge (part C/E) |
| PortalLightmap | 1 | (portal pass) |
| DrawFlat | 0 | all world polys drawn flat-shaded, no texture |
| Wireframe | 0 | glPolygonMode(GL_LINE) for the whole scene (Mac 0x157858) |
| Force1Pass / Force2Pass | 0 / 0 | force single-/dual-pass lightmapping (part E) |
| DisableMultitexture / DisableEnvCombine | 0 / 0 | Mac-only GL capability overrides |
| FullBright (none) | — | no "Fullbright" cvar exists; fullbright is per-texture (part E) |
| DetailTextures / DetailTextureScale / DetailTextureAdd / DetailTextureAngle | 1 / 1 / 1 / 0 | detail-texture overlay pass (part E) |
| EnvMapWorld / EnvMapEnable / EnvMapAll | 1 / 0 / 0 | environment-map overlays (part E) |
| EnvPanSpeed / EnvScale | 0.0005 / 1 | env-map coords: panU/V = pos.x/z*speed+0.5, scale = 0.5/EnvScale (r_RenderScene) |
| AlphaTest | 1 | alpha test for chromakey (part E) |
| Bilinear / Trilinear / MipmapOffset / LodBias / Anisotropic | 1 / 0 / 0 / -2 / 1 | texture filtering (Mac r_ReadExtraConsoleVariables → r_SetNewFilters, GL_TEXTURE_LOD_BIAS) |
| Saturate | Mac 1, PC 0 | colour ramps built with factor 2 instead of 1 |
| LightSaturate | 1 | saturation table (r_InitSaturateTable) |
| Gamma | 100 | |
| FixTJunc | 0 | |
| DrawWorldModels | 1 | world models (part D) |
| SkyScale | 1 | sky camera scale (r_DrawSky passes it to r_InitFrustrum2) |
| AllSkyPortals / ShowSkySplits | 0 / 0 | sky portal debugging (part D) |
| FogEnable | 0 | GL_FOG on/off |
| FogR / FogG / FogB | 255 | fog colour (ints) |
| FogNearZ / FogFarZ | 0 / 2000 | linear fog range, world units |
| SkyFogEnable | 1 | fog on sky geometry (part D) |
| SkyFogNearZ / SkyFogFarZ | 0 / 2000 | sky fog range |
| TableFog | 0 | PC: table (per-pixel) vs vertex fog; Mac: selects r_CalcFog_None |
| NoFogCoord | 0 | Mac: disable GL_EXT_fog_coord path |
| VFog, VFogMinY, VFogMaxY, VFogMinYVal, VFogMaxYVal, VFogMax, VFogDensity | 0, 0, 1300, 0.5, 0, 180, 1800 | volumetric (height) fog, r_CalcFog_Volume |
| LightAddPoly | 1 | enables the LightAdd full-screen quad |
| DrawPortals / ShowPortalBounds | 1 / 0 | portal pass; ShowPortalBounds=1 skips the whole scene draw (Mac 0x157868) |
| DrawWorldTree / DrawTerrainSections | -1 / 0 | debug |
| ShowPolyCounts / ShowTextureCounts / ShowFillInfo / ShowTexInfo / RenderDebug | 0 | debug output |
| WarbleSpeed / WarbleScale | 25 / 0.92 | model warble |

Engine-side (not renderer) variables in SceneDesc: "FarZ" → m_FarZ;
"RenderEnable" gates the RenderScene call.

### Fog

`r_SetupFogStuff` (Mac 0x157190, every frame):
`g_FogRange = FogFarZ - FogNearZ; g_255OverFogRange = 255/g_FogRange;
g_SkyFogInvRange = 1/(SkyFogFarZ - SkyFogNearZ)` (and ×255);
sky fog fn = `r_CalcFog_Sky_Linear`; world fog fn = TableFog ?
`r_CalcFog_None` : VFog ? `r_CalcFog_Volume` : `r_CalcFog_Linear`
(function pointers resolved from Mac data+0xa0fc/0xa10c/0xa0f4/0xa104).
`r_CalcFog_Linear(pos, &out)` (0x151ed0): `d = |pos - vp.m_EyePos|`
(radial distance), `out = d < FogNearZ ? 0 : min(d, FogFarZ)` → written to
the vertex fog coordinate (TLVertex +0x14).
`r_ReadExtraConsoleVariables` (Mac 0x148ed0..0x149310), when any fog cvar
changed: GL_FOG enabled iff FogEnable; GL_FOG_MODE = GL_LINEAR; GL_FOG_COLOR =
(FogR,FogG,FogB)/255, alpha 1; with GL_EXT_fog_coord:
`glFogi(GL_FOG_COORDINATE_SOURCE_EXT, GL_FOG_COORDINATE_EXT)`,
GL_FOG_START = FogNearZ, GL_FOG_END = FogFarZ. Without it, start/end are
converted to the projected-depth units of TLVertex.z (Mac 0x149180..0x149254;
quirk: start uses max(2*FogNearZ, 500), and an all-zero colour with
FogNearZ ≥ 0 is treated as "Fog set to stupid values"). PC: D3D table fog,
LINEAR when TableFog else NONE (vertex fog), start/end = FogNearZ/FogFarZ,
forced off if `FogNearZ == FogFarZ` or no table-fog caps
(w15_100316e0.cpp:153-185). For a modern renderer: linear fog on eye
distance (or eye Z) from FogNearZ to FogFarZ in colour FogR/G/B/255, enabled by
FogEnable; sky geometry uses SkyFogNearZ/FarZ when SkyFogEnable.

---------------------------------------------------------------------------


# B. World data structures the renderer walks

Scope: in-memory layouts of MainWorld, WorldData, WorldBsp, Node, Leaf,
WorldPoly (+vertex), Surface, Plane, SharedTexture/TextureData, surface flag
bits, and texture-coordinate generation. Lightmap-specific fields are only
cross-referenced (Part C, Part C); visibility walking, world models and sky
drawing are Part D; pass logic is Part E.

## Evidence conventions

* `Mac 0xNNNNNN` = code offset in ext/rez/games/nolf-mac-usa/NOLF/LithTech (PPC,
  CodeWarrior, names preserved). Listings come from `ext/tools/pef.py dis`.
* `PC 0x1000xxxx` = d3d.ren, `PC 0x004xxxxx` = lithtech.exe, with the repo file
  that reconstructs it.
* The renderer never loads the world file; the engine does (w_LoadWorldBsp,
  Mac 0xd46e0..0xd62b0). The loader is read here only because it defines every
  field's meaning, size and the allocation strides. All file data is
  little-endian; the Mac loader byte-swaps (`lwbrx`/`lhbrx`/FPEEK). In memory
  everything is native.
* PC vs Mac layout: every offset below that the PC d3d.ren also touches agrees
  (listed per struct). No alignment difference was found: all members are
  4-byte or smaller, no doubles. The one PC/Mac difference to watch is C++
  vtable shape (CodeWarrior vtables have an 8-byte header, so Mac "+0x08" is
  PC slot 0; and the slot sets differ for WorldBsp, see below).

---------------------------------------------------------------------------

## 1. MainWorld (the object the renderer gets the world from)

Mac ctor `__ct__9MainWorldFv` 0xd81a0, `Clear__9MainWorldFv` 0xd8580,
`Load__9MainWorldFP16LoadWorldRequest` 0xd8670, vtable data+0x183e0.
PC: d3d.ren's render world keeps it at RenderWorld+0x08 and reads +0xf8 and
+0x18c/+0x190 (src/engine/d3d/w13_1006c6e0_osoiqip.cpp: "BspWorld6d130",
lines ~232-243 of that file). How the renderer is handed this pointer
(RenderContextInit) is Part A.

```c
struct CMoArrayHdr {            // engine dynamic array, 0x14 bytes
    void*   vptr;               // +0x00
    void*   m_pArray;           // +0x04
    uint32  m_nElements;        // +0x08
    uint32  m_cache[2];         // +0x0c,+0x10
};

struct MainWorld {                              // size >= 0x1ec
    void*        vptr;          // +0x000 PC slots: 0 FindLMAnim(name,&index), 1 GetLMPoly(LMPolyRef*),
                                //        2 GetVisBSP(), 3 GetPhysicsBSP()  (Mac data+0x183e0: dtor first)
    CMoArrayHdr  m_LMAnims;     // +0x004 array of 0x60-byte LMAnim, name inline at +0 (FindLMAnim 0xd9e70: base +8, count +0xc, stride 0x60)  [Part C]
    CMoArrayHdr  m_Arr18;       // +0x018 }
    CMoArrayHdr  m_Arr2c;       // +0x02c } lightmap-data arrays filled by ReadLightmapData (Mac 0xd98c0) [Part C]
    CMoArrayHdr  m_Arr40;       // +0x040 }
    CMoArrayHdr  m_Arr54;       // +0x054 }
    WorldTree    m_WorldTree;   // +0x06c object/visibility tree (__ct__9WorldTreeFv at ctor 0xd8364)  [Part D]
    LTLink       m_EffectHead;  // +0x0e8 (Clear: +0xe8/+0xec self-linked)
    SurfaceEffectRec* m_pEffects; // +0x0f4 singly linked, see 1.2
    float        m_LMGridSize;  // +0x0f8 lightmap texel size in world units (Load 0xd879c; PC d3d reads +0xf8)
    LightTable   m_LightTable;  // +0x0fc (ctor 0xd8378)  vertex/model light grid, not needed for BSP drawing
    LTVector     m_ExtentsMin;  // +0x144 read from file
    LTVector     m_ExtentsMax;  // +0x150 read from file
    LTVector     m_InvExtents;  // +0x15c 1/(max-min) per axis
    LTVector     m_BoxMin;      // +0x168 ExtentsMin - 100
    LTVector     m_BoxMax;      // +0x174 ExtentsMax + 100
    uint32       m_Flags;       // +0x180 bit 0x2 set by Load when a BSP is a vis container;
                                //        bit 0x4 tested by r_SetPolyFunctions (Mac 0x153a70) : lightmapping is used only when SET
                                //        (set by ReadLightmapData when "LightAnim_BASE" exists, Mac 0xd9d2c)
    uint32       m_184;         // +0x184
    CMoArrayHdr  m_Worlds;      // +0x188 WorldData* array: m_pArray @ +0x18c, count @ +0x190  (PC d3d same)
    CMoArrayHdr  m_SkyPortals;  // +0x19c WorldPoly* array: @ +0x1a0, count @ +0x1a4 (BuildSkyPortalList)
    void*        m_1b0;         // +0x1b0 } PC lithtech 0x00422180 walks 0x38-byte records named inline
    uint32       m_1b4;         // +0x1b4 } (portal views, "w_FindPortalView"): inferred
    char*        m_pWorldInfo;  // +0x1b8 world info string (Load 0xd872c)
    LTLink       m_1bc;         // +0x1bc
    RenderContext* m_pRenderContext; // +0x1c8 renderer's per-world context (Part A)
    uint32       m_bLoaded;     // +0x1cc set to 1 at end of Load
    LTLink       m_1d0, m_1dc;  // +0x1d0, +0x1dc
    uint32       m_1e8;         // +0x1e8
};
```

### 1.1 WorldData (one per BSP world: main world + every world model)

Mac `__ct__9WorldDataFv` 0xd7fc0, filled in MainWorld::Load 0xd89c0..0xd8b48.

```c
struct WorldData {              // 0x10 bytes (operator new(0x10), 0xd89c0)
    uint32    m_Flags;          // +0x00 |= 3 after load
    WorldBsp* m_pWorldBsp;      // +0x04 first BSP in the file record ("physics"/full geometry)
    WorldBsp* m_pVisBsp;        // +0x08 second BSP, loaded only if m_pWorldBsp->m_WorldInfoFlags & 0x2
    WorldBsp* m_pRenderBsp;     // +0x0c = m_pVisBsp ? m_pVisBsp : m_pWorldBsp
};
```

* World model lookup: `w_FindWorldModel` 0xd7640 compares `WorldData->m_pWorldBsp->m_WorldName` (+0x04).
* `GetVisBSP` 0xda1c0: first `m_Worlds[i]->m_pWorldBsp` with `m_WorldInfoFlags & 0x20`.
  `GetPhysicsBSP` 0xda240: same with `& 0x10`.
* `GetLMPoly(LMPolyRef*)` 0xd9f30: `LMPolyRef = {uint16 world; uint16 poly;}` →
  `m_Worlds[world]->m_pWorldBsp->m_Polies[poly]`. The same {world, poly} pair is
  the HPOLY: `WorldBsp::MakeHPoly` 0xdc210 returns `(m_WorldIndex << 16) | poly->m_Index`.
* `WorldModelInstance::InitWorldData(physBsp, visBsp)` 0xc9480: stores
  `LTObject+0x1a8 = physBsp, +0x1ac = visBsp, +0x1b0 = visBsp ? visBsp : physBsp`.
  +0x1a8 is the LTObject m_pWorldBsp already known on PC
  (src/engine/server/ltobject_internal.h; d3dqip.cpp walks `+0x1a8` root).
  `IsMainWorldModel` 0xc9410 = `GetWorldInfoFlags(+0x1a8) & 0x4`. World-model
  drawing is Part D.

### 1.2 Surface effects (texture panning etc.)

Loader 0xd56dc..0xd57e0: if a surface's "has effect" byte is 1, it reads an
effect name (0x7f bytes) and param string (0xff), finds the registered effect
(Pan, Rotate, Warble, Portal, Mirror; PC table 0x004a9e38 in
src/engine/lithtech/clientdsi.cpp ~line 1380) and links a 0x14-byte record
`{WorldBsp* +0, Surface* +4, void* effectData +8, SurfaceEffectDesc* +0xc, next +0x10}`
on MainWorld+0xf4. Each frame `cs_UpdateSurfaceEffects` (Mac 0x22540) copies the
surface's O/P/Q into a SurfaceData, calls UpdateEffect, and if it returns 1
writes O/P/Q back and calls `w_GenerateTextureCoordinates` for every poly of the
surface (list: Surface+0x40 first poly index, WorldPoly+0x40 next, 0xffff ends).
**So texture panning/rotation/warble is done by the engine rewriting the
per-vertex u,v in WorldPoly; the renderer just reads them.** Portal sets
Surface+0x42 to the portal index; Mirror sets it to 0x7ffe (|0x8000 for
"OVERLAY") (clientdsi.cpp 0x0042eec0 / 0x0042ef00).

---------------------------------------------------------------------------

## 2. WorldBsp (0x150 bytes)

Mac `operator new(0x150)` in w_LoadWorldBsp 0xd473c; ctor 0xdb530; Clear 0xdb960;
vtable data+0x180c0.

```c
struct WorldBsp {
    void*      vptr;            // +0x000 see vtable below
    char       m_WorldName[64]; // +0x004 read raw (0xd488c)
    uint32     m_44;            // +0x044
    uint32     m_WorldInfoFlags;// +0x048 GetWorldInfoFlags (0xdc110)
    uint32     m_4c;            // +0x04c
    uint32     m_MemoryUse;     // +0x050 bytes allocated (sum computed at 0xd4f18)
    CMoArrayHdr m_PolyData54;   // +0x054 4-byte records {int16,int16}; ptr +0x58, count +0x5c (see WorldPoly +0x0c)
    uint32     m_60, m_64;      // +0x060
    LTPlane*   m_Planes;        // +0x068 16-byte LTPlane {LTVector normal; float dist;}
    uint32     m_nPlanes;       // +0x06c
    Node*      m_Nodes;         // +0x070 0x18-byte Node[]  (GetNodeList 0xdc360)
    uint32     m_nNodes;        // +0x074
    Surface*   m_Surfaces;      // +0x078 0x48-byte Surface[]
    uint32     m_nSurfaces;     // +0x07c
    LeafList*  m_LeafLists;     // +0x080 8-byte LeafList[] (all leaves' lists, contiguous)
    uint32     m_nLeafLists;    // +0x084 (0 for a BSP with no vis data; r_CreateLMPages 0x19c508 branches on it)
    Leaf*      m_Leafs;         // +0x088 0x30-byte Leaf[]
    uint32     m_nLeafs;        // +0x08c
    void**     m_LeafPolyRefs;  // +0x090 storage for every leaf's poly list (4 bytes each)
    uint32     m_nLeafPolyRefs; // +0x094
    uint32     m_VisDataSize;   // +0x098
    Node*      m_RootNode;      // +0x09c GetRootNode (0xdc1d0)
    WorldPoly**m_Polies;        // +0x0a0 (PC d3d same)
    uint32     m_nPolies;       // +0x0a4 (PC d3d same)
    LTVector*  m_Points;        // +0x0a8 vertex positions, 12 bytes each
    uint32     m_nPoints;       // +0x0ac
    UserPortal*m_UserPortals;   // +0x0b0 0x24 bytes each
    uint32     m_nUserPortals;  // +0x0b4
    char*      m_TextureNameData; // +0x0b8 all names, NUL-separated
    char**     m_TextureNames;  // +0x0bc m_TextureNames[i] -> into the block above
    uint32     m_nTextures;     // +0x0c0
    LTVector   m_MinBox;        // +0x0c4
    LTVector   m_MaxBox;        // +0x0d0
    uint32     m_dc;            // +0x0dc read from file (header dword 10), meaning unknown
    uint32     m_e0, m_e4;      // +0x0e0
    LTVector   m_WorldTranslation; // +0x0e8 read from file; SetClipRadius measures from it
    uint32     m_f4;            // +0x0f4
    uint16     m_WorldIndex;    // +0x0f8 index in MainWorld::m_Worlds (Load 0xd8b38)
    uint16     pad;
    uint8*     m_PolyBlock;     // +0x0fc one allocation holding all WorldPoly records
    uint32     m_PolyBlockSize; // +0x100
    uint8*     m_VisData;       // +0x104 all compressed leaf-list (PVS) bytes
    PBlockTable m_PBlocks;      // +0x108 physics blocks (Load__11PBlockTable 0xd6028), not for rendering
    CMoArrayHdr m_Terrain;      // +0x138 TerrainSection[0xc0 each]; ptr +0x13c, count +0x140 (unused by renderer: r_DrawTerrainSections is an empty stub, 0x1574f0)
    uint32     m_144, m_148;
    float      m_RenderClipRadius; // +0x14c max |vertex - m_WorldTranslation| (SetClipRadius 0xdbff0)
};
```

WorldBsp vtable. Mac (data+0x180c0, entries after the 8-byte header):
GetAbstractBspType, GetRootNode, MakeHPoly(Node*), GetHPoly(uint32),
[0xdc2e0: forwards to m_PBlocks], GetNodeList, GetWorldInfoFlags,
IsVisContainer, GetRenderClipRadius, ~WorldBsp. On PC, d3dqip.cpp calls the
root getter as "virtual +0x04" (matches GetRootNode = PC slot 1). d3d.ren's w13
also calls a "slot 9 traverse" on this object, which the Mac vtable does not
have at that slot — **do not rely on PC virtual slots beyond 1; read the fields
directly** (that is what both renderers mostly do).

World-info flag bits (from code, names inferred):

| bit  | evidence | meaning |
|------|----------|---------|
| 0x02 | MainWorld::Load 0xd8a88: load a second BSP into WorldData+8 | has separate vis BSP |
| 0x04 | IsMainWorldModel 0xc9430 (and server ltobject_internal.h WORLDFLAG_MAINWORLD) | main world |
| 0x10 | GetPhysicsBSP 0xda268 | physics BSP |
| 0x20 | GetVisBSP 0xda1e8, IsVisContainer 0xdc154 | vis BSP (has leaves/PVS) |

### 2.1 Node (0x18)

Mac `__ct__4NodeFv` 0xd64e0; loader 0xd5bd8..0xd5d00; PC d3dqip.cpp banner
(flags +0x16, sides +4/+8), w28 sky BSP walk.

```c
struct Node {
    WorldPoly* m_pPoly;         // +0x00 splitting poly; plane = m_pPoly->m_pSurface->m_pPlane
    Node*      m_Sides[2];      // +0x04 [0] = back (behind plane), +0x08 [1] = front
                                //   (PC d3dqip.cpp: "wholly behind -> side +0x04, wholly in front -> +0x08")
    LTLink     m_Objects;       // +0x0c list head (prev +0x0c / next +0x10), objects filed in this node
                                //   (r_DrawBSP 0x16f4f0 walks it for world-model BSPs)
    uint16     m_iLeaf;         // +0x14 0xffff = none
    uint8      m_Flags;         // +0x16 8 for real nodes (loader 0xd5be4); 1 = NODE_IN sentinel, 2 = NODE_OUT sentinel
    uint8      m_PlaneType;     // +0x17 0 +X,1 -X,2 +Y,3 -Y,4 +Z,5 -Z (|n.axis| > 0.99999), 6 = general
                                //   (w_SetPlaneTypes 0xd45f0; only computed for the first BSP of a record)
};
```

Child indices in the file: -1 → the global NODE_IN node, -2 → NODE_OUT
(w_NodeForIndex 0xd3de0; the two statics are initialised in
`__sinit_de_nodes_cpp` 0xe2ca0 with m_Flags 1 and 2, m_pPoly 0, m_iLeaf 0xffff).
A walk stops at any node with `m_Flags & 3` (PC d3dqip.cpp banner). With
pointers into the engine's memory, compare against the sentinel by flags, not
by address.

### 2.2 Leaf (0x30) and LeafList (8)

Loader 0xd5090..0xd52b8; ConvertTouchLists 0xda090; CalcBoundingSpheres
(w_CalcBoundingSpheres 0xd3e60); r_ProcessLeaf 0x16e900.

```c
struct LeafList {               // 8 bytes
    uint16 m_Portal;            // +0x00
    uint16 m_Size;              // +0x02 bytes of data
    uint8* m_pData;             // +0x04 into WorldBsp::m_VisData (compressed leaf bitset; decoding = Part D)
};

struct Leaf {
    LTVector   m_Center;        // +0x00 bounding sphere (w_CalcBoundingSpheres)
    float      m_Radius;        // +0x0c
    LeafList*  m_LeafLists;     // +0x10
    LTLink     m_Objects;       // +0x14 prev / +0x18 next: object links in this leaf (r_ProcessLeaf 0x16e944)
    uint8      pad[4];          // +0x1c
    WorldPoly**m_Polies;        // +0x20 polys touching this leaf (PC d3d w13 "Node13" +0x20 / +0x24)
    uint32     m_nPolies;       // +0x24
    float      m_28;            // +0x28 read from file
    uint16     m_FrameCode;     // +0x2c renderer marks visited leaves (r_ProcessLeaf 0x16e934)
    uint16     m_nLeafLists;    // +0x2e
};
```

File semantics: nLeafLists==0xffff means "share leaf N's lists" (copies +0x2e and
+0x10 from leaf N, 0xd50d8). The poly list is read as `{uint16 world, uint16
poly}` pairs and `MainWorld::ConvertTouchLists` (run on GetVisBSP()) rewrites
each 4-byte slot in place into the `WorldPoly*` of
`m_Worlds[world]->m_pWorldBsp->m_Polies[poly]`. **So the vis BSP's leaves point at
polys of the main world's full BSP (WorldData+4), and those are what get
drawn.**

### 2.3 Plane

`LTPlane {LTVector m_Normal; float m_Dist;}`, 16 bytes (loader 0xd52e8; PC d3dqip4.cpp
`Plane1007aeb4`). Point side: `dot(n,p) - dist`.

### 2.4 UserPortal (0x24)

Loader 0xd5d3c: `{char* m_pName +0; uint16 +4; uint16 m_Index +8; LTVector m_Center +0xc; LTVector m_Dims +0x18}`.

---------------------------------------------------------------------------

## 3. WorldPoly

Allocated inside WorldBsp::m_PolyBlock, variable size:
`size = 0x9c + (nVerts + nFixVerts - 3) * 0x18`, rounded up to 4 (loader
0xd4d48 and 0xd5034). PC confirmation: w13_1006c6e0_osoiqip.cpp `Poly6d130`
(+0x14, +0x28, +0x34, +0x44, +0x48..+0x4b, +0x4c/+0x50/+0x52/+0x54), d3dqip4.cpp
`Poly1007aeb4` (+0x18 centre, +0x24 radius, +0x28, +0x42), d3dqip.cpp banner
(+0x2c light list, +0x42 frame code, +0x18/+0x24 sphere).

```c
struct WorldPolyVertex {        // 0x18 bytes
    LTVector* m_pPos;           // +0x00 -> WorldBsp::m_Points[i]  (shared)
    float     m_U, m_V;         // +0x04,+0x08 base-texture coords in TEXELS (w_GenerateTextureCoordinates)
    float     m_LMU, m_LMV;     // +0x0c,+0x10 lightmap-page coords, written by the RENDERER (r_AddToLMPage 0x19bf98)
    uint8     m_B, m_G, m_R, m_A;// +0x14..+0x17 vertex colour; A = 0xff. On PC (little-endian) the dword at
                                //   +0x14 is D3DCOLOR 0xAARRGGBB. File order is R,G,B (loader stores the
                                //   three bytes to +0x16,+0x15,+0x14; gouraud draw 0x15a5ec reads +0x16 as red)
};

struct WorldPoly {
    uint8        m_00[0x0c];    // +0x00 not referenced by any world draw path found
    void*        m_pData0c;     // +0x0c -> WorldBsp::m_PolyData54 slice, {int16,int16} per entry (loader 0xd5954);
    uint32       m_nData10;     // +0x10    meaning unknown, not used by the world draw paths read
    uint16       m_Flags;       // +0x14 renderer-owned; low 6 bits = lightmap state (r_AddToLMPage 0x19bdb0:
                                //   skip if any set, else clear them and set bit 0)  [Part C]
    uint16       m_Index;       // +0x16 index in WorldBsp::m_Polies (loader 0xd5010)
    LTVector     m_Center;      // +0x18 bounding sphere (PC d3dqip4.cpp; frustum test)
    float        m_Radius;      // +0x24
    Surface*     m_pSurface;    // +0x28
    LightLink*   m_pLights;     // +0x2c dynamic lights touching the poly this frame (renderer-owned)
    uint32       m_30;          // +0x30
    LTVector     m_LMOrigin;    // +0x34 lightmap origin, read from file (loader 0xd5884)  [Part C]
    uint16       m_NextInSurface;// +0x40 next poly of the same surface, 0xffff ends (w_BuildSurfacePolyLists 0xd4350)
    uint16       m_FrameCode;   // +0x42 "already queued this frame" (r_DrawBSP 0x16f564, r_ProcessLeaf 0x16e9b0)
    void*        m_pLMPage;     // +0x44 renderer-owned lightmap page / data pointer  [Part C]
    uint8        m_LMWidth;     // +0x48 lightmap texels, from file
    uint8        m_LMHeight;    // +0x49
    uint8        m_LMPageX;     // +0x4a renderer-owned placement on the page  [Part C]
    uint8        m_LMPageY;     // +0x4b
    WorldPolyVertex* m_pFixVerts;// +0x4c T-junction-fixed vertex list (see below)
    uint16       m_nVerts;      // +0x50
    uint16       m_nFixVerts;   // +0x52
    WorldPolyVertex m_Verts[3]; // +0x54 really m_nVerts, followed by m_nFixVerts more when m_nFixVerts was nonzero in the file
};
```

Two vertex lists (loader 0xd5b68..0xd5b90; consumers r_ReallyDrawGouraudPoly
0x15a558, w_GenerateTextureCoordinates 0xd7ae0):

* base: `m_Verts[0 .. m_nVerts)`.
* "FixTJunc": if the file gave extra vertices, `m_pFixVerts = &m_Verts[m_nVerts]`,
  count `m_nFixVerts` (a complete second polygon with T-junction vertices inserted);
  otherwise the loader sets `m_pFixVerts = m_Verts` and `m_nFixVerts = m_nVerts`.
* The renderer draws the second list when console variable **FixTJunc** (default 0;
  registered at Mac 0x14e844, value word data+0x291d4) is non-zero, else the base list.
* Both lists get texture coordinates; a whole-poly loop is
  `end = m_Verts + (m_pFixVerts == m_Verts ? m_nVerts : m_nVerts + m_nFixVerts)`.

Polys are convex fans: drawn with GL_TRIANGLE_FAN (mode 6 passed to
r_DrawGLPrimitive at 0x15ab20), vertices in stored order.

---------------------------------------------------------------------------

## 4. Surface (0x48)

Mac ctor 0xd63d0, dtor 0xd6340, vtable data+0x1807c; loader 0xd53e8..0xd5810.
PC: d3dqip4.cpp `Surf1007aeb4` (+0x30 texture, +0x34 plane, +0x38 flags, +0x42);
w30_1000f020_osqipni.cpp `Basis1000f020` (vtable slot 0 = lightmap axes);
w8_1006a5f0 (+0x38 & 1 = solid).

```c
struct Surface {
    void*          vptr;        // +0x00 PC slot 0 (Mac 2nd entry, code 0xdc450): GetLMVectors(LTVector* P, LTVector* Q), thiscall
    LTVector       O;           // +0x04 texture origin
    LTVector       P;           // +0x10 texture U axis (length = texels per world unit)
    LTVector       Q;           // +0x1c texture V axis
    Nexus          m_Nexus;     // +0x28 (8 bytes)
    SharedTexture* m_pTexture;  // +0x30 NULL => no texture => flat-shaded poly (dispatch, section 6)
    LTPlane*       m_pPlane;    // +0x34 -> WorldBsp::m_Planes
    uint32         m_Flags : 29;// +0x38 low 29 bits: SURF_* (file dword, 0xd5630). Low bits on BOTH PC and Mac
                                //   (both test 0x80 for lightmap: PC w13 0x80, Mac r_AddToLMPage 0x19bda0)
    uint32         m_LMPlane : 3;//      top 3 bits (bits 29..31): lightmap plane index 0..5 (0xd55f0..0xd5610)
    uint16         m_TextureFlags; // +0x3c file "texture flags" (game SurfaceType id; GetPolyTextureFlags)
    uint16         m_iTexture;  // +0x3e index into WorldBsp::m_TextureNames
    uint16         m_FirstPoly; // +0x40 first poly of this surface, 0xffff none (w_BuildSurfacePolyLists)
    uint16         m_PortalId;  // +0x42 0x7fff = not a portal; user-portal index; 0x7ffe = mirror (|0x8000 overlay)
    uint8          m_44;        // +0x44 byte from file (0xd5650), meaning unknown
    uint8          pad[3];
};
```

Note: the PC bitfield packing of `m_LMPlane` is inferred — the PC code read
only tests low flag bits and calls the vtable for the lightmap axes, so a PC
renderer never needs the top 3 bits itself: mask flags with 0x1fffffff or call
slot 0.

### 4.1 Lightmap axes (for reference; C covers use)

`lightmap_planes.cpp` (Mac static init 0xbb1c0, table pointer data+0x236f4)
builds 6 records `{LTVector v0, up, normal}` (0x24 each):

| idx | normal | up |
|-----|--------|----|
| 0 | (0,1,0) | (0,0,-1) |
| 1 | (0,-1,0) | (0,0,1) |
| 2 | (0,0,1) | (0,1,0) |
| 3 | (0,0,-1) | (0,-1,0) |
| 4 | (1,0,0) | (0,-1,0) |
| 5 | (-1,0,0) | (0,-1,0) |

Plane index (0xbaf50) = argmax over the 6 of dot(planeNormal, table.normal).
GetLMVectors (0xbb0b0): `P = up × n; Q = n × P` with n = the surface plane's
normal. PC's thiscall slot 0 returns the same pair (w30 `Slot00(&vAxisU,&vAxisV)`).

### 4.2 Surface flag bits

No SURF_* header exists in the repo (src/LT2/sdk has none). Values below are from
code; names are the LithTech conventional ones and are inferred unless the code
shows the meaning.

| bit | where tested | meaning (confidence) |
|-----|--------------|----------------------|
| 0x1 | PC w8_1006a5f0_osoiqip.cpp ("1 = solid") | SURF_SOLID; not used for drawing |
| 0x4 | r_ProcessLeaf 0x16e9c4, r_DrawBSP 0x16f578, r_RecurseAndDrawWM, r_DrawSkyWorldModel, r_VLTagPolies; PC d3dqip4.cpp `& 4 -> continue` | SURF_INVISIBLE: never drawn (still frame-stamped) |
| 0x8 | r_ProcessWorldModel 0x16ffd0 / r_GetFirstSurfaceFlags (first poly's surface) | SURF_TRANSLUCENT: whole world model goes to the translucent set |
| 0x10 | BuildSkyPortalList 0xd9468, r_ProcessLeaf 0x16e9cc, r_DrawBSP 0x16f580 | SURF_SKY: sky portal. Queued on the sky-portal list (VisibleSet+0x28) instead of drawn, unless DrawFlat is on |
| 0x80 | r_AddToLMPage, r_CalcFillInfo, r_VLTagPolies; PC d3dqip4.cpp/w13 | SURF_LIGHTMAP: poly has a lightmap |
| 0x8000 | r_DrawLMPolies, r_VLTagPolies; PC d3dqip4.cpp (4th draw function) | SURF_PANNINGSKY (draw function slot = r_QueuePanningSkyPoly) |
| 0x100000 | PC w30_1000f020_osqipni.cpp line ~1722, passed into the lightmap vertex copy | unknown (PC only) |

Fullbright is NOT a surface flag: it is DTX header flag 0x1 on the texture
(section 5). Chromakey for world geometry is per object (LTObject FLAG2_CHROMAKEY
= 0x2, src/LT2/sdk/inc/ltbasedefs.h:1187, tested on world models at
r_ProcessWorldModel 0x170024 via LTObject+0x8c) plus the texture's command
string ColorKey/AlphaRef (section 5).

---------------------------------------------------------------------------

## 5. Textures: SharedTexture, TextureData (DTX), render data

### 5.1 SharedTexture (engine object; PC src/engine/lithtech/clienttexmgr.cpp:117-157, clientrender.cpp, lttexmod.cpp:259)

```c
struct SharedTexture {
    void*          m_BankLink[2];   // +0x00
    TextureData*   m_pEngineData;   // +0x08 system-memory DTX; NULL until loaded
    void*          m_pRenderData;   // +0x0c RENDERER-OWNED (Mac: RTexture* list, 0x15e650)
    LTLink         m_Link;          // +0x10
    uint32         m_1c;            // +0x1c
    void*          m_20;            // +0x20 Mac renderer uses it as a per-frame poly bucket (r_QueueGouraudPoly 0x1599e8): renderer scratch
    void*          m_pFile;         // +0x24
    SharedTexture* m_pLinkedTexture;// +0x28 detail / env map texture named by the DTX command string
    uint32         m_nLinkType;     // +0x2c 0 detail, 1 env map, 2 env map in alpha (clientrender.cpp)
    uint16         m_FrameCode;     // +0x30 renderer stamps the current texture frame code (Mac 0x15e64c)
    uint16         m_RefCount:15, m_bMarked:1; // +0x32
};
```

Getting pixels: RenderStruct callback +0x08 (`GetTexture`: returns
m_pEngineData, loading it if NULL; Mac r_GetTexture 0xec1c0), release with +0x0c.
The engine calls renderer slot +0x74 BindTexture(tex, changed) when contents
change / for unbound textures and +0x78 UnbindTexture before freeing
(clientrender.cpp header, lines ~150-160).

### 5.2 TextureData (PC src/engine/lithtech/clientdsi.cpp:922-1040; Mac same offsets)

```c
struct DtxHeader {                  // 0xa4 bytes, copied verbatim from the .dtx
    uint32 m_ResType;               // +0x00 (TextureData+0x08)
    int32  m_Version;               // +0x04 -5
    uint16 m_BaseWidth, m_BaseHeight; // +0x08,+0x0a (TD+0x10,+0x12)
    uint16 m_nMipmaps;              // +0x0c (TD+0x14)
    uint16 m_nSections;             // +0x0e
    uint32 m_IFlags;                // +0x10 (TD+0x18) DTX flags
    uint32 m_UserFlags;             // +0x14
    uint8  m_Extra[12];             // +0x18 (TD+0x20..+0x2b)
    char   m_CommandString[128];    // +0x24 (TD+0x2c)
};
struct TextureMipData { uint32 m_Width, m_Height; void* m_pData; uint32 m_Pitch; uint32 bump[2]; }; // 0x18
struct TextureData {                // 0x18c
    void*          vptr;            // +0x00
    uint32         m_04;
    DtxHeader      m_Header;        // +0x08
    LTLink         m_Link;          // +0xac
    uint32         m_AllocSize;     // +0xb8
    void*          m_pSections;     // +0xbc
    SharedTexture* m_pSharedTexture;// +0xc0
    uint32         m_c4;
    void*          m_pDataBuffer;   // +0xc8
    TextureMipData m_Mips[8];       // +0xcc (mip 0 width at TD+0xcc, height TD+0xd0, data TD+0xd4, pitch TD+0xd8)
};
```

Header fields the Mac renderer reads (r_SetupTextureSurface 0x180590,
r_SetupTextureMip 0x17ff70):

* `m_IFlags & 0x1` → RTexture "fullbright" word (0x180c78; returned by
  r_IsFullbriteTexture 0x1822a0). **DTX_FULLBRITE = 0x1.**
* `m_IFlags` 0x2 / 0x100 / 0x80 select the GL internal format class (0x180628..0x180688); Part E.
* `m_Extra[0]` (TD+0x20) texture group (clamped to 9, selects a mip bias table),
  `m_Extra[1]` (TD+0x21) max mips to use (0 → 4), `m_Extra[2]` (TD+0x22) BPP ident
  (0 → 3 = 32-bit ARGB), `m_Extra[3]` (TD+0x23) mipmap offset (skip top mips),
  **`m_Extra[4]` (TD+0x24) texture-coordinate scale shift** (below),
  `m_Extra[6..9]` (TD+0x26) float detail-texture scale, int16 at TD+0x2a detail
  angle in degrees (cos/sin at 0x180ab4).
* Command string keywords parsed by the renderer: `ColorKey r g b` (chromakey colour,
  0x17ffd0) and `AlphaRef n` (alpha-test ref, `glAlphaFunc(GL_GREATER, n/255)` at
  0x15e7f8). The engine itself parses `DetailTex`, `EnvMap`, `EnvMapAlpha` into
  SharedTexture+0x28/+0x2c (clientrender.cpp).

### 5.3 Texture-coordinate math

Engine side (`w_GenerateTextureCoordinates` Mac 0xd7ae0, run at load 0xd61f0 and by
surface effects):

```c
for each vertex v of the poly (both lists):
    LTVector d = *v.m_pPos - surf->O;
    v.m_U = dot(d, surf->P);
    v.m_V = dot(d, surf->Q);
```

Renderer side: per bound texture the Mac computes (r_SetupTextureMip 0x18048c..0x180534)

```c
uMul = (1 << TD->m_Header.m_Extra[4]) / (float)TD->m_Mips[0].m_Width;
vMul = (1 << TD->m_Header.m_Extra[4]) / (float)TD->m_Mips[0].m_Height;
```

stores them in RTexture+4/+8, and on bind loads them into a per-texture-unit
`TCoordMul` (r_SetTexture 0x15e80c → Set__9TCoordMulFff), then multiplies every
emitted vertex: `s = v.m_U * uMul; t = v.m_V * vMul` (r_ReallyDrawGouraudPoly
0x15aa14..0x15aadc; PC equivalent: per-stage inverse width/height table
g_a1008e664, w30_1000f020_osqipni.cpp). GL wrap mode is repeat (tiling). With
m_Extra[4] = 0 this is plain `u / width, v / height`.

Lightmap coordinates (Part C has the full derivation; PC w30 sub_1000f0a0 and Mac
r_AddToLMPage agree):
`lmU = ((dot(pos - poly->m_LMOrigin, P_lm) / MainWorld.m_LMGridSize) + 0.5 + pageX) / 64`,
same for V with Q_lm/pageY (Mac multiplies by 0.015625 = 1/64).

---------------------------------------------------------------------------

## 6. How the renderer classifies a poly (summary, for E)

PC d3dqip4.cpp lines ~756-770 and Mac r_SetPolyFunctions 0x1539b0 agree:

```c
if (surf->m_Flags & 0x4)              skip;                 // invisible
else if (!surf->m_pTexture)           FlatPoly(poly);       // Mac slot data+0x29998 = r_QueueFlatPoly
else if (surf->m_Flags & 0x80)        LightmapPoly(poly);   // data+0x2999c = r_DrawLMDualPass or r_QueueLMSinglePass
else if (surf->m_Flags & 0x8000)      PanningSkyPoly(poly); // data+0x299a0 = r_QueuePanningSkyPoly (LMSinglePass in single-pass mode)
else                                  GouraudPoly(poly);    // data+0x29994 = r_QueueGouraudPoly
```

Overrides in r_SetPolyFunctions: ShowFillInfo → all r_CalcFillInfo;
**DrawFlat** (data+0x290fc) → all flat; **LightMap** cvar (data+0x1f9bc) off, no LM
pages, or MainWorld+0x180 & 4 → lightmap and panning-sky slots become gouraud;
draw mode 1 → same. Before dispatch, the leaf/BSP walk (r_ProcessLeaf, r_DrawBSP)
already removed SURF_INVISIBLE, diverted SURF_SKY (0x10) to the sky-portal list
and Surface::m_PortalId != 0x7fff to the portal list, and stamped
`poly->m_FrameCode`.

---------------------------------------------------------------------------

## 7. File record order (for reference; renderer does not read files)

Per world BSP record (w_LoadWorldBsp): 8 ignored dwords; worldInfoFlags; dword (→+0xdc);
name[64]; nPoints, nPlanes, nSurfaces, nUserPortals, nPolies, nLeafs, (ignored),
visDataSize, nLeafLists, nNodes, nLeafPolyRefs, nPolyData54; MinBox, MaxBox,
WorldTranslation (9 floats); texNameBytes, nTextures, names; per poly
{uint8 nVerts, uint8 nFixVerts}; leaves {uint16 nLists | 0xffff+uint16 shareIndex,
lists {uint16 portal, uint16 size, bytes}, uint32 nPolyRefs, {uint16 world, uint16 poly}[],
float}; planes {normal, dist}; surfaces {O,P,Q, uint16 texture, uint32 plane,
uint32 flags, uint8 (+0x44), 3 bytes ignored, uint8 hasEffect [+ name[127], params[255]],
uint16 textureFlags}; polies {LMOrigin vec3, uint16 lmW, uint16 lmH, uint16 n,
{int16,int16}[n], uint32 surface, per vertex {uint16 point, uint8 r, g, b}};
nodes {uint32 poly, uint16 leaf, int32 sides[2]}; user portals {name[127], dword,
uint16, vec3, vec3}; points {vec3 pos, vec3 (ignored)}; PBlockTable; uint32 root
node; uint32 nTerrain + sections. MainWorld::Load: header, world info string,
LMGridSize, extents min/max, WorldTree layout, nWorlds, then per world a dword
(next-record offset) and 1 or 2 BSP records; then lightmap data (Part C).

---------------------------------------------------------------------------


# C. Lightmaps, end to end

Sources:
- Mac `LithTech` PEF (PowerPC, names intact), disassembled with `ext/tools/pef.py`. Every Mac address below is a code offset.
- PC d3d.ren reconstruction:
  - `src/engine/d3d/w13_1006c6e0_osoiqip.cpp` (LM pages)
  - `src/engine/d3d/w17_1006ad20_qip.cpp` (compositing, dynamic lights)
  - `d3dbatchd.cpp:656` and `d3dbatchc.cpp:681` (decompressors)

**Result in one line:** the PC and Mac implementations use the same design and the same field offsets for everything here. They differ only in the texture API (DirectDraw surface vs GL texture plus a system-memory copy).

A full list of the Mac renderer's console variables is in `mac-cvars.txt`, extracted from the `__sinit_*` `ConVar` constructors.

---

## C.1 Where lightmap data lives (engine side, `MainWorld`)

All lightmaps, static ones included, are stored as **light animations** (`LMAnim`). The static lightmaps are the animation named **`"LightAnim_BASE"`**: frame 0, every lightmapped poly.

### File layout (`MainWorld::ReadLightmapData(ILTStream*)`, Mac 0xd98c0)

Evidence: Mac 0xd98d8..0xd9d4c. Integers are little-endian; the Mac byte-swaps them with `lwbrx`/`lhbrx`. The PC reads them raw.

```
seek(MainWorld+0x68)              // m_LMDataOffset, 3rd out-param of w_ReadWorldHeader (Mac 0xd86d8)
u32 nTotalPolyRefs                // -> array MainWorld+0x18 (4-byte elements), data ptr at +0x1c, count +0x20
u32 nTotalFramePtrs               // -> array MainWorld+0x2c (4-byte elements), data +0x30, count +0x34
u32 nTotalDataBytes               // -> array MainWorld+0x40 (bytes),           data +0x44, count +0x48
u32 nTotalFrameEntries            // -> array MainWorld+0x54 (8-byte elements), data +0x58, count +0x5c
u32 nAnims                        // -> array MainWorld+0x04 (0x60-byte LMAnim), data +0x08, count +0x0c
for each anim (0x60 bytes each):
    char  name[32]                          -> anim+0x00
    u32   bShadowMap                        -> anim+0x20
    u32   nFrames                           -> anim+0x28   (anim+0x24 = &framePtrPool[cursor])
    u32   nPolies                           -> anim+0x30   (anim+0x2c = &polyRefPool[cursor])
    nPolies x { u16 iWorldModel; u16 iPoly }   -> anim+0x2c[i]   (LMPolyRef)
    for each frame f:
        anim+0x24[f] = &frameEntryPool[cursor]; cursor += nPolies
        for each poly i:
            entry.pData = &dataPool[dcursor]
            u32 entry.nSize
            u8  data[nSize]                 (compressed lightmap, see C.2)
            dcursor += nSize
```

Each pool cursor is bounds-checked against its count. On overflow the world is freed and an error code returned (0x43 when allocation fails, 1 otherwise).

After loading, the loader looks for `"LightAnim_BASE"` through the MainWorld virtual at +0xc (`FindLMAnim`). If found:
- it sets bit 4 of `MainWorld+0x180`;
- it forces that anim's `iFrames[0] = iFrames[1] = 0` and `fPercentBetween = 0` (Mac 0xd9d08..0xd9d48).

`FreeLightmapData` (0xd9da0) empties the five arrays.

### Structures (evidence for each in the comment)

```c
struct LMPolyRef {            // MainWorld::GetLMPoly (0xd9f30)
    uint16 m_iWorldModel;     // +0  index into MainWorld+0x18c (WorldModel* array, count +0x190); element+4 = WorldBsp*
    uint16 m_iPoly;           // +2  index into WorldBsp+0xa0 (WorldPoly* array, count +0xa4)
};

struct LMFrameEntry {         // 8 bytes; ReadLightmapData; PC LMFrameEntry (w17:189)
    uint8* m_pData;           // +0 compressed lightmap of one poly in one frame
    uint32 m_nSize;           // +4
};

struct LMAnim {               // 0x60 bytes. __ct__6LMAnimFv (0xd7f40); ic_Get/SetLightAnimInfo (0xb81e0/0xb8280)
                              // map +0x20..+0x5c 1:1 onto SDK LAInfo (src/LT2/sdk/inc/iltlightanim.h:19)
    char          m_Name[32];        // +0x00
    uint32        m_bShadowMap;      // +0x20 LAInfo.m_bShadowMap (PC: "m_bDynamic20")
    LMFrameEntry**m_Frames;          // +0x24 [nFrames] -> LMFrameEntry[nPolies]
    uint32        m_nFrames;         // +0x28
    LMPolyRef*    m_Polies;          // +0x2c
    uint32        m_nPolies;         // +0x30
    uint32        m_iFrames[2];      // +0x34,+0x38 (ctor: -1 = LIGHTANIMFRAME_NONE)
    float         m_fPercentBetween; // +0x3c
    float         m_fBlendPercent;   // +0x40 (ctor 1.0) "intensity"
    LTVector      m_vLightPos;       // +0x44  shadow-map anims only
    LTVector      m_vLightColor;     // +0x50  RGB 0..255
    float         m_fLightRadius;    // +0x5c
};
```

`MainWorld`, the lightmap-relevant fields only (other forks own the rest):
- +0x08/+0x0c: `LMAnim*` array and count.
- +0x68: `m_LMDataOffset`.
- +0xf8: **float lightmap grid size** (world units per lightmap texel). Read as the float that directly follows the world-info string (Mac `MainWorld::Load` 0xd8774..0xd87a0); the PC comment calls it "m_fLMGridSize" (w13:41, w13:226).
- +0x18c/+0x190: world models.

### Per-poly references

The poly's own list of the animations that touch it is read in `w_LoadWorldBsp` (Mac 0xd5858..0xd5a6c; per-poly record, in file order):

```
float LMOrigin[3]       -> WorldPoly+0x34..+0x3c
u16   lmWidth           -> (u8) WorldPoly+0x48
u16   lmHeight          -> (u8) WorldPoly+0x49
u16   nAnimRefs         -> WorldPoly+0x10   (WorldPoly+0x0c = &WorldBsp+0x58 pool[cursor])
nAnimRefs x { u16 iAnim; u16 iPolyInAnim }  -> WorldPoly+0x0c[i]   ("PolyAnimRef")
u32   iSurface          -> WorldPoly+0x28 = &surfaces(WorldBsp+0x78)[iSurface*0x48]
```

```c
struct PolyAnimRef { uint16 m_iAnim; uint16 m_iPoly; };   // r_UpdatePolyLightmap 0x19e264..0x19e29c; PC LMRef (w17:226)
// m_iAnim indexes MainWorld+0x08 (LMAnim, 0x60 stride); m_iPoly indexes that anim's per-frame LMFrameEntry array.
```

### Dirty marking

`MainWorld::SetLMPoliesChanged(LMAnim*)` (0xd9fc0) sets bit **0x8000** of `WorldPoly+0x14` on every poly of the anim.

It is called from `InternalSetLightAnimInfo` (Mac 0x34e0) after `ic_DetectLMAnimChange` and `ic_SetLightAnimInfo`. That function is in turn driven by the server's LMAnim packet (`OnLMAnimInfoPacket`, 0x3d9xx) and by `ILTLightAnim`.

---

## C.2 Compression formats and pixel format

### Colour lightmaps: 16-bit RLE of X1R5G5B5 pixels

Evidence:
- `DecompressLMData(uint8*, uint32, uint16*)`, Mac 0xbacf0;
- PC `sub_1006a790` (d3dbatchd.cpp:656), identical.

```c
bool DecompressLMData(const uint8* src, uint32 size, uint16* out /* >= 1024 */)
{
    const uint8* end = src + size;  int left = 1024;          // max 32x32 texels
    while (src < end) {
        uint16 w = src[0] | (src[1] << 8); src += 2;           // little-endian
        uint8 n;
        if (w & 0x8000) { n = *src++; w &= 0x7fff; }           // run: value + 8-bit count
        else            { n = 1; }
        if ((left -= n) < 0) return false;
        while (n--) *out++ = w;
    }
    return true;
}
```

Pixel format: **5:5:5, R in bits 14..10, G in 9..5, B in 4..0**. Bit 15 is free, which is why it can serve as the run flag. Evidence:
- `Term__15LMRenderSurface` (0x19b1d4..0x19b1ec) and `r_CreateLMPages` (0x19c6e4..0x19c700) hand FormatMgr a source format of (type 2, A 0, R 0x7c00, G 0x3e0, B 0x1f).
- PC w17:952 does the same.
- The blending code masks with 0x7c00/0x3e0/0x1f (0x19def8..).

Output is **row-major, `lmWidth` texels per row, `lmWidth*lmHeight` texels**: `lightmap_blit` is given a pitch of `width*2` (0x19e3e0).

Conversion to RGB8, the engine's own way: FormatMgr expands n-bit channels with `table[n][i] = i*255/((1<<n)-1)` (`src/engine/lithtech/fmconvert.cpp:109`), so `r8 = r5*255/31`, and the same for g and b. The dynamic-light code uses `c<<3` instead (C.6).

### Shadow-map layers: 8-bit, two-colour RLE

Used when `LMAnim.m_bShadowMap`. Evidence: PC `sub_1006a800` (d3dbatchc.cpp:681); Mac `DecompressShadowMap(uint8*, uint32, uint8*)` at 0xbad90.

```c
// alternating runs of 0x00 and 0xFF, starting with 0x00; each byte is a run length (0 allowed)
idx = 0; colors = {0x00, 0xFF};
while (len--) { n = *src++; if (out+n > out0+1024) return false; memset(out, colors[idx], n); out += n; idx ^= 1; }
```

---

## C.3 Per-poly lightmap fields, the surface, and the lightmap basis

### Fields

`WorldPoly` fields used by lightmaps. Part B owns the full layout; these offsets agree across the PC (w13:140, w17:241) and the Mac:

| off  | type | meaning |
|------|------|---------|
| +0x0c | PolyAnimRef* | animation refs |
| +0x10 | uint32 | ref count |
| +0x14 | uint16 | flags. Bits 0..5 = "LM page state" (0 means not placed; `r_AddToLMPage` sets it to 1). Bit 15 (0x8000) = lightmap changed, needs recompositing |
| +0x28 | Surface* | |
| +0x2c | LightLink* | dynamic lights touching this poly (node: +0 next, +4 LTObject* light, +8 LTVector light pos) |
| +0x34 | LTVector | **lightmap origin** O (world/BSP space) |
| +0x44 | LMPage* | page the lightmap is placed on (renderer-owned) |
| +0x48 | uint8 | lightmap width (texels, ≤ 32 in practice: 1024-texel decompress cap) |
| +0x49 | uint8 | lightmap height |
| +0x4a | uint8 | X of the lightmap on its page |
| +0x4b | uint8 | Y of the lightmap on its page |
| +0x4c | Vertex* | vertex array pointer (normally == &poly+0x54) |
| +0x50 | uint16 | vertex count |
| +0x52 | uint16 | extra vertices (included when +0x4c != +0x54) |
| +0x54 | Vertex[] | 0x18 bytes each: +0x00 LTVector* point, +0x04/+0x08 texture u,v, **+0x0c/+0x10 lightmap u,v** (written by the renderer), +0x14 ? |

`Surface` (0x48 bytes; `__ct__7SurfaceFv` 0xd63d0, `w_LoadWorldBsp` 0xd55d0..0xd564c), lightmap-relevant part:
- +0x00: vtable;
- +0x04/+0x10/+0x1c: texture O, P, Q (Part B);
- +0x34: `LTPlane*` (plane normal first; the plane index from the file selects `WorldBsp+0x68[idx*0x10]`);
- +0x38: **flags**. Bit 0x80 = lightmapped (`r_AddToLMPage` 0x19bd98; PC w13:383). **Bits 29..31 = lightmap plane type 0..5**, computed at load as `GetLMPlaneType(plane normal)` (Mac 0xd55f0..0xd5610) and OR-ed over the file's low 29 bits.

### Lightmap axes: `Surface::GetLightmapTextureVectors(LTVector& P, LTVector& Q)`

Mac 0xdc450 (unnamed in the function list; the traceback name is at 0xdc4a0). It is the first virtual after the destructor; on the PC it is vtable slot 0, `GetVectors` (w13:115). It calls `SetupLMPlaneVectors(flags>>29, &plane->normal, &P, &Q)` (Mac 0xbb0b0):

```c
// g_LMPlanes[6], built by __sinit_lightmap_planes_cpp (Mac 0xbb1c0): {X, Up, Normal}
//  type  Normal    Up (the vector used)
//   0    ( 0, 1, 0) ( 0, 0,-1)
//   1    ( 0,-1, 0) ( 0, 0, 1)
//   2    ( 0, 0, 1) ( 0, 1, 0)
//   3    ( 0, 0,-1) ( 0,-1, 0)
//   4    ( 1, 0, 0) ( 0,-1, 0)
//   5    (-1, 0, 0) ( 0,-1, 0)
int GetLMPlaneType(LTVector N)        // Mac 0xbaf50: argmax_i dot(N, g_LMPlanes[i].Normal)
void SetupLMPlaneVectors(int type, const LTVector* N, LTVector* P, LTVector* Q)
{
    LTVector A = g_LMPlanes[type].Up;
    *P = Cross(A, *N);      // P.x = A.y*N.z - A.z*N.y; P.y = A.z*N.x - A.x*N.z; P.z = A.x*N.y - A.y*N.x
    *Q = Cross(*N, *P);
}
```

P and Q are **not normalised**: |P| = |Q| = sin(angle between A and N), which is 1 for axis-aligned planes. The lightmap tools must have used the same basis, so the formulas below have to be followed exactly.

---

## C.4 Lightmap texel ↔ world mapping and UVs

`grid = MainWorld+0xf8`. Lightmap texel (x, y), counted from the poly's lightmap rectangle, samples the world point

```
W(x,y) = O + x*stepU + y*stepV,   stepU = P * grid/|P|^2,  stepV = Q * grid/|Q|^2
```

Evidence: Mac 0x19cd8c..0x19cecc; PC `sub_1006b300`, w17:513ff ("basis axes scaled by the texel size over their squared length").

So texel x's centre sits at `dot(P, W-O)/grid = x`. The per-vertex lightmap coordinate in texels is that value plus 0.5:

```c
// r_AddToLMPage (Mac 0x19bec8..0x19c038) == PC sub_1006c8a0 (w13:451..470), done once at page build:
for each vertex v of poly:
    d  = *v.point - poly.LMOrigin;
    lu = (dot(d, P) / grid + 0.5f + slotX) * (1/64.f);   // stored at vertex+0x0c
    lv = (dot(d, Q) / grid + 0.5f + slotY) * (1/64.f);   // stored at vertex+0x10
```

`slotX` and `slotY` are the poly's place on its 64x64 page (+0x4a/+0x4b). The +0.5 maps the lightmap's texel *centres* onto the polygon's sample points. With bilinear filtering (the pages use `GL_LINEAR`), texels from a neighbouring lightmap on the same page can bleed into the poly's border region; the engine accepts this, with 4-texel block packing (C.5).

When the lightmap is instead drawn from a private per-poly surface (Mac `LMRenderSurface`, C.7), `r_DrawLMPoly` recomputes it as `lu = (dot(d,P)/grid + 0.5) * texScale[lmUnit].u` with no slot offset, where texScale = 1/surfaceSize (Mac 0x194a50..0x194b40, 0x19b364..0x19b3ac).

Cvars `LMUOffset` and `LMVOffset` (lm_dualpass, default 0) exist, but nothing reads them (no code references besides the constructor).

---

## C.5 Packing into pages

The same algorithm runs on both platforms. Mac:
- `r_CreateLMPages` 0x19c3e0
- `r_AddToLMPage` 0x19bd60
- `r_FindFreeSlot` 0x19ba10
- `r_CreateNewPage` 0x19bb90
- `r_LeafCB` 0x19c0e0 / `r_ProcessLeaf` 0x19c190

PC: w13 `sub_1006cc90` / `sub_1006c8a0` / `sub_1006c6e0`.

Page: **64x64 texels**, allocation in **4x4-texel blocks**. Block map bit `n = (y>>2)*64 + (x>>2)` (rows padded to 64 bits; 0x80 bytes).

```c
struct LMPage {                 // Mac: new 0x30 bytes (0x19bba0); PC 0x28 bytes (w13:240)
    void*  vtbl;                // +0x00  (Mac LMPage derives from the RTexture-like base: GetTextureType__6LMPage)
    uint32 _04, _08;            // +0x04,+0x08 zero
    uint8* m_pBlocks;           // +0x0c  0x80-byte block bitmap (freed after packing)
    uint32 m_nUsed;             // +0x10  texels placed
    uint32 m_MemUsage;          // +0x14  0x2000 (= 64*64*2)
    uint32 m_FrameCode;         // +0x18  Mac: last frame it was bound (texture-memory stat)
    GLuint m_TexName;           // +0x1c  Mac glGenTextures (PC: IDirectDrawSurface7*, 64x64, lightmap texture format)
    uint16*m_pPixels;           // +0x20  Mac only: 64x64 X1R5G5B5 system copy (PC: +0x20 dirty flag)
    uint32 m_Pitch;             // +0x24  Mac: 0x80 (PC: +0x24 next)
    uint32 m_bFlag;             // +0x28  Mac: init 1 ("page bound normally"; 0 = a per-poly LM surface is bound instead, C.7)
    LMPage*m_pNext;             // +0x2c  Mac
};
struct RenderContext { LMPage* m_Pages; uint32 m_nPages; MainWorld* m_pWorld; uint16 m_FrameCode /*0xffff*/; };
// Mac r_CreateContext 0x14a1f0: dalloc_z(0x10); +8 = init->[0]; +0xc = 0xffff.
// PC "render world" {pages, count, world} (w13:270).
```

### Algorithm

```c
bool r_CreateLMPages(RenderContext* ctx)            // Mac 0x19c3e0, PC 0x1006cc90
{
    for each world model wm (MainWorld+0x18c[i]->+4 = WorldBsp* bsp):
        for each leaf L in bsp+0x88 (0x30-byte Leaf, count +0x8c): L+0x2c (u16) = 0
        for each poly p in bsp+0xa0/+0xa4: p.flags14 &= 0xffc0
    base = world->FindLMAnim("LightAnim_BASE");
    if (!base || base->m_nFrames < 1) return false;           // -> no LM pages (Mac clears the LMPages-supported global)
    for i < base->m_nPolies:                                   // poly->+0x44 temporarily = its base frame entry
        if ((p = world->GetLMPoly(&base->m_Polies[i]))) p->m_pLMPage = &base->m_Frames[0][i];
    for each world model bsp:
        if (bsp+0x84 /*leaf/node count*/ > 0)
            for each leaf L: VisitPVS(bsp, L, cb = r_LeafCB)   // Mac: bsp vtable +0x30 with a VisitPVSRequest
                                                               // {leaf, bsp, DummyIterateLeaf, r_LeafCB, ..., AddObjectsNone};
                                                               // PC slot 9 "Traverse"
                // r_LeafCB(Leaf* l): for j < l+0x24: if (!r_AddToLMPage(ctx, l+0x20[j])) stop
        else
            for each poly: if (!r_AddToLMPage(ctx, p)) { destroy pages; return false; }
    free all page block maps                                   // r_DestroySlotTags
    for each world model, each poly p:
        if (!(p.flags14 & 0x3f)) p->m_pLMPage = NULL;          // not lightmapped
        else r_UpdatePolyLightmap(world, p, /*bForce*/1);      // decode/composite and write into the page
    Mac: for each page: FormatMgr 555 -> GL format; glBindTexture; MIN/MAG = GL_LINEAR;
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB or GL_RGBA, 64, 64, 0, fmt, type, converted)   (0x19c730..0x19c7fc)
    print "Lightmaps paged in %.1f seconds."
}

bool r_AddToLMPage(RenderContext* ctx, WorldPoly* p)
{
    if (!(p->surface->flags & 0x80)) return true;              // SURF_LIGHTMAP
    if (p->flags14 & 0x3f) return true;                        // already placed
    p->flags14 = (p->flags14 & 0xffc0) | 1;
    w = p->lmW; h = p->lmH;
    if (!FindFreeSlot(ctx, w, h, &x, &y, &page)) { page = CreateNewPage(ctx); x = y = 0; }
    if (x + w > 64 || y + h > 64) return false;
    mark every texel's block, page->m_nUsed += w*h;
    compute vertex lightmap UVs (C.4);
    p->lmX = x; p->lmY = y; p->m_pLMPage = page;
}

FindFreeSlot: for page in list:
    if (page->m_nUsed < w*h) continue;                         // sic: compares USED texels, not free ones
    for (y = 0; y < 65-h; y += 4) for (x = 0; x < 65-w; x += 4)
        if every block (x..x+w-1, y..y+h-1) step 4 is clear -> found
```

The `m_nUsed < w*h` test is in both binaries (Mac 0x19ba28; PC w13:391). Its effect: a brand-new page (used 0) is never searched again until it holds at least `w*h` texels. Packing is greedy in PVS-visit order (which keeps nearby polys on the same page). The order does not affect correctness, so any packer works for a new renderer.

`r_DestroyLMPages` (Mac 0x19c920) deletes each page's texture and pixels, then `r_ClearLMRenderData` (0x19c8b0) clears every `poly->m_pLMPage`. It is called from `r_DeleteContext` (0x14a308). PC: `sub_1006d130`.

---

## C.6 Building one poly's lightmap: `r_UpdatePolyLightmap(MainWorld*, WorldPoly*, bForce)`

Mac 0x19e1c0; PC `sub_1006be50` (w17:809). This is the compositor. Static lightmaps are simply the case "one ref, to `LightAnim_BASE`, frames 0/0, blend 1".

```c
bool r_UpdatePolyLightmap(MainWorld* w, WorldPoly* p, bool bForce)
{
    LMPage* page = p->m_pLMPage; if (!page) return false;
    page->m_bFlag = 1;
    if (p->nAnimRefs == 0) return true;
    n = p->lmW * p->lmH;  bDirect = bForce || cvar LMAnimStatic (Mac; PC: cvar at 0x10091220);
    uint16 accum[1024], layer[1024]; int done = 0; bool layered = false;
    for (i = 0; i < p->nAnimRefs; i++) {
        PolyAnimRef r = p->animRefs[i];
        if (r.m_iAnim >= w->nAnims) return false;
        LMAnim* a = &w->anims[r.m_iAnim];
        if (a->m_iFrames[0] >= a->m_nFrames || a->m_iFrames[1] >= a->m_nFrames) continue;  // light off
        if (a->m_fBlendPercent < 0.02f) continue;
        if (!r_SetupPolyLightmap(w, p, a, &r, layer)) return false;  // ShadowMap ? r_SetupPolyShadowMap : r_SetupPolyLMAnim
        layered = (i > 0);
        if (done == 0) memcpy(accum, layer, n*2);           // NOTE: the first layer is NOT scaled by its blend percent
        else r_AddLightmapPixels(accum, layer, a->m_fBlendPercent, n);
        done++;
    }
    if (!done) memset(accum, 0, n*2);
    if (bDirect)  { lightmap_blit(accum, {0,0,w,h}, w*2, page->m_pPixels, page->m_Pitch, p->lmX, p->lmY); return true; }
                  // PC: convert 555 -> device format into scratch surface, BltFast to (lmX,lmY) on the page surface
    if (!layered) { page->m_bFlag = 1; return true; }     // only the base layer: the page already holds it
    // Animated: upload into a private per-poly LM surface (C.7), page->m_bFlag = 0, p->flags14 |= 0x8000 (redo next frame)
}

// r_SetupPolyLMAnim (Mac 0x19dd30) == PC sub_1006bb30 (w17:709)
bool SetupPolyLMAnim(WorldPoly* p, LMAnim* a, PolyAnimRef* r, uint16* out)
{
    if (r->m_iPoly >= a->m_nPolies) return false;
    A = &a->m_Frames[a->m_iFrames[0]][r->m_iPoly];  B = &a->m_Frames[a->m_iFrames[1]][r->m_iPoly];
    if (a->m_iFrames[0] == a->m_iFrames[1] || a->m_fPercentBetween < 1/32.f) return Decompress(A, out);
    if (a->m_fPercentBetween > 31/32.f) return Decompress(B, out);
    Decompress(A, ta); Decompress(B, tb); t = (int)(a->m_fPercentBetween * 64);
    per texel, per 5-bit channel c: out = min(31, cA + (((cB - cA) * t) >> 6));
}

// r_AddLightmapPixels(dst, src, f, n) (Mac 0x19e0c0): s = (int)(f*64); per channel: dst = min(31, dst + ((src*s) >> 6))
```

### Shadow-map anims

`m_bShadowMap` anims go through `r_SetupPolyShadowMap` (Mac 0x19d8b0) / PC `sub_1006b500` (w17:571):
1. Decompress or blend the 8-bit layers the same way: `L = A + (((B-A)*t)>>6)`.
2. Light each texel by the anim's own light (pos +0x44, colour +0x50 as ints, radius +0x5c):
   ```
   if (L && d² < r²)
       i = (L * ramp[(int)((1 - d²/r²)*63)]) >> 8;
       c = (i * color16_16) >> 32   (MulHigh)
   else black
   ```
   then repack to 5:5:5 with clamp. `ramp`/`color16_16` are as for dynamic lights below.

### Static-only fast path

For a lightmap with exactly one ref to `LightAnim_BASE` (frames 0/0), `r_SetupPolyLMAnim` reduces to "decompress frame 0's entry". In other words: **static lightmap = `DecompressLMData(base->m_Frames[0][r.m_iPoly])`**, and that is enough to render the world.

### Dynamic lights on lightmaps

`r_LightData` (Mac 0x19cfd0) == PC `sub_1006ad20` (w17:359).

Walk `poly+0x2c`. Each node gives the light (`LTObject*` at +4; colour bytes at obj+0x94/0x95/0x96, radius at obj+0x1a8, flags at obj+0x88) and its position (node+8).

Setup (Mac 0x19cd10 / PC `sub_1006b300`):
- `color16 = (2*c - 255) << 16` per channel, so a colour below 128 darkens;
- `r2 = radius²`, `inv = 1/r2`.

Per texel at W(x,y):
- `d2 = |W - lightPos|²`
- if `d2 < r2`: `add = MulHigh(ramp[(int)((1 - d2*inv)*63)], color16)`;
- channel8 = `(r5<<3, g5<<3, b5<<3) + add`, clamped to 0..255 through the 768-entry clamp table (`r_InitClampTable` 0x19cca0 / PC 0x1006c4d0: 256 zeros, identity, 256 x 255; indexed `c+255`), then `>>3` back to 5:5:5.
- `ramp[i] = min(1, i/63 * LightSaturate) * 65536` (`r_InitSaturateTable(LightSaturate)` 0x19cbd0, called from `r_RenderScene`; cvar `LightSaturate`, default 1.0).

The flat branch (no attenuation, writes the light's 5:5:5 colour) is taken when cvar `FastLight` (Mac data+0x29118) is set or `obj+0x88 & 0x10`.

The lights are composited into a copy of the poly's base lightmap, read back from the page's system-memory pixels (`LMRenderSurface::Init` with copy=1, 0x19afd0..0x19b040, only when cvar `LightMap` is on, else black). The result is uploaded to a private per-poly texture (C.7). Each frame that dynamic lights touch the poly, the dual-pass/single-pass LM drawers call `r_SetupLitLightmap(poly, 0)` (Mac 0x182090; called from `r_DrawLMPoly` 0x1949e8 and `r_ReallyDrawLMDualPass` 0x197314).

---

## C.7 Runtime updates and the Mac's per-poly LM surfaces

### Animated lightmaps per frame

`r_ReallyDrawLMDualPass` (Mac 0x19729c..0x1972cc) does this when cvar `LMAnim` (default 1) is set and `poly.flags14 & 0x8000`:
1. clear the bit;
2. call `r_UpdatePolyLightmap(world, poly, 0)`;
3. if afterwards `page->m_bFlag == 0`, a freshly built private texture is bound on the LM unit: set `m_bFlag = 1` and draw with surface-relative UVs (C.4).

The PC instead converts the composite straight into the page surface through `CLightDesc` (w17:960) and keeps `m_bDirty` / bit 0x8000.

### `LMRenderSurfaceMgr` (Mac only, `lightmap_surface.cpp`)

- 5 size classes, `{4, 8, 16, 32, 64}` (data+0x214a0).
- Each class has a round-robin ring of 2 textures (data+0x214b4) plus an "upload surface" (system memory, `r_CreateLightmapTexture` as the factory; `CreateSurfaces` 0x19b4b0).
- `LMRenderSurface::Init(poly, bCopyBase, w, h)` (0x19aea0) picks the smallest class ≥ (w, h).
- `Term(bUpload)` (0x19b0d0) does the following, then sets `texScale[lmUnit] = {1/size, 1/size}`:
  1. converts 555 to the GL format;
  2. `glTexSubImage2D(0, 0, w, h)` into the ring texture;
  3. sets `GL_LINEAR`.

### Recommendation for a new GL renderer (inferred, not engine behaviour)

Keep one RGB8 (or RGBA8) atlas texture per page. For a dynamic or animated change:
1. recomposite the poly's 5:5:5 data on the CPU;
2. `glTexSubImage2D` it at (lmX, lmY) in its page;
3. restore the base lightmap the next frame the poly is not lit.

This keeps one UV set (the page UVs) and drops the private-surface special case.

---

## C.8 Drawing hookup (Part E owns the passes)

In `r_DrawLMPoly` (Mac 0x1947f0) and its dual-pass twin:
- `poly->m_pLMPage == NULL` → `r_QueueFlatPoly(poly)`: no lightmap, drawn flat (0x194e30..0x194f3c).
- Otherwise bind `page->m_TexName` on the lightmap texture unit (`glActiveTextureARB(GL_TEXTURE0 + lmUnit)`, unit index at data+0x1fde4) unless it is already bound or `m_bFlag == 0`. The page also gets `m_FrameCode = current`; when that changes, `+= m_MemUsage` is added to the texture-memory statistic.

The vertex sent is `LMVertex` (0x38 bytes):
- +0x00: xyz
- +0x0c: rgba = 0xffffffff
- +0x18/+0x1c: base uv = vertex+4/+8 × `texScale[0]` (texels → 0..1)
- +0x28/+0x2c: lightmap uv = vertex+0x0c/+0x10 (already 0..1 on the page)

It is drawn as `GL_TRIANGLE_FAN` (`r_DrawGLPrimitive(6, …)`, 0x195120).

Lightmap texture filtering is `GL_LINEAR` min and mag, with no mipmaps (0x19c77c..0x19c798).

---

## C.9 Console variables touching lightmaps (Mac, `__sinit_*` ConVar constructors)

| name | default | where read / effect |
|------|---------|----------|
| `LightMap` | 1 | `r_SetPolyFunctions` / `r_DrawPortals`; `LMRenderSurface::Init` (copy base lightmap) |
| `NoLMPages` | 0 | `r_CreateContext`: if set, print "Warning: NoLMPages is TRUE" and build no pages (0x14a238) |
| `LightmapsOnly` | 0 | `r_SetSinglePassLMStates`, `r_DrawOverlayPolies` |
| `LMFullBright` | 0 | `r_ReallyDrawLMDualPass`, `r_DrawOverlayPolies` |
| `LMAnim` | 1 | per-frame animated-lightmap updates (C.7) |
| `LMAnimStatic` | 0 | force direct-into-page compositing in `r_UpdatePolyLightmap` |
| `LightSaturate` | 1.0 | ramp table for dynamic lights |
| `FastLight` | 0 | flat (unattenuated) dynamic lights on lightmaps |
| `DynamicLight` | 1 | (Part E) |
| `LMDynamic` | 1 | gouraud and panning-sky dynamic lights (`r_RenderScaledLightmap`, `r_DrawGouraudDynamicLight`). Not world lightmaps |
| `LMDynamicScale` | 2.5 | same |
| `LMDynamicSize` | 16 | same |
| `LMUOffset` | 0 | declared, unused |
| `LMVOffset` | 0 | declared, unused |

`r_CreateContext` additionally needs the "LM pages supported" global (Mac data+0x2a108), which `r_CheckCapabilities` always sets to 1 (0x149d6c). If `r_CreateLMPages` fails, it prints a debug message and clears that global (0x14a26c..0x14a290).

---

## C.10 Minimal recipe for an OpenGL implementation

1. At world bind (CreateWorld):
   - `base = FindLMAnim("LightAnim_BASE")`;
   - for each ref i: `poly = GetLMPoly(base->m_Polies[i])`, `pixels555 = Decompress(base->m_Frames[0][i])` (`lmW*lmH` texels);
   - polys whose surface lacks flag 0x80 have no lightmap.
2. Pack each lightmapped poly's `lmW x lmH` rectangle into 64x64 (or larger) atlases, recording (atlas, x, y).
3. Convert 5:5:5 → RGB8 (`c*255/31`) and upload with `GL_LINEAR` and no mipmaps (clamp-to-edge recommended).
4. Per vertex:
   ```
   P, Q = SetupLMPlaneVectors(surface->flags >> 29, surface->plane->normal)
   d = pos - poly->LMOrigin
   lmU = (dot(d,P)/grid + 0.5 + x) / atlasW
   lmV = (dot(d,Q)/grid + 0.5 + y) / atlasH
   ```
   with `grid = MainWorld+0xf8`.
5. Draw base × lightmap (modulate; Part E has the exact blend/combine modes and overbright handling).
6. Optional:
   - animated lights: recomposite per C.6 when `poly.flags14 & 0x8000`, `glTexSubImage2D` the rectangle;
   - dynamic lights per C.6 (or replace with a shader).

---------------------------------------------------------------------------


# Part D — Visibility, world models, sky

Sources: Mac `LithTech` PEF (named, PowerPC, OpenGL renderer linked in), disassembled with a
skipdata wrapper (stock `ext/tools/pef.py dis` stops at the first `fcmpo`/`fcmpu`, so long
functions come out truncated). PC d3d.ren equivalents come from `src/engine/d3d/*.cpp`. "Mac
0xNNNNNN" is a code-section offset in the Mac binary. "PC 0x10NNNNNN" is a d3d.ren address.
Where a claim is inferred rather than read off the code, it says so.

Offsets are the same on Mac and PC for every structure checked here: VisibleSet,
Leaf, Poly/Surface fields, LTObject fields, SceneDesc, and the sky ramp all cross-check between
`d3dqip4.cpp`/`d3dr3b.cpp` and the Mac disassembly.

Tooling notes: `pef.py funcs` also drops names that contain `<...>` (for example
`r_RouteWorldModelPolies__…P11_CVector<f>Ul` at Mac 0x1710d0, `FindPointInTree` at Mac 0xc7ac0,
`gr_SetupWMTransform` at Mac 0xb6600, `r_InitFrustrum`/`r_InitFrustrum2` at Mac 0x14fa70/0x14fc10).
Find them by scanning the code section for the mangled string, which sits in the traceback
table just after the function's `blr`.

---------------------------------------------------------------------------

## 0. Globals and fields referenced throughout

| Mac global | PC global | meaning |
|---|---|---|
| data+0x2920c `g_ViewParams` (struct) | `g_1008b880` D3DXForm (see big1ef30.cpp) | current view |
| data+0x2983c `g_pSceneDesc` | `g_1008beb0` | SceneDesc of this frame |
| data+0x29838 `g_pWorld` (MainWorld*) | `g_1008beac` | = RenderContext+0x08 |
| data+0x29834 `uint16 g_CurFrameCode` | `g_1008bea8` | poly/leaf frame code = RenderContext+0x0c |
| data+0x29830 `uint32 g_ObjectFrameCode` | `g_1008bea4` | object frame code (from the engine's RenderStruct callback) |
| data+0x2ac80 `g_VisibleSet` | `g_10091a10` | the visible set (§1.4) |
| data+0x29200 `uint32 g_ClipFlags` | `g_1008b874` | bit i set = must clip against frustum plane i |
| data+0x29998 / 0x29994 / 0x2999c / 0x299a0 | `g_1008ce98 / ce94 / ce9c / cea0` | per-poly draw functions (§1.6) |
| data+0x2a110 `g_bDrawPortals` | `g_1008d958` | `DrawPortals` cvar && `r_CanDrawPortals()` (stencil present), set in r_RenderScene Mac 0x157644 |
| data+0x2b674 `g_SkyViewParams` | `g_1008ff60` | ViewParams used for the sky pass |
| data+0x2afb0 `float g_WMColorScale[3]`, data+0x297ec `uint8 g_WMAlpha` | — | world-model tint (§2.4) |

ViewParams fields used here (Mac `g_ViewParams`; part A owns the full layout):
`+0x15c` world→camera 4x4 (rows at 0x15c/0x16c/0x17c/0x18c), `+0x19c` projection 4x4,
`+0x1dc` their product, `+0x21c` matrix passed with portals, `+0x31c..+0x330` software
projection scale/offset terms (used by `r_DrawSkyPoly`), `+0x3d0` **six frustum planes**
(LTPlane {N.xyz, D}, 16 bytes each, world space), `+0x4b4` camera position (world),
`+0x4c0` sky-view position (§3.2), `+0x4d4` "drawing a portal view" flag, `+0x4d8` the
portal view's eye, `+0x4e4` the volumetric-fog reference point (`0x176190` sets it).

Frustum test convention (r_NodeFilter Mac 0x16f720, r_VLTagPolies Mac 0x16fa90, PC
`DrawQueue1007b580` d3dqip4.cpp:720-760): `d = N·P − D`. The sphere is rejected if
`d < −radius`, and plane i's clip bit is cleared if `d > radius`. The planes face inward.

Surface flag bits used in this part (Part B owns the full table, and the names here follow LT
de_world.h): 0x4 INVISIBLE (skip), 0x8 TRANSLUCENT (world models go to the translucent
set), 0x10 SKY, 0x80 LIGHTMAP, 0x8000 PANNINGSKY, 0x80000 additive (the
`(flags>>19)&1` argument to r_SetTranslucentObjectStates, Mac 0x17999c).

---------------------------------------------------------------------------

## 1. Visibility of the main world

### 1.1 Data structures

```c
// WorldBsp (Part B owns the full layout; these are the fields the vis code touches)
struct WorldBsp {
    void*      vtbl;                 // Mac slots: +0x08 GetAbstractBspType (0=WorldBsp, 1=TerrainSection),
                                     //            +0x0c GetRootNode, +0x28 GetRenderClipRadius, +0x24 IsVisContainer
    /* ... */
    LTPlane*   m_Planes;             // +0x68  (16-byte LTPlane), count +0x6c
    Node*      m_Nodes;              // +0x70  0x18-byte Node records, count +0x74
    /* ... */
    Leaf*      m_Leafs;              // +0x88  0x30-byte Leaf records, count +0x8c
    WorldPoly** m_LeafPolyRefs;      // +0x90  backing store of every Leaf::m_Polies
    Node*      m_RootNode;           // +0x9c
    WorldPoly** m_Polies;            // +0xa0  all polys, count +0xa4
    LTVector*  m_Points;             // +0xa8  vertex positions, count +0xac
    UserPortal* m_UserPortals;       // +0xb0  0x24-byte records
    LTVector   m_WorldTranslation;   // +0xe8  (world models: the authored origin, §2.2)
    uint8*     m_PVSData;            // +0x104 blob all LeafList::m_pBits point into
};
// Evidence: VisitPVS Mac 0xe5b90 (+0x70/+0x74/+0x88/+0x8c/+0x9c/+0xb0), r_DrawBSP Mac 0x16f4c0
// (+0x70/+0x74/+0xa0/+0xa4), r_ResetWorldFrameCode Mac 0x14ef90, w_TransformWorldModel Mac
// 0xd7700 (+0x68/+0x6c/+0xa0/+0xa8/+0xac), w_LoadWorldBsp Mac 0xd5090.. (+0x88/+0x90/+0x104).

struct Node {                        // 0x18 bytes
    WorldPoly* m_pPoly;              // +0x00  splitting poly; plane = m_pPoly->m_pSurface->m_pPlane (+0x28 → +0x34)
    Node*      m_Sides[2];           // +0x04 [0] = BACK half-space (N·p−D < 0), +0x08 [1] = FRONT
    CheapLTLink m_Objects;           // +0x0c  objects filtered into this node (prev,next)
    uint16     m_iLeaf;              // +0x14  index into m_Leafs, 0xFFFF = none
    uint8      m_Flags;              // +0x16  &3 != 0 → terminal pseudo-node (NF_IN=1 / NF_OUT=2: stop);
                                     //        &8 → "subtree is in the PVS stream" (§1.2)
};
// Side convention: FindPointInTree (Mac 0xc7ac0) goes to m_Sides[ (d >= 0) ? 1 : 0 ]
// (offset 4 + 4*(d>=0), Mac 0xc7b64..0xc7b74). GetBacksideObjects (Mac 0xe66e0, keeps
// d < r+0.1) feeds m_Sides[0].

struct LeafList {                    // 8 bytes
    uint16 m_iPortal;                // +0x00 index into WorldBsp::m_UserPortals (list 0 is unconditional)
    uint16 m_nBytes;                 // +0x02
    uint8* m_pBits;                  // +0x04 → WorldBsp::m_PVSData + running offset
};

struct Leaf {                        // 0x30 bytes
    uint8      unk00[0x10];          // not touched by the renderer
    LeafList*  m_LeafLists;          // +0x10
    LTLink     m_Objects;            // +0x14 prev,next(+0x18),data(+0x1c); renderer walks .next; link->m_pData→ +0x18 = LTObject*
    WorldPoly** m_Polies;            // +0x20
    uint32     m_nPolies;            // +0x24
    float      unk28;                // +0x28 read from file, not used by renderer
    uint16     m_FrameCode;          // +0x2c
    uint16     m_nLeafLists;         // +0x2e
};
// Evidence: w_LoadWorldBsp Mac 0xd5090..0xd52b0 (file: u16 nLists, or 0xFFFF + u16 leaf to SHARE
// another leaf's lists; then per list u16 portal, u16 size, bytes; then u32 nPolyRefs + pairs of u16;
// then a float). r_ProcessLeaf Mac 0x16e900 and PC Leaf1007a7b0 (d3dqip4.cpp:499) for
// +0x14/+0x20/+0x24/+0x2c.

struct UserPortal {                  // 0x24 bytes
    uint8    unk00[6];
    uint16   m_Flags;                // +0x06 bit0 = open (list is used only if set; VisitPVS Mac 0xe5c1c)
    uint8    unk08[4];
    LTVector m_Center;               // +0x0c
    LTVector m_Dims;                 // +0x18 half extents
};   // r_IsPortalVisible Mac 0x14f4b0; PC sub_1001e770 (d3dr5d_os.cpp:1102) reads the same +0x0c/+0x18

// WorldPoly fields used here (Part B): +0x18 LTVector m_Center, +0x24 float m_Radius,
// +0x28 Surface* m_pSurface, +0x2c dynamic-light link list, +0x42 uint16 m_FrameCode,
// +0x4c/+0x52 alt (T-junction-fixed) vertex list/count, +0x50 uint16 nVerts, +0x54 vertices[] (0x18 each:
// +0 LTVector* pos, +4 u, +8 v, +0x14..0x16 colour bytes b,g,r).
// Surface fields: +0x30 SharedTexture* (NULL = untextured/flat), +0x34 LTPlane*, +0x38 uint32 flags,
// +0x42 uint16 portal-view index (0x7FFF = none; set by the "Portal" surface effect, InitPortal Mac 0x335c0).
```

`PC d3dqip4.cpp` calls `Surf1007aeb4::m_wLMAnim42`, the 32-entry table "LM anim
entries" and the queue at +0x28 "m_Translucent". The Mac names (CodeWarrior keeps them) correct
all three. Surface+0x42 is a **portal-view index** (`InitPortal` stores
`w_FindPortalView` there, and the consumer is `VisibleSet::AddPortal`). The +0x28 queue is the
**sky-poly list**: it receives SURF_SKY (0x10) polys and is read by `r_CalcSkyExtents`.

### 1.2 PVS: a pruned-tree bitstream

`VisitPVS(VisitPVSRequest*)` (Mac 0xe5b90; `RecurseAndCallback` Mac 0xe60c0 is the recursive
half). This is **engine** code. The renderer reaches it through virtuals on engine objects
(§1.3). An implementation can either call the same virtual (PC slot below) or re-implement it from
this description.

```c
struct VisitPVSRequest {             // built in WorldModelInstance::DoVisQuery, Mac 0xc9010
    Leaf*     m_pViewLeaf;           // +0x00 FindPointInTree(bsp, &viewpoint)
    WorldBsp* m_pBsp;                // +0x04
    void (*m_ObjectCB)(LTObject*, void*);              // +0x08
    void (*m_LeafCB)(Leaf*, void*);                    // +0x0c
    void (*m_AddObjects)(CheapLTLink*, LTObject***, int*); // +0x10 default AddObjectsAll
    BOOL (*m_PortalTest)(UserPortal*);                 // +0x14
    void*     m_pUser;               // +0x18
    uint32    m_FrameCode;           // +0x1c
};

Leaf* FindPointInTree(WorldBsp* bsp, LTVector p) {      // Mac 0xc7ac0
    uint16 leaf = 0xFFFF; Node* n = bsp->m_RootNode;
    while (!(n->m_Flags & 3)) {
        if (n->m_iLeaf != 0xFFFF) leaf = n->m_iLeaf;     // remember the deepest leaf on the path
        n = n->m_Sides[ Dot(plane(n).N, p) - plane(n).D >= 0 ];
    }
    return leaf == 0xFFFF ? NULL : &bsp->m_Leafs[leaf];
}

// bit reader: one global bit index, LSB-first within each byte
#define NEXTBIT() ((bits[idx>>3] >> (idx&7)) & 1, idx++)

void VisitPVS(VisitPVSRequest* r) {
    Leaf* vl = r->m_pViewLeaf;
    if (!vl || vl->m_nLeafLists == 0) {                  // fallback: every leaf
        for each node in bsp->m_Nodes: if node.m_iLeaf != 0xFFFF → LeafCB(&leafs[iLeaf]);
            + objects of node via AddObjects/ObjectCB
        return;
    }
    for (i = 0; i < vl->m_nLeafLists; i++) {
        LeafList* L = &vl->m_LeafLists[i];
        if (i > 0) { UserPortal* up = &bsp->m_UserPortals[L->m_iPortal];
                     if (!(up->m_Flags & 1) || !r->m_PortalTest(up)) continue; }
        bits = L->m_pBits; idx = 0;
        Walk(bsp->m_RootNode, objects = none);
    }
}
void Walk(Node* n, objs) {                               // iterative in the original
    r->m_AddObjects(&n->m_Objects, &objs, &nObjs);       // gather objects attached here
    if (n->m_iLeaf != 0xFFFF && NEXTBIT()) {
        if (n->m_iLeaf < bsp->nLeafs) r->m_LeafCB(&bsp->m_Leafs[n->m_iLeaf], r->m_pUser);
        for each obj in objs: if (obj->m_FrameCode58 != r->m_FrameCode) { stamp; r->m_ObjectCB(obj, user); }
        objs = none;
    }
    if ((n->m_Sides[0]->m_Flags & 8) && NEXTBIT())       // back child
        Walk(n->m_Sides[0], objects with N·pos − D < radius+0.1);   // GetBacksideObjects
    if ((n->m_Sides[1]->m_Flags & 8) && NEXTBIT())       // front child
        Walk(n->m_Sides[1], objects with N·pos − D > −(radius+0.1)); // GetFrontsideObjects
}
```
The bit order within each node is fixed: the leaf bit first, then the back-child bit, then the
front-child bit. A child's bit is present only if that child has flag 8, and the bits of a subtree
are present only when its bit was 1 (Mac 0xe5c7c..0xe5e70, 0xe6108..0xe6688). For rendering,
only `m_LeafCB` matters; objects reach the renderer separately (§2.1).

### 1.3 The renderer's frame walk

Call chain (Mac): `r_RenderScene` → `r_FullDrawScene` (Mac 0x154780) → for DrawMode 1 (normal):
`r_StartDrawWorld(0); r_TagVisibleLeaves(); r_EndDrawWorld(0)`, then `r_FlushObjectQueues()`.
DrawMode 2 instead calls `r_ProcessObjectList(scene+0xdc, scene+0xe0)` (Mac 0x1547ac..0x1547cc).

```c
void r_TagVisibleLeaves() {                     // Mac 0x16fc40, PC 0x1007b880
    if ((g_pWorld->m_Flags180 & 2) && !DrawAll) r_VLTagPolies(FALSE);   // PVS path
    else                                        r_VLTagPolies(TRUE);    // draw everything
}
void r_VLTagPolies(BOOL bAll) {                 // Mac 0x16f860, PC 0x1007b580 (d3dqip4.cpp:778)
    if ((g_VisibleSet.m_Flags0c & 1) && !LockPVS) {          // LockPVS freezes the set
        g_VisibleSet.ClearSet();                              // Mac 0x16f460
        if (bAll) r_DrawBSP(g_pWorld->GetVisBSP());           // Mac vtbl +0x14, PC vtbl +0x08
        else {
            VisQueryRequest q = {0};
            q.m_iObjArray = 0;
            q.m_Viewpoint = VP.portalView(+0x4d4) ? VP+0x4d8 : VP+0x4b4;
            q.m_ViewRadius = 10000.0f;
            q.m_ObjectCB = r_ProcessObject; q.m_AddObjects = r_AddRenderObjects; q.m_pUser = 0;
            q.m_LeafCB = r_ProcessLeaf; q.m_PortalTest = r_IsPortalVisible; q.m_NodeFilter = r_NodeFilter;
            g_pWorld->m_WorldTree(+0x6c)->DoVisQuery(&q);      // Mac vtbl +0x20, PC vtbl +0x14
        }
    }
    r_DrawSky();            // Mac 0x177130 / PC 0x1005f580 — BEFORE the world polys (§3)
    r_DrawLights();         // Mac 0x176c30 / PC 0x10040dd0
    if (DrawWorld) for each poly in g_VisibleSet.m_Polies {            // §1.5
        if (Dot(plane.N, campos) − plane.D <= 0.01f) continue;         // back face (camera = VP+0x4b4)
        g_ClipFlags = 0x3f;
        for i in 0..5: d = planes[i]·poly.m_Center − D;
                       if (d < −poly.m_Radius) goto next; if (d > poly.m_Radius) g_ClipFlags &= ~(1<<i);
        WorldPoliesProcessed++;
        DispatchPoly(poly);                                             // §1.6
    next:;
    }
}
```

`VisQueryRequest` layout (Mac stack frame at r1+0x50 in r_VLTagPolies, and DoVisQuery Mac 0xe9530;
PC `Visit1007b580` d3dqip4.cpp:665):
```c
struct VisQueryRequest {
    uint8    m_iObjArray;      // +0x00 which node object array (node + 8*i)
    LTVector m_Viewpoint;      // +0x04
    float    m_ViewRadius;     // +0x10
    void (*m_ObjectCB)(WorldTreeObj*, void*);            // +0x14
    void (*m_AddObjects)(LTLink*, LTObject***, int*);    // +0x18
    void*    m_pUser;          // +0x1c
    void (*m_LeafCB)(Leaf*, void*);                      // +0x20
    BOOL (*m_PortalTest)(UserPortal*);                   // +0x24  (PC comment calls this "node")
    BOOL (*m_NodeFilter)(WorldTreeNode*);                // +0x28
    LTVector m_ViewMin, m_ViewMax;   // +0x2c/+0x38, = viewpoint ∓ radius (filled by DoVisQuery)
    WorldTree* m_pTree;        // +0x44 (filled by DoVisQuery)
};
```
PC check: 0x1001e770 reads +0x0c/+0x18 (a UserPortal's centre and dims), so it is
`r_IsPortalVisible` at +0x24. 0x1007b520 reads +0x30/+0x40 (a WorldTreeNode's centre and radius), so it is
`r_NodeFilter` at +0x28.

`WorldTree::DoVisQuery` (Mac 0xe9530) recurses through the object quadtree (`VisQuery_R`,
Mac 0xe7860). WorldTreeNode: +0x00/+0x08 object lists (CheapLTLink, chosen by m_iObjArray),
+0x10 vis-container list, +0x18 min, +0x24 max, +0x30 centre, +0x40 radius, +0x48 parent,
+0x4c..+0x58 four children (split in X/Z around centre.x/centre.z) (ctor Mac 0xe7de0,
VisQuery_R 0xe7a58..). The steps:
1. `if (!m_NodeFilter(node)) return;`, where `r_NodeFilter` is the sphere (centre +0x30, radius +0x40) against the 6 planes.
2. If the node's vis-container list is non-empty, call `obj->DoVisQuery(req)` (Mac vtbl +0x24) for each container whose frame
   code (+0x58) differs from the tree's, and return. The main world is such a container: a
   `WorldModelInstance` whose DoVisQuery (Mac 0xc9010) builds the VisitPVSRequest from the
   request's callbacks and calls `VisitPVS` (§1.2).
3. Otherwise each object in list[m_iObjArray] whose bbox (vtbl +0x14) overlaps
   [ViewMin,ViewMax] gets `m_ObjectCB`. Then recurse into the children that overlap the box.
4. Afterwards, `AddListToVisibility(tree+0x74)` calls the ObjectCB for the always-visible objects.

### 1.4 VisibleSet (Mac data+0x2ac80, PC 0x10091a10)

```c
struct PolyArray { void* vtbl; WorldPoly** m_pArray; uint32 m_nAlloc; uint32 m_nCache;
                   uint32 m_nWantedCache; uint32 m_nUsed; };          // CMoArray + use count, 0x18
struct AllocSet  { LTObject** m_pArray; uint32 m_nUsed; CheapLTLink m_Link; void* self;
                   uint32 m_nMax; const char* m_pName; };              // 0x20
struct VisibleSet {
    CheapLTLink m_Sets;          // +0x000 list of AllocSets (counts zeroed by ClearSet)
    uint32    m_Flags0c;         // +0x00c ctor sets 1; bit0 = rebuild each frame
    PolyArray m_Polies;          // +0x010 normal world polys (used count +0x24)
    PolyArray m_SkyPolies;       // +0x028 SURF_SKY polys (used +0x3c)
    struct { WorldPoly* pPoly; LTMatrix* pMat; } m_Portals[32]; // +0x040 portal-view polys
    uint32    m_nPortals;        // +0x140
    AllocSet  m_Models, m_ModelsTranslucent, m_ModelsChromakey;           // +0x144 +0x164 +0x184 (max 300)
    AllocSet  m_Sprites, m_SpritesNoZ;                                    // +0x1a4 +0x1c4 (128)
    AllocSet  m_WorldModels, m_WorldModelsTranslucent, m_WorldModelsChromakey; // +0x1e4 +0x204 +0x224 (64)
    AllocSet  m_Lights;                                                   // +0x244 (40)
    AllocSet  m_PolyGrids, m_PolyGridsTranslucent, m_LineSystems;         // +0x264 +0x284 +0x2a4 (32)
    AllocSet  m_ParticleSystems;                                          // +0x2c4 (64)
    AllocSet  m_Canvases, m_CanvasesTranslucent;                          // +0x2e4 +0x304 (32)
};  // VisibleSet::Init Mac 0x16f1c0 (names "VS_MODELS"…), ctor 0x16efd0, AddPortal 0x16eb70
```

### 1.5 Leaf callback: collecting polys

```c
void r_ProcessLeaf(Leaf* leaf, void*) {          // Mac 0x16e900, PC 0x1007a7b0
    leaf->m_FrameCode = g_CurFrameCode; VisibleLeaves++;
    for link in leaf->m_Objects (via .next): obj = ((WorldTreeObjRef*)link->m_pData)->obj(+0x18);
        if (obj->type(+0xbc) == OT_POLYGRID(7) && (obj->flags & FLAG_VISIBLE) && obj->frame(+0x58) != g_ObjectFrameCode)
            { stamp; r_ReallyProcessObject(obj); }     // polygrids hang off leaves
    for each p in leaf->m_Polies[0..m_nPolies): QueuePoly(p);
}
void QueuePoly(WorldPoly* p) {                    // identical inline copy in r_DrawBSP and PC QueuePoly1007aeb4
    if (p->m_FrameCode == g_CurFrameCode) return;       // dedupe (polys are shared between leaves)
    s = p->m_pSurface;
    if (!(s->flags & 4 /*INVISIBLE*/)) {
        if ((s->flags & 0x10 /*SKY*/) && !DrawFlat)  g_VisibleSet.m_SkyPolies.add(p);
        else if (s->m_iPortal42 != 0x7FFF && g_bDrawPortals) g_VisibleSet.AddPortal(p, &VP+0x21c); // ≤32, deduped
        else g_VisibleSet.m_Polies.add(p);
    }
    p->m_FrameCode = g_CurFrameCode;
}
```
`r_DrawBSP(WorldBsp*)` (Mac 0x16f4c0, PC 0x1007aeb0) is the DrawAll path. It walks every node's object
list (tail-first, and it stops at an object whose +0x188 is non-zero) through `r_CheckAndProcessObject`, then calls
`QueuePoly` on every poly in `bsp->m_Polies`.

Frame codes: `r_IncrementFrameCode(RenderContext*)` (Mac 0x14f060) increments the context's
uint16 at +0x0c. When the value reaches 0xFFFF it restarts at 1 and `r_ResetWorldFrameCode` (Mac 0x14ef90) zeroes every
Leaf+0x2c and WorldPoly+0x42 in every BSP of `MainWorld+0x18c[i]->+4` (count `MainWorld+0x190`).
`r_InitFrame` (Mac 0x151590..0x15159c) copies it to `g_CurFrameCode`. The object frame code comes
from the engine each frame (RenderStruct callbacks, Mac 0x1515b0..0x1515c8).

### 1.6 Per-poly dispatch

The poly loop (and the world-model loop, §2.3) call one of four function pointers, chosen by
`r_SetPolyFunctions` (Mac 0x1539b0, called from r_StartDrawWorld; Part E owns the details):
```
if (!surf->m_pTexture)          g_fnFlat(p)        // data+0x29998: r_QueueFlatPoly
else if (surf->flags & 0x80)    g_fnLightmap(p)    // data+0x2999c: r_QueueLMSinglePass | r_DrawLMDualPass
else if (surf->flags & 0x8000)  g_fnPanSky(p)      // data+0x299a0: r_QueuePanningSkyPoly | r_QueueLMSinglePass
else                            g_fnNormal(p)      // data+0x29994: r_QueueGouraudPoly
```
`r_EndDrawWorld` (Mac 0x1545e0) flushes the queues: `r_DrawQueuedLightmapPolies`,
`r_DrawPanningSkyPolies`, then the LM polies (single- or dual-pass), the gouraud polies and the flat polies.

### 1.7 User-portal test

```c
BOOL r_IsPortalVisible(UserPortal* up) {          // Mac 0x14f4b0, PC 0x1001e770
    build the 8 corners of box(up->m_Center ± up->m_Dims);
    for each of the 6 frustum planes: if every corner has N·c − D <= 0 → return FALSE;
    return TRUE;
}
```

### 1.8 Portal views ("mirrors"/cameras, optional)

Polys whose Surface+0x42 != 0x7FFF are collected into `m_Portals` (with the matrix of the model they
belong to: `&VP+0x21c` for the world, `&wmi->m_Transform` for world models). They are not drawn in the
normal passes. `r_FlushObjectQueues` calls `g_pfnDrawPortalsCB` (data+0x2ac7c, set to
`r_DrawPortalsCB` only around r_FullDrawScene in r_RenderScene Mac 0x157878..0x15788c), which
runs `r_DrawPortals(scene, visibleSet)` (Mac 0x154db0). It uses stencil (glStencilFunc/Op): it writes
the portal poly into the stencil buffer, re-renders the scene with `r_FullDrawScene` from the
portal-view camera with `VP+0x4d4` set (this flag is what the FLAG2_PORTALINVISIBLE /
FLAG_PORTALVISIBLE tests react to), and then calls `r_OverlayPortalPoly`. The feature requires `r_CanDrawPortals()` (the stencil
byte at Mac data+0x282f4). It is safe to skip in a first implementation: return FALSE from the
can-draw test and AddPortal is never used. In that case portal surfaces fall into `m_Polies` and draw normally.

---------------------------------------------------------------------------

## 2. World models (moving brushes: doors, etc.)

### 2.1 How they reach the renderer

World models are LTObjects of type OT_WORLDMODEL (2) in the world tree. The VisQuery calls
`r_ProcessObject(obj)` (Mac 0x16e4b0, PC 0x1007a680) for each one:
```c
visible = g_ObjHandlers[type].process (data+0x2045c, 0x14-byte entries, fn at +0x10) && type != 7
       && !(flags2(+0x8c) & FLAG2_SKYOBJECT 0x40)
       && ( (flags(+0x88) & FLAG_VISIBLE) && (!VP.portalView || !(flags2 & FLAG2_PORTALINVISIBLE 1))
         || !(flags & FLAG_VISIBLE) && VP.portalView && (flags & FLAG_PORTALVISIBLE 0x400) );
if (visible) r_ReallyProcessObject(obj);   // → handler[type].process(obj); then attachments (+0x98)
```
For a world model the handler is `r_ProcessWorldModel` (Mac 0x16ff60), which puts the object into a set:
```c
sflags = (obj->m_pWorldBsp && obj->vtbl[+0x40]() /*Mac slot; "has BSP data", inferred*/ && bsp->nPolies)
         ? bsp->m_Polies[0]->m_pSurface->flags : 0;          // = r_GetFirstSurfaceFlags (Mac 0x172db0)
if (sflags & 0x8 /*TRANSLUCENT*/)          VS.m_WorldModelsTranslucent.add(obj);
else if (obj->flags2 & FLAG2_CHROMAKEY 2)  VS.m_WorldModelsChromakey.add(obj);
else                                       VS.m_WorldModels.add(obj);
```
A set that overflows only prints "Set '%s' overflowed".

`r_FlushObjectQueues` (Mac 0x16a450) draws in this order: solid world models → solid models →
solid polygrids → solid canvases → (chromakey states) chromakey models, chromakey world models →
portal callback → (translucent states) translucent particles, polygrids, line systems,
**translucent world models**, translucent models, canvases, sprites → `ObjectDrawList::Draw`
(sorted, see below) → no-Z sprites. The `DrawWorldModels` cvar (Mac data+0x2afbc, default 1) gates all
three world-model sets.

Translucent sorting (Mac `ObjectDrawList`, 0x16aab0/0x16a860/0x16ac40): when the cvar `DrawSorted`
(Mac data+0x2ac3c) is non-zero, `r_DrawTranslucentWorldModels` (Mac 0x172bf0) queues each object
with its draw function. Additive ones (first-surface flag 0x80000) use
`r_DrawAdditiveTranslucentWorldModel`; the others use `r_DrawTranslucentWorldModel`. The key is
|obj.pos − campos|² (or |obj.pos|² for FLAG_REALLYCLOSE 0x40, whose positions are camera-relative).
A shell sort orders them far→near, with REALLYCLOSE objects last. When DrawSorted is 0 they are
drawn immediately.

### 2.2 WorldModelInstance fields and transform

```c
// LTObject / WorldModelInstance (offsets equal on PC and Mac)
+0x058 uint32   m_FrameCode (WorldTreeObj)
+0x088 uint32   m_Flags        (FLAG_VISIBLE 1, FLAG_REALLYCLOSE 0x40, FLAG_FOGDISABLE 0x80, FLAG_PORTALVISIBLE 0x400, FLAG_BOXPHYSICS 0x4000)
+0x08c uint32   m_Flags2       (PORTALINVISIBLE 1, CHROMAKEY 2, SKYOBJECT 0x40)
+0x094 uint8    m_ColorR, G(+0x95), B(+0x96), A(+0x97)
+0x098          attachments
+0x09c LTRotation m_Rotation
+0x0bc uint8    m_ObjectType
+0x18c LTVector m_Pos
+0x1a4 float    vis radius (used by the PVS object split)
+0x1a8 WorldBsp* m_pWorldBsp   // the BSP the RENDERER draws (authored coordinates)
+0x1ac WorldBsp* m_pTransformedBsp // world-space copy kept by w_TransformWorldModel for physics
+0x1b4          vis-container links (0xc each)
+0x1f0 LTMatrix m_Transform    // model → world
+0x230 LTMatrix m_BackTransform // world → model
```
`obj_SetupWorldModelTransform` (Mac 0xe3750) → `gr_SetupWMTransform(&bsp->m_WorldTranslation
(+0xe8), &m_Pos, &m_Rotation, &m_Transform, &m_BackTransform)` (Mac 0xb6600):
`m_Transform = Translate(m_Pos) · RotationToMatrix(m_Rotation) · Translate(−bsp->m_WorldTranslation)`,
and `m_BackTransform = Mat_InverseTransformation(m_Transform)`. The engine calls it whenever the model
moves (`RetransformWorldModel` Mac 0xc1850, `RotateWorldModel`, `InitialWorldModelRotate`).
**The BSP at +0x1a8 keeps the authored (editor world-space) positions.** The renderer
draws it with m_Transform. `w_TransformWorldModel` (Mac 0xd7700) writes the transformed points, planes and
poly centres into the +0x1ac copy, unless FLAG_BOXPHYSICS is set. The renderer does not read that copy.

### 2.3 Solid / chromakey world models

`r_DrawSolidWorldModel(VP, obj)` (Mac 0x171d30; used for both the solid and the chromakey sets, the latter
between `r_SetChromakeyStates`/`r_UnsetChromakeyStates`):
```c
r = bsp->GetRenderClipRadius();                          // Mac vtbl +0x28
clip = 0x3f; for each frustum plane: d = plane·obj->m_Pos − D;
     if (d < −r) return; if (d > r) clip &= ~(1<<i);     // sphere cull, clip flags for the whole model
r_SetupWMColorScale(obj);                                // §2.4
if (obj->flags & FLAG_FOGDISABLE) glDisable(GL_FOG) (restored after);
save VP world→camera (+0x15c) and the combined (+0x1dc);
VP.worldToCam = VP.worldToCam × obj->m_Transform; VP.combined = VP.proj(+0x19c) × VP.worldToCam;
localCam = obj->m_BackTransform × VP.campos(+0x4b4);
r_StartDrawWorld(0);
r_RouteWorldModelPolies(obj, bsp->m_Polies, bsp->nPolies, &localCam, clip);  // Mac 0x1710d0
   // (TerrainSection BSPs, type 1: polys at +0x54 count +0x58 instead)
r_EndDrawWorld(0);                                       // flush the queued polys with this transform
restore VP matrices;
```
`r_RouteWorldModelPolies` draws **every poly of the model**. There is no BSP walk and no PVS.
Polys whose frame code is already current are skipped (the code is then stamped). The rest
pass back-face culling against `localCam` (`N·cam − D > 0.0001`), skip INVISIBLE, send portal
surfaces to `AddPortal(p, &obj->m_Transform)`, and go to the four dispatch pointers of §1.6 with
`g_ClipFlags = clip`. World-model polys are therefore lit and drawn exactly like world polys
(lightmaps included), in model space under the object's matrix.

### 2.4 Tint and alpha

`r_SetupWMColorScale` (Mac 0x171320): `g_WMColorScale = (R,G,B)/255` from obj+0x94..0x96, and
`g_WMAlpha = obj+0x97`. `r_InitFrame` resets g_WMAlpha to 0xff (Mac 0x15178c). The poly draw
paths multiply vertex colours by the scale and put the alpha in the vertex alpha
(`r_RecurseAndDrawWM` Mac 0x1703b0..0x170458, `r_DrawSkyPoly` Mac 0x178c8c..).

### 2.5 Translucent world models

`r_DrawTranslucentWorldModel(VP, obj)` (Mac 0x1713e0) sets the same tint, fog-disable and matrix setup as
§2.3, then calls `r_RecurseAndDrawWM(obj, bsp->GetRootNode() /*vtbl +0xc*/, 0x3f)` (Mac 0x170110).
This draws **back-to-front by walking the model's BSP**, **without lightmaps**:
```c
// explicit stack, ≤ 0x400 deep
visit(n): if (n->m_Flags & 3) return;
          side = (Dot(N(n), cam) − D >= 0);                // cam = VP+0x4b4 (see note)
          visit(n->m_Sides[!side]);                         // the far side first ([0]=back when cam is in front)
          draw(n->m_pPoly);
          visit(n->m_Sides[side]);
draw(p): skip INVISIBLE; portal surfaces → AddPortal;
   verts (FixTJunc cvar ? p+0x4c/p+0x52 : p+0x54/p+0x50, ≤ 128):
       pos = *v.pos; rgb = v.colour(+0x16 R,+0x15 G,+0x14 B) × g_WMColorScale; a = g_WMAlpha;
       uv = (v.u, v.v) × texScale (data+0x2d7a4: per-texture factor set by r_SetTexture, see part E);
       if surface texture has an env map and EnvMapWorld → r_CalcEnvMapCoords for uv2;
   clip to the frustum (Mac 0x170840); r_SetTexture(surf->m_pTexture) or a flat poly;
   detail texture if DetailTextures && the texture's m_pLinkedTexture(+0x28) (link type +0x2c == 0);
   r_DrawGLPrimitive(GL_TRIANGLE_FAN, verts)
```
Blending comes from `r_SetTranslucentObjectStates` (set once around the whole translucent block in
r_FlushObjectQueues). The additive variant (Mac 0x172af0) wraps the call in
`glPushAttrib(GL_COLOR_BUFFER_BIT); glBlendFunc(GL_ONE, GL_ONE); glDisable(GL_FOG)`.

Note: the BSP side test above uses the **world-space** camera VP+0x4b4 against
**model-space** planes. That is what the code does (Mac 0x170164 loads VP+0x4b4, and nothing in
r_DrawTranslucentWorldModel overwrites it), and it is only correct for an untransformed model. A
re-implementation should use `m_BackTransform × campos`.

---------------------------------------------------------------------------

## 3. Sky

### 3.1 Data

```c
struct SkyDef {                  // ltbasetypes.h:125, 48 bytes; SceneDesc+0x6c
    LTVector m_Min, m_Max;       // +0x00/+0x0c box around the sky objects (not read by the renderer)
    LTVector m_ViewMin, m_ViewMax; // +0x18/+0x24 (SceneDesc+0x84/+0x90) box the camera is squished into
};
// SceneDesc+0x9c LTObject** sky objects, +0xa0 count: up to 30, in the order of
// ILTPhysics::AddObjectToSky(obj, index<30) (server si_AddObjectToSky Mac 0x83460 stores id at
// slot index; client OnSkyDef Mac 0x3bca0 reads the SkyDef and 30 uint16 ids; clientmgr2.cpp cm_RenderCamera fills the list from the
// 30 ids at mgr+0x1160, skipping nulls). The list ORDER is the sky draw order.
// SceneDesc+0xbc m_FarZ.
// MainWorld+0x144 / +0x150: world extents min / max.
// MainWorld+0x1a0 / +0x1a4: all sky-portal polys (BuildSkyPortalList, used by cvar AllSkyPortals).
```
Main-world polys with SURF_SKY (0x10) are **not drawn at all**. They are only gathered into
`VisibleSet.m_SkyPolies`, which decides whether, and over which screen rectangle, the sky pass
runs. The sky shows through holes left in the world: the sky is drawn first with no depth
writes, and the world's opaque polys then cover everything except the sky openings.

### 3.2 Sky camera position (`r_SetupSkyStuff`, Mac 0x14f230; PC 0x1001e660, d3dr3b.cpp:675)

PC calls this at the end of view setup (big1ef30.cpp:1401). The result goes to Mac
VP+0x4c0 / PC `g_v1008bd40`.
```c
t = (scene->m_DrawMode == 1) ? (campos − world.min) / (world.max − world.min)   // per axis
                             : (0.5, 0.5, 0.5);
// Mac skips the divide if any extent component is 0 (Mac 0x14f31c..); PC divides unconditionally
skyPos = sky.m_ViewMin + (sky.m_ViewMax − sky.m_ViewMin) * t;
```

### 3.3 Sky pass (`r_DrawSky`, Mac 0x177130; PC 0x1005f580 w17_1005f580.cpp:90)

```c
if (!DrawSky || scene->nSkyObjects <= 0 || !r_CalcSkyExtents()) return;
lens = r_InitViewBox2(near 0.01, far scene->m_FarZ, VP, minX, minY, maxX, maxY);  // sub-rectangle frustum
mat  = VP camera matrix (VP+0x9c..0xd8) with its translation column (+0xa8,+0xb8,+0xc8) = skyPos;
r_InitFrustrum2(&g_SkyViewParams, &lens, rect, &mat, skyPos, scale = (SkyScale,SkyScale,SkyScale));
r_DrawSkyObjects();
```
So the sky is viewed with the main camera's **rotation** and FOV, from `skyPos`, and the
frustum is restricted to the screen rectangle covering the visible sky polys. `SkyScale` (cvar, default
1.0) scales the sky view. PC passes the same values to 0x1001ef30 (the PC r_InitFrustrum2).

`r_CalcSkyExtents` (Mac 0x176ca0, PC 0x1005dca0 big5dca0.cpp): it processes the polys in `VisibleSet.m_SkyPolies` (or,
with the cvar AllSkyPortals, MainWorld+0x1a0/+0x1a4 with clip mask 0x3d). For each poly it transforms the
vertices by VP world→camera, clips them to the frustum (one clip routine per plane bit), projects them with VP+0x19c,
and grows the screen rectangle (min/max at Mac data+0x2b650/0x2b64c/0x2b648/0x2b644). It counts "Sky
Portals". It returns `(maxX−minX > 0.9) && (maxY−minY > 0.9)`. An OpenGL port can simply set
`glScissor` to that rectangle (or skip the rectangle and draw the sky full-screen, since the world
covers non-sky areas anyway). The rectangle is an optimisation plus a frustum restriction.

`r_DrawSkyObjects` (Mac 0x1797e0; PC 0x10030a60):
```c
glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glPushAttrib(GL_FOG_BIT);
disable texture unit 1;
glFog(GL_FOG_START, SkyFogNearZ); glFog(GL_FOG_END, SkyFogFarZ); if (!SkyFogEnable) glDisable(GL_FOG);
for i in 0..scene->nSkyObjects-1 (list order = painter's order):
    obj = list[i]; if (!(obj->flags & FLAG_VISIBLE)) continue;
    switch (obj->type) {
    case OT_WORLDMODEL (2):
        fog off if FLAG_FOGDISABLE, else restore;
        if (firstSurfaceFlags & 0x8) r_SetTranslucentObjectStates(firstSurfaceFlags>>19 & 1 /*additive*/);
        else { glBlendFunc(GL_SRC_ALPHA, GL_ONE); r_UnsetTranslucentObjectStates(0); }
        g_WMAlpha = obj->m_ColorA; r_DrawSkyWorldModel(obj); break;
    case OT_POLYGRID (7): translucent states iff alpha != 255; r_DrawPolyGrid(&g_SkyViewParams, obj); break;
    case OT_SPRITE (3):   translucent states; r_DrawSprite(&g_SkyViewParams, obj); break;
    // models and other types are ignored in the sky
    }
glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glPopAttrib();
FogEnable ? glEnable(GL_FOG) : glDisable(GL_FOG);
```

`r_DrawSkyWorldModel(obj)` (Mac 0x179600):
- The tint is obj RGB/255 (Mac data+0x2cb1c).
- The model's BSP (+0x1a8) is used **untransformed**: m_Transform is not applied, because sky brushes are
  static and sit at their authored positions inside the sky box.
- It walks from `bsp->m_RootNode` (+0x9c):
  ```
  visit(n): if (n->m_Flags & 3) return;
            front = (N·skyCam − D > 0);                 // skyCam = g_SkyViewParams+0x4b4
            if (front && !(surf.flags & INVISIBLE)) r_DrawSkyPoly(n->m_pPoly);   // back-facing polys are not drawn
            visit(n->m_Sides[!front]); visit(n->m_Sides[front]);
  ```
  (Mac 0x1796d0..0x179778.) The node poly is drawn **before** its far subtree. With depth
  testing off this is not strict back-to-front; it is what ships. Sky models are normally
  convex shells.

`r_DrawSkyPoly(p)` (Mac 0x178bb0):
- Vertex colour = v.rgb × tint, alpha = g_WMAlpha, plus dynamic lights from `p+0x2c`.
  Each light adds `colour × (1 − dist/radius)`, ×2 when the `Saturate` cvar is set, clamped to 255.
- uv = (v.u, v.v) × texScale.
- Positions go through the sky VP, are clipped, then projected in software (TLVertex) and drawn as
  `GL_TRIANGLE_FAN` with `surf->m_pTexture`.
- `ShowSkySplits` draws them untextured.

The sky polys get **no lightmap**.

### 3.4 Panning sky (SURF_PANNINGSKY 0x8000)

These are ordinary world polys flagged PANNINGSKY. They use the global pan texture
`RenderStruct+0x108` `PanInfo[GLOBALPAN_SKYSHADOW=0]`, and `PanInfo[1]` (FOGLAYER) is not read here:
```c
struct PanInfo { SharedTexture* m_pTexture; float m_xOffset, m_zOffset, m_xScale, m_zScale; }; // 0x14 each
// clientmgr.h:437, dmgrow.cpp:505; Mac g_pPanInfo = data+0x29844 = &RenderStruct+0x108 (r_InitFrame Mac 0x151538)
```
`r_InitSkyPanInfo` (Mac 0x1788b0):
`panScale = (texScaleU / m_xScale, texScaleV / m_zScale)`, `panOff = (m_xOffset, m_zOffset)`.
**Pan texture coordinates** for a vertex at world position P:
`u = (P.x + m_xOffset) * panScaleU`, `v = (P.z + m_zOffset) * panScaleV`
(Mac 0x177bb0..0x177be0, 0x19617c..0x196198). The projection is planar on world XZ, and the engine
animates the offsets.
- Dual-pass mode: `r_QueuePanningSkyPoly` queues the poly (with its clip flags). `r_EndDrawWorld` →
  `r_DrawPanningSkyPolies` (Mac 0x178950) binds the pan texture once, then calls `r_DrawPanningSkyPoly`
  for each queued poly. Each poly is drawn with **the pan texture only**, using vertex colours through the saturate colour
  maps, plus dynamic lights.
- Single-pass multitexture mode: the LM queue handles the poly, and `r_DrawLMPolies` calls
  `r_DrawSPMTPanningSkyPoly` (Mac 0x196090). Stage 0 is the surface texture at uv=(v.u,v.v)×texScale and
  stage 1 is the pan texture at the planar uv above, with vertex colours.
- If `PanInfo[0].m_pTexture == NULL`, r_SetPolyFunctions routes PANNINGSKY polys to the gouraud path
  (Mac 0x153a98..0x153aac).

### 3.5 Sky-related cvars (Mac ConVar table)

| cvar | default | effect |
|---|---|---|
| DrawSky | 1 | whole sky pass |
| SkyScale | 1.0 | sky view scale |
| SkyFogEnable | 1 | fog in the sky pass |
| SkyFogNearZ / SkyFogFarZ | 0 / 2000 | fog range used in the sky pass |
| AllSkyPortals | 0 | use every sky poly of the world for the extents |
| ShowSkySplits | 0 | debug |
| DrawWorld | 1 | world poly loop |
| DrawAll | 0 | skip the PVS (r_DrawBSP) |
| LockPVS | 0 | freeze the visible set |
| DrawWorldModels | 1 | world-model sets |
| DrawPortals / ShowPortalBounds | 1 / 0 | portal views. A non-zero ShowPortalBounds skips r_FullDrawScene altogether (Mac 0x157868) |
| DrawWorldTree | -1 | ≥0 = debug-draw the world tree |
| DrawSorted | 1 | sort translucent objects (Mac data+0x2ac3c) |
| FixTJunc | 0 | use the alternate vertex list (poly+0x4c/+0x52) |

(The table comes from `__sinit_common_stuff_cpp` Mac 0x14e6d0, `__sinit_opengl_draw_cpp` 0x159760 and
`__sinit_drawobjects_cpp`. `ConVar` is {int, float, default, int* dst, float* dst, name, hVar, next}
(ctor Mac 0x14e340), and r_ReadConsoleVariables (Mac 0x14e5a0) refreshes them every frame.)

---------------------------------------------------------------------------

## 4. Implementation recipe (OpenGL)

1. Per frame: bump the frame code (on wrap, reset Leaf+0x2c and Poly+0x42 to 0). Build the 6 world-space frustum planes.
   Compute skyPos (§3.2).
2. Visibility: leaf = FindPointInTree(mainBsp, camera). Run the PVS bitstream walk (§1.2) for list 0, plus
   each further list whose UserPortal is open and inside the frustum. For each visible leaf, QueuePoly its
   polys (dedupe by frame code; sky → sky list; invisible → skip). Instead of the engine's world tree
   you can call `g_pWorld->m_WorldTree.DoVisQuery` (PC vtable +0x14 of the object at MainWorld+0x6c)
   with the PC Visit record layout. That also delivers the objects.
3. Sky: if any sky polys survive the frustum, render the sky objects with the camera rotation at skyPos ×
   SkyScale, depth test and depth writes off, in list order. Sky world models are drawn untransformed and
   front-facing only.
4. World: for each queued poly, cull back faces against the camera and cull its bounding sphere against the frustum, then draw it (lightmap,
   gouraud, flat or pan-sky per §1.6 and part E). Do not draw SKY polys.
5. World models: opaque/chromakey ones use model matrix m_Transform, all polys, back faces culled in
   model space, the same poly pipeline as the world, tinted by the object's colour. Translucent ones are drawn after the
   opaque scene, sorted far→near by object, each walked back-to-front by its own BSP using the
   model-space camera, with vertex colours × tint and alpha = object alpha. They use no lightmap and do not write depth. The
   blend is (SRC_ALPHA, ONE_MINUS_SRC_ALPHA), or (ONE, ONE) for additive surfaces. The blend modes are inferred from
   r_SetTranslucentObjectStates' role; part E has the exact states.

---------------------------------------------------------------------------


# Part E: how the renderer draws world polygons

Sources. The main source is the Mac OpenGL renderer (`ext/rez/games/nolf-mac-usa/NOLF/LithTech`). Addresses are offsets into the Mac code section, as `ext/tools/pef.py` prints them. **The stock `pef.py dis` stops at the first `fcmpo`/`fcmpu`** (capstone cannot decode them), so every long function here was re-disassembled with capstone `skipdata=True`. The skipped compares show up as `.byte fc 0x 0x 40`. The PC `d3d.ren` side is used where it confirms or differs.

Console-variable names come from the `ConVar(name, default, int*, float*)` constructors in `__sinit_common_stuff_cpp` (0x14e6d0), `__sinit_opengl_draw_cpp` (0x159760) and `__sinit_lm_dualpass_cpp` (0x19ad50), mapped to the data addresses that the code reads. GL enum values were decoded from the immediates.

**Status labels.** "Read" means taken straight from the disassembly. "Inferred" means it is not directly visible.

---

## E.0 Global switches that pick the path

| Mac global | Meaning | Set by (evidence) |
|---|---|---|
| `g_bSinglePassMT` data+0x2a104 | Lightmaps drawn single-pass with two TMUs | `r_CheckCapabilities` 0x149d00: `= caps.ARB_multitexture && !Force2Pass`, also forced to 1 by `Force1Pass` when multitexture is available |
| `g_bOverlayMT` data+0x1fdec | Two-pass mode may use TMU1 for detail/envmap in the texture pass | 0x149d78: `= caps.texture_env_combine`, cleared when single-pass |
| `g_TMU_Second` data+0x1fde4 | TMU index used for the lightmap or pan texture | 1 when single-pass, else 0 (0x149dbc / 0x149dcc) |
| `g_TMU_Base` data+0x2a114 | TMU index for the base texture | bss, never written: **0** |
| `g_bUseLMPages` data+0x2a108 | Lightmap pages exist | `r_CheckCapabilities` =1; `r_CreateContext` clears it if `r_CreateLMPages` fails |
| `g_GLCaps` data+0x2842c | Bytes: +8 `ARB_texture_env_combine`, +9 packed pixels, +0xa `ARB_multitexture`, +0xd anisotropic, +0x10 `EXT_fog_coord` | `r_glStrings` 0x14a5xx (the strings are next to the `stb`s) |

**PC difference (important for matching the retail look).** `d3d.ren` uses two-pass lightmapping unless `Force1Pass` is set and `ValidateDevice` succeeds: `src/engine/d3d/w38_100322c0_qip.cpp:1127-1160` prints "Using 2 pass lightmapping." `Force1Pass` defaults to 0. On the PC `Saturate` defaults to **0.0** (`src/engine/d3d/big21f90.cpp:473`); on the Mac it defaults to **1.0** (0x14e800).

So the retail PC frame is:

- lightmap pass first, then the texture pass with blend `(ZERO, SRC_COLOR)`, an un-doubled modulate;
- detail textures and environment maps on the world.

The Mac default (single-pass multitexture) draws **no** world detail textures or environment maps. Those exist only in its two-pass texture pass, see E.4.

---

## E.1 Poly classification and dispatch

### E.1.1 Collecting polys (`r_ProcessLeaf__FP4LeafPv` 0x16e900; Part D owns visibility)

For each `WorldPoly* p` in a leaf's poly array (leaf+0x20 = `WorldPoly**`, leaf+0x24 = count):

- Skip it if `p->+0x42 (u16 frame code) == g_FrameCode16`, then stamp it (0x16eb10).
- `s = p->+0x28` (surface), `flags = s->+0x38`.
- `flags & 0x4` → skip (0x16e9c4). **Invisible.**
- `flags & 0x10` and `!DrawFlat` → `VisibleSet.sky` list (+0x28/+0x3c) (0x16e9cc). **Sky poly**, drawn by `r_DrawSky`.
- Otherwise:
  - `s->+0x42 (u16) == 0x7fff` → `VisibleSet.polys` (+0x10/+0x14/+0x24).
  - Otherwise, if portals are enabled (`DrawPortals` and `r_CanDrawPortals`, data+0x2a110) → `VisibleSet::AddPortal`.
  - Otherwise → `polys`.

### E.1.2 Dispatch (`r_VLTagPolies__FUi` 0x16f860, loop 0x16fa1c-0x16fbf8)

The pass ordering inside this function is: tag/collect the leaves, then `r_DrawSky`, then `r_DrawLights`, then (if `DrawWorld`) the per-poly loop below.

```
for each p in VisibleSet.polys:
    plane = p->surface->+0x34 (LTPlane*: N at +0, dist at +0xc)
    if (dot(N, camPos /*ViewParams+0x4b4*/) - dist <= 0.01) continue;      // back-face cull, 0x16fa70
    g_ClipMask = 0x3f
    for i in 0..5:                                      // 6 frustum planes at ViewParams+0x3d0, stride 0x10
        d = dot(plane_i.N, p->+0x18 /*sphere centre*/) - plane_i.d
        if (d < -p->+0x24 /*radius*/) culled
        if (d >  radius) g_ClipMask &= ~(1<<i)          // fully inside this plane: no clip needed
    flags = surface->+0x38
    if (flags & 4) continue
    if (surface->+0x30 /*SharedTexture* */ == 0)  pfnFlatPoly(p)            // 0x16fbe8
    else if (flags & 0x80)                       pfnLMPoly(p)               // lightmapped
    else if (flags & 0x8000)                     pfnPanSkyLMPoly(p)         // receives the sky-pan ("cloud shadow") texture
    else                                         pfnGouraudPoly(p)          // vertex-lit
```

Surface flag bits that are confirmed: **0x4** invisible, **0x10** sky, **0x80** lightmap, **0x8000** panning-sky receiver. No other bits are tested on the static world path. The static world BSP never tests translucent or chromakey bits; those surfaces reach the renderer as world models (Part D).

### E.1.3 `r_SetPolyFunctions__F13WorldDrawMode` 0x1539b0

Called from `r_StartDrawWorld` 0x154500, which is called only by `r_FullDrawScene` with mode **0**. Mode 1 exists but the Mac never uses it.

```
pfnFlatPoly     (data+0x29998) = r_QueueFlatPoly
pfnGouraudPoly  (data+0x29994) = r_QueueGouraudPoly
pfnPanSkyLMPoly (data+0x299a0) = g_bSinglePassMT ? r_QueueLMSinglePass : r_QueuePanningSkyPoly
pfnLMPoly       (data+0x2999c) = g_bSinglePassMT ? r_QueueLMSinglePass : r_DrawLMDualPass
if ShowFillInfo:  all four = r_CalcFillInfo
elif DrawFlat:    all four = r_QueueFlatPoly
elif !(LightMap && g_bUseLMPages && (curWorld->+0x180 & 4)):  LM and PanSky = r_QueueGouraudPoly
if mode == 1:     LM and PanSky = r_QueueGouraudPoly
if *g_pSkyPanInfo (data+0x29844 -> PanInfo*) ->+0 (SharedTexture*) == 0:  PanSky = r_QueueGouraudPoly
```

`curWorld->+0x180` is world-info flags read through data+0x29838 (the current `MainWorld*`). Bit 2 (0x4) means "world has static lightmaps": it is set by MainWorld::ReadLightmapData when "LightAnim_BASE" exists (Mac 0xd9d2c, verified in the merge).

### E.1.4 Queues (all are free-list "banks", `bank->+0x18` = free head)

```c
struct PolyQ    { WorldPoly* poly; uint32 vtxStart/*+4*/; uint32 nVerts/*+8*/; uint32 clipped/*+0xc*/;
                  uint32 clipMask/*+0x10*/; PolyQ* next/*+0x14*/; };      // 0x18 bytes, bank data+0x29fec
struct TextureQ { SharedTexture* tex; PolyQ* polys/*+4*/; TextureQ* next/*+8*/; }; // bank data+0x29fd0
```

| Queue function | What it does |
|---|---|
| `r_QueueLMSinglePass` 0x196c10 | Batches **by base texture**. `tex->+0x1c` caches the TextureQ; it is pushed on `LMSPTexList` data+0x29fcc. Each node stores the current `g_ClipMask` (data+0x29200). |
| `r_QueueGouraudPoly` 0x1599c0 | The same, using `tex->+0x20` and `GouraudTexList` data+0x29fc8. |
| `r_QueueFlatPoly` 0x152c50 | A single list, `FlatPolyList` data+0x29fc4. |
| `r_QueuePanningSkyPoly` 0x178ae0 | A single TOC-resident list. |
| `r_DrawLMDualPass` 0x198e30 | Batches **by lightmap page**. `page = p->+0x44`, `page->+8` = PolyQ list, `page->+4` = next page; list head data+0x2f498. |

### E.1.5 When the queues are drawn (`r_EndDrawWorld` 0x1545e0)

1. `r_DrawQueuedLightmapPolies`. This is the two-pass lightmap pass and is empty when single-pass.
2. `r_DrawPanningSkyPolies`.
3. `g_bSinglePassMT ? r_DrawLMPolies : r_DrawDualLMPolies` (the two-pass texture pass).
4. `r_UnsetLightmapStates`.
5. If `GouraudTexList` is non-empty: `r_SetGouraudStates(mode)`, `r_DrawGouraudPolies`, `r_UnsetGouraudStates`.
6. If `FlatPolyList` is non-empty: `r_SetFlatStates`, `r_DrawFlatPolies`, `r_UnsetFlatStates`.

`r_StartDrawWorld` calls `r_SetLightmapStates`, which is `g_bSinglePassMT ? r_SetSinglePassLMStates : r_SetDualPassLMStates`. Objects (world models, models, translucent objects) come later, in `r_FlushObjectQueues` 0x16a450, in this order:

1. solid world models, solid models, solid polygrids, canvases;
2. chromakey states (`glEnable(GL_ALPHA_TEST)`, `glAlphaFunc(GL_GREATER, current ref)`), then chromakey models and world models;
3. translucent states (blend on, depth writes off), then particles, polygrids, line systems, translucent world models, models, canvases, sprites, and the sorted draw list;
4. no-Z sprites.

---

## E.2 Texture binding and texture coordinates

### E.2.1 `RTexture` (the per-TMU GL record; constructor `__ct__8RTextureFv` 0x17ea80)

The struct below was read off `r_InternalSetTexture` 0x181e70 (the clone copy), `r_SetupTextureSurface` 0x180590 and `r_InternalSetCurrentTexture` 0x154bb0:

```c
struct RTexture {               // has a vtable at +0 (slot 0xc / 0x10 queried by draw code)
  /*+0x04*/ float  uMul;        // texcoord multiplier for this texture (see E.2.3)
  /*+0x08*/ float  vMul;
  /*+0x0c*/ uint32 memSize;     // stats
  /*+0x10*/ uint16 frameCode;   // last frame used (LRU)
  /*+0x12*/ uint16 alphaRef;    // DTX command string "AlphaRef n" (r_SetupTextureMip 0x1800c0); 0 = none
  /*+0x14*/ RTexture* base;     // points at the original record (self for the original)
  /*+0x1c*/ LTLink  lru;        // in the used-texture list data+0x2d768
  /*+0x28*/ uint16 width, height /*+0x2a*/;   // TextureData mip0 size (+0xcc/+0xd0)
  /*+0x2c*/ SharedTexture* owner;
  /*+0x30*/ RTexture* nextVariant;            // per-TMU clones hang off SharedTexture+0x0c
  /*+0x34*/ float detailScale;  // 1 + DTX extra float at TextureData+0x26
  /*+0x38*/ float detailCos, detailSin /*+0x3c*/;  // DTX int16 angle (deg) at TextureData+0x2a
  /*+0x40*/ uint16 bFullbright; // TextureData flags (+0x18) & 1  (DTX_FULLBRITE)
  /*+0x42*/ uint16 tmu;         // which texture unit this record is bound to
  /*+0x44*/ uint16 dirty;       // cleared after binding
  /*+0x46,47,48*/ uint8 startMip, nMips, format;
  /*+0x49*/ uint8 flags;        // 0x81 for clones
  /*+0x4c*/ GLuint glName;
};
```

### E.2.2 Binding a SharedTexture to a TMU

This is open-coded everywhere, for example `r_DrawLMPoly` 0x194f40:

```
tex->+0x30 (u16) = frameCode(data+0x291fc)
r = first of tex->+0x0c chain (via +0x30) with r->tmu == wantedTMU
if !r: r = r_InternalSetTexture(tex, wantedTMU, haveAnyRecord)   // creates/uploads, or clones with same glName
r_InternalSetCurrentTexture(r, -1):
    g_CurRTex[r->tmu] = r                                        // data+0x2d7c4
    glActiveTextureARB(GL_TEXTURE0 + r->tmu); glBindTexture(GL_TEXTURE_2D, r->glName); glEnable(GL_TEXTURE_2D)
    if r->alphaRef: glAlphaFunc(GL_GREATER, r->alphaRef/255.0)
    g_TCoordMul[r->tmu] = (r->uMul, r->vMul)                     // data+0x2d7a4, 8 bytes per TMU
    glActiveTextureARB(GL_TEXTURE0)
```

### E.2.3 Texture upload (`r_SetupTextureSurface` / `r_SetupTextureMip`)

**Format** (0x180628). `TextureData` flags at +0x18:

| Condition | Format |
|---|---|
| `!(f & 2)` && `32BitTextures` | 32-bit |
| `f & 0x100` | 5551 |
| `f & 0x80` | 4444 |
| `f & 1` (fullbright) | 5551, so the alpha bit carries the fullbright mask |
| otherwise | 565 |

**Mip selection.** `startMip = clamp(MipmapOffset + GroupOffset[TextureData+0x20] + TextureData+0x23, 0..3)`, which is also limited to `nMips-1`. The number of mips used comes from `TextureData+0x21` (0 means 4).

**Texture parameters** (0x18096c-0x180a30):

- `MIN_FILTER` from data+0x1efc8, `MAG_FILTER` from data+0x1efc4. These are set from `Bilinear`/`Trilinear` in `r_ReadExtraConsoleVariables`.
- Wrap S and T are `GL_REPEAT` (0x2901).
- Anisotropy uses `GL_TEXTURE_MAX_ANISOTROPY_EXT` (0x84fe) when the extension is present and `Anisotropic > 0`.

**Colour key.** The DTX command string `"ColorKey r g b"` is handled only when `AlphaTest == 0`; the key colour becomes transparent.

**Texture-coordinate multiplier** (0x18048c-0x180534):

```
uMul = (1 << TextureData+0x24) / TextureData.mip[0].width      // +0xcc
vMul = (1 << TextureData+0x24) / TextureData.mip[0].height     // +0xd0
```

`TextureData+0x24` is DTX `m_Extra[4]`. That it is the "UI mipmap offset" is **inferred** from the DTX v2 header layout.

**So:** world vertex `u,v` are in texels of the full-size art. Per `w_GenerateTextureCoordinates`, `u = dot(pos - O, P)` and `v = dot(pos - O, Q)`, with `O/P/Q` at surface+0x04/+0x10/+0x1c. Then **s = u·uMul, t = v·vMul**. Every draw function applies `s = vert.u * g_TCoordMul[TMU].u` (for example 0x195030).

### E.2.4 Fullbright test

`r_IsFullbriteTexture(tex, tmu)` 0x1822a0 returns `RTexture.bFullbright`, which is DTX flag bit 0.

---

## E.3 Single-pass lightmapping (Mac default)

### E.3.1 Fixed texenv state

`r_SetStartingRenderStates` 0x149840 runs at init:

- `glDisable(GL_FOG)`, `glFogi(GL_FOG_MODE, GL_LINEAR)`, `glDisable(GL_CULL_FACE)`, `glEnable(GL_DEPTH_TEST)`, `glDepthMask(1)`, `glDepthFunc(GL_LEQUAL)`, `glShadeModel(GL_SMOOTH)`, `r_SetModulateAlpha`, `glPolygonMode(FRONT_AND_BACK, FILL)`.
- When combine is available:
  - TMU0 = `COMBINE`, RGB `MODULATE(TEXTURE, PRIMARY_COLOR)`.
  - TMU1 = `COMBINE`, RGB `MODULATE(TEXTURE, PREVIOUS)`.

`r_SetModulateAlpha` 0x152750 sets TMU0 alpha to `MODULATE(TEXTURE.a, PRIMARY.a)`.

### E.3.2 `r_SetSinglePassLMStates` 0x196d40

```
TMU0: COMBINE, RGB = MODULATE(src0 TEXTURE/SRC_COLOR, src1 PRIMARY_COLOR/SRC_COLOR)
TMU1: glEnable(GL_TEXTURE_2D);
      LightmapsOnly ? COMBINE RGB=REPLACE(TEXTURE) : COMBINE RGB=MODULATE(TEXTURE, PREVIOUS)
(no GL_RGB_SCALE: the lightmap modulate is 1x on this path)
```

Without the combine extension it uses `GL_TEXTURE_ENV_MODE = MODULATE` (or `REPLACE` for `LightmapsOnly`). `r_UnsetSinglePassLMStates` disables TMU1 and resets TMU0 to modulate.

**Which TMU holds what:** base texture on **TMU0**, lightmap on **TMU1**.

### E.3.3 `r_DrawLMPolies` 0x196690

```
for each TextureQ tq in LMSPTexList:
    fb = r_IsFullbriteTexture(tq->tex, 0)
    for each PolyQ q in tq:
        if q->poly->surface->flags & 0x8000: move q to panList; continue
        g_ClipMask = q->clipMask
        r_DrawLMPoly(q->poly)
    if fb: keep tq on fbList; else free it
if panList:                         // single-pass panning-sky receivers
    bind *g_pSkyPanInfo->tex on TMU g_TMU_Second; r_InitSkyPanInfo()
    for each q: r_DrawSPMTPanningSkyPoly(q->poly)
if fbList:                          // fullbright overlay pass
    glEnable(GL_BLEND); disable TMU1 texturing
    for each tq: bind tq->tex on TMU0; for each q: r_DrawSPMTFullbrite(q->poly)
    glDisable(GL_BLEND); restore TMU1
```

The blend function for the fullbright pass is **never set here**. It is whatever was left from the previous frame: `r_SetGouraudStates(0)` sets `(GL_SRC_ALPHA, GL_ONE)` (0x15b404), and translucent-object code sets `(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)` or `(ONE, ONE)`. The intent is **inferred** to be "draw texels whose alpha bit is set, unlit, over the lit result". Use `(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)` with the 5551 alpha as the mask.

### E.3.4 `r_DrawLMPoly` 0x1947f0 (one poly)

```
verts = p->+0x54 (n = p->+0x50, max 0x80)
for each v (0x18 bytes: LTVector* pos; float u,v; float lmU,lmV; uint8 b,g,r,a):
    LM.xyz = *v.pos; LM.color = 0xFFFFFFFF; LM.uv0 = (v.u, v.v); LM.uv1 = (v.lmU, v.lmV)   // uv1 are page coords (Part C)
clip/transform (0x195190); if fully clipped return 0
if p->+0x2c (dynamic-light list) && r_SetupLitLightmap(p,0):    // temp lightmap with dyn lights (Part C) bound on TMU1
    for each LM: d = pos - p->+0x34 (LM origin);  (P,Q) = surface->vtbl[3](...)   // lightmap vectors
        uv1 = ((dot(d,P)/world->+0xf8 /*LM grid size*/ + 0.5) * g_TCoordMul[1].u,
               (dot(d,Q)/grid + 0.5) * g_TCoordMul[1].v)
else if p->+0x44 (LMPage*):                                     // static page
    glActiveTexture(TMU g_TMU_Second); bind page->+0x1c (GL name) unless already bound; back to TMU0
else: r_QueueFlatPoly(p); return 0                              // no lightmap -> flat
bind surface texture on TMU0;  uv0 *= g_TCoordMul[0]
r_DrawGLPrimitive(GL_TRIANGLE_FAN, LM, n)
```

Two related functions:

- `r_DrawSPMTFullbrite` 0x195820 is the same fan with `TLVertex`, `uv0 = v.u,v · mul[0]`, and colour white.
- `r_DrawSPMTPanningSkyPoly` 0x196090 draws the panning-sky receivers:
  - TMU0 holds the base texture, with vertex colour taken from the surface's Gouraud colours (see E.5).
  - TMU1 holds the pan texture with `s = (pos.x + pan.offU) * (1/pan.scaleU) * mul[1].u` and `t = (pos.z + pan.offV) * (1/pan.scaleV) * mul[1].v`.
  - The `PanInfo` it uses is the engine's `g_GlobalPanInfo` (`RenderStruct`+0x108): `{SharedTexture* +0; float offU +4, offV +8, scaleU +0xc, scaleV +0x10}`. `r_InitSkyPanInfo` 0x1788b0 precomputes `1/scale·mul`.

---

## E.4 Two-pass lightmapping (PC retail default; Mac with `Force2Pass` or no multitexture)

### E.4.1 State

`r_SetDualPassLMStates` 0x19ab80 sets TMU0 to `COMBINE MODULATE(TEXTURE, PRIMARY)` for RGB and alpha, and disables TMU1.

### E.4.2 Pass 1, lightmap

`r_DrawQueuedLightmapPolies` 0x19aa70 runs at `EndDrawWorld`:

- It saves `GL_FOG_COLOR` and sets the fog colour to `r_GetMultipassFogColorFloat(fogRGB)`.
- For each `LMPage` in data+0x2f498, for each poly, it calls `r_ReallyDrawLMDualPass(poly)`.
- It then restores the fog colour.

`r_ReallyDrawLMDualPass` 0x1970b0:

```
build LMVertex like E.3.4 (uses p->+0x4c/+0x52 T-junction-fixed list if FixTJunc)
per vertex: pfnFog(data+0x2998c)(vtx, &vtx.fog)          // E.7
LMAnim && (p->+0x14 & 0x8000): clear bit, r_UpdatePolyLightmap(world, p, 0)   // animated LM re-upload (Part C)
clip; append TLVertices to the frame vertex queue (data+0x2a02c, 0x28 stride; size data+0x2a024)
queue the poly on its base texture's TextureQ (LMSPTexList) with vtxStart/nVerts  -> drawn in pass 2
if chromakey active (r_GetLMChromakey):                   // base first, then lightmap on top
    glPushAttrib(0x46100); r_DrawOverlayPolies(); glPopAttrib()
    glAlphaFunc(GL_ALWAYS); glEnable(GL_BLEND)
    glBlendFunc(Saturate ? (GL_DST_COLOR, GL_SRC_COLOR) : (GL_ZERO, GL_SRC_COLOR))
    glDepthFunc(GL_EQUAL)
if !LMFullBright:
    bind LM page (p->+0x44 ->+0x1c) on TMU0 (or dynamic lit LM); if none -> r_QueueFlatPoly
    draw fan: TLVertex uv = lightmap uv, colour white
(chromakey: restore alpha func / depth func / blend)
```

### E.4.3 Pass 2, texture

`r_DrawDualLMPolies` 0x19aa10 → `r_DrawOverlayPolies` 0x19a660:

```
if combine: TMU1 enable, COMBINE src0 TEXTURE, src1 PREVIOUS
if !chromakey && !LMFullBright:
    glEnable(GL_BLEND); glBlendFunc(Saturate ? (DST_COLOR, SRC_COLOR) /*2x*/ : (ZERO, SRC_COLOR) /*1x*/)
fog colour <- multipass fog colour
for each TextureQ:
    LightmapsOnly -> just free
    fullbright tex (and not chromakey/LMFullBright):
        Saturate ? draw it now with r_DrawOverlayPolyList(tq,tex,0,0) and defer; or just defer
    else r_DrawOverlayPolyList(tq, tex, 1, 0)
deferred fullbright list:
    glBlendFunc(Saturate ? (SRC_ALPHA, ONE_MINUS_SRC_ALPHA) : (SRC_ALPHA, SRC_COLOR))
    r_DrawOverlayPolyList(tq, tex, 1, Saturate)
restore blend enable, fog colour
```

The texture pass reuses the screen-space vertices queued in pass 1, so it draws exactly the same pixels under `GL_LEQUAL`.

### E.4.4 `r_DrawOverlayPolyList(tq, tex, bFree, b)` 0x19a160

**Path A, environment map.** Taken when `g_bOverlayMT && tex->+0x2c (link type) != 0 && EnvMapWorld`.

- The linked texture (`tex->+0x28`) goes on TMU0 and the base texture on TMU1.
- `r_SetEnvMapTextureStates(linkType)` 0x1996a0 sets both units to combine modulate. Then for link type 1 (EnvMap), TMU1 RGB is `DetailTextureAdd ? ADD_SIGNED : MODULATE` with alpha `REPLACE`; for type 2 (EnvMapAlpha), TMU1 alpha is `MODULATE`.
- `r_DrawOverlayDetail` computes TMU0 coordinates with the reflection function at 0x151a80 (camPos, vertex pos, surface plane):

```
d = pos - cam; r = normalize(d - 2*dot(d,N)*N)
if (N.y² > 0.54439 || N.x² < N.z²)  (s,t) = (0.5-0.5*r.x, 0.5-0.5*r.y)
else                                 (s,t) = (0.5-0.5*r.z, 0.5-0.5*r.y)
s,t *= DetailTextureScale
```

The axis choice is as read and looks odd, so treat it as approximate. Separately, `r_RenderScene` stores `g_fEnvPanU = cam.x*EnvPanSpeed+0.5`, `g_fEnvPanV = cam.z*EnvPanSpeed+0.5` and `g_fEnvScale = 0.5/EnvScale` (0x1576c8). These are probably used by models.

**Path B, detail texture.** Taken when `g_bOverlayMT && DetailTextures && tex->+0x28 && linkType == 0`.

- The base texture goes on TMU0 and the detail texture (linked texture) on TMU1.
- `scale = DetailTextureScale` cvar (×`RTexture.detailScale` when the record says so), `cos/sin` = `RTexture.detailCos/Sin`.
- `r_SetDetailTextureStates` 0x199bf0: TMU0 `MODULATE(TEXTURE, PRIMARY)`; TMU1 `COMBINE`, RGB `DetailTextureAdd ? GL_ADD_SIGNED_ARB : GL_MODULATE` of `(TEXTURE, PREVIOUS)`, alpha `REPLACE(PREVIOUS)`.
- Detail coordinates, from base texel u,v (0x1991ac, 0x1994f8):

```
s1 = (u*cos*mul1.u - v*sin*mul1.v) * scale
t1 = (u*sin*mul1.u + v*cos*mul1.v) * scale
```

**Path C.** `r_DrawOverlayNoDetail`: base texture only, `uv0 = u,v · mul[0]`.

**PC confirmation.** `d3d.ren` draws its deferred second-pass buckets the same three ways: detail object in stage 1, linked "second block", or plain (`src/engine/d3d/w14_10011980_osoiqip.cpp:7-31`).

### E.4.5 Multipass fog colour

`r_GetMultipassFogColorFloat` 0x152590 builds `table[i] = clamp(255*sqrt(i/512))`, then `c' = table[c*255]/255` per channel. Each pass is fogged towards about `sqrt(c/2)`, so the 2x multiply of the two passes gives back colour `c`.

---

## E.5 Gouraud (vertex-lit) and flat polys

### E.5.1 Gouraud states

`r_SetGouraudStates(0)` sets `glBlendFunc(GL_SRC_ALPHA, GL_ONE)`; blending itself stays off. `r_DrawGouraudPolies` walks `GouraudTexList`.

### E.5.2 `r_ReallyDrawGouraudPoly` 0x15a500

```
for each vertex: TL.xyz = pos; TL.color = (A = data+0x297ec byte,
      R = cmap[v.+0x16], G = cmap[0x100 + v.+0x15], B = cmap[0x200 + v.+0x14]);  TL.uv = v.u,v
pfnFog per vertex
if !LMDynamic: for each dynamic light L on p->+0x2c (list node: +0 next, +4 LTObject*, +8 pos):
    if |plane dist| < L.radius(+0x1a8): for each vertex within radius:
        add (L.colour bytes +0x94..0x96 - ...) * (1 - dist/radius), x2 if Saturate, clamp 0..255
clip; bind base texture TMU0; uv *= mul[0]  (no texture -> r_ReallyDrawFlatPoly)
draw GL_TRIANGLE_FAN
if base RTexture is fullbright (vtbl+0x10) && fullbright-enabled (data+0x299b4 = cvar && mode!=1):
    fog off; all vertex colours = (cmap[255], cmap[0x1ff], cmap[0x2ff], 255)
    glEnable(GL_BLEND) [func (SRC_ALPHA, ONE) from r_SetGouraudStates]; draw again; glDisable(GL_BLEND); fog back
if p->+0x2c: r_DrawGouraudDynamicLight(p, verts, n)       // LMDynamic: projected dyn-light texture,
       // glPushAttrib, glEnable(BLEND), glBlendFunc(GL_DST_COLOR, GL_ONE), CLAMP, texenv REPLACE,
       // LMRenderSurface + r_RenderScaledLightmap(…, 2.0)  (0x159b68-0x159e14)
```

**Vertex colour bytes.** The world vertex colour bytes at +0x14/+0x15/+0x16 are B, G, R: the D3DCOLOR byte order, kept as-is on the Mac.

**Colour map.** `cmap` (data+0x299c4, 3×256) is refilled in `r_RenderScene` 0x15777c with `table[c] = min(255, c*k)`, where **k = 2 if `Saturate` else 1** (`r_FillColorMap` 0x153d50). So with `Saturate` on, vertex lighting is overbright 2x.

### E.5.3 Flat polys

`r_ReallyDrawFlatPoly2` 0x153560 handles surfaces with no texture, and everything when `DrawFlat` is set.

- TMU0 texturing is disabled (`r_SetFlatStates` 0x153bd0).
- Vertices go through the 4×4 matrix argument (`ViewParams`+0x21c, or a world-model matrix).
- The vertex colour is the **surface pointer value** (or the poly pointer with `ShowSplits`). That is a debug false colour; draw such polys untextured, or skip them.

`r_CalcFillInfo` is the `ShowFillInfo` debug path.

---

## E.6 Vertex formats, transform, clipping

```c
struct TLVertex { float x,y,z;  uint8 a,r,g,b /*+0xc*/;  uint8 spec[4] /*+0x10*/;
                  float fog /*+0x14*/; float u,v /*+0x18,+0x1c*/; float pad /*+0x20*/; float rhw /*+0x24*/; }; // 0x28
struct LMVertex { TLVertex-like first 0x28 bytes; float u1,v1 /*+0x28,+0x2c*/; float pad /*+0x30*/; ... }; // 0x38
```

**Draw call** (`r_DrawGLPrimitive` 0x1917e0 / 0x191bf0):

- `glVertexPointer(3, GL_FLOAT, stride, &v.x)` and, with `EXT_fog_coord`, `glFogCoordPointerEXT(GL_FLOAT, stride, &v.fog)` (`r_SetupTL` 0x1915b0, `r_SetupLM` 0x1916c0).
- Colour is repacked to RGBA bytes.
- Texture coordinates are sent as **4-component `(u·rhw, v·rhw, 0, rhw)`** (projective) for TMU0 and, for `LMVertex`, TMU1.
- Then `glDrawArrays(mode, 0, n)`, with mode 6 = `GL_TRIANGLE_FAN`.

**Transform on the CPU.** `r_SetOpenGLTransformStates` 0x1546f0 calls `r_LoadMatrix`, which is an **empty function** (0x1546c0, a lone `blr`). The Mac does the D3D-style transform itself in the clip routines (0x195190 for `LMVertex`, 0x195a00 for `TLVertex`, 0x15acb0, 0x152e90):

- If `g_ClipMask == 0`: multiply by the 4×4 matrix at `ViewParams`+0x1dc, then `rhw = 1/w` and screen x,y.
- Otherwise: clip in homogeneous space against each plane whose bit is set (bit0 near; bits 1-5 the side and far planes; separate clip functions 0x172e70..0x174500), then project. `UseD3DClip` skips CPU clipping when the near plane is not involved.

A new renderer can instead submit world-space positions with a normal GL projection and modelview built from the `SceneDesc` camera (Part A), and let the GPU clip. The only things that depend on screen space are the perspective-correct texture coordinates, which GL does natively.

---

## E.7 Fog

**Enabling** (`r_ReadExtraConsoleVariables` 0x148ed0):

- `FogEnable` → `glEnable(GL_FOG)` or `glDisable(GL_FOG)`.
- `glFogi(GL_FOG_MODE, GL_LINEAR)`.
- `GL_FOG_COLOR = (FogR, FogG, FogB)/255`, alpha 1.
- With `EXT_fog_coord`: `glEnableClientState(GL_FOG_COORDINATE_ARRAY)`, `glFogi(GL_FOG_COORDINATE_SOURCE, GL_FOG_COORDINATE)`, `GL_FOG_START = FogNearZ`, `GL_FOG_END = FogFarZ`.
- Without it, start and end are derived from the projection, or 1 and 2.

**Defaults:** `FogNearZ` 0, `FogFarZ` 2000, `FogR/G/B` 255, `SkyFogNearZ` 0, `SkyFogFarZ` 2000. `FogEnable` defaults to 0; the game turns it on per level.

**Per-vertex fog function** (`r_SetupFogStuff` 0x157190 picks `pfnFog`, data+0x2998c):

| Case | Fog coordinate |
|---|---|
| Default | `clamp(|pos - camPos|, FogNearZ, FogFarZ)` (0x151ed0). Radial distance, then linear. |
| `TableFog` | 0 (0x151e90) |
| `VFog` | Height ("volumetric") fog (0x152070) using `VFogMinY/MaxY/MinYVal/MaxYVal/Density` |
| Sky | `(z - SkyFogNearZ)/(SkyFogFarZ - SkyFogNearZ)` (0x151fe0) |

**Interaction with the passes:**

- Two-pass: both passes are fogged with the multipass colour (E.4.5).
- Single-pass: one fogged pass.
- Gouraud fullbright and dynamic-light add passes switch fog off or set it to black.

---

## E.8 Other cvars that affect world drawing

`LightMap`(1), `DrawFlat`(0), `LightmapsOnly`(0), `ShowFillInfo`, `DrawWorld`(1), `Saturate`(Mac 1 / PC 0), `LightSaturate`(1, `r_InitSaturateTable` lightmap saturation, Part C), `LMFullBright`(0), `DetailTextures`(1), `DetailTextureAdd`(1), `DetailTextureScale`(1), `EnvMapWorld`(1), `FixTJunc`(0), `LMAnim`(1), `LMDynamic`(1), `LMDynamicScale`(2.5), `LMDynamicSize`(16), `DynamicLight`(1), `Force1Pass`/`Force2Pass`, `DisableMultitexture`, `Bilinear`(1), `Trilinear`, `MipmapOffset`, `GroupOffset0..9`, `32BitTextures`, `AlphaTest`(1), `FogEnable` and the other fog cvars, `DrawSky`(1), `UseD3DClip`(0).

---

## E.9 Recommended OpenGL implementation (synthesis)

Per lightmapped poly (surface flag 0x80, has a texture and an LM page):

```
color = texture(base, (u,v)*mulBase) * texture(lightmap, lmUV) * (Saturate ? 2 : 1)
if detail (linked tex, type 0): color = DetailTextureAdd ? color + detail - 0.5 : color * detail   (rotated/scaled coords, E.4.4)
if fullbright texture: color = mix(color, base.rgb, base.a)
fog: linear on radial distance FogNearZ..FogFarZ
```

The other poly types:

- **Vertex-lit** (no 0x80, no 0x8000): `base * vertexColor(B,G,R bytes at +0x14, ×2 clamped if Saturate)`.
- **Panning-sky receiver** (0x8000): vertex-lit base × pan texture, with `pan uv = ((x + offU)/scaleU, (z + offV)/scaleV)` in pan-texture texels × mul.
- **No texture:** untextured.

Depth is `LEQUAL` with writes on, and there is no back-face culling in GL (the renderer culls on the CPU with the plane test in E.1.2).

## Offsets this part depends on (for Part B to cross-check)

- **WorldPoly:**
  - +0x14 u16 flags (0x8000 = lightmap-animation dirty)
  - +0x18 sphere centre, +0x24 radius, +0x28 surface
  - +0x2c dynamic-light list, +0x34 lightmap origin vector
  - +0x42 u16 frame code, +0x44 `LMPage*`
  - +0x4c T-junction vertex pointer, +0x50 u16 nVerts, +0x52 u16 T-junction count
  - +0x54 vertex array, 0x18 bytes each: pos*, u, v, lmU, lmV, B, G, R, A
- **Surface:**
  - +0 vtable (slot 0xc gives the lightmap P/Q vectors)
  - +0x04 O, +0x10 P, +0x1c Q
  - +0x30 `SharedTexture*`, +0x34 `LTPlane*`, +0x38 flags, +0x42 u16 portal index (0x7fff = none)
- **SharedTexture:**
  - +0x0c `RTexture` chain
  - +0x1c / +0x20 renderer TextureQ caches
  - +0x28 linked texture, +0x2c link type (0 detail, 1 env, 2 env alpha), +0x30 u16 frame code
- **World:** `MainWorld` +0xf8 float lightmap grid size; +0x180 world-info flags (0x4 = has static lightmaps, verified at Mac 0xd9d2c).

## Names of the helper functions that `pef.py funcs` does not list

`pef.py funcs` skips functions with template names. These names were taken from the traceback strings:

| Address | Name |
|---|---|
| 0x151a80 | `r_CalcEnvMapCoords(const CVector& cam, const CVector& pos, const CVector& N, float& s, float& t)` |
| 0x151e90 | `r_CalcFog_None` (TableFog) |
| 0x151ed0 | `r_CalcFog_Linear` (default) |
| 0x151fe0 | `r_CalcFog_Sky_Linear` |
| 0x152070 | `r_CalcFog_Volume` (VFog) |
| 0x195190, 0x1987a0 | `r_FullSetupPoly3<LMVertex, FogHandlerNone>(LMVertex** verts, int* n, ViewParams*, FogHandlerNone*)`: CPU transform plus clip, one copy per caller TU |
| 0x195a00, 0x198110, 0x15acb0, 0x152e90 | `r_FullSetupPoly3<TLVertex, FogHandlerNone>`: the same for TLVertex |

Part A reports a near plane at Z = 7.0 with an orthographic GL projection; see A.md.

---------------------------------------------------------------------------
