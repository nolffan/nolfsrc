// *********************************************************************** //
//
//  w17_1006ad20_qip.cpp -- the lightmap lighting TU: 0x1006ad20 (dynamic
//  lights onto a lightmap), 0x1006b310 (one light's setup), 0x1006b500
//  (a dynamic light's animated layer), 0x1006bb30 and 0x1006be50 (the
//  animated-lightmap compositor, moved here from w8_1006bb30_qip.cpp).
//  /O2 /Qip (see build/engine.sh): lea-nop padding, callee saves as
//  frame-slot stores, the non-/Qrcd float-to-int expansion.
//
//  WHY ONE FILE.  The shipped code passes arguments in registers between
//  these five, which /Qip only does inside one translation unit:
//    * 0x1006ad20 (stack entry, called from w6_10039730_qip.cpp) calls
//      0x1006b310's register entry at 0x1006adf5 (%eax/%edx/%ecx, the
//      float radius in %st(0), ten stack slots);
//    * 0x1006b310 is external -- its stack entry is 0x1006b300, `mov
//      0x4/0x8/0xc(%esp)` into %eax/%edx/%ecx and `flds 0x28(%esp)` --
//      and is also called by 0x1006b500 (0x1006b73c) the register way;
//    * 0x1006b500 and 0x1006bb30 have NO stack entry (static): their one
//      caller is 0x1006be50 (0x1006bf87 / 0x1006bfac).
//
//  THE DATA.
//    * the context (argument 1): +0x08 the lightmap animations (0x60-byte
//      records), +0x0c their count.
//    * an animation: +0x20 dynamic (built by 0x1006b500), +0x24 its frame
//      table (per frame, an array of 8-byte compressed-lightmap entries,
//      one per polygon), +0x28 frame count, +0x30 polygon count, +0x34 and
//      +0x38 the two frames being blended, +0x3c the blend, +0x40 the
//      intensity.
//    * the polygon (argument 2): +0x0c / +0x10 its animation references
//      (uint16 animation index, uint16 polygon index within it), +0x14 a
//      flag word, +0x44 its lightmap record (+0x1c the DirectDraw surface,
//      +0x20 a dirty flag), +0x48/+0x49 the lightmap's size, +0x4a/+0x4b
//      its position on the surface.
//    * pixels are 5:5:5, at most 32x32 (the 0x800-byte layers).
//
//  RESULTS (byte comparison against the retail build):
//    * 0x1006b300 / 0x1006b310 -- IDENTICAL (the pair).  The colour stores
//      come out in the shipped order only with the 5:5:5 word written
//      before the three 16.16 channels.
//    * 0x1006c4d0 -- IDENTICAL.
//    * 0x1006ad20 -- near, 1504 bytes against 1499.  Same frame, same
//      slots (the answer is the info block's +0x60, which is what frees the
//      frame's last 8 bytes), same arithmetic; icl here sinks the lit arm
//      of each texel test to the end of the function where the shipped
//      code keeps both arms inline, and decrements the loop counters at
//      the bottom where the shipped code decrements at the top (tried:
//      `while (n--)`, up-counting loops, a goto join, both test polarities).
//    * 0x1006b500 -- near, 1584 against 1642: the same shape plus one
//      extra spill slot, which shifts every frame offset by 8.
//    * 0x1006bb30 / 0x1006be50 -- near, as w8 left them (784 vs 788, 1651
//      vs 1616); 0x1006be50 now reaches 0x1006b500 through registers.
//
//  MulHigh is an __asm helper, inlined: the shipped code spills both
//  operands and the result through memory around a one-operand `imull`
//  at every use, which is what an inlined __asm block looks like
//  (w1_10001000.cpp's RoundF is the precedent); no C spelling of a 32x32
//  high multiply reproduces it.
// *********************************************************************** //

#include "ltbasedefs.h"

extern "C"
{
	void*	__cdecl memcpy(void* pDst, const void* pSrc, unsigned int nBytes);
	void*	__cdecl memset(void* pDst, int c, unsigned int nBytes);
}


// ----------------------------------------------------------------------- //
//  DirectDraw, the pieces used.
// ----------------------------------------------------------------------- //
struct DDSurface;

struct DDSurfaceDesc2
{
	uint32		dwSize;                                 // +0x00
	uint32		dwFlags;                                // +0x04
	uint32		dwHeight;                               // +0x08
	uint32		dwWidth;                                // +0x0c
	int32		lPitch;                                 // +0x10
	uint32		dwBackBufferCount;                      // +0x14
	uint32		dwMipMapCount;                          // +0x18
	uint32		dwAlphaBitDepth;                        // +0x1c
	uint32		dwReserved;                             // +0x20
	void*		lpSurface;                              // +0x24
	uint8		m_pad_28[0x7c - 0x28];
};

struct Rect1006be50
{
	int32	left, top, right, bottom;
};

