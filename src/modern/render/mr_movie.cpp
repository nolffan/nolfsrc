// mr_movie.cpp -- the screen-mode movies.
//
// The engine's Bink and Smacker players (binkvideo.cpp, smackvideo.cpp)
// draw a movie through DirectDraw: GetHook("LPDIRECTDRAW") for an object
// to make a surface with, GetHook("BACKBUFFER") for the surface to Blt
// each frame into. d3d.ren hands over its own DirectDraw objects. Here the
// two are small stand-ins with DirectDraw's vtable layout (IDirectDraw7,
// IDirectDrawSurface7): the movie surface is system memory the player
// decodes into, and a Blt or BltFast of it onto the back buffer uploads it
// and draws it into the back buffer, which the engine's FlipScreen then
// shows. The few calls the players make are implemented; the rest answer
// E_NOTIMPL, each with its own argument count (COM methods are __stdcall
// and pop their own arguments).

#include "mrscene.h"

#define DD_OK           0
#define DD_NOTIMPL      ((HRESULT)0x80004001)
#define DD_INVALID      ((HRESULT)0x80070057)
#define DDBLT_COLORFILL 0x00000400

// ---- the descriptors (ddraw.h's DDSURFACEDESC2, DDPIXELFORMAT, DDBLTFX) ---------------

struct MRPixelFormat {                 // 0x20
    uint32_t dwSize, dwFlags, dwFourCC, dwRGBBitCount;
    uint32_t dwRBitMask, dwGBitMask, dwBBitMask, dwRGBAlphaBitMask;
};
struct MRSurfaceDesc {                 // 0x7c
    uint32_t dwSize, dwFlags, dwHeight, dwWidth;
    int32_t  lPitch;
    uint32_t dwBackBufferCount, dwMipMapCount, dwAlphaBitDepth, dwReserved;
    void*    lpSurface;
    uint32_t ddck[8];
    MRPixelFormat ddpfPixelFormat;     // 0x48
    uint32_t ddsCaps[4];                // 0x68
    uint32_t dwTextureStage;            // 0x78
};
static_assert(sizeof(MRSurfaceDesc) == 0x7c, "DDSURFACEDESC2");
#define DDSD_CAPS        0x00000001
#define DDSD_HEIGHT      0x00000002
#define DDSD_WIDTH       0x00000004
#define DDSD_PITCH       0x00000008
#define DDSD_PIXELFORMAT 0x00001000
#define DDSD_LPSURFACE   0x00000800
#define DDPF_RGB         0x00000040

// X8R8G8B8: the 32-bit format the players decode into (BINKSURFACE32).
static void mr_MoviePixelFormat(MRPixelFormat* p)
{
    memset(p, 0, sizeof(*p));
    p->dwSize = sizeof(*p);
    p->dwFlags = DDPF_RGB;
    p->dwRGBBitCount = 32;
    p->dwRBitMask = 0x00ff0000;
    p->dwGBitMask = 0x0000ff00;
    p->dwBBitMask = 0x000000ff;
}

// ---- the objects -------------------------------------------------------------------

struct MRDDSurface {
    void**    m_pVtbl;
    long      m_nRefs;
    bool      m_bBackBuffer;
    uint32_t  m_nWidth, m_nHeight;
    uint32_t* m_pPixels;                // X8R8G8B8, pitch = width * 4
};
struct MRDirectDraw {
    void** m_pVtbl;
};

// Stubs by argument count (after `this`).
#define STUB(n, args) static HRESULT __stdcall dd_Stub##n args { return DD_NOTIMPL; }
STUB(0, (void*))
STUB(1, (void*, void*))
STUB(2, (void*, void*, void*))
STUB(3, (void*, void*, void*, void*))
STUB(4, (void*, void*, void*, void*, void*))
STUB(5, (void*, void*, void*, void*, void*, void*))
#undef STUB
static ULONG __stdcall dd_AddRef(void* p) { return ++((MRDDSurface*)p)->m_nRefs; }
static HRESULT __stdcall dd_QueryInterface(void*, void*, void** ppOut)
{
    if (ppOut)
        *ppOut = NULL;
    return (HRESULT)0x80004002;         // E_NOINTERFACE
}

