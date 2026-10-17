// *********************************************************************** //
//
//  MODULE:   BIG16324 (.CPP)
//
//  PURPOSE:  0x10016324 .. 0x10017c05 -- 6,384 bytes, 37 direct call sites
//            and 1 indirect one, TWO callers (0x10017e2f and 0x10019445,
//            both inside the same model-drawing function).  Reconstructed
//            instruction by instruction from the retail d3d.ren under the
//            "structurally right, not byte-exact" acceptance bar: same
//            control flow, same callees in the same order, same field
//            offsets, same constants.
//
//  (The row quoted "69 call sites, 93 bytes per call".  The
//  image does not agree: 0x10016324..0x10017c14 holds 38 calls, one of which is the indirect COM
//  call at 0x10017b1b.  The true ratio is 168 bytes per call, which puts
//  this row in the WEAK band, not the strong one.  What actually
//  pins the body is stated under EVIDENCE below and it is not the call
//  sequence.)
//
//  WHAT IT IS.  It draws ONE Y-AXIS-LOCKED BILLBOARD QUAD for a model --
//  four vertices built around `this->m_v410`, textured, lit out of the
//  light grid, run through the whole view frustum clipper and handed to
//  IDirect3DDevice7::DrawPrimitive as a TRIANGLEFAN.
//
//  Three things settle that:
//
//    * the corner algebra at 0x100163df .. 0x100167ff is
//      `P -/+ vRight*w -/+ vUp*h` with vUp the literal (0,1,0) built at
//      0x1001634e and vRight = normalise(((P - g_v1008bd34) x vUp) with .y
//      forced to 0) -- a billboard that yaws to face g_v1008bd34 and never
//      pitches;
//    * the texture coordinates at 0x10016804 .. 0x10016897 are
//      (u,v) (1-u,v) (1-u,1-v) (u,1-v) with u,v out of the pair at
//      0x1008e664 -- one whole texture tile, in fan order;
//    * the tail at 0x10017ae2 is `DrawPrimitive(D3DPT_TRIANGLEFAN, 0x1c4,
//      pVerts, nVerts, 0)`; 0x1c4 is XYZRHW|DIFFUSE|SPECULAR|TEX1, which is
//      the 32-byte vertex big4b720.cpp already anchors, and 32 is exactly
//      the stride every walk in this function uses.
//
//    The second caller passes a literal 0xff for the alpha byte, the first
//    computes `0xff - fade`; both sit next to the string
//    "Model::SetupTransforms failed for %s.", so `this` is a model
//    renderer.
//
//  Coin-flip (5) below settled -- the four-vertex
//  transform is the pointer walk with a `while (n--)` count, which gives
//  the shipped `lea aVerts,%ebx ; mov $4,%esi ; mov $0x1008b880,%edi ;
//  push ; push ; call ; add $0x20,%ebx ; dec ; jne` (normalised
//  instruction diff 95 -> 81).  Coin-flip (3) as `nClip = 0x3f;
//  g_1008b874 = nClip;` changes nothing (the dead `mov $0x3f,%eax` is
//  still missing).  `pV = &(*ppVerts)[0]` in the six plane bodies
//  is WORSE here (95 -> 119; each plane alone 84..90): this driver's
//  pVerts is not kept in memory as 0x100525b0's is.  Plane 1's
//  `pOutStart = *ppOut` read before its classify loop gives 82 (the
//  shipped plane-1 pOutStart is forwarded from the driver's pOut).  That
//  order breaks w1_10002310.cpp's byte-identical out-of-line copy of the
//  same body, but it is taken here, inlined: it is what the shipped
//  driver does (one load of g_1008b87c stored to -0x40 and -0x10), and
//  w30_1000f020_osqipni.cpp's inlined near plane needs the same order.
//  With it, the volatile read of g_p1008beac at the light-grid call (as
//  w63_10060010_qip.cpp reads it) and the +0x14 dword chained
//  [0] = [3] = [2] = [1]: 1778 of 1790 instructions matched.
//
//  WHICH FLAG -- /Os (the readings below are superseded).
//    The shipped prologue is `sub $0x870 ; push %edi ; push %edi ;
//    push %esi ; push %ebx` -- /Os pushes.  At /Os, with the clipper
//    written as the real inline calls (0x1000f4b0 over the six plane
//    bodies, below) and RoundF on the thirty conversions, the frame is
//    0x870 exactly and the body 6357 bytes against 6384 (it was 9136 at
//    /O2): the row pairs as a near.
//  The old reading, /O2, on four independent readings:
//    * outgoing arguments are `add $-N,%esp` then `mov` (0x1001636b,
//      0x10016415, 0x100168bb ...) -- the /O2 pattern;
//      /Os would push.  The only `push`es in the function are the two
//      inside the two __cdecl transform loops, where the argument area is
//      one word per argument anyway.
//    * `&p->m` comes out as `mov $0x4,%edi ; add %esp,%edi` (0x100163bd,
//      0x10016465, 0x100164db ...), the /O2 spelling of `lea 0x4(%esp),%edi`.
//    * the six epsilon "else" arms are moved out of line PAST the epilogue,
//      to 0x10017b27, 0x10017b4a, 0x10017b6f, 0x10017b94, 0x10017bb9 and
//      0x10017bde -- /O2 cold-block placement.  /Os cross-jumps instead.
//    * the boolean materialisation in all six classify loops is
//      `mov $0x1,%esi ; mov $0x0,%eax` (0x10016aa2/0x10016aab and five
//      more), not `xor`/`inc`.
//
//  THE FRAME.  `sub $0x870` -- 2,160 bytes -- and it decomposes exactly,
//  which is how the local set below was fixed rather than guessed:
//
//      -0x870 .. -0x851   32 B   alignment
//      -0x850 .. -0x781  112 B   6 x 5 float conversion temps, 8 B apiece
//                                (-0x850, -0x828, -0x800, -0x7d8, -0x7b0,
//                                 -0x788 are the six group bases)
//      -0x764 .. -0x6ed  120 B   10 LTVec slots -- vUp (-0x764) and
//                                vRight (-0x740) are named, the other eight
//                                are return-value temps
//      -0x6ec .. -0x66d  128 B   aVerts[4], stride 0x20
//      -0x66c .. -0x595  216 B   18 more LTVec return-value temps, all
//                                anonymous (none is ever copied out of)
//      -0x594 .. -0x591    4 B   the three light-grid bytes
//      -0x590, -0x58c      8 B   pVerts, nVerts
//      -0x588 .. -0x061 1320 B   6 x (int32 bInside[50] + 5 int temps)
//                                = 6 x 220; the group bases are -0x588,
//                                -0x4ac, -0x3d0, -0x2f4, -0x218, -0x13c and
//                                each one's five int temps start exactly
//                                200 bytes in (-0x4c0, -0x3e4, -0x308,
//                                -0x22c, -0x150, -0x074)
//      -0x060 .. -0x009   88 B   the shared fildl scratch at -0x60 and the
//                                six spilled interpolants at -0x38, -0x30,
//                                -0x28, -0x20, -0x18, -0x10
//
//  Six of everything, because the WHOLE FRUSTUM CLIPPER IS INLINED HERE:
//  0x1000f4b0's driver and all six of its plane bodies.
//
//  EVIDENCE, region by region -- and the honest part first.
//
//  0x10016a2f .. 0x10017aba is 4,236 bytes with NOT ONE CALL IN IT: 66% of
//  the function.  The call-sequence comparison is blind to all of it.  What pins it instead is
//  that it is not new code.  It is 0x1000f4b0 (decoded in full in
//  src/engine/d3d/d3dr3c.cpp) with its six callees inlined, and those six
//  callees are the 32-byte-vertex half of the twelve-body clip family whose
//  40-byte half is decoded in src/engine/d3d/d3dr3a.cpp -- 0x10002d40 and
//  0x100030a0 are written out there instruction by instruction.  Every one
//  of the six blocks below was diffed against those two decodes and against
//  the shipped bytes here, and the only differences are (a) the vertex is
//  32 bytes not 40, so +0x20 and +0x24 are not interpolated and the stride
//  is `add $0x20`/`shl $0x5` not 0x28, and (b) the plane, listed per block.
//  So the region is cross-checked against two independent already-decoded
//  functions, which is stronger evidence than a call sequence would be --
//  but it IS a 4.2 KB call-free region and that is said here.
//
//  The six plane tests, each read off its own classify loop, in the order
//  the inlined driver applies them (which is 0x1000f4b0's order, not the
//  numeric order of the bits -- the far plane's 0x02 comes last):
//
//      bit 0x01  near    z  >= g_1008b908     0x10016a97  fcomps/jae
//      bit 0x04  left    -z <  x              0x10016d52  fchs/fcomps/jb
//      bit 0x08  top     y  <  z              0x10017018  fcomps/jb
//      bit 0x10  right   x  <  z              0x100172d6  fcomps/jb
//      bit 0x20  bottom  -z <  y              0x10017594  fchs/fcomps/jb
//      bit 0x02  far     z  <= g_1008b910     0x10017853  fcomps/jbe
//
//  and they agree, plane for plane, with the table d3dr3c.cpp derived from
//  0x1000f4b0's six guarded call sites.  `and $0x1,%ebx` at 0x10016a40
//  MUTATES the flag word exactly as the out-of-line driver does.
//
//  The remaining regions, which the call sequence does cover:
//
//    0x10016324 .. 0x1001640f  the basis.  vUp is the literal (0,1,0)
//      (three stack floats 0/1.0f/0 into 0x10015480).  vRight is
//      `(m_v410 - g_v1008bd34).Cross(vUp)` COPIED to a named local
//      (`rep movsl` at 0x100163f6 -- the return-value temp at -0x74c is
//      copied to -0x740, so -0x740 is a variable and -0x74c is not), then
//      `.y = 0.0f` (0x10016406 stores a literal zero to -0x73c, which is
//      -0x740+4) and then Norm(1.0f).
//
//    0x10016415 .. 0x100167ff  the four corners, sixteen operator calls in
//      four groups of four.  Every group evaluates the full left operand of
//      the outer +/- before the right, and no subexpression is shared
//      between groups -- `vRight * m_c4 * m_ac` is recomputed four times in
//      the shipped bytes, so it is spelled four times here.  The four
//      signs, read off which of 0x100155f0 (+) and 0x10015640 (-) each
//      group calls:
//          aVerts[0]  P - R + U      0x10016488 (-) then 0x100164f8 (+)
//          aVerts[1]  P + R + U      0x10016583 (+) then 0x100165f3 (+)
//          aVerts[2]  P + R - U      0x1001667e (+) then 0x100166ee (-)
//          aVerts[3]  P - R - U      0x10016779 (-) then 0x100167e9 (-)
//
//    0x10016804 .. 0x10016897  the texture coordinates, four calls to
//      0x1000d9d0 (the +0x18/+0x1c setter d3dbatchb.cpp decoded).  The four
//      1.0f constants are 0x100a4b28, 0x100a4b2c, 0x100a4b30, 0x100a4b34,
//      all read out of the image.  0x1008e664 and 0x1008e668 move with
//      integer `mov`s where they are not arithmetic operands and with
//      `fsubs` where they are, which is the float-lvalue rule.
//
//    0x1001689c .. 0x100169f4  the colour.  0x1008beac is loaded THREE
//      times (0x100168a9, 0x100168b6 and the guard), which d3dr3b.cpp
//      already observed for this global and which is what makes the guard a
//      two-test `&&` rather than one cached pointer.  The three ramps are
//      d3dhigh.cpp's: 0x1008d1bc -> +0x12, 0x1008d2bc -> +0x11,
//      0x1008d3bc -> +0x10, and the else arm reads index 255 of the same
//      three (0x1008d2bb = 0x1008d1bc+0xff, and so on).
//
//    0x100169f6 .. 0x10016a2e  g_1008b874 = 0x3f, then aVerts/4 into the
//      two clipper locals, then four calls to 0x1000d9b0 against the fixed
//      matrix global 0x1008b880.
//
//    0x10017abb .. 0x10017b24  nVerts calls to 0x10014580 against the same
//      0x1008b880 -- this is sub_1005fa80 (d3dr2a.cpp) inlined, same guard,
//      same `dec`/`jne`, same 0x20 stride -- then DrawPrimitive and the
//      single shared epilogue at 0x10017b1e that all seven early exits
//      reach.
//
//  COIN-FLIPS are listed at the bottom of the file, each with both exact
//  spellings.
//
// *********************************************************************** //

