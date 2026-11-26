// *********************************************************************** //
//
//  MODULE:   W26_10020FD0_OSOIQIP (.CPP)  --  d3d.ren's init TU
//
//  0x10020fd0..0x10021a7f of the retail d3d.ren: the three exports
//  (GetSupportedModes 0x100212d0, FreeModeList 0x100213e0, RenderDLLSetup
//  0x10021410), the renderer's Init (slot +0x6c, 0x100215a0) and Term
//  (+0x70, 0x100211d0), GetRenderObject (+0xb8, 0x10020fd0), and the
//  buffer-format pair 0x10021010/0x10021070 (moved here from
//  w4_10021010_osqip.cpp, where they were identical at /Os /Qip).
//
//  FLAG SET /Os /Oi- /Qip (the _osoiqip suffix), from the bytes:
//    * /Qip: Init calls 0x10021070 through its REGISTER entry 0x10021078
//      (`mov 0x1008bf6c,%eax ; mov $0x1008bf28,%edx ; call 0x10021078`),
//      and 0x10021070 calls 0x10021010 the same way (0x10021018) -- icl's
//      external two-entry pairs, so all three share one /Qip TU.
//    * /Oi-: GetRenderObject CALLS the CRT strcmp (0x10080910) twice; at
//      /Os icl expands it inline (123 bytes against 62 -- measured), and
//      `#pragma function(strcmp)` does not stop it.  0x10021070's memset
//      and Init's strncpy/strcmp/atoi are calls too.  The neighbouring
//      mode callback 0x10021220 (d3dr5c.cpp) is identical only at /Oi-.
//    * /Os: 0x90 padding after each function (0x1002100e, 0x1002121b,
//      0x100213d8), `lea 0x0(%esi),%esi` then 0x90 after 0x10021405.
//
//  Not here, though they were in the shipped TU: 0x10020f30 (d3dr2d_os.cpp;
//  inlined here as FreeDeviceList), 0x100210d0 (d3dbatcha.cpp) and the mode
//  callback 0x10021220 (d3dr5c.cpp).  The TU's static initialiser
//  0x10021580 (in the CRT's initialiser table at 0x10089060) also belongs
//  to it: it stores the vtable 0x10089390 into the PFormat at 0x1008bf28
//  and ties off the device-list head 0x1008bf78 -- i.e. this TU DEFINED
//  those two objects.  They are defined by d3ddata.cpp here, so the same
//  initialisation is run from this file (TUInit10021580, below).
//
// *********************************************************************** //

#include "ltbasedefs.h"

// DirectDraw's DDPIXELFORMAT (big3a080.cpp spells it the same way).
struct DDPixelFormat
{
	uint32 dwSize;                                      // +0x00
	uint32 dwFlags;                                     // +0x04
	uint32 dwFourCC;                                    // +0x08
	uint32 dwRGBBitCount;                               // +0x0c
	uint32 dwRBitMask;                                  // +0x10
	uint32 dwGBitMask;                                  // +0x14
	uint32 dwBBitMask;                                  // +0x18
	uint32 dwRGBAlphaBitMask;                           // +0x1c
};                                                      // 0x20

// big3a080.cpp's pixel-format object: one virtual at slot +0x00 that
// takes the format type and the four channel masks.
class PFormat
{
public:
	virtual uint32  Slot00(uint32 nType, uint32 nAlphaMask, uint32 nRedMask,
	                       uint32 nGreenMask, uint32 nBlueMask);
};


// ======================================================================= //
//  0x10021010 -- copy a DDPIXELFORMAT into a PFormat: 16-bit formats are
//  type 2, everything else type 3.
// ======================================================================= //
void sub_10021010(const DDPixelFormat* pPF, PFormat* pFormat)
{
	uint32 nType;

	if (pPF->dwRGBBitCount == 16)
		nType = 2;
	else
		nType = 3;

	pFormat->Slot00(nType, pPF->dwRGBAlphaBitMask, pPF->dwRBitMask,
	                pPF->dwGBitMask, pPF->dwBBitMask);
}


// ======================================================================= //
//  0x10021070 -- read a surface's pixel format into a PFormat: clear a
//  DDPIXELFORMAT, set its dwSize, IDirectDrawSurface7::GetPixelFormat
//  (vtable +0x54), and on success hand it to 0x10021010.  Answers
//  TRUE/FALSE.  Written here because it is 0x10021010's only register-
//  entry caller: without a same-TU caller icl does not emit the two-entry
//  pair at all.  memset is CALLED in the shipped body (0x100809a0).
// ======================================================================= //
class DDSurface7
{
public:
	virtual long __stdcall Slot00();  virtual long __stdcall Slot04();
	virtual long __stdcall Slot08();  virtual long __stdcall Slot0c();
	virtual long __stdcall Slot10();  virtual long __stdcall Slot14();
	virtual long __stdcall Slot18();  virtual long __stdcall Slot1c();
	virtual long __stdcall Slot20();  virtual long __stdcall Slot24();
	virtual long __stdcall Slot28();  virtual long __stdcall Slot2c();
	virtual long __stdcall Slot30();  virtual long __stdcall Slot34();
	virtual long __stdcall Slot38();  virtual long __stdcall Slot3c();
	virtual long __stdcall Slot40();  virtual long __stdcall Slot44();
	virtual long __stdcall Slot48();  virtual long __stdcall Slot4c();
	virtual long __stdcall Slot50();
	virtual long __stdcall GetPixelFormat(DDPixelFormat* pPF);      // +0x54
};

