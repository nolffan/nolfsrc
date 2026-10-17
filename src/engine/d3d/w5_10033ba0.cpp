// *********************************************************************** //
//
//  MODULE:   W5_10033BA0 (.CPP)  --  create the z-buffer: 0x10033ba0 and its
//            EnumZBufferFormats callback 0x100329a0.   /O2.
//
// *********************************************************************** //

#include "ltbasedefs.h"
#include <string.h>


// ---------------------------------------------------------------------- //
//  Types, at the offsets the bytes give (big331e0.cpp spells the same
//  shapes; RenderStructInit is ltbasedefs.h's RMode behind a version word).
// ---------------------------------------------------------------------- //
struct D3DGuid
{
	uint32 m_Data1;
	uint16 m_Data2;
	uint16 m_Data3;
	uint8  m_Data4[8];
};

struct RenderStructInit
{
	uint32 m_nVersion;                                  // +0x000
	RMode  m_Mode;                                      // +0x004 (Width +0x208, Height +0x20c)
	void*  m_hWnd;                                      // +0x218
};

// DDPIXELFORMAT (0x20 bytes).  For a z-buffer format +0x18 is
// dwStencilBitMask and +0x1c dwRGBZBitMask -- the two the log line prints.
struct DDPixFmt10033ba0
{
	uint32 dwSize;                                      // +0x00
	uint32 dwFlags;                                     // +0x04
	uint32 dwFourCC;                                    // +0x08
	uint32 dwZBufferBitDepth;                           // +0x0c
	uint32 dwStencilBitDepth;                           // +0x10
	uint32 dwZBitMask;                                  // +0x14
	uint32 dwStencilBitMask;                            // +0x18
	uint32 dwRGBZBitMask;                               // +0x1c
};

struct DDSCaps2_10033ba0 { uint32 dwCaps, dwCaps2, dwCaps3, dwCaps4; };

// DDSURFACEDESC2 (0x7c bytes).
struct DDSurfDesc10033ba0
{
	uint32            dwSize;                           // +0x00
	uint32            dwFlags;                          // +0x04
	uint32            dwHeight;                         // +0x08
	uint32            dwWidth;                          // +0x0c
	uint32            m_Rest10[14];                     // +0x10..+0x47
	DDPixFmt10033ba0  ddpfPixelFormat;                  // +0x48
	DDSCaps2_10033ba0 ddsCaps;                          // +0x68
	uint32            dwTextureStage;                   // +0x78
};

struct DDSurface;
struct DDSurfaceVtbl
{
	long   (__stdcall* QueryInterface)(DDSurface*, const void*, void**);   // +0x00
	uint32 (__stdcall* AddRef)(DDSurface*);                                 // +0x04
	uint32 (__stdcall* Release)(DDSurface*);                                // +0x08
	long   (__stdcall* AddAttachedSurface)(DDSurface*, DDSurface* pAttach); // +0x0c
	void*  m_pad_10[4];                                                     // +0x10..+0x1c
	long   (__stdcall* DeleteAttachedSurface)(DDSurface*, uint32 dwFlags,
	                                          DDSurface* pAttached);        // +0x20
};
struct DDSurface { DDSurfaceVtbl* lpVtbl; };

struct DDraw7;
struct DDraw7Vtbl
{
	void* m_pad_00[6];                                  // +0x00..+0x14
	long (__stdcall* CreateSurface)(DDraw7*, DDSurfDesc10033ba0* pDesc,
	                                DDSurface** ppSurface, void* pUnkOuter);   // +0x18
};
struct DDraw7 { DDraw7Vtbl* lpVtbl; };

struct D3DDevice7;

typedef long (__stdcall* ZEnumFn10033ba0)(DDPixFmt10033ba0* pFmt, void* pContext);

struct D3D7;
struct D3D7Vtbl
{
	void* m_pad_00[4];                                  // +0x00..+0x0c
	long (__stdcall* CreateDevice)(D3D7*, const D3DGuid* rclsid, DDSurface* pSurface,
	                               D3DDevice7** ppDevice);                     // +0x10
	void* m_pad_14;                                     // +0x14
	long (__stdcall* EnumZBufferFormats)(D3D7*, const D3DGuid* riidDevice,
	                                     ZEnumFn10033ba0 pfnCallback,
	                                     void* pContext);                      // +0x18
};
struct D3D7 { D3D7Vtbl* lpVtbl; };