#include "ltbasedefs.h"

// Round to nearest in the current FPU mode (w1_10001000.cpp's helper;
// 0x10015190 out of line): icl inlines a function holding __asm as
// `fstps f ; flds f ; fistpl n` -- the thirty bare `fistpl` sites.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}


// ======================================================================= //
//  THE VECTOR
//
//  src/LT2/sdk/inc/ltvector.h's _CVector<float>, spelled out locally so the
//  out-of-line members can carry their addresses in their names.  Every one
//  is DECLARED AND NOT DEFINED: the shipped function CALLS all six of them,
//  so this translation unit must not be able to inline any.  The set and
//  the address of each is big48180.cpp's, unchanged.
// ======================================================================= //
struct LTVec
{
	LTVec* sub_10015480(float mx, float my, float mz);   // (x,y,z) ctor
	void   sub_10015530(float nVal);                     // Norm
	LTVec  sub_10015590(const LTVec v) const;            // Cross
	LTVec  sub_100155f0(const LTVec v) const;            // operator +
	LTVec  sub_10015640(const LTVec v) const;            // operator -
	LTVec  sub_10015690(float v) const;                  // operator * (T)

	float x;                                             // +0x00
	float y;                                             // +0x04
	float z;                                             // +0x08
};


// ======================================================================= //
//  THE VERTEX -- 32 bytes.
//
//  Identical to d3dr3c.cpp's ClipVert, which was pinned from the other side
//  by 0x1000f4b0.  Here every offset is a store or a load in these bytes:
//  +0x00..+0x08 the position (12-byte `rep movsl` at 0x100164fd), +0x10..
//  +0x13 the four diffuse bytes, +0x14 a whole dword written at 0x100169d8,
//  +0x17 the one specular byte the clipper interpolates, +0x18/+0x1c the
//  texture coordinates 0x1000d9d0 writes.  +0x0c is never touched by this
//  function; 0x10014580 fills it.
// ======================================================================= //
struct RVert
{
	// 0x1000d9d0 -- the two-field setter d3dbatchb.cpp decoded, whose
	// arguments move with integer `mov`s because they are float lvalues.
	void sub_1000d9d0(float fU, float fV);

