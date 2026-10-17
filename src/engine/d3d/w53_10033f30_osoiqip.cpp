// *********************************************************************** //
//
//  MODULE:   W53_10033F30_OSOIQIP (.CPP)  --  d3dblit.cpp's optimized-
//            surface family, 0x10033f30 .. 0x10034922, as ONE /Os /Oi- /Qip
//            translation unit (was w5_10033f30_osoi.cpp and
//            w81_10034510_os.cpp):
//
//              0x10033f30  (104)   drop the tiles (DeleteSurface's helper)
//              0x10033fa0 / 0x10033fac  (12 + 1376)  fill the tiles -- a
//                                  /Qip two-entry pair, written sub_10033fa0
//              0x10034510  (108)   UnoptimizeSurface, slot +0xe0
//              0x10034580  (930)   OptimizeSurface, slot +0xdc
//
//  WHY ONE TU.  OptimizeSurface calls the fill at its /Qip REGISTER entry
//  0x10033fac (`add $-0x10,%esp ; mov %esi,%eax ; call`), and the stack
//  entry 0x10033fa0 sits in the 124 bytes the enumeration gives 0x10033f30
//  (0x10033f30's body ends at 0x10033f96, then `mov %esi,%esi` and 0x90s);
//  icl emits the pair and the register-entry call only with caller and
//  callee in one /Qip TU.  OptimizeSurface also expands UnoptimizeSurface
//  TWICE (0x100345f1 and 0x10034899, its release loop and free inline),
//  which /Qip's inlining does here.  /Os by the push saves and 0x90 pads,
//  /Oi- because the fill CALLS memset.  Measured: the shim 0x10033fa0
//  identical (new), 0x10033f30 and 0x10034510 still identical;
//  0x10034580 746 -> 918 bytes (shipped 930), 0x10033fac a near; the
//  pFormat local (0x1008e6c0 read once, kept in -0x24(%ebp) for the fill
//  call and the pixel-format copy) and the grid pointer re-read through
//  pData for each tile are the shipped reads.
//
//  (w5_10033f30_osoi.cpp's notes:) /Os: pushed outgoing arguments with a
//  dead `push %edi` rounding each area to eight bytes, the `push %edi;
//  push %edi; push %esi; push %ebx` save, one shared epilogue, and
//  4-then-0x90 padding.  /Oi-: 0x10033fac CALLS memset (0x100809a0) for
//  its two DDSURFACEDESC2 clears.
//
// *********************************************************************** //

#include "ltbasedefs.h"
#include "ltrect.h"
#include <string.h>


// ---------------------------------------------------------------------- //
//  DirectDraw, the slots used here (d3dblit.cpp spells the same ones).
// ---------------------------------------------------------------------- //
struct DDPixelFormat                                    // 0x20 bytes
{
	uint32 dwSize;                                      // +0x00
	uint32 dwFlags;                                     // +0x04
	uint32 dwFourCC;                                    // +0x08
	uint32 dwRGBBitCount;                               // +0x0c
	uint32 dwRBitMask;                                  // +0x10
	uint32 dwGBitMask;                                  // +0x14
	uint32 dwBBitMask;                                  // +0x18
	uint32 dwRGBAlphaBitMask;                           // +0x1c
};

struct DDSurface;
struct DDSurfaceDesc2;
struct DDBltFX;
struct DDColorKey;
struct DDClipper;

struct DDSurfaceCaps                                    // 0x10 bytes
{
	uint32 dwCaps;                                      // +0x00
	uint32 dwCaps2;                                     // +0x04
	uint32 dwCaps3;                                     // +0x08
	uint32 dwCaps4;                                     // +0x0c
};

struct DDSurfaceDesc2
{
	uint32          dwSize;                             // +0x00
	uint32          dwFlags;                            // +0x04
	uint32          dwHeight;                           // +0x08
	uint32          dwWidth;                            // +0x0c
	int32           lPitch;                             // +0x10
	uint32          dwBackBufferCount;                  // +0x14
	uint32          dwMipMapCount;                      // +0x18
	uint32          dwAlphaBitDepth;                    // +0x1c
	uint32          dwReserved;                         // +0x20
	void*           lpSurface;                          // +0x24
	uint32          ddckCKDestOverlay[2];               // +0x28
	uint32          ddckCKDestBlt[2];                   // +0x30
	uint32          ddckCKSrcOverlay[2];                // +0x38
	uint32          ddckCKSrcBlt[2];                    // +0x40
	DDPixelFormat   ddpfPixelFormat;                    // +0x48
	DDSurfaceCaps   ddsCaps;                            // +0x68
	uint32          dwTextureStage;                     // +0x78
};                                                      // == 0x7c