extern DDraw7*     g_pDirectDraw;       // 0x1008bf74
extern DDSurface*  g_pZBuffer;          // 0x1008bf64
extern DDSurface*  g_pOffscreen;        // 0x1008bf68
extern D3D7*       g_pD3D;              // 0x1008d980
extern D3DDevice7* g_pD3DDevice;        // 0x1008d988
extern uint32      g_nSurfCaps1008cbb8; // 0x1008cbb8  DDSCAPS memory half

// The enumeration's output, published through two globals that point into
// 0x10033ba0's frame for the callback: the count and the format array.
extern uint32*           g_p1008d98c;   // 0x1008d98c  -> nZFormats
extern DDPixFmt10033ba0* g_p1008d990;   // 0x1008d990  -> aZFormats

// The chosen format is kept in 0x1008d7d4..0x1008d7f0 by eight separate
// dword stores.  The module already names one of those dwords on its own
// (g_1008d7e4, +0x10 dwStencilBitDepth, d3dneutral.cpp), so they are eight
// globals here, one per store, not one struct.
extern uint32 g_1008d7d4;               // 0x1008d7d4  dwSize
extern uint32 g_1008d7d8;               // 0x1008d7d8  dwFlags
extern uint32 g_1008d7dc;               // 0x1008d7dc  dwFourCC
extern uint32 g_1008d7e0;               // 0x1008d7e0  dwZBufferBitDepth
extern uint32 g_1008d7e4;               // 0x1008d7e4  dwStencilBitDepth
extern uint32 g_1008d7e8;               // 0x1008d7e8  dwZBitMask
extern uint32 g_1008d7ec;               // 0x1008d7ec  dwStencilBitMask
extern uint32 g_1008d7f0;               // 0x1008d7f0  dwRGBZBitMask

// 0x10021d60 -- the renderer's warning printf.
extern void sub_10021d60(uint32 nLevel, const char* pszMsg, ...);

// 0x100210d0 -- d3dbatcha.cpp's DirectDraw teardown.
extern void sub_100210d0();

#define MAX_ZFORMATS_10033BA0	64

// The enumeration buffer: the count sits immediately below the array
// (-0x888 and -0x884 in 0x10033ba0's frame), so they are one object.
struct ZFormats10033ba0
{
	uint32           nZFormats;
	DDPixFmt10033ba0 aZFormats[MAX_ZFORMATS_10033BA0];
};


// ======================================================================= //
//  0x100329a0 -- 94 bytes.  The LPD3DENUMPIXELFORMATSCALLBACK handed to
//  IDirect3D7::EnumZBufferFormats at 0x10033bf1: __stdcall (`ret $0x8`),
//  appends the format while fewer than 64 are held, always returns
//  D3DENUMRET_OK.  The count is re-read through its global for the
//  increment (`incl (%eax)` after reloading 0x1008d98c) because the
//  32-byte copy through another pointer could alias it.
// ======================================================================= //
long __stdcall sub_100329a0(DDPixFmt10033ba0* pFmt, void* pContext)
{
	if (*g_p1008d98c < MAX_ZFORMATS_10033BA0)
	{
		g_p1008d990[*g_p1008d98c] = *pFmt;
		(*g_p1008d98c)++;
	}

	return 1;
}


