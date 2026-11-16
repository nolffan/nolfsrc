# PS2 NOLF world file (`worlds/*.ltb`, version 66): binary layout

This is the layout as decoded by `scripts/ps2world.py`. The parser reads all 16 PS2 worlds in
`extract/Worlds/` (thiefmission1-3, t01s01-t10s01, t01s02, t01s03, m01s01) from start to EOF and
accounts for every byte. Each section boundary is checked against the header offsets and the
per-model "next record" offsets.

Sources used: the LithTools GDScript reader (`LTB_PS2.gd`, `LTB_PS2_Level_V66.bt`) as a starting
point. Its header, world-tree, texture-list, polygon and object layouts are mostly right, but
its user-portal layout is wrong, and its PBlock, root/terrain and render-data parts are wrong or
skipped. The PC structures are taken from this repo's reconstructed engine
(`src/engine/server/worldbsp.cpp` `w_LoadWorldModel` and `CWorldSimple::Load`) and from
`src/modern/render/docs/world.md` (Part B §7 gives the PC record order). PS2 and PC share the same
LithTech 2 BSP structures. The PS2 version mainly differs in vertex data, surfaces and lighting.

All values are little-endian. `str16` = u16 byte count + bytes. The count includes the trailing NUL
for texture and object strings but not for user-portal names, so strip NULs. `vec3` = 3×f32.
Coordinates are plain IEEE floats in LithTech world units. There is **no quantisation or packing**
of positions, planes or UVs.

## 0. File order (thiefmission1.ltb, 3,804,224 bytes)

| # | section | offset | size |
|---|---|---|---|
| 1 | header | 0 | 44 |
| 2 | world info string + LM grid + extents | 44 | 53 |
| 3 | world tree (quadtree layout bits) | 97 | 75 |
| 4 | texture list | 172 | 2,526 |
| 5 | world models: 153 BSP records | 2,698 | 2,883,279 |
|   | … 151 small models (AIVolume, TranslucentWorldModel, doors, sky…) | 2,698 | 274,723 |
|   | … `PhysicsBSP` (main world, render + collision) | 277,421 | 1,869,340 |
|   | … `VisBSP` (leaves/PVS) | 2,146,761 | 739,216 |
| 6 | static light list (426 lights) | 2,885,977 | 23,860 |
| 7 | light grid (29×6×36 cells) | 2,909,837 | 25,124 |
| 8 | lightmap pages (133 × 64×64 CLUT8) | 2,934,961 | 680,968 |
| 9 | objects (396) | 3,615,929 | 188,295 |

## 1. Header (44 bytes)

```
u32 version            = 66
u32 objectDataPos      -> section 9
u32 lightmapDataPos    -> section 8   ("RenderDataPos" in LithTools)
u32 0                  (always 0)
u32 13                 (always 13; meaning unknown)
u32 lightListPos       -> section 6
u32 lightGridPos       -> section 7
u32 0, 0, 0, 0
```
PC v66 uses version, objPos, renderPos and then 8 ignored dwords. The PS2 version stores its extra
section offsets in those dwords.

## 2. World info
`u32 len; char info[len]` (e.g. `ambientlight 56 56 56`), `f32 lmGridSize` (20), `vec3 extentsMin`, `vec3 extentsMax`.

## 3. World tree
`vec3 boxMin, boxMax; u32 nNodes; u32 terrainDepth` (0, or 1 in t08/t10), then the layout bits. The
bits are read depth-first, one per node, LSB first. Each set bit means the node has 4 children. The
bit count equals nNodes (341 in thiefmission1). The same structure is used on PC.

## 4. Texture list
`u32 nTextures; u32 nBytes; str16 name[nTextures]` (NUL included; nBytes = total bytes of the
strings incl. their u16 prefixes). On PS2 this is **one list for the whole world**. The PC version
has a per-BSP list. `Surface.texture` indexes this list.