struct DDColorKey
{
	uint32 dwColorSpaceLowValue;                        // +0x00
	uint32 dwColorSpaceHighValue;                       // +0x04
};

// Only the size matters here: every construction of one is memset to zero
// and then given dwSize = 0x64.  Nothing in this file reads a field of it.
struct DDBltFX
{
	uint32 dwSize;                                      // +0x00
	uint8  m_Rest[0x60];                                // +0x04
};                                                      // == 0x64

struct DDSurfaceVtbl
{
	void*  _QueryInterface;                                             // +0x00
	void*  _AddRef;                                                     // +0x04
	uint32 (__stdcall* Release)(DDSurface*);                            // +0x08
	void*  _pad_0c[2];                                                  // +0x0c
	long   (__stdcall* Blt)(DDSurface*, LTRect*, DDSurface*,
	                        LTRect*, uint32, DDBltFX*);                 // +0x14
	void*  _pad_18[5];                                                  // +0x18
	long   (__stdcall* Flip)(DDSurface*, DDSurface*, uint32);           // +0x2c
	void*  _pad_30[10];                                                 // +0x30
	long   (__stdcall* GetSurfaceDesc)(DDSurface*, DDSurfaceDesc2*);    // +0x58
	void*  _pad_5c[2];                                                  // +0x5c
	long   (__stdcall* Lock)(DDSurface*, LTRect*, DDSurfaceDesc2*,
	                         uint32, void*);                            // +0x64
	void*  _pad_68[2];                                                  // +0x68
	long   (__stdcall* SetClipper)(DDSurface*, DDClipper*);             // +0x70
	long   (__stdcall* SetColorKey)(DDSurface*, uint32, DDColorKey*);   // +0x74
	void*  _pad_78[2];                                                  // +0x78
	long   (__stdcall* Unlock)(DDSurface*, LTRect*);                    // +0x80
};

struct DDSurface
{
	DDSurfaceVtbl* lpVtbl;
};

struct DDrawVtbl
{
	void*  _pad_00[4];                                                  // +0x00
	long   (__stdcall* CreateClipper)(void*, uint32,
	                                  DDClipper**, void*);              // +0x10
	void*  _pad_14;                                                     // +0x14
	long   (__stdcall* CreateSurface)(void*, DDSurfaceDesc2*,
	                                  DDSurface**, void*);              // +0x18
};

struct DDraw
{
	DDrawVtbl* lpVtbl;
};

struct DDClipperVtbl
{
	void*  _pad_00[8];                                                  // +0x00
	long   (__stdcall* SetHWnd)(DDClipper*, uint32, void*);             // +0x20
};

struct DDClipper
{
	DDClipperVtbl* lpVtbl;
};

#define DDSD_CAPS                       0x00000001
#define DDSD_HEIGHT                     0x00000002
#define DDSD_WIDTH                      0x00000004
#define DDSD_PIXELFORMAT                0x00001000

#define DDSCAPS_OFFSCREENPLAIN          0x00000040
#define DDSCAPS_SYSTEMMEMORY            0x00000800
#define DDSCAPS_TEXTURE                 0x00001000
#define DDSCAPS2_TEXTUREMANAGE          0x00000010

#define DDCKEY_SRCBLT                   0x00000008
#define DDBLT_KEYSRC                    0x00008000
#define DDBLT_WAIT                      0x01000000
#define DDLOCK_WAIT         0x00000001
#define DDLOCK_READONLY     0x00000010
#define DDLOCK_WRITEONLY    0x00000020


// ---------------------------------------------------------------------- //
//  d3d.ren's optimized surface (d3dblit.cpp): a grid of 64x64 tiles, each
//  a texture plus the source rectangle it was cut from (+0x00, 16 bytes --
//  0x10033fac writes it with one `rep movsl` of four dwords).
// ---------------------------------------------------------------------- //
struct D3DOptTile
{
	LTRect     m_rcSrc;                                 // +0x00
	uint32     m_nWidth;                                // +0x10
	uint32     m_nHeight;                               // +0x14
	DDSurface* m_pTexture;                              // +0x18
};                                                      // == 0x1c

