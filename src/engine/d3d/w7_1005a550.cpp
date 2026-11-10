// ======================================================================= //
//  w7_1005a550.cpp -- d3d.ren functions written for the whole-DLL link
//.  /O2 (lea padding).  On its own because icl's ordering of
//  the w row's terms moved with unrelated declarations elsewhere in a
//  shared file.
// ======================================================================= //
#include "ltbasedefs.h"


// ======================================================================= //
//  0x1005a550 -- 173 bytes (then a lea-nop).  big5a960.cpp's in-place
//  homogeneous transform of the polygrid vertices through the context's
//  +0x15c matrix.  d3dr3c.cpp's 0x1005fac0 is the same source over a
//  32-byte vertex (byte-identical there); here the stride is 0x28 and the
//  w row is associated differently: the shipped code sums m30*x and m32*z
//  first and adds m31*y last, where the three xyz rows (and every row of
//  0x1005fac0) take m?2*z, then m?1*y, then m?0*x -- icl emits a sum's
//  terms from the last backwards, so the w row's source order would be
//  y, z, x.
//
//  NEAR (173 = 173 bytes, 16 differ in this file; 25 and then 51 when it
//  shared w7_10040cc0.cpp with other functions): only the w row's three
//  product terms come out in our z, y, x order.  Measured and changing
//  nothing: every order of the three terms, every parenthesisation, the
//  product operands swapped, and ltmatrix.h's MatVMul_H in the loop
//  (that one is worse, 178 bytes), the struct declarations swapped, the
//  divisor as its own statement, no ltbasedefs.h.  icl orders the sum
//  itself, by something outside the function's own source.
//  Confirmed: header includes (<math.h>, <stdio.h>, <windows.h>...)
//  change nothing, but ANY function compiled ahead of this one in the
//  TU does (one small unrelated float function before it: 16 -> 20
//  bytes), so the order is TU history -- the shipped TU's other
//  functions (0x1005a390..0x1005a630, now spread over d3dr2c, d3dr4a,
//  d3dbatchd, d3dr5d, big5a960 ...) would be the lever, not this body.
//  0x1005a410 (d3dr5d.cpp) and 0x1005a490 (d3dr3c.cpp),
//  the /O2 neighbours, compiled ahead of it in either order: still 16
//  (both stay identical).  The term order is not the source's -- all six
//  orders of the w row give identical bytes at any one TU state -- and
//  over 160 preambles of one-line functions the residual runs through
//  10, 11, 16, 20, 27, 31..53 with a period of about 34 functions, never
//  0 (best 10, at 62/101/135).  It also moves with the LENGTH of the
//  source file's name (a 55-character name changed a test TU's row order
//  where 5..12 did not), so icl's sum order is keyed by heap state:
//  measured on this file, stems of 12..43 characters all give 16.
//
//  IDENTICAL.  The lever inside this body is the operands' ADDRESS
//  FORMS (w2_1000a8a0_os.cpp's finding): with each w-row matrix term
//  spelled pMat->m[3][c] or f[12+c] through a float pointer, and each
//  vertex component as the member or v[c], 3 of 256 spellings are
//  identical; the one kept reads only m[3][0] through the pointer, in
//  the natural x, y, z term order.  The pointer is declared in the
//  loop body: declared ahead of the loop the same row is 12 bytes off.
// ======================================================================= //
struct Mat1005a550
{
	float m[4][4];
};

struct Vert1005a550                                     // 40 bytes
{
	float  x, y, z;
	uint32 m_pad_0c[7];
};

extern "C" void sub_1005a550(Vert1005a550* pVerts, uint32 nCount,
                             Mat1005a550* pMat)
{
	Vert1005a550* pV = pVerts;

	for (; nCount; --nCount, ++pV)
	{
		const float* f = &pMat->m[0][0];
		float oow = 1.0f / (f[12]*pV->x + pMat->m[3][1]*pV->y +
		                    pMat->m[3][2]*pV->z + pMat->m[3][3]);

		float fX = (pMat->m[0][0]*pV->x + pMat->m[0][1]*pV->y +
		            pMat->m[0][2]*pV->z + pMat->m[0][3]) * oow;
		float fY = (pMat->m[1][0]*pV->x + pMat->m[1][1]*pV->y +
		            pMat->m[1][2]*pV->z + pMat->m[1][3]) * oow;
		float fZ = (pMat->m[2][0]*pV->x + pMat->m[2][1]*pV->y +
		            pMat->m[2][2]*pV->z + pMat->m[2][3]) * oow;

		pV->x = fX;
		pV->y = fY;
		pV->z = fZ;
	}
}
