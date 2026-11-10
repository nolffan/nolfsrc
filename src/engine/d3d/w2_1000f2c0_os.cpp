// *********************************************************************** //
//
//  w2_1000f2c0_os.cpp -- sub_1000f2c0 (written for the d3d.ren link).
//
//  sub_1000a8a0 (w2_1000a8a0_os.cpp) instantiated for the 0x20-byte
//  vertex: the same two paths through the transform object's
//  +0x1dc..+0x218 block / +0x15c 3x4 and +0x31c..+0x330 divide, with the
//  32-byte clipper 0x1000f4b0 where 0x1000a8a0 calls 0x1000aa90.  The
//  instruction streams are the same apart from the strides and the
//  scheduling of the z row, so the source is the same.
//
//  Callers: d3dr2b.cpp and d3dr2c_os.cpp (their spellings are aliased).
//
//  /Os: frameless with pushes, `xor %eax,%eax; inc %eax`, 0x90 padding.
//
//  Measured again, all byte-for-byte unchanged unless noted:
//  the z row as `(float)(...)`, matrix-first, the +0x208 term first, the
//  reciprocal on the left, and regrouped (icl canonicalises the sum --
//  every term order of that row compiles to the same bytes); the three
//  rows in the other five statement orders (worse, 139..148 bytes); the
//  last loop with its own counter, as a `for`, and with its two loads
//  swapped; and the function in a TU with its shipped neighbours
//  (0x1000a8a0 with 0x1000a7d0 and d3dr3b.cpp's 0x1000aa90 around it;
//  0x1000f2c0 with d3dr3c_os.cpp's 0x1000f4b0, and inside the /Os /Qip
//  lightmap TU w30_1000f020_osqipni.cpp).  (It was filed with 0x10023230,
//  w4_10023fb0.cpp, as "icl's sum order follows TU history"; it is the
//  operands' address forms, below.)
//
//  IDENTICAL.  The first loop is w2_1000a8a0_os.cpp's: the +0x1dc block
//  as an LTMatrix through a copy of ltmatrix.h's MatVMul_H, and the z
//  row's product order (shipped y*m[2][1], z*m[2][2], x*m[2][0]) from
//  that helper's address forms -- see MatVMulH_f2c0 below.  The hand-
//  written float fields this file had gave z, y, x (4 bytes), and none
//  of the term orders, groupings or TU neighbours measured above moved
//  it.  The last loop is 0x1000a8a0's
//  `for (nLeft = *pnVerts; nLeft; nLeft--, pWalk++)`.
//
// *********************************************************************** //

#include "ltbasedefs.h"


extern unsigned long g_1008b874;                            // 0x1008b874

// 0x1000f4b0 -- the 32-byte-vertex twin of d3dr3b.cpp's clipper
// 0x1000aa90 (d3dr3c_os.cpp; the same (flags, &verts, &count) call).
LTBOOL sub_1000f4b0(uint32 nFlags, void** ppVerts, uint32* pnVerts);
// w2_1000a770.cpp: a vector through a row-major 3x4.
void sub_1000a770(LTVector* pVec, const float* pMat34);


// The 0x20-byte vertex (FVF 0x1c4): position, 1/w.
struct TVert1000f2c0
{
	LTVector m_vPos;                                        // +0x00
	float    m_fRHW;                                        // +0x0c
	uint8    m_pad_10[0x20 - 0x10];                         // +0x10
};

// The transform object: the 3x4 at +0x15c, the projective 4x4 block at
// +0x1dc..+0x218, the perspective terms at +0x31c..+0x330.
struct XForm1000f2c0
{
	uint8   m_pad_000[0x15c];                               // +0x000
	float   m_Mat15c[12];                                   // +0x15c
	uint8   m_pad_18c[0x1dc - 0x18c];                       // +0x18c
	LTMatrix m_Mat1dc;                                      // +0x1dc
	uint8   m_pad_21c[0x31c - 0x21c];                       // +0x21c
	float   m_f31c;                                         // +0x31c
	float   m_f320;                                         // +0x320
	float   m_f324;                                         // +0x324
	float   m_f328;                                         // +0x328
	float   m_f32c;                                         // +0x32c
	float   m_f330;                                         // +0x330
};


// ltmatrix.h's MatVMul_H, with the z row's SECOND matrix term read
// through a float pointer to the matrix (w2_1000a8a0_os.cpp's helper
// reads the first and third that way).  icl canonicalises the term order
// of a sum, but which product it emits first follows the operands'
// address forms: this spelling gives the shipped y, z, x (the header's
// gives y, x, z; the 40-byte twin ships z, x, y).  Found by sweeping
// m[2][c] / f[8+c] per column x member / v[c] per component x product
// order x term order x the side of one_over_w (3072 spellings, 80
// identical -- every one with exactly f[9] among the three products and
// members for the vertex; the term order and the rest do not matter).
inline float MatVMulH_f2c0(LTVector *pDest, LTMatrix *pMat, LTVector *pSrc)
{
	const float* f = &pMat->m[0][0];
	float one_over_w = 1.0f / (pMat->m[3][0]*pSrc->x + pMat->m[3][1]*pSrc->y + pMat->m[3][2]*pSrc->z + pMat->m[3][3]);

	pDest->x = one_over_w * (pMat->m[0][0]*pSrc->x + pMat->m[0][1]*pSrc->y + pMat->m[0][2]*pSrc->z + pMat->m[0][3]);
	pDest->y = one_over_w * (pMat->m[1][0]*pSrc->x + pMat->m[1][1]*pSrc->y + pMat->m[1][2]*pSrc->z + pMat->m[1][3]);
	pDest->z = one_over_w * (pMat->m[2][0]*pSrc->x + f[9]*pSrc->y + pMat->m[2][2]*pSrc->z + pMat->m[2][3]);

	return one_over_w;
}


LTBOOL sub_1000f2c0(TVert1000f2c0** ppVerts, uint32* pnVerts,
                    XForm1000f2c0* pM, uint32 nUnused)
{
	TVert1000f2c0* pWalk;
	uint32 nLeft;
	float fOOW;
	LTVector vScreen;

	if (!g_1008b874)
	{
		nLeft = *pnVerts;
		pWalk = *ppVerts;
		while (nLeft--)
		{
			fOOW = MatVMulH_f2c0(&vScreen, &pM->m_Mat1dc, &pWalk->m_vPos);
			pWalk->m_vPos = vScreen;
			pWalk->m_fRHW = fOOW;
			pWalk++;
		}
	}
	else
	{
	pWalk = *ppVerts;
	nLeft = *pnVerts;
	while (nLeft--)
	{
		sub_1000a770(&pWalk->m_vPos, pM->m_Mat15c);
		pWalk++;
	}

	if (!sub_1000f4b0(g_1008b874, (void**)ppVerts, pnVerts))
		return 0;

	pWalk = *ppVerts;
	for (nLeft = *pnVerts; nLeft; nLeft--, pWalk++)
	{
		fOOW = 1.0f / pWalk->m_vPos.z;
		pWalk->m_fRHW = fOOW;
		pWalk->m_vPos.x = (pWalk->m_vPos.x * pM->m_f31c + pWalk->m_vPos.z * pM->m_f320) * fOOW;
		pWalk->m_vPos.y = (pWalk->m_vPos.y * pM->m_f324 + pWalk->m_vPos.z * pM->m_f328) * pWalk->m_fRHW;
		pWalk->m_vPos.z = (pWalk->m_vPos.z * pM->m_f32c + pM->m_f330) * pWalk->m_fRHW;
	}
	}
	return 1;
}