struct D3DOptSurface
{
	uint32     m_nCols;                                 // +0x00
	uint32     m_nRows;                                 // +0x04
	D3DOptTile m_Tiles[1];                              // +0x08
};

struct D3DRenderData
{
	DDSurface*      m_pSurface;                         // +0x00
	DDSurfaceDesc2  m_Desc;                             // +0x04
	uint32          m_nColorKey;                        // +0x80
	D3DOptSurface*  m_pOptimized;                       // +0x84
	uint32          m_bColorKeyed;                      // +0x88
};

extern void d3d_Free(void* pMem);                       // 0x10021cb0
extern uint32 g_nCaps_1008d938;                         // 0x1008d938
extern LTBOOL g_bFlag_1008cb60;                         // 0x1008cb60
extern void*  g_pUnknown_1008e6c0;                      // 0x1008e6c0  (a record whose
                                                        //  +0x0c is the DDPIXELFORMAT
                                                        //  tiles are made with)
extern DDraw* g_pDirectDraw;                            // 0x1008bf74

// The allowed tile texture dimensions: { 2, 4, 8, 16, 32, 64 }, six dwords
// at 0x10089430.
extern uint32 g_TextureSizes[6];                        // 0x10089430

extern void*  d3d_Alloc(uint32 nSize);                  // 0x10021c50 -- allocates
                                                        //   AND zero-fills nSize


// ---------------------------------------------------------------------- //
//  The module's pixel-format machinery (big3a080.cpp / d3dr4c.cpp /
//  d3dr2c_os.cpp).  A PFormat is 0x38 bytes: vtable 0x10089390 at +0x00,
//  the format type at +0x04 (2 = 16-bit, 3 = 32-bit) and the alpha mask at
//  +0x08 (Slot00's argument order is type, alpha, red, green, blue).  Its
//  vtable is stored by hand, as d3dr4c.cpp does, so no vtable symbol is
//  needed here.
// ---------------------------------------------------------------------- //
extern uint8 g_10089390[];                              // PFormat's vtable

class PFormat
{
public:
	void*  m_pVTable;                                   // +0x00
	uint32 m_nType;                                     // +0x04
	uint32 m_nAlphaMask;                                // +0x08
	uint32 m_Rest0c[11];                                // +0x0c..+0x37
};                                                      // 0x38

struct ConvertRequest
{
	ConvertRequest* Construct();                        // 0x10079fd0

	PFormat* m_pSrcFormat;                              // +0x00
	void*    m_pSrcData;                                // +0x04
	int32    m_nSrcPitch;                               // +0x08
	uint32   m_nUnknown0c;                              // +0x0c
	PFormat* m_pDstFormat;                              // +0x10
	void*    m_pDstData;                                // +0x14
	int32    m_nDstPitch;                               // +0x18
	uint32   m_nWidth;                                  // +0x1c
	uint32   m_nHeight;                                 // +0x20
	uint32   m_nUnknown24;                              // +0x24
	uint8    m_SrcFormat[0x38];                         // +0x28
	uint8    m_DstFormat[0x38];                         // +0x60
};                                                      // 0x98

class FormatMgr
{
public:
	uint32 ConvertPixels(ConvertRequest* pRequest);     // 0x1007a130
};
extern FormatMgr g_1008daa0;                            // 0x1008daa0

// 0x1007a180 on the same manager: convert a colour value into a format
// (the type word at +0x04 of its first argument picks the handler).
struct C1007a180Req;
class C1007a180Disp
{
public:
	int32 sub_1007a180(C1007a180Req* pReq, uint32 nParam, void* pData);
};

// 0x10079f70 -- the per-pixel shift of a format, g_10089870[m_nType].
class C10079f70
{
public:
	uint32 sub_10079f70();
};

// 0x10036df0 -- d3dblit.cpp: fills a PFormat with the screen's format.
struct D3DRenderInfo;
extern void sub_10036df0(D3DRenderInfo* pInfo);

// 0x10021010 -- a DDPIXELFORMAT into a PFormat (big3a080.cpp).
extern void sub_10021010(const DDPixelFormat* pPF, PFormat* pFormat);