int32 sub_10021070(DDSurface7* pSurface, PFormat* pFormat)
{
	DDPixelFormat ddpf;

	memset(&ddpf, 0, sizeof(ddpf));
	ddpf.dwSize = sizeof(ddpf);
	if (pSurface->GetPixelFormat(&ddpf) < 0)
		return 0;

	sub_10021010(&ddpf, pFormat);
	return 1;
}


// ---------------------------------------------------------------------- //
//  THE RENDERSTRUCT, as this TU writes it
// ---------------------------------------------------------------------- //
//
//  Only the slots RenderDLLSetup stores are typed; the engine's callbacks
//  (+0x00..+0x38), the screen size and the counters stay reserved bytes
//  here -- the files that call them spell their own views (big331e0.cpp's
//  RenderStructCB, d3dmid.cpp's RenderStructCallbacks, ...).  Each slot's
//  type is the signature the function it receives is DEFINED with in our
//  objects (the mangled name fixes it), so the stores need no casts.

struct D3DRenderData;
struct D3DRenderDataHead;
struct RenderBlitRequest;
struct D3DRenderInfo;
struct SceneDesc;
// RenderStructInit, big331e0.cpp's (and the client's): the version word
// is an OUT parameter, then the RMode, then the window.
struct RenderStructInit
{
	uint32  m_nVersion;                                 // +0x000
	RMode   m_Mode;                                     // +0x004
	void*   m_hWnd;                                     // +0x218
};
struct SharedTexture;

// The slot targets that were unwritten are written now: Init, Term and
// GetRenderObject here; the rest in the files named beside them.
int32   sub_100215a0(RenderStructInit* pInit);          // +0x6c Init
void    sub_100211d0();                                 // +0x70 Term
void    sub_10039fe0(SharedTexture* pShared, LTBOOL bUpload);  // +0x74 (w6_10039730_qip.cpp)
void    sub_1003aac0(SharedTexture* pShared);           // +0x78 (w6_10039730_qip.cpp)
void*   d3d_CreateWorld(void** pWorldData);             // +0x7c
void    d3d_DestroyWorld(void* pWorld);                 // +0x80
void    sub_100262f0(LTRect* pRect, uint32 dwFlags);    // +0x84 ClearScreen (w26_100262f0.cpp)
LTBOOL  d3d_Start3D();                                  // +0x88
LTBOOL  d3d_End3D();                                    // +0x8c
LTBOOL  d3d_IsIn3D();                                   // +0x90
LTBOOL  d3d_StartOptimized2D();                         // +0x94
void    d3d_EndOptimized2D();                           // +0x98
LTBOOL  d3d_IsInOptimized2D();                          // +0x9c
LTBOOL  d3d_SetOptimized2DBlend(LTSurfaceBlend blend);  // +0xa0
LTBOOL  d3d_GetOptimized2DBlend(LTSurfaceBlend* pBlend);// +0xa4
LTBOOL  d3d_SetOptimized2DColor(uint32 color);          // +0xa8
LTBOOL  d3d_GetOptimized2DColor(uint32* pColor);        // +0xac
LTBOOL  d3d_RenderScene(SceneDesc* pDesc);              // +0xb0
void    sub_10032d00(int argc, char** argv);            // +0xb4 RenderCommand (w32_10032d00_qip.cpp)
void*   sub_10020fd0(const char* pName);                // +0xb8 GetRenderObject
void    sub_100371e0(uint32 dwFlags);                   // +0xbc FlipScreen (w26_100371e0_osoi.cpp)
uint32  d3d_Slot_c0();                                  // +0xc0
void    sub_10036df0(D3DRenderInfo* pInfo);             // +0xc4
D3DRenderData* d3d_CreateSurface(uint32 w, uint32 h);   // +0xc8
void    d3d_DeleteSurface(D3DRenderData* pData);        // +0xcc
void    d3d_GetSurfaceInfo(D3DRenderDataHead* pData, uint32* pWidth,
                           uint32* pHeight, uint32* pPitch);            // +0xd0
void*   d3d_LockSurface(D3DRenderData* pData);          // +0xd4
void    d3d_UnlockSurface(D3DRenderData* pData);        // +0xd8
LTBOOL  d3d_OptimizeSurface(D3DRenderData* pData, uint32 transparentColor);   // +0xdc
void    d3d_UnoptimizeSurface(D3DRenderData* pData);    // +0xe0
LTBOOL  d3d_LockScreen(long left, long top, long right, long bottom,
                       void** pData, uint32* pPitch);   // +0xe4