struct DDSurfaceVtbl
{
	void*	m_pad_00[7];                                // +0x00..+0x18
	long (__stdcall* BltFast)(DDSurface*, uint32 dwX, uint32 dwY,
	                          DDSurface* pSrc, Rect1006be50* pSrcRect,
	                          uint32 dwTrans);          // +0x1c
	void*	m_pad_20[17];                               // +0x20..+0x60
	long (__stdcall* Lock)(DDSurface*, Rect1006be50* pRect,
	                       DDSurfaceDesc2* pDesc, uint32 dwFlags,
	                       void* hEvent);               // +0x64
	void*	m_pad_68[6];                                // +0x68..+0x7c
	long (__stdcall* Unlock)(DDSurface*, Rect1006be50* pRect);  // +0x80
};

struct DDSurface
{
	DDSurfaceVtbl*	lpVtbl;
};

#define DDBLTFAST_WAIT_1006be50		0x10

extern DDSurface*	g_1008da7c;                    // 0x1008da7c scratch


// ----------------------------------------------------------------------- //
//  big3a080.cpp's pixel-format conversion classes, same spellings.
// ----------------------------------------------------------------------- //
struct DDPixelFormat;

class PFormat
{
public:
	virtual uint32  Slot00(uint32 nType, uint32 nAlphaMask, uint32 nRedMask,
	                       uint32 nGreenMask, uint32 nBlueMask);
};

struct ConvertRequest
{
	ConvertRequest* Construct();                        // 0x10079fd0

	PFormat* m_pSrcFormat;                              // +0x00
	void*    m_pSrcData;                                // +0x04
	uint32   m_nSrcPitch;                               // +0x08
	uint32   m_nUnknown0c;                              // +0x0c
	PFormat* m_pDstFormat;                              // +0x10
	void*    m_pDstData;                                // +0x14
	uint32   m_nDstPitch;                               // +0x18
	uint32   m_nWidth;                                  // +0x1c
	uint32   m_nHeight;                                 // +0x20
	uint32   m_nUnknown24;                              // +0x24
	uint8    m_SrcFormat[0x38];                         // +0x28
	uint8    m_DstFormat[0x38];                         // +0x60
};

class FormatMgr
{
public:
	uint32 ConvertPixels(ConvertRequest* pRequest);     // 0x1007a130
};

extern FormatMgr g_1008daa0;                            // 0x1008daa0

extern void sub_10021010(const DDPixelFormat* pPF, PFormat* pFormat);

// 0x1008e6c4 -- the current lightmap surface format's record; its
// DDPIXELFORMAT is at +0x0c (big3a080.cpp's D3DFormatRec).
extern uint32 g_1008e6c4;                               // 0x1008e6c4

// 0x10091220 -- a console variable; its +0x00 is the value as an int.
struct C10091220
{
	int32	m_nValue00;                                 // +0x00
	uint8	m_pad_04[0x20 - 0x04];
};

extern C10091220 g_10091220;                            // 0x10091220


// ----------------------------------------------------------------------- //
//  big0ae20.cpp's light-descriptor pair, same spellings.
// ----------------------------------------------------------------------- //
struct Surf0ae20;

class CLightDesc0ae20
{
public:
	int32	sub_1006d550(Surf0ae20* pSurf, uint32 nA, uint32 nB,
	                     uint32 nC);                    // 0x1006d550
	int32	sub_1006d210(void* pMap);                   // 0x1006d210

	uint16*	m_pData00;                                  // +0x00
	uint32	m_nPitch04;                                 // +0x04
	uint8	m_pad_08[0x1c - 0x08];
};


// ----------------------------------------------------------------------- //
//  The lightmap data.
// ----------------------------------------------------------------------- //
struct LMFrameEntry
{
	uint8*	m_pData;                                    // +0x00
	uint32	m_nSize;                                    // +0x04
};

struct LMAnim
{
	uint8			m_pad_00[0x20];
	uint32			m_bDynamic20;                       // +0x20
	LMFrameEntry**	m_pFrames24;                        // +0x24
	uint32			m_nFrames28;                        // +0x28
	uint32			m_n2c;                              // +0x2c
	uint32			m_nPolies30;                        // +0x30
	uint32			m_iFrameA34;                        // +0x34
	uint32			m_iFrameB38;                        // +0x38
	float			m_fBlend3c;                         // +0x3c
	float			m_fIntensity40;                     // +0x40
	LTVector		m_vPos44;                           // +0x44 dynamic: the light
	float			m_fR50;                             // +0x50
	float			m_fG54;                             // +0x54
	float			m_fB58;                             // +0x58
	float			m_fRadius5c;                        // +0x5c
};

struct LMCtx
{
	uint8			m_pad_00[0x08];
	LMAnim*			m_pAnims08;                         // +0x08
	uint32			m_nAnims0c;                         // +0x0c
	uint8			m_pad_10[0xf8 - 0x10];
	float			m_fTexel_f8;                        // +0xf8 world units per texel
};

struct LMRef
{
	uint16			m_iAnim00;                          // +0x00
	uint16			m_iPoly02;                          // +0x02
};

struct LMRecord
{
	uint8			m_pad_00[0x1c];
	DDSurface*		m_pSurface1c;                       // +0x1c
	uint32			m_bDirty20;                         // +0x20
};

