# PC NOLF world file (`Worlds\*.dat`, LithTech 2.x, version 66): byte layout as the engine loads it

Status: derived from the byte-matching reconstruction in this repo (server.dll / lithtech.exe /
d3d.ren), and validated with `build/ps2/pcworld.py`, which walks **all 71 world
files in `rez/games/nolf-eu-goty/NOLF.REZ` to the exact last byte** (no gaps, no slack, no
leftovers). It also checks every cross-reference the engine checks, plus several it doesn't.

Conventions:
- All integers are little-endian. Nothing is aligned or padded in the file. Every struct below is
  packed, and the field offsets are file offsets relative to the record start.
- `f32` is an IEEE float. `vec3` is 3×f32 (x, y, z). LithTech space is left-handed with Y up.
- `lstr` = `ILTStream::ReadString` (src/engine/server/basestream.cpp `CBaseStream::ReadString`):
  `u16 len` and then `len` raw chars. There is **no NUL** in the file. The reader truncates to its
  buffer size minus 1 but always consumes all `len` chars, so an over-long string is safe on the
  wire and is only cut short in memory.
- `cstr` = NUL-terminated chars (used only in the texture-name pool).
- "Reader" = the function that consumes the bytes. Every loader runs identically on the server
  (server.dll) and on a remote client (lithtech.exe: the same sources are compiled twice). A
  local client inherits the server's copy (`InheritWorld`).

Contents:
1. File-level layout and load sequence
2. Header
3. World info string, lightmap grid, extents
4. World-tree layout (object quadtree)
5. World-model table and the BSP record (the core: geometry, surfaces, BSP, leaves/PVS, PBlocks)
6. Object list
7. Light animations (lightmaps)
8. What the renderer gets
9. Cross-section consistency rules and limits (the "must be consistent" list)
10. Required vs optional: a minimal first test level
11. Surprises / engine quirks
12. Validation results

---------------------------------------------------------------------------------------------------

## 1. File-level layout and load sequence

```
offset 0                     Header (44 bytes)                         §2
offset 44                    WorldInfo string, LMGridSize, Extents     §3
...                          WorldTree layout                          §4
...                          u32 nWorlds
...                          nWorlds × { u32 nextOffset; BSP record }  §5
hdr.objectListOffset  ==>    Object list                               §6
hdr.lightAnimOffset   ==>    Light-animation (lightmap) block          §7   (runs to EOF)
```

Every shipped file has the sections strictly back to back:
- the last world model's `nextOffset` equals `hdr.objectListOffset`;
- the object list ends exactly at `hdr.lightAnimOffset`;
- the light-animation block ends at EOF.

The engine **depends on the first of these**. `w_ReadWorldLights` reads the object list
*sequentially* from wherever the stream stands after the last `SeekTo(nextOffset)`. Meanwhile
`sm_LoadWorldObjects` seeks to `hdr.objectListOffset`. The two must therefore point at the same
bytes.

Load sequence (server: `CServerMgr::DoStartWorld` → `CServerMgr::LoadWorld`, src/engine/server/smgr.cpp:505,1087):

1. `CServerWorldFull::Load` (src/engine/server/commonmodel.cpp ~5933):
   - header (§2) → info string, grid, extents (§3) → world tree (§4) → world models (§5);
   - then, from the current stream position, `w_ReadWorldLights` reads the object list (§6). It
     keeps only Light/DirLight/ObjectLight, for the light table.
   - then `ResolvePolyKeys` and `CollectFlaggedPolies`.
2. `CWorldData::LoadObjects` (src/engine/server/worldmodels.cpp:855): `SeekTo(hdr.lightAnimOffset)`,
   then the light animations (§7). The name is misleading: it loads lightmaps.
3. `sm_LoadWorldObjects` (src/engine/server/servermodel.cpp:668): `SeekTo(0)`, re-reads dword 0
   and dword 1, then `SeekTo(hdr.objectListOffset)` and creates every object (§6).

Remote client: `CClientShell::DoLoadWorld` (src/engine/lithtech/startload.cpp:779) calls
`LoadWorldQ2B` = the same `CServerWorldFull::Load` (lithtech 0x004224d0), then `Sub4250e0` = the same
`CWorldData::LoadObjects` (0x004250e0). Then:
- `CreateRenderWorld` hands the in-memory world to the renderer;
- `CreateVisContainerObjects` (src/engine/lithtech/csvis.cpp:331) builds client objects for the
  0x10/0x20 BSPs.

`GetWorldInfoString` (client, src/engine/server/commonmodel.cpp `sub_421be0`) reads only §2 and the
info string.

Any reader failure makes the whole load fail with `LT_INVALIDWORLDFILE`. A failure is a version
mismatch, an index out of range, or a stream error.

---------------------------------------------------------------------------------------------------

## 2. Header — 44 bytes

Reader: `w_ReadWorldHeader` (src/engine/server/commonmodel.cpp ~5215). Dword 1 is read again by
`sm_LoadWorldObjects`.

| off | type   | name                | use |
|-----|--------|---------------------|-----|
| 0   | u32    | version             | **must be 66** or the file is refused (`return 0` → LT_INVALIDWORLDFILE) |
| 4   | u32    | objectListOffset    | absolute offset of §6. `CServerWorldFull::Load` reads it and drops it; `sm_LoadWorldObjects` seeks to it |
| 8   | u32    | lightAnimOffset     | absolute offset of §7. Stored at CWorldData+0x68 (`m_nObjectsOffset`, a misnomer) and used by `LoadObjects` |
| 12  | u32[8] | unused              | read into a throwaway. 0 in every shipped file |

Minimal: version 66, two correct offsets, 8 zero dwords. **Required.**

---------------------------------------------------------------------------------------------------

## 3. World info string, lightmap grid size, extents

Reader: `CServerWorldFull::Load`.

| type      | name        | use |
|-----------|-------------|-----|
| u32       | infoLen     | |
| char[infoLen] | info    | world properties text, **no NUL**, stored NUL-terminated at MainWorld+0x1b8 |
| f32       | lmGridSize  | MainWorld+0xf8: world units per lightmap texel (shipped: 20.0). Used by the renderer's lightmap UV math |
| vec3      | extentsMin  | MainWorld+0x144 |
| vec3      | extentsMax  | MainWorld+0x150 |