// ======================================================================= //
//  0x10033f30 -- 124 bytes, one caller (d3dblit.cpp's DeleteSurface,
//  0x100368f1).  d3d_UnoptimizeSurface (0x10034510) without its null test
//  on pData: release every tile texture (the grid pointer re-read for each
//  one, since the Release call could change it), free the grid, clear the
//  pointer.
// ======================================================================= //
void sub_10033f30(D3DRenderData* pData)
{
	uint32 nCount, i;

	if (pData->m_pOptimized)
	{
		nCount = pData->m_pOptimized->m_nCols * pData->m_pOptimized->m_nRows;

		for (i = 0; i < nCount; ++i)
		{
			D3DOptTile* pTile = &pData->m_pOptimized->m_Tiles[i];

			if (pTile->m_pTexture)
			{
				DDSurface* pTex = *(DDSurface* volatile*)&pTile->m_pTexture;
				pTex->lpVtbl->Release(pTex);
			}
		}

		d3d_Free(pData->m_pOptimized);
		pData->m_pOptimized = LTNULL;
	}
}


// ======================================================================= //
//  0x10033fa0 / 0x10033fac -- 12 + 1376 bytes, one caller (OptimizeSurface,
//  0x100345db, below).  Copy the surface into its tile textures, converting the
//  pixel format, and give every pixel that is not the colour key (or every
//  pixel, when there is no key) the destination format's alpha.
//
//  THE SHIPPED ENTRY IS /Qip'S TWO-ENTRY PAIR: 0x10033fa0 loads the three
//  stack arguments into %eax/%edx/%ecx and falls into 0x10033fac, which
//  takes them there -- and the caller, OptimizeSurface below, uses the
//  register entry.  Named for the stack entry, sub_10033fa0.
//
//  WHAT THE BYTES SAY:
//   * a ConvertRequest built by 0x10079fd0 and then pointed at two frame
//     PFormats of its own (-0x1a4 source, -0x16c destination), both given
//     the vtable by hand; the source is the screen format (0x10036df0),
//     the destination the tile format from pUnknown + 0x0c (0x10021010);
//   * the colour key converted into the source format through 0x1007a180,
//     and pData->m_bColorKeyed (+0x88) = (nColorKey != 0xffffffff);
//   * the source locked read-only (flags 0x11), each tile's texture locked
//     write-only (0x21); a failed tile lock unlocks the source and fails;
//   * each tile's rectangle clipped to the surface (signed `jle`), copied
//     into the tile, the request aimed at it (the source pointer built as
//     row + (left << the format's pixel shift)) and ConvertPixels called;
//   * ONLY when the conversion answers 0 is alpha added by hand: to every
//     pixel without a key, else to every pixel whose source value is not
//     the converted key -- 16/32-bit source against 16/32-bit destination,
//     the source type tested first;
//   * the tile loop re-reads pData->m_pOptimized each time round.
// ======================================================================= //
LTBOOL sub_10033fa0(D3DRenderData* pData, void* pUnknown, uint32 nColorKey)
{
	LTRect          rc;
	ConvertRequest  req;
	PFormat         fmtSrc;
	PFormat         fmtDst;
	uint32          nKey;
	DDSurfaceDesc2  ddsd;
	DDSurfaceDesc2  ddsdTile;
	D3DOptTile*     pTile;
	void*           pSrcBits;
	uint32          x, y;
	uint32          nAlpha, nAlpha16;
	uint32          nRows, nCols;
	uint8*          pSrcRow;
	uint8*          pDstRow;

	req.Construct();

	fmtSrc.m_pVTable = fmtDst.m_pVTable = g_10089390;

	sub_10036df0((D3DRenderInfo*)&fmtSrc);
	sub_10021010((const DDPixelFormat*)((uint8*)pUnknown + 0x0c), &fmtDst);

	((C1007a180Disp*)&g_1008daa0)->sub_1007a180((C1007a180Req*)&fmtSrc, nColorKey, &nKey);

	pData->m_bColorKeyed = (nColorKey != 0xffffffff);

	req.m_nUnknown24 = 0;
	req.m_pDstFormat = &fmtDst;
	req.m_pSrcFormat = &fmtSrc;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	if (pData->m_pSurface->lpVtbl->Lock(pData->m_pSurface, LTNULL, &ddsd,
	                                    DDLOCK_WAIT | DDLOCK_READONLY, LTNULL) == 0)
	{

	pSrcBits = ddsd.lpSurface;

	for (x = 0; x < pData->m_pOptimized->m_nCols; x++)
	{
		for (y = 0; y < pData->m_pOptimized->m_nRows; y++)
		{
			pTile = &pData->m_pOptimized->m_Tiles[y * pData->m_pOptimized->m_nCols + x];

			memset(&ddsdTile, 0, sizeof(ddsdTile));
			ddsdTile.dwSize = sizeof(ddsdTile);

			if (pTile->m_pTexture->lpVtbl->Lock(pTile->m_pTexture, LTNULL, &ddsdTile,
			                                    DDLOCK_WAIT | DDLOCK_WRITEONLY, LTNULL) != 0)
			{
				pData->m_pSurface->lpVtbl->Unlock(pData->m_pSurface, LTNULL);
				return LTFALSE;
			}

			rc.left = x * 64;
			rc.right = x * 64 + 64;
			if (rc.right > (int)pData->m_Desc.dwWidth)
				rc.right = pData->m_Desc.dwWidth;

			rc.top = y * 64;
			rc.bottom = y * 64 + 64;
			if (rc.bottom > (int)pData->m_Desc.dwHeight)
				rc.bottom = pData->m_Desc.dwHeight;

			pTile->m_rcSrc = rc;

			req.m_pSrcData  = (uint8*)pSrcBits + rc.top * ddsd.lPitch;
			req.m_pSrcData  = (uint8*)req.m_pSrcData +
			                  (rc.left << ((C10079f70*)&fmtSrc)->sub_10079f70());
			req.m_nWidth    = rc.right - rc.left;
			req.m_nHeight   = rc.bottom - rc.top;
			req.m_nSrcPitch = ddsd.lPitch;
			req.m_pDstData  = ddsdTile.lpSurface;
			req.m_nDstPitch = ddsdTile.lPitch;

			if (g_1008daa0.ConvertPixels(&req) == 0)
			{
				if (nColorKey == 0xffffffff)
				{
					nAlpha   = fmtDst.m_nAlphaMask;
					nAlpha16 = (uint16)nAlpha;
					pDstRow  = (uint8*)req.m_pDstData;
					nRows    = req.m_nHeight;

					while (nRows)
					{
						nRows--;
						nCols = req.m_nWidth;

						if (req.m_pDstFormat->m_nType == 2)
						{
							uint16* pDst = (uint16*)pDstRow;
							while (nCols)
							{
								*pDst |= (uint16)nAlpha16;
								pDst++;
								nCols--;
							}
						}
						else
						{
							uint32* pDst = (uint32*)pDstRow;
							while (nCols)
							{
								*pDst |= nAlpha;
								pDst++;
								nCols--;
							}
						}

						pDstRow += req.m_nDstPitch;
					}
				}
				else
				{
					pDstRow  = (uint8*)req.m_pDstData;
					nAlpha   = fmtDst.m_nAlphaMask;
					nAlpha16 = (uint16)nAlpha;
					nRows    = req.m_nHeight;
					pSrcRow  = (uint8*)req.m_pSrcData;

					while (nRows)
					{
						nRows--;
						nCols = req.m_nWidth;

						if (req.m_pSrcFormat->m_nType == 2)
						{
							if (req.m_pDstFormat->m_nType == 2)
							{
								uint16* pSrc = (uint16*)pSrcRow;
								uint16* pDst = (uint16*)pDstRow;
								while (nCols)
								{
									nCols--;
									if (*pSrc != (uint16)nKey)
										*pDst |= (uint16)nAlpha16;
									pDst++;
									pSrc++;
								}
							}
							else if (req.m_pDstFormat->m_nType == 3)
							{
								uint16* pSrc = (uint16*)pSrcRow;
								uint32* pDst = (uint32*)pDstRow;
								while (nCols)
								{
									nCols--;
									if (*pSrc != (uint16)nKey)
										*pDst |= nAlpha;
									pDst++;
									pSrc++;
								}
							}
						}
						else
						{
							if (req.m_pDstFormat->m_nType == 2)
							{
								uint32* pSrc = (uint32*)pSrcRow;
								uint16* pDst = (uint16*)pDstRow;
								while (nCols)
								{
									nCols--;
									if (*pSrc != nKey)
										*pDst |= (uint16)nAlpha16;
									pDst++;
									pSrc++;
								}
							}
							else if (req.m_pDstFormat->m_nType == 3)
							{
								uint32* pSrc = (uint32*)pSrcRow;
								uint32* pDst = (uint32*)pDstRow;
								while (nCols)
								{
									nCols--;
									if (*pSrc != nKey)
										*pDst |= nAlpha;
									pDst++;
									pSrc++;
								}
							}
						}

						pSrcRow += req.m_nSrcPitch;
						pDstRow += req.m_nDstPitch;
					}
				}
			}

			pTile->m_pTexture->lpVtbl->Unlock(pTile->m_pTexture, LTNULL);
		}
	}

	pData->m_pSurface->lpVtbl->Unlock(pData->m_pSurface, LTNULL);

	return LTTRUE;
	}

	return LTFALSE;
}