## 5. World models
`u32 nRecords`, then per record `u32 nextRecordOffset` (absolute) followed by **exactly one** BSP.
Even models with info flag 0x02 have only one BSP. On PC, flag 0x02 adds a second BSP.

### 5.1 BSP record
```
str16 name
u32 infoFlags        0x02 world model, 0x14 = MAINWORLD|PHYSICS ("PhysicsBSP"), 0x20 = VIS ("VisBSP"), 0x08 terrain model
u32 unkDC            PC WorldBsp+0xdc, meaning unknown on both platforms
u32 nPoints, nPlanes, nSurfaces, nUserPortals, nPolies, nLeaves
u32 nVerts           sum of poly vertex counts (checked)
u32 visDataSize      total leaf-list bytes
u32 nLeafLists
u32 nNodes
u32 nLeafPolyRefs    total of the leaves' poly-ref counts
u32 nLightmappedPolys  == number of polys with lmPage >= 0 (true in all 1469 BSPs)
vec3 boxMin, boxMax, worldTranslation   (translation = the object's Pos for world models)
u32 unkSize          sits where PC has the per-BSP texture-name block; looks like a memory
                     size (PhysicsBSP 938,832; a 6-poly box 896); not needed
u8  nVerts, u8 mode    × nPolies
```
`mode` is the PC "nFixVerts" byte, but on PS2 it is a **render mode**: 0 = lightmapped,
2 = bright/unlit (flags 0x121, glass), 3 = vertex-coloured gouraud (0x1101, 0x401001),
4 = invisible (0x1085). No extra vertices are stored for it.

**Leaves** (×nLeaves). Same as PC:
`u16 n` (0xFFFF → `u16 shareIndex`), else n × {`i16 portal; u16 size; u8 data[size]`} (compressed
PVS), then `u32 nRefs; {u16 world, u16 poly}[nRefs]; f32 unk`. In thiefmission1 every VisBSP leaf
ref points at world 151 = PhysicsBSP. **The PVS works on PhysicsBSP polygons.**

**Planes** (×nPlanes): `vec3 normal; f32 dist` (n·p = dist).

**Surfaces** (×nSurfaces, 52 bytes + effect):
```
vec3 O, P, Q         texture origin/axes - valid only for the first poly(s) of the surface
u32 plane            a plane index (the surface's first poly's); NOT the plane of each poly
u32 flags            SURF_*: 1 solid, 4 invisible, 8 translucent, 0x10 sky, 0x20 bright,
                     0x80 lightmap, 0x100 nosubdiv, 0x1000 gouraud, 0x400000 / 0x200000 unknown
u32 texture          index into §4
u16 textureFlags     (game surface type)
u16 hasEffect        if nonzero: str16 effect, str16 effectParam
```
The PS2 compiler **merged surfaces**: PhysicsBSP has 77 surfaces for 5930 polys. There is roughly
one surface per (texture, flags). O/P/Q do not reproduce the UVs of most polys. **Use the per-vertex
UVs**, and recompute OPQ per poly for the PC (LithTools `UVMath.convert_uv_to_opq` does the same:
3 vertices → O,P,Q scaled by texture size).

**Polygons** (×nPolies): a 40-byte header plus vertices.
```
i32 lmPage           lightmap page index (§8), -1 = none
u8  lmW, lmH         lightmap rectangle size in texels (often 1×1)
u8  lmX, lmY         rectangle position in the 64×64 page
u32 surface
u32 plane            the poly's own plane
f32 unk              values cluster at -0.0 / 0.0 / -1.0 (±1e-6); meaning unknown
vec3 center          = average of the poly's vertices
f32 radius           = max distance from center + 0.2
```
The vertex format depends on the surface flags:
* `flags & 4` (invisible): `u32 point` per vertex (no UV).
* `flags & 0x80 && !(flags & 8)` (lightmap kind): 20 bytes: `u16 point; u16 0; f32 u, v; f32 lmU, lmV`.
* otherwise (colour kind): 16 bytes: `u16 point; u16 0; f32 u, v; u8 r, g, b, a`.