struct LMPoly
{
	uint8			m_pad_00[0x0c];
	LMRef*			m_pRefs0c;                          // +0x0c
	uint32			m_nRefs10;                          // +0x10
	uint16			m_nFlags14;                         // +0x14
	uint8			m_pad_16[0x28 - 0x16];
	class CLMBasis*	m_pBasis28;                         // +0x28
	struct LMLightNode*	m_pLights2c;                    // +0x2c
	uint32			m_n30;                              // +0x30
	LTVector		m_vOrigin34;                        // +0x34
	uint8			m_pad_40[0x44 - 0x40];
	LMRecord*		m_pLM44;                            // +0x44
	uint8			m_nWidth48;                         // +0x48
	uint8			m_nHeight49;                        // +0x49
	uint8			m_nX4a;                             // +0x4a
	uint8			m_nY4b;                             // +0x4b
};



// ----------------------------------------------------------------------- //
//  The dynamic-light pieces.
// ----------------------------------------------------------------------- //

// A polygon's texture-space basis object (LMPoly +0x28): slot 0 hands
// back the two lightmap axis vectors.
class CLMBasis
{
public:
	virtual void	GetAxes(LTVector* pU, LTVector* pV);
};

// The light object a light node points at (+0x04).
struct LMLightObj
{
	uint8			m_pad_00[0x88];
	uint32			m_nFlags88;                         // +0x88
	uint8			m_pad_8c[0x94 - 0x8c];
	uint8			m_nR94;                             // +0x94
	uint8			m_nG95;                             // +0x95
	uint8			m_nB96;                             // +0x96
	uint8			m_pad_97[0x1a8 - 0x97];
	float			m_fRadius1a8;                       // +0x1a8
};

// A polygon's light list (LMPoly +0x2c).
struct LMLightNode
{
	LMLightNode*	m_pNext00;                          // +0x00
	LMLightObj*		m_pObj04;                           // +0x04
	LTVector		m_vPos08;                           // +0x08
};

// What 0x1006b310 fills: the light in the polygon's lightmap space.
struct LMLightInfo
{
	LTVector		m_vOrigin00;                        // +0x00 texel (0,0)
	LTVector		m_vU0c;                             // +0x0c per texel in u
	LTVector		m_vV18;                             // +0x18 per texel in v
	LTVector		m_vLight24;                         // +0x24
	float			m_fDist30;                          // +0x30 squared
	float			m_fRadius34;                        // +0x34 squared
	float			m_fInvRadius38;                     // +0x38 1/radius^2
	uint16*			m_pSrc3c;                           // +0x3c
	uint16*			m_pDst40;                           // +0x40
	uint32			m_nWidth44;                         // +0x44
	uint32			m_nHeight48;                        // +0x48
	int32			m_nSrcPitch4c;                      // +0x4c
	int32			m_nDstPitch50;                      // +0x50
	int32			m_nColor54[3];                      // +0x54 16.16
	uint32			m_bLit60;                           // +0x60 0x1006ad20's answer
	uint16			m_wColor64;                         // +0x64 5:5:5
};

// 0x10091240 -- the 64-step attenuation ramp (16.16), d3dr3b.cpp.
extern int32 g_nRamp10091240[64];                       // 0x10091240

// 0x10091340 -- three 256-entry uint16 tables built by 0x1006c4d0 (all 0,
// 0..255, all 0xff): indexed by a channel + 0xff it is a clamp.
extern uint16 g_10091340[0x300];                        // 0x10091340

// 0x1008cac0 -- set: dynamic lights are drawn flat, unattenuated.
extern uint32 g_1008cac0;                               // 0x1008cac0

// 0x1006a790 -- decompress one lightmap (data, size, out); d3dbatchd.cpp.
extern LTBOOL sub_1006a790(uint8* pData, uint32 nSize, uint16* pOut);

// 0x1006a800 -- decompress an 8-bit lightmap layer; d3dbatchc.cpp.
extern LTBOOL sub_1006a800(const uint8* pSrc, uint32 nSrcLen, uint8* pDst);

// The high dword of a 32x32 signed multiply.  The shipped code stores
// both operands to their own slots, `mov a,%eax ; imull b ; mov %edx,r ;
// mov r,%eax` at every use -- an __asm helper inlined (the same shape as
// the RoundF helper in w1_10001000.cpp, and the reason these functions
// carry an %ebp frame).
inline int32 MulHigh(int32 a, int32 b)
{
	int32 r;
	__asm mov  eax, a
	__asm imul b
	__asm mov  r, edx
	return r;
}


// ======================================================================= //
//  0x1006ad20  1504 bytes (+ the 16-byte stack entry of 0x1006b310).
//  Light a polygon's lightmap with its dynamic lights: for each light
//  node, 0x1006b310 sets up the light in lightmap space, then every texel
//  within the radius is lit -- flat (g_1008cac0, light flag 0x10, or no
//  source) or attenuated through the ramp -- from the source into the
//  destination.  After the first light the destination is the source.
//  No lights: the source is copied.  The answer: whether any texel was
//  lit.
// ======================================================================= //
extern "C" uint32 sub_1006ad20(LMCtx* pCtx, LMPoly* pPoly, uint16* pSrc,
                               int32 nSrcPitch, uint32 nWidth,
                               uint32 nHeight, uint16* pDst,
                               int32 nDstPitch);

