# NOLF (LithTech, NOLF 1 engine) — drawing the OBJECTS of a scene: spec for opengl.ren

How to read this spec. Evidence is cited as `file:line` (repo src/), as a **PC address** (d3d.ren 0x1xxxxxxx, lithtech.exe 0x004xxxxx, server.dll 0x100xxxxx), or as **Mac** function@offset. The Mac addresses come from the Mac engine `ext/rez/games/nolf-mac-usa/NOLF/LithTech`. Its OpenGL renderer is linked into the engine, and its function names are preserved. For Mac disassembly, use `ext/tools/pef.py dis - <name>`. Anything inferred is marked *inferred* / INFERRED. PC and Mac use identical object layouts.

Contents
- Part I (core): 1. engine/renderer boundary (RenderStruct callbacks, SceneDesc); 2. per-frame object enumeration, visibility, frame codes, queues and draw order; 3. LTObject common layout, flags, object matrix.
- Part II: Models (OT_MODEL): layouts, TransformMaker (how node transforms are obtained), skinning, LOD, lighting, ModelHook, env map, shadows, GL pseudocode.
- Part III: Sprites, particle systems, poly grids, line systems, canvases (ILTCustomDraw), dynamic lights, world models and containers, sky objects, cameras.

**Key answers in brief**
1. Getting the objects. With DRAWMODE 2 (RenderObjects), the renderer draws the SceneDesc+0xdc/+0xe0 list directly. With DRAWMODE 1, it calls the engine world tree's vis-query virtual (world+0x6c, vtbl +0x14 on PC) with renderer callbacks. The engine does the PVS walk and the per-node sphere/plane culling, then calls r_ProcessObject for each object. Poly grids come instead through BSP leaf records, in r_ProcessLeaf. Each type's Process function only queues the object into a VisibleSet bucket; r_FlushObjectQueues draws the buckets in a fixed order, with the translucent objects depth-sorted. The engine's per-frame object frame code (RenderStruct+0x28) stamps LTObject+0x58, so each object is drawn once per frame.
2. Engine services. These are the RenderStruct callbacks (Part I §1.2), plus whatever vtables and function pointers live inside engine objects. Neither DLL imports anything from the other. Node transforms: the renderer carries its own copy of TransformMaker (src/engine/lithtech/transform.cpp section B) and runs it over engine memory, after calling ModelInstance vtbl+0x38 (Part II §0). Attachments are positioned by the engine, through RenderStruct+0x00.


# Part I — Core: boundary, enumeration, LTObject

## 1. The engine/renderer boundary: what a separate render DLL gets

### 1.1 There is no import channel. Everything goes through RenderStruct, SceneDesc, and engine memory

* Retail `d3d.ren` imports only DDRAW.dll, WINMM.dll, KERNEL32.dll and USER32.dll. It exports `FreeModeList`, `GetSupportedModes` and `RenderDLLSetup`. `lithtech.exe` exports nothing. Evidence: `objdump -p ext/rez/games/nolf-eu-goty/d3d.ren` and `.../lithtech.exe`.
* The renderer reaches engine services in only three ways:
  1. **The 15 engine callbacks** at RenderStruct +0x00..+0x38 (table below).
  2. **SceneDesc**, the struct passed to RenderScene (RenderStruct +0xb0). It holds pointers that include the ModelHook function and the object lists.
  3. **Reading engine objects in place**: `LTObject` and its subclasses, `Model`, `SharedTexture`/`TextureData`, the world. The renderer also calls **C++ virtuals and function pointers stored inside those objects**, for example the world tree's vis-query virtual, `GetVisBSP`, a canvas's draw function and a model's node-control callback. A vtable call needs no import, so this works across the DLL boundary.
* So **no engine callback supplies animated model node transforms**. The only engine code touched is the ModelInstance's own vtable slot +0x38 (`SetupTransformMaker`, which copies the tracker time refs and the node-control hook). See Models §0. d3d.ren carries its own copy of the model animation code (TransformMaker / "Model::SetupTransforms", d3d.ren 0x1007ca60, 0x1007cab4, 0x1007cc90, 0x1007da80). That code reads the engine's `Model` and `ModelInstance` memory directly. See the models section.
* The Mac renderer is linked into the engine. Even so, it calls the engine through the same RenderStruct pointer (TOC slot data+0x291f4). Examples: `r_RenderScene` calls `+0x14` (printf) for its stats lines, `r_InitFrame` calls `+0x28`/`+0x30`, and `r_FlushObjectQueues` calls `+0x24` between passes. A few things it calls directly that a PC DLL cannot call: `r_ProcessAttachment` and the engine's `TransformMaker`. On PC each of those becomes a RenderStruct callback, or code the DLL carries itself.

### 1.2 RenderStruct +0x00..+0x38: the engine callbacks, named by the Mac

The Mac `r_InitRenderStruct__FUi` (0xec6e0) stores each slot from a named function. That makes the Mac the authoritative source for names; the PC equivalent is 0x00462110 (clientrender.cpp:60-120).

| off | Mac name / signature | PC stub (clientrender.cpp) | notes |
|---|---|---|---|
| +0x00 | `LTObject* r_ProcessAttachment(LTObject* pParent, Attachment* pAttach)` | 0x00462080 | Mac body (0xec030) is below. It **positions and rotates the child** and returns it, or NULL. |
| +0x04 | `SharedTexture* r_GetSharedTexture(const char* filename)` | 0x00462240 | Loads or looks up a texture by filename. |
| +0x08 | `TextureData* r_GetTexture(SharedTexture*, int* pStatus)` | 0x00462270 | Gives the system-memory pixels (loads them if needed). |
| +0x0c | `void r_FreeTexture(SharedTexture*)` | 0x00462290 | |
| +0x10 | `void r_RunConsoleString(char*)` | 0x004622b0 | clientrender.cpp calls this "print one string". The Mac name is RunConsoleString; check the PC callee 0x0041abe0 before relying on either reading. |
| +0x14 | `void r_ConsolePrint(char* fmt, ...)` | 0x004622d0 | printf to the console. |
| +0x18 | `HCONSOLEVAR r_GetParameter(char* name)` | 0x00462310 | |
| +0x1c | `float r_GetParameterValueFloat(HCONSOLEVAR)` | 0x00462330 | |
| +0x20 | `char* r_GetParameterValueString(HCONSOLEVAR)` | 0x00462350 | |
| +0x24 | `void r_RendererPing()` | 0x00404470 (bare `ret`) | Called between the draw passes (r_FlushObjectQueues). Does nothing. |
| +0x28 | `uint32 r_IncObjectFrameCode()` | 0x00462380 | **Returns the new object frame code.** Mac body: `ObjectMgr::IncFrameCode` (0xc7f00) on clientmgr+0x1ec. That function does `++code`; on wrap it sets the code to 1 and zeroes +0x58 of every object in all 11 per-type lists (ObjectMgr+0x224 + 0x10*type). |
| +0x2c | `uint32 r_GetObjectFrameCode()` | 0x00462360 | ObjectMgr::GetFrameCode (0xc7ec0), returns ObjectMgr+4. |
| +0x30 | `uint16 r_IncCurTextureFrameCode()` | 0x004623a0 | CClientMgr::IncCurTextureFrameCode (0x156b0). Returns the new uint16 code; on wrap it zeroes `SharedTexture+0x30` for every texture. |
| +0x34 | `void* r_Alloc(uint32)` | 0x004623b0 dalloc | |
| +0x38 | `void r_Free(void*)` | 0x004623c0 dfree | |

Engine data that the renderer reads from the same struct (clientrender.cpp:150-185; Mac r_InitRenderStruct):
`+0x3c/+0x40` screen width and height; `+0x104 SharedTexture*` the global **environment map** (the console "EnvMap" command); `+0x130 LTVector m_GlobalLightDir`, defaulting to normalize(0,-2,-1); `+0x13c LTVector m_GlobalLightColor`. Mac `r_RenderScene` (0x1576ec..0x15770c) copies the **negated** +0x130 into a renderer global each frame. That global is the direction toward the light.

`r_ProcessAttachment` (Mac 0xec030), which PC slot +0x00 matches:
```c
LTObject* r_ProcessAttachment(LTObject* pParent, Attachment* pAtt) {
    LTransform t;                    // pos +0, rot +0xc
    // ILTCommon (clientDE+8) vtable slot: GetAttachmentTransform(hAttachment, t, bWorldSpace=TRUE)
    if (pCommon->GetAttachmentTransform(pAtt, t, 1) != LT_OK) return NULL;
    LTObject* pChild = cm_FindObject(clientMgr, pAtt->m_nChildID /*+0x1e*/);
    if (!pChild) return NULL;
    cm_MoveObject(clientMgr, pChild, &t.m_Pos, 0);   // 0x2c240
    cm_RotateObject(clientMgr, pChild, &t.m_Rot);    // 0x2c3a0
    return pChild;
}
```
So **the engine computes every attachment position**, including attachments to model nodes and sockets, which go through its own `ILTModel::GetNodeTransform`. The renderer only walks the list and draws the child.

### 1.3 SceneDesc: the struct passed to RenderScene (0xec bytes)

The PC side builds it in cm_RenderCamera, 0x00411700 (clientmgr2.cpp:1100-1290; clientblit.cpp:428-497). The Mac side builds it in `cm_Render__FP10CClientMgrP14CameraInstanceiPP8LTObjecti` (0x15c40, struct at r1+0xb0). The two agree offset for offset.

```c
struct SceneDesc {                       // passed to RenderStruct+0xb0
 /*00*/ int        m_DrawMode;          // 1 = normal (world + visible objects), ci_RenderCamera
                                        // 2 = object list only, ci_RenderObjects (Mac 0x4498 / 0x462c: li r5,1 / li r5,2)
 /*04*/ int*       m_pTicks[6];         // engine counters the renderer may write
 /*1c*/ LTVector   m_ModelAdd;          // console "ModelAdd"      (PC g_ModelAdd 0x4b3228)
 /*28*/ LTVector   m_ModelDirAdd;       // console "ModelDirAdd"   (PC 0x4b3218)
 /*34*/ RenderContext* m_pRenderContext;// the result of CreateWorld; NULL = no world. Its +0x08 = the world, +0x0c = uint16 frame code
 /*38*/ LTVector   m_v38;               // clientmgr+0x75c  } named "GlobalLightDir/Color" by clientmgr2.cpp, but only
 /*44*/ LTVector   m_v44;               // clientmgr+0x750  } cm_Init ever writes these (both zero). Use RenderStruct +0x130/+0x13c
 /*50*/ LTVector   m_GlobalLightScale;  // clientmgr+0x768 (ci_Get/SetGlobalLightScale); applied as a full-screen modulate pass, see 2.6
 /*5c*/ LTVector   m_GlobalLightAdd;    // camera+0x1c4 (camera "LightAdd"); full-screen additive pass, see 2.6
 /*68*/ float      m_FrameTime;         // clientmgr+0x1258 (Mac +0x1850)
 /*6c*/ SkyDef     m_SkyDef;            // 0x30 bytes
 /*9c*/ LTObject** m_SkyObjects;        // up to 30 (FLAG2_SKYOBJECT objects, clientmgr+0x1160 id table)
 /*a0*/ int        m_nSkyObjects;
 /*a4*/ LTRect     m_Rect;              // l,t,r,b (full screen if camera m_bFullScreen)
 /*b4*/ float      m_xFov, m_yFov;      // radians; camera +0x1b8 -> xFov, +0x1bc -> yFov (Mac cm_Render 0x15dd0)
 /*bc*/ float      m_FarZ;              // console FarZ
 /*c0*/ LTVector   m_Pos;               // camera +0x18c
 /*cc*/ LTRotation m_Rotation;          // camera +0x9c
 /*dc*/ LTObject** m_pObjectList;       // DRAWMODE 2 only (ci_RenderObjects' list)
 /*e0*/ int        m_ObjectListSize;
 /*e4*/ ModelHookFn m_ModelHookFn;      // ILTClient::SetModelHook (clientmgr+0x1524)
 /*e8*/ void*      m_ModelHookUser;
};
```
Correction to clientmgr2.cpp: it calls +0xdc/+0xe0 `m_pUser4/m_pUser5`. They are the object list and its count. The Mac reads them as such (`r_FullDrawScene` 0x1547c4: `r_ProcessObjectList(scene->0xdc, scene->0xe0)`), and `cm_Render`'s 4th and 5th parameters are `LTObject**, int`. The +0x9c list is the **sky object** list.

## 2. How the objects to draw are found, each frame

### 2.1 Frame setup (Mac `r_RenderScene` 0x157560, `r_InitFrame` 0x1514e0)

```
r_RenderScene(scene):
    if (!scene || !g_bIn3D) return 0;                  // data+0x28440
    if (g_bInOptimized2D) { error "tried to render 3D while in optimized 2D mode"; return 0; }
    r_InitFrame(scene, buf, 0x7000):
        g_pScene = scene;
        if (scene->m_DrawMode == 1) {
            if (!scene->m_pRenderContext) return 0;
            g_pWorld = context->+0x08;   r_IncrementFrameCode(context)   // the uint16 world/leaf/poly frame code at context+0x0c
        } else g_pWorld = NULL;
        g_ObjectFrameCode  = RenderStruct->IncObjectFrameCode();        // +0x28, stored as uint32 (data+0x29830)
        g_TextureFrameCode = (uint16)RenderStruct->IncCurTextureFrameCode(); // +0x30 (data+0x291fc)
        copy light fields; set up the view (rect, fov, farZ, pos, rot) -> ViewParams g_ViewParams (data+0x2920c)
    r_FullDrawScene(scene)                              // below
    print stats via RenderStruct+0x14
```

### 2.2 `r_FullDrawScene` (Mac 0x154780), the whole object pipeline

```
r_SetOpenGLTransformStates();
r_InitObjectQueues();            // calls each type's PreFrame (table below)
r_InitLitPolyList();
if (scene->m_DrawMode == 2) {    // ILTClient::RenderObjects
    VisibleSet::ClearSet();
    r_ProcessObjectList(scene->m_pObjectList, scene->m_ObjectListSize);
} else {                         // ILTClient::RenderCamera
    r_StartDrawWorld(0);
    r_TagVisibleLeaves();        // vis query: calls the per-object Process fns + queues world polys;
                                 // then r_DrawSky(), r_DrawLights(), draws the opaque world
    r_EndDrawWorld(0);           // lightmap / gouraud / flat world passes
}
r_FlushObjectQueues();           // DRAWS all the queued objects (2.5)
r_ClearLitPolyList();
LightScalePass(&scene->m_GlobalLightScale);   // 0x154220, see 2.6
LightAddPass(&scene->m_GlobalLightAdd);       // 0x153f20
```
`r_ProcessObjectList` (Mac 0x153e40) does nothing more than call `g_ObjTypeHandlers[obj->m_ObjectType].Process(obj)` for each non-NULL entry with `0 <= type < 11` and a non-NULL handler. It applies **no visibility or flag test and no attachments**.

### 2.3 Per-type handler table (Mac data+0x2045c; PC d3d.ren 0x100895c0, d3dqip4.cpp:60-66)

There are 11 records of 0x14 bytes each: `{ Init(), Term(), int bInited (set to 1 by r_InitObjectModules), PreFrame(), Process(LTObject*) }`. The Mac contents, read from the data section:

| type | OT_ | Init/Term | PreFrame | Process |
|---|---|---|---|---|
| 0 | OT_NORMAL | - | - | - (never drawn) |
| 1 | OT_MODEL | r_InitModelDraw / r_TermModelDraw | r_ModelPreFrame | r_ProcessModel |
| 2 | OT_WORLDMODEL | - | r_WorldModelPreFrame | r_ProcessWorldModel |
| 3 | OT_SPRITE | - | r_SpritePreFrame | r_ProcessSprite |
| 4 | OT_LIGHT | - | - | r_ProcessLight |
| 5 | OT_CAMERA | - | - | - (never drawn) |
| 6 | OT_PARTICLESYSTEM | - | r_ParticlePreFrame | r_ProcessParticles |
| 7 | OT_POLYGRID | r_InitPolyGridDraw / r_TermPolyGridDraw | r_PolyGridPreFrame | r_ProcessPolyGrid (only reached through BSP leaves, 2.4) |
| 8 | OT_LINESYSTEM | - | r_LineSystemPreFrame | r_ProcessLineSystem |
| 9 | OT_CONTAINER | - | - | r_ProcessWorldModel (drawn like a world model) |
| 10 | OT_CANVAS | - | r_CanvasPreFrame | r_ProcessCanvas |

