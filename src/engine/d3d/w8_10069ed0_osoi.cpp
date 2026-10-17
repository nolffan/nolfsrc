// *********************************************************************** //
//
//  w8_10069ed0_osoi.cpp -- 0x10069ed0, a vector perpendicular to another.
//  /Os /Oi- (see build/engine.sh).
//
//  WHY THESE FLAGS.  It pads with `lea 0x0(%esi),%esi` and 0x90s (the /Os
//  pad), and every magnitude is `fstpl (%esp) ; call 0x1007fc98` -- the
//  CRT sqrt CALLED, which is /Oi-.
//
//  NEAR, AND WHY.  The shipped function opens with /Qip's external-linkage
//  stack entry (`mov 0x4/0x8/0xc(%esp)` into %eax/%edx/%ecx, twelve bytes)
//  falling into a register body at 0x10069edc -- yet nothing in the image
//  calls 0x10069edc (the one caller, big48180.cpp's 0x10048776, enters at
//  0x10069ed0 through the stack) and no pointer to either address is
//  stored anywhere.  Measured: under /Os /Oi- /Qip icl emits the pair only
//  when the file holds a caller; alone, with an unused inline or static
//  caller, or next to w8_10069ca0_osoiqip.cpp's pair, it emits one plain
//  entry.  So the shim cannot be produced without inventing a caller, and
//  this file is built without /Qip.  The body also keeps the input's
//  components in x87 registers across the not-taken arms where the
//  shipped code reloads them (tried: const-reference Dot, VEC_DOT, the
//  header's by-value Dot -- the last is closest, 783 bytes against 713).
//
//  WHAT IT DOES (big48180.cpp's caller: `sub_10069ed0(&vNormal, LTNULL,
//  &vRight)`):
//    * the hint defaults to a local (0, 1, 0) when null (0x1006a18a), and
//      the output starts as a copy of it (`rep movsl`, three dwords);
//    * three times over: when |Dot(in, out)| > 0.99 the output is nudged
//      -- z += 5; then x += 5, y += 5; then x += 5, y -= 2, z -= 5 -- and
//      renormalised (ltvector.h's Norm: `if (mag == 0) return; inv =
//      1 / mag`);
//    * then the input's component is removed, out += in * -Dot(in, out),
//      and the result normalised once more.
//  The 0.99 tests are `0.99 < d` first and `d < -0.99` in a sunk block,
//  i.e. `d > 0.99f || d < -0.99f`.
// *********************************************************************** //

#include "ltbasedefs.h"
#include <math.h>

// ltvector.h's _CVector<float>, the members used, spelled under the tag
// the caller's declaration (big48180.cpp) names.
struct LTVec
{
	LTVec() {}
	LTVec(float mx, float my, float mz) { x = mx; y = my; z = mz; }

	float	Mag() const { return (float)sqrt((x * x) + (y * y) + (z * z)); }
	float	Dot(LTVec v) const { return (x * v.x) + (y * v.y) + (z * v.z); }
	void	Norm(float nVal = 1.0f);

	LTVec	operator * (float v) const { return LTVec(x * v, y * v, z * v); }
	void	operator += (const LTVec v) { x += v.x; y += v.y; z += v.z; }

	float	x;                                          // +0x00
	float	y;                                          // +0x04
	float	z;                                          // +0x08
};

inline void LTVec::Norm(float nVal)
{
	float inv;
	float mag = Mag();

	if (mag == 0.0f)
		return;

	inv = nVal / mag;
	x = x * inv;
	y = y * inv;
	z = z * inv;
}


// ======================================================================= //
//  0x10069ed0  728 bytes (12-byte stack entry + register body)
// ======================================================================= //
void sub_10069ed0(LTVec* pIn, LTVec* pHint, LTVec* pOut)
{
	LTVec	vUp;
	volatile float	fDot;

	if (!pHint)
	{
		vUp.x = 0.0f;
		vUp.y = 1.0f;
		vUp.z = 0.0f;
		pHint = &vUp;
	}

	*pOut = *pHint;

	fDot = pIn->Dot(*pOut);
	if (fDot > 0.99f || fDot < -0.99f)
	{
		pOut->z += 5.0f;
		pOut->Norm();
	}

	fDot = pIn->Dot(*pOut);
	if (fDot > 0.99f || fDot < -0.99f)
	{
		pOut->x += 5.0f;
		pOut->y += 5.0f;
		pOut->Norm();
	}

	fDot = pIn->Dot(*pOut);
	if (fDot > 0.99f || fDot < -0.99f)
	{
		pOut->x += 5.0f;
		pOut->y -= 2.0f;
		pOut->z -= 5.0f;
		pOut->Norm();
	}

	*pOut += *pIn * -pIn->Dot(*pOut);
	pOut->Norm();
}