	LTVec  m_vPos;                                       // +0x00
	float  m_fRHW;                                       // +0x0c
	uint8  m_b;                                          // +0x10
	uint8  m_g;                                          // +0x11
	uint8  m_r;                                          // +0x12
	uint8  m_a;                                          // +0x13
	uint8  m_sb;                                         // +0x14
	uint8  m_sg;                                         // +0x15
	uint8  m_sr;                                         // +0x16
	uint8  m_sa;                                         // +0x17
	float  m_fU;                                         // +0x18
	float  m_fV;                                         // +0x1c
};


// ======================================================================= //
//  THE TWO OBJECTS THE ARGUMENT CHAIN REACHES
//
//  0x100158c0 is `mov 0x1dc(%ecx),%eax` -- a one-line getter, and
//  d3dbatchc.cpp already identified it as the sibling of 0x100158b0.  Its
//  answer is dereferenced at +0xc0 (a pointer, null-tested), +0xc4 and
//  +0xc8 (two floats used as the quad's half-width and half-height).  The
//  object it is called on is dereferenced at +0xac and +0xb0, two more
//  floats, so the two are different types and only one of them is `this`
//  for 0x100158c0.
// ======================================================================= //
struct SDesc1dc
{
	uint8   m_pad_00[0xc0];                              // +0x00 .. +0xbf
	void*   m_p00c0;                                     // +0xc0
	float   m_f00c4;                                     // +0xc4
	float   m_f00c8;                                     // +0xc8
};