What the engine derives:
- `BoxMin = extentsMin - 100` and `BoxMax = extentsMax + 100` (+0x168/+0x174);
- `InvExtents = 1/(max-min)` per axis (+0x15c). **A degenerate (zero-thickness) axis divides by
  zero.**
- The point-shade "light table" grid is sized from min/max, with cell size `LightTableRes` from
  the info string (default 250.0, `w_GetShadeCellSize`, src/engine/server/smgrmisc.cpp:372). It is
  filled with `AmbientLight` plus "fast" Light objects (§6).

Info-string commands are CommandParser syntax (`Cmd arg arg;Cmd ...`). Keys seen in shipped files:
- `AmbientLight r g b`: engine `w_GetAmbientLight`. Values clamped 0..255; used for model lighting.
- `LightTableRes n`: engine.
- `WORLDNAME name`: game, `CGameClientShell::GetNiceWorldName`.
- `PBlockSize`, `LMGridSize`, `TerrainSubDivSize`, `MaxLMSize`: preprocessor-only inputs. The
  engine ignores them. lmGridSize and the PBlock cell size are written as binary fields elsewhere.

Example (T01S01): `"AmbientLight 0 16 32"`, grid 20.0, extents (-4096,-256,-1280)..(10048,1088,3648).

Minimal: an empty string (or `AmbientLight 128 128 128`, so models are not black), grid 20.0, and
extents equal to the geometry's bounding box (each axis non-zero). **Required** (the fields must be
present and sane).

---------------------------------------------------------------------------------------------------

## 4. World-tree layout (object quadtree)

Reader: `CWorldTree_F::LoadLayout` (src/engine/server/worldtree.cpp:2693; client twin
src/engine/lithtech/worldtree.cpp:870) → `CWTNode_D::LoadNodes` (worldtree.cpp:1799).

| type  | name      | use |
|-------|-----------|-----|
| vec3  | boxMin    | root node box |
| vec3  | boxMax    | |
| u32   | nNodes    | read and **ignored**. Always equals the number of nodes in the bitstream in shipped files |
| u32   | m_p070    | stored at tree+0x70, never used. Shipped values 0..4; looks like the subdivision depth |
| bits  | topology  | pre-order, **1 bit per node, LSB first**, bytes fetched lazily (a new byte only when the bit cursor reaches 8). Bit 1 = "has 4 children" (quadtree, split in X and Z around the centre), recursing into child 0..3. The last partial byte ends the section |

Size = 32 + ceil(nNodes/8). For T01S01: 237 nodes, 59 with children, 30 bytes of bits.

Use: this is the spatial index for objects and lights (`FindObjectsInBox`, vis queries). The
renderer's whole visibility query starts here: `MainWorld+0x6c` vtable +0x14 → per tree node, vis
containers → the VisBSP's PVS walk (§8).

Objects outside the box go on the tree's root list and still work.

Minimal: box = extents (padded), nNodes = 1, m_p070 = 0, one byte `0x00` (a single leaf node).
**Required**: 33 bytes.

---------------------------------------------------------------------------------------------------

## 5. World models

```
u32 nWorlds
repeat nWorlds:
    u32 nextOffset      absolute offset of the next world's nextOffset field (for the last world: of §6)
    BSP record          §5.1 .. §5.16
```

Reader: `CServerWorldFull::Load`, which calls `w_LoadWorldModel` (src/engine/server/worldbsp.cpp:976)
for each record. After each record it does `SeekTo(nextOffset)`, so any bytes between the record end
and nextOffset would be skipped. Shipped files have none.