// ---------------------------------------------------------------------- //
//  SLOT +0xe0 -- throw away the optimized form
// ---------------------------------------------------------------------- //
//
// 0x10034510, 108 bytes.  DECODED.
//
// Release every tile texture, free the grid, null the pointer.  The tile
// count is m_nCols * m_nRows, which is what fixes those two header dwords
// (the shipped code does `imul 0x4(%eax)` against `(%eax)`).
// The teardown proper, inlined into d3d_OptimizeSurface's two failure
// paths WITHOUT the null test on pData (the shipped copies at 0x100345f1
// and 0x10034899 go straight to +0x84), so that test is not part of it.
static inline void FreeOptimized(D3DRenderData* pData)
{
	uint32 nCount, i;

	if (pData->m_pOptimized)
	{
		nCount = pData->m_pOptimized->m_nCols * pData->m_pOptimized->m_nRows;

		for (i = 0; i < nCount; ++i)
		{
			D3DOptTile* pTile = &pData->m_pOptimized->m_Tiles[i];

			if (pTile->m_pTexture)
			{
				DDSurface* pTex = *(DDSurface* volatile*)&pTile->m_pTexture;
				pTex->lpVtbl->Release(pTex);
			}
		}

		d3d_Free(pData->m_pOptimized);
		pData->m_pOptimized = LTNULL;
	}
}