struct SModel
{
	SDesc1dc* sub_100158c0() const;                      // 0x100158c0

	uint8     m_pad_00[0xac];                            // +0x00 .. +0xab
	float     m_f00ac;                                   // +0xac
	float     m_f00b0;                                   // +0xb0
	uint8     m_pad_b4[0x128];                           // +0xb4 .. +0x1db
	SDesc1dc* m_p01dc;                                   // +0x1dc
};


// ======================================================================= //
//  THE DIRECT3D 7 DEVICE AT 0x1008d988 -- the slice big4b720.cpp models,
//  cut down to the one slot this function uses.  Index = offset/4 against
//  IDirect3DDevice7, anchored by the project's +0x50 SetRenderState (20)
//  and +0x8c SetTexture (35); +0x64 is 25, DrawPrimitive.
// ======================================================================= //
struct D3DDevice7;

struct D3DDevice7Vtbl
{
	void*   m_pad_00[25];                                // +0x00 .. +0x60
	long (__stdcall* DrawPrimitive)(D3DDevice7*, uint32 nType, uint32 nFVF,
	                                void* pVerts, uint32 nCount,
	                                uint32 nFlags);      // +0x64
};

struct D3DDevice7
{
	D3DDevice7Vtbl* lpVtbl;
};

#define D3DPT_TRIANGLEFAN                     6
#define D3DFVF_CLIPVERTEX                 0x1c4  // XYZRHW|DIFFUSE|SPECULAR|TEX1


// ======================================================================= //
//  GLOBALS -- every one of them is an address this function touches.
// ======================================================================= //

extern D3DDevice7* g_pD3DDevice;                         // 0x1008d988

// 0x1008bd34 -- the point the billboard yaws to face.  d3dr3b.cpp reaches
// the same global as a position (`g_vPoint1008bd34`).
extern LTVec       g_v1008bd34;                          // 0x1008bd34

// 0x1008e664 / 0x1008e668 -- the base texture coordinate pair.  Read as two
// independent scalars here; d3dr2c_os.cpp sees the same address as a table.
extern float       g_f1008e664;                          // 0x1008e664
extern float       g_f1008e668;                          // 0x1008e668

// The clipper's state, all four already named by d3dr3a/d3dr3c.
extern uint32      g_1008b874;                           // clip flags
extern RVert*      g_1008b87c;                           // scratch cursor
extern uint32      g_1008d7a0;                           // "near plane only"
extern uint32      g_1008be44;                           // clipped-poly count
extern float       g_1008b908;                           // near plane
extern float       g_1008b910;                           // far plane

// 0x1008b880 -- the fixed transform global d3dlow.cpp and d3dr2a.cpp both
// see pushed as the second argument of this pair of helpers.
extern uint8       g_1008b880[];                         // 0x1008b880

