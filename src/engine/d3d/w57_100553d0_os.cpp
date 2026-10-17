// *********************************************************************** //
//
//  w57_100553d0_os -- d3d.ren.
//
//  0x100553d0 (5,828 bytes, then 0x90 padding): slot 1 of the SECOND
//  __fastcall clip table at .data 0x10089770 ({0x10053e20, 0x100553d0,
//  0x10056aa0, 0x10059c70}), the clipper for the 40-byte two-stage vertex (SClipVertA).
//  The table at 0x1008975c holds the clip-and-project versions of the same
//  vertices (0x100525b0 (w23_100525b0_os.cpp)); this one is their middle part only: the driver
//  0x1000f4b0 (d3dr3c_os.cpp) with its six plane clippers inlined, and
//  NO transform before it and NO projection after it -- the bytes have no
//  call at all.  Read off them:
//
//    * the prologue to the first plane is w23's driver verbatim (`sub
//      $0x6e8`, the UseD3DClip test `and $0x1,%ecx ; jne` answering 1
//      through `xor %edx,%edx ; inc %edx ; jmp <exit>`);
//    * six classify / Sutherland-Hodgman blocks in the driver's order near
//      0x01, left 0x04, top 0x08, right 0x10, bottom 0x20, far 0x02, each
//      with its own sunk epsilon arm and its own `return 0` after the one
//      `ret $0x4`;
//    * `*ppVerts = pVerts ; *pnVerts = nVerts ; return 1`.
//
//  WRITTEN from w23's source for the same vertex (lerp, crossings, RoundF)
//  with the driver kept as the six plane functions of w1_10001000.cpp
//  inlined (inline functions taking &pVerts / &nVerts / &pOut, as the
//  out-of-line 0x10001000.. are called from 0x1000f4b0) rather than w23's
//  macro blocks: 250 -> 248 differing mnemonic lines on 0x10053e20, 139 ->
//  139 here.  /Os, as w23 (`rep movsl` vertex copies, 0x90 padding).
//
//  OUTCOME: NEAR -- same frame, same blocks in the same order, the
//  instruction count within 1%; the residual is the clip family's (w23's
//  notes): register choice through the crossing arithmetic and the
//  shipped pOutStart slot being written at the top of the near block.
//  Measured without effect: pOutStart at the driver's scope, the order of
//  the driver's three loads.
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

extern SClipVertA*     g_1008b87c;                        // 0x1008b87c  clip scratch
extern uint32        g_1008d7a0;                        // 0x1008d7a0  UseD3DClip
extern uint32        g_1008be44;                        // 0x1008be44  clip counter
extern float         g_1008b908;                        // 0x1008b908  near plane z
extern float         g_1008b910;                        // 0x1008b910  far plane z

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
//  One Sutherland-Hodgman pass over the driver's pVerts / nVerts / pOut
//  (the body of w1_10001000.cpp's six functions); `IN` is the plane's
//  inside test on pV, `EDGE` its crossing.  A macro because the six
//  inlined planes differ only in those; a polygon with no vertex inside
//  rejects the whole clip.
// ----------------------------------------------------------------------- //
#define CLIP_FN(NAME, IN, EDGE)                                             \
inline LTBOOL NAME(SClipVertA** ppVerts, int32* pnVerts, SClipVertA** ppOut)    \
{                                                                           \
	uint32     anInside[CLIP_MAX_VERTS];                                    \
	SClipVertA*  pOutStart;                                                   \
	SClipVertA*  pV;                                                          \
	SClipVertA*  pEnd;                                                        \
	SClipVertA*  pPrev;                                                       \
	SClipVertA*  pCur;                                                        \
	uint32*    pFlag;                                                       \
	int32      nInside;                                                     \
	int32      i;                                                           \
	int32      iPrev;                                                       \
                                                                            \
	++g_1008be44;                                                           \
                                                                            \
	nInside = 0;                                                            \
	pFlag   = anInside;                                                     \
	pV      = *ppVerts;                                                     \
	pEnd    = pV + *pnVerts;                                                \
                                                                            \
	for (; pV != pEnd; ++pV)                                                \
	{                                                                       \
		uint32 bIn = (IN);                                                  \
                                                                            \
		nInside += bIn;                                                     \
		*pFlag++ = bIn;                                                     \
	}                                                                       \
                                                                            \
	if (nInside == 0)                                                       \
		return LTFALSE;                                                     \
                                                                            \
	if (nInside != *pnVerts)                                                \
	{                                                                       \
		pOutStart = *ppOut;                                                 \
		iPrev     = *pnVerts - 1;                                           \
		pPrev     = *ppVerts + (*pnVerts - 1);                              \
                                                                            \
		for (i = 0; i < *pnVerts; ++i)                                      \
		{                                                                   \
			pCur = *ppVerts + i;                                            \
                                                                            \
			if (anInside[iPrev])                                            \
				*(*ppOut)++ = *pPrev;                                       \
                                                                            \
			if (anInside[iPrev] != anInside[i])                             \
			{                                                               \
				float t = EDGE(&pPrev->v, &pCur->v, &(*ppOut)->v);                        \
                                                                            \
				LerpSClipVertA(pPrev, pCur, *ppOut, t);                       \
                                                                            \
				++(*ppOut);                                                 \
			}                                                               \
                                                                            \
			iPrev = i;                                                      \
			pPrev = pCur;                                                   \
		}                                                                   \
                                                                            \
		*pnVerts = *ppOut - pOutStart;                                      \
		*ppVerts = pOutStart;                                               \
		*ppOut  += *pnVerts;                                                \
	}                                                                       \
                                                                            \
	return LTTRUE;                                                          \
}