void    d3d_UnlockScreen();                             // +0xe8
void    d3d_BlitToScreen(RenderBlitRequest* pRequest);  // +0xec
LTBOOL  d3d_BlitWarpedToScreen(RenderBlitRequest* pRequest, void* pPoints,
                               uint32 nPoints);         // +0xf0
void    sub_10036e40(const char* pFilename);            // +0xf4 MakeScreenShot (w26_10036e40_osoi.cpp)
void    sub_10021e50();                                 // +0xf8
void    d3d_BlitFromScreen(RenderBlitRequest* pRequest);// +0xfc

struct RenderStruct
{
	uint8   m_Reserved00[0x18];                                     // +0x00..+0x17
	void*   (*GetConsoleVar)(const char* pName);                    // +0x18
	float   (*GetVarValueFloat)(void* hVar);                        // +0x1c
	char*   (*GetVarValueString)(void* hVar);                       // +0x20
	uint8   m_Reserved24[0x3c - 0x24];                              // +0x24..+0x3b
	uint32  m_Width;                                                // +0x3c
	uint32  m_Height;                                               // +0x40
	uint8   m_Reserved44[0x6c - 0x44];                              // +0x44..+0x6b

	int32   (*Init)(RenderStructInit* pInit);                       // +0x6c
	void    (*Term)();                                              // +0x70
	void    (*BindTexture)(SharedTexture* pShared, LTBOOL bUpload); // +0x74
	void    (*UnbindTexture)(SharedTexture* pShared);               // +0x78
	void*   (*CreateWorld)(void** pWorldData);                      // +0x7c
	void    (*DestroyWorld)(void* pWorld);                          // +0x80
	void    (*ClearScreen)(LTRect* pRect, uint32 dwFlags);          // +0x84
	LTBOOL  (*Start3D)();                                           // +0x88
	LTBOOL  (*End3D)();                                             // +0x8c
	LTBOOL  (*IsIn3D)();                                            // +0x90
	LTBOOL  (*StartOptimized2D)();                                  // +0x94
	void    (*EndOptimized2D)();                                    // +0x98
	LTBOOL  (*IsInOptimized2D)();                                   // +0x9c
	LTBOOL  (*SetOptimized2DBlend)(LTSurfaceBlend blend);           // +0xa0
	LTBOOL  (*GetOptimized2DBlend)(LTSurfaceBlend* pBlend);         // +0xa4
	LTBOOL  (*SetOptimized2DColor)(uint32 color);                   // +0xa8
	LTBOOL  (*GetOptimized2DColor)(uint32* pColor);                 // +0xac
	LTBOOL  (*RenderScene)(SceneDesc* pDesc);                       // +0xb0
	void    (*RenderCommand)(int argc, char** argv);                // +0xb4
	void*   (*GetHook)(const char* pName);                          // +0xb8
	void    (*SwapBuffers)(uint32 dwFlags);                         // +0xbc
	uint32  (*GetInfoFlags)();                                      // +0xc0
	void    (*GetRenderInfo)(D3DRenderInfo* pInfo);                 // +0xc4
	D3DRenderData* (*CreateSurface)(uint32 w, uint32 h);            // +0xc8
	void    (*DeleteSurface)(D3DRenderData* pData);                 // +0xcc
	void    (*GetSurfaceInfo)(D3DRenderDataHead* pData, uint32* pWidth,
	                          uint32* pHeight, uint32* pPitch);     // +0xd0
	void*   (*LockSurface)(D3DRenderData* pData);                   // +0xd4
	void    (*UnlockSurface)(D3DRenderData* pData);                 // +0xd8
	LTBOOL  (*OptimizeSurface)(D3DRenderData* pData, uint32 transparentColor); // +0xdc
	void    (*UnoptimizeSurface)(D3DRenderData* pData);             // +0xe0
	LTBOOL  (*LockScreen)(long left, long top, long right, long bottom,
	                      void** pData, uint32* pPitch);            // +0xe4
	void    (*UnlockScreen)();                                      // +0xe8
	void    (*BlitToScreen)(RenderBlitRequest* pRequest);           // +0xec
	LTBOOL  (*BlitWarpedToScreen)(RenderBlitRequest* pRequest,
	                              void* pPoints, uint32 nPoints);   // +0xf0
	void    (*MakeScreenShot)(const char* pFilename);               // +0xf4
	void    (*ReadConsoleVariables)();                              // +0xf8
	void    (*BlitFromScreen)(RenderBlitRequest* pRequest);         // +0xfc
};

// 0x1008cbcc -- RenderDLLSetup's first store; every renderer function
// that calls back into the engine reads it.
extern RenderStruct* g_pStruct;                         // 0x1008cbcc