void sub_1006b300(LMCtx* pCtx, LMPoly* pPoly, LTVector* pPos,
                  uint16* pSrc, int32 nSrcPitch, uint32 nWidth,
                  uint32 nHeight, uint16* pDst, int32 nDstPitch,
                  float fRadius, int32 nR, int32 nG, int32 nB,
                  LMLightInfo* pInfo);

extern "C" uint32 sub_1006ad20(LMCtx* pCtx, LMPoly* pPoly, uint16* pSrc,
                               int32 nSrcPitch, uint32 nWidth,
                               uint32 nHeight, uint16* pDst,
                               int32 nDstPitch)
{
	LMLightInfo		info;
	LMLightNode*	pNode;
	LMLightObj*		pObj;
	uint32			x, y;
	uint16*			pS;
	uint16*			pD;
	uint16*			pRowS;
	uint16*			pRowD;
	float			rx, ry, rz;
	float			px, py, pz;
	float			fDist;
	int32			nAtten;
	int32			c[3];

	info.m_pSrc3c = pSrc;
	info.m_nSrcPitch4c = nSrcPitch;
	info.m_bLit60 = 0;

	if (!pPoly->m_pLights2c)
	{
		memcpy(pDst, pSrc, nWidth * nHeight * 2);
		return info.m_bLit60;
	}

	for (pNode = pPoly->m_pLights2c; pNode; pNode = pNode->m_pNext00)
	{
		pObj = pNode->m_pObj04;
		sub_1006b300(pCtx, pPoly, &pNode->m_vPos08, pSrc, nSrcPitch,
		             nWidth, nHeight, pDst, nDstPitch, pObj->m_fRadius1a8,
		             pObj->m_nR94, pObj->m_nG95, pObj->m_nB96, &info);

		if (g_1008cac0 || (pObj->m_nFlags88 & 0x10) || !info.m_pSrc3c)
		{
			rx = info.m_vOrigin00.x;
			ry = info.m_vOrigin00.y;
			rz = info.m_vOrigin00.z;
			pRowS = info.m_pSrc3c;
			pRowD = info.m_pDst40;
			for (y = info.m_nHeight48; y; y--)
			{
				px = rx;
				py = ry;
				pz = rz;
				pS = pRowS;
				pD = pRowD;
				for (x = info.m_nWidth44; x; x--)
				{
					info.m_fDist30 = (float)((px - info.m_vLight24.x) * (px - info.m_vLight24.x) +
					                         (py - info.m_vLight24.y) * (py - info.m_vLight24.y) +
					                         (pz - info.m_vLight24.z) * (pz - info.m_vLight24.z));
					if (info.m_fDist30 >= info.m_fRadius34)
					{
						*pD = *pS;
					}
					else
					{
						*pD = info.m_wColor64;
						info.m_bLit60 = 1;
					}

					px += info.m_vU0c.x;
					py += info.m_vU0c.y;
					pz += info.m_vU0c.z;
					pS++;
					pD++;
				}

				rx += info.m_vV18.x;
				ry += info.m_vV18.y;
				rz += info.m_vV18.z;
				pRowS += info.m_nSrcPitch4c >> 1;
				pRowD += info.m_nDstPitch50 >> 1;
			}
		}
		else
		{
			rx = info.m_vOrigin00.x;
			ry = info.m_vOrigin00.y;
			rz = info.m_vOrigin00.z;
			pRowS = info.m_pSrc3c;
			pRowD = info.m_pDst40;
			for (y = info.m_nHeight48; y; y--)
			{
				px = rx;
				py = ry;
				pz = rz;
				pS = pRowS;
				pD = pRowD;
				for (x = info.m_nWidth44; x; x--)
				{
					info.m_fDist30 = (float)((px - info.m_vLight24.x) * (px - info.m_vLight24.x) +
					                         (py - info.m_vLight24.y) * (py - info.m_vLight24.y) +
					                         (pz - info.m_vLight24.z) * (pz - info.m_vLight24.z));
					if (info.m_fDist30 >= info.m_fRadius34)
					{
						*pD = *pS;
					}
					else
					{
						nAtten = g_nRamp10091240[(int32)((1.0f - info.m_fDist30 * info.m_fInvRadius38) * 63.0f)];
						c[0] = ((*pS & 0x7c00) >> 7) + MulHigh(nAtten, info.m_nColor54[0]);
						c[1] = ((*pS & 0x3e0) >> 2) + MulHigh(nAtten, info.m_nColor54[1]);
						c[2] = ((*pS & 0x1f) << 3) + MulHigh(nAtten, info.m_nColor54[2]);
						*pD = (uint16)(((g_10091340[c[0] + 0xff] << 7) & 0x7c00) |
						               ((g_10091340[c[1] + 0xff] << 2) & 0x3e0) |
						               (g_10091340[c[2] + 0xff] >> 3));
						info.m_bLit60 = 1;
					}

					px += info.m_vU0c.x;
					py += info.m_vU0c.y;
					pz += info.m_vU0c.z;
					pS++;
					pD++;
				}

				rx += info.m_vV18.x;
				ry += info.m_vV18.y;
				rz += info.m_vV18.z;
				pRowS += info.m_nSrcPitch4c >> 1;
				pRowD += info.m_nDstPitch50 >> 1;
			}
		}

		info.m_nSrcPitch4c = info.m_nDstPitch50;
		info.m_pSrc3c = info.m_pDst40;
	}

	return info.m_bLit60;
}


