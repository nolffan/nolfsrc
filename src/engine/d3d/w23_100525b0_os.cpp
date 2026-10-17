// *********************************************************************** //
//
//  w23_100525b0_os.cpp -- d3d.ren, written for the link.
//
//  0x100525b0 .. 0x10053e18 (6,248 bytes, then 8 bytes of 0x90 to
//  0x10053e20): slot 2 of g_1008975c, the clipper for the TWO-texture-
//  stage vertex (0x28 bytes, FVF 0x2c4).  The same source as slot 1,
//  0x10050e60 (w23_10050e60_os.cpp -- read that file's notes first), over
//  the 40-byte vertex; the differences, each read off these bytes:
//
//    * stride 0x28: `lea (%eax,%eax,4)` / `lea (%ebx,%edx,8)` for the
//      pointer arithmetic and `cltd ; idiv` by 0x28 for the six
//      `nVerts = pOut - pOutStart` (there `sar $0x5`);
//    * the lerp carries the second texture pair: +0x18, +0x1c, +0x20,
//      +0x24, then the five bytes +0x12, +0x11, +0x10, +0x13, +0x17
//      (w1_10002310.cpp's LerpSClipVertAA order);
//    * the screen projection is 0x1000d940 (w2_1000d940_os.cpp), the
//      same body as 0x10014580, and the clip-scratch global g_1008b87c is
//      spelled with the 40-byte tag (d3dr3b.cpp's SSClipVertAA).
//
//  ONE prologue (0x100525b0), ONE `ret $0x4` (0x10053d25); the six sunk
//  epsilon arms and the shared `return 0` are 0x10053d28..0x10053e18.
//  No row of the enumeration falls inside it.
//
//  SIGNATURE: the table's `long (__fastcall*)(unsigned long, unsigned
//  char**, long*)`, as for slot 1 (%ecx bits, %edx vertex pointer
//  pointer, one stack argument, `ret $0x4`).
//
//  FLAGS: /Os, as slot 1 (pushed call arguments, `rep movsl` x10 for the
//  vertex copy, 0x90 padding after the last sunk arm).
//
//  OUTCOME: NEAR (6,267 against 6,248 bytes; same frame, same blocks in
//  the same order); the residual is slot 1's, per crossing.
//
//  The clipper is now the inline driver over the six inline
//  plane bodies (see THE CLIPPER below), instruction match 0.900 -> 0.916.
//  Still unpaired by ONE byte: the first 15 bytes match and the 16th is
//  ppVerts' home, -0x64 here against the shipped -0x6c (pnVerts -0x68 /
//  -0x70, the clip flags -0x70 / -0x68).  Everything below -0x70 maps 1:1.
//  The three slots are register-allocator spills, and ours has one more
//  (-0x6c, pVerts kept in %ecx across the classify loop, where the shipped
//  code re-reads pVerts from its home -0x80 and has a hole instead).  No
//  effect on the order: swapping XFormClip's or ClipPoly's parameters,
//  the driver's local order, the flags as a driver local (worse), the
//  transform body written straight into the export (pnVerts right,
//  ppVerts -0x68), an XFormClip flag parameter, a result variable, the
//  counter's type, and the flag sets /Os /Oi-, /Os /Qsfalign16, /Os /G5,
//  /Os /Qip, /Os /Oi- /Qip, /O2 (worse), /Os /Op (worse).
//  THE LEAD (measured, not kept): the shipped driver keeps pVerts
//  MEMORY-resident (stored to -0x80 the moment it is read, re-read at every
//  plane); icl here promotes it to %ecx with a lazy home, and that is what
//  adds the -0x6c spill.  Any spelling that stops the promotion gives the
//  shipped -0x6c / -0x70 / -0x68 opening at once -- `pV = ppVerts[0]` (or
//  `ppVerts[0] = pOutStart`, `ppVerts[0] + i`, or the type-punned
//  `(SClipVertA*)*(uint32*)ppVerts`) in the six plane bodies,
//  or a volatile pVerts -- but each puts pVerts among the aggregates
//  (-0x600) and grows the frame by 8 or 16, so the row still does not
//  pair.  What keeps it a SCALAR memory slot is the open question.
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
//  Result: 6,248 bytes shipped against 6,256, the first 49 identical,
//  frame 0x700, normalised instruction match 0.916 -> 0.971.  What is
//  left: in planes 2..6 the classify loop's pV / pFlag / pEnd take
//  %esi / %ecx / %edx where ours take %edx / %esi / %ecx, plane 1's
//  pOutStart is stored at the driver (forwarded from pOut) where ours
//  re-reads it, and the no-clip transform's z row multiplies y, x, z where
//  ours does z, y, x (every term order and `fOOW *` placement compiles the
//  same; ltmatrix.h's MatVMul_InPlace_H is worse, prefix 34).  Also
//  measured without effect here: the plane signatures as the real
//  out-of-line (void**, uint32*, void**) with casts, `static inline`
//  ClipEdge / Lerp, the driver's three loads in all orders, the write-back
//  order, the far plane as a sixth guard.  pVerts as a by-value ClipPoly
//  parameter (with or without nVerts) is worse: 0.896.
//
// *********************************************************************** //