// The light-grid gate and the light-grid object.
extern uint32      g_1008b054;                           // 0x1008b054
extern uint8*      g_p1008beac;                          // 0x1008beac

// d3dhigh.cpp's three 256-entry byte ramps.
extern uint8       g_ColorRamp2[256];                    // 0x1008d1bc
extern uint8       g_ColorRamp1[256];                    // 0x1008d2bc
extern uint8       g_ColorRamp0[256];                    // 0x1008d3bc

// 0x1008d7d0 -- d3dqip2.cpp/d3dqip3.cpp's "current texture stage".
extern uint32      g_1008d7d0;                           // 0x1008d7d0


// 0x10015320 -- d3dbatcha.cpp decoded it: a lookup-or-create over the list
// hung off argument0+0x0c, keyed on the low byte of argument 1.  Binding a
// texture to a stage.
extern void sub_10015320(void* pTex, uint32 nStage, uint32 nFlag);

// 0x10014b60 -- three arguments, __cdecl (the caller unwinds 0x10 at
// 0x100168ea).  Its own body subtracts argument0+0x3c from *argument1 with
// 0x10015640 and scales by the floats at argument0+0x30/+0x34, then writes
// three bytes through argument2: sample the light grid at a point.
extern void sub_10014b60(void* pGrid, LTVec* pPos, uint8* pOutRGB);

// 0x1000d9b0 and 0x10014580 -- the two transform helpers, both __cdecl,
// both taking (element, 0x1008b880).
extern void sub_1000d9b0(LTVec* pVec, uint8* pXForm);
extern void sub_10014580(void* pElem, void* pXForm);


// ======================================================================= //
//  `this`
//
//  Only two members are reached: an LTVec at +0x410 (used as the `this` of
//  three vector members and as the second argument of 0x10014b60) and a
//  dword at +0x480 that is copied straight into every vertex's +0x14.
// ======================================================================= //
// ======================================================================= //
//  THE CLIPPER -- 0x1000f4b0 and its six plane bodies, which the shipped
//  function INLINES (0x10016a2f .. 0x10017aba).  The bodies are the ones
//  w1_10001000.cpp / w44_10001670.cpp compile byte-identical out of line,
//  on this file's 32-byte RVert; the driver is d3dr3c_os.cpp's 0x1000f4b0.
//  Inlined, the driver's pVerts / nVerts keep their own frame slots
//  (-0x590 / -0x58c), stored and reloaded around every plane, because
//  their addresses are handed to the plane bodies.
// ======================================================================= //
#define CLIP_EPSILON    0.00001f
#define CLIP_MAX_VERTS  50

static inline void LerpRVert(RVert* a, RVert* b, RVert* dst, float t)
{
	dst->m_fU = (b->m_fU - a->m_fU) * t + a->m_fU;
	dst->m_fV = (b->m_fV - a->m_fV) * t + a->m_fV;
	dst->m_r  = (uint8)RoundF((b->m_r  - a->m_r)  * t + a->m_r);
	dst->m_g  = (uint8)RoundF((b->m_g  - a->m_g)  * t + a->m_g);
	dst->m_b  = (uint8)RoundF((b->m_b  - a->m_b)  * t + a->m_b);
	dst->m_a  = (uint8)RoundF((b->m_a  - a->m_a)  * t + a->m_a);
	dst->m_sa = (uint8)RoundF((b->m_sa - a->m_sa) * t + a->m_sa);
}


