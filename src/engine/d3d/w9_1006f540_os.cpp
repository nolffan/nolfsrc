// *********************************************************************** //
//
//  w9_1006f540_os.cpp  --  d3d.ren 0x1006f540, 6,096 bytes (/Os).
//
//  Was w9_1006f540.cpp at /O2.  The shipped body is /Os --
//  `push %edi ; push %edi ; push %esi ; push %ebx` after the frame and not
//  one lea-nop pad in 6,096 bytes -- and its thirty colour bytes are the
//  RoundF helper's `fstps t ; flds t ; fistpl n` (now used below).
//  Measured: /O2 8208 bytes (realign stub + two bodies), /Os 6845,
//  /Os + RoundF 6110, + the fill loop's position read through one pointer
//  and `for (...; nLeft > 0; ...)` (the shipped `jbe`) 6124 bytes against
//  6096, the first 24 bytes identical (frame 0xf10 and the fill's opening
//  as shipped).  Notes below that say /O2 predate this.
//
//  Written for the d3d link from the shipped bytes.  One caller,
//  big729c0.cpp's flush (0x10072ec8: `push %edi ; push pOwner ; call`,
//  the %edi being icl's eight-byte argument-area padding -- the body reads
//  0x8(%ebp) and never 0xc(%ebp)).
//
//  WHAT IT IS.  The ONE-STAGE, 32-byte-vertex sibling of big6d910.cpp's
//  0x1006d910: build a TRIANGLEFAN from the item's source vertices, clip it
//  against the six frustum planes, project it and DrawPrimitive it with
//  FVF 0x1c4 (XYZRHW | DIFFUSE | SPECULAR | TEX1).  Compared with
//  0x1006d910 it has NO overflow guard (no 0x1000a740), NO 0x1006d8e0
//  pre-pass, NO draw decision and NO texture bind: the texture scale is
//  folded into the fill loop instead (u * 0x1008e664, v * 0x1008e668 at
//  0x1006f586..0x1006f592), and the function always returns 1 unless a
//  clip pass rejects the whole polygon.
//
//  REGION MAP
//
//    0x1006f540..0x1006f5ac  prologue (`sub $0xf10`, no _chkstk) and THE
//        FILL: aVerts[i] from pObj->m_aSrc54[i] -- position through the
//        source's +0x00 pointer, white diffuse, the texture pair scaled by
//        stage 0's inverse size.  Count is the uint16 at +0x50, tested
//        `jbe` (unsigned).  The array is 64 x 32 bytes, -0xe0c..-0x60c.
//    0x1006f5ac..0x1006f5c9  pVerts/nVerts (-0x60c/-0x608), then the clip
//        mask 0x1008b874.
//    0x1006f5cf..0x1006f6c2  NO CLIPPING: the inline projective transform
//        through 0x1008b880+0x1dc..+0x218 (the same sixteen fields and the
//        same `fdivrs 1.0f`, 0x100ab750, as big6d910.cpp), the three
//        results through a named three-float local (`rep movsl $3` at
//        0x1006f6aa) and 1/w into +0x0c.  Then straight to the draw.
//    0x1006f6c7..0x1006f70c  the per-vertex 0x1000d9b0 pre-transform, the
//        RE-READ of 0x1008b874 (0x1006f6ee) and UseD3DClip (0x1008d7a0):
//        near plane only, and no near bit -> skip to the projection.
//    0x1006f70c..0x10070b9f  THE SIX CLIP PASSES, bits 0x01, 0x04, 0x08,
//        0x10, 0x20, 0x02 in that order -- z >= near, -z < x, y < z, x < z,
//        -z < y, z <= far -- each with the family's -1e-5/+1e-5/0.0f
//        epsilon triple (0x100ab754..0x100ab7b0), two texture floats and
//        the five byte lerps +0x12, +0x11, +0x10, +0x13, +0x17.  A whole-
//        polygon reject is `return 0` (0x10070bdd).  Expression for
//        expression big124a4.cpp's passes.
//    0x10070ba8..0x10070bdb  pVerts = pIn ; nVerts = nIn, then THE
//        PROJECTION: 0x10014580(pVert, g_1008b880) per vertex.
//    0x10070be6..0x10070c25  DrawPrimitive(6, 0x1c4, pVerts, nVerts, 0)
//        and `return 1`.
//    0x10070c27..0x10070d0e  the six sunk `else fT = 0.0f` epsilon arms.
//
//  FLAGS: /O2 -- outgoing arguments as `add $-N,%esp ; mov` (0x10070be6),
//  `mov $0x20,%ecx ; add %eax,%ecx` for pOut++ (0x1006fa34) and cold
//  blocks sunk past the `ret`, the tells big6d910.cpp lists for the same
//  family.
//
//  NOT BYTE-IDENTICAL, and not expected to be: like every other member of
//  this family in the project (big6d910, big124a4, big70d10, big299a8,
//  big28030) the clipper's x87 scheduling and the float-to-byte
//  conversions (shipped: `fstps ; flds ; fistpl`, the /Qrcd-family
//  form) are not reproduced.  The logic, the calls, the
//  field offsets and the constants are the shipped ones.
// *********************************************************************** //

