// *********************************************************************** //
//
//  w23_10050e60_os.cpp -- d3d.ren, written for the link.
//
//  0x10050e60 .. 0x100525b0 (5,968 bytes): slot 1 of g_1008975c, the
//  per-stage-count clipper table DrawPolyMgr's pass drawers call
//  (w16_10056ab4_osqipni.cpp: `g_1008975c[pPass->m_nStages1c](clip bits,
//  &pVerts, &nVerts)`).  Slot 1 is the ONE-texture-stage vertex (0x20
//  bytes, FVF 0x1c4); slot 2, 0x100525b0 (w23_100525b0_os.cpp), is the
//  same source over the 0x28-byte two-stage vertex; slot 3 is
//  0x10056aa0 (d3dhigh.cpp, the refusal) and slot 0 is null.
//
//  THE EXTENT.  The enumeration's row says 192 bytes; it is not.  The
//  function has ONE prologue (0x10050e60, `sub $0x700`), ONE `ret $0x4`
//  (0x100524bd), and every branch target is inside 0x10050e60..0x100525b0:
//  its first branch already jumps to 0x100524b2.  The rows 0x10050f20
//  (740), 0x10051204 (732), 0x100514e0 (732), 0x100517bc (740) and
//  0x10051aa0 (2832) are enumerator artefacts INSIDE it -- 0x10050f20 is
//  the middle of `fmuls 0x1008ba7c` (0x10050f1e), 0x10051204 of
//  `mov %dl,0x11(%eax)` (0x10051203); no `e8`/`e9` in the image reaches
//  any of them (their "call" type is a false decode of float bytes).
//  0x100524c0..0x100525ab are the six sunk epsilon arms and the shared
//  `return 0` (`xor %edx,%edx ; jmp 0x100524b5`).
//
//  WHAT IT IS -- 0x1000a8a0 (w2_1000a8a0_os.cpp) for the 32-byte vertex
//  with its clipper INLINED:
//
//    * g_1008b874 = the caller's clip bits;
//    * no bits: every vertex through the transform object's projective
//      4x4 at 0x1008ba5c (g_1008b880 + 0x1dc), 1/w into +0x0c;
//    * otherwise every vertex to camera space by 0x1000d9b0, then
//      0x1000f4b0 (d3dr3c_os.cpp) inlined with all six of its plane
//      bodies (w1_10001000.cpp, d3dr3c.cpp's 0x10001670) inlined too --
//      the planes in the driver's order near 0x01, left 0x04, top 0x08,
//      right 0x10, bottom 0x20, far 0x02, each read off its classify
//      loop and its crossing stores here; a rejected polygon answers 0;
//    * then every surviving vertex through the screen projection
//      0x10014580.  Answer 1.
//
//  SIGNATURE.  The table's type, `long (__fastcall*)(unsigned long,
//  unsigned char**, long*)`, is what the bytes show: %ecx the bits
//  (stored to 0x1008b874), %edx the vertex-pointer pointer (spilled to
//  -0x6c and dereferenced), 0x8(%ebp) the count pointer, `ret $0x4`.
//
//  FLAGS.  /Os: the per-vertex calls push their arguments, the vertex
//  copy is `rep movsl` x8, TRUE is `xor %edx,%edx ; inc %edx`.  The byte
//  conversions are the inlined __asm RoundF (bare `fistpl`), as in
//  w1_10001000.cpp.
//
//  HOW THE SOURCE IS SHAPED (each measured: normalised instruction diff
//  against the shipped bytes, lines differing):
//    * the whole body is 0x1000a8a0's source as an inline over
//      (ppVerts, pnVerts, g_1008b880), called after the store to
//      0x1008b874 -- that is what keeps the clip bits in %eax across the
//      camera-space loop (the zero-trip path skips the global's re-read)
//      and stores the count pointer to -0x70 exactly as shipped; with the
//      body written straight into the function: 560 -> 545;
//    * the six planes as blocks over the driver's own pVerts / nVerts /
//      pOut (big124a4.cpp's spelling of the same inlined clipper) rather
//      than inline functions taking their addresses: 545 -> 466;
//    * the crossing stays an inline function returning t (as a macro of
//      statements: 466 -> 724, the frame slots reshuffle); t spilled as
//      a double (`fstpl` / `fmull`) is icl's own x87 spill, not a double
//      in the source (declaring it double: 560 -> 572).
//
//  OUTCOME: NEAR (5,930 of 5,968 bytes, same frame, same blocks in the
//  same order, the six sunk epsilon arms and the shared `return 0`).
//  What is left is the clip family's known residual (w1_10001000.cpp,
//  item 1): in every crossing shipped loads pPrev->z first and subtracts
//  with `fsub %st(1)`, keeping it for the numerator, and loads *ppOut
//  before the epsilon compare; ours folds the load into `fsubs`.  Tried
//  here too without effect: a local for a->z, `-(a->z - b->z)`,
//  `-a->z + b->z`, the numerator first, a pDst local; and the order of
//  the driver's three loads (+/-2 lines).
//
//  REWRITTEN OVER THE REAL BODIES.  The six planes are now
//  w1_10001000.cpp's / w44_10001670.cpp's byte-identical out-of-line
//  functions and the driver d3dr3c_os.cpp's byte-identical 0x1000f4b0,
//  copied here as statics and inlined (the planes taking &pVerts, &nVerts,
//  &pOut), and the whole-fan projection is ltmatrix.h's MatVMul_H.  The
//  body is now 5,968 bytes, the shipped length exactly, and the
//  normalised instruction match 0.840 -> 0.895; the clip flags move into
//  the spill area beside the two parameters, as shipped.  (The same
//  bodies took 0x100124a4, w30_1000f020_osqipni.cpp, from 0.78 to 0.96.)
//  STILL ATTEMPTED: the driver's pVerts.  The shipped code keeps it in a
//  frame slot (-0x80, reloaded by every plane); icl gives ours a register
//  and spills it around each classify loop to a temporary, and that extra
//  spill slot pushes the parameter spills four bytes up (`mov %edx,-0x68`
//  at +0xd where shipped has -0x6c), so the first 16 bytes do not pair.
//  Measured without effect on that: the driver's locals reordered, the
//  file under /Os /Qip, /Os /Qip /Qip_no_inlining and /Os /Oi-, and the
//  plane functions as plain statics (not inlined at all then).
//
//  NEAR.  The answer to the open question above: in each
//  of the six plane bodies, `pV = &(*ppVerts)[0];` instead of
//  `pV = *ppVerts;`.  That one spelling keeps the driver's pVerts a SCALAR
//  memory slot in the spill area (-0x80, stored at each plane's end and
//  re-read at each use, as shipped) instead of a promoted register value
//  with a lazy home; the extra spill slot disappears, the three parameter
//  spills land at the shipped -0x68 / -0x6c / -0x70 (with the shipped hole
//  at -0x64) and every frame slot's use count matches.  The same spelling
//  in big24090.cpp's inlined copy of this clipper fixed its frame too.
//  Spellings measured on pCur / pPrev / pEnd / pOutStart / the write-back
//  (`&(*ppVerts)[i]`, `&pV[n]`, `&(*ppOut)[0]`, `&pOutStart[0]`) change
//  nothing, and `pEnd = &(*ppVerts)[*pnVerts]` is worse.
//  Result: 5,968 bytes shipped against 5,956, the first 49 identical,
//  normalised instruction match 0.895 -> 0.961 (see w23_100525b0_os.cpp
//  for what is left; it is the same here).
// *********************************************************************** //

