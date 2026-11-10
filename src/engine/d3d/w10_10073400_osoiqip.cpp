// *********************************************************************** //
//
//  w10_10073400_osoiqip.cpp -- d3d.ren 0x10073400..0x10073bf5 (quaternion to
//  matrix, quaternion slerp, and the world-tree polygon collector),
//  written for the whole-DLL link.  /Os /Oi- /Qip (w<N>_*_osoiqip.cpp).
//  (the collector 0x10073700 moved to
//  w56_10073700_osoiqipsf.cpp -- it needs /Qsfalign16, which would cost
//  0x10073500 its byte-identical realign entry here.)
//
//  Why /Os /Oi-: the three end with /Os padding (`lea 0x0(%esi),%esi`
//  to a 4-byte boundary, then 0x90s), and the slerp and the collector
//  CALL the CRT's acos (0x10080230), sin (0x1007fdd0) and sqrt
//  (0x1007fc98) rather than expanding them.
//
//  /Qip, and the file's name (osoiqip = /Os /Oi- /Qip).  The shipped
//  TU is src/LT2/sdk/inc/ltquatbase.cpp: 0x10073400 is quat_ConvertToMatrix
//  and 0x10073500 quat_Slerp, in the SDK file's order.  0x10073400 is
//  icl's /Qip two-entry form (`mov 0x4(%esp),%eax ; mov 0x8(%esp),%edx`
//  at 0x10073400 falling into a register entry at 0x10073408), which icl
//  emits only when a same-TU caller exists -- and ltquatbase.cpp has one:
//  quat_GetVectors calls quat_ConvertToMatrix.  quat_GetVectors (and
//  quat_ConvertFromMatrix) have no caller in the image, so the shipped
//  link dropped them; nothing reaches 0x10073408 there.  quat_GetVectors
//  is therefore written below, verbatim, as the reason the pair exists.
//  Measured: /Os /Oi- /Qip with it, 0x10073400 (8 bytes) and 0x10073408
//  (233) byte-identical, where /Os /Oi- alone was 43 bytes off; without
//  it /Qip changes nothing.  quat_ConvertFromMatrix is left out (no
//  effect on any byte measured); 0x10073500 and 0x10073700 compile the
//  same with and without /Qip.
//
// *********************************************************************** //

#include "ltbasedefs.h"


// ======================================================================= //
//  0x10073400, 244 bytes.  __cdecl (rotation, matrix): quaternion to
//  matrix -- ltquatbase.cpp's quat_ConvertToMatrix, the renderer's own
//  copy.  Callers: 0x1000e3a6, 0x1001523b, 0x10020c16, 0x1007cd4c,
//  0x1007d0ce, all `push; push; call; add $0x8,%esp`.
//
//  WHAT PINS IT:
//    * s = 2.0f (0x100ab834) / (sum of the four squares), one fdivrs;
//    * three `1.0f -` (0x100ab838/3c/40, fsubrs) for the diagonal;
//    * zero stores to +0x0c/+0x1c/+0x2c/+0x30/+0x34/+0x38 and 1.0f
//      (0x3f800000) to +0x3c -- the matrix's fourth row and column.
//  The x87 schedule interleaves the products, so the order of the
//  statements below is the SDK's.
// ======================================================================= //
struct Quat10073400
{
	float	m_Quat[4];                                  // x, y, z, w
};

struct Mat10073400
{
	float	m[4][4];
};

extern "C" void sub_10073400(Quat10073400 *pRot, Mat10073400 *pMat)
{
	float s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;
	const float *pQuat = pRot->m_Quat;

	s = 2.0f / ((pQuat[2] * pQuat[2]) + (pQuat[1] * pQuat[1]) +
		(pQuat[0] * pQuat[0]) + (pQuat[3] * pQuat[3]));

	xs = pQuat[0] * s;
	ys = pQuat[1] * s;
	zs = pQuat[2] * s;

	wx = pQuat[3] * xs;
	wy = pQuat[3] * ys;
	wz = pQuat[3] * zs;

	xx = pQuat[0] * xs;
	xy = pQuat[0] * ys;
	xz = pQuat[0] * zs;

	yy = pQuat[1] * ys;
	yz = pQuat[1] * zs;

	zz = pQuat[2] * zs;

	pMat->m[0][0] = 1.0f - (yy + zz);
	pMat->m[0][1] = xy - wz;
	pMat->m[0][2] = xz + wy;

	pMat->m[1][0] = xy + wz;
	pMat->m[1][1] = 1.0f - (xx + zz);
	pMat->m[1][2] = yz - wx;

	pMat->m[2][0] = xz - wy;
	pMat->m[2][1] = yz + wx;
	pMat->m[2][2] = 1.0f - (xx + yy);

	pMat->m[0][3] = pMat->m[1][3] = pMat->m[2][3] =
		pMat->m[3][0] = pMat->m[3][1] = pMat->m[3][2] = 0.0f;
	pMat->m[3][3] = 1.0f;
}