#include "ltbasedefs.h"

// Round to nearest in the current FPU mode: w1_10001000.cpp's helper
// (0x10015190 out of line), the clipper family's colour-byte conversion.
// Inlined, it is the shipped `fstps t ; flds t ; fistpl n ; mov n,%dl`.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}


// ---------------------------------------------------------------------- //
//  The D3D7 device: only +0x64 DrawPrimitive.
// ---------------------------------------------------------------------- //
struct D3DDevice;

struct D3DDeviceVtbl
{
	uint8			m_pad_00[0x64];
	long (__stdcall* DrawPrimitive)(D3DDevice*, uint32, uint32, void*,
	                                uint32, uint32);                // +0x64
};

struct D3DDevice
{
	D3DDeviceVtbl*	lpVtbl;
};


// ---------------------------------------------------------------------- //
//  The 32-byte vertex, FVF 0x1c4.  Named ClipVert so that the scratch
//  pointer 0x1008b87c keeps the spelling d3ddata.cpp defines it under.
// ---------------------------------------------------------------------- //
struct Vec6f540
{
	float	x;                                          // +0x00
	float	y;                                          // +0x04
	float	z;                                          // +0x08
};

union VColor6f540
{
	uint32		m_n;
	uint8		m_b[4];
};

struct ClipVert                                         // 0x20 bytes
{
	Vec6f540		m_vPos;                             // +0x00
	float			m_fRHW;                             // +0x0c
	VColor6f540		m_Diffuse10;                        // +0x10
	VColor6f540		m_Specular14;                       // +0x14
	float			m_fU;                               // +0x18
	float			m_fV;                               // +0x1c
};


// The source vertex, stride 0x18 (`add $0x18,%edx` at 0x1006f59a): a
// position POINTER and the texture pair.  big6d910.cpp's SrcVert6d910.
struct SrcVert6f540
{
	Vec6f540*	m_pPos00;                               // +0x00
	float		m_fU04;                                 // +0x04
	float		m_fV08;                                 // +0x08
	float		m_fU10c;                                // +0x0c
	float		m_fV110;                                // +0x10
	uint32		m_n14;                                  // +0x14
};

// The item: the uint16 vertex count at +0x50 and the sources at +0x54.
struct Obj6f540
{
	uint8			m_pad_00[0x50];
	uint16			m_nCount50;                         // +0x50
	uint8			m_pad_52[0x54 - 0x52];
	SrcVert6f540	m_aSrc54[1];                        // +0x54
};

// 0x1008b880's projective block, +0x1dc..+0x218 (big6d910.cpp's XForm6d910).
struct XForm6f540
{
	uint8	m_pad_000[0x1dc];
	float	m_f1dc, m_f1e0, m_f1e4, m_f1e8;             // +0x1dc
	float	m_f1ec, m_f1f0, m_f1f4, m_f1f8;             // +0x1ec
	float	m_f1fc, m_f200, m_f204, m_f208;             // +0x1fc
	float	m_f20c, m_f210, m_f214, m_f218;             // +0x20c
};


// ---------------------------------------------------------------------- //
//  Externals.
// ---------------------------------------------------------------------- //
extern D3DDevice*		g_pD3DDevice;                   // 0x1008d988
extern uint32			g_1008b874;                     // 0x1008b874  clip mask
extern ClipVert*		g_1008b87c;                     // 0x1008b87c  scratch
extern uint8			g_1008b880[];                   // 0x1008b880  transform
extern float			g_1008b908;                     // 0x1008b908  near
extern float			g_1008b910;                     // 0x1008b910  far
extern uint32			g_1008be44;                     // 0x1008be44  clip passes
extern void*			g_p1008d7a0;                    // 0x1008d7a0  UseD3DClip
extern float			g_f1008e664;                    // 0x1008e664  stage 0 1/w
extern float			g_f1008e668;                    // 0x1008e668  stage 0 1/h