`point` indexes the BSP's point list. `u, v` are base-texture coordinates in **texture repeats**
(already divided by the texture size; multiply by the DTX width/height for PC texel UVs). `lmU, lmV`
are 0..1 coordinates on the 64×64 lightmap page. They fall inside [lmX, lmX+lmW] / 64 with a
half-texel inset. Polys with lmPage = -1 still carry lmU/lmV. They have lmW=lmH=0 and their values
are junk (sky, 25 polys). In colour-kind polys `a` is always 127, and `rgb` is a 0-255 light value:
ambient-only vertices hold the ambient value (56,56,56), and bright surfaces hold (128,128,128).
Polys are convex fans in stored order, as on PC.

**Nodes** (×nNodes, 16 bytes): `u32 poly; i32 leaf (-1 none); i32 side[2]` (node index, -1 = in,
-2 = out). PC stores the leaf as u16, the PS2 as a full dword.

**User portals** (×nUserPortals): `str16 name; u32 unk (0); u16 flags (0x1c); vec3 center; vec3 dims`.
Only t01s03's VisBSP has them (9). LithTools reads `int,int,ushort` here, which is wrong.

**Points** (×nPoints): `vec3 pos; f32 w` (always 1.0). PC has vec3 + vec3.

**PBlock table** (collision grid, same as PC `m_Sub108`):
```
u32 sx, sy, sz; vec3 blockSize (500,500,500); vec3 origin; u32 nRecords
nRecords × {u16 node; i16 side0; i16 side1}   per-block mini BSPs over the BSP's nodes;
                                              sides are block-local record indices, -1 in / -2 out
sx*sy*sz × {u32 0 (runtime pointer); u16 nRecords; u16 root (= nRecords-1)}
```
The VisBSP has a 0×0×0 table, which is still 40 bytes.

**Tail**: `i32 rootNode` (0, or -2 for terrain models), `u32 nTerrainSections`, sections.

**Terrain section** (models with flag 0x08: t01s01 Terrain1, m01s01 Dome1/Canopy_a/b, t02s01 grave,
t08/t10 Terrain1 with 4 sections, t10 EggChair). Same as PC `CWorldSimple::Load`:
`u32 nNodes; nNodes × {u32 poly, i32 side0, i32 side1}; i32 root; u32 nPolys; u32 poly[nPolys];
PBlock table; u8 h[4], u32 h4 (header, zeros); vec3 center`.

### 5.2 What is render and what is collision
There are **no separate PS2 render batches or triangle strips** in the file. The rendered world is
the `PhysicsBSP` polygons (convex fans) with their UVs, and the lightmap pages or vertex colours.
The same PhysicsBSP also carries the BSP nodes and the PBlock grid used for collision. VisBSP is a
coarser hull: its polygons are not lightmapped (lmPage -1), 1487 of its 3685 poly centres coincide
with PhysicsBSP polys, and it holds 568 leaves. Its leaf poly refs index PhysicsBSP polys.

World models (doors, translucent panes, rotating models, sky pieces) are separate BSP records. Each
is named exactly like the object whose `Name` property matches. All 151 non-main BSPs in
thiefmission1 have a matching object: AIVolume 73, TranslucentWorldModel 73, Door 2,
RotatingWorldModel, Weather, DemoSkyWorldModel. Their points are in **world space**, and
`worldTranslation` equals the object's `Pos`.

## 6. Static light list
`u32 n`, n × 56 bytes:
`f32 radius; f32 r, g, b (0-255); f32 unk[3] (always 0); vec3 pos; vec3 dir; f32 cosCone (-1 = omni,
e.g. 0.866 = 30° spot)`. These are the level's static lights. thiefmission3 has none.

## 7. Light grid (model ambient lighting volume)
```
u32 count = nx*ny*nz; u32 nx, ny, nz; u32 nx-1, ny-1, nz-1; u32 nx*ny
vec3 cellSize (250); vec3 1/cellSize; vec3 origin (= world extents min)
count × {u8 r, g, b, 0}       x fastest, then y, then z
```