// ======================================================================= //
//  0x1006b310  496 bytes; external, so /Qip gives it the two-entry pair
//  (0x1006b300 loads %eax/%edx/%ecx and the float from 0x28(%esp)).  Set
//  up one light in the polygon's lightmap space: texel origin, the u and
//  v steps (the basis axes scaled by the texel size over their squared
//  length), the light position, radius^2 and its inverse, the surface,
//  and the colour both as 16.16 offsets from full (2c - 255) and as
//  5:5:5.  The source surface arguments are not read.
// ======================================================================= //
void sub_1006b300(LMCtx* pCtx, LMPoly* pPoly, LTVector* pPos,
                  uint16* pSrc, int32 nSrcPitch, uint32 nWidth,
                  uint32 nHeight, uint16* pDst, int32 nDstPitch,
                  float fRadius, int32 nR, int32 nG, int32 nB,
                  LMLightInfo* pInfo)
{
	LTVector	vU, vV;
	int32		aFull[3];
	float		fInv;

	pPoly->m_pBasis28->GetAxes(&vU, &vV);

	pInfo->m_vLight24 = *pPos;
	pInfo->m_vOrigin00 = pPoly->m_vOrigin34;

	fInv = 1.0f / vU.Mag();
	pInfo->m_vU0c = vU * (pCtx->m_fTexel_f8 * fInv * fInv);

	fInv = 1.0f / vV.Mag();
	pInfo->m_vV18 = vV * (pCtx->m_fTexel_f8 * fInv * fInv);

	pInfo->m_fRadius34 = fRadius * fRadius;
	pInfo->m_fInvRadius38 = 1.0f / pInfo->m_fRadius34;

	pInfo->m_wColor64 = (uint16)((((uint16)nR << 7) & 0x7c00) |
	                             (((uint16)nG << 2) & 0x3e0) |
	                             ((uint16)nB >> 3));
	pInfo->m_nColor54[0] = nR << 16;
	pInfo->m_nColor54[1] = nG << 16;
	pInfo->m_nColor54[2] = nB << 16;

	aFull[0] = 0xff0000 - pInfo->m_nColor54[0];
	aFull[1] = 0xff0000 - pInfo->m_nColor54[1];
	aFull[2] = 0xff0000 - pInfo->m_nColor54[2];
	pInfo->m_nColor54[0] -= aFull[0];
	pInfo->m_nColor54[1] -= aFull[1];
	pInfo->m_nColor54[2] -= aFull[2];

	pInfo->m_pDst40 = pDst;
	pInfo->m_nWidth44 = nWidth;
	pInfo->m_nHeight48 = nHeight;
	pInfo->m_nDstPitch50 = nDstPitch;
}