// ======================================================================= //
//  ltquatbase.cpp's quat_GetVectors, verbatim but for the callee's name
//  and types.  No shipped address: the linker dropped it (nothing calls
//  it).  It is here because it is 0x10073400's same-TU caller, which is
//  what makes icl emit that function's /Qip two-entry pair (see the file
//  header).  /Qip inlines the call, as it may.
// ======================================================================= //
void quat_GetVectors(const float *pQuat, float *pRight, float *pUp, float *pForward)
{
	float mat[4][4];

	sub_10073400((Quat10073400*)pQuat, (Mat10073400*)mat);
	
	pRight[0] = mat[0][0];
	pRight[1] = mat[1][0];
	pRight[2] = mat[2][0];

	pUp[0] = mat[0][1];
	pUp[1] = mat[1][1];
	pUp[2] = mat[2][1];

	pForward[0] = mat[0][2];
	pForward[1] = mat[1][2];
	pForward[2] = mat[2][2];
}


// ======================================================================= //
//  0x10073500, 308 bytes.  __cdecl (dest, q1, q2, t): ltquatbase.cpp's
//  quat_Slerp.  Callers 0x1007dafe (0x1007da80), 0x1007d6d0 (0x1007d624)
//  and 0x1007d90e (0x1007d880), all four dwords in an outgoing area.
//
//  WHAT PINS IT: the dot product's `cosom < 0.0f` (0x100ab884) negating
//  q2 into a local four-float array (`fchs` in a -4..0 counted loop,
//  0x10073554); `1.0 - cosom > 0.00001f` in DOUBLE (0x100ab888 is the
//  double 1.0, 0x100ab890 the float 1e-5 widened); acos at 0x10080230 and
//  three sin at 0x1007fdd0; the linear fallback `1.0f - t` (0x100ab8a4);
//  and the final `pDest[i] = scalerot0*pQ1[i] + scalerot1*rot1q[i]` loop.
//  The double work is what makes icl realign the frame
//  (`and $0xfffffff0,%esp` behind a second, aligned entry at 0x1007350e).
//
//  THE DOT PRODUCT'S TERM ORDER.  The shipped sum runs w, x, z, y.  icl
//  does not emit a four-term sum in source order: the SDK's x, y, z, w
//  comes out w, z, x, y, and z, y, x, w comes out w, x, z, y -- the same
//  rewrite, measured, that makes 0x10073400's normalisation identical.
//
//  IDENTICAL.  It was 4 bytes: only the w and z products of the dot
//  loaded/folded the other way round (shipped `flds pQ1[3] ; fmuls
//  pQ2[3]`, ours `flds pQ2[3] ; fmuls pQ1[3]`, the same for [2]).  icl
//  canonicalises the operand order of each product, and what it sorts on
//  follows the operands' ADDRESS FORMS (as in w2_1000a8a0_os.cpp), not
//  the order they are written in: with each of the eight operands spelled
//  either through the float pointer (pQ1[i] / pQ2[i]) or as the member
//  (pFrom->x / pTo->x), 3 of the 256 spellings are identical -- the one
//  kept (x of pFrom, z and w of pTo as members) and two with more
//  members.  Variable names do not matter (16 pairs measured).
//  The earlier measurements, for the record --
//  changing nothing: all 24 term
//  orders, all 16 operand orders, four parenthesisations, all 720 orders
//  of the local declarations, pQ1/pQ2 declared then assigned, the SDK's
//  own (float*) signature and body verbatim, /Qip and quat_GetVectors in
//  the TU.  Declaring pQ2 before pQ1 swaps which pair is wrong (x and y
//  instead of w and z, 6 bytes), so the choice is icl's, per term.
//  Also unchanged at 4: the SDK's ltquatbase.cpp compiled
//  verbatim as its own TU (4; quat_Slerp alone in a TU is 6, its terms
//  then w, z, x, y), /O1, /G6, /Qrcd; worse: /G5, /Op, /O2, a QDot inline
//  helper over floats/LTQuat*/LTQuat& (212+), the fields read straight
//  off pFrom/pTo (18).  Renaming the file (8..45-character stems) and up
//  to 13 small functions ahead of it change nothing.
// ======================================================================= //
#include <math.h>

struct LTQuat
{
	float  x, y, z, w;
};

void sub_10073500(LTQuat* pDestQ, LTQuat* pFrom, LTQuat* pTo, float t)
{
	const float *pQ1 = &pFrom->x;
	const float *pQ2 = &pTo->x;
	float	rot1q[4];
	float	omega, cosom, sinom;
	float	scalerot0, scalerot1;
	int		i, j;

	// Calculate the cosine
	// (the address forms are the sweep's -- see the note above)
	cosom = pFrom->x*pQ2[0] + pQ1[1]*pQ2[1] + pQ1[2]*pTo->z + pQ1[3]*pTo->w;

	// adjust signs if necessary
	if (cosom < 0.0f)
	{
		cosom = -cosom;
		for (j = 0; j < 4; j++)
			rot1q[j] = -pQ2[j];
	}
	else
	{
		for (j = 0; j < 4; j++)
			rot1q[j] = pQ2[j];
	}

	// calculate interpolating coeffs
	if ((1.0 - cosom) > 0.00001f)
	{
		// standard case
		omega = (float)acos(cosom);
		sinom = (float)sin(omega);
		scalerot0 = (float)sin((1.0 - t) * omega) / sinom;
		scalerot1 = (float)sin(t * omega) / sinom;
	}
	else
	{
		// rot0 and rot1 very close - just do linear interp.
		scalerot0 = 1.0f - t;
		scalerot1 = t;
	}

	for (i = 0; i < 4; i++) // build the new quarternion
		(&pDestQ->x)[i] = (float)(scalerot0 * pQ1[i] + scalerot1 * rot1q[i]);
}
