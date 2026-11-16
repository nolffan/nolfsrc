# NOLF models: PS2 `.ltb` (file type 2, version 16) and PC `.abc` (version 12)

Implemented by `ps2model.py` (PS2 reader, converter, `thief` batch) and `abcmodel.py`
(PC reader/writer). Both readers check every section boundary: the PS2 reader asserts each
section ends exactly at the next header offset and the last at the file size (all 17 PS2
models the thief levels use, plus hero_action, player_action, playerbase, fembase,
lighter_pv/_hh, codedec_pv, scope, purses_01 and overcoatagent_action_hat from the PS2
archive); `abcmodel.py check` re-writes all 692 v12 `.abc` files in the PC archives
byte-identically (header counts included).

All little-endian. `str16` = u16 length + bytes, no NUL. `vec3` = 3×f32, `quat` = x,y,z,w f32.
Matrices are 4×4 f32 row-major with the translation in column 3 (`m[0][3], m[1][3], m[2][3]`),
the same on both platforms.

## PS2 `.ltb` v16

### Header (0x00)

| off | type | field |
|---|---|---|
| 0x00 | u32 | file type = 2 (world `.ltb`s are other types) |
| 0x04 | u16 | version = 16 |
| 0x06 | u16, 3×u32 | 0 |
| 0x14 | u32 | 0x10 (offset of the offsets) |
| 0x18 | u32 | offset of the piece section |
| 0x1c | u32 | offset of the node section |
| 0x20 | u32 | offset of the child-model section |
| 0x24 | u32 | offset of the animation section |
| 0x28 | u32 | offset of the socket section |
| 0x2c | u32 | file size |
| 0x30 | u32 | 0x10 |
| 0x34 | 14×u32 | counts: keyframes, animations, nodes, pieces, child models (incl. self), triangles, vertices, weights, LODs, sockets, weight sets, strings, string bytes, 0 |
| 0x6c | str16 | command string (`""` literally when empty) |
| | f32 | internal radius |
| | u32 | **hash multiplier** for this file's hashed names (1, 2, 4, 9 seen) |
| | u32 | 0 or 1, unknown (not carried over) |
| | u32 | LOD-distance count (= LODs − 1) |
| | f32[n] | LOD distances |

Hashed names: `h = 0; for c in name.upper() ('/'→'\\'): h = h + c + h*mult` (u32), the
REZ directory's hash (`ps2rez.rez_hash`). Piece, animation and weight-set names are only
hashes; node names and child-model names are plain strings. Sockets carry two hashes.

### Pieces (at the piece offset)

`u32 count`, then per piece:

| size | field |
|---|---|
| u32 | name hash |
| 12×f32 | three RGBA colours (0.8,0.8,0.8,1 / 0.5,0.5,0.5,0 / 0,0,0,1 everywhere) |
| u32 | texture (skin) index = PC material index |
| 2×u32 | 0 |
| u32 | 4 (always) |
| LOD × count | see below |

Per LOD:

| size | field |
|---|---|
| u32 | mesh type: 4 rigid, 5 skeletal (6 vertex-animated: not seen) |
| skeletal: u32 | qwords in the DMA chain below |
| skeletal: u32 | u16 entries in the weight map |
| u32 | rigid: qwords in the DMA chain + 1; skeletal: unique-vertex count |
| u32 | rigid: the node the piece hangs on; skeletal: bone-map length |
| DMA chain | 16-byte DMA tags (qwc = low 16 bits, id = bits 28-30), each followed by qwc qwords; tags with id 1 (CNT) continue, the chain ends after an id 6 (RET) tag's data |

The VIF code stream starts in the upper 8 bytes of each tag: NOPs, `FLUSH`, then
`UNPACK V4-32` (cmd 0x6c, num = qwords, imm = VU address; data follows inline) and
`MSCALF` (0x15) to run the VU program. Each MSCALF buffer (contiguous VU memory from
address 80) is:

- header qword: u32 strips − 1, u32 vertices, 0, 0
- per strip: a GIF tag (NLOOP = vertex count, EOP on the last strip, PRIM = 0x5c triangle
  strip/Gouraud/textured/alpha, 3 registers ST,RGBAQ,XYZF2; REGS = 0x412, or 0x8412 when the
  strip's first triangle is reversed; upper u32 small integer, unknown) and NLOOP vertices of
  3 qwords: `x y z 1.0`, `nx ny nz 0`, `s t q(=1.0) u32`.

Strip triangle *k* (k ≥ 2) is (k−2, k−1, k), swapped for odd *k*, and swapped once more for
a 0x8412 strip; the result is PC (clockwise) order. No degenerate triangles occur; the
triangle count equals the header's.

Rigid vertices are in the space of their node (= PC weight location). Skeletal vertices are
in model space (bind pose); after the chain come:

| size | field |
|---|---|
| u16 × entries, padded to 16 | weight map: per unique vertex, `u16 n` + n qword offsets (from the chain's first tag) of the strip vertices that are copies of it (their position qwords) |
| 32 × unique | unique vertices: `x y z 1`, `nx ny nz 0` (model space) |
| 4 × bone-map length | bone map: u32 node index per local bone |
| 12 × unique | weights: 4×s16 (4096 = 1.0), 4×u8 local bone × 4 (0xff = unused) |

### Nodes (at the node offset)

Per node: `str16 name`, 16×f32 bind matrix (model space), u32 name hash, u32 child count,
u16 index, u16 flags (= PC node flags; PC sets 1 on the root). Then `u32 weight-set count`,
per set: u32 name hash, u32 n, f32[n].

### Child models

`u32 count` (incl. the model itself), then count−1 × `str16` file name (`playerbase.ltb`,
`base_general.ltb`, ...).

### Animations

`u32 count`, then per animation: vec3 dims, vec3 translation (the PC anim binding's origin),
u32 name hash, u32 interpolation ms, u32 keyframe count, keyframes (`u32 time ms, str16
command`), then per node: `u32 0xffffffff` and per keyframe 16 bytes: `s16 x,y,z, s16 flag,
s16 qx,qy,qz,qw` — position / 16 when flag ≠ 0, else / 4096; quaternion / 16384; parent-
relative, as PC. PS2 keeps fewer keyframes than PC for the same animation.

### Sockets

`u32 count`, then 48 bytes each: quat rotation, vec3 position, u32 0, u32 node, u32 name
hash (file multiplier), u32 name hash (multiplier 1), u32 0.

## PC `.abc` v12

A chain of sections: `str16 name`, `u32 offset of the next section` (absolute;
0xffffffff on the last), data. Order: Header, Pieces, Nodes, ChildModels, Animation,
Sockets, AnimBindings.

**Header**: u32 version 12; u32 × 13 counts: keyframes (sum over animations),
animations, nodes, pieces, child models (incl. self), faces, vertices, weights (all LODs),
LODs (≥ 1), sockets, weight sets, strings, string bytes; `str16` command string; f32
radius; u32 LOD-distance count (the engine reads u16 + 62 skipped bytes); 60 zero bytes;
f32 LOD distances. The engine sizes one allocation from the 13 counts. Strings = the
distinct non-empty (case-sensitive) command string, node names, animation names, keyframe
commands and child-model names; string bytes = their total length (this reproduces every
shipped file).

**Pieces**: u32 weight count, u32 piece count; per piece: u16 material, f32 specular
power (5.0), f32 specular scale (1.0), f32 LOD weight (1.0), u16 0, `str16` name, then per
LOD: `u32 nfaces`, faces (3 × {f32 u, f32 v, u16 vertex}), `u32 nverts`, vertices
{u16 nweights, u16 sub-LOD index (0xcdcd when there is one LOD), weights {u32 node, vec3
position in the node's space **pre-multiplied by the bias** (= bias × inverse(bind) × model
position; exact in every shipped file; the engine skins with Σ M_i·(pos_i, bias_i) / Σ bias_i),
f32 bias}, vec3 position
(model space), vec3 normal (model space)}.

**Nodes**: per node `str16 name, u16 index, u8 flags, 16×f32 bind matrix, u32 child
count` (depth-first order); then `u32 weight-set count`, per set `str16 name, u32 n,
f32[n]`.

**ChildModels**: `u16 count` (incl. self), then per child `str16 name` (empty for self),
u32 build number, per node vec3 + quat (all identity in every shipped file). The engine
reads the count as u32 (swallowing self's empty name), loads each named child from the
parent's own directory, and refuses a child that has children of its own.

**Animation**: `u32 count`; per animation vec3 dims, `str16` name, i32 −1, u32
interpolation ms, u32 keyframe count, keyframes {u32 time, `str16` command}, then per node
per keyframe vec3 position + quat rotation (parent-relative).

**Sockets**: `u32 count`; per socket u32 node, `str16` name, quat, vec3.

**AnimBindings**: for the model, then for each child in order: `u32 n`, n × {`str16`
animation name, vec3 dims, vec3 origin} — for a child, a copy of that child file's own
bindings (the engine sets each child animation's origin from it).

## Conversion (ps2model.py)

Nodes, sockets, weight sets, animations, LOD distances and radius copy across. The command
string too, except FovXOffset/FovYOffset, which move by (−11°, +9°): every PS2 first-person
model at 31/41 but bucklezip is 20/50 on the PC, with the same geometry and weapon Pos. Rigid LODs: strip vertices de-duplicated by (position, normal) — the PC files
have the same vertex counts; model-space position/normal = bind × node-space. Skeletal
LODs: PC vertex *i* = unique vertex *i*, strip vertices mapped to it through the weight
map; weights normalised to sum 1. Names: `ps2model.collect_names` (every name in the
PC `.abc` files, identifiers in the PS2 executable and the PS2 attribute files; collected
at install time, never committed),
plus the model's own node names; unresolved hashes become `h%08x`. Checked against PC
twins of PS2 models (overcoatagent_action_hat, purses_01, lighter_pv, lighter_hh): same
faces with the same orientation and UVs, same vertices, normals and node-space weight
positions; animations agree with the PC keyframes they share to within the PS2
quantisation (1/16 unit, 1/16384).

Child models: the PS2 splits the human animation set into `base_general/_pistol/_rifle/
_death.ltb`; the PC has `basemodel.abc` (+ `fembase.abc`, `playerbase.abc`) holding the
same animations under the same names. `hero_thief.abc` references the PC player's three
children (PC `player_action.abc` does the same); the PS2 animation-only children are also
converted (`BASE_*.abc`, no pieces) but not referenced.