#include "ltbasedefs.h"


// The 40-byte clip vertex -- w1_10002310.cpp's SClipVertA (same tag, so
// g_1008b87c links under a spelling the TUs already use).
struct SClipVertA                                         // 40 bytes
{
	LTVector v;                                         // +0x00  x, y, z
	float  rhw;                                         // +0x0c
	uint8  b, g, r, a;                                  // +0x10..+0x13
	uint8  sb, sg, sr, sa;                              // +0x14..+0x17
	float  tu;                                          // +0x18
	float  tv;                                          // +0x1c
	float  tu2;                                         // +0x20
	float  tv2;                                         // +0x24
};

// The transform object at 0x1008b880: the projective 4x4 at +0x1dc.
struct XForm100525b0
{
	uint8   m_pad_000[0x1dc];                           // +0x000
	float   m[4][4];                                    // +0x1dc
};

extern unsigned long g_1008b874;                        // 0x1008b874  clip flags
extern uint8         g_1008b880[];                      // 0x1008b880  transform object
extern SClipVertA*     g_1008b87c;                        // 0x1008b87c  clip scratch
extern uint32        g_1008d7a0;                        // 0x1008d7a0  UseD3DClip
extern uint32        g_1008be44;                        // 0x1008be44  clip counter
extern float         g_1008b908;                        // 0x1008b908  near plane z
extern float         g_1008b910;                        // 0x1008b910  far plane z

// d3dlow.cpp: a position to camera space through the 3x4 at +0x15c.
extern void sub_1000d9b0(void* pVert, void* pXForm);
// w2_1000d940_os.cpp: the screen projection through +0x31c..+0x330.
extern "C" void sub_1000d940(void* pVert, void* pXForm);

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

// 0x10023380, the vertex lerp, over the 40-byte vertex.
inline void LerpSClipVertA(SClipVertA* a, SClipVertA* b, SClipVertA* dst, float t)
{
	dst->tu = (b->tu - a->tu) * t + a->tu;
	dst->tv = (b->tv - a->tv) * t + a->tv;
	dst->tu2 = (b->tu2 - a->tu2) * t + a->tu2;
	dst->tv2 = (b->tv2 - a->tv2) * t + a->tv2;
	dst->r  = (uint8)RoundF((b->r  - a->r)  * t + a->r);
	dst->g  = (uint8)RoundF((b->g  - a->g)  * t + a->g);
	dst->b  = (uint8)RoundF((b->b  - a->b)  * t + a->b);
	dst->a  = (uint8)RoundF((b->a  - a->a)  * t + a->a);
	dst->sa = (uint8)RoundF((b->sa - a->sa) * t + a->sa);
}


// ----------------------------------------------------------------------- //
//  The six crossings (w1_10001000.cpp's ClipEdge*): t, and the position
//  of the crossing with the plane-pinned coordinates.
// ----------------------------------------------------------------------- //
inline float ClipEdgeNear(LTVector* a, LTVector* b, LTVector* dst)
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

inline float ClipEdgeLeft(LTVector* a, LTVector* b, LTVector* dst)
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

inline float ClipEdgeTop(LTVector* a, LTVector* b, LTVector* dst)
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

inline float ClipEdgeRight(LTVector* a, LTVector* b, LTVector* dst)
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

inline float ClipEdgeBottom(LTVector* a, LTVector* b, LTVector* dst)
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