#include "ltbasedefs.h"


// The 32-byte clip vertex -- w1_10001000.cpp's ClipVert (same tag, so
// g_1008b87c links under the spelling d3dr3c.cpp already uses).
struct ClipVert                                         // 32 bytes
{
	LTVector v;                                         // +0x00  x, y, z
	float  rhw;                                         // +0x0c
	uint8  b, g, r, a;                                  // +0x10..+0x13
	uint8  sb, sg, sr, sa;                              // +0x14..+0x17
	float  tu;                                          // +0x18
	float  tv;                                          // +0x1c
};

// The transform object at 0x1008b880: the projective 4x4 at +0x1dc.
struct XForm10050e60
{
	uint8   m_pad_000[0x1dc];                           // +0x000
	float   m[4][4];                                    // +0x1dc
};

extern unsigned long g_1008b874;                        // 0x1008b874  clip flags
extern uint8         g_1008b880[];                      // 0x1008b880  transform object
extern ClipVert*     g_1008b87c;                        // 0x1008b87c  clip scratch
extern uint32        g_1008d7a0;                        // 0x1008d7a0  UseD3DClip
extern uint32        g_1008be44;                        // 0x1008be44  clip counter
extern float         g_1008b908;                        // 0x1008b908  near plane z
extern float         g_1008b910;                        // 0x1008b910  far plane z