CLIP_FN(ClipNear,   (pV->v.z >= g_1008b908), ClipEdgeNear)
CLIP_FN(ClipLeft,   (-pV->v.z < pV->v.x),      ClipEdgeLeft)
CLIP_FN(ClipTop,    (pV->v.y < pV->v.z),       ClipEdgeTop)
CLIP_FN(ClipRight,  (pV->v.x < pV->v.z),       ClipEdgeRight)
CLIP_FN(ClipBottom, (-pV->v.z < pV->v.y),      ClipEdgeBottom)
CLIP_FN(ClipFar,    (pV->v.z <= g_1008b910), ClipEdgeFar)


// 0x1000f4b0 (d3dr3c_os.cpp), the driver, inline.
inline LTBOOL ClipPoly(uint32 nClipFlags, SClipVertA** ppVerts, int32* pnVerts)
{
	SClipVertA*  pOut;
	SClipVertA*  pOutStart;
	SClipVertA*  pVerts;
	int32      nVerts;

	if (g_1008d7a0)
	{
		nClipFlags &= 1;
		if (!nClipFlags)
			return LTTRUE;
	}

	pOut   = pOutStart = g_1008b87c;
	pVerts = *ppVerts;
	nVerts = *pnVerts;

	if (nClipFlags & 0x01)
		if (!ClipNear(&pVerts, &nVerts, &pOut))
			return LTFALSE;

	if (nClipFlags & 0x04)
		if (!ClipLeft(&pVerts, &nVerts, &pOut))
			return LTFALSE;

	if (nClipFlags & 0x08)
		if (!ClipTop(&pVerts, &nVerts, &pOut))
			return LTFALSE;

	if (nClipFlags & 0x10)
		if (!ClipRight(&pVerts, &nVerts, &pOut))
			return LTFALSE;

	if (nClipFlags & 0x20)
		if (!ClipBottom(&pVerts, &nVerts, &pOut))
			return LTFALSE;

	if (nClipFlags & 0x02)
		if (!ClipFar(&pVerts, &nVerts, &pOut))
			return LTFALSE;

	*ppVerts = pVerts;
	*pnVerts = nVerts;
	return LTTRUE;
}

// ======================================================================= //
//  0x100553d0
// ======================================================================= //
long __fastcall sub_100553d0(unsigned long nClipFlags, unsigned char** ppVerts,
                             long* pnVerts)
{
	return ClipPoly(nClipFlags, (SClipVertA**)ppVerts, (int32*)pnVerts);
}