// ---------------------------------------------------------------------- //
//  THE DEVICE LIST AND THE MODE LIST
// ---------------------------------------------------------------------- //
//
//  0x1008bf78 is the list head of DirectDraw devices sub_10069e40 builds
//  (w8_10069ca0_osoiqip.cpp's DDDevice10069ca0: LTLink at +0, the driver
//  GUID pointer at +0x20, name +0x24, description +0x88).  0x1008bf20 is
//  the device the mode callback sub_10021220 (d3dr5c.cpp) is filling in
//  for, and 0x1008bf24 the head of the RMode list it pushes onto.  The
//  types keep the tags those files already declare them under (only the
//  tag reaches the mangled name).

struct DLink                                            // ltlink.h's LTLink
{
	DLink*  m_pPrev;                                    // +0x00
	DLink*  m_pNext;                                    // +0x04
	void*   m_pData;                                    // +0x08
};

struct D3DGuid;

struct Src10021220                                      // the device record
{
	uint8           m_Reserved00[0x20];                 // +0x00..+0x1f
	D3DGuid*        m_pGuid;                            // +0x20
	char            m_szName[100];                      // +0x24
	char            m_szDesc[100];                      // +0x88
};

struct Node10021220;                                    // an RMode
struct Mode10021220;                                    // a DDSURFACEDESC2

extern DLink            g_1008bf78;                     // 0x1008bf78
extern Src10021220*     g_1008bf20;                     // 0x1008bf20
extern Node10021220*    g_1008bf24;                     // 0x1008bf24

LTBOOL  sub_10069e40();                                 // enumerate DirectDraw devices
LTBOOL  __stdcall sub_10021220(Mode10021220* pDesc, uint32 pContext);   // mode callback
void    sub_10021cb0(void* p);                          // the module's free
extern "C" void __cdecl free(void* p);                  // 0x10080b8e

// IDirectDraw (DirectDrawCreate's answer) and IDirectDraw4, as far as
// GetSupportedModes reaches them: QueryInterface, Release, and
// IDirectDraw4::EnumDisplayModes at +0x20 (index 8).
class DDrawSM
{
public:
	virtual long   __stdcall QueryInterface(const D3DGuid* riid, void** ppv) = 0;  // +0x00
	virtual uint32 __stdcall AddRef() = 0;                                          // +0x04
	virtual uint32 __stdcall Release() = 0;                                         // +0x08
	virtual long   __stdcall Compact() = 0;                                         // +0x0c
	virtual long   __stdcall CreateClipper() = 0;                                   // +0x10
	virtual long   __stdcall CreatePalette() = 0;                                   // +0x14
	virtual long   __stdcall CreateSurface() = 0;                                   // +0x18
	virtual long   __stdcall DuplicateSurface() = 0;                                // +0x1c
	virtual long   __stdcall EnumDisplayModes(uint32 dwFlags, void* pDesc, void* pContext,
	                                          LTBOOL (__stdcall* pCallback)(Mode10021220*, uint32)) = 0;  // +0x20
};

extern "C" long __stdcall DirectDrawCreate(D3DGuid* pGuid, DDrawSM** ppDD, void* pUnkOuter);

// 0x100881a8 in .rdata, straight after the eight DX7 GUIDs:
// {9C59509A-39BD-11D1-8C4A-00C04FD930C5}, defined in d3dguid.cpp with C
// linkage like the others (VC6's dxguid.lib predates it).
extern "C" const D3DGuid IID_IDirectDraw4;              // 0x100881a8

// d3dr2d_os.cpp's sub_10020f30 -- free every device record and tie the
// head off -- appears INLINE in GetSupportedModes (0x10021394..0x100213cf)
// and in Term (0x100211d5..0x10021206), both in this function's own TU,
// while 0x10069e40 (another TU) calls it.  So the source spelled it as an
// inline helper; this is that helper.  The head is stored +0x00 then +0x04,
// as the bytes show (and as sub_10020f30 does).
inline void FreeDeviceList()
{
	DLink* pCur;
	DLink* pNext;

	pCur = g_1008bf78.m_pNext;
	while (pCur != &g_1008bf78)
	{
		pNext = pCur->m_pNext;
		sub_10021cb0(pCur->m_pData);
		pCur = pNext;
	}

	g_1008bf78.m_pNext = &g_1008bf78;
	g_1008bf78.m_pPrev = &g_1008bf78;
}


// ---------------------------------------------------------------------- //
//  TWO SLOT TARGETS FROM THIS TU'S OWN REGION (0x10020f30..0x10021a7f)
// ---------------------------------------------------------------------- //

struct DDraw;
struct DDSurface;
extern DDraw*       g_pDirectDraw;                      // 0x1008bf74
extern DDSurface*   g_pOffscreen;                       // 0x1008bf68

extern "C" int __cdecl strcmp(const char* a, const char* b);   // 0x10080910