void d3d_UnoptimizeSurface(D3DRenderData* pData)
{
	if (!pData)
		return;

	// From here on the body is 0x10033f30's (w5_10033f30_osoi.cpp), and the
	// bytes are too: the grid test wraps the teardown rather than returning
	// early, and the texture pointer is re-read for the Release.
	FreeOptimized(pData);
}


// ---------------------------------------------------------------------- //
//  SLOT +0xdc -- build the optimized form
// ---------------------------------------------------------------------- //
//
// 0x10034580, 930 bytes.  DECODED, with two INFERRED points called out.
//
// Cuts the surface into a grid of 64x64 cells and gives each cell a managed
// texture whose dimensions are rounded UP to entries of the six-element
// table { 2, 4, 8, 16, 32, 64 } at 0x10089430, then hands the whole thing to
// 0x10033fac to fill.  If the fill fails the grid is torn down again and the
// answer is zero, which is why OptimizeSurface can be called speculatively.
//
// TWO THINGS THE BYTES SAY THAT ARE WORTH NOT SMOOTHING OVER:
//
//  * the size search tests `remaining < size`, strictly -- so a cell with
//    exactly 64 pixels left does NOT match the 64 entry and falls out of the
//    loop to the `g_TextureSizes[5]` default, which is 64 anyway.  Written
//    here as the loop the shipped code actually runs, not as the rounding it
//    is presumably meant to be.  The shipped loop is unrolled by three and
//    that unrolling is visible; it is written rolled here because the roll
//    is the source and the unroll is the compiler.
//
//  * when bit 0x20 of 0x1008d938 is set both tile dimensions become the
//    LARGER of the two -- square tiles.  That is what a square-textures-only
//    device cap forces, which is the reading, but the flag itself is named
//    after its address because this function does not set it.
//
// INFERRED: the second argument of 0x10033fac.  The shipped call puts
// 0x1008e6c0 in %edx (see the register-argument note under BlitToScreen);
// that global is also the record whose +0x0c is the DDPIXELFORMAT the tiles
// are created with, so it is passed as itself, not derived.  Its type is
// unknown here.
//
// IDENTICAL.  What closed it (it was 918 bytes against 930):
//  * the size search is an inline helper that RETURNS the entry (and the
//    [5] default after the loop): the default is loaded on loop exit, not
//    before it, and the width search is then unrolled by three as shipped;
//  * x and y are not variables: `nCol * 64` / `nRow * 64`, which icl
//    strength-reduces itself (the row loop then compares y against
//    nRows*64 with no row counter, as shipped);
//  * the tile pointer is taken after the height search;
//  * the three leading failures are one `||` guard (one shared return);
//  * the two teardowns are FreeOptimized, not d3d_UnoptimizeSurface;
//  * the column/row counts are both shifted before either is rounded up,
//    and the four desc stores are written size, flags, caps, caps2 (icl
//    emits them in reverse).
static inline uint32 GetTextureSize(uint32 nSize)
{
	uint32 i;

	for (i = 0; i < 6; ++i)
	{
		if (nSize < g_TextureSizes[i])
			return g_TextureSizes[i];
	}

	return g_TextureSizes[5];
}