inline float ClipEdgeFar(LTVector* a, LTVector* b, LTVector* dst)
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
//  THE CLIPPER: the 40-byte driver (big051a0.cpp's
//  ClipPoly051a0, the twin of d3dr3c_os.cpp's 0x1000f4b0) over the six
//  plane bodies 0x10002310..0x10003410 (w1_10002310.cpp /
//  w44_10002d40.cpp), all inline.  The bodies take &pVerts / &nVerts /
//  &pOut, so those keep frame slots (-0x80 / -0xbc / -0x78 shipped) and the
//  clip flags too (-0x68): the earlier CLIP_BLOCK macro kept pVerts in a
//  register and pushed ppVerts from -0x6c up to -0x68.
// ----------------------------------------------------------------------- //
static inline LTBOOL sub_10002310(uint32* pUnused, SClipVertA** ppVerts, int32* pnVerts, SClipVertA** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	SClipVertA*  pOutStart;
	SClipVertA*  pV;
	SClipVertA*  pEnd;
	SClipVertA*  pPrev;
	SClipVertA*  pCur;
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

				LerpSClipVertA(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_10002670(uint32* pUnused, SClipVertA** ppVerts, int32* pnVerts, SClipVertA** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	SClipVertA*  pOutStart;
	SClipVertA*  pV;
	SClipVertA*  pEnd;
	SClipVertA*  pPrev;
	SClipVertA*  pCur;
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

				LerpSClipVertA(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_100029e0(uint32* pUnused, SClipVertA** ppVerts, int32* pnVerts, SClipVertA** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	SClipVertA*  pOutStart;
	SClipVertA*  pV;
	SClipVertA*  pEnd;
	SClipVertA*  pPrev;
	SClipVertA*  pCur;
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

				LerpSClipVertA(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_10002d40(uint32* pUnused, SClipVertA** ppVerts, int32* pnVerts, SClipVertA** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	SClipVertA*  pOutStart;
	SClipVertA*  pV;
	SClipVertA*  pEnd;
	SClipVertA*  pPrev;
	SClipVertA*  pCur;
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

				LerpSClipVertA(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_100030a0(uint32* pUnused, SClipVertA** ppVerts, int32* pnVerts, SClipVertA** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	SClipVertA*  pOutStart;
	SClipVertA*  pV;
	SClipVertA*  pEnd;
	SClipVertA*  pPrev;
	SClipVertA*  pCur;
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

				LerpSClipVertA(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_10003410(uint32* pUnused, SClipVertA** ppVerts, int32* pnVerts, SClipVertA** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	SClipVertA*  pOutStart;
	SClipVertA*  pV;
	SClipVertA*  pEnd;
	SClipVertA*  pPrev;
	SClipVertA*  pCur;
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

				LerpSClipVertA(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL ClipPoly(uint32 nClipFlags, SClipVertA** ppVerts,
                                     int32* pnVerts)
{
	SClipVertA*  pOut;
	SClipVertA*  pVerts;
	int32       nVerts;
	uint32      anUnused[6];

	if (g_1008d7a0)
	{
		nClipFlags &= 1;
		if (!nClipFlags)
			return LTTRUE;
	}

	pOut   = *(SClipVertA* volatile*)&g_1008b87c;
	pVerts = *ppVerts;
	nVerts = *pnVerts;

	if (nClipFlags & 0x01)
	{
		if (!sub_10002310(&anUnused[0], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x04)
	{
		if (!sub_10002670(&anUnused[1], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x08)
	{
		if (!sub_100029e0(&anUnused[2], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x10)
	{
		if (!sub_10002d40(&anUnused[3], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x20)
	{
		if (!sub_100030a0(&anUnused[4], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (!(nClipFlags & 0x02) ||
	    sub_10003410(&anUnused[5], &pVerts, &nVerts, &pOut))
	{
		*ppVerts = pVerts;
		*pnVerts = nVerts;
		return LTTRUE;
	}

	return LTFALSE;
}



// 0x1000a8a0's body (w2_1000a8a0_os.cpp) over the 40-byte vertex, inline.
inline LTBOOL XFormClip(SClipVertA** ppVerts, int32* pnVerts, XForm100525b0* pM)
{
	SClipVertA*  pWalk;
	int32      nLeft;
	float      fOOW;
	LTVector   vScreen;

	if (!g_1008b874)
	{
		nLeft = *pnVerts;
		pWalk = *ppVerts;
		while (nLeft--)
		{
			fOOW = 1.0f / (float)(pM->m[3][0] * pWalk->v.x
			                    + pM->m[3][1] * pWalk->v.y
			                    + pM->m[3][2] * pWalk->v.z
			                    + pM->m[3][3]);

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

			*(LTVector*)&pWalk->v.x = vScreen;
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

		if (!ClipPoly(g_1008b874, ppVerts, pnVerts))
			return LTFALSE;

		nLeft = *pnVerts;
		pWalk = *ppVerts;
		while (nLeft--)
		{
			sub_1000d940(pWalk, pM);
			pWalk++;
		}
	}

	return LTTRUE;
}


// ======================================================================= //
//  0x10050e60
// ======================================================================= //
long __fastcall sub_100525b0(unsigned long nClipFlags, unsigned char** ppVerts,
                             long* pnVerts)
{
	g_1008b874 = nClipFlags;
	return XFormClip((SClipVertA**)ppVerts, (int32*)pnVerts, (XForm100525b0*)g_1008b880);
}