static inline float ClipEdgeNear(LTVec* a, LTVec* b, LTVec* dst)
{
	float fDenom = b->z - a->z;
	float t;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = (g_1008b908 - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	dst->y = (b->y - a->y) * t + a->y;
	dst->z = g_1008b908;

	return t;
}


static inline float ClipEdgeLeft(LTVec* a, LTVec* b, LTVec* dst)
{
	float fDenom = (b->x - a->x) + b->z - a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = -(a->x + a->z) / fDenom;
	else
		t = 0.0f;

	dst->y = (b->y - a->y) * t + a->y;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->x = -fZ;

	return t;
}


static inline float ClipEdgeTop(LTVec* a, LTVec* b, LTVec* dst)
{
	float fDenom = (b->y - a->y) - b->z + a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = -(a->y - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->y = fZ;

	return t;
}


static inline float ClipEdgeRight(LTVec* a, LTVec* b, LTVec* dst)
{
	float fDenom = (b->x - a->x) - b->z + a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = -(a->x - a->z) / fDenom;
	else
		t = 0.0f;

	dst->y = (b->y - a->y) * t + a->y;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->x = fZ;

	return t;
}


static inline float ClipEdgeBottom(LTVec* a, LTVec* b, LTVec* dst)
{
	float fDenom = (b->y - a->y) + b->z - a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = -(a->y + a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->y = -fZ;

	return t;
}


static inline float ClipEdgeFar(LTVec* a, LTVec* b, LTVec* dst)
{
	float fDenom = b->z - a->z;
	float t;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = (g_1008b910 - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	dst->y = (b->y - a->y) * t + a->y;
	dst->z = g_1008b910;

	return t;
}


// 0x10001000
static inline LTBOOL sub_10001000(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	pOutStart = *ppOut;
	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.z >= g_1008b908);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeNear(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// 0x10001330
static inline LTBOOL sub_10001330(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (-pV->m_vPos.z < pV->m_vPos.x);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeLeft(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// 0x10001670
static inline LTBOOL sub_10001670(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.y < pV->m_vPos.z);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeTop(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// 0x10001990
static inline LTBOOL sub_10001990(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.x < pV->m_vPos.z);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeRight(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// 0x10001cb0
static inline LTBOOL sub_10001cb0(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (-pV->m_vPos.z < pV->m_vPos.y);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeBottom(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// 0x10001fe0
static inline LTBOOL sub_10001fe0(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pV      = *ppVerts;
	pFlag   = anInside;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.z <= g_1008b910);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeFar(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// 0x1000f4b0 -- d3dr3c_os.cpp's driver.
static inline LTBOOL sub_1000f4b0(uint32 nClipFlags, RVert** ppVerts, int32* pnVerts)
{
	RVert*     pOut;
	RVert*     pVerts;
	int32      nVerts;
	uint32     anUnused[6];

	if (g_1008d7a0)
	{
		nClipFlags &= 1;
		if (!nClipFlags)
			return LTTRUE;
	}

	pOut   = g_1008b87c;
	pVerts = *ppVerts;
	nVerts = *pnVerts;

	if (nClipFlags & 0x01)
	{
		if (!sub_10001000(&anUnused[0], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x04)
	{
		if (!sub_10001330(&anUnused[1], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x08)
	{
		if (!sub_10001670(&anUnused[2], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x10)
	{
		if (!sub_10001990(&anUnused[3], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x20)
	{
		if (!sub_10001cb0(&anUnused[4], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (!(nClipFlags & 0x02) ||
	    sub_10001fe0(&anUnused[5], &pVerts, &nVerts, &pOut))
	{
		*ppVerts = pVerts;
		*pnVerts = nVerts;
		return LTTRUE;
	}

	return LTFALSE;
}


class CModelDraw
{
public:
	void sub_10016324(SModel* pModel, uint8 nAlpha);

	uint8  m_pad_000[0x410];                             // +0x000 .. +0x40f
	LTVec  m_v410;                                       // +0x410
	uint8  m_pad_41c[0x64];                              // +0x41c .. +0x47f
	uint32 m_n480;                                       // +0x480
};


// ----------------------------------------------------------------------- //
//  0x10016324
// ----------------------------------------------------------------------- //
void CModelDraw::sub_10016324(SModel* pModel, uint8 nAlpha)
{
	LTVec       vUp;
	LTVec       vRight;
	RVert       aVerts[4];
	uint8       nRGB[4];
	RVert*      pVerts;
	int32       nVerts;
	SDesc1dc*   pDesc;
	RVert*      pWalk;
	int32       nLeft;
	int32       n;

	// 0x1001634e -- (0.0f, 1.0f, 0.0f).
	vUp.sub_10015480(0.0f, 1.0f, 0.0f);

	// 0x10016356 / 0x1001635d -- the getter, then the null test on +0xc0
	// whose false arm is the trampoline at 0x10017c01 into the epilogue.
	pDesc = pModel->sub_100158c0();
	if (!pDesc->m_p00c0)
		return;

	// 0x10016383.
	sub_10015320(pDesc->m_p00c0, g_1008d7d0, 0);

	// 0x100163af .. 0x10016410.  The Cross result is COPIED into a named
	// local (rep movsl at 0x100163f6) before .y is zeroed and it is
	// normalised, which is what makes vRight a variable.
	vRight = m_v410.sub_10015640(g_v1008bd34).sub_10015590(vUp);
	vRight.y = 0.0f;
	vRight.sub_10015530(1.0f);

	// 0x10016415 .. 0x100167ff.  Four corners, no subexpression shared.
	aVerts[0].m_vPos =
		m_v410.sub_10015640(
			vRight.sub_10015690(pDesc->m_f00c4).sub_10015690(pModel->m_f00ac))
		.sub_100155f0(
			vUp.sub_10015690(pDesc->m_f00c8).sub_10015690(pModel->m_f00b0));

	aVerts[1].m_vPos =
		m_v410.sub_100155f0(
			vRight.sub_10015690(pDesc->m_f00c4).sub_10015690(pModel->m_f00ac))
		.sub_100155f0(
			vUp.sub_10015690(pDesc->m_f00c8).sub_10015690(pModel->m_f00b0));

	aVerts[2].m_vPos =
		m_v410.sub_100155f0(
			vRight.sub_10015690(pDesc->m_f00c4).sub_10015690(pModel->m_f00ac))
		.sub_10015640(
			vUp.sub_10015690(pDesc->m_f00c8).sub_10015690(pModel->m_f00b0));

	aVerts[3].m_vPos =
		m_v410.sub_10015640(
			vRight.sub_10015690(pDesc->m_f00c4).sub_10015690(pModel->m_f00ac))
		.sub_10015640(
			vUp.sub_10015690(pDesc->m_f00c8).sub_10015690(pModel->m_f00b0));

	// 0x10016804 .. 0x10016897.  One tile, fan order.
	aVerts[0].sub_1000d9d0(g_f1008e664,        g_f1008e668);
	aVerts[1].sub_1000d9d0(1.0f - g_f1008e664, g_f1008e668);
	aVerts[2].sub_1000d9d0(1.0f - g_f1008e664, 1.0f - g_f1008e668);
	aVerts[3].sub_1000d9d0(g_f1008e664,        1.0f - g_f1008e668);

	// 0x1001689c .. 0x100169ac.
	if (g_1008b054 && g_p1008beac)
	{
		sub_10014b60(*(uint8* volatile*)&g_p1008beac + 0xfc, &m_v410, nRGB);

		aVerts[0].m_r = aVerts[1].m_r = aVerts[2].m_r = aVerts[3].m_r =
			g_ColorRamp2[nRGB[2]];
		aVerts[0].m_g = aVerts[1].m_g = aVerts[2].m_g = aVerts[3].m_g =
			g_ColorRamp1[nRGB[1]];
		aVerts[0].m_b = aVerts[1].m_b = aVerts[2].m_b = aVerts[3].m_b =
			g_ColorRamp0[nRGB[0]];
	}
	else
	{
		aVerts[0].m_r = aVerts[1].m_r = aVerts[2].m_r = aVerts[3].m_r =
			g_ColorRamp2[255];
		aVerts[0].m_g = aVerts[1].m_g = aVerts[2].m_g = aVerts[3].m_g =
			g_ColorRamp1[255];
		aVerts[0].m_b = aVerts[1].m_b = aVerts[2].m_b = aVerts[3].m_b =
			g_ColorRamp0[255];
	}

	// 0x100169ae -- the argument byte, saved to -0x10 at 0x100163f3 the
	// moment it was read and reloaded here.
	aVerts[0].m_a = aVerts[1].m_a = aVerts[2].m_a = aVerts[3].m_a = nAlpha;

	// 0x100169cc -- one dword of `this` into every vertex's +0x14.
	*(uint32*)&aVerts[0].m_sb = *(uint32*)&aVerts[3].m_sb =
	*(uint32*)&aVerts[2].m_sb = *(uint32*)&aVerts[1].m_sb = m_n480;

	// 0x100169f0 .. 0x10016a09.
	pVerts      = aVerts;
	g_1008b874  = 0x3f;
	nVerts      = 4;

	// 0x10016a0f -- four iterations, stride 0x20, no guard because the
	// count is a literal.
	pWalk = aVerts;
	n     = 4;

	while (n--)
	{
		sub_1000d9b0(&pWalk->m_vPos, g_1008b880);
		pWalk++;
	}


	// =================================================================== //
	//  0x10016a2f .. 0x10017aba -- 0x1000f4b0 INLINED, with all six of its
	//  plane bodies inlined too.  Call-free; see the header note on what
	//  pins it.  The shape of each block is d3dr3a.cpp's 0x10002d40 /
	//  0x100030a0 with the 32-byte vertex.
	// =================================================================== //

	if (!sub_1000f4b0(g_1008b874, &pVerts, &nVerts))
		return;

	// 0x10017abb -- sub_1005fa80 (d3dr2a.cpp) inlined: guard, `dec`/`jne`,
	// stride 0x20, the same 0x1008b880.
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		sub_10014580(pWalk, g_1008b880);
		pWalk++;
	}

	// 0x10017ae2 -- +0x64 of the device vtable, six arguments, nothing
	// unwound afterwards, straight into the shared epilogue.
	g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, D3DPT_TRIANGLEFAN,
	                                    D3DFVF_CLIPVERTEX, pVerts, nVerts, 0);
}


// *********************************************************************** //
//
//  COIN-FLIPS -- both spellings given, so one build settles each.
//
//  (1) THE CHAINED COLOUR ASSIGNMENTS.  0x100168f3 .. 0x100169ea writes
//      five values to four vertices each.  Four of the five groups store in
//      the order v1, v2, v3, v0 (r at 0x100168f3, g at 0x10016918, b at
//      0x1001693d, the +0x14 dword at 0x100169d8) and the fifth -- alpha,
//      0x100169b4 -- stores v3, v2, v1, v0.  A right-associative chain
//      `a = b = c = d = val` stores d, c, b, a, so the ALPHA order is
//      exactly what the file writes and the other four are one rotation
//      off.  Written above as
//
//          aVerts[0].m_r = aVerts[1].m_r = aVerts[2].m_r = aVerts[3].m_r = v;
//
//      for all five, taking the four odd ones as store scheduling between
//      independent stack slots.  If they come back wrong, the spelling that
//      reproduces those four literally is
//
//          aVerts[0].m_r = aVerts[3].m_r = aVerts[2].m_r = aVerts[1].m_r = v;
//
//      with the alpha and the +0x14 dword left as they are.  Try that one
//      and nothing else; the two differ only in store order.
//
//  (2) THE INTERPOLANT'S TYPE.  All six blocks spill fT with `fstpl` and
//      reload it with `fmull` -- 64 bits (0x10016b6a, 0x10016e2d,
//      0x100170f4, 0x100173af, 0x10017672, 0x10017922).  d3dr3a.cpp's
//      0x10002d40 has the same source with fT kept in %st and never stored,
//      so a spill at 64 bits proves nothing about the declared type and
//      `float fT;` is written here to match that file.  The alternative is
//      the single-word change
//
//          double  fDenom, fT, fZ;      /* instead of float */
//
//      in all six blocks.  It is worth one build because if fT really is a
//      double the multiply operand type changes throughout.
//
//  (3) THE DEAD 0x3f.  0x10016a0a materialises `mov $0x3f,%eax` and nothing
//      reads it: the clipper reloads the value from 0x1008b874 at
//      0x10016a2f.  Written as
//
//          g_1008b874 = 0x3f;  ...  nClipFlags = g_1008b874;
//
//      The other reading, which would explain the dead register, is
//
//          nClipFlags = 0x3f;  g_1008b874 = nClipFlags;
//
//      with `nClipFlags` reloaded from the global anyway because the four
//      intervening calls may write it.
//
//  (4) THE SPECULAR DWORD.  0x100169cc..0x100169ea writes one dword to
//      +0x14 while the clipper interpolates only the byte at +0x17, so the
//      vertex needs both views.  Spelled here as four bytes plus a
//      `*(uint32*)&aVerts[n].m_sb` cast.  The alternative, if icl objects
//      or the cast costs bytes, is a `uint32 m_nSpecular;` at +0x14 with
//      the clipper reaching its top byte as
//
//          ((uint8*)&pOut->m_nSpecular)[3]
//
//      Both are the same object code; only the source is at issue.
//
//  (5) [SETTLED: the second spelling, see the banner.]
//      THE FOUR-VERTEX TRANSFORM LOOP at 0x10016a1f has no guard because
//      its count is the literal 4.  Written as `for (n = 0; n < 4; n++)`.
//      If the shipped `dec`/`jne` with the pointer walk matters, the other
//      spelling is d3dr2a.cpp's, with the count in a variable:
//
//          pWalk = aVerts; n = 4;
//          while (n--) { sub_1000d9b0(&pWalk->m_vPos, g_1008b880); pWalk++; }
//
//  (6) THE CORNER EXPRESSIONS' EVALUATION ORDER -- the one coin-flip that
//      can actually move the CALL SEQUENCE, so it is the one to settle
//      first.  The shipped order inside each group is
//
//          *c4, *ac, +/-, *c8, *b0, +/-
//
//      i.e. the outer operator's LEFT operand is finished before its right
//      one is begun, and no result is ever copied to a named local (there
//      is no `rep movsl` between the six calls of a group -- the first copy
//      in each group is the one INTO aVerts[n], at 0x100164fd, 0x100165f8,
//      0x100166f3 and 0x100167ee).  That forces one chained expression, and
//      it is written as one above:
//
//          m_v410.sub_10015640(A).sub_100155f0(B)
//
//      If icl evaluates the outer argument B first, the call-sequence comparison will report the
//      four groups as `*c8, *b0, *c4, *ac, -, +` and the fix is to force
//      the order with the comma operator rather than with locals, because
//      locals would add four 12-byte copies the shipped code does not have:
//
//          aVerts[0].m_vPos = m_v410.sub_10015640(
//                  vRight.sub_10015690(pDesc->m_f00c4)
//                        .sub_10015690(pModel->m_f00ac)).sub_100155f0(
//                  vUp.sub_10015690(pDesc->m_f00c8)
//                     .sub_10015690(pModel->m_f00b0));
//
//      -- which is what is written; the alternative to test is the same
//      four statements with the RIGHT operand hoisted into a named LTVec
//      local, accepting the extra copy, to see which of the two matches the
//      order in the image.
//
// *********************************************************************** //