## 8. Lightmap pages
`u32 sectionSize (incl. these 8 bytes); u32 nPages`, then nPages × 5120 bytes:
`u8 clut[256][4]` followed by `u8 index[64*64]` (row-major, linear, not GS-swizzled).

* The CLUT is in PS2 **CSM1 order**: logical index i is stored at entry `i` with bits 3 and 4 swapped
  (`ps2world.clut_index`). This was verified on every page: every stored non-zero entry is reached.
* Entry bytes are `(dR, dG, dB, L)`, all multiples of 8 (5-bit source). Entry 0 of every level equals
  its ambient: thiefmission1 `00 00 00 38` for ambient 56,56,56, and m01s01 `08 00 00 20` for ambient
  28,32,36. This fits **colour = (L-dR, L-dG, L-dB)**, where L is the brightest channel and d is how
  far each channel falls below it. Decoded pages look like proper lightmaps with warm and cool lights
  (checked visually). The decode is not yet compared against PC lightmaps. On the PS2, L is likely
  drawn as a modulating alpha and d as a subtractive pass.
* Each lightmapped poly owns a unique rectangle (no two polys share one in thiefmission1).

## 9. Objects
```
u32 nObjects; u32 nStrings
u32 stringBytes (= Σ(len+3)); u32 unk; u32 unk; u16 unk; u16 unk
str16 strings[nStrings]          (class names, property names, string values)
per object: u16 dataLen; u32 classString; u32 nProps;
  per prop: u32 nameString; u8 type; u16 len; value
```
Property types: 0 string = `u32 stringIndex`, 1 vector and 2 color = vec3, 3 real = f32, 4 flags and
6 longint = f32 (as on PC), 5 bool = u8, 7 rotation = 4×f32 **Euler (pitch, yaw, roll, 0)**, not a
quaternion. `len` is checked for every property. thiefmission1 has 396 objects; the largest classes
are AIVolume 73, TranslucentWorldModel 73, PropType 61, ScaleSprite 53, AINode 43, DoorKnob 20 and
Trigger 13. There are no `Light` objects: lighting is baked into sections 6 to 8.

## 10. Geometry check (thiefmission1)
`thiefmission1.obj` (PhysicsBSP) and `thiefmission1_visbsp.obj` were written by the parser.
* PhysicsBSP: 9729 points, 5930 polys, 23620 verts. bbox (-3456,-64,-6624)..(3712,1216,2368),
  which equals the header extents and the world-tree box. No NaN or inf.
* Planarity (|n·p - d| per vertex): max 0.005 in most levels. In thiefmission1, 7 polys deviate by
  up to 10.4 units, all on 15° walls whose stored plane normal is (±0.25, 0, ∓0.968). The polys
  themselves are planar (Newell normal (0.259, 0, -0.966)), so the stored plane is a rounded normal
  from the source. Recompute planes when converting. Other levels show up to 4 units on a few polys.
* All UVs are finite (u range -430..110 repeats on sky/huge polys). Polys that share a point and a
  (merged) surface agree on its UV, up to an integer repeat, in only 43% of cases. UVs are per
  original face, so the converter must keep per-poly UV/OPQ and not weld UVs per point.

## 11. Unknown / undecoded
* Header dword 4 (= 13). BSP `unkDC` and `unkSize`. Poly `unk` f32 (0 / -1 values).
* Light record `unk[3]` (always 0). Object header dwords 2-4.
* Leaf `f32 unk` (thousands; a radius/size?). User-portal dword/word.
* Surface flag bits 0x200000 and 0x400000.
* Exact lightmap colour semantics (§8): the (L - d) decode is a well-supported hypothesis, not proven.
* PVS leaf-list compression: assumed identical to PC, not decoded here.
* PS2 lightmaps are lower resolution than the PC's (LM grid 20 nominal, many 1×1 rects). A converter
  must resample them into PC lightmap sizes (lmGridSize-based) or regenerate them.