**Flag 0x2 double load.** If the BSP's `worldInfoFlags & 0x2`, the loader does `SeekTo(position
after nextOffset)` and loads **the same record again**, with `bCurrent = 0`. It becomes the
"original" copy (CServerWorldRec+0x08) that the transformed copy is reset from. It is *not* a
second record in the file.
- Flag 0x2 is set on every movable world model in shipped data: 16341 of 16514 BSPs.
- The `bCurrent = 0` copy gets node projection code 6 everywhere.
- docs/world.md's (Mac-derived) reading of 0x2 as "separate vis BSP / 1 or 2 records" does not
  match the PC loader.

Each record is a complete, independent BSP world (WorldBsp, 0x150 bytes in memory) with its own
points, planes, surfaces, texture list, polys, nodes and PBlocks. In every shipped file:
- the record list is: world models (doors, props, sky boxes, terrains...) first;
- then **`PhysicsBSP`** (flags 0x14);
- then **`VisBSP`** (flags 0x20), always the last two (71/71 files).

`worldInfoFlags` bits (WorldBsp+0x48):

| bit  | meaning | who looks |
|------|---------|-----------|
| 0x02 | movable: load an untransformed original copy (see above) | CServerWorldFull::Load |
| 0x04 | main world (WORLDFLAG_MAINWORLD) | `IsWorldObject`, `IsMainWorldModel`, modern renderer's DrawAll path |
| 0x08 | "terrain": has terrain sections (§5.16); the client makes one object per section | CreateVisContainerObjects, PrecacheModels |
| 0x10 | physics BSP | `FindFlagged10`/GetPhysicsBSP; PrecacheModels and CreateVisContainerObjects create a world-model object for it |
| 0x20 | vis BSP (has leaves + PVS) | `FindFlagged20`/GetVisBSP; `IsFlagSet` → MainWorld+0x180 bit 2 ("has PVS"); `ResolvePolyKeys`; a world-model object is created for it |

Server `CPrecacheView::PrecacheModels` (smgr.cpp ~8630) creates an OT_WORLDMODEL `BaseClass`
object for every 0x10/0x20 BSP, flags RAYHIT|SOLID, filename = BSP name. Client
`CreateVisContainerObjects` does the same, VISIBLE|RAYHIT|SOLID, at the BSP translation.

Game world-model objects (Door, HingedDoor, TranslucentWorldModel...) set `m_Filename = their Name
property` (e.g. src/game/ObjectDLL/AIVolume.cpp:103). The engine attaches the BSP by
**case-insensitive name match** (`CWorldModelHost::AttachWorldModel` / `wd_FindWorldModel`,
worldmodels.cpp:719,776). Then `se_InitWorldModel` (serverengine.cpp:500):
- **sets the object position to the BSP's `translation`**, ignoring the object's Pos property;
- sets the object dims to `(boxMax - boxMin)/2`.

### 5.1 BSP record header — 32 + 4 + 4 + (2+len) + 48 + 36 bytes

| type    | name | WorldBsp field | use |
|---------|------|----------------|-----|
| u32[8]  | unused | — | read into a throwaway; 0 in all shipped files |
| u32     | worldInfoFlags | +0x48 | see table above |
| u32     | unk_dc | +0xdc | stored, copied on inherit, never read. Shipped values look like a tree depth (6 for a box, 0xED for a PhysicsBSP). Write 0 |
| lstr    | name | +0x04 (char[0x41], read with max 0x40 → ≤ 63 chars kept) | world-model name; must match the game object's Name. Terrain section names are derived as `"#$#%d %s"` and the name must leave 8 chars of room (§5.16) |
| u32     | nPoints | +0xac | |
| u32     | nPlanes | +0x6c | |
| u32     | nSurfaces | +0x7c | |
| u32     | nUserPortals | +0xb4 | |
| u32     | nPolies | +0xa4 | |
| u32     | nLeafs | +0x8c | |
| u32     | nTotalVerts | — | **read and dropped**. Equals Σ base vertex count (nVerts, not nFixVerts) over all polys in every shipped BSP |
| u32     | visDataSize | +0x98 | bytes of the PVS pool (Σ leaf-list sizes) |
| u32     | nLeafLists | +0x84 | Σ non-shared leaf-list counts |
| u32     | nNodes | +0x74 | |
| u32     | nLeafPolyRefs | +0x94 | Σ leaf poly-ref counts |
| u32     | nLMAnimRefs | +0x54 array size | Σ per-poly light-anim back-references (§5.7). docs/world.md calls this "PolyData54, meaning unknown"; see there |
| vec3    | boxMin | +0xc4 | bounding box. Gives world-model object dims; used for containment |
| vec3    | boxMax | +0xd0 | |
| vec3    | translation | +0xe8 | pivot/centre. Shipped: box centre for world models, (0,0,0) for VisBSP, and for PhysicsBSP an arbitrary interior point. The object is placed here (see above); `Recalc` measures the max vertex distance from it (+0x14c clip radius) |

These counts only size allocations (dalloc'd pools). They are summed into `g_WorldBytes`.

**Several pools are filled through unchecked cursors**, so a count *smaller* than the data is heap
corruption, not an error:
- the texture string pool;
- leaf lists;
- vis bytes;
- leaf poly refs.

Use exact counts.

### 5.2 Texture name table

| type | name | use |
|------|------|-----|
| u32 | nStringBytes | size of the pool (Σ (strlen+1)) |
| u32 | nTextures | +0xc0 |
| cstr × nTextures | names | read byte by byte until each NUL into the pool. **No bound check against nStringBytes** |

Names are resource-relative paths, e.g. `Tex\Stone\St18\St0927.dtx`. Some are `.spr` (sprite
animations, used when surface flag 0x4000).

The client binds them in `sub_414240` (src/engine/lithtech/clientmgr2.cpp:1932): `m_pTexture =
cm_AddSharedTexture(names[surface.iTexture])`.
- A missing file prints "Unable to find world texture %s" (debug ≥ 1) and falls back to
  `textures\default_texture.dtx`.
- If that is missing too, the texture is NULL and the poly is drawn **flat/untextured**. It does
  not fail.

### 5.3 Per-poly vertex counts — 2 × nPolies bytes

| type | name | use |
|------|------|-----|
| u8 | nVerts | base vertex count (WorldPoly+0x50) |
| u8 | nFixVerts | extra "T-junction fixed" vertex count (+0x52). 0 = none |

These are read **twice** (worldbsp.cpp regions E and H): once to size the poly block, then a seek
back and a read to carve it.
- In-memory record size = `0x54 + 0x18*(nVerts+nFixVerts)`, rounded up to 4.
- When nFixVerts is 0 the loader sets the fix list = the base list.
- The renderer draws the fix list only when cvar `FixTJunc` is set. Physics and the sphere/centre
  maths use the base list.

### 5.4 Leaves — nLeafs variable records

Reader: worldbsp.cpp region I ("groups"). In memory, Leaf is 0x30 bytes at WorldBsp+0x88.

```
u16 nLists
if nLists == 0xFFFF:
    u16 shareLeaf            take leaf[shareLeaf]'s list count and list pointer (must be an EARLIER leaf:
                             later leaves are still zero at that point)
else:
    nLists × { u16 portal;   0 for list 0 (unconditional); else index into userPortals,
                             used only if that portal is open (flags bit 0) and on screen
               u16 size;
               u8  bits[size] }   PVS bitstream, into the visDataSize pool
u32 nPolyRefs
nPolyRefs × { u16 world; u16 poly }    polys touching this leaf, in ANY world model
f32 unk28                              Leaf+0x28, not used by the engine or the renderers.
                                       Shipped values look like a distance (~1000..7500)