// 0x1000d9b0 -- d3dlow.cpp, camera-space transform of one vertex.
extern "C" void  sub_1000d9b0(ClipVert* pVert, uint8* pXForm);

// 0x10014580 -- the 32-byte vertex's screen projection (big124a4.cpp,
// big299a8.cpp and big28030.cpp call it the same way).
extern "C" void  sub_10014580(ClipVert* pVert, uint8* pXForm);


// ---------------------------------------------------------------------- //
//  THE CLIPPER: d3dr3c_os.cpp's 0x1000f4b0 over the six
//  32-byte plane bodies 0x10001000 .. 0x10001fe0, all INLINED, as
//  w55_10016324_os.cpp writes them; pVerts / nVerts / pOut keep frame
//  slots because their addresses go to the plane bodies.
// ---------------------------------------------------------------------- //
#define CLIP_EPSILON    0.00001f
#define CLIP_MAX_VERTS  50

static inline void LerpClipVert(ClipVert* a, ClipVert* b, ClipVert* dst, float t)
{
	dst->m_fU = (b->m_fU - a->m_fU) * t + a->m_fU;
	dst->m_fV = (b->m_fV - a->m_fV) * t + a->m_fV;
	dst->m_Diffuse10.m_b[2]  = (uint8)RoundF((b->m_Diffuse10.m_b[2]  - a->m_Diffuse10.m_b[2])  * t + a->m_Diffuse10.m_b[2]);
	dst->m_Diffuse10.m_b[1]  = (uint8)RoundF((b->m_Diffuse10.m_b[1]  - a->m_Diffuse10.m_b[1])  * t + a->m_Diffuse10.m_b[1]);
	dst->m_Diffuse10.m_b[0]  = (uint8)RoundF((b->m_Diffuse10.m_b[0]  - a->m_Diffuse10.m_b[0])  * t + a->m_Diffuse10.m_b[0]);
	dst->m_Diffuse10.m_b[3]  = (uint8)RoundF((b->m_Diffuse10.m_b[3]  - a->m_Diffuse10.m_b[3])  * t + a->m_Diffuse10.m_b[3]);
	dst->m_Specular14.m_b[3] = (uint8)RoundF((b->m_Specular14.m_b[3] - a->m_Specular14.m_b[3]) * t + a->m_Specular14.m_b[3]);
}


static inline float ClipEdgeNear(Vec6f540* a, Vec6f540* b, Vec6f540* dst)
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


static inline float ClipEdgeLeft(Vec6f540* a, Vec6f540* b, Vec6f540* dst)
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


static inline float ClipEdgeTop(Vec6f540* a, Vec6f540* b, Vec6f540* dst)
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


static inline float ClipEdgeRight(Vec6f540* a, Vec6f540* b, Vec6f540* dst)
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


static inline float ClipEdgeBottom(Vec6f540* a, Vec6f540* b, Vec6f540* dst)
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