void    sub_10032cc0();                                 // d3dr2a.cpp
void    sub_100210d0();                                 // d3dbatcha.cpp
#ifdef NOLFSRC_MODERN
extern "C" LTBOOL __cdecl d3d_FillScreenRect(LTRect* pRect, uint32 nPixel);  // M-P0-03, d3dblit.cpp
#endif


// ======================================================================= //
//  0x10020fd0  62 bytes.  Slot +0xb8, GetRenderObject: the engine asks
//  for the renderer's DirectDraw object or its back buffer by name
//  (0x100a53f0 "LPDIRECTDRAW", 0x100a53e0 "BACKBUFFER"; strcmp is the CRT's
//  0x10080910).  Anything else is NULL.
// ======================================================================= //
void* sub_10020fd0(const char* pName)
{
	if (strcmp(pName, "LPDIRECTDRAW") == 0)
		return g_pDirectDraw;

	if (strcmp(pName, "BACKBUFFER") == 0)
		return g_pOffscreen;
#ifdef NOLFSRC_MODERN
	// M-P0-03: fast FillRect -- the renderer's colour fill (d3dblit.cpp)
	if (strcmp(pName, "FillScreenRect") == 0)
		return (void*)d3d_FillScreenRect;
#endif

	return LTNULL;
}


// ======================================================================= //
//  0x100211d0  75 bytes.  Slot +0x70, Term: free the device list (the
//  inline helper above, 0x100211d5..0x10021206), then 0x10032cc0 and
//  0x100210d0 -- the two teardown halves.  Init calls it on every failure
//  path.
// ======================================================================= //
void sub_100211d0()
{
	FreeDeviceList();
	sub_10032cc0();
	sub_100210d0();
}


// ---------------------------------------------------------------------- //
//  THE THREE EXPORTS
// ---------------------------------------------------------------------- //
//
//  Plain C names in the shipped export table (RenderDLLSetup,
//  GetSupportedModes, FreeModeList) -- the engine GetProcAddresses them --
//  so they are extern "C" here and build/engine.sh's d3d-link exports them
//  by those names.

extern "C" {


// ======================================================================= //
//  0x100212d0  264 bytes.  GetSupportedModes.
//
//  For every DirectDraw device sub_10069e40 finds: DirectDrawCreate on its
//  GUID (0x1007fa80 is the DDRAW.dll!DirectDrawCreate thunk),
//  QueryInterface for IDirectDraw4, and EnumDisplayModes(0, NULL, NULL,
//  sub_10021220) with 0x1008bf20 pointing at the device, so the callback
//  pushes one RMode per 16/32-bit mode onto 0x1008bf24.  Then the device
//  list is freed and the RMode list handed back.
// ======================================================================= //
RMode* GetSupportedModes()
{
	DLink*          pCur;
	Src10021220*    pDevice;
	DDrawSM*        pDD;
	DDrawSM*        pDD4;

	g_1008bf24 = LTNULL;
	sub_10069e40();

	for (pCur = g_1008bf78.m_pNext; pCur != &g_1008bf78; pCur = pCur->m_pNext)
	{
		pDevice = (Src10021220*)pCur->m_pData;

		if (DirectDrawCreate(pDevice->m_pGuid, &pDD, LTNULL) == 0)
		{
			if (pDD->QueryInterface(&IID_IDirectDraw4, (void**)&pDD4) == 0)
			{
				g_1008bf20 = pDevice;
				pDD4->EnumDisplayModes(0, LTNULL, LTNULL, sub_10021220);
				pDD4->Release();
			}

			pDD->Release();
		}
	}

	FreeDeviceList();
	return (RMode*)g_1008bf24;
}


// ======================================================================= //
//  0x100213e0  40 bytes.  FreeModeList: walk m_pNext (+0x210), CRT free
//  (0x10080b8e -- the same free sub_10021220's malloc pairs with).
// ======================================================================= //
void FreeModeList(RMode* pModes)
{
	RMode* pCur;
	RMode* pNext;

	pCur = pModes;
	while (pCur)
	{
		pNext = pCur->m_pNext;
		free(pCur);
		pCur = pNext;
	}
}


// ======================================================================= //
//  0x10021410  368 bytes.  RenderDLLSetup: remember the engine's
//  RenderStruct (0x1008cbcc) and fill its 37 renderer slots, in the order
//  the shipped stores run (not slot order: +0x9c after +0xac, +0xdc/+0xe0
//  before +0xb0, +0xf4/+0xf8 before +0xec, +0xf0 last).
// ======================================================================= //
void RenderDLLSetup(RenderStruct* pStruct)
{
	g_pStruct = pStruct;

	pStruct->Init                   = sub_100215a0;
	pStruct->Term                   = sub_100211d0;
	pStruct->BindTexture            = sub_10039fe0;
	pStruct->UnbindTexture          = sub_1003aac0;
	pStruct->CreateWorld            = d3d_CreateWorld;
	pStruct->DestroyWorld           = d3d_DestroyWorld;
	pStruct->ClearScreen            = sub_100262f0;
	pStruct->Start3D                = d3d_Start3D;
	pStruct->End3D                  = d3d_End3D;
	pStruct->IsIn3D                 = d3d_IsIn3D;
	pStruct->StartOptimized2D       = d3d_StartOptimized2D;
	pStruct->EndOptimized2D         = d3d_EndOptimized2D;
	pStruct->SetOptimized2DBlend    = d3d_SetOptimized2DBlend;
	pStruct->GetOptimized2DBlend    = d3d_GetOptimized2DBlend;
	pStruct->SetOptimized2DColor    = d3d_SetOptimized2DColor;
	pStruct->GetOptimized2DColor    = d3d_GetOptimized2DColor;
	pStruct->IsInOptimized2D        = d3d_IsInOptimized2D;
	pStruct->OptimizeSurface        = d3d_OptimizeSurface;
	pStruct->UnoptimizeSurface      = d3d_UnoptimizeSurface;
	pStruct->RenderScene            = d3d_RenderScene;
	pStruct->RenderCommand          = sub_10032d00;
	pStruct->GetHook                = sub_10020fd0;
	pStruct->SwapBuffers            = sub_100371e0;
	pStruct->GetInfoFlags           = d3d_Slot_c0;
	pStruct->GetRenderInfo          = sub_10036df0;
	pStruct->CreateSurface          = d3d_CreateSurface;
	pStruct->DeleteSurface          = d3d_DeleteSurface;
	pStruct->GetSurfaceInfo         = d3d_GetSurfaceInfo;
	pStruct->LockSurface            = d3d_LockSurface;
	pStruct->UnlockSurface          = d3d_UnlockSurface;
	pStruct->LockScreen             = d3d_LockScreen;
	pStruct->UnlockScreen           = d3d_UnlockScreen;
	pStruct->MakeScreenShot         = sub_10036e40;
	pStruct->ReadConsoleVariables   = sub_10021e50;
	pStruct->BlitToScreen           = d3d_BlitToScreen;
	pStruct->BlitFromScreen         = d3d_BlitFromScreen;
	pStruct->BlitWarpedToScreen     = d3d_BlitWarpedToScreen;
}


}   // extern "C"