```

After load:
- `ResolvePolyKeys` (commonmodel.cpp ~6220) rewrites every {world, poly} into the `WorldPoly*`
  of `worlds[world].bsp.polies[poly]`.
- **It runs on the first 0x20 BSP only, and fails the whole load (code 2) if either index is out
  of range — or if there is no 0x20 BSP at all.**
- In shipped files the VisBSP's leaves reference **PhysicsBSP** polys (and polys of 0x08 terrain
  models), never its own.
- `wm_ResetGeometry` (worldmodels.cpp:524) computes each leaf's centre/radius from those polys'
  base vertices.
  - Retail divides by the vertex total unguarded: a leaf with 0 refs gets a NaN centre. That is
    harmless for drawing but sloppy.

PVS bitstream: `VisitPVS`, in lithtech.exe (not reconstructed). docs/world.md D.1.2 has it from
the Mac build.
- The bit cursor is global per list, LSB first.
- The tree is walked from the root. At each node: if node.leaf != 0xFFFF, read one bit (1 → that
  leaf is visible). Then, for each child that is a real node (flag 8), read one bit; on 1, descend.
- If the view leaf is NULL or has 0 lists, **every leaf referenced by any node is visited**: the
  "draw all" fallback.

Leaves are only meaningful in a 0x20 BSP. Every other BSP has nLeafs = 0 in shipped data.

### 5.5 Planes — 16 × nPlanes bytes

| type | name |
|------|------|
| vec3 | normal |
| f32  | dist |

Point side = `dot(normal, p) - dist`. Positive = front.

### 5.6 Surfaces — variable records, nSurfaces of them

Reader: worldbsp.cpp region K. In memory, Surface is 0x48 bytes at WorldBsp+0x78.

| type | name | Surface field | use |
|------|------|---------------|-----|
| vec3 | O | +0x04 | texture origin |
| vec3 | P | +0x10 | texture U axis, **texels per world unit** (length = scale) |
| vec3 | Q | +0x1c | texture V axis |
| u16  | iTexture | +0x3e | index into §5.2. **Not range-checked** |
| u32  | iPlane | +0x34 → &planes[i] | **must be < nPlanes**, else the load fails. The surface's plane is every one of its polys' plane, for the BSP split, physics, culling and lightmap axes |
| u32  | flags | +0x38 low 29 bits | SURF_* (table below). The top 3 bits are **replaced** by `math_FindClosestAxisBasis(plane.normal)` (the lightmap plane type); file bits 29..31 are ignored |
| u8   | unk44 | +0x44 | stored, meaning unknown. Shipped: 0/1 in PhysicsBSP, 0 in VisBSP |
| u8[3]| unusedRGB | — | read and discarded (all 0 shipped) |
| u8   | hasEffect | — | **== 1** (exactly) means two strings follow |
| lstr | effectName | (if hasEffect==1) | ≤ 126 chars kept. The server looks it up in the request's world-class list (the server passes NULL, so nothing is ever made there). The client links Pan/Rotate/Warble/Portal/Mirror effects (docs/world.md B.1.2). Shipped example: `"Pan"`, params `"8 -16"` |
| lstr | effectParams | (if hasEffect==1) | ≤ 254 chars kept |
| u16  | textureFlags | +0x3c | game "surface type" id (footsteps/impacts; `GetPolyTextureFlags`). 0 is fine |

Surface flag bits seen and used:

| bit | meaning | consumer |
|-----|---------|----------|
| 0x1 | SOLID: polys collide. **Without it `w_IsPolyBlocking` returns 0** (worldsim.cpp:290) | physics |
| 0x4 | INVISIBLE: never drawn | renderer |
| 0x8 | TRANSLUCENT: a world model whose first surface has it draws in the translucent pass | renderer |
| 0x10 | SKY portal: collected (`CollectFlaggedPolies`, BuildSkyPortalList); marks where the sky shows; not drawn itself | engine/renderer |
| 0x80 | LIGHTMAP: drawn base × lightmap **only if** MainWorld+0x180 bit 4 (LightAnim_BASE exists), else gouraud | renderer |
| 0x100, 0x200, 0x1000, 0x80000, 0x100000 | seen in data. 0x80000 = additive translucency; the rest are mostly editor/preprocessor bits (0x1000 is on almost every surface) | — |
| 0x4000 | texture is a `.spr` sprite animation | client texture binding |
| 0x8000 | PANNINGSKY | renderer |
| 0x400000 | NOSTEP: physics won't step up onto it (WBSP_NOSTEP) | worldsim.cpp:2927 |

Texture coordinates are not in the file. `w_SetupPolyTexCoordsQT1` (commonmodel.cpp ~6338) computes
for every vertex: `u = dot(pos - O, P)`, `v = dot(pos - O, Q)`, in texels. The renderer divides by
the DTX size.

### 5.7 Polys — variable records, nPolies of them

Reader: worldbsp.cpp region L. The vertex counts come from §5.3.

| type | name | WorldPoly field | use |
|------|------|-----------------|-----|
| vec3 | lmOrigin | +0x34 | lightmap origin (world space) |
| u16  | lmWidth | +0x48 (stored as **u8**) | lightmap texels; ≤ 32 in practice (the 1024-texel decompress cap) |
| u16  | lmHeight | +0x49 (u8) | |
| u16  | nLMAnimRefs | +0x10 | |
| nLMAnimRefs × {u16 iAnim; u16 iSlot} | lmAnimRefs | +0x0c → slice of the nLMAnimRefs pool | back-references: anim `iAnim` of §7 lists this poly at position `iSlot` of its poly-ref list. **Verified for every poly in every shipped file (0 mismatches).** The running total is checked against nLMAnimRefs (overflow fails the load) |
| u32  | iSurface | +0x28 | **must be < nSurfaces**, else the load fails |
| (nVerts+nFixVerts) × { u16 iPoint; u8 r; u8 g; u8 b } | verts | +0x54.. | `iPoint` **must be < nPoints**, else the load fails. The colour is stored B,G,R,A(=0xFF) at vertex+0x14 (a D3DCOLOR): the gouraud/vertex-lit colour, used whenever lightmaps are off |

Derived at load and not in the file:
- centre/radius from the base vertices (+0.2) in `wm_ResetGeometry`. A 0-vertex poly → divide by
  zero in retail;
- u/v (§5.6);
- the per-surface poly chain (`w_LinkSurfacePoliesQT1`);
- `WorldPoly+0x16` = own index.

Polys are convex fans in stored order. The visible side is the plane's front: the renderer culls
when `dot(N, cam) - D <= 0.01`.

### 5.8 BSP nodes — 14 × nNodes bytes

Reader: worldbsp.cpp region M. In memory, Node/CWorldItem18 is 0x18 bytes at WorldBsp+0x70.

| type | name | Node field | use |
|------|------|------------|-----|
| u32 | iPoly | +0x00 → polies[i] | **must be < nPolies**, else the load fails. The splitting plane is this poly's surface plane |
| u16 | iLeaf | +0x14 | 0xFFFF = none. In a VisBSP, exactly the nodes whose **front** child is the -1 terminal carry a leaf. **Not range-checked**: keep < nLeafs |
| i32 | back | +0x04 | child index, or **-2** |
| i32 | front | +0x08 | child index, or **-1** |

Child resolution: `w_GetTreeNodeQT1`, worldbsp.cpp:1798.
- -1 → global terminal node (flags 1).
- -2 → global terminal node (flags 2).
- `(int)i >= nNodes` → NULL → the load fails.
- **Any other negative value is not caught**: it becomes a garbage pointer.

Shipped convention, without exception:
- back terminals are always -2 (solid/outside);
- front terminals are always -1 (empty/inside).

The loader presets node flag byte +0x16 = 8 ("real node"). `w_SetNodeProjectionsQT1` sets +0x17
to 0..5 when the plane is axis-aligned (|n| > 0.99999), else 6. Only for bCurrent = 1.

Uses:
- ray casts / IntersectSegment;
- `FindPointInTree` (VisBSP: camera leaf);
- object filtering into nodes;
- **PBlock nodes index this array** (§5.11).

### 5.9 User portals — variable records, nUserPortals of them

Reader: worldbsp.cpp region O. In memory, 0x24 bytes at WorldBsp+0xb0.

| type | name | use |
|------|------|-----|
| lstr | name | ≤ 126 chars, dalloc'd copy. Found by name (`w_FindPortalInWorld`) for the game's portal open/close |
| u32 | unused | read and dropped |
| u16 | flags | +0x04. Bit 0 = open (gates leaf lists > 0) |
| vec3 | center | +0x0c |
| vec3 | dims | +0x18, half extents (on-screen test) |

0 in nearly every shipped BSP. M11S01's VisBSP has 11. **Optional (0).**

### 5.10 Points — 24 × nPoints bytes

| type | name | use |
|------|------|-----|
| vec3 | pos | WorldBsp+0xa8 |
| vec3 | normal | **read and discarded** (shipped data holds a per-vertex normal) |

### 5.11 PBlock table (physics grid) — CSubGrid

Reader: `CSubGrid::Load` (worldmodels.cpp:413, = worldbsp.h CWorldSub108 at WorldBsp+0x108).

```
i32 countX, countY, countZ          (one 12-byte read)
vec3 cellSize                        e.g. 500,500,500 (8192, 2048, 1600 and 1024 also shipped)
vec3 origin                          = the BSP's boxMin in shipped files
for each of countX*countY*countZ cells, index = x + y*countX + z*(countX*countY), x fastest:
    u16 nNodes
    u16 root                         index into this cell's node run, or 0xFFFF / 0xFFFE
    nNodes × { u16 iBspNode; u16 back; u16 front }      (6 bytes each, WSNode, worldsim.h:231)