// ======================================================================= //
//  0x1006b500  1584 bytes (register ABI: %eax, %edx, %ecx, then two stack
//  arguments; static, only 0x1006be50 calls it).  A dynamic light's layer
//  of an animated lightmap: the 8-bit intensity layer (frame A, frame B
//  or the two blended, as 0x1006bb30 does for colour), then lit by the
//  light through 0x1006b310's setup -- intensity x ramp x colour per
//  texel inside the radius, black outside.
// ======================================================================= //
// PL = aLayer is set BEFORE the frame choice and the three
// decompress calls write through it -- the shipped code holds &aLayer in
// %edi across all three (`lea -0xd50(%ebp),%edi` at 0x1006b527).  Pairs as
// near (829 of 1524 bytes positionally equal, was 89).  LEFT: frame 0xdb0
// against 0xdb8 and a permutation of %ebx/%esi/%edi.
static LTBOOL sub_1006b500(LMCtx* pCtx, LMPoly* pPoly, LMAnim* pAnim,
                           LMRef* pRef, uint16* pOut)
{
	uint8			aLayer[0x400];
	uint8			aB[0x400];
	uint8			aA[0x400];
	LMLightInfo		info;
	uint32			iA, iB;
	LMFrameEntry*	pA;
	LMFrameEntry*	pB;
	int32			nBlend;
	uint32			nLeft;
	uint8*			pSrcA;
	uint8*			pSrcB;
	uint8*			pL;
	uint32			x, y;
	uint16*			pD;
	uint16*			pRowD;
	float			rx, ry, rz;
	float			px, py, pz;
	uint32			nI;
	int32			c[3];
	LTBOOL			bOK;

	pL = aLayer;
	iA = pAnim->m_iFrameA34;
	iB = pAnim->m_iFrameB38;

	if (iA == iB)
	{
		pA = &pAnim->m_pFrames24[iA][pRef->m_iPoly02];
		bOK = sub_1006a800(pA->m_pData, pA->m_nSize, pL);
	}
	else if (pAnim->m_fBlend3c < 0.03125f)
	{
		pA = &pAnim->m_pFrames24[iA][pRef->m_iPoly02];
		bOK = sub_1006a800(pA->m_pData, pA->m_nSize, pL);
	}
	else if (pAnim->m_fBlend3c > 0.96875f)
	{
		pB = &pAnim->m_pFrames24[iB][pRef->m_iPoly02];
		bOK = sub_1006a800(pB->m_pData, pB->m_nSize, pL);
	}
	else
	{
		pA = &pAnim->m_pFrames24[iA][pRef->m_iPoly02];
		pB = &pAnim->m_pFrames24[iB][pRef->m_iPoly02];

		if (!sub_1006a800(pA->m_pData, pA->m_nSize, aA))
			return LTFALSE;

		if (!sub_1006a800(pB->m_pData, pB->m_nSize, aB))
			return LTFALSE;

		nBlend = (int32)(pAnim->m_fBlend3c * 64.0f);

		pSrcA = aA;
		pSrcB = aB;
		for (nLeft = pPoly->m_nWidth48 * pPoly->m_nHeight49; nLeft; )
		{
			nLeft--;
			*pL = (uint8)(*pSrcA + (((*pSrcB - *pSrcA) * nBlend) >> 6));
			pSrcA++;
			pSrcB++;
			pL++;
		}

		bOK = LTTRUE;
	}

	if (!bOK)
		return LTFALSE;

	sub_1006b300(pCtx, pPoly, (LTVector*)&pAnim->m_vPos44, pOut,
	             pPoly->m_nWidth48 * 2, pPoly->m_nWidth48,
	             pPoly->m_nHeight49, pOut, pPoly->m_nWidth48 * 2,
	             pAnim->m_fRadius5c, (int32)pAnim->m_fR50,
	             (int32)pAnim->m_fG54, (int32)pAnim->m_fB58, &info);

	rx = info.m_vOrigin00.x;
	ry = info.m_vOrigin00.y;
	rz = info.m_vOrigin00.z;
	pL = aLayer;
	pRowD = pOut;
	for (y = info.m_nHeight48; y; )
	{
		y--;
		px = rx;
		py = ry;
		pz = rz;
		pD = pRowD;
		for (x = info.m_nWidth44; x; )
		{
			x--;
			info.m_fDist30 = (float)((px - info.m_vLight24.x) * (px - info.m_vLight24.x) +
			                         (py - info.m_vLight24.y) * (py - info.m_vLight24.y) +
			                         (pz - info.m_vLight24.z) * (pz - info.m_vLight24.z));
			nI = *pL;
			if (nI && info.m_fDist30 < info.m_fRadius34)
			{
				nI = (nI * g_nRamp10091240[(int32)((1.0f - info.m_fInvRadius38 * info.m_fDist30) * 63.0f)]) >> 8;
				c[0] = MulHigh(nI, info.m_nColor54[0]);
				c[1] = MulHigh(nI, info.m_nColor54[1]);
				c[2] = MulHigh(nI, info.m_nColor54[2]);
				*pD = (uint16)(((g_10091340[c[0] + 0xff] << 7) & 0x7c00) |
				               ((g_10091340[c[1] + 0xff] << 2) & 0x3e0) |
				               (g_10091340[c[2] + 0xff] >> 3));
			}
			else
			{
				*pD = 0;
			}

			px += info.m_vU0c.x;
			py += info.m_vU0c.y;
			pz += info.m_vU0c.z;
			pL++;
			pD++;
		}

		rx += info.m_vV18.x;
		ry += info.m_vV18.y;
		rz += info.m_vV18.z;
		pRowD += info.m_nDstPitch50 >> 1;
	}

	return LTTRUE;
}