// ---- surfaces ------------------------------------------------------------------------

static MRDDSurface g_BackBuffer;
static uint32_t g_MovieTex;
static uint32_t g_nMovieTexW, g_nMovieTexH, g_nMovieTexGen;

static ULONG __stdcall surf_Release(void* p)
{
    MRDDSurface* s = (MRDDSurface*)p;
    if (s->m_bBackBuffer)
        return 1;
    long n = --s->m_nRefs;
    if (n <= 0)
    {
        free(s->m_pPixels);
        free(s);
        return 0;
    }
    return (ULONG)n;
}

static void mr_FillRect(MRDDSurface* s, const RECT* pRect, uint32_t nColor)
{
    if (s->m_bBackBuffer)
    {
        if (!g.bContext)
            return;
        RECT r = { 0, 0, (LONG)g.nWidth, (LONG)g.nHeight };
        if (pRect)
            r = *pRect;
        rd_PushAttrib(RD_ATTR_SCISSOR | RD_ATTR_COLOR);
        rd_Enable(RD_SCISSOR_TEST);
        rd_Scissor(r.left, r.top, r.right - r.left, r.bottom - r.top);
        rd_ClearColor(((nColor >> 16) & 0xff) / 255.0f, ((nColor >> 8) & 0xff) / 255.0f, (nColor & 0xff) / 255.0f, 1.0f);
        rd_Clear(RD_CLEAR_COLOR);
        rd_PopAttrib();
        return;
    }
    RECT r = { 0, 0, (LONG)s->m_nWidth, (LONG)s->m_nHeight };
    if (pRect)
        r = *pRect;
    for (LONG y = r.top < 0 ? 0 : r.top; y < r.bottom && y < (LONG)s->m_nHeight; y++)
        for (LONG x = r.left < 0 ? 0 : r.left; x < r.right && x < (LONG)s->m_nWidth; x++)
            s->m_pPixels[y * s->m_nWidth + x] = nColor;
}

// The movie frame onto the screen: upload, then a textured quad.
static void mr_DrawMovie(MRDDSurface* pSrc, const RECT& dst, const RECT& src)
{
    if (!g.bContext || !pSrc || pSrc->m_bBackBuffer || !pSrc->m_pPixels)
        return;
    uint32_t w = pSrc->m_nWidth, h = pSrc->m_nHeight;
    uint32_t tw = 1, th = 1;
    while (tw < w) tw <<= 1;
    while (th < h) th <<= 1;
    mr_Begin2D();
    if (!g_MovieTex || g_nMovieTexGen != g.nContextGen || g_nMovieTexW != tw || g_nMovieTexH != th)
    {
        if (g_MovieTex && g_nMovieTexGen == g.nContextGen)
            rd_DeleteTexture(g_MovieTex);
        g_MovieTex = rd_GenTexture();
        rd_BindTexture(g_MovieTex);
        rd_TexFilter(RD_LINEAR, RD_LINEAR);
        rd_TexWrap(false, false);
        rd_TexMaxLevel(0);
        rd_TexImage(0, RD_FMT_RGB8, tw, th, RD_PIX_BGRA, NULL);
        g_nMovieTexW = tw;
        g_nMovieTexH = th;
        g_nMovieTexGen = g.nContextGen;
    }
    rd_BindTexture(g_MovieTex);
    rd_TexSubImage(0, 0, 0, w, h, RD_PIX_BGRA, pSrc->m_pPixels);
    rd_Enable(RD_TEXTURE_2D);
    rd_Disable(RD_BLEND);
    rd_Disable(RD_ALPHA_TEST);
    rd_TexEnv(RD_ENV_REPLACE);
    float u0 = (float)src.left / tw, u1 = (float)src.right / tw;
    float v0 = (float)src.top / th, v1 = (float)src.bottom / th;
    rd_Begin(RD_QUADS);
    rd_TexCoord2f(u0, v0); rd_Vertex2f((float)dst.left, (float)dst.top);
    rd_TexCoord2f(u1, v0); rd_Vertex2f((float)dst.right, (float)dst.top);
    rd_TexCoord2f(u1, v1); rd_Vertex2f((float)dst.right, (float)dst.bottom);
    rd_TexCoord2f(u0, v1); rd_Vertex2f((float)dst.left, (float)dst.bottom);
    rd_End();
    mr_End2D();
    mr_ForgetBindings();
}