```

Cell index order matters: `CWorldSub108::GetPointStatus`, worldbsp.cpp:325, returns
`&cells[ix + iz*sliceStride + iy*countX]` with sliceStride = countX*countY. So X varies fastest,
then Y, and **Z is the outermost axis**.

What each field means:
- `iBspNode` indexes **this BSP's node array (§5.8)**, not polys. Physics reads `node.poly`'s
  vertices and `node.poly.surface.plane`.
- `back`/`front` index **this cell's own node run**, or a terminal:
  - 0xFFFF = WS_NODE_SOLID;
  - 0xFFFE = WS_NODE_EMPTY.
- In shipped data, back terminals are 0xFFFF and front terminals 0xFFFE, mirroring BSP -2/-1.
  The cell tree is the BSP pruned to the cell box.
- An empty cell is `nNodes = 0, root = 0xFFFF`: nothing to collide with.

How the grid is used:
- `GetPBlock(point)` (the WorldBsp vtable slot) truncates `(p - origin)/cellSize` per axis and
  clamps it to `[0, count-1]`. So any point maps to *some* cell; one 1×1×1 cell covering
  everything works.
- **If countX/Y/Z are 0, `GetPBlock` returns NULL.** `ws_MoveObject` (worldsim.cpp:1844) then
  prints "World %s doesn't have PBlocks!" and the object does not collide with that BSP.
- This is what `ws_MoveObject`, `ws_StairStep`, `ws_MoveCylinder` and `ws_MoveBox` walk
  (worldsim.cpp ~2313, 2731, 3033), with a 1024-entry u16 stack.
- **The PhysicsBSP and every solid world model need a non-empty grid for collision.**
- Shipped VisBSPs (and 0x08 terrain models) have `0,0,0` and no cells.

Shipped: PhysicsBSP 29×3×10 at 500 (T01S01); world models mostly 1×1×1 at 500.

### 5.12 Root node — 4 bytes

`i32 root`, resolved like a child (§5.8); NULL fails the load. Normally 0. Terrain models use -2.

### 5.13 .. 5.16 Terrain sections ("CWorldSimple", the 0x08 BSPs)

```
u32 nSections
nSections × {
    u32 nNodes
    nNodes × { u32 iPart (parent poly index, < parent nPolies); i32 back; i32 front }
                                              (children resolved within this section's own nodes; -1/-2 terminals)
    i32 root
    u32 nParts
    nParts × u32 iPoly                         (< parent nPolies)
    CSubGrid                                   (exact §5.11 format)
    u8 hdr[4]; u32 hdr2                        (wb_ReadSimpleHeader: 4 bytes + 1 dword, stored, unused)
    vec3 centre                                (radius computed from the parts)
}
```

Reader: `CWorldSimple::Load` (worldbsp.cpp:1965). Each section is named `"#$#<i> <parentName>"`;
the name must fit, which caps the parent name at 63-8 chars.

Shipped: T01S01 "Terrain1" has 2 sections. All other BSPs have 0. **Optional: write `u32 0`.**

---------------------------------------------------------------------------------------------------

## 6. Object list

Readers:
- `sm_LoadWorldObjects` (servermodel.cpp:668) creates objects.
- `w_ReadWorldLights` (commonmodel.cpp ~5673) reads Light, DirLight and ObjectLight for the light
  table.

```
u32 nObjects
nObjects × {
    u16  recordSize          bytes from after this field to the end of the record (= the lstr through the last prop)
    lstr className           ≤ 255 chars
    u32  nProps
    nProps × {
        lstr name            ≤ 255 read; ≤ 79 kept (ObjectProp::m_Name)
        u8   type            PT_*: 0 string, 1 vector, 2 color, 3 real, 4 flags, 5 bool, 6 longint, 7 rotation
        u32  propFlags       editor PF_* bits (0x40, 0x2000, 0x1, ...); read and ignored
        u16  dataLen
        u8   data[dataLen]
    }
}
```

What the engine does with them:
- `recordSize` is ignored by `sm_LoadWorldObjects`. **`w_ReadWorldLights` uses it**
  (`SeekTo(recordStart + recordSize)`) to skip non-light records, so it must be exact. Every
  shipped record matches exactly (asserted by the parser).
- An unknown class only prints "Server is missing class %s".
- Classes flagged CF_NORUNTIME, or not CF_ALWAYSLOAD unless the caller asks for all, are skipped.
- Properties go on a list that the game's PreCreate reads with `GetPropGeneric` / `GetPropReal`,
  etc.

Data encoding by type (verified on M01S01):

| type | dataLen | data |
|------|---------|------|
| 0 string | 2 + n | **an lstr** (u16 n + chars) inside the data. The engine re-reads it with ReadString(0x258) and copies `dataLen-1` |
| 1 vector, 2 color | 12 | vec3 (colour 0..255 floats) |
| 3 real | 4 | f32 |
| 4 flags, 6 longint | 4 | **f32**: the engine reads longints as `(long)m_Float` (smgr.cpp `si_GetPropLongInt`). Shipped e.g. `Type` = `0000803f` (1.0) |
| 5 bool | 1 | u8 |
| 7 rotation | 16 | f32 euler pitch, yaw, roll (radians), plus one f32 that is always 0 and ignored |

`w_ReadWorldLights` quirks: any property of a Light-class record longer than 0x80 bytes makes it
seek to the record end mid-loop and keep reading the next "property" from there. Keep light props
≤ 0x80 bytes.

What the game needs (src/game/ObjectDLL): nothing is engine-required.
- Without a `GameStartPoint`, `CGameServerShell::CreatePlayer` spawns the player at (0,0,0)
  (GameServerShell.cpp:1581).
- `GameStartPoint` forces its own name to "GameStartPoint" (GameStartPoint.cpp:114), and the
  finder matches by name.
- `WorldProperties` carries FarZ, fog, sky panning, environment map and music.
- Every Door/TranslucentWorldModel/etc. object must name an existing BSP.
- Shipped T01S01: 426 objects; M01S01: 731; M11S01: 3626.

Minimal: `nObjects = 1` with a GameStartPoint (Name, Pos, Rotation, GameType "Single"), plus
WorldProperties for sane FarZ/fog. `nObjects = 0` also loads, with the player at the origin.

---------------------------------------------------------------------------------------------------

## 7. Light animations (lightmaps) — at hdr.lightAnimOffset, to EOF

Reader: `CWorldData::LoadObjects` (worldmodels.cpp:855). The format and codecs are documented in
docs/world.md part C (src/modern/render/docs/world.md:1291).

```
u32 nTotalPolyRefs       pool sizes; every cursor is bounds-checked against them (LT_ERROR on overflow)
u32 nTotalFrames
u32 nTotalDataBytes
u32 nTotalFrameEntries   (= Σ nFrames*nPolies)
u32 nAnims
nAnims × {
    lstr name            ≤ 31 chars ("LightAnim_BASE", "<light>__LA", ...)
    u32  bShadowMap      0 = colour lightmaps (16-bit RLE of X1R5G5B5), 1 = shadow map (8-bit two-colour RLE)
    u32  nFrames
    u32  nPolies
    nPolies × { u16 world; u16 poly }          which polys (any world model) this anim covers
    nFrames × nPolies × { u32 size; u8 data[size] }   per frame, per poly, compressed lightmap of lmWidth×lmHeight
}
```

Behaviour:
- If an anim named `LightAnim_BASE` exists (case-insensitive), MainWorld+0x180 gets bit 0x4.
  Its frame pair and blend are zeroed.
- **Only then** does the renderer lightmap SURF_LIGHTMAP polys: PC d3d.ren 0x100256d0 cascade;
  Mac `r_SetPolyFunctions`; modern `mr_lightmap.cpp:363`.
- Without it, those polys are drawn gouraud with the file's vertex colours.
- The {world, poly} refs are not validated at load and must be valid.
- Each poly's §5.7 back-refs must point back here.

Shipped: T01S01 has 2 anims (`LightAnim_BASE` covering 3888 polys, 128480 data bytes; plus
`cigarette__LA`). M11S01 has 28.

Minimal: **five zero dwords** (nAnims = 0). Every poly then has nLMAnimRefs = 0 (and nLMAnimRefs =
0 in each BSP header), and lmWidth/lmHeight can be 0. This gives vertex-colour lighting: set the
vertex RGB to 255,255,255 for fullbright.

---------------------------------------------------------------------------------------------------

## 8. What the renderer gets (nothing is read from the file by d3d.ren)

`CreateRenderWorld` hands d3d.ren a `MainWorld*`.
- d3d.ren (src/engine/d3d/*.cpp) and the modern renderers (src/modern/render/ltworld.h,
  mr_scene.cpp, mr_lightmap.cpp) read the in-memory structures directly:
  - WorldData {flags, bsp, originalBsp, renderBsp} per world;
  - WorldBsp, Node, Leaf, WorldPoly, Surface (offsets in ltworld.h / docs/world.md part B);
  - the LMAnim arrays.
- Field-for-field, the renderer consumes:
  - poly vertices → `points[]`;
  - u/v computed from O/P/Q;
  - vertex RGB;
  - lmOrigin/lmWidth/lmHeight plus lmGridSize and the surface's plane-type bits (lightmap UVs);
  - surface flags and texture;
  - node tree, leaves and PVS (visibility);
  - world-model objects' transforms.

The per-frame visibility path (retail PC d3dqip4.cpp:778 `sub_1007b580`):
- **If MainWorld+0x180 & 2** (a 0x20 BSP exists) and not DrawAll:
  - world-tree query → the VisBSP container's `VisitPVS` from `FindPointInTree(VisBSP, camera)`;
  - each visible leaf queues its `polyRefs`. These are PhysicsBSP/terrain polys: **what is drawn
    is the PhysicsBSP geometry, selected by VisBSP leaves**.
- **Else (or DrawAll)**: `r_DrawBSP(GetVisBSP())` queues every poly of the **VisBSP itself**.
  - If no 0x20 BSP exists, GetVisBSP returns NULL: "ERROR: GetVisBSP returned LTNULL!" and no
    world is drawn. This is moot: such a file never loads (§5.4 `ResolvePolyKeys`).
  - The modern renderer's DrawAll path draws the 0x04 main world's polys instead (mr_scene.cpp:917).
- Then come the sky (SURF_SKY polys plus SkyPointer objects), world models (each its own BSP under
  its transform) and objects.

---------------------------------------------------------------------------------------------------

## 9. Consistency rules and limits

Checked by the engine (load fails):
- version == 66;
- surface.iPlane < nPlanes;
- poly.iSurface < nSurfaces;
- vertex iPoint < nPoints;
- Σ poly lmAnimRef counts ≤ nLMAnimRefs;
- node.iPoly < nPolies;
- node children ∈ [0, nNodes) ∪ {-1, -2} (other negatives are NOT caught);
- root resolvable;
- terrain part/node indices;
- **a 0x20 BSP exists**, and all its leaf poly refs {world < nWorlds, poly < that world's
  nPolies};
- light-anim pool totals ≥ use;
- stream error at the end.

Not checked (corrupt memory or misbehave instead):
- texture pool overflow; surface.iTexture ≥ nTextures;
- leaf-list, vis-byte and leaf-ref pool overflow (counts must be ≥ actual; use exact);
- shared-leaf index (must be earlier);
- node.iLeaf < nLeafs;
- PBlock root/children < cell nNodes or 0xFFFE/0xFFFF; PBlock iBspNode < nNodes;
- light-anim {world, poly} refs; poly back-refs (only the renderer uses them);
- object recordSize (must be exact for w_ReadWorldLights).

Limits from field widths and fixed buffers:
- **≤ 255 base verts and ≤ 255 fix verts per poly** (u8). **Physics clips with
  `WS_MAX_POLY_VERTS = 40`** (worldsim.h:354) and retail has no bound check (stack overflow
  beyond). Shipped max is 24. Keep ≤ 32 to leave room for clipping.
- Poly index u16 in: leaf refs, light-anim refs, `WorldPoly+0x16`, surface poly chains (0xFFFF =
  end), and HPOLY (world << 16 | poly). So **< 65535 polys per BSP**.
- PBlock: iBspNode u16 (**< 65534 BSP nodes per BSP if PBlocks reference them**); < 65534 nodes
  per cell; traversal stack 1024 (deep cell trees overflow it). The BSP walker uses a 1000-entry
  stack (worldsim.cpp:8).
  - Shipped maxima: 59278 BSP nodes, 23234 polys and 33585 nodes in one cell.
- Lightmaps ≤ 32×32 texels (1024-texel decompress buffer); lmWidth/lmHeight are stored as u8.
- Names: BSP ≤ 63, portal ≤ 126, effect ≤ 126/254, anim ≤ 31, object prop name ≤ 79 (in memory).
- Leaf lists: count u16 (0xFFFF reserved for "share"), size u16.

Coordinate/semantic conventions to reproduce:
- Plane test is `n·p - d`, front positive.
- A face is visible from its front.
- BSP back-terminal = -2 (solid), front-terminal = -1 (empty). The leaf index sits on the node
  whose front is -1.
- PBlock 0xFFFF back / 0xFFFE front.
- u/v in texels: P/Q have length = texels per unit.
- Vertex colour order in the file is R, G, B.

---------------------------------------------------------------------------------------------------

## 10. Required vs optional — a minimal first test level (loads, renders vertex-lit, collides)

| section | verdict | minimal content |
|---------|---------|-----------------|
| §2 header | REQUIRED | 66, objOfs, lmOfs, 8×0 |
| §3 info/grid/extents | REQUIRED | "" or "AmbientLight 128 128 128"; 20.0; real bounding box |
| §4 world tree | REQUIRED | box, 1, 0, byte 0x00 |
| §5 nWorlds | REQUIRED | 2 + number of world models |
| world models (doors...) | optional | 0. If present: flags 0x2, a name matching the object's Name, box/translation = their bounds/centre, own PBlock grid for collision |
| PhysicsBSP (flags **0x14**) | REQUIRED | all level geometry. Points, planes, surfaces (SOLID 0x1 for collision; omit 0x80 without lightmaps), polys (vertex RGB 255 = fullbright), BSP nodes (needed for ray casts and as PBlock targets), nLeafs 0, PBlock grid non-empty (simplest: 1×1×1 cell, cellSize ≥ box size, origin = boxMin, the cell's node run = the whole BSP with -2→0xFFFF and -1→0xFFFE, root = BSP root; ≤ 65533 nodes), root, nTerrain 0 |
| VisBSP (flags **0x20**) | REQUIRED (the load fails without one) | can be tiny: 3 points, 1 plane, 1 surface, 1 texture name, 1 triangle poly, **1 node {poly 0, leaf 0, back -2, front -1}**, root 0, PBlock grid 0,0,0, **1 leaf**: nLists 1 → {portal 0, size 1, bits 0xFF} (or nLists 0, which relies on the draw-all fallback), polyRefs = {PhysicsBSP world index, i} for every PhysicsBSP poly, f32 0. nLeafLists 1, visDataSize 1, nLeafPolyRefs = PhysicsBSP nPolies. Later, a real PVS (multiple leaves) only improves culling |
| user portals | optional | 0 |
| terrain sections | optional | 0 |
| §6 objects | optional for loading; recommended | GameStartPoint (+ WorldProperties) |
| §7 light anims | optional | 5×u32 0. All polys: nLMAnimRefs 0 |
| per-poly lightmap fields | optional | lmOrigin any, lmW/H 0 |
| fix (T-junction) verts | optional | 0 |
| per-point normals | ignored | zeros |
| surface effects | optional | hasEffect 0 |
| texture flags / unk44 / unk_dc / nTotalVerts / 8 unused dwords | ignored or cosmetic | 0 (nTotalVerts may be the real sum; it is dropped) |

The VisBSP's own geometry is never drawn on the PVS path. Its single poly only has to give the
node a plane. Make that plane put the whole level on the **front** side (e.g. a horizontal plane
below the level with its normal up). Then `FindPointInTree` returns leaf 0 for any camera: the root
node carries leaf 0, so the leaf is remembered regardless of the side taken.

Objects get filtered into VisBSP nodes, so with one node all objects share leaf 0's visibility.
That is fine.

---------------------------------------------------------------------------------------------------

## 11. Surprises / quirks

1. **The 0x2 flag re-reads the same record** (the "original" copy for movers). There is never a
   second record in the file. docs/world.md §7 / B.1.1 (Mac reading) should be read with that in
   mind.
2. **No 0x20 VisBSP → the world fails to load** (`ResolvePolyKeys` returns 0 → code 2 →
   LT_INVALIDWORLDFILE).
3. The **drawn** geometry is the PhysicsBSP's, chosen through VisBSP leaves. Retail's DrawAll path
   instead draws the VisBSP's own (coarser) polys. The modern renderer differs and draws the
   0x04 world.
4. The header's "objects offset" field at CWorldData+0x68 (worldmodels.cpp) actually holds the
   **lightmap** offset: dword 2. Dword 1 is the object list.
5. The object list is read twice: once sequentially (lights only, needing exact recordSize), once
   by seek.
6. `nTotalVerts` (the 7th count) is read and dropped. The leaf `f32`, the point normals, the
   surface RGB and the user-portal dword are read and dropped too. `unk_dc`, `unk44` and the
   world-tree `m_p070` are stored and never used.
7. The "edges" array (WorldBsp+0x54, per-poly `{u16,u16}` at WorldPoly+0x0c, called PolyData54 in
   docs/world.md) is **the poly → light-animation back-reference list**: `{iAnim, iSlotInAnim}`.
   This is verified exhaustively on all 71 files.
8. lmWidth/lmHeight are u16 on the wire but stored as u8.
9. Surface flag bits 29..31 from the file are overwritten (lightmap plane type).
10. PT_LONGINT and PT_FLAGS values are stored as **floats**.
11. PBlock node records point at **BSP nodes**, not polys. The cell index order is
    `x + y*cx + z*cx*cy`. A grid with a zero dimension = "doesn't have PBlocks" (no collision,
    console spam per move).
12. `se_InitWorldModel` moves every world-model object to the BSP's `translation` and sizes it from
    the BSP box. The object's own Pos property does not position the geometry.
13. Retail divide-by-zero on 0-vertex polys and on 0-poly leaves (centres become NaN). The modern
    build guards these (`NOLFSRC_MODERN`, RETAIL_BUGS.md).
14. `w_GetTreeNodeQT1` uses a signed compare: child indices < -2 are not rejected.
15. Unchecked pools (texture names, leaf lists, vis bytes, leaf refs) make exact counts mandatory.
16. Info-string keys `PBlockSize`/`LMGridSize`/`MaxLMSize`/`TerrainSubDivSize` are inert at
    runtime. The engine reads only `AmbientLight` and `LightTableRes` (case-insensitive parser;
    shipped files use `ambientlight` too).

---------------------------------------------------------------------------------------------------

## 12. Validation (`scripts/pcworld.py`)

`python3 -I build/ps2/pcworld.py [-v] file.dat ...`; `--rez <path.rez>` parses
every world inside a rez.

The script walks every section in file order and asserts all the §9 ranges, including the unchecked
ones:
- pool totals;
- PBlock child/root ranges;
- leaf indices;
- string prop lengths;
- record sizes;
- the light-anim back-ref identity.

It then reports any gap or overlap and whether the parse ends at EOF.

Results:
- **All 71 world files in NOLF.REZ (NOLF GOTY EU) parse to EXACT EOF**, with 0 back-ref mismatches.
- Every file ends with PhysicsBSP (0x14) and VisBSP (0x20).
- The 8 unused header dwords and the 8 unused BSP dwords are 0 everywhere.
- The world-tree `nNodes` dword equals the bitstream node count in all 71 files.

| | T01S01 (1,565,749 B) | M01S01 (3,101,263 B) | M11S01 (11,463,901 B) |
|---|---|---|---|
| objOfs / lmOfs | 1,072,940 / 1,405,793 | 1,937,679 / 2,736,729 | 7,070,564 / 9,683,562 |
| world tree nodes | 237 | 229 | 305 |
| BSP records | 23 (20×0x2, 1×0x8, 0x14, 0x20) | 144 (139×0x2, 3×0x8, 0x14, 0x20) | 639 (637×0x2, 0x14, 0x20) |
| PhysicsBSP pts/planes/surfs/polys/nodes | 4397/1137/2389/3848/7214 | 5141/973/2490/5049/11309 | 39170/9632/22404/23234/42583 |
| PhysicsBSP PBlocks | 29×3×10 @500, 48404 nodes | 14×4×11 @500, 96359 nodes | 2×1×1 @8192, 50946 nodes |
| VisBSP polys/nodes/leaves | 998/1411/335 | 2034/3388/616 | 3948/5873/962 (11 portals) |
| VisBSP leaf lists / PVS bytes / leaf refs | 233 / 14841 / 4769 | 597 / 66988 / 6618 | 1476 / 61824 / 28681 |
| totals pts/planes/surfs/polys/textures | 6495/1662/3438/5094/205 | 10727/2672/5442/8431/439 | 60856/18390/35052/36755/1288 |
| objects / props | 426 / 12956 | 731 / 30959 | 3626 / 103488 |
| light anims (refs / data bytes) | 2 (3915 / 128561) | 2 (6052 / 316032) | 28 (30765 / 1533559) |
| section bytes: header+info+tree+worlds | 1,072,940 | 1,937,679 | 7,070,564 |
| objects | 332,853 | 799,050 | 2,612,998 |
| light anims | 159,956 | 364,534 | 1,780,339 |
| parse end | EXACT | EXACT | EXACT |