static inline float ClipEdgeFar(Vec6f540* a, Vec6f540* b, Vec6f540* dst)
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
// Each plane body opens `pV = &(*ppVerts)[0];` rather than
// `pV = *ppVerts;` -- w23_100525b0_os.cpp's finding: it keeps the
// inlined driver's pVerts a memory slot, as the shipped 0x1006f540 has it
// (normalised instruction diff 171 -> 78).
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
	pEnd    = &(*ppVerts)[*pnVerts];

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
				float t = ClipEdgeNear(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

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


// 0x10001330
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
	pEnd    = &(*ppVerts)[*pnVerts];

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


// 0x10001670
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
	pEnd    = &(*ppVerts)[*pnVerts];

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


// 0x10001990
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
	pEnd    = &(*ppVerts)[*pnVerts];

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


// 0x10001cb0
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
	pEnd    = &(*ppVerts)[*pnVerts];

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


// 0x10001fe0
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
	pEnd    = &(*ppVerts)[*pnVerts];

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


// 0x1000f4b0 -- d3dr3c_os.cpp's driver.
static inline LTBOOL sub_1000f4b0(uint32 nClipFlags, ClipVert** ppVerts, int32* pnVerts)
{
	ClipVert*     pOut;
	ClipVert*     pOutStart;
	ClipVert*     pVerts;
	int32      nVerts;
	uint32     anUnused[6];

	if (g_p1008d7a0)
	{
		nClipFlags &= 1;
		if (!nClipFlags)
			return LTTRUE;
	}

	pOut   = pOutStart = g_1008b87c;
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



// ======================================================================= //
//  0x1006f540 .. 0x10070d0e   (6,096 bytes)   1 caller
// ======================================================================= //
extern "C" int32 sub_1006f540(Obj6f540* pObj)
{
	ClipVert		aVerts[64];                         // -0xe0c
	ClipVert*		pVerts;                             // -0x60c
	int32			nVerts;                             // -0x608

	ClipVert*		pWalk;
	SrcVert6f540*	pSrc;
	uint32			nLeft;

	Vec6f540		vScreen;
	float			fOOW;
	XForm6f540*		pM;

	// -- 0x1006f54d: THE FILL.  Every position float moves as an integer
	//    `mov`; the texture pair is scaled here, source operand first.
	Vec6f540*		pPos;

	pWalk = aVerts;
	pSrc  = pObj->m_aSrc54;
	for (nLeft = pObj->m_nCount50; nLeft > 0; nLeft--)
	{
		pPos = pSrc->m_pPos00;
		pWalk->m_vPos.x = pPos->x;
		pWalk->m_vPos.y = pPos->y;
		pWalk->m_vPos.z = pPos->z;

		pWalk->m_Diffuse10.m_n = 0xffffffff;

		// Both products before either store (the shipped loop keeps them
		// on the x87 stack and stores them at the top of the next pass).
		{
			float fU = pSrc->m_fU04 * g_f1008e664;
			float fV = pSrc->m_fV08 * g_f1008e668;

			pWalk->m_fU = fU;
			pWalk->m_fV = fV;
		}

		pSrc++;
		pWalk++;
	}

	pVerts = aVerts;
	nVerts = pObj->m_nCount50;

	if (!g_1008b874)
	{
		// -------------------------------------------------------------- //
		//  0x1006f5cf -- NO CLIPPING WANTED.  big6d910.cpp's inline
		//  projective transform, verbatim.
		// -------------------------------------------------------------- //
		pM = (XForm6f540*)g_1008b880;

		pWalk = pVerts;
		nLeft = nVerts;

		while (nLeft--)
		{
			// The operand spellings are what reproduce icl's shipped product
			// order (it canonicalises the term order of a sum, but which
			// product comes first, and which operand of it is loaded first,
			// follows the operands' address forms).  Found by sweeping
			// member / pointer forms per operand, about 500 spellings.
			const float* v = &pWalk->m_vPos.x;

			fOOW = 1.0f / (pWalk->m_vPos.x * pM->m_f20c
			             + pWalk->m_vPos.y * pM->m_f210
			             + pWalk->m_vPos.z * (&pM->m_f20c)[2]
			             + pM->m_f218);

			vScreen.x = (pWalk->m_vPos.x * pM->m_f1dc
			           + pWalk->m_vPos.y * pM->m_f1e0
			           + pWalk->m_vPos.z * pM->m_f1e4
			           + pM->m_f1e8) * fOOW;

			vScreen.y = (pWalk->m_vPos.x * pM->m_f1ec
			           + pWalk->m_vPos.y * pM->m_f1f0
			           + pWalk->m_vPos.z * pM->m_f1f4
			           + pM->m_f1f8) * fOOW;

			vScreen.z = (v[0] * pM->m_f1fc
			           + v[1] * pM->m_f200
			           + pWalk->m_vPos.z * pM->m_f204
			           + pM->m_f208) * fOOW;

			pWalk->m_vPos = vScreen;
			pWalk->m_fRHW = fOOW;
			pWalk++;
		}

		goto L_Draw;
	}

	// -- 0x1006f6c7: camera-space pre-transform, then the mask is RE-READ
	//    (0x1006f6ee), as in big6d910.cpp and 0x1000a8a0.
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		sub_1000d9b0(pWalk, g_1008b880);
		pWalk++;
	}

	if (!sub_1000f4b0(g_1008b874, &pVerts, &nVerts))
		return 0;


	// -- 0x10070bb4: camera space -> screen, one call per vertex.
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		sub_10014580(pWalk, g_1008b880);
		pWalk++;
	}

	goto L_Draw;

L_Draw:
	// -- 0x10070be6: vtable +0x64.  6 = D3DPT_TRIANGLEFAN, 0x1c4 =
	//    XYZRHW | DIFFUSE | SPECULAR | TEX1.
	g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, 6, 0x1c4, pVerts,
	                                    nVerts, 0);
	return 1;
}