// ======================================================================= //
//  0x1006bb30  789 bytes (register ABI: %eax, %edx, %ecx, then one stack
//  argument).  One polygon's layer of an animated lightmap: frame A, frame
//  B, or -- when the blend is strictly between 1/32 and 31/32 -- the two
//  decompressed and mixed per channel as A + ((B - A) * blend*64 >> 6),
//  clamped at 31.
// ======================================================================= //
// The two frames are ONE uint16[2][0x400] (the shipped
// pSrcB is `lea 0x10(%esp),%eax ; add $0x800,%eax`) and the two decompress
// failures one `||`: 784 -> 800 bytes with the padding, pairs as near.
// LEFT: pPoly kept in %edi and the count in %esi as shipped.
static LTBOOL sub_1006bb30(LMPoly* pPoly, LMAnim* pAnim, LMRef* pRef,
                           uint16* pOut)
{
	uint16			aFrame[2][0x400];
	uint32			iPoly;
	uint32			iA, iB;
	LMFrameEntry*	pA;
	LMFrameEntry*	pB;
	int32			nBlend;
	uint32			nLeft;
	uint16*			pSrcA;
	uint16*			pSrcB;
	int32			aC[2][3];
	int32			r, g, b;

	iPoly = pRef->m_iPoly02;
	if (iPoly >= pAnim->m_nPolies30)
		return LTFALSE;

	iA = pAnim->m_iFrameA34;
	iB = pAnim->m_iFrameB38;

	if (iA == iB)
	{
		pA = &pAnim->m_pFrames24[iA][iPoly];
		return sub_1006a790(pA->m_pData, pA->m_nSize, pOut);
	}

	if (pAnim->m_fBlend3c < 0.03125f)
	{
		pA = &pAnim->m_pFrames24[iA][iPoly];
		return sub_1006a790(pA->m_pData, pA->m_nSize, pOut);
	}

	if (pAnim->m_fBlend3c > 0.96875f)
	{
		pB = &pAnim->m_pFrames24[iB][iPoly];
		return sub_1006a790(pB->m_pData, pB->m_nSize, pOut);
	}

	pA = &pAnim->m_pFrames24[iA][iPoly];
	pB = &pAnim->m_pFrames24[iB][iPoly];

	if (!sub_1006a790(pA->m_pData, pA->m_nSize, aFrame[0]) ||
	    !sub_1006a790(pB->m_pData, pB->m_nSize, aFrame[1]))
		return LTFALSE;

	nBlend = (int32)(pAnim->m_fBlend3c * 64.0f);

	pSrcA = aFrame[0];
	pSrcB = aFrame[1];
	for (nLeft = pPoly->m_nWidth48 * pPoly->m_nHeight49; nLeft; nLeft--)
	{
		aC[0][0] = (*pSrcA & 0x7c00) >> 10;
		aC[0][1] = (*pSrcA & 0x3e0) >> 5;
		aC[0][2] = *pSrcA & 0x1f;
		aC[1][0] = (*pSrcB & 0x7c00) >> 10;
		aC[1][1] = (*pSrcB & 0x3e0) >> 5;
		aC[1][2] = *pSrcB & 0x1f;

		r = aC[0][0] + (((aC[1][0] - aC[0][0]) * nBlend) >> 6);
		if (r > 0x1f)
			r = 0x1f;
		g = aC[0][1] + (((aC[1][1] - aC[0][1]) * nBlend) >> 6);
		if (g > 0x1f)
			g = 0x1f;
		b = aC[0][2] + (((aC[1][2] - aC[0][2]) * nBlend) >> 6);
		if (b > 0x1f)
			b = 0x1f;

		*pOut = (uint16)((r << 10) | (g << 5) | b);

		pSrcA++;
		pSrcB++;
		pOut++;
	}

	return LTTRUE;
}