The **Process functions only queue**: they append the object to a bucket of the `VisibleSet`. Drawing happens later. For example, `r_ProcessModel` (0x189a30) puts the object in VS_MODELS_TRANSLUCENT if `colour.a != 255`, otherwise in VS_MODELS_CHROMAKEY if `Flags2 & FLAG2_CHROMAKEY`, otherwise in VS_MODELS. `r_ProcessSprite` (0x15e8f0) puts it in VS_SPRITES_NOZ if `Flags & FLAG_SPRITE_NOZ (0x10)`, otherwise in VS_SPRITES. The VisibleSet buckets (`VisibleSet::Init` 0x16f1c0) are each `{T** array; uint32 count; ...; uint32 max (+0x14); char* name (+0x18)}`, 0x20 bytes. An overflow logs "Set '%s' overflowed" and drops the object.

| VisibleSet off | name | max |
|---|---|---|
| +0x144 | VS_MODELS | 300 |
| +0x164 | VS_MODELS_TRANSLUCENT | 300 |
| +0x184 | VS_MODELS_CHROMAKEY | 300 |
| +0x1a4 | VS_SPRITES | 128 |
| +0x1c4 | VS_SPRITES_NOZ | 128 |
| +0x1e4 / +0x204 / +0x224 | VS_WORLDMODELS / _TRANSLUCENT / _CHROMAKEY | 64 each |
| +0x244 | VS_LIGHTS | 40 |
| +0x264 / +0x284 | VS_POLYGRIDS / _TRANSLUCENT | 32 each |
| +0x2a4 | VS_LINESYSTEMS | 32 |
| +0x2c4 | VS_PARTICLESYSTEMS | 64 |
| +0x2e4 / +0x304 | VS_CANVASES / _TRANSLUCENT | 32 each |

### 2.4 Visibility: the world's PVS walk with renderer callbacks (DRAWMODE 1)

`r_TagVisibleLeaves` (0x16fc40) calls `r_VLTagPolies(bAll)` (0x16f860). `bAll` is set when the world has no PVS (`world+0x180 & 2` clear) or when console DrawAll is on; PC 0x1007b880 does the same in d3dqip4.cpp:820-848. In the normal path the renderer fills a 0x28-byte request and calls **a virtual on the engine's world tree**:

```c
struct VisQueryRequest {                 // Mac r1+0x50 in r_VLTagPolies; PC Visit1007b580 (d3dqip4.cpp:690)
 /*00*/ uint8    m_Flags;                // 0
 /*04*/ LTVector m_Viewpoint;            // ViewParams+0x4b4 (camera pos), or +0x4d8 when drawing a portal view (ViewParams+0x4d4 != 0)
 /*10*/ float    m_ViewRadius;           // 10000.0
 /*14*/ void   (*m_ObjectCB)(WorldTreeObj* pObj, void* pUser);            // r_ProcessObject
 /*18*/ void   (*m_AddObjects)(LTLink* pList, LTObject**& pArray, int& n);// r_AddRenderObjects
 /*1c*/ void*    m_pUser;                // 0
 /*20*/ void   (*m_LeafCB)(Leaf* pLeaf, void* pUser);                     // r_ProcessLeaf
 /*24*/ LTBOOL (*m_PortalTest)(UserPortal*);                              // r_IsPortalVisible
 /*28*/ LTBOOL (*m_NodeFilter)(WorldTreeNode*);                           // r_NodeFilter
};
// PC: ((CWorldTree*)(world + 0x6c))->vtbl[+0x14](&req);   (d3dqip4.cpp:799; Mac uses vtbl +0x20 because its vtable has a different header)
// In the PC reconstruction, Visit1007b580's +0x24/+0x28 are named the reverse way round from the Mac:
// PC 0x1001e770 is the PortalTest and 0x1007b520 is the NodeFilter.
```
What the engine does with the request: it walks its BSP/PVS (fntdraw.cpp:1719ff, the client's vis traversal 0x0048b8e0, and worldtree.cpp). For every node it gathers that node's object list through `m_AddObjects`, splits it against the node planes using each object's `m_Pos`/`m_Radius` (+0x18c/+0x1a4), and for each visible leaf calls `m_LeafCB(leaf)`. Then for each object whose `+0x58 != mark` it sets `+0x58 = mark` and calls `m_ObjectCB(obj)`. **Per-object culling is therefore sphere vs. PVS plus BSP plane splits**, done inside the engine. `r_NodeFilter` (0x16f720) rejects world-tree nodes whose bounding sphere (centre +0x30, radius at +0x40) lies wholly outside any of the 6 frustum planes at `ViewParams+0x3d0` (each plane is 16 bytes: normal, dist).

The renderer's callbacks:
```c
// The visibility predicate. The Mac inlines it in r_ProcessObject / r_CheckAndProcessObject /
// r_AddRenderObjects / r_ProcessAttachments; PC writes it as CanDraw1007a680, d3dqip4.cpp:94-104.
bool ShouldDraw(LTObject* o) {
    if (!g_Handlers[o->m_ObjectType].Process || o->m_ObjectType == OT_POLYGRID) return false;
    if (o->m_Flags2 & FLAG2_SKYOBJECT /*0x40*/) return false;          // drawn only through SceneDesc+0x9c
    if (o->m_Flags & FLAG_VISIBLE) {
        if (g_ViewParams.m_bPortalView /*+0x4d4*/ && (o->m_Flags2 & FLAG2_PORTALINVISIBLE /*1*/)) return false;
        return true;
    }
    return g_ViewParams.m_bPortalView && (o->m_Flags & FLAG_PORTALVISIBLE /*0x400*/);
}
void r_ProcessObject(WorldTreeObj* o, void*)  { if (ShouldDraw(o)) r_ReallyProcessObject(o); }   // 0x16e4b0
void r_CheckAndProcessObject(LTObject* o)     { if (ShouldDraw(o)) { o->m_FrameCode58 = g_ObjectFrameCode; r_ReallyProcessObject(o); } }
void r_ReallyProcessObject(LTObject* o) {                                                         // 0x16e410
    g_Handlers[o->m_ObjectType].Process(o);
    if (o->m_pAttachments /*+0x98*/) r_ProcessAttachments(o, 0);
}
void r_ProcessAttachments(LTObject* o, uint32 depth) {                                             // 0x16e240
    if (depth > 32) return;
    for (Attachment* a = o->m_pAttachments; a; a = a->m_pNext /*+0x24*/) {
        LTObject* c = RenderStruct->ProcessAttachment(o, a);        // +0x00: engine moves/rotates c
        if (!c || c->m_FrameCode58 == g_ObjectFrameCode) continue;
        r_ProcessAttachments(c, depth + 1);                          // grandchildren first
        if (ShouldDraw(c)) { g_Handlers[c->m_ObjectType].Process(c); c->m_FrameCode58 = g_ObjectFrameCode; }
    }
}
// m_AddObjects: append each object of a world-tree node list that passes ShouldDraw. The walk stops at the
// first object whose +0x188 is non-NULL (Mac 0x16e624; PC 0x1007aeb4 likewise). +0x188 is an unidentified
// client field; the reading that such objects sit at the list tail is an inference.
void r_AddRenderObjects(LTLink* head, LTObject**& arr, int& n);    // 0x16e600
// m_LeafCB: stamp the leaf, then draw the leaf's poly grids, then queue its world polys.
void r_ProcessLeaf(Leaf* leaf, void*) {                                                            // 0x16e900
    leaf->m_FrameCode2c = g_WorldFrameCode; g_nVisibleLeaves++;
    for (link in leaf->m_PolyGridRecords /*LTLink at +0x14*/) {     // PGRecord: +0x18 = the poly grid LTObject
        LTObject* pg = ((PGRecord*)link->m_pData)->m_pObject;
        if (pg->m_ObjectType == OT_POLYGRID && (pg->m_Flags & FLAG_VISIBLE) && pg->m_FrameCode58 != g_ObjectFrameCode) {
            pg->m_FrameCode58 = g_ObjectFrameCode;  r_ReallyProcessObject(pg);
        }
    }
    for (i < leaf->m_nPolies /*+0x24*/) queue leaf->m_pPolies[i] /*+0x20*/ unless poly->m_FrameCode42 == g_WorldFrameCode;
}
```
So the **poly grids are linked into BSP leaves**: `obj_AddPolyGridToLeaves` (Mac 0xe31d0) links one PGRecord per leaf, and each record is in the leaf's +0x14 list (ltobject_internal.h PGRecord). **All other object types come from the world-tree node lists**, through `m_AddObjects` and `m_ObjectCB`.

`bAll` path (no PVS): PC 0x1007aeb0 (d3dqip4.cpp:578-605) walks every leaf's object list (leaf +0x0c) from the tail, stopping at the first object with +0x188 set. It applies ShouldDraw, stamps +0x58 and processes the object with its attachments, then queues every poly.

After the vis query, `r_VLTagPolies` calls `r_DrawSky()` and `r_DrawLights()`, then draws the queued opaque world polygons with back-face and frustum tests.

### 2.5 Draw order: `r_FlushObjectQueues` (Mac 0x16a450)
```
r_DrawSolidWorldModels();   r_DrawSolidModels();   r_DrawSolidPolyGrids();   r_DrawSolidCanvases();
r_SetChromakeyStates();  r_DrawChromakeyModels();  r_DrawChromakeyWorldModels();  r_UnsetChromakeyStates();
(*optional hook at data+0x2ac7c)();
ObjectDrawList.GrowList(n translucent: polygrids_t + particles + linesystems + worldmodels_t + models_t + canvases_t + sprites);
r_SetTranslucentObjectStates(0);
r_DrawTranslucentParticles(); r_DrawTranslucentPolyGrids(); r_DrawLineSystems();
r_DrawTranslucentWorldModels(); r_DrawTranslucentModels(); r_DrawTranslucentCanvases(); r_DrawTranslucentSprites();
    // each of these r_DrawTranslucentX only ADDs (obj, drawFn) to the ObjectDrawList (r_QueueTranslucentX)
ObjectDrawList::Draw(&g_ViewParams);     // Sort, then call each drawFn (see below)
ObjectDrawList::Clear();
r_DrawNoZSprites();                      // FLAG_SPRITE_NOZ sprites last, with no depth test
r_UnsetTranslucentObjectStates(1);
```
(RenderStruct+0x24 ping is called between the passes.)

**ObjectDrawList** (Mac 0x16ac40 Add, 0x16a860 Sort, 0x16aab0 Draw). This list holds the translucent objects. When console `DrawSorted` (default 1) is 0, `Add` calls `drawFn(&g_ViewParams, obj)` immediately. Otherwise it appends `{obj, drawFn, float key}` (12 bytes). `Sort` sets `key = |obj.m_Pos - ViewParams.m_Pos(+0x4b4)|²`. For objects with **FLAG_REALLYCLOSE (0x40)** it uses `key = |obj.m_Pos|²` instead, because a really-close object's position is **relative to the camera**. The list is then shell-sorted far to near (inferred from the use; the comparison loop at 0x16a948 was not traced further). `Draw` re-applies the visible/portal test and calls each function. A separate console variable `SortProfile` exists. Each solid pass calls `BaseObjectSet::Draw(set, &g_ViewParams, drawFn)` (0x16ec80). That function **re-applies the FLAG_VISIBLE / portal test** before calling `drawFn(ViewParams*, LTObject*)` for each queued object. The draw functions are r_DrawModel, r_DrawSprite, r_DrawSolidWorldModel, r_TestAndDrawPS, r_DrawPolyGrid, r_DrawLineSystem and r_DrawCanvasCB.

### 2.6 The two full-screen passes after the objects (Mac 0x154220, 0x153f20)
* **Global light scale** (`SceneDesc+0x50`): skipped when all three components lie in (0.99, 1.01). Otherwise a full-screen quad over the viewport with texturing, fog and depth test off and depth writes off, `glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR)`, and `glColor3f(scale*0.5)`. The result is `dst*scale`. This means the global light scale **darkens the whole 3D view as a post pass**; it is not applied per object.
* **Light add** (`SceneDesc+0x5c`, the camera LightAdd): runs if any component ≥ 0.001. A quad over the camera rect (ViewParams +0x1c..+0x28), `glBlendFunc(GL_ONE, GL_ONE)`, `glColor3f(lightAdd)`.

### 2.7 Frame codes, summarised
* `LTObject+0x58` (uint32) holds the object frame code. It is compared with and set to `g_ObjectFrameCode = RenderStruct->IncObjectFrameCode()`, which is called once per RenderScene. It ensures an object is processed once, even when it is reached through several leaves, nodes or attachments. The engine's vis walk uses the same field as its visit mark.
* `RenderContext+0x0c` (uint16) is the world frame code, bumped by `r_IncrementFrameCode`. On wrap it is reset to 1 and every BSP's leaves and polys are reset with r_ResetWorldFrameCode. Leaves keep it at +0x2c and polys at +0x42.
* `SharedTexture+0x30` (uint16) is stamped with `g_TextureFrameCode` whenever a texture is bound (`r_SetTexture` 0x15e644). The engine uses it to tell which textures were used this frame.

## 3. LTObject: the common layout as the renderer reads it

The same layout holds on PC and Mac. Sources are src/engine/server/objtypes.h:44-157 and ltobject_internal.h:200-290, cross-checked against the Mac accessors (`ci_GetObjectColor` 0x8410 reads 0x94..0x97, `ci_GetObjectUserData` 0xc0, `ci_GetObjectClientFlags` 0x180, `LTObject::SetupTransform` 0xc8640 uses 0x18c/0x9c/0xac).

```c
struct LTObject {                 // the vptr is at +0x00 (virtuals include GetTransform / radius, unused by the renderer)
 /*058*/ uint32     m_FrameCode;      // renderer/vis "already processed this frame" stamp (2.7)
 /*088*/ uint32     m_Flags;          // FLAG_*
 /*08c*/ uint32     m_Flags2;         // FLAG2_*
 /*090*/ uint32     m_UserFlags;
 /*094*/ uint8      r, g, b, a;       // object colour; a=255 is opaque (models with a!=255 are translucent)
 /*098*/ Attachment* m_pAttachments;  // singly linked by +0x24
 /*09c*/ LTRotation m_Rotation;       // quaternion float[4] = x,y,z,w (ltrotation.h:67,83)
 /*0ac*/ LTVector   m_Scale;
 /*0b8*/ uint16     m_ObjectID;
 /*0bc*/ int8       m_ObjectType;     // OT_* (sign-extended by readers)
 /*0c0*/ void*      m_pUserData;      // (client)
 /*0f0*/ LTVector   m_BBoxMin, /*0fc*/ m_BBoxMax;   // world-space box
 /*180*/ uint16     m_ClientFlags;    // CF_*
 /*188*/ void*      m_188;            // see r_AddRenderObjects
 /*18c*/ LTVector   m_Pos;
 /*198*/ LTVector   m_Dims;
 /*1a4*/ float      m_Radius;         // bounding sphere, used by the vis walk
 /*1a8*/ ...                          // type-specific from here (light radius, worldmodel BSP, ModelInstance fields, ...)
};
struct Attachment {               // ltobject_internal.h:188
 /*00*/ LTransform m_Offset;        // pos + rot
 /*1c*/ uint16 m_nParentID, /*1e*/ m_nChildID;
 /*20*/ int    m_iNode;             // node/socket, -1 for none
 /*24*/ Attachment* m_pNext;
};
```
**Object matrix** (engine `mat_SetupTransform`, Mac 0xb62d0, which LTObject::SetupTransform calls): `M = quat_ConvertToMatrix(rot)` (ltquatbase.cpp:11, row-major, column-vector convention). Column 0 is multiplied by `scale.x`, column 1 by `scale.y` and column 2 by `scale.z`, and `M[0..2][3] = pos`. So `world = M * local`, which is `T * R * S`. For OpenGL, load the transpose or use `glLoadTransposeMatrixf`.

**Object flags** (src/LT2/sdk/inc/ltbasedefs.h:1057-1196). The bits are overloaded per type:

| bit | value | name(s) |
|---|---|---|
| 0 | 0x1 | FLAG_VISIBLE |
| 1 | 0x2 | FLAG_SHADOW (model casts a shadow) / FLAG_UNSIGNED (polygrid) |
| 2 | 0x4 | FLAG_SPRITEBIAS / FLAG_MODELTINT |
| 3 | 0x8 | FLAG_ROTATEABLESPRITE / FLAG_DETAILTEXTURE (model) / FLAG_CASTSHADOWS (light) / FLAG_UPDATEUNSEEN (PS) |
| 4 | 0x10 | FLAG_SPRITE_NOZ / FLAG_SOLIDLIGHT / FLAG_MODELWIREFRAME / FLAG_WASDRAWN (PS) |
| 5 | 0x20 | FLAG_GLOWSPRITE / FLAG_ENVIRONMENTMAP (model) / FLAG_ENVIRONMENTMAPONLY (polygrid) / FLAG_ONLYLIGHTWORLD (light) |
| 6 | 0x40 | FLAG_REALLYCLOSE / FLAG_DONTLIGHTBACKFACING (light) |
| 7 | 0x80 | FLAG_FOGDISABLE / FLAG_ANIMTRANSITION / FLAG_ONLYLIGHTOBJECTS (light) |
| 8 | 0x100 | FLAG_FULLPOSITIONRES |
| 9 | 0x200 | FLAG_NOLIGHT ("just use the object's colour and global light scale") |
| 10 | 0x400 | FLAG_HARDWAREONLY / FLAG_PORTALVISIBLE (the renderer tests it as PORTALVISIBLE) |
| 11..31 | | physics, network and engine bits (FLAG_YROTATION, RAYHIT, SOLID, ..., FLAG_NOSLIDING 1<<25, FLAG_INTERNAL1/2); none of them matter for drawing |

There is **no FLAG_FULLBRIGHT in this engine version**. The fullbright-like flag is FLAG_NOLIGHT.

FLAG2: `0x1 FLAG2_PORTALINVISIBLE`, `0x2 FLAG2_CHROMAKEY` (models and world models), `0x4 FLAG2_ADDITIVE` (models and sprites), `0x8 FLAG2_MULTIPLY`, `0x10 FLAG2_PLAYERCOLLIDE`, `0x20 FLAG2_DYNAMICDIRLIGHT` (models), `0x40 FLAG2_SKYOBJECT`.

Object types (ltbasedefs.h:1199-1211): `OT_NORMAL 0, OT_MODEL 1, OT_WORLDMODEL 2, OT_SPRITE 3, OT_LIGHT 4, OT_CAMERA 5, OT_PARTICLESYSTEM 6, OT_POLYGRID 7, OT_LINESYSTEM 8, OT_CONTAINER 9, OT_CANVAS 10`. Type 0 and type 5 have no handlers (2.3) and are never drawn.

Type-specific fields start at +0x1a8. The object banks give the sizes: light 0x1ac with `m_LightRadius` at +0x1a8; camera rect +0x1a8, xFov +0x1b8, yFov +0x1bc (objtypes.h names these two the other way round; Mac cm_Render copies +0x1b8 into SceneDesc.m_xFov), bFullScreen +0x1c0, LightAdd +0x1c4; canvas `fn` +0x1a8, `user` +0x1ac, radius +0x1b0; world model BSP +0x1a8 with matrices +0x1f0/+0x230; container code +0x270 (objtypes.h). The model, sprite and other layouts are given in the per-type sections.



# Part II — Models (OT_MODEL): data, transforms, lighting, drawing

Scope: everything a renderer needs to draw an `OT_MODEL` object. All offsets are
byte offsets on the PC (x86, lithtech.exe / d3d.ren). The Mac (PowerPC, CodeWarrior)
binary turns out to use the same offsets for every field read below. Each one was
checked on both sides wherever a PC reader exists.

Conventions used for evidence:
- **Mac `Name` @addr**: disassembly of the Mac engine
  (`ext/tools/pef.py dis - <name>`). The address is the code offset.
- **PC 0x1xxxxxxx**: d3d.ren. **PC 0x004xxxxx**: lithtech.exe.
  **srv 0x100xxxxx**: server.dll (same engine classes).
- *Inferred*: read from the code's shape, or from the math, rather than
  from a named field or a string.

---

### 0. Answer to the critical question: where node transforms come from

**On PC, d3d.ren evaluates the animation itself.** It does this with its own
compiled copy of the engine's `TransformMaker` code, reading the engine's
`Model` / `ModelInstance` memory directly. There is no RenderStruct callback for
this. It needs exactly two engine services, and it reaches both through the C++
vtable of the `ModelInstance` object. Those vtable slots hold engine code in
lithtech.exe, and d3d.ren calls them in-process as `__thiscall`:

| vtable slot | engine function (srv addr / Mac name) | what it does |
|---|---|---|
| +0x38 | `CObjModel::SetupModelWalker` srv 0x10024fda / Mac `ModelInstance::SetupTransformMaker` @0c98c0 | `tm->nTimeRefs=0; for (t = inst->m_pTrackerList(+0x1f8); t && n<8; t = t->next(+4)) t->SetupTimeRef(&tm->timeRefs[n++]);` then `tm->+0xec = inst->+0x1fc` (NodeControlFn) and `tm->+0xf0 = inst->+0x200` (its user data). |
| +0x3c | `CObjModel::SetupModelWalkerRefsOnly` srv 0x10025035 / Mac `ModelInstance::SetupGVPTime` @0c99a0 | Same gather, without the two hook fields. The renderer passes `&ModelDraw+0x4e8` (PC `&m_n4e4`), a second time-ref set that the vertex cache uses. **Not needed to draw.** |

Evidence:
- PC d3d.ren 0x100183c1 calls slot +0x3c and 0x10018401 calls slot +0x38 on
  the ModelInstance (src/engine/d3d/w43_10017c10_osoiqipni.cpp:1980-1990). The
  file's notes say lithtech.exe's bodies (e.g. 0x0045b390) are `__thiscall`
  `ret $4` (w43…:850-859).
- Then d3d.ren calls **its own** `0x1007ca60` (= `TransformMaker::SetupTransforms`;
  its failure string is "Model::SetupTransforms failed for %s."). That calls
  `0x1007cab4` (Prepare/SetupCall), `0x1007cc90` (Recurse / ProcessNode) and
  `0x1007d624` / `0x1007da80` / `0x1007d880` (BlendNode / GetNodeTransform /
  GetNodeTransformRel). See src/engine/d3d/w10_1007cab4_os.cpp:1-34 ("rewritten
  from their lithtech.exe twin (src/engine/lithtech/transform.cpp …)") and
  src/engine/d3d/big7cc90.cpp.
- The Mac renderer is linked into the engine, so it calls the same engine
  functions directly: `DrawModel__9ModelDraw` @163f7c..163fdc does a vcall to
  slot 0x44, a vcall to slot 0x40, then `bl SetupTransforms__14TransformMaker`.
  The Mac vtables carry an 8-byte header, so Mac 0x40/0x44 are PC 0x38/0x3c
  (*inferred* from that header, but it matches the PC call sites exactly).
- `LTAnimTracker::SetupTimeRef` (Mac @05a2f0) is a pure copy of the tracker's
  `AnimTimeRef` (tracker +0x1c..+0x37). **The renderer does no animation timing.**
  The engine's tracker update (`cm_UpdateModels`, `trk_*`) has already set the
  frames and `m_Percent` for this frame.

**Recommendation for opengl.ren:** do the same. Call the instance's
vtable +0x38 to fill a TransformMaker. Then run your own copy of the
TransformMaker algorithm (section 3) over the engine's Model memory. The source
already exists as `CTransformMakerK` in src/engine/lithtech/transform.cpp
(section B, PC 0x00489a60..0x0048a4ab), so compile it into the DLL. You could
instead fill the time refs yourself from `inst->+0x1f8`, since `SetupTimeRef` is a
plain copy. The vtable call is what retail did.

---

### 1. Data layout

#### 1.1 ModelInstance (LTObject + model part), 0x248 bytes on PC

The common LTObject fields are the parent's section. These are the
model-specific fields plus the common ones the model draw reads.

```c
// srv objbank.cpp:144-175 (CObjModel, 0x248 bytes), commonmodel.h:1149
struct ModelInstance /* : LTObject */ {
  /* +0x000 */ void*      vtbl;            // slot +0x38 SetupTransformMaker, +0x3c SetupGVPTime
  /* +0x088 */ uint32     m_Flags;         // FLAG_*  (see 1.6)
  /* +0x08c */ uint32     m_Flags2;        // FLAG2_* (chromakey 2, additive 4, multiply 8, dynamicdirlight 0x20)
  /* +0x094 */ uint8      r, g, b, a;      // object colour; a<255 => translucent model
  /* +0x09c */ LTRotation m_Rotation;      // quaternion x,y,z,w
  /* +0x0ac */ LTVector   m_Scale;
  /* +0x0bc */ char       m_ObjectType;    // OT_MODEL == 1
  /* +0x180 */ uint16     m_n180;          // bit 3 (0x8): see "instance hook" (PC w43 0x10019140, Mac DrawModel @164384)
  /* +0x18c */ LTVector   m_Pos;
  /* +0x1b4 */ ModelInstanceHookFn m_HookFn;   // ILTModel::SetInstanceHookFn stores here (Mac @0bf8bc)
  /* +0x1b8 */ SharedTexture* m_pSkins[2];     // per-piece skin, indexed by ModelPiece::m_iTexture (<2)
                                               //   Mac SetModelTexture @188da0..188dac; ModelSetTexture @036e0c
  /* +0x1c0 */ LTAnimTracker m_AnimTracker;    // main tracker, 0x38 bytes
  /*   +0x1dc */   //  = m_AnimTracker.m_TimeRef.m_pModel  -> Model*   (THE model pointer; r_DrawModel @15f05c)
  /*   +0x1e0 */   //  = tracker m_Prev.m_iAnim (uint16)
  /*   +0x1e8 */   //  = tracker m_Cur.m_iAnim  (uint16)
  /* +0x1f8 */ LTAnimTracker* m_pTrackerList;  // ALL trackers, singly linked through tracker+0x04 (main tracker included: inferred, it is the only list the TM reads)
  /* +0x1fc */ NodeControlFn  m_NodeControlFn; // CLTClient::ModelNodeControl stores here (Mac @00f570)
  /* +0x200 */ void*          m_pNodeControlUser;
  /* +0x204 */ uint32         m_HiddenPieces;  // bit i => piece i not drawn (Mac SetupVerts @187d14)
  /* +0x208 */ ...                             // (ModelSetTexture writes it; skin bookkeeping)
  /* +0x238 */ LTVector  m_vSunCachePos;       // renderer cache: position the sun test was done at
  /* +0x244 */ float     m_fSunCacheAmount;    // renderer cache: dir-light (sun) visibility 0..1, <0 = invalid
};                                             // Mac SetupModelLight @160f5c..161040
```

`r_DrawModel` (Mac @15f040) and PC 0x10015c70 both refuse to draw when
`Model* == NULL` or either tracker anim index is `>= model->m_AnimArray.count (+0x16c)`.

#### 1.2 LTAnimTracker / AnimTimeRef (SDK src/LT2/sdk/inc/ltanimtracker.h, offsets confirmed by Mac SetupTimeRef @05a2f0)

```c
struct AnimTimeRef {            // 0x1c bytes
  /* +0x00 */ Model* m_pModel;
  /* +0x04 */ uint16 prevAnim;  /* +0x06 */ uint16 prevFrame;  /* +0x08 */ uint32 prevTime;
  /* +0x0c */ uint16 curAnim;   /* +0x0e */ uint16 curFrame;   /* +0x10 */ uint32 curTime;
  /* +0x14 */ float  m_Percent; // 0..1 between prev and cur keyframe
  /* +0x18 */ uint32 m_iWeightSet; // ignored for the first time-ref
};
struct LTAnimTracker {          // 0x38 bytes
  /* +0x00 */ LTLink m_Link;    // +0x04 = next tracker, +0x08 = owning ModelInstance
  /* +0x0c */ StringKeyCallback; /* +0x10 */ void* m_pUser1; /* +0x14 */ uint32 m_CurKey;
  /* +0x18 */ uint16 m_Flags;   /* +0x1a */ uint16 m_InterpolationMS;
  /* +0x1c */ AnimTimeRef m_TimeRef;
};
```

#### 1.3 Model (srv commonmodel.h:418-640; arrays confirmed by lithtech transform.cpp section B, Mac and PC d3d)

`CMoArray<T>` is `{ +0 cache/vtbl, +4 T* pArray, +8 uint32 nElements }`.
Every array below is given as "array ptr / count".

```c
struct Model {
  /* +0x004 */ char*  m_pFilename;
  /* +0x028/+0x02c */ ModelNode**  nodes, nNodes;           // CMoArray at 0x24
  /* +0x038/+0x03c */ ModelPiece** pieces, nPieces;         // CMoArray at 0x34 (PC w14_10008a40 D3DModel8a40; Mac CalcNumVerts @162d4c)
  /* +0x048/+0x04c */ ModelWeightSet** weightSets, nWeightSets;  // CMoArray at 0x44
  /* +0x064 */ uint32 nVerts?                                // (PC D3DModel::m_n64, "NumVerts")
  /* +0x074/+0x078 */ LTMatrix* nodeXforms, nNodeXforms;    // OUTPUT of TransformMaker, 64 B / node
  /* +0x080 */ char*  m_pCommandString;
  /* +0x090/+0x094 */ float* lodDist, nExtraLODs;           // distances of LOD 1..n  (Mac SelectLOD @1646dc)
  /* +0x0a0 */ float  lodDist0;                             // distance of LOD 0 (0.0)
  /* +0x0a4 */ float  m_Radius;                             // 96.0 default; ×max(scale) = frustum-cull radius (Mac r_GetModelClipMask @15ee78)
  /* +0x0a8 */ float  m_VisRadius;                          // cmd "VisRadius", 50.0 default; lighting-gather radius,
                                                            //   srv GetScaledRadius = max(scale)*this (objbank.cpp:440)
  /* +0x0ac */ uint32 m_bNoAnimation;                       // cmd "NoAnimation"
  /* +0x0b0 */ float  fadeA;  /* +0x0b4 */ float fadeA2;    // Model::SetFadeRange(a,b) Mac @060220: b0=a, b4=a², b8=b, bc=b²
  /* +0x0b8 */ float  fadeB;  /* +0x0bc */ float fadeB2;    // cmds "FadeRangeMin"→a, "FadeRangeMax"→b; ctor default 20000
  /* +0x0c0 */ SharedTexture* m_pFadeSprite;                // billboard texture for the far LOD (Mac DrawModelSprite @161a10)
  /* +0x0c4/+0x0c8 */ float fadeSpriteW, fadeSpriteH;       // cmd "FadeSpriteSize w h" (100,100)
  /* +0x0cc */ float  m_AmbientLight;  /* +0x0d0 */ float m_DirLight;   // cmds (0.25, 0.75)
  /* +0x0d4 */ uint32 m_bShadowEnable;                      // cmd "ShadowEnable"
  /* +0x0d8 */ float  shadowProjectLength; /* +0x0dc */ shadowLightDist;   // (200,200)
  /* +0x0e0 */ float  shadowSizeX;  /* +0x0e4 */ shadowSizeY;              // (50,50)
  /* +0x0e8 */ LTVector shadowCenterOffset;                 // cmd "ShadowCenterOffset x y z"
  /* +0x0f4 */ uint32 normalRefNode; /* +0x0f8 */ uint32 normalRefAnim;   // cmd "NormalRef node anim"
  /* +0x0fc */ LTMatrix m_NormalRefMat;                     // computed by running TransformMaker on that anim (Mac @0600b8..060154)
  /* +0x13c */ uint32 m_bNormalRef;
  /* +0x140 */ uint32 m_bFovOffset; /* +0x144 */ float fovXOffset; /* +0x148 */ float fovYOffset;   // radians; cmds "FovXOffset/FovYOffset" (deg)
  /* +0x14c */ uint32 m_bSpecularEnable;                    // cmd "SpecularEnable"
  /* +0x150 */ uint32 m_bRigid;                             // cmd "Rigid"
  /* +0x168/+0x16c */ AnimInfo* anims, nAnims;              // CMoArray at 0x164, 0x20-byte AnimInfo
  /* +0x190 */ ChildInfo* m_pChildModels[16]; /* +0x1d0 */ uint32 nChildModels;
  /* +0x1d4 */ ChildInfo  m_SelfChildModel;
  /* +0x2c0 */ ModelNode* m_pRootNode;
};
```

The command-string parsing is Mac `Model::ParseCommandString` @05fc80. The
7-entry float table at data+0x128cc maps VisRadius→+0xa8, AmbientLight→+0xcc,
DirLight→+0xd0, ShadowProjectLength→+0xd8, ShadowLightDist→+0xdc,
ShadowSizeX→+0xe0 and ShadowSizeY→+0xe4. The other keys are compared one by one
(see the stores at @05ff5c..0601bc).

```c
struct ModelNode {             // transform.cpp section B ModelNodeK; commonmodel.h ModelNode
  /* +0x04 */ LTVector m_vOffset;     // used INSTEAD of the animated translation when (m_Flags & 2)
  /* +0x10 */ uint16   m_iNode;       // own index
  /* +0x12 */ uint16   m_Flags;
  /* +0x1c/+0x20 */ ModelNode** children, nChildren;
  /* +0x28 */ uint32   m_iParent;     // 0xffffffff at root
  /* +0x2c */ LTMatrix m_Transform;   // bind-pose matrix (not used by drawing)
  /* +0xb0 */ char*    m_pName;
};
struct AnimInfo {              // 0x20 bytes
  /* +0x00 */ ModelAnim* m_pAnim;
  /* +0x04 */ ChildInfo* m_pChildModel;   // model the anim comes from (self for own anims)
  /* +0x08 */ LTVector   m_vDims;
  /* +0x14 */ LTVector   m_vTranslation;  // added to node 0 only (transform.cpp GetNodeTransform)
};
struct ModelAnim {             // +0x04: AnimNode** (per node), via CMoArray at +0x00
  /* +0x0c */ AnimKeyFrame* keys; /* +0x10 */ uint32 nKeys; /* +0x24 */ char* name; };
struct AnimNode { /* +0x0c */ CModelXform* m_pXforms; };   // one per keyframe
struct CModelXform { LTVector m_Pos; LTRotation m_Rot; };  // 0x1c bytes
struct ChildInfo { /* +0x04 */ CModelXform* relation; ... }; // per-node "relation" xform; CMoArray at +0
struct ModelWeightSet { char name[0x10]; /* +0x14 */ float* weights; /* per node */ };
```

#### 1.4 Pieces, LODs, vertices, triangles (Mac SetupSkeletalVerts / DrawTrisUnclipped / SetupVerts; PC w14_10008a40, w43 0x1001bf34)

```c
struct PieceLOD {              // 0x24 bytes. A ModelPiece BEGINS with its LOD-0 PieceLOD
  /* +0x00 */ uint32     ?;
  /* +0x04 */ ModelVert* m_Verts;
  /* +0x08 */ uint32     m_nVerts;
  /* +0x14 */ ModelTri*  m_Tris;
  /* +0x18 */ uint32     m_nTris;
};
struct ModelPiece /* : PieceLOD */ {
  /* +0x24 */ uint32  m_iTexture;        // skin index -> ModelInstance::m_pSkins[i]   (Mac DrawTris @186aa8 -> SetModelTexture)
  /* +0x2c */ float   m_SpecularPower;   // -> ModelDraw+0x46c (cvar SpecularPowerTest overrides)
  /* +0x30 */ float   m_SpecularScale;   // -> ModelDraw+0x470 (cvar SpecularScaleTest overrides)  Mac SetupVerts @187d20
  /* +0x38/+0x3c */ PieceLOD* lods, nLODs;  // LODs 1..n
  /* +0x48 */ char    m_Name[0x20];
};
PieceLOD* GetLOD(ModelPiece* p, uint32 i)
  { return i==0 ? (PieceLOD*)p : (i-1 < p->nLODs ? &p->lods[i-1] : NULL); }  // PC 0x1001bf34 (identical)

struct ModelVert {             // 0x20 bytes
  /* +0x00 */ NewVertexWeight* m_Weights;
  /* +0x04 */ uint16 m_nWeights;
  /* +0x06 */ uint16 m_iReplacement;  // index of this vertex in the NEXT LOD (LOD cross-blend)
  /* +0x08 */ LTVector m_Vec;         // model-space (bind) position: used for lighting and for the rigid path
  /* +0x14 */ LTVector m_Normal;      // model-space normal: used for lighting / env map / specular
};
struct NewVertexWeight {       // 0x14 bytes
  /* +0x00 */ float  m_Vec[4];        // node-space position; [3] is the weight. Inferred: xyz pre-multiplied by weight
  /* +0x10 */ uint32 m_iNode;
};
struct ModelTri {              // 0x20 bytes (Mac DrawTrisUnclipped: stride @185fc4, idx @185c28)
  /* +0x00 */ uint16 m_Indices[3];    // into this PieceLOD's m_Verts
  /* +0x06 */ uint16 pad;
  /* +0x08 */ float  uv[3][2];        // per-corner (u,v)
};
```

#### 1.5 Model-related cvars the Mac renderer registers (defaults)

These come from `__sinit_setupmodel_cpp` @16a0a0, `__sinit_common_stuff_cpp`
@14e6d0, and the sinit around @187e38. On Mac a ConVar object keeps its int
value at +0 and its float at +4.

DrawModels 1, LightModels 1, MaxModelLights 4 (clamped to 16), ModelFullbrite 1,
TextureModels 1, DrawGuns 1 (draws `FLAG_REALLYCLOSE` models at all),
DrawModelsRigid 1, ModelCacheRigid 0, ModelUseTnL 0, ModelLODOffset 0,
ModelLODBlendEnable 1, ModelLODBlendDist 60, ExtraFOVXOffset/YOffset 0,
ModelFovTest 0, ModelSunVariance 0.05, ModelApplySun 1, ModelSaturation 2 (used
when Saturate=1), EnvMapEnable 0, EnvMapAll 0, EnvScale 1, EnvPanSpeed 0.0005,
TintModels 1, ModelWarble 0 / WarbleSpeed 25 / WarbleScale 0.92, ModelSpecular 1,
ModelDetailTextureScale 3, ModelMinTri 0.2, MaxModelShadows 1, ModelShadowAlpha
230, ModelShadowOffset 0.1, ModelShadowProj 0, ModelBoxes 0 (debug box),
ModelProfile 0.

#### 1.6 Object flags the model path reads (bit, where)

| flag | value | effect (Mac evidence) |
|---|---|---|
| FLAG_SHADOW | 0x2 | `DrawModelShadows` after the model (DoModelDrawing @189844 tests the *hook-modified* flags at ModelDraw+0x69c) |
| FLAG_MODELTINT | 0x4 | `m_478 = TintModels && (flags&4) && a==255` (DrawModel @1633e0). **No reader found on Mac** (inferred: unused) |
| FLAG_DETAILTEXTURE | 0x8 | ModelDraw+0x30 = `(flags>>3)&1` (@163014). Draws a 2nd stage with the skin's linked texture (`SharedTexture+0x28`), uv × ModelDetailTextureScale |
| FLAG_ENVIRONMENTMAP | 0x20 | chrome pass (section 5.4) |
| FLAG_REALLYCLOSE | 0x40 | camera-space rendering with special view matrices, near plane 0.01 (section 5.1) |
| FLAG_FOGDISABLE | 0x80 | translucent draw: `glDisable(GL_FOG)` (r_DrawTransparentModel @189e8c) |
| FLAG_NOLIGHT | 0x200 | no lighting: vertex colour = 255·colorScale + lightAdd (SetupModelLight @160edc) |
| FLAG2_CHROMAKEY | 0x2 (Flags2) | goes into the chromakey set (r_ProcessModel @189aa4) |
| FLAG2_ADDITIVE | 0x4 (Flags2) | translucent only: `glBlendFunc(ONE,ONE)`, fog colour constant |
| FLAG2_MULTIPLY | 0x8 (Flags2) | translucent only: `glBlendFunc(ZERO,SRC_COLOR)`, fog colour constant |
| FLAG2_DYNAMICDIRLIGHT | 0x20 (Flags2) | recompute the sun visibility when the model moves more than ModelSunVariance |

There is **no FLAG_FULLBRIGHT** in this SDK (grep of src/LT2/sdk/inc). "Fullbrite"
is a per-texture property, handled by the ModelFullbrite cvar (InitTextureInfo
@188c2c).

---

### 2. Per-frame flow for one model (Mac `r_DrawModel` @15f040, `ModelDraw::DrawModel` @162e60; PC 0x10015c70 → `CD3D_ModelDraw::Draw` 0x10017c14 on the global ModelDraw at 0x1008b090)

The PC and Mac `ModelDraw` objects have the same field layout at every offset
compared. PC w43 `CD3DModelDraw` names m_Mat310/350/390/3d0, m_vPos410,
m_pfn41c/420, m_n44c/450/454/458, m_n478/47c/480, m_6ac/6b0, and these match
the Mac reads.

```
r_ProcessModel(obj):               // Mac @189a30: sort into sets
   if obj.a != 255          -> translucent set   (drawn back-to-front later by r_DrawTransparentModel)
   else if Flags2 & CHROMAKEY -> chromakey set
   else                     -> solid set          (r_DrawSolidModels: glBlendFunc(SRC_ALPHA,1-SRC_ALPHA), Draw each)

r_DrawModel(vp, inst):
   if (!inst.model || prevAnim>=nAnims || curAnim>=nAnims) return
   if (!r_GetModelClipMask(...)) return       // sphere (model.m_Radius*max(scale)) vs 6 frustum planes
   ModelDraw::DrawModel(inst)

DrawModel(inst):
  model = inst.+0x1dc;  pos = inst.m_Pos (REALLYCLOSE: pos transformed into camera space by vp matrix @vp+0x9c)
  if (!DrawModelsRigid && model.m_bRigid) return;    if (!DrawGuns && REALLYCLOSE) return
  fogFn(&pos, &m_480)                         // per-model fog factor from the centre (PC g_pfn1008ce8c 0x10017d29)
  d2 = |camPos - pos|²
  // fade/LOD-sprite (section 6)
  alpha (m_6ac) = inst.a, possibly faded
  envOn (m_458) = (EnvMapAll || flags&FLAG_ENVIRONMENTMAP) && EnvMapEnable && RenderStruct.m_pEnvMap(+0x104)
  m_47c = sqrt(d2);  SelectLOD()               // section 4
  ObjXform (m_Mat310) = T(pos) * R(quat, sanitised: |c|>1e5 -> 0, w->1) * S(scale)
  View    (m_Mat350)  = ViewProj * ObjXform  (software path) | ObjXform (TnL path) | special FOV proj (model.m_bFovOffset)
  m_Mat390 = inverse(m_Mat350)
  SizeAllLists()
  if REALLYCLOSE: clip near plane to 0.01
  -- transforms --
  inst.vtbl[+0x3c](&m_4e8)                     // GVP time refs (cache only)
  TM tm; inst.vtbl[+0x38](&tm)                  // fill time refs + NodeControlFn
  tm.m_hObj(+0xf4) = inst
  tm.m_pBase(+0xe4) = tm.NodeControlFn ? &ObjXform : &m_Mat350
  if (!tm.SetupTransforms()) { print "Model::SetupTransforms failed for %s."; return }
  if (tm.NodeControlFn) for each node i: model.nodeXforms[i] = ViewProjOrView * model.nodeXforms[i]
  // => node matrices ALWAYS end up as   ViewProj * ObjXform * animated-node   (software path)
  //    the NodeControlFn sees them in WORLD space (base = ObjXform)
  if (!clip) ApplyToExtraTransforms(vp+0x19c)    // precomputes ModelDraw+0x644 per-node matrices for the unclipped path
  SetupModelLight()                              // section 5
  env normal matrix m_Mat3d0 (section 5.4)
  if (inst.m_NodeControlFn && !(inst.m_n180 & 8)) clip |= 0x3f   // force full clipping when the game moves nodes
  instance hook: if inst.m_HookFn: data.m_Flags=0; m_HookFn(inst, &data /*ModelDraw+0xc*/, 0);
       if (data.m_Flags & MIH_CLIPPLANE) add user clip plane data.m_ClipPlane (world space) -> clip bit 0
  DoModelDrawing()                               // section 7
```

---

### 3. TransformMaker: exact algorithm (the node matrices)

Source: src/engine/lithtech/transform.cpp section B (`CTransformMakerK`,
lithtech.exe 0x00489a60..0x0048a4ab). The same code is d3d.ren
0x1007ca60/0x1007cab4/0x1007cc90/0x1007d624/0x1007da80/0x1007d880, and
Mac `TransformMaker::*` @07b700..07ca7c (SetupCall @07b9c0 matches Prepare
field for field).

```c
struct TransformMaker {           // layout identical PC/Mac
  AnimTimeRef m_TimeRefs[8];      // +0x000
  uint32      m_nTimeRefs;        // +0x0e0
  LTMatrix*   m_pBaseTransform;   // +0x0e4  parent of the root node; NULL => identity
  LTMatrix*   m_pTransforms;      // +0x0e8  output; NULL => model->nodeXforms (+0x74)
  NodeControlFn m_NodeControlFn;  // +0x0ec
  void*       m_pNodeControlData; // +0x0f0
  HOBJECT     m_hObject;          // +0x0f4
  uint32*     m_pNodePath; uint32 m_nNodePath;   // +0x0f8/+0x0fc (single-node mode)
  ...                             // +0x100..+0x187 scratch
  LTRotation  m_rBlend;           // +0x188
  LTVector    m_vBlend;           // +0x198
  Model*      m_pModel;           // +0x1a4
  LTMatrix    m_matIdentity;      // +0x1a8
  ModelWeightSet* m_pWeightSets[8]; // +0x1e8
  ModelAnim*  m_pPrevAnims[8];    // +0x208
  ModelAnim*  m_pCurAnims[8];     // +0x228
};                                // >= 0x248 bytes (Mac DrawModel reserves 0x5a0..0x7e0 for it)

bool SetupTransforms() {
  // IsValid: 1..8 refs, every ref valid, all the same model, weightset < model->nWeightSets for i>0
  m_pModel = m_TimeRefs[0].m_pModel;
  for i: m_pWeightSets[i]=model->weightSets[ref[i].m_iWeightSet] (i>0);
         m_pPrevAnims[i]=model->anims[ref[i].prevAnim].m_pAnim; m_pCurAnims[i]=model->anims[ref[i].curAnim].m_pAnim;
  if (!m_pBaseTransform) m_pBaseTransform = identity;
  if (!m_pTransforms)    m_pTransforms    = model->nodeXforms;          // +0x74
  animOwner = model->anims[ref[0].prevAnim].m_pChildModel;             // relation xforms
  ProcessNode(model->m_pRootNode->m_iNode, m_pBaseTransform);
}

GetNodeTransform(t, node, &rot, &pos):    // keyframe lerp/slerp
  A = m_pPrevAnims[t]->animNodes[node]->m_pXforms[ref[t].prevFrame]
  B = m_pCurAnims[t] ->animNodes[node]->m_pXforms[ref[t].curFrame]
  rot = slerp(A.rot, B.rot, ref[t].m_Percent);  pos = lerp(A.pos, B.pos, pct)
  if node==0: pos += lerp(anims[prevAnim].m_vTranslation, anims[curAnim].m_vTranslation, pct)

BlendNode(t, node):                        // tracker t>0 blended by its weight set
  w = m_pWeightSets[t]->weights[node]
  if w == 0: return
  if w == 2.0:  (additive) GetNodeTransformRel: rot,pos relative to the anim's FIRST keyframe
                (rot = ~key0.rot * rot; pos -= key0.pos);  m_rBlend = m_rBlend * rot; m_vBlend += pos
  else:         GetNodeTransform; m_rBlend = slerp(m_rBlend, rot, w); m_vBlend = lerp(m_vBlend, pos, w)

ProcessNode(node, parent):                 // recursive (tail-iterates the last child)
  GetNodeTransform(0, node, &m_rBlend, &m_vBlend)
  for t in 1..n-1: BlendNode(t, node)
  L = quat_ToMatrix(m_rBlend); L.translation = (nodeFlags & 2) ? node->m_vOffset : m_vBlend
  tmp = parent * L
  rel = animOwner->relation[node]          // CModelXform; identity-like for the model's own anims (inferred)
  out[node] = tmp * Matrix(rel.rot, rel.pos)
  if (m_NodeControlFn) m_NodeControlFn(m_hObject, node, &out[node], m_pNodeControlData)
  for each child c: ProcessNode(c->m_iNode, &out[node])
```

LTMatrix is row-major `float m[4][4]` with the translation in `m[0..2][3]`
(transform.cpp ToMatrix notes), and points are transformed as `M * (x,y,z,1)`.
**The output node matrices include whatever base you pass.** For GL, pass
`ObjXform` (world) as the base. That way the NodeControlFn gets world-space
matrices, as it does on Mac and PC, and you skin straight into world space.

Note: the node matrices are written into the **shared Model** (+0x74), not into
the instance. The engine's own `ILTModel::GetNodeTransform` does the same, so
recompute them per instance immediately before skinning.

---

### 4. LOD selection and vertex skinning

#### 4.1 SelectLOD (Mac @1646c0; PC 0x10019644)

```
d = m_47c (distance camera->model)
if ModelLODOffset > 0: d += lodDist(min(off, nExtraLODs))      // lodDist(0)=model+0xa0, lodDist(i)=model+0x90[i-1]
if ModelLODOffset < 0: d -= lodDist(min(-off, nExtraLODs))
lod (m_44c) = last i in 0..nExtraLODs with d > lodDist(i)      (0 if none)
m_450 = 0
if ModelLODBlendEnable && lod+1 <= nExtraLODs:
   width = min(ModelLODBlendDist, 0.5*(lodDist(lod+1)-lodDist(lod)))
   if (lodDist(lod+1) - d) < width:  m_454 = 1 - (lodDist(lod+1)-d)/width ;  m_450 = 1   // geomorph toward next LOD
```

#### 4.2 SetupVerts / SetupSkeletalVerts (Mac @187bd0 / @186d80)

```
xforms = clipping ? model->nodeXforms : ModelDraw+0x644      // same values, different precompute
outBase = 0
for piece i in model pieces:
   if inst.m_HiddenPieces & (1<<i): continue
   L  = GetLOD(piece, lod);  if !L: continue
   L2 = m_450 ? GetLOD(piece, lod+1) : NULL
   specPower/specScale = piece+0x2c/+0x30 (cvar overrides)
   for each v in L.m_Verts:                         // out vertex k = outBase + index
      P = Σ_w  xforms[w.m_iNode] * (w.vec[0..3])      // full 4x4, w component accumulates too
      if L2: P2 = same for L2.m_Verts[v.m_iReplacement];  P = P + m_454*(P2-P)
      out.xyz = P.xyz / P.w ; out.rhw = 1/P.w          // software path: matrices include projection
      colour = light(v.m_Vec, v.m_Normal)  (section 5.3), alpha byte = m_6ac
      uv1    = TCalc(v)                     (env map / specular / none)
   outBase += L.m_nVerts
```

For OpenGL with world-space node matrices: `pos_world = Σ M_node · (vec.xyz, vec.w)`.
Divide by the accumulated w, which equals Σweights ≈ 1. That division is a no-op
for affine matrices and handles imperfectly normalised weights the same way the
original did. Lighting uses the **bind-pose** model-space position and normal
(`m_Vec`, `m_Normal`) against lights transformed into model space. **Normals are
not skinned** (Mac SetupSkeletalVerts @187728..1879b8 reads v+0x08..0x1c, not the
skinned result).

The rigid/non-skeletal path (`TransformNormal` @15ebc0, `ModelWarble` variant) is
`out = M * v.m_Vec` with a projective divide.

---

### 5. Lighting

#### 5.1 Matrices and special projection

- FLAG_REALLYCLOSE: Mac uses ViewParams matrices at vp+0x25c / +0x2dc / +0x430 (the
  close-range view/projection/frustum) instead of vp+0x29c / +0x15c / +0x3d0, and the
  near clip becomes 0.01 (PC w43 0x1001837e: `g_f1008b908 = 0.01f`). For GL: render
  with a projection whose near plane is about 0.01, and clear or partition depth so the
  weapon is not clipped by world geometry. *Inferred* from the role of the matrices.
- `model.m_bFovOffset` (FovXOffset/FovYOffset command): builds its own projection with
  `fovX = clamp(vp.fovX + model.fovXOffset + ExtraFOVXOffset·deg, 10°, 170°)` (same for y),
  `tan(fov/2)` scale (Mac DrawModel @1635e0..163928). Used for view-models that need
  their own FOV.

#### 5.2 SetupModelLight (Mac @1606c0; PC CD3DModelLight 0x10019d64)

```
nLights(m_2fc)=0; maxLights(m_300) = min(MaxModelLights, 16)
W2M = inverse(model.m_bNormalRef ? ObjXform*nodeXforms[normalRefNode]*model.m_NormalRefMat : ObjXform)
      (REALLYCLOSE: premultiplied by the camera matrix vp+0x9c)
colorScale(m_660) = inst.rgb (0..255);  lightAdd(m_654) = SceneDesc.m_ModelAdd (+0x1c)
CallModelHook()                       // 5.5 — may change colorScale, lightAdd, flags
colorScale /= 255; if (Saturate) colorScale *= ModelSaturation
if inst.flags & FLAG_NOLIGHT: dirAmt=0, ambient=(255,255,255) -> done
// sun / directional light (RenderStruct+0x130 dir, +0x13c colour — clientrender.cpp part 4)
if world && ModelApplySun && sunColor != 0:
    amt (m_684) = cached inst+0x244 unless invalid (<0) or (FLAG2_DYNAMICDIRLIGHT and moved > ModelSunVariance)
                  else GetDirLightAmount(): ray-casts from the model toward the sun through the
                  world (unnamed static @160140), 0..1; cached in inst+0x238..0x244
    dirLight(m_678) = -(W2M.rot * RenderStruct.m_GlobalLightDir)
else dirLight=0, amt=0
dirColor(m_66c) = RenderStruct.m_GlobalLightColor
ambient(m_688)  = world ? trilinear sample of the world's LightTable grid at the model position
                  (world+0xfc data, +0x104/+0x11c strides, +0x110..+0x118 dims, +0x12c.. 1/cellsize,
                   +0x138.. origin; RGB bytes)  : 0
// dynamic lights: the frame's visible OT_LIGHT list (data+0x29710 count / +0x29714 array, filled by r_ProcessLight)
for each light L while nLights < max:
    if |L.pos - modelPos| - model.m_VisRadius(+0xa8) >= L.radius(+0x1a8): skip
    rec = &m_lights[nLights++]   // 0x2c bytes at ModelDraw+0x3c
    rec.pos  (+0x00) = W2M * L.pos          // model space
    rec.dir  (+0x0c) = normalize(rec.pos)   // direction from model origin to light (inferred: 0xb9c50 = normalize)
    rec.r2   (+0x18) = radius²;  rec.invR2 (+0x1c) = 1/radius²
    rec.color(+0x20) = L.rgb (0..255, the light's object colour)
// static lights from the world tree (StaticLightCB @15fe70), same record format, box = VisRadius
```

`SceneDesc.m_ModelDirAdd`, `m_GlobalLightScale` and `m_GlobalLightAdd` were not seen being
read by the Mac model path (*not verified exhaustively*).

#### 5.3 Per-vertex colour (Mac SetupSkeletalVerts @187728..187b10)

```
N = v.m_Normal (model space);  P = v.m_Vec (model space)
d = dot(N, dirLight) * dirAmt
c = d > 0 ? ambient + d*(dirColor - ambient) : ambient
for each rec:
   ndl = dot(N, rec.dir);  if ndl <= 0 continue
   D = P - rec.pos; dist2 = dot(D,D); if dist2 >= rec.r2 continue
   c += rec.color * ndl * (1 - dist2*rec.invR2)
c = c*colorScale + lightAdd;  clamp each to 255
vertex RGBA = (c.r, c.g, c.b, m_6ac)          // TLVertex +0x0c: a, +0x0d r, +0x0e g, +0x0f b
```

For GL: compute this on the CPU per vertex (it is cheap and exact). An equivalent
shader needs the same model-space inputs.

#### 5.4 Environment map / chrome (DoModelDrawing @189490, TCalcEnvMap @182ce0)

- On when `EnvMapAll || m_458` and `RenderStruct.m_pEnvMap (+0x104)` is set. The
  engine sets that from "EnvironmentMap" (clientrender.cpp header: 0x0041e0fd).
- Env normal matrix `m_Mat3d0` = ViewParams rotation at vp+0xdc. If
  `model.m_bNormalRef`, it is `vp+0xdc * nodeXforms[normalRefNode] * model.m_NormalRefMat`
  (DrawModel @164168..1642f0; PC 0x10018895).
- Per vertex: `u = dot(row0(Mat3d0), N)*su + ou`, `v = dot(row1(Mat3d0), N)*sv + ov`, with
  `su = sv = 0.5/EnvScale`, `ou = camPos.x*EnvPanSpeed + 0.5`, `ov = camPos.z*EnvPanSpeed + 0.5`
  (r_RenderScene @1576a0..1576e4 sets ModelDraw+0x45c..0x468).
- Order: an **env pass first**, opaque, env texture only, uv = the env uv (AutoDrawTris
  with model textures off). Then `glEnable(GL_BLEND)` and the **normal skin pass on top**,
  so the skin's alpha decides how much chrome shows. When `EnvMapAll` is set, only the
  env pass is drawn.

#### 5.5 ModelHook (SceneDesc +0xe4 fn / +0xe8 user) — Mac `CallModelHook` @15fde0

Called once per model from SetupModelLight, before the colour normalisation:
```c
ModelHookData d;               // lives at ModelDraw+0x694
d.m_hObject     = inst;
d.m_Flags       = MHF_USETEXTURE (1);
d.m_ObjectFlags = inst->m_Flags;          // copy; the game may change it for this draw
d.m_LightAdd    = &lightAdd;              // ModelDraw+0x654, preset to SceneDesc.m_ModelAdd (0..255)
d.m_ObjectColor = &colorScale;            // ModelDraw+0x660, preset to inst rgb (0..255)
if (scene->m_ModelHookFn) scene->m_ModelHookFn(&d, scene->m_ModelHookUser);
```
Outputs used: `lightAdd` and `colorScale` (5.3). `d.m_Flags & MHF_USETEXTURE`: when it
is cleared the model draws untextured (InitTextureInfo @188b78, together with
TextureModels). `d.m_ObjectFlags` is what decides the shadow (FLAG_SHADOW) and the
`0x40` tests in DoModelDrawing (@189844, @189874).

#### 5.6 Specular (DoModelDrawing @189684, TCalcSpecular @182db0)

Used when `ModelSpecular && caps && model.m_bSpecularEnable && specularTexture`. It is a
2-texture pass: skin plus a generated specular ramp (`r_CreateSpecularTexture(power)`).
`uv1 = max(0, dot(N, dirLight)) * specScale` in both u and v.

---

### 6. Distance fade and LOD sprite (DrawModel @16303c..1631d4; DrawModelSprite @1619a0; PC billboard 0x10016324)

```
A=fadeA(+0xb0), A2=+0xb4, B=fadeB(+0xb8), B2=+0xbc; d2 = |cam - pos|²
if B2 >= A2:              // normal: fade OUT to the sprite with distance
   if d2 > B2:  DrawModelSprite(inst, 255); return
   if d2 > A2:  t=(sqrt(d2)-A)/(B-A); alpha=inst.a*(1-t); DrawModelSprite(inst, 255-alpha); draw model with alpha
   else alpha = inst.a
else:                     // reversed range: model fades IN with distance, invisible up close
   if d2 < B2: return
   if d2 < A2: alpha = inst.a * (sqrt(d2)-B)/(A-B)  else alpha = inst.a
```
The sprite is a quad of size `fadeSpriteW·scale.x × fadeSpriteH·scale.y`, centred on
the model and yawed to face the camera (no pitch). Its texture is `model+0xc0`, with uv
(u,v)(1-u,v)(1-u,1-v)(u,1-v), drawn as a triangle fan with the given alpha (PC
w55_10016324_os.cpp:30-38). With the default range of 20000 this never triggers.

---

### 7. Drawing (DoModelDrawing @189490, DrawTris @186840, SetModelTexture @188d50)

```
glPushAttrib(...)
if env: env pass (5.4); glEnable(GL_BLEND)
if m_6ac < 255: glEnable(GL_BLEND)         // blend func set by the caller (solid set: SRC_ALPHA/1-SRC_ALPHA;
                                           //  translucent: per FLAG2_ADDITIVE/MULTIPLY, depth writes off)
if !EnvMapAll:
   if specular: skin + specular 2-stage pass
   elif m_30 (detail): stage1 = skin.linked texture (SharedTexture+0x28) MODULATE, uv*DetailScale
   else: normal pass
glPopAttrib()
if (hook flags & FLAG_SHADOW) DrawModelShadows()
if ModelBoxes: DrawModelBox()               // debug

pass (DrawTris):
  for piece i (not hidden), L = GetLOD(piece, lod):
     bind inst.m_pSkins[piece.m_iTexture]   (index >= 2 or NULL => texturing off)
     for tri in L.m_Tris:
        v0,v1,v2 = out verts [outBase + tri.idx[k]]
        backface cull (screen-space cross product; sign flips if the view is mirrored, vp+0x4cc;
                        area <= ModelMinTri also culled)
        emit (pos, colour RGBA, uv = tri.uv[k] + halfTexel, uv1 = env/spec/detail)
  // the fog factor is per model (m_480/m_484 copied into every vertex), not per vertex
```

The model draw state is never reset per piece: the same blend and depth state applies
to every piece.

#### 7.1 Shadows (DrawModelShadows @18ad30, DrawShadowOnPoly @18a120, ModelShadowCB @18e500)

Only for models with FLAG_SHADOW (after the hook), up to MaxModelShadows per frame.
The model's command-string shadow parameters (ShadowLightDist, ShadowProjectLength,
ShadowSizeX/Y, ShadowCenterOffset) define a box from the model along the shadow
direction. World polys inside it are gathered through the world tree (`ModelShadowCB`;
it skips polys facing away, with a 0.7 cutoff, *inferred*). Each poly is re-drawn
blended with a shadow texture (data+0x2cba0), projected onto it, alpha
ModelShadowAlpha (230), depth offset ModelShadowOffset (0.1), through
`r_DrawGLPrimitive`. `DrawModelShadowsProj` (ModelShadowProj) is an empty stub on Mac.
**This is a projected blob/texture onto world polygons, not a stencil shadow.**
Details beyond this were not decoded.

---

### 8. Pseudocode: drawing one model in OpenGL

```c
void gl_DrawModel(ViewParams* vp, ModelInstance* inst, SceneDesc* scene)
{
    Model* m = *(Model**)((char*)inst + 0x1dc);
    if (!m || anim16(inst,0x1e0) >= m->nAnims || anim16(inst,0x1e8) >= m->nAnims) return;
    if (sphereOutsideFrustum(inst->m_Pos, m->m_Radius * maxc(inst->m_Scale))) return;

    float d = dist(vp->camPos, inst->m_Pos);
    uint8 alpha = fadeAlpha(m, d, inst->a /* may draw sprite and return */);
    int lod = selectLOD(m, d, &blendNext, &blendT);

    LTMatrix world = TRS(inst->m_Pos, sanitize(inst->m_Rotation), inst->m_Scale);

    // ---- node transforms (engine data, our TransformMaker copy) ----
    TransformMaker tm; tm_Init(&tm);                 // zero; weightset refs = INVALID
    VCALL_THIS(inst, 0x38, &tm);                     // engine: SetupTransformMaker (time refs + NodeControlFn)
    tm.m_hObject = inst; tm.m_pBaseTransform = &world; tm.m_pTransforms = NULL;   // -> m->nodeXforms
    if (!tm_SetupTransforms(&tm)) return;            // node[i] now = world * animated node i (world space)

    // ---- lighting setup (model space) ----
    LTMatrix w2m = inverse(m->m_bNormalRef ? world*node[m->normalRefNode]*m->m_NormalRefMat : world);
    vec3 scale = inst->rgb, add = scene->m_ModelAdd;   uint32 flags = inst->m_Flags;
    callModelHook(scene, inst, &flags, &add, &scale);   scale /= 255;
    gatherLights(...);                                 // 5.2

    // ---- skin + light every visible piece ----
    for (i = 0; i < m->nPieces; i++) {
        if (inst->m_HiddenPieces & (1u << i)) continue;
        PieceLOD* L = GetLOD(m->pieces[i], lod); if (!L) continue;
        PieceLOD* L2 = blendNext ? GetLOD(m->pieces[i], lod+1) : NULL;
        for (k = 0; k < L->m_nVerts; k++) {
            ModelVert* v = &L->m_Verts[k];
            vec4 P = skin(v, m->nodeXforms);           // Σ M_node * w.vec
            if (L2) P = lerp(P, skin(&L2->m_Verts[v->m_iReplacement], ...), blendT);
            pos[k]   = P.xyz / P.w;
            color[k] = rgba(lightVertex(v->m_Vec, v->m_Normal), alpha);   // 5.3
            if (envPass) envUV[k] = envMapUV(v->m_Normal);               // 5.4
        }
        SharedTexture* skin = m->pieces[i]->m_iTexture < 2 ? inst->m_pSkins[m->pieces[i]->m_iTexture] : NULL;
        // expand triangles: per-corner UVs live in ModelTri, so emit 3 unique verts per tri (or a de-dup table)
        build(L->m_Tris, L->m_nTris, pos, color, envUV) -> VBO;
        glBindTexture(skin); set blend for alpha / FLAG2_ADDITIVE / FLAG2_MULTIPLY; cull back faces (CW/CCW per LithTech)
        glDrawArrays(GL_TRIANGLES, 0, 3*L->m_nTris);   // (or glDrawElements with a de-duplicated index buffer)
    }
    if (flags & FLAG_SHADOW) drawProjectedShadow(...);   // optional, 7.1
}
```

The view and projection go in the GL matrices. Use a separate near/depth range
for FLAG_REALLYCLOSE (5.1). Models with alpha < 255 must be queued and drawn after
the solid world and objects, with depth writes off (r_ProcessModel /
r_DrawTransparentModel; the queue ordering is in the object-enumeration section).

---

### 9. PC d3d.ren cross-reference (for anyone reading the reconstruction)

| what | PC d3d.ren | Mac |
|---|---|---|
| per-model callback from the object walk | 0x10015c70 | r_DrawModel @15f040 |
| ModelDraw global object | 0x1008b090 | data+0x2a444 (`ModelDraw` instance) |
| DrawModel | 0x10017c14 (w43_10017c10_osoiqipni.cpp) | DrawModel__9ModelDraw @162e60 |
| SelectLOD | 0x10019644 | @1646c0 |
| CalcNumVerts (tris*3) | 0x1001bf34 | @162d40 |
| SetupModelLight | 0x10019d64 (big19d64 → w43) | @1606c0 |
| TransformMaker::SetupTransforms | 0x1007ca60 (d3dr4a.cpp:1001) | @07b800 |
| TM Prepare / blend / keyframe | 0x1007cab4, 0x1007d624, 0x1007da80, 0x1007d880 (w10_1007cab4_os.cpp) | SetupCall @07b9c0, BlendTransform @07c0e0 |
| TM Recurse (ProcessNode) | 0x1007cc90 (big7cc90.cpp) | Recurse @07c340 |
| LOD/fade billboard | 0x10016324 (w55_10016324_os.cpp) | DrawModelSprite @1619a0 |
| vertex setup / pieces | 0x10006840.. 0x10008a40 (w14_*) | SetupVerts @187bd0, SetupSkeletalVerts @186d80 |
| env-map uv | 0x10003b50 (w2_10003a10.cpp) | TCalcEnvMap @182ce0 |
| model cache | 0x10091d80 | VertexBufferCacheMgr (data+0x2cb28) |


# Part III — Non-model object types: sprites, particles, poly grids, line systems, canvases, lights, world models, cameras

Scope: everything except models (OT_MODEL). Object enumeration, the common LTObject
fields and the RenderStruct services are covered in other sections. All offsets are
byte offsets into the object, and they hold on both Mac and PC (same engine version).

Evidence conventions:
* `Mac fn+addr`: the Mac PEF `ext/rez/games/nolf-mac-usa/NOLF/LithTech`, read with
  `ext/tools/pef.py` (`dis - <name>`, or `dis - 0x<addr>` for an address).
* `PC 0x1xxxxxxx`: d3d.ren, reconstructed in `src/engine/d3d/`.
* **INFERRED** marks anything that was not read directly off code.

---------------------------------------------------------------------------

### 0. Shared renderer machinery these types use

#### 0.1 Per-type dispatch table (Mac data+0x2045c, 11 rows × 0x14 bytes, indexed by `m_ObjectType`)

`r_ReallyProcessObject` (Mac 0x16e410) does three things:
1. It loads `table[obj->m_ObjectType(+0xbc, signed char)].+0x10`.
2. It calls that entry as `Process(obj)`.
3. If `obj->m_pAttachments` (+0x98) is non-null, it calls `r_ProcessAttachments(obj, 0)`.

Each row is `{ Init, Term, flag(written to 1 by r_InitObjectModules), PreFrame, Process }`.
I dumped the rows from the data section:

| OT | Init / Term | PreFrame | Process |
|---|---|---|---|
| 0 NORMAL | – | – | – (nothing) |
| 1 MODEL | r_InitModelDraw / r_TermModelDraw | r_ModelPreFrame | r_ProcessModel |
| 2 WORLDMODEL | – | r_WorldModelPreFrame | r_ProcessWorldModel |
| 3 SPRITE | – | r_SpritePreFrame | r_ProcessSprite |
| 4 LIGHT | – | – | r_ProcessLight |
| 5 CAMERA | – | – | **null: cameras are never drawn** |
| 6 PARTICLESYSTEM | – | r_ParticlePreFrame | r_ProcessParticles |
| 7 POLYGRID | r_InitPolyGridDraw / r_TermPolyGridDraw | r_PolyGridPreFrame | r_ProcessPolyGrid |
| 8 LINESYSTEM | – | r_LineSystemPreFrame | r_ProcessLineSystem |
| 9 CONTAINER | – | – | **r_ProcessWorldModel** (a container is drawn as a world model) |
| 10 CANVAS | – | r_CanvasPreFrame | r_ProcessCanvas |

#### 0.2 VisibleSet: the per-frame buckets that "Process" fills

Each `ProcessX` only appends the object to a bucket of the frame's VisibleSet
(`r_GetVisibleSet()`, Mac 0x16fd20). If a bucket is full, it prints
`"Set '%s' overflowed"`. Drawing happens later, bucket by bucket.

The layout comes from PC `VisibleSet::Init` (PC 0x1007be90, `src/engine/d3d/w26_1007be90.cpp:50-100`).
The Mac reads the same offsets.

```c
struct ObjectSet {            // 0x20 bytes
    LTObject** m_pObjects;    // +0x00
    uint32     m_nObjects;    // +0x04
    uint8      pad[0x0c];
    uint32     m_nMax;        // +0x14
    const char*m_pName;       // +0x18
    uint32     pad2;
};
struct VisibleSet {
    /* +0x144 */ ObjectSet Models;                 // "VS_MODELS"            cap 0x80
    /* +0x164 */ ObjectSet ModelsTranslucent;      //                        0x80
    /* +0x184 */ ObjectSet ModelsChromakey;        //                        0x80
    /* +0x1a4 */ ObjectSet Sprites;                // "VS_SPRITES"           0x80
    /* +0x1c4 */ ObjectSet SpritesNoZ;             // "VS_SPRITES_NOZ"       0x80
    /* +0x1e4 */ ObjectSet WorldModels;            //                        0x40
    /* +0x204 */ ObjectSet WorldModelsTranslucent; //                        0x40
    /* +0x224 */ ObjectSet WorldModelsChromakey;   //                        0x40
    /* +0x244 */ ObjectSet Lights;                 //                        0x28 (=40)
    /* +0x264 */ ObjectSet PolyGrids;              //                        0x20
    /* +0x284 */ ObjectSet PolyGridsTranslucent;   //                        0x20
    /* +0x2a4 */ ObjectSet LineSystems;            //                        0x20
    /* +0x2c4 */ ObjectSet ParticleSystems;        //                        0x40
    /* +0x2e4 */ ObjectSet Canvases;               //                        0x20
    /* +0x304 */ ObjectSet CanvasesTranslucent;    //                        0x20
};
```

**Translucent objects** are not drawn straight from their bucket. Their
`r_DrawTranslucentX` walks the bucket with `BaseObjectSet::Draw(vp, r_QueueTranslucentX)`.
The queue function calls `ObjectDrawList::Add(obj, drawFn)` on the global list at
data+0x2ac14. That list is sorted (`ObjectDrawList::Sort`, console vars `DrawSorted`
and `SortProfile`) and drawn in `r_FlushObjectQueues`. So sprites, particle systems,
line systems, translucent poly grids, canvases and world models are all sorted
together, back to front. For example, `r_QueueSprite` (Mac 0x15e9b0) does
`Add(obj, r_DrawSprite)`.

**Portal and visibility filter**, used by every "solid" bucket loop
(Mac `r_DrawSolidPolyGrids` 0x191080, `r_DrawSolidCanvases` 0x1945a0,
`r_DrawTranslucentWorldModels` 0x172bf0):

```c
// vp->m_bInPortalView is ViewParams+0x4d4 (non-zero while rendering through a portal)
if (obj->m_Flags & FLAG_VISIBLE) {
    if (vp->m_bInPortalView && (obj->m_Flags2 & FLAG2_PORTALINVISIBLE)) skip;
} else {
    if (!(vp->m_bInPortalView && (obj->m_Flags & FLAG_PORTALVISIBLE /*0x400*/))) skip;
}
```

#### 0.3 ViewParams fields these draw functions read

The global ViewParams is at Mac data+0x2920c, filled by `r_InitFrame` and `r_InitFrustrum*`.

| off | meaning | evidence |
|---|---|---|
| +0x1c,+0x20,+0x24,+0x28 | viewport rect (int l,t,r,b) | particle size uses `0x24-0x1c` (r_DrawParticleBatch 0x17521c) |
| +0x2c,+0x30,+0x34,+0x38 | viewport rect as floats (screen clip of particle quads) | r_DrawParticleBatch 0x175530..0x1755e0 |
| +0x88 | near Z (temporarily set to 0.01 for FLAG_REALLYCLOSE) | r_DrawSprite 0x15db64, 0x15de30 |
| +0x8c | far Z | r_DrawParticleBatch 0x1753dc |
| +0x94 / +0x98 | `xScale = 1/tan(fovX/2)`, `yScale = 1/tan(fovY/2)` | r_InitFrustrum2 0x1503c4..0x150404 (`near/halfWindowX`, with fov recovered as `pi-2*atan`) |
| +0x15c | world→camera 3×4 matrix (rows at +0x15c, +0x16c, +0x17c). Camera space is **pre-scaled** so the frustum is `\|x\|<z, \|y\|<z` | r_DrawSprite 0x15d858, clip tests in r_DrawParticleBatch |
| +0x19c | 4×4 projection matrix (16 floats) | r_DrawSprite 0x15dbc0.. |
| +0x2dc | world→camera matrix used for FLAG_REALLYCLOSE objects | r_DrawSprite 0x15d7d8 |
| +0x31c..+0x330 | fast projection: `rhw=1/z; sx=(x*[31c]+z*[320])*rhw; sy=(y*[324]+z*[328])*rhw; sz=(z*[32c]+[330])*rhw` | r_ProjectPoint2 (Mac 0x15deb0) |
| +0x3d0 / +0x430 | 6 frustum planes {n.xyz,d} for normal / really-close | r_TestAndDrawPS 0x174d20 |
| +0x4b4 | camera world position | r_DrawSprite 0x15d8f8, r_DrawRotateableSprite |
| +0x4d4 | "in portal view" flag | see 0.2 |

**INFERRED (should be cross-checked).** The world→camera matrix at +0x15c includes the
`xScale`/`yScale` factors, because the clippers test `|x|<z`. That is why world-space
sizes are multiplied by +0x94/+0x98 before they are offset in camera space. In plain
OpenGL, use an ordinary view matrix and real world sizes. The size formulas below are
given both ways.

#### 0.4 TLVertex (the Mac GL renderer's vertex, 0x28 bytes)

```c
struct TLVertex {      // stride 0x28: sprite quad stores at +0x00/+0x28/...; particle batches 0x28/vertex
    float  x, y, z;    // +0x00 camera/screen space
    uint8  a, r, g, b; // +0x0c diffuse (byte order A,R,G,B; r_GetSpriteColor writes [0]=alpha [1..3]=rgb)
    uint32 specular;   // +0x10 (fog/specular; INFERRED)
    float  fog;        // +0x14 (from the fog function at data+0x2998c; INFERRED)
    float  u, v;       // +0x18, +0x1c
    float  pad;        // +0x20 (0)
    float  rhw;        // +0x24
};
```

#### 0.5 Blend and fog state shared by sprites, particle systems and poly grids

From Mac r_DrawSprite 0x15d61c..0x15d69c. r_TestAndDrawPS 0x174e88.. and r_DrawPolyGrid 0x18f208.. are identical:

```c
if (flags & FLAG_FOGDISABLE /*0x80*/) glDisable(GL_FOG);
if      (flags2 & FLAG2_ADDITIVE /*4*/) { glFogfv(GL_FOG_COLOR, black); glBlendFunc(GL_ONE, GL_ONE); }
else if (flags2 & FLAG2_MULTIPLY /*8*/) { glFogfv(GL_FOG_COLOR, black); glBlendFunc(GL_ZERO, GL_SRC_COLOR); }
else                                     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
// texture env: GL_COMBINE, RGB = modulate(texture, primary colour), alpha = modulate
```

Fog colour is set to black for additive and multiply blending (the 4 floats at data+0x1a388c).

---------------------------------------------------------------------------

### 1. Sprites (OT_SPRITE = 3)

#### 1.1 Data

```c
// SpriteInstance : LTObject      (Mac __ct__14SpriteInstanceFv 0xc9be0)
struct SpriteTracker {           // 0x14 bytes. spr_InitTracker 0x3ea50, spr_UpdateTracker 0x3eae0
    Sprite*          m_pSprite;    // +0x00
    SpriteAnim*      m_pCurAnim;   // +0x04 (Sprite+0x0c = first anim)
    SharedTexture**  m_pCurFrame;  // +0x08 -> element of anim->m_Frames; NULL if no frames
    uint32           m_MsCurTime;  // +0x0c ms into the anim
    uint32           m_Flags;      // +0x10 bit0 playing, bit1 looping (init = 3)
};
struct SpriteAnim {              // as read by spr_UpdateTracker
    /* +0x20 */ SharedTexture** m_Frames;
    /* +0x24 */ uint32  m_nFrames;
    /* +0x28 */ uint32  m_MsAnimLength;
    /* +0x2c */ uint32  m_MsFrameRate;   // frames per second: msPerFrame = 1000 / this
};
// SpriteInstance fields
/* +0x1a8 */ SpriteTracker m_SpriteTracker;  // ctor memsets 0x14 bytes; SpriteExtraInit 0x37410 calls spr_InitTracker(+0x1a8, sprite)
/* +0x1bc */ uint32        m_ClipperPoly;    // HPOLY, ctor sets -1 (INVALID_HPOLY); ci_ClipSprite 0xa4c0 stores here
/* +0x1c0 */ SpriteControlImpl (vtable) ; /* +0x1c4 */ back-pointer to the object (ILTSpriteControl)
```

PC side: the server's `SpriteInstance::m_nCurFrame` at +0x1bc (`src/engine/server/serverengine.h:235`)
is the same slot as the client's clipper poly. The client meaning comes from `ci_ClipSprite`.

**The ENGINE advances the frame, not the renderer.** `CClientMgr::UpdateObjects` (Mac 0x28420)
calls `cm_UpdateAnimations` (Mac 0x27f90). That function walks the client manager's sprite
list (+0x440) and calls `spr_UpdateTracker(&obj->m_SpriteTracker, frameTimeMs)`:

```c
if (!anim) return;
if (!anim->m_MsAnimLength) { t->m_pCurFrame = NULL; return; }
if (t->m_Flags & 1) t->m_MsCurTime += dt;
if (t->m_MsCurTime >= len && !(t->m_Flags & 2)) t->m_MsCurTime = len - 1;
else t->m_MsCurTime %= len;
t->m_pCurFrame = &anim->m_Frames[(t->m_MsCurTime / (1000 / anim->m_MsFrameRate)) % anim->m_nFrames];
```

The renderer only reads `*obj->m_SpriteTracker.m_pCurFrame`: a `SharedTexture*` that it binds
like any other texture. r_DrawSprite 0x15d588..0x15d5c4 checks +0x1ac (cur anim), +0x1b0
(cur frame pointer) and `*(+0x1b0)`, and draws nothing if any of them is null.

#### 1.2 Bucketing (r_ProcessSprite Mac 0x15e8f0)

* If `flags & FLAG_SPRITE_NOZ` (0x10), the sprite goes to `SpritesNoZ` (+0x1c4).
* Otherwise it goes to `Sprites` (+0x1a4).
* `r_DrawTranslucentSprites` (0x15ea20) queues every normal sprite into the sorted
  translucent list with `r_QueueSprite`. **Sprites are always treated as translucent.**
* `r_DrawNoZSprites` (0x15eaa0) runs after that. It does `glPushAttrib(GL_DEPTH_BUFFER_BIT)`
  and `glDisable(GL_DEPTH_TEST)`, then calls r_DrawSprite on each NoZ sprite in bucket order.

#### 1.3 r_DrawSprite (Mac 0x15d570, 2308 bytes)

Pseudocode, one numbered step per stage of the function. `vp` is the ViewParams.

**1. Check for a frame.** Read `tex = *obj->m_SpriteTracker.m_pCurFrame`. Return if there is none.

**2. Set state.** Apply the blend and fog state from 0.5.

**3. Rotatable sprites branch off.**
`if (flags & FLAG_ROTATEABLESPRITE /*8*/)`: call
`r_DrawRotateableSprite(vp, obj, &obj->m_Pos)` (Mac 0x15b530, see 1.4) and return.

**4. Move the centre to camera space.**
```c
M = (flags & FLAG_REALLYCLOSE /*0x40*/) ? vp->m_ReallyCloseMat /*+0x2dc*/ : vp->m_ViewMat /*+0x15c*/;
c = M * obj->m_Pos;                                     // camera space
minZ = (flags & FLAG_REALLYCLOSE) ? 0.01f : 7.0f;
if (c.z <= minZ) return;                                // 0x15d8f0
```

**5. Bind the texture.**
`if (!r_SetTexture(tex)) return;` Read `W = texWidth`, `H = texHeight`
(RTexture vtable +0x14 and +0x18), and `tu, tv` = texel size of the bound texture
(data+0x2d7a4; also what CanvasDrawMgr::GetTexelSize returns).

**6. Compute UVs.** The UVs are inset by 2 texels on every edge:
```c
u0 = 2*tu;  u1 = (W-2)*tu;
v0 = 2*tv;  v1 = (H-2)*tv;
```

**7. Compute the half extents in pre-scaled camera space.**
```c
hw = vp->xScale(+0x94) * (W * obj->m_Scale.x);
hh = vp->yScale(+0x98) * (H * obj->m_Scale.y);
```
→ In world units the half extent is `W*scale.x` by `H*scale.y`. **INFERRED** from 0.3.
It agrees with the rotatable path, which uses local corners `(±W, ±H)` times the object scale.

**8. Glow sprites shrink as they get near.**
```c
if (flags & FLAG_GLOWSPRITE /*0x20*/) {                 // 0x15da08
    k = clamp((c.z - 10.0f) / 490.0f, 0, 1);
    s = 0.1f + 1.9f * k;
    hw *= s; hh *= s;
}
```

**9. Compute the colour.** `col = r_GetSpriteColor(obj, &c)` (Mac 0x15df70, see 1.5).

**10. Build the quad**, as a camera-space triangle fan:
```c
v[0] = {c.x-hw, c.y+hh, c.z, col, u0, v0};
v[1] = {c.x+hw, c.y+hh, c.z, col, u1, v0};
v[2] = {c.x+hw, c.y-hh, c.z, col, u1, v1};
v[3] = {c.x-hw, c.y-hh, c.z, col, u0, v1};
```

**11. Clip.**
```c
if (flags & FLAG_REALLYCLOSE) vp->nearZ = 0.01f;        // restored at the end
clip polygon against 6 planes (r_ClipPoly 0x15cde0); if nothing is left, return;
```

**12. Project, applying the sprite bias.** The bias moves only the depth value; x and y
are projected with the true z.
```c
if (flags & FLAG_SPRITEBIAS /*4*/) {                    // 0x15db84
    bias = -20.0f;
    if (c.z - 20 < 7) bias = 7 - c.z;                   // never nearer than z=7
    // per vertex: x,y projected with the true z; the stored depth and rhw use (z + bias)
} else if (flags & FLAG_REALLYCLOSE) {
    // same projection with +7.0 added to z for depth only (0x15dcc8.. uses f27=7.0)
} else {
    r_ProjectPoint2(v, vp);                             // 0x15deb0
}
```

**13. Draw.** `r_DrawGLPrimitive(GL_TRIANGLE_FAN, v, n)` (Mac 0x1917e0).

OpenGL translation:
* Billboard axes are the camera right and up vectors.
* Half size is `(W*scale.x, H*scale.y)` world units, times `s` for glow sprites.
* For SPRITEBIAS, a polygon offset or `glDepthRange` trick that moves depth 20 units
  toward the eye is equivalent.

#### 1.4 Rotatable sprites (FLAG_ROTATEABLESPRITE, `r_DrawRotateableSprite` Mac 0x15b530..0x15c6c0)

* **Transform.** `R = quat_ConvertToMatrix(obj->m_Rotation)` (the quaternion's 4 components
  are first clamped to [-100000, 100000], with w=1 if out of range). Column 0 is multiplied
  by scale.x, column 1 by scale.y, column 2 by scale.z. Translation is `m_Pos`.
* **Corners** in object space: `(-W,+H,0) (+W,+H,0) (+W,-H,0) (-W,-H,0)`, with UVs as in 1.3.
  W and H are the texture's texel dimensions, read via the same RTexture vtable +0x14 call
  (0x15b738 and 0x15b76c).
* **Two-sided.**
  `dot(-forward, camPos(vp+0x4b4) - m_Pos) < 0` picks one of two vertex-order tables
  (data+0x200f4 / data+0x200e4). The sprite is visible from both sides.
* **Colour.** Not FLAG_NOLIGHT and a light table present: `clamp(objColour + lightTableSample + r_CalcLightAdd)`.
  Otherwise the object colour. Alpha is `m_ColorA`. Same as 1.5.
* **ClipSprite.** If `m_ClipperPoly` (+0x1bc) is a valid HPOLY:
  * Look up the world poly (`MainWorld::GetWorldDataFromHPoly`, then the bsp vtable +0x14).
  * Return (draw nothing) if the camera is not more than 0.01 in front of the poly's plane.
  * Clip the sprite quad against each edge plane of the poly. Each edge plane is
    `normalize(cross(edge, polyNormal))` (0x15be78..).
  * **Only rotatable sprites honour ClipSprite.** r_DrawSprite never reads +0x1bc.
* **Projection.** Then clip, project and apply sprite bias exactly as in 1.3 (0x15c54c..,
  the same -20 and 7.0 constants), and draw a fan.

#### 1.5 Sprite colour, `r_GetSpriteColor` (Mac 0x15df70)

```c
out.a = obj->m_ColorA;
if ((obj->m_Flags & FLAG_NOLIGHT /*0x200*/) || !g_pLightTable /*world light grid, data+0x29838*/) {
    out.rgb = obj->m_ColorRGB;
    return;
}
L = trilinear sample of the world LightTable at obj->m_Pos   // light grid: origin +0x138, 1/cellsize +0x12c,
                                                             // dims +0x110.., data +0xfc, 3 bytes rgb per cell
D = r_CalcLightAdd(obj, pos)                                 // dynamic lights, §6.3
out.rgb[i] = clamp(obj->m_Color[i] + L[i] + D[i], 0, 255)    // ADDITIVE, not modulated (0x15e480..)
```

Sprites are therefore lit by default (the world light grid plus dynamic lights are
*added* to the object colour). `FLAG_NOLIGHT` turns that off.

---------------------------------------------------------------------------

### 2. Particle systems (OT_PARTICLESYSTEM = 6)

#### 2.1 Data

The layout comes from `__ct__14ParticleSystemFv` (Mac 0xc9f70), `ci_*` accessors,
`ps_UpdateParticles` (Mac 0xf30) and `cm_UpdateParticleSystems` (Mac 0x27e10).

```c
struct PSParticle {              // 0x3c bytes; the first 0x24 bytes are the SDK LTParticle (iltclient.h:23)
    LTVector m_Velocity;         // +0x00
    LTVector m_Color;            // +0x0c  0..255 each
    float    m_Alpha;            // +0x18  0..1
    float    m_Size;             // +0x1c
    PSParticle* m_pNext;         // +0x20
    LTVector m_Pos;              // +0x24  LOCAL to the system (object pos/rot/scale applied at draw)
    float    m_Lifetime;         // +0x30  remaining   (ci_GetParticleLifetime)
    float    m_TotalLifetime;    // +0x34              (ci_GetParticleTotalLifetime)
    PSParticle* m_pPrev;         // +0x38
};
// ParticleSystem : LTObject
/* +0x1a8 */ PSParticle m_ParticleHead;   // sentinel; first = +0x1c8 (head.m_pNext), loop ends at &head
/* +0x1e4 */ void*  m_pParticleBank;
/* +0x1e8 */ SharedTexture* m_pCurTexture; // the texture to draw (set from the sprite tracker each frame)
/* +0x1ec */ uint8  m_SoftwareR, G, B;      // ci_SetSoftwarePSColor (unused by the GL path)
/* +0x1f0 */ Sprite* m_pSprite;             // non-null if the texture is a .spr
/* +0x1f4 */ SpriteTracker m_SpriteTracker; // 0x14
/* +0x208 */ LTVector m_SystemCenter; float m_SystemRadius /*+0x214*/;  // bounding sphere (local, see 2.3)
/* +0x218 */ previous centre/radius (ps_StartUpdatingPositions)
/* +0x228 */ int32  m_nParticles;
/* +0x22c */ uint32 m_bChanged;
/* +0x230 */ LTVector m_MinPos, m_MaxPos /*+0x23c*/;
/* +0x248 */ float  m_GravityAccel;   // default -500
/* +0x24c */ float  m_ParticleRadius; // default 300
/* +0x250, +0x254 */ -1, -1
/* +0x258 */ uint32 m_psFlags;        // PS_BOUNCE 1, PS_NEVERDIE 4, PS_DUMB 8 (iltclient.h:17)
```

**The engine moves the particles** (`cm_UpdateParticleSystems`). The renderer only reads.
* Integration: `pos += vel*dt` and `vel.y += gravity*dt`.
* Lifetime is decremented and dead particles are unlinked, unless PS_NEVERDIE.
* PS_DUMB means the engine skips the update entirely.
* The engine updates the system's sprite tracker and copies `*m_pCurFrame` into +0x1e8
  (0x27e98..0x27ed0). The renderer just binds +0x1e8.

#### 2.2 Bucketing and drawing

* **Bucketing.** `r_ProcessParticles` (Mac 0x174c20) puts every visible system into
  ParticleSystems (+0x2c4). `r_DrawTranslucentParticles` (0x1750b0) queues them all into
  the sorted translucent list with `r_TestAndDrawPS`. Particles are always translucent.
* **`r_TestAndDrawPS` (Mac 0x174cd0):**
  1. Sphere test: radius `m_SystemRadius * max(scale.x, scale.y, scale.z)`, centre +0x208,
     against the 6 planes at vp+0x3d0 (vp+0x430 if FLAG_REALLYCLOSE).
  2. If visible, set `FLAG_INTERNAL1` (0x40000000, "the renderer saw it", 0x174e38).
  3. Apply the state from 0.5 (additive, multiply, fog).
  4. Call `r_DrawParticleSystem`.
* **`r_DrawParticleSystem` (Mac 0x1758d0):**
  1. Bind `m_pCurTexture` (+0x1e8). With no texture, texturing is disabled.
     UV rectangle = `[2*tu, (W-2)*tu] × [2*tv, (H-2)*tv]`, as for sprites
     (globals data+0x2b628..0x2b634).
  2. Build `Obj = R(quat m_Rotation) * diag(m_Scale)` with translation `m_Pos`.
     Multiply it by the camera matrix (vp+0x15c, or vp+0x2dc if REALLYCLOSE).
  3. Set the colour scale to (1/255, 1/255, 1/255).
  4. Call `r_DrawParticles(ps, &M, 1, 0.0f)`. That calls `r_DrawParticleBatch` on batches
     of 128 particles (0x175810: `n>>7` full batches plus the remainder).
* **`r_DrawParticleBatch` (Mac 0x1751c0)**, per particle:
```c
p  = M * part->m_Pos;                       // camera space (pre-scaled, see 0.3)
if (p.z <= vp->nearZ || p.z >= vp->farZ || |p.x| > p.z || |p.y| > p.z) continue;
project p (proj matrix vp+0x19c) -> sx, sy, sz, rhw
halfPix = part->m_Size * vp->xScale(+0x94) * (vp->rect.right - vp->rect.left) * rhw;
                                            // (if the 'size override' argument is non-zero it replaces m_Size; the PS path passes 0)
quad = [sx-halfPix, sx+halfPix] × [sy-halfPix, sy+halfPix], clipped to the viewport rect (floats vp+0x2c..0x38)
       with the UVs adjusted proportionally
colour: r = part->m_Color.x * objR/255, g = .y * objG/255, b = .z * objB/255  (0..255)
        a = part->m_Alpha * objA                                              (0..255)
4 screen-space TLVertex, then all quads -> r_DrawIndexedGLPrimitive(GL_TRIANGLES, verts, indices, 6/quad)
```
  The index buffer (data+0x2b020) is built once in `r_ParticlePreFrame`.

OpenGL translation: each particle is a screen-aligned quad. Its centre is
`ObjectMatrix * m_Pos`. The particles are relative to the system's position, rotation
and scale (the SDK says so in iltclient.h, and 0x175c30..0x175cc4 confirms it).

**Particle quad size (INFERRED).** The half size in pixels is
`m_Size * (1/tan(fovX/2)) * viewportWidth / z`. The true projection of a world half
extent h is `h * (1/tan(fovX/2)) * (viewportWidth/2) / z`. So the quad is equivalent to
a world-space half extent of `2*m_Size`, i.e. a billboard of side `4*m_Size`.
Verify this against the PC d3d.ren particle code before relying on the factor 2.

---------------------------------------------------------------------------

### 3. Poly grids (OT_POLYGRID = 7)

#### 3.1 Data

The layout comes from `__ct__8PolyGridFv` (Mac 0xca270), `pg_Init` (Mac 0x620),
`ci_*PolyGrid*` and `GetRadius__8PolyGrid` (Mac 0xcb200).

```c
/* +0x1a8 */ int8*   m_Data;        // width*height bytes (dalloc_z), one height value per vertex
/* +0x1ac */ uint16* m_Indices;     // triangle list, built once by pg_Init
/* +0x1b0 */ Sprite* m_pSprite;     // the texture MUST be a .spr (SetPolyGridTexture)
/* +0x1b4 */ SpriteTracker m_SpriteTracker;   // engine-advanced (cm_UpdatePolyGrids 0x28160)
/* +0x1c8 */ SharedTexture* m_pEnvMap;        // SetPolyGridEnvMap (a .dtx), NULL = none
/* +0x1cc */ float m_xPan, m_yPan /*+0x1d0*/, m_xScale /*+0x1d4, def 1*/, m_yScale /*+0x1d8, def 1*/;
/* +0x1dc */ uint32 m_nTris;   /* +0x1e0 */ uint32 m_nIndices;
/* +0x1e4 */ LTLink m_LeafLinks;    // records in world leaves (ltobject_internal.h PolyGridObject)
/* +0x1f0 */ uint32 m_Width;  /* +0x1f4 */ uint32 m_Height;   // >= 2, w*h <= 65000
/* +0x1f8 */ PGColor m_ColorTable[256];   // {float r,g,b,a} 0..255 (ltbasetypes.h:77); GetPolyGridInfo returns +0x1f8
```

**Index layout** (pg_Init). For each cell, with `i = row*w + col`:
* Full mode: two triangles `(i+w+1, i+1, i)` and `(i+w, i+w+1, i)`.
* `bHalfTriangles`: only the first of the two.

#### 3.2 Vertex generation (r_DrawPolyGrid Mac 0x18f130, 7724 bytes)

```c
// local grid frame (0x18fa34..0x18faa0):
//   x_local = col - (w-1)/2,  z_local = row - (h-1)/2,  y_local = value
//   value   = FLAG_UNSIGNED(flags&2) ? (uint8)data - 128 : (int8)data
// world = R(quat m_Rotation) * diag(m_Scale) * local + m_Pos      (so the grid spans w*scale.x by h*scale.z)
// colour index = FLAG_UNSIGNED ? (uint8)data : (int8)data + 128
//   rgb   = m_ColorTable[idx].rgb  (through an identity 0..255 clamp table)
//   alpha = m_ColorTable[idx].a * (objA / 255)
// texture coords (0x18f754..0x18f840; tu,tv = texel scale of the bound sprite frame):
//   uScale = (1/m_xScale) * tu * scale.x * w/(w-1);   uPan = m_xPan * tu
//   vScale = (1/m_yScale) * tv * scale.z * h/(h-1);   vPan = m_yPan * tv
//   u = (col + uPan) * uScale;   v = (row + vPan) * vScale
```

**Drawing:**
* Indexed triangles: `r_DrawIndexedGLPrimitive(GL_TRIANGLES, verts, m_Indices, m_nIndices)`
  (0x190d1c and 0x190f00).
* Env map: if `m_pEnvMap` is set, the grid is drawn again with `r_SetEnvMapTextureStates`.
  Sphere-map style coordinates come from the camera-space normal, built by the 0x18eb80 helper.
* `FLAG_ENVIRONMENTMAPONLY` (0x20) draws only the env-map pass. **INFERRED** from the
  flag's SDK comment and the two-pass shape; not traced instruction by instruction.
* The radius for culling is `|(w/2*scale.x, 128*scale.y, h/2*scale.z)| + 1` (GetRadius).

**Solid or translucent** (`r_DrawSolidPolyGrids` Mac 0x191080):
* `objA == 255`: drawn immediately (solid pass).
* Otherwise: moved to PolyGridsTranslucent (+0x284), then sorted with the other
  translucents (`r_QueueTranslucentPolygrid`).

Same blend and fog state as 0.5.

---------------------------------------------------------------------------

### 4. Line systems (OT_LINESYSTEM = 8)

#### 4.1 Data

The layout comes from `__ct__10LineSystemFv` (Mac 0xca3e0), `linesystem_AddLine` (Mac 0x346a0),
`linesystem_GetNextLine` (Mac 0x34190), and PC `big40e10.cpp:949-1000`.

```c
struct LSLine {                // the renderer-side line record
    LTLine   m_Line;           // +0x00: 2 × {LTVector m_Pos; float r,g,b,a;} = 0x38 (iltclient.h:34)
    LTObject* m_pOwner;        // +0x38
    LSLine*  m_pPrev;          // +0x3c
    LSLine*  m_pNext;          // +0x40
};
/* +0x1a8 */ void*  m_pLineBank;
/* +0x1ac */ uint32 m_bChanged;
/* +0x1b0 */ LSLine m_LineHead;     // sentinel; first line = +0x1f0 (head.m_pNext), last = +0x1ec
/* +0x1f4 */ LTVector m_MinPos, m_MaxPos /*+0x200*/;   // local bounding box, grown by AddLine
/* +0x20c */ LTVector m_SystemCenter; /* +0x218 */ float m_SystemRadius;
```

#### 4.2 Draw

`r_DrawLineSystems` (Mac 0x16e170) queues every line system as translucent.
`r_DrawLineSystem` (Mac 0x16c6d0, PC 0x10040e10 `big40e10.cpp`) does the following.

**Transform.** `M = R(quat) * diag(m_Scale)` with translation `m_Pos` (0x16c830..0x16c8dc).
The points are **relative to the object's position, rotation and scale**.

**Per line:**
1. Transform both points by M, then by the camera matrix.
2. Clip the segment against the 6 frustum planes (Cohen-Sutherland, endpoints only),
   interpolating colour.
3. Draw it as one line: `GL_LINES`, 2 vertices (Mac `glBegin(GL_LINES)`; PC
   `DrawPrimitive(D3DPT_LINELIST, 2)` at 0x10041f13).

**Colour.** Use the PC reading (`big40e10.cpp:1155-1186`):
```c
rgb_byte = point.rgb * 255;      // point.r,g,b are 0..1 (SDK)
a_byte   = point.a * objA;       // objA 0..255
```
The Mac 0x16ded8.. converts `point.r` with no ×255 and multiplies alpha by `objA/255`.
That does not match the SDK's 0..1 contract, so it is probably a Mac quirk.

Line systems are always drawn in the translucent list. The `DrawLineSystems` console
variable gates the whole pass. Fog is disabled during the pass if data+0x1f974 is set.

---------------------------------------------------------------------------

### 5. Canvases (OT_CANVAS = 10)

#### 5.1 Data

The layout comes from `__ct__6Canvas` (Mac 0xca870), CLTClient Get/SetCanvasFn/Radius,
and `objtypes.h` CObjCanvas.

```c
/* +0x1a8 */ CanvasDrawFn m_Fn;        // void (*)(ILTCustomDraw* pDraw, HLOCALOBJ hObj, void* pUser)  (ltbasedefs.h:328)
/* +0x1ac */ void*        m_pUser;
/* +0x1b0 */ float        m_Radius;    // visibility radius, default 1 (GetRadius/GetVisRadius return it)
```

#### 5.2 Bucketing (`r_DrawSolidCanvases` Mac 0x1945a0)

* If the object's **client flags** (LTObject +0x180, uint16, `ci_Get/SetObjectClientFlags`)
  have `CF_SOLIDCANVAS` (0x10, ltcodes.h:41), the canvas is drawn immediately in the solid pass.
* Otherwise it goes to CanvasesTranslucent (+0x304) and is sorted with the translucents
  (`r_QueueCanvasCB`).

#### 5.3 What the game callback gets: the CanvasDrawMgr implementation of ILTCustomDraw

`ILTCustomDraw` is defined in `src/LT2/sdk/inc/iltcustomdraw.h`:
`DrawPrimitive`, `SetState`, `GetState`, `SetTexture`, `GetTexelSize`.

`CanvasDrawMgr::DrawCanvas(canvas)` (Mac 0x1927f0):
```c
if (!canvas->m_Fn) return;
InitStates(canvas);
canvas->m_Fn(this /*ILTCustomDraw*/, canvas, canvas->m_pUser);
RestoreStates();   // glPopAttrib, re-enable fog if it was on
```

`InitStates` (Mac 0x1928a0) is the default state block. It is stored in `mgr+0x2c..+0x48`
as `uint32 state[LTRSTATE_*]`:

| state | default | GL |
|---|---|---|
| ALPHABLENDENABLE | 0 | `glDisable(GL_BLEND)` |
| ZREADENABLE | 1 | `glEnable(GL_DEPTH_TEST)` |
| ZWRITEENABLE | 1 | `glDepthMask(1)` |
| SRCBLEND / DESTBLEND | 5 / 6 (SRCALPHA / INVSRCALPHA) | `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` |
| TEXADDR | 1 (WRAP) | `GL_TEXTURE_WRAP_S/T = GL_REPEAT` |
| COLOROP | 3 (MODULATE) | texenv combine modulate |
| ALPHAOP | **2 (SELECTDIFFUSE)** in the stored state (the SDK comment says SELECTTEXTURE) | combine alpha = replace(primary) |

`InitStates` also:
* calls `glPushAttrib`;
* disables fog if `FLAG_FOGDISABLE`;
* unbinds the texture;
* records `mgr+0x28 = (flags & FLAG_REALLYCLOSE) != 0`.

**`SetState`** (Mac 0x193830) maps each state onto GL:
* ALPHABLENDENABLE → `glEnable` / `glDisable(GL_BLEND)`.
* ZREADENABLE → `GL_DEPTH_TEST`.
* ZWRITEENABLE → `glDepthMask`.
* SRCBLEND / DESTBLEND → `glBlendFunc`, using the table ZERO, ONE, SRC_COLOR, ONE_MINUS_SRC_COLOR, SRC_ALPHA, ONE_MINUS_SRC_ALPHA.
* TEXADDR → `GL_REPEAT` / `GL_CLAMP` on S and T.
* COLOROP / ALPHAOP → `GL_COMBINE` with REPLACE (texture or primary), MODULATE, ADD or ADD_SIGNED.

**`SetTexture(const char* name)`** (Mac 0x194070): resolves the name to a SharedTexture
through the engine's texture loader, then binds it. `NULL` disables texturing.

**`GetTexelSize`** (Mac 0x194370): returns the bound texture's texel size (data+0x2d7a4).

**`DrawPrimitive(LTVertex* v, n, flags)`** (Mac 0x192ba0):
* `n` must be at least 3 and at most 100 (0x64).
* `LTVertex = {LTVector m_Vec; float m_RHW; LTVertexColor m_Color(b,g,r,a); LTVertexColor m_Specular; float m_TU, m_TV}`.
* `m_Vec` is in **world space**. It is transformed by the camera matrix (the really-close
  matrix, and near = 0.01, if mgr+0x28 is set).
* The polygon is clipped against 6 planes (the six DoClip helpers at 0x157e20..0x159330),
  projected, and drawn as a `GL_TRIANGLE_FAN` (`r_DrawGLPrimitive(6, ...)`).

So for OpenGL, the vertices are a world-space convex polygon, drawn as a fan.

---------------------------------------------------------------------------

### 6. Dynamic lights (OT_LIGHT = 4)

#### 6.1 Data

* Light colour is the **object colour** bytes +0x94..+0x96. `ci_SetLightColor` (Mac 0xa270)
  calls `ci_SetObjectColor(r,g,b, a/255)`, which stores `byte = 255*value`.
* `m_LightRadius` is the float at **+0x1a8**, default 100 (`__ct__12DynamicLightFv` Mac 0xc9d40;
  `objtypes.h` CObjLight).
* Flags:
  * `FLAG_ONLYLIGHTWORLD` 0x20: the light does not light objects.
  * `FLAG_ONLYLIGHTOBJECTS` 0x80: the light does not light the world.
  * `FLAG_DONTLIGHTBACKFACING` 0x40.

#### 6.2 Per frame

`r_ProcessLight` (Mac 0x1765c0) puts lights into Lights (+0x244, cap 40), gated by a
console variable at data+0x1f998. `r_DrawLights` (0x176c30) calls `r_DrawLight` on each one.
This happens **before** objects are lit; the order is set by r_FullDrawScene, which the
parent section covers. `r_DrawLight` (Mac 0x176a70):

```c
if (!(flags & FLAG_ONLYLIGHTWORLD) && g_nObjectLights < 40)          // 0x176ae0
    g_ObjectLights[g_nObjectLights++] = light;   // data+0x29714 / count data+0x29710, used by sprites & models
if (!(flags & FLAG_ONLYLIGHTOBJECTS) && g_pWorld) {                  // 0x176b0c
    box = light->m_Pos ± radius;
    world->FindObjectsInBox(box, r_DrawLightCB)  // worldtree vtbl +0x14
        // r_DrawLightCB: for OT_WORLDMODEL objects -> r_ProcessBspLight(light, wm)
}
```

`r_ProcessBspLight` (Mac 0x176680) works like this:
1. Transform the light position into the world model's local space using wm+0x230
   (the inverse transform).
2. Walk that BSP (node: +0 poly, +4 front child, +8 back child), comparing
   `plane·pos - d` with the radius.
3. For every poly that `r_DoesLightTouchPoly` (0x176480) accepts, link the light into
   that poly's per-frame light list.

World polys that carry lights are then lit during world drawing: per vertex for
gouraud polys (`r_DrawGouraudDynamicLight` Mac 0x159af0), or as an extra additive
pass for lightmapped ones (`r_DrawLightAddPoly`). The world-drawing section covers
the per-poly maths.

#### 6.3 How lights affect sprites and models: `r_CalcLightAdd`

The same routine appears on both platforms: Mac 0x1762a0 and PC 0x1001e210
(`src/engine/d3d/w32_1001e210.cpp:20-120`). They agree exactly.

```c
out = 0;
if (!g_CV_DynamicLight) return;
for each light L in g_ObjectLights:
    d2 = |L->m_Pos - obj->m_Pos|^2;
    if (L->radius^2 > d2) {
        s = (1 - sqrt(d2)/L->radius) * 0.7f;
        for c in r,g,b: out.c += (L->col.c - (255 - L->col.c)) * s;   // i.e. (2*col - 255)*s — a colour below 128 SUBTRACTS light
    }
```

---------------------------------------------------------------------------

### 7. World models and containers (OT_WORLDMODEL = 2, OT_CONTAINER = 9) — brief

#### 7.1 Data

`WorldModelInstance` (`ltobject_internal.h`, `objtypes.h`):
* `+0x1a8` IWorldBsp* `m_pWorldBsp`, the BSP to draw.
* `+0x1ac` original BSP.
* `+0x1f0` LTMatrix: the object transform (local→world).
* `+0x230` LTMatrix: its inverse (world→local). r_ProcessBspLight reads +0x230..+0x26c to
  bring a world position into model space.

#### 7.2 Bucketing (`r_ProcessWorldModel` Mac 0x16ff60)

Let `firstSurfaceFlags = bsp->m_Polies[0]->m_pSurface(+0x28)->m_Flags(+0x38)`, read via
bsp+0xa0/+0xa4. This is `r_GetFirstSurfaceFlags` (0x172db0). The BSP must also exist and
its vtable +0x40 must answer true.

```c
if (firstSurfaceFlags & 0x8)               -> WorldModelsTranslucent (+0x204)
else if (flags2 & FLAG2_CHROMAKEY /*2*/)    -> WorldModelsChromakey   (+0x224)
else                                        -> WorldModels            (+0x1e4)
```

The surface-flag bit names (0x8 translucent, 0x80000 additive) are **INFERRED**.

#### 7.3 Drawing

* `r_DrawSolidWorldModels` and `r_DrawChromakeyWorldModels` call `r_DrawSolidWorldModel`
  (Mac 0x171d30). It:
  * draws the BSP polys with the wm+0x1f0 transform applied;
  * scales colour by `objRGB/255`, with alpha = `objA` (`r_SetupWMColorScale` 0x171320);
  * disables fog for `FLAG_FOGDISABLE`.
* `r_DrawTranslucentWorldModels` (0x172bf0) applies the portal filter (0.2), then:
  * if the first-surface flags have 0x80000, the model is added to the sorted list with
    `r_DrawAdditiveTranslucentWorldModel`, which uses `glBlendFunc(GL_ONE, GL_ONE)` with
    fog disabled (0x172af0);
  * otherwise it is queued for `r_DrawTranslucentWorldModel` (0x1713e0), which uses
    normal alpha blending.

#### 7.4 Sky objects (`FLAG2_SKYOBJECT`, `r_DrawSkyObjects` Mac 0x1797e0)

The SceneDesc's sky list (SceneDesc +0x9c pointer, +0xa0 count) is drawn first. Setup:
* depth test off and depth writes off;
* fog off;
* `glPushAttrib`.

Each object is drawn in list order by type:
* OT_WORLDMODEL → `r_DrawSkyWorldModel`;
* OT_POLYGRID → `r_DrawPolyGrid`;
* OT_SPRITE → `r_DrawSprite`.

If `objA < 255`, the object is wrapped in `r_SetTranslucentObjectStates` /
`r_UnsetTranslucentObjectStates`.

---------------------------------------------------------------------------

### 8. Cameras (OT_CAMERA = 5)

Cameras are never drawn. Their dispatch-table row is all null (§0.1). A camera only
supplies the SceneDesc's position, rotation, fov, rect and light-add
(`clientmgr2.cpp` `cm_RenderCamera`, CCameraQ5).

---------------------------------------------------------------------------

### 9. Quick implementation checklist for OpenGL

1. **Sprite.**
   * Texture: `*(SharedTexture**)(*(void**)(obj+0x1b0))`.
   * Billboard of half size `(W*sx, H*sy)` at m_Pos. Use the object rotation instead if
     flag 0x08 is set; that case is two-sided and honours ClipSprite.
   * Colour: `objRGB + lightgrid + dynlights` unless FLAG_NOLIGHT. Alpha = objA.
   * Blend per 0.5. Glow scale 0.1..2.0 over z 10..500. Depth bias 20 units toward the camera for 0x04.
   * NoZ sprites (0x10) go last, with no depth test.
2. **Particle system.**
   * Particles live in the list at +0x1c8, ending at +0x1a8. Position +0x24, local to the object TRS.
   * Size +0x1c; colour +0x0c (0..255) × objRGB/255; alpha +0x18 × objA.
   * Texture +0x1e8. Always translucent and sorted.
3. **Poly grid.**
   * w×h heights at +0x1a8, centred grid, with y = signed height × scale.y.
   * Colour from PGColor[idx] at +0x1f8; texture from the sprite tracker at +0x1b4; pan and scale at +0x1cc.
   * Indices at +0x1ac. Optional env map at +0x1c8. Opaque if objA == 255.
4. **Line system.** Walk from +0x1f0 to +0x1b0 via +0x40. Each line is two points in the
   object TRS. Draw `GL_LINES` with rgb×255 and a×objA.
5. **Canvas.** Call `m_Fn(&customDraw, obj, m_pUser)` with the state defaults above. The
   implementation draws world-space fans of at most 100 vertices.
6. **Light.** Colour comes from the object colour, radius from +0x1a8. Lights touch at
   most 40 objects. The object contribution is `(2c-255)*0.7*(1-d/r)`.