static HRESULT __stdcall surf_Blt(void* p, RECT* pDst, void* pSrcSurf, RECT* pSrc, DWORD nFlags, void* pFX)
{
    MRLock lock("movie Blt");           // the engine calls these, not through a slot
    MRDDSurface* s = (MRDDSurface*)p;
    if (nFlags & DDBLT_COLORFILL)
    {
        uint32_t nColor = pFX ? *(uint32_t*)((uint8_t*)pFX + 0x50) : 0;
        mr_FillRect(s, pDst, nColor);
        return DD_OK;
    }
    MRDDSurface* pFrom = (MRDDSurface*)pSrcSurf;
    if (!s->m_bBackBuffer || !pFrom)
        return DD_NOTIMPL;
    RECT src = { 0, 0, (LONG)pFrom->m_nWidth, (LONG)pFrom->m_nHeight };
    RECT dst = { 0, 0, (LONG)g.nWidth, (LONG)g.nHeight };
    if (pSrc)
        src = *pSrc;
    if (pDst)
        dst = *pDst;
    mr_DrawMovie(pFrom, dst, src);
    return DD_OK;
}

static HRESULT __stdcall surf_BltFast(void* p, DWORD x, DWORD y, void* pSrcSurf, RECT* pSrc, DWORD)
{
    MRLock lock("movie BltFast");
    MRDDSurface* s = (MRDDSurface*)p;
    MRDDSurface* pFrom = (MRDDSurface*)pSrcSurf;
    if (!s->m_bBackBuffer || !pFrom)
        return DD_NOTIMPL;
    RECT src = { 0, 0, (LONG)pFrom->m_nWidth, (LONG)pFrom->m_nHeight };
    if (pSrc)
        src = *pSrc;
    RECT dst = { (LONG)x, (LONG)y, (LONG)x + (src.right - src.left), (LONG)y + (src.bottom - src.top) };
    mr_DrawMovie(pFrom, dst, src);
    return DD_OK;
}

static void mr_FillDesc(MRDDSurface* s, MRSurfaceDesc* d)
{
    uint32_t nSize = d->dwSize ? d->dwSize : sizeof(*d);
    memset(d, 0, nSize < sizeof(*d) ? nSize : sizeof(*d));
    d->dwSize = nSize;
    d->dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT;
    d->dwWidth = s->m_bBackBuffer ? g.nWidth : s->m_nWidth;
    d->dwHeight = s->m_bBackBuffer ? g.nHeight : s->m_nHeight;
    d->lPitch = (int32_t)(d->dwWidth * 4);
    mr_MoviePixelFormat(&d->ddpfPixelFormat);
}

static HRESULT __stdcall surf_GetSurfaceDesc(void* p, void* pDesc)
{
    if (!pDesc)
        return DD_INVALID;
    mr_FillDesc((MRDDSurface*)p, (MRSurfaceDesc*)pDesc);
    return DD_OK;
}

static HRESULT __stdcall surf_GetPixelFormat(void*, void* pFormat)
{
    if (!pFormat)
        return DD_INVALID;
    mr_MoviePixelFormat((MRPixelFormat*)pFormat);
    return DD_OK;
}