LTBOOL d3d_OptimizeSurface(D3DRenderData* pData, uint32 nColorKey)
{
	void*          pFormat;
	uint32         nCols, nRows;
	uint32         nCol, nRow;

	if (!pData || !g_bFlag_1008cb60 || (g_nCaps_1008d938 & 0x20))
		return LTFALSE;

	pFormat = g_pUnknown_1008e6c0;
	if (!pFormat)
		return LTFALSE;

	if (!pData->m_pOptimized)
	{
		nCols = pData->m_Desc.dwWidth  >> 6;
		nRows = pData->m_Desc.dwHeight >> 6;

		if ((nCols << 6) != pData->m_Desc.dwWidth)
			++nCols;

		if ((nRows << 6) != pData->m_Desc.dwHeight)
			++nRows;

		if (!nCols && !nRows)
			return LTFALSE;

		pData->m_pOptimized = (D3DOptSurface*)d3d_Alloc(
			(nCols * nRows - 1) * sizeof(D3DOptTile)
			+ sizeof(D3DOptSurface));

		if (!pData->m_pOptimized)
			return LTFALSE;

		pData->m_pOptimized->m_nCols = nCols;
		pData->m_pOptimized->m_nRows = nRows;

		for (nCol = 0; nCol < nCols; ++nCol)
		{
			uint32 nTileWidth = GetTextureSize(pData->m_Desc.dwWidth - nCol * 64);

			for (nRow = 0; nRow < nRows; ++nRow)
			{
				uint32         nTileHeight = GetTextureSize(pData->m_Desc.dwHeight - nRow * 64);
				D3DOptTile*    pTile = &pData->m_pOptimized->m_Tiles[nRow * nCols + nCol];
				DDSurfaceDesc2 desc;

				if (g_nCaps_1008d938 & 0x20)
				{
					pTile->m_nWidth  = (nTileWidth > nTileHeight)
					                 ? nTileWidth : nTileHeight;
					pTile->m_nHeight = (nTileWidth > nTileHeight)
					                 ? nTileWidth : nTileHeight;
				}
				else
				{
					pTile->m_nHeight = nTileHeight;
					pTile->m_nWidth  = nTileWidth;
				}

				memset(&desc, 0, sizeof(desc));
				desc.dwSize          = sizeof(desc);
				desc.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH
				                     | DDSD_PIXELFORMAT;
				desc.ddsCaps.dwCaps  = DDSCAPS_TEXTURE;
				desc.ddsCaps.dwCaps2 = DDSCAPS2_TEXTUREMANAGE;
				desc.dwWidth         = pTile->m_nWidth;
				desc.dwHeight        = pTile->m_nHeight;

				memcpy(&desc.ddpfPixelFormat, (uint8*)pFormat + 0x0c,
				       sizeof(DDPixelFormat));

				if (g_pDirectDraw->lpVtbl->CreateSurface(g_pDirectDraw, &desc,
				                                         &pTile->m_pTexture,
				                                         LTNULL))
				{
					FreeOptimized(pData);
					return LTFALSE;
				}
			}
		}
	}

	if (sub_10033fa0(pData, pFormat, nColorKey))
		return LTTRUE;

	FreeOptimized(pData);

	return LTFALSE;
}