// ---------------------------------------------------------------------- //
//  INIT
// ---------------------------------------------------------------------- //

struct D3DDeviceRec;
struct W26Rect
{
	long    left;                                       // +0x00
	long    top;                                        // +0x04
	long    right;                                      // +0x08
	long    bottom;                                     // +0x0c
};

extern "C"
{
	__declspec(dllimport) void* __stdcall GetDesktopWindow();                         // *0x10088114
	__declspec(dllimport) int   __stdcall GetWindowRect(void* hWnd, W26Rect* pRect);  // *0x1008810c
	__declspec(dllimport) int   __stdcall AdjustWindowRect(W26Rect* pRect, uint32 dwStyle,
	                                                       int bMenu);                // *0x10088110
	__declspec(dllimport) int   __stdcall SetWindowPos(void* hWnd, void* hAfter, int x, int y,
	                                                   int cx, int cy, uint32 uFlags); // *0x10088108

	int     __cdecl atoi(const char* p);                                              // 0x10080c61
	char*   __cdecl strncpy(char* pDst, const char* pSrc, unsigned int n);            // 0x10080a90
}

// 0x10091a10, the renderer root (0x1007b8d0 answers its address); its
// Init is w26_1007be90.cpp.
class VisibleSet
{
public:
	LTBOOL  sub_1007be90();                             // 0x1007be90
};

void    sub_10021dc0();                                 // d3dr3c.cpp
LTBOOL  sub_10033120();                                 // d3dr3a.cpp
void*   sub_1007b8d0();                                 // d3dneutral.cpp
void    sub_10021ed0(char* pName);                      // d3dbatchd.cpp
LTBOOL  sub_100331e0(D3DDeviceRec* pDevice, RenderStructInit* pInit);   // big331e0.cpp
void    sub_10021d60(uint32 nLevel, const char* pszFmt, ...);          // dlog

extern LTBOOL       g_bWindowed;                        // 0x1008cbbc
extern void*        g_hWnd;                             // 0x1008cbc0
extern uint32       g_nWindowWidth;                     // 0x1008cbb4
extern uint32       g_nWindowHeight;                    // 0x1008cbb0
extern uint32       g_nSurfCaps1008cbb8;                // 0x1008cbb8
extern uint32       g_1008cba8;                         // 0x1008cba8
extern uint32       g_1008cbac;                         // 0x1008cbac frame count
extern uint32       g_1008ca44;                         // 0x1008ca44 filter state valid
extern uint32       g_1008ca4c;                         // 0x1008ca4c dither state valid
extern uint32       g_1008ca8c;                         // 0x1008ca8c fog state valid
extern int32        g_bWarbleTableValid;                // 0x1008ca94
extern DDSurface*   g_pPrimary;                         // 0x1008bf6c