static HRESULT __stdcall surf_Lock(void* p, RECT*, void* pDesc, DWORD, void*)
{
    MRDDSurface* s = (MRDDSurface*)p;
    if (s->m_bBackBuffer || !s->m_pPixels || !pDesc)
        return DD_NOTIMPL;
    MRSurfaceDesc* d = (MRSurfaceDesc*)pDesc;
    mr_FillDesc(s, d);
    d->dwFlags |= DDSD_LPSURFACE;
    d->lpSurface = s->m_pPixels;
    return DD_OK;
}

static HRESULT __stdcall surf_Unlock(void*, void*) { return DD_OK; }
static HRESULT __stdcall surf_IsLost(void*) { return DD_OK; }
static HRESULT __stdcall surf_Restore(void*) { return DD_OK; }

static void* g_SurfaceVtbl[49] = {
    (void*)dd_QueryInterface, (void*)dd_AddRef, (void*)surf_Release,
    (void*)dd_Stub1,            // AddAttachedSurface
    (void*)dd_Stub1,            // AddOverlayDirtyRect
    (void*)surf_Blt,
    (void*)dd_Stub3,            // BltBatch
    (void*)surf_BltFast,
    (void*)dd_Stub2,            // DeleteAttachedSurface
    (void*)dd_Stub2,            // EnumAttachedSurfaces
    (void*)dd_Stub3,            // EnumOverlayZOrders
    (void*)dd_Stub2,            // Flip
    (void*)dd_Stub2,            // GetAttachedSurface
    (void*)dd_Stub1,            // GetBltStatus
    (void*)dd_Stub1,            // GetCaps
    (void*)dd_Stub1,            // GetClipper
    (void*)dd_Stub2,            // GetColorKey
    (void*)dd_Stub1,            // GetDC
    (void*)dd_Stub1,            // GetFlipStatus
    (void*)dd_Stub2,            // GetOverlayPosition
    (void*)dd_Stub1,            // GetPalette
    (void*)surf_GetPixelFormat,
    (void*)surf_GetSurfaceDesc,
    (void*)dd_Stub2,            // Initialize
    (void*)surf_IsLost,
    (void*)surf_Lock,
    (void*)dd_Stub1,            // ReleaseDC
    (void*)surf_Restore,
    (void*)dd_Stub1,            // SetClipper
    (void*)dd_Stub2,            // SetColorKey
    (void*)dd_Stub2,            // SetOverlayPosition
    (void*)dd_Stub1,            // SetPalette
    (void*)surf_Unlock,
    (void*)dd_Stub5,            // UpdateOverlay
    (void*)dd_Stub1,            // UpdateOverlayDisplay
    (void*)dd_Stub2,            // UpdateOverlayZOrder
    (void*)dd_Stub1,            // GetDDInterface
    (void*)dd_Stub1,            // PageLock
    (void*)dd_Stub1,            // PageUnlock
    (void*)dd_Stub2,            // SetSurfaceDesc
    (void*)dd_Stub4,            // SetPrivateData
    (void*)dd_Stub3,            // GetPrivateData
    (void*)dd_Stub1,            // FreePrivateData
    (void*)dd_Stub1,            // GetUniquenessValue
    (void*)dd_Stub0,            // ChangeUniquenessValue
    (void*)dd_Stub1,            // SetPriority
    (void*)dd_Stub1,            // GetPriority
    (void*)dd_Stub1,            // SetLOD
    (void*)dd_Stub1,            // GetLOD
};

// ---- the DirectDraw object --------------------------------------------------------------

static ULONG __stdcall dd_Release(void*) { return 1; }
static ULONG __stdcall dd_AddRefDD(void*) { return 2; }