// ======================================================================= //
//  0x1006be50  1616 bytes.  Composite a polygon's lightmap layers.
//
//  Every usable layer (a valid pair of frames, intensity at least 0.02)
//  is built into a 32x32 scratch by 0x1006b500 or 0x1006bb30; the first
//  one is copied into the accumulator, later ones are scaled by their
//  intensity (x64, >> 6) and added per channel with a clamp at 31.  No
//  usable layer: the accumulator is cleared.  Then either
//    * the "dynamic" console variable (0x10091220) or the caller's flag is
//      set: convert the accumulator from 5:5:5 into the scratch surface
//      0x1008da7c (Lock / 0x1007a130 / Unlock) and BltFast it into the
//      polygon's place on its lightmap surface; the answer is BltFast's
//      success; or
//    * a layer past the first was used: convert straight into the
//      lightmap through the light descriptor (0x1006d550 / 0x1006d210),
//      clear the record's dirty flag and set bit 15 of the polygon's flag
//      word; or
//    * otherwise just mark the record dirty.
// ======================================================================= //
LTBOOL sub_1006be50(void* pCtxArg, void* pPolyArg, uint32 bForce)
{
	LMCtx*			pCtx = (LMCtx*)pCtxArg;
	LMPoly*			pPoly = (LMPoly*)pPolyArg;
	ConvertRequest	req;
	uint16			aLayer[0x400];
	uint16			aAccum[0x400];
	uint32			aC[3][3];
	CLightDesc0ae20	desc;
	DDSurfaceDesc2	ddsd;
	Rect1006be50	rcSrc;
	LMRecord*		pLM;
	LMRef*			pRef;
	LMAnim*			pAnim;
	uint32			nPixels, nBytes;
	uint32			bDynamic;
	uint32			bLayered;
	uint32			nDone;
	uint32			i;
	uint32			nScale;
	uint32			nLeft;
	uint16*			pSrc;
	uint16*			pDst;
	LTBOOL			bOK;

	req.Construct();

	pLM = pPoly->m_pLM44;
	if (!pLM || !g_1008da7c)
		return LTFALSE;

	pLM->m_bDirty20 = 1;

	if (!pPoly->m_nRefs10)
		return LTTRUE;

	nPixels = pPoly->m_nWidth48 * pPoly->m_nHeight49;
	nBytes = nPixels * 2;
	bDynamic = (g_10091220.m_nValue00 != 0) | bForce;

	nDone = 0;
	bLayered = 0;
	i = 0;
	do
	{
		pRef = &pPoly->m_pRefs0c[i];
		if (pRef->m_iAnim00 >= pCtx->m_nAnims0c)
			return LTFALSE;

		pAnim = &pCtx->m_pAnims08[pRef->m_iAnim00];

		if (pAnim->m_iFrameA34 >= pAnim->m_nFrames28 ||
		    pAnim->m_iFrameB38 >= pAnim->m_nFrames28)
			continue;

		if (pAnim->m_fIntensity40 < 0.02f)
			continue;

		if (pAnim->m_bDynamic20)
			bOK = sub_1006b500(pCtx, pPoly, pAnim, pRef, aLayer);
		else
			bOK = sub_1006bb30(pPoly, pAnim, pRef, aLayer);

		if (!bOK)
			return LTFALSE;

		bLayered = (i > 0);

		if (!nDone)
		{
			memcpy(aAccum, aLayer, nBytes);
		}
		else
		{
			nScale = (int32)(pAnim->m_fIntensity40 * 64.0f);

			pSrc = aLayer;
			pDst = aAccum;
			for (nLeft = nPixels; nLeft; nLeft--)
			{
				aC[0][0] = (((*pSrc & 0x7c00) >> 10) * nScale) >> 6;
				aC[0][1] = (((*pSrc & 0x3e0) >> 5) * nScale) >> 6;
				aC[0][2] = ((*pSrc & 0x1f) * nScale) >> 6;
				aC[1][0] = (*pDst & 0x7c00) >> 10;
				aC[1][1] = (*pDst & 0x3e0) >> 5;
				aC[1][2] = *pDst & 0x1f;

				aC[2][0] = aC[1][0] + aC[0][0];
				if (aC[2][0] > 0x1f)
					aC[2][0] = 0x1f;
				aC[2][1] = aC[0][1] + aC[1][1];
				if (aC[2][1] > 0x1f)
					aC[2][1] = 0x1f;
				aC[2][2] = aC[1][2] + aC[0][2];
				if (aC[2][2] > 0x1f)
					aC[2][2] = 0x1f;

				*pDst = (uint16)((aC[2][0] << 10) | (aC[2][1] << 5) | aC[2][2]);

				pSrc++;
				pDst++;
			}
		}

		nDone++;
	} while (++i < pPoly->m_nRefs10);

	if (!nDone)
		memset(aAccum, 0, nBytes);

	if (bDynamic)
	{
		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);

		if (g_1008da7c->lpVtbl->Lock(g_1008da7c, LTNULL, &ddsd, 0,
		                                  LTNULL) != 0)
			return LTFALSE;

		req.m_pSrcFormat->Slot00(2, 0, 0x7c00, 0x3e0, 0x1f);
		req.m_pSrcData = aAccum;
		req.m_nSrcPitch = pPoly->m_nWidth48 * 2;
		sub_10021010((const DDPixelFormat*)(g_1008e6c4 + 0xc), req.m_pDstFormat);
		req.m_nDstPitch = ddsd.lPitch;
		req.m_pDstData = ddsd.lpSurface;
		req.m_nWidth = pPoly->m_nWidth48;
		req.m_nHeight = pPoly->m_nHeight49;
		g_1008daa0.ConvertPixels(&req);

		g_1008da7c->lpVtbl->Unlock(g_1008da7c, LTNULL);

		rcSrc.left = 0;
		rcSrc.top = 0;
		rcSrc.right = pPoly->m_nWidth48;
		rcSrc.bottom = pPoly->m_nHeight49;

		return pLM->m_pSurface1c->lpVtbl->BltFast(pLM->m_pSurface1c,
		                                          pPoly->m_nX4a, pPoly->m_nY4b,
		                                          g_1008da7c, &rcSrc,
		                                          DDBLTFAST_WAIT_1006be50) == 0;
	}

	if (!bLayered)
	{
		pLM->m_bDirty20 = 1;
		return LTTRUE;
	}

	if (!desc.sub_1006d550((Surf0ae20*)pPoly, 0, 0, 0))
		return LTFALSE;

	req.m_pSrcFormat->Slot00(2, 0, 0x7c00, 0x3e0, 0x1f);
	req.m_pSrcData = aAccum;
	req.m_nSrcPitch = pPoly->m_nWidth48 * 2;
	sub_10021010((const DDPixelFormat*)(g_1008e6c4 + 0xc), req.m_pDstFormat);
	req.m_nDstPitch = desc.m_nPitch04;
	req.m_pDstData = desc.m_pData00;
	req.m_nWidth = pPoly->m_nWidth48;
	req.m_nHeight = pPoly->m_nHeight49;
	g_1008daa0.ConvertPixels(&req);

	pLM->m_bDirty20 = 0;
	pPoly->m_nFlags14 |= 0x8000;

	return desc.sub_1006d210((void*)1);
}


// ======================================================================= //
//  0x1006c4d0 -- 49 bytes.  Build the clamp table 0x1006ad20 and
//  0x1006b500 index with a channel + 0xff: 256 zeros, the identity, 256
//  x 0xff.  Called once from d3dqip2.cpp's 0x1003ad30.
// ======================================================================= //
void sub_1006c4d0()
{
	int32	i;

	for (i = 0; i < 0x100; i++)
	{
		g_10091340[i] = 0;
		g_10091340[0x100 + i] = (uint16)i;
		g_10091340[0x200 + i] = 0xff;
	}
}