// d3dlow.cpp: a position to camera space through the 3x4 at +0x15c.
extern void sub_1000d9b0(void* pVert, void* pXForm);
// w3_10014580_os.cpp: the screen projection through +0x31c..+0x330.
extern void sub_10014580(void* pVert, void* pXForm);

#define CLIP_EPSILON    0.00001f
#define CLIP_MAX_VERTS  50


// Round to nearest in the current FPU mode; 0x10015190 out of line.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}

// 0x10023380, the vertex lerp (out of line there, inlined here).
static inline void LerpClipVert(ClipVert* a, ClipVert* b, ClipVert* dst, float t)
{
	dst->tu = (b->tu - a->tu) * t + a->tu;
	dst->tv = (b->tv - a->tv) * t + a->tv;
	dst->r  = (uint8)RoundF((b->r  - a->r)  * t + a->r);
	dst->g  = (uint8)RoundF((b->g  - a->g)  * t + a->g);
	dst->b  = (uint8)RoundF((b->b  - a->b)  * t + a->b);
	dst->a  = (uint8)RoundF((b->a  - a->a)  * t + a->a);
	dst->sa = (uint8)RoundF((b->sa - a->sa) * t + a->sa);
}

static inline float ClipEdgeNear(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeLeft(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeTop(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeRight(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeBottom(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeFar(LTVector* a, LTVector* b, LTVector* dst)
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


// ----------------------------------------------------------------------- //
//  0x10001000 -- near plane
// ----------------------------------------------------------------------- //
static inline LTBOOL sub_10001000(uint32* pUnused, ClipVert** ppVerts, int32* pnVerts, ClipVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	ClipVert*  pOutStart;
	ClipVert*  pV;
	ClipVert*  pEnd;
	ClipVert*  pPrev;
	ClipVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->v.z >= g_1008b908);

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
				float t = ClipEdgeNear(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpClipVert(pPrev, pCur, *ppOut, t);

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


// ----------------------------------------------------------------------- //
//  0x10001330 -- left plane
// ----------------------------------------------------------------------- //
static inline LTBOOL sub_10001330(uint32* pUnused, ClipVert** ppVerts, int32* pnVerts, ClipVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	ClipVert*  pOutStart;
	ClipVert*  pV;
	ClipVert*  pEnd;
	ClipVert*  pPrev;
	ClipVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (-pV->v.z < pV->v.x);

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
				float t = ClipEdgeLeft(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpClipVert(pPrev, pCur, *ppOut, t);

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


// ----------------------------------------------------------------------- //
//  0x10001990 -- right plane
// ----------------------------------------------------------------------- //
static inline LTBOOL sub_10001670(uint32* pUnused, ClipVert** ppVerts, int32* pnVerts, ClipVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	ClipVert*  pOutStart;
	ClipVert*  pV;
	ClipVert*  pEnd;
	ClipVert*  pPrev;
	ClipVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->v.y < pV->v.z);

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
				float t = ClipEdgeTop(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpClipVert(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_10001990(uint32* pUnused, ClipVert** ppVerts, int32* pnVerts, ClipVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	ClipVert*  pOutStart;
	ClipVert*  pV;
	ClipVert*  pEnd;
	ClipVert*  pPrev;
	ClipVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->v.x < pV->v.z);

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
				float t = ClipEdgeRight(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpClipVert(pPrev, pCur, *ppOut, t);

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


// ----------------------------------------------------------------------- //
//  0x10001cb0 -- bottom plane
// ----------------------------------------------------------------------- //
static inline LTBOOL sub_10001cb0(uint32* pUnused, ClipVert** ppVerts, int32* pnVerts, ClipVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	ClipVert*  pOutStart;
	ClipVert*  pV;
	ClipVert*  pEnd;
	ClipVert*  pPrev;
	ClipVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (-pV->v.z < pV->v.y);

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
				float t = ClipEdgeBottom(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpClipVert(pPrev, pCur, *ppOut, t);

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


// ----------------------------------------------------------------------- //
//  0x10001fe0 -- far plane
// ----------------------------------------------------------------------- //
static inline LTBOOL sub_10001fe0(uint32* pUnused, ClipVert** ppVerts, int32* pnVerts, ClipVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	ClipVert*  pOutStart;
	ClipVert*  pV;
	ClipVert*  pEnd;
	ClipVert*  pPrev;
	ClipVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->v.z <= g_1008b910);

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
				float t = ClipEdgeFar(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpClipVert(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_1000f4b0(uint32 nClipFlags, ClipVert** ppVerts, int32* pnVerts)
{
	ClipVert*  pOut;
	ClipVert*  pVerts;
	int32      nVerts;
	uint32     anUnused[6];

	if (g_1008d7a0)
	{
		nClipFlags &= 1;
		if (!nClipFlags)
			return LTTRUE;
	}

	pOut   = *(ClipVert* volatile*)&g_1008b87c;
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

	// The far plane is written the other way round from the five above:
	// the shipped tail FALLS THROUGH into the write-back and jumps to the
	// shared `return 0` block (`je` to it), where the five-guards spelling
	// inverts the last test and lays the failure block first.  Measured:
	// this spelling is byte-identical, the other differs in the last 32.
	if (!(nClipFlags & 0x02) ||
	    sub_10001fe0(&anUnused[5], &pVerts, &nVerts, &pOut))
	{
		*ppVerts = pVerts;
		*pnVerts = nVerts;
		return LTTRUE;
	}

	return LTFALSE;
}


// 0x1000a8a0's body (w2_1000a8a0_os.cpp) over the 32-byte vertex, inline.
inline LTBOOL XFormClip(ClipVert** ppVerts, int32* pnVerts, XForm10050e60* pM)
{
	ClipVert*  pWalk;
	int32      nLeft;
	float      fOOW;
	LTVector   vScreen;

	if (!g_1008b874)
	{
		nLeft = *pnVerts;
		pWalk = *ppVerts;
		while (nLeft--)
		{
			fOOW = 1.0f / (float)(pM->m[3][2] * pWalk->v.z + pM->m[3][1] * pWalk->v.y + pM->m[3][0] * pWalk->v.x + pM->m[3][3]);

			vScreen.x = (pWalk->v.x * pM->m[0][0]
			           + pWalk->v.y * pM->m[0][1]
			           + pWalk->v.z * pM->m[0][2]
			           + pM->m[0][3]) * fOOW;

			vScreen.y = (pWalk->v.x * pM->m[1][0]
			           + pWalk->v.y * pM->m[1][1]
			           + pWalk->v.z * pM->m[1][2]
			           + pM->m[1][3]) * fOOW;

			vScreen.z = (pWalk->v.x * pM->m[2][0]
			           + pWalk->v.y * pM->m[2][1]
			           + pWalk->v.z * pM->m[2][2]
			           + pM->m[2][3]) * fOOW;

			pWalk->v = vScreen;
			pWalk->rhw = fOOW;
			pWalk++;
		}
	}
	else
	{
		pWalk = *ppVerts;
		nLeft = *pnVerts;
		while (nLeft--)
		{
			sub_1000d9b0(pWalk, pM);
			pWalk++;
		}

		if (!sub_1000f4b0(g_1008b874, ppVerts, pnVerts))
			return LTFALSE;

		nLeft = *pnVerts;
		pWalk = *ppVerts;
		while (nLeft--)
		{
			sub_10014580(pWalk, pM);
			pWalk++;
		}
	}

	return LTTRUE;
}


// ======================================================================= //
//  0x10050e60
// ======================================================================= //
long __fastcall sub_10050e60(unsigned long nClipFlags, unsigned char** ppVerts,
                             long* pnVerts)
{
	g_1008b874 = nClipFlags;
	return XFormClip((ClipVert**)ppVerts, (int32*)pnVerts, (XForm10050e60*)g_1008b880);
}