// ======================================================================= //
//  0x10033ba0 -- 912 bytes, one caller (big331e0.cpp, 0x1003387a).  Create
//  a z-buffer the size of the mode, attach it to the render target and
//  create the Direct3D device on it -- trying each enumerated z format in
//  turn until CreateDevice succeeds.
//
//  WHAT PINS IT:
//   * the frame: a 64-dword "tried" table at -0x988 (memset), the count at
//     -0x888 and the 64 x 0x20-byte formats straight after it at -0x884,
//     whose two addresses go to 0x1008d98c / 0x1008d990 for the callback;
//   * the pick: the first untried format, replaced by a later untried one
//     only when the current pick has no stencil mask (+0x18) and the later
//     one has; nothing untried (index still -1, `cmp $0x40; jb`) is FALSE;
//   * CreateSurface (+0x18) of a DDSURFACEDESC2 (0x7c, flags 0x1007 =
//     CAPS|HEIGHT|WIDTH|PIXELFORMAT, caps = 0x1008cbb8 | DDSCAPS_ZBUFFER)
//     into g_pZBuffer: failure logs, tears DirectDraw down (0x100210d0) and
//     returns FALSE;
//   * AddAttachedSurface (+0x0c) on g_pOffscreen: failure releases the
//     z-buffer (without clearing the pointer) and returns FALSE;
//   * CreateDevice (+0x10) with the caller's device GUID: failure detaches
//     (+0x20) and releases the z-buffer, clears it, marks the format tried
//     and goes round again while the count is non-zero;
//   * success copies the format out and logs its z and stencil masks.
//  The GUID is taken BY VALUE (`lea 0xc(%ebp)` is passed as the REFCLSID).
// ======================================================================= //
LTBOOL sub_10033ba0(RenderStructInit* pInit, D3DGuid guid)
{
	uint32              bTried[MAX_ZFORMATS_10033BA0];
	ZFormats10033ba0    zf;
	DDSurfDesc10033ba0  ddsd;
	uint32              i, iBest;

	memset(bTried, 0, sizeof(bTried));

	zf.nZFormats = 0;
	g_p1008d98c = &zf.nZFormats;
	g_p1008d990 = zf.aZFormats;

	if (g_pD3D->lpVtbl->EnumZBufferFormats(g_pD3D, &guid, sub_100329a0, LTNULL) != 0 ||
		zf.nZFormats == 0)
	{
		sub_10021d60(1, "Unable to find an acceptable z-buffer format.");
		return LTFALSE;
	}

	while (zf.nZFormats > 0)
	{
		iBest = (uint32)-1;
		for (i = 0; i < zf.nZFormats; i++)
		{
			if (!bTried[i])
			{
				if (iBest == (uint32)-1)
					iBest = i;
				else if (zf.aZFormats[iBest].dwStencilBitMask == 0 &&
				         zf.aZFormats[i].dwStencilBitMask != 0)
					iBest = i;
			}
		}

		if (iBest >= MAX_ZFORMATS_10033BA0)
			return LTFALSE;

		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = 0x1007;
		ddsd.dwWidth = pInit->m_Mode.m_Width;
		ddsd.dwHeight = pInit->m_Mode.m_Height;
		ddsd.ddsCaps.dwCaps = g_nSurfCaps1008cbb8 | 0x20000;   // DDSCAPS_ZBUFFER
		ddsd.ddpfPixelFormat = zf.aZFormats[iBest];

		if (g_pDirectDraw->lpVtbl->CreateSurface(g_pDirectDraw, &ddsd, &g_pZBuffer, LTNULL) != 0)
		{
			sub_10021d60(1, "Failed to make z-buffer.");
			sub_100210d0();
			return LTFALSE;
		}

		if (g_pOffscreen->lpVtbl->AddAttachedSurface(g_pOffscreen, g_pZBuffer) < 0)
		{
			g_pZBuffer->lpVtbl->Release(g_pZBuffer);
			return LTFALSE;
		}

		if (g_pD3D->lpVtbl->CreateDevice(g_pD3D, &guid, g_pOffscreen, &g_pD3DDevice) >= 0)
		{
			g_1008d7d4 = zf.aZFormats[iBest].dwSize;
			g_1008d7d8 = zf.aZFormats[iBest].dwFlags;
			g_1008d7dc = zf.aZFormats[iBest].dwFourCC;
			g_1008d7e0 = zf.aZFormats[iBest].dwZBufferBitDepth;
			g_1008d7e4 = zf.aZFormats[iBest].dwStencilBitDepth;
			g_1008d7e8 = zf.aZFormats[iBest].dwZBitMask;
			g_1008d7ec = zf.aZFormats[iBest].dwStencilBitMask;
			g_1008d7f0 = zf.aZFormats[iBest].dwRGBZBitMask;
			sub_10021d60(1, "ZBuffer: Z Mask: %d, Stencil mask: %d",
			             zf.aZFormats[iBest].dwRGBZBitMask,
			             zf.aZFormats[iBest].dwStencilBitMask);
			return LTTRUE;
		}

		g_pOffscreen->lpVtbl->DeleteAttachedSurface(g_pOffscreen, 0, g_pZBuffer);
		g_pZBuffer->lpVtbl->Release(g_pZBuffer);
		g_pZBuffer = LTNULL;

		bTried[iBest] = 1;
	}

	return LTFALSE;
}