// NEW here (no TU declared them before): the "SysMem" console value,
// stored as an unsigned (the conversion is icl's 64-bit fistpll), and the
// two screen divisors Init sets to 1 and divides the mode's width/height
// by (and the engine's RenderStruct +0x3c/+0x40 after the device is up).
extern uint32       g_1008cb9c;                         // 0x1008cb9c
extern uint32       g_1008cb78;                         // 0x1008cb78
extern uint32       g_1008cb74;                         // 0x1008cb74

// 0x1008bf28, the back buffer's pixel format (0x10021070 fills it).
extern PFormat      g_1008bf28;                         // 0x1008bf28

// PFormat's vtable (d3ddata.cpp's g_vtbl_10089390; w12_79fd0_os.cpp spells
// it this way too).
extern uint8        g_10089390[];                       // 0x10089390


// ======================================================================= //
//  0x10021580  26 bytes, this TU's static initialiser (the CRT's
//  initialiser table holds it, at 0x10089060):
//
//      mov $0x1008bf78,%eax
//      movl $0x10089390,0x1008bf28        the PFormat's vptr
//      mov %eax,0x1008bf78                the device list head, tied off
//      mov %eax,0x1008bf7c
//      ret
//
//  So the shipped TU DEFINED those two objects (a PFormat, whose
//  constructor stores the vptr, and an LTLink head that ties itself off).
//  Here they are defined by d3ddata.cpp as plain BSS, so this TU runs the
//  same initialisation from a file-scope object's constructor -- without
//  it the head is zero and the first sub_10020f30 walks NULL (the linked
//  d3d.ren faulted there on load in GetSupportedModes, measured with
//  `d3d-link test`).  Compiler-generated, so not measured byte for byte.
// ======================================================================= //
struct TUInit10021580
{
	TUInit10021580()
	{
		*(void**)&g_1008bf28 = g_10089390;
		g_1008bf78.m_pNext = &g_1008bf78;
		g_1008bf78.m_pPrev = &g_1008bf78;
	}
};
static TUInit10021580 s_TUInit10021580;


// ======================================================================= //
//  0x100215a0  1247 bytes (to 0x10021a7f; the csv's 979 stops at the
//  phantom 0x10021973, which is this function's shared `return 1` tail).
//  Slot +0x6c, Init(RenderStructInit*).
//
//    * the version handshake (0xd5d into the OUT word), then 0x10021dc0
//      and 0x10021e50 (console variables);
//    * "windowed" (GetConsoleVar +0x18, GetVarValueString +0x20, atoi == 1)
//      and "SysMem" (+0x18, GetVarValueFloat +0x1c);
//    * the screen divisors (both 1), DDSCAPS_VIDEOMEMORY, the window, the
//      mode size;
//    * sub_10069e40 enumerates the DirectDraw devices -- failure is 10;
//    * the device the engine asked for by name (both names normalised
//      through 0x10021ed0 into 100-byte buffers), else the first device
//      0x100331e0 can initialise;
//    * the chosen device's name and description go back into the RMode,
//      five render-state caches are invalidated;
//    * windowed (or "nullrender" != 0): centre the window -- the vertical
//      offset really is computed from the desktop WIDTH and the mode
//      WIDTH, as the bytes do (-0x44/-0x4c and +0x208 both times) --
//      AdjustWindowRect(WS_OVERLAPPEDWINDOW), clamp to the screen,
//      SetWindowPos(SWP_NOOWNERZORDER); fullscreen: HWND_TOPMOST with
//      SWP_NOMOVE|SWP_NOSIZE;
//    * the back buffer's pixel format (0x10021070), 0x10033120, the
//      engine's screen size divided down, and VisibleSet::Init.
//  Returns 0 on success, 1 on failure (after Term), 10 if DirectDraw
//  cannot be enumerated.
// ======================================================================= //
// The named-device search, inlined into Init: ship tests its result with
// `xor %edx,%edx ; test %edx,%edx ; jne` after the loop -- an inlined
// `return LTNULL` joined with the in-loop `return pDev`, not a flag local.
// NEAR (Init, 1272 against 1247): with this the search loop is ship's; left
// are the block layout around it (ship threads the not-found arm straight
// into the fallback loop and keeps the named search as the fall-through)
// and the error returns (ship hosts the epilogue in the r_GetBufferFormat
// failure and shares its `add $8 ; return 1` with the other two; a goto to
// one `return 1`, a positive guard on r_GetBufferFormat, a second inline for
// the fallback loop and if/else spellings of the found test do not).
static inline Src10021220* FindDev100215a0(const char* pName)
{
	DLink*			pCur;
	Src10021220*	pDev;
	char			szWant[100];
	char			szHave[100];

	strncpy(szWant, pName, sizeof(szWant));
	sub_10021ed0(szWant);

	for (pCur = g_1008bf78.m_pNext; pCur != &g_1008bf78; pCur = pCur->m_pNext)
	{
		pDev = (Src10021220*)pCur->m_pData;

		strncpy(szHave, pDev->m_szName, sizeof(szHave));
		sub_10021ed0(szHave);

		if (strcmp(szWant, szHave) == 0)
			return pDev;
	}

	return LTNULL;
}