static HRESULT __stdcall dd_CreateSurface(void*, void* pDesc, void** ppSurface, void*)
{
    if (!pDesc || !ppSurface)
        return DD_INVALID;
    const MRSurfaceDesc* d = (const MRSurfaceDesc*)pDesc;
    *ppSurface = NULL;
    if (!(d->dwFlags & DDSD_WIDTH) || !(d->dwFlags & DDSD_HEIGHT) || !d->dwWidth || !d->dwHeight ||
        d->dwWidth > 4096 || d->dwHeight > 4096)
        return DD_INVALID;
    MRDDSurface* s = (MRDDSurface*)calloc(1, sizeof(MRDDSurface));
    if (!s)
        return (HRESULT)0x8007000e;     // E_OUTOFMEMORY
    s->m_pVtbl = g_SurfaceVtbl;
    s->m_nRefs = 1;
    s->m_nWidth = d->dwWidth;
    s->m_nHeight = d->dwHeight;
    s->m_pPixels = (uint32_t*)calloc(d->dwWidth * d->dwHeight, 4);
    if (!s->m_pPixels)
    {
        free(s);
        return (HRESULT)0x8007000e;
    }
    *ppSurface = s;
    return DD_OK;
}

static HRESULT __stdcall dd_GetDisplayMode(void*, void* pDesc)
{
    if (!pDesc)
        return DD_INVALID;
    mr_FillDesc(&g_BackBuffer, (MRSurfaceDesc*)pDesc);
    return DD_OK;
}

static HRESULT __stdcall dd_Ok0(void*) { return DD_OK; }

static void* g_DDVtbl[30] = {
    (void*)dd_QueryInterface, (void*)dd_AddRefDD, (void*)dd_Release,
    (void*)dd_Ok0,              // Compact
    (void*)dd_Stub3,            // CreateClipper
    (void*)dd_Stub4,            // CreatePalette
    (void*)dd_CreateSurface,
    (void*)dd_Stub2,            // DuplicateSurface
    (void*)dd_Stub4,            // EnumDisplayModes
    (void*)dd_Stub4,            // EnumSurfaces
    (void*)dd_Ok0,              // FlipToGDISurface
    (void*)dd_Stub2,            // GetCaps
    (void*)dd_GetDisplayMode,
    (void*)dd_Stub2,            // GetFourCCCodes
    (void*)dd_Stub1,            // GetGDISurface
    (void*)dd_Stub1,            // GetMonitorFrequency
    (void*)dd_Stub1,            // GetScanLine
    (void*)dd_Stub1,            // GetVerticalBlankStatus
    (void*)dd_Stub1,            // Initialize
    (void*)dd_Ok0,              // RestoreDisplayMode
    (void*)dd_Stub2,            // SetCooperativeLevel
    (void*)dd_Stub5,            // SetDisplayMode
    (void*)dd_Stub2,            // WaitForVerticalBlank
    (void*)dd_Stub3,            // GetAvailableVidMem
    (void*)dd_Stub2,            // GetSurfaceFromDC
    (void*)dd_Ok0,              // RestoreAllSurfaces
    (void*)dd_Ok0,              // TestCooperativeLevel
    (void*)dd_Stub2,            // GetDeviceIdentifier
    (void*)dd_Stub3,            // StartModeTest
    (void*)dd_Stub2,            // EvaluateMode
};

static MRDirectDraw g_DirectDraw = { g_DDVtbl };

void* mr_MovieHook(const char* pName)
{
    if (!g.bContext || !pName)
        return NULL;
    if (!_stricmp(pName, "LPDIRECTDRAW"))
        return &g_DirectDraw;
    if (!_stricmp(pName, "BACKBUFFER"))
    {
        g_BackBuffer.m_pVtbl = g_SurfaceVtbl;
        g_BackBuffer.m_nRefs = 1;
        g_BackBuffer.m_bBackBuffer = true;
        return &g_BackBuffer;
    }
    return NULL;
}