int32 sub_100215a0(RenderStructInit* pInit)
{
	char*           pValue;
	char*           pWant;
	Src10021220*    pDevice;
	Src10021220*    pFound;
	Src10021220*    pDev;
	DLink*          pCur;
	W26Rect         rcDesk;
	W26Rect         rc;

	pInit->m_nVersion = 0xd5d;

	sub_10021dc0();
	sub_10021e50();

	pValue = g_pStruct->GetVarValueString(g_pStruct->GetConsoleVar("windowed"));
	g_bWindowed = (pValue && atoi(pValue) == 1);

	g_1008cb9c = (uint32)g_pStruct->GetVarValueFloat(g_pStruct->GetConsoleVar("SysMem"));

	g_1008cb78 = 1;
	g_1008cb74 = 1;

	g_nSurfCaps1008cbb8 = 0x4000;
	g_1008cba8 = 0;

	g_hWnd = pInit->m_hWnd;
	g_nWindowWidth  = pInit->m_Mode.m_Width / g_1008cb78;
	g_nWindowHeight = pInit->m_Mode.m_Height / g_1008cb74;

	if (!sub_10069e40())
		return 10;

	pDevice = LTNULL;

	pWant = pInit->m_Mode.m_InternalName;
	if (pWant[0])
	{
		pFound = FindDev100215a0(pWant);

		if (!pFound)
		{
			sub_10021d60(1, "Can't find hardware device: %s", pWant);
		}
		else if (sub_100331e0((D3DDeviceRec*)pFound, pInit))
		{
			pDevice = pFound;
		}
		else
		{
			sub_10021d60(1, "Can't initialize hardware device: %s", pWant);
		}
	}

	if (!pDevice)
	{
		for (pCur = g_1008bf78.m_pNext; pCur != &g_1008bf78; pCur = pCur->m_pNext)
		{
			pDev = (Src10021220*)pCur->m_pData;

			if (sub_100331e0((D3DDeviceRec*)pDev, pInit))
			{
				pDevice = pDev;
				break;
			}
		}
	}

	if (!pDevice)
	{
		sub_100211d0();
		sub_10021d60(0, "Can't find any d3d devices to use!");
		return 1;
	}

	strncpy(pInit->m_Mode.m_InternalName, pDevice->m_szName, 0x7f);
	strncpy(pInit->m_Mode.m_Description, pDevice->m_szDesc, 0x7f);
	sub_10021d60(0, "Using Direct3D Device %s", pDevice->m_szName);

	g_1008ca44 = 0;
	g_1008ca4c = 0;
	g_1008ca8c = 0;
	g_bWarbleTableValid = 0;
	g_1008cbac = 0;

	if (g_bWindowed ||
	    g_pStruct->GetVarValueFloat(g_pStruct->GetConsoleVar("nullrender")) != 0.0f)
	{
		GetWindowRect(GetDesktopWindow(), &rcDesk);

		rc.left   = (rcDesk.right - rcDesk.left - pInit->m_Mode.m_Width) / 2;
		rc.top    = (rcDesk.right - rcDesk.left - pInit->m_Mode.m_Width) / 2;
		rc.right  = pInit->m_Mode.m_Width + rc.left;
		rc.bottom = pInit->m_Mode.m_Height + rc.top;

		AdjustWindowRect(&rc, 0x00cf0000, 0);

		if (rc.left < 0)
		{
			rc.right -= rc.left;
			rc.left = 0;
		}

		if (rc.top < 0)
		{
			rc.bottom -= rc.top;
			rc.top = 0;
		}

		SetWindowPos(g_hWnd, LTNULL, rc.left, rc.top,
		             rc.right - rc.left, rc.bottom - rc.top, 0x200);
	}
	else
	{
		SetWindowPos(g_hWnd, (void*)-1, 0, 0, 0, 0, 3);
	}

	if (!sub_10021070((DDSurface7*)g_pPrimary, &g_1008bf28))
	{
		sub_100211d0();
		sub_10021d60(0, "r_GetBufferFormatOfSurface failed.");
		return 1;
	}

	sub_10033120();

	g_pStruct->m_Width  /= g_1008cb78;
	g_pStruct->m_Height /= g_1008cb74;

	if (((VisibleSet*)sub_1007b8d0())->sub_1007be90())
		return 0;

	sub_100211d0();
	sub_10021d60(0, "VisibleSet::Init failed (invalid object list size?).");
	return 1;
}
