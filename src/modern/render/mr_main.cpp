// mr_main.cpp -- the renderer's exports, device and frame: Init/Term (the
// backend's device on the engine's window), the mode list, clearing and
// flipping, console variables and the small slots.
//
// The shape follows the Mac release's opengl_init.cpp (r_Init, r_Term,
// r_Clear, r_SwapBuffers, glRenderDLLSetup, glGetSupportedModes), with AGL
// and DrawSprocket replaced by mrplatform.h (mr_window.cpp on Windows) and
// rdev.h, and the behaviour the PC engine expects from d3d.ren where the
// two differ.

#include "mr.h"

MRGlobals g;

// ---- console ---------------------------------------------------------------

void mr_Print(const char* pFormat, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, pFormat);
    vsnprintf(buf, sizeof(buf), pFormat, ap);
    va_end(ap);
    if (g.pStruct && g.pStruct->ConsolePrint)
        g.pStruct->ConsolePrint("%s", buf);
}

// One line into the backend's log (opengl.log) beside the game.
static void mr_LogLine(const char* pLine)
{
    static FILE* f;
    if (!f)
        f = fopen(g_RDLog, "w");
    if (f)
    {
        fprintf(f, "%s\n", pLine);
        fflush(f);
    }
}

// RenderDebug 1+ goes to the console; RenderDebug 3+ also to the backend's
// log (opengl.log) beside the game, which survives a crash.
void mr_Debug(int nLevel, const char* pFormat, ...)
{
    if (cvi(CV_RenderDebug) < nLevel)
        return;
    char buf[512];
    va_list ap;
    va_start(ap, pFormat);
    vsnprintf(buf, sizeof(buf), pFormat, ap);
    va_end(ap);
    mr_Print("%s: %s", g_RDDll, buf);
    if (cvi(CV_RenderDebug) >= 3)
        mr_LogLine(buf);
}

// ---- one thread at a time (mr.h) ----------------------------------------------

static CRITICAL_SECTION s_csLock;
static volatile LONG    s_nLockInit;            // 0, 1 (being made), 2 (made)
static DWORD            s_nLockOwner;           // valid while s_nLockDepth > 0
static int              s_nLockDepth;
#ifdef NOLFSRC_DEBUG
static const char* volatile s_pLockWhat;        // the outermost call that holds it
static DWORD            s_nMainThread;          // the thread Init ran on
#endif

static void mr_InitLock(void)
{
    if (s_nLockInit == 2)
        return;
    if (InterlockedCompareExchange(&s_nLockInit, 1, 0) == 0)
    {
        InitializeCriticalSection(&s_csLock);
        s_nLockInit = 2;
    }
    while (s_nLockInit != 2)
        Sleep(0);
}

MRLock::MRLock(const char* pWhat)
{
    (void)pWhat;
    mr_InitLock();
#ifdef NOLFSRC_DEBUG
    // Another thread is in the renderer now: without the lock, the two
    // would have used the device and the core's state at the same time.
    if (!TryEnterCriticalSection(&s_csLock))
    {
        static volatile LONG s_nReports;
        const char* pOther = s_pLockWhat;
        DWORD nOther = s_nLockOwner;
        EnterCriticalSection(&s_csLock);
        if (InterlockedIncrement(&s_nReports) <= 20)
            mr_CheckFailed(__FILE__, __LINE__, false,
                           "%s on thread %lu waited: thread %lu was in %s (two threads in the renderer)",
                           pWhat, (unsigned long)GetCurrentThreadId(), (unsigned long)nOther,
                           pOther ? pOther : "?");
    }
#else
    EnterCriticalSection(&s_csLock);
#endif
    if (s_nLockDepth++ == 0)
    {
        s_nLockOwner = GetCurrentThreadId();
#ifdef NOLFSRC_DEBUG
        s_pLockWhat = pWhat;
        // Each slot the first time it comes from a thread other than Init's.
        if (s_nMainThread && s_nLockOwner != s_nMainThread)
        {
            static const char* s_pSeen[64];
            static int s_nSeen;
            bool bSeen = false;
            for (int i = 0; i < s_nSeen && !bSeen; i++)
                bSeen = s_pSeen[i] == pWhat;
            if (!bSeen && s_nSeen < 64)
            {
                s_pSeen[s_nSeen++] = pWhat;
                mr_Print("%s: %s called on thread %lu (Init ran on %lu)", g_RDDll, pWhat,
                         (unsigned long)s_nLockOwner, (unsigned long)s_nMainThread);
                fprintf(stderr, "%s: %s called on thread %lu (Init ran on %lu)\n", g_RDDll, pWhat,
                        (unsigned long)s_nLockOwner, (unsigned long)s_nMainThread);
            }
        }
#endif
    }
}

MRLock::~MRLock()
{
    if (--s_nLockDepth == 0)
    {
        s_nLockOwner = 0;
#ifdef NOLFSRC_DEBUG
        s_pLockWhat = NULL;
#endif
    }
    LeaveCriticalSection(&s_csLock);
}

bool mr_LockHeld(void)
{
    return s_nLockDepth > 0 && s_nLockOwner == GetCurrentThreadId();
}

// A slot's function with the lock held around it: MRLocked<name, fn>::Call
// has fn's own type, so it goes in the slot fn went in.
template <const char* N, auto F> struct MRLocked;
template <const char* N, typename R, typename... A, R (*F)(A...)>
struct MRLocked<N, F> {
    static R Call(A... a)
    {
        MRLock lock(N);
        return F(a...);
    }
};

#ifdef NOLFSRC_DEBUG
void mr_CheckFailed(const char* pFile, int nLine, bool bFatal, const char* pFormat, ...)
{
    // Once per site (the first 64 sites), so a check that fails every frame
    // does not flood the log; a fatal one always reports.
    static struct { const char* m_pFile; int m_nLine; } s_Sites[64];
    static int s_nSites;
    if (!bFatal)
    {
        for (int i = 0; i < s_nSites; i++)
            if (s_Sites[i].m_nLine == nLine && s_Sites[i].m_pFile == pFile)
                return;
        if (s_nSites >= 64)
            return;
        s_Sites[s_nSites].m_pFile = pFile;
        s_Sites[s_nSites].m_nLine = nLine;
        s_nSites++;
    }
    char msg[512], line[700];
    va_list ap;
    va_start(ap, pFormat);
    vsnprintf(msg, sizeof(msg), pFormat, ap);
    va_end(ap);
    const char* pName = strrchr(pFile, '/');
    pName = pName ? pName + 1 : pFile;
    snprintf(line, sizeof(line), "RENDER %s %s:%d: %s", bFatal ? "ASSERT" : "CHECK", pName, nLine, msg);
    mr_Print("%s: %s", g_RDDll, line);
    fprintf(stderr, "%s: %s\n", g_RDDll, line);
    fflush(stderr);
    OutputDebugStringA(line);
    mr_LogLine(line);
    if (bFatal)
        __builtin_trap();
}
#endif

// The names and defaults d3d.ren and the Mac renderer register, where they
// share one; VSync and Antialiasing are ours.
MRVar g_Vars[CV_Count] = {
    { "RenderDebug", 0 }, { "Bilinear", 1 }, { "Trilinear", 0 }, { "Anisotropic", 0 },
    { "MipmapOffset", 0 }, { "FilterOptimized", 0 }, { "MaxTextureSize", 16384 },
    { "S3TCEnable", 1 },
    { "GroupOffset0", 0 }, { "GroupOffset1", 0 }, { "GroupOffset2", 0 }, { "GroupOffset3", 0 },
    { "GroupOffset4", 0 }, { "GroupOffset5", 0 }, { "GroupOffset6", 0 }, { "GroupOffset7", 0 },
    { "GroupOffset8", 0 }, { "GroupOffset9", 0 },
    { "VSync", 0 }, { "LightMap", 1 },
    { "DrawWorld", 1 }, { "DrawSky", 1 }, { "DrawWorldModels", 1 }, { "DrawAll", 0 },
    { "SkyScale", 1 }, { "LightAddPoly", 1 },
    { "FogEnable", 0 }, { "FogR", 255 }, { "FogG", 255 }, { "FogB", 255 },
    { "FogNearZ", 0 }, { "FogFarZ", 2000 },
    { "SkyFogEnable", 1 }, { "SkyFogNearZ", 0 }, { "SkyFogFarZ", 2000 },
    { "FixTJunc", 0 }, { "DrawSorted", 1 },
    { "Saturate", 0 }, { "DetailTextures", 1 }, { "DetailTextureAdd", 1 }, { "DetailTextureScale", 0.2f },
    { "DrawModels", 1 }, { "DrawGuns", 1 }, { "TextureModels", 1 }, { "ModelSaturation", 2 },
    { "MaxModelLights", 4 }, { "ModelLODOffset", 0 }, { "DrawSprites", 1 },
    { "32BitTextures", 0 },
    { "DrawParticles", 1 }, { "DrawPolyGrids", 1 }, { "DrawLineSystems", 1 }, { "DrawCanvases", 1 },
    { "DynamicLight", 1 }, { "LightSaturate", 1 }, { "FastLight", 0 },
    { "ModelApplySun", 1 }, { "ModelSunVariance", 0.05f },
    { "EnvMapWorld", 1 }, { "EnvMapEnable", 0 }, { "EnvMapAll", 0 }, { "EnvScale", 1 },
    { "EnvPanSpeed", 0.0005f }, { "EnvMapPolyGrids", 1 },
    { "MaxModelShadows", 1 }, { "ModelShadowAlpha", 230 }, { "ModelShadowOffset", 0.05f },
    { "Antialiasing", 0 },
};

// Find a variable, creating it with its default if the engine has none
// (r_MaybeCreateCVar).
static HCONSOLEVAR mr_FindVar(MRVar* pVar)
{
    HCONSOLEVAR h = g.pStruct->GetParameter(pVar->m_pName);
    if (!h)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s %f", pVar->m_pName, pVar->m_fDefault);
        g.pStruct->RunConsoleString(buf);
        h = g.pStruct->GetParameter(pVar->m_pName);
    }
    return h;
}

void mr_RegisterVars(void)
{
    for (int i = 0; i < CV_Count; i++)
        g_Vars[i].m_fValue = g_Vars[i].m_fDefault;
    if (!g.pStruct || !g.pStruct->GetParameter)
        return;
    for (int i = 0; i < CV_Count; i++)
        mr_FindVar(&g_Vars[i]);
}

void mr_ReadConsoleVariables(void)
{
    if (!g.pStruct || !g.pStruct->GetParameter)
        return;
    for (int i = 0; i < CV_Count; i++)
    {
        HCONSOLEVAR h = g.pStruct->GetParameter(g_Vars[i].m_pName);
        g_Vars[i].m_fValue = h ? g.pStruct->GetParameterValueFloat(h) : g_Vars[i].m_fDefault;
    }
}

const char* mr_GetVarString(const char* pName)
{
    HCONSOLEVAR h = g.pStruct->GetParameter(pName);
    return h ? g.pStruct->GetParameterValueString(h) : NULL;
}

// ---- the mode list -----------------------------------------------------------

static void mr_FillMode(RMode* pMode, uint32_t w, uint32_t h, uint32_t bpp)
{
    memset(pMode, 0, sizeof(*pMode));
    pMode->m_bHardware = 1;
    strcpy(pMode->m_RenderDLL, g_RDDll);
    strcpy(pMode->m_InternalName, g_RDCard);
    strcpy(pMode->m_Description, g_RDCard);
    pMode->m_Width = w;
    pMode->m_Height = h;
    pMode->m_BitDepth = bpp;
}

struct MRModeList { RMode* m_pHead; RMode* m_pTail; };

static void mr_AddMode(uint32_t w, uint32_t h, uint32_t bpp, void* pUser)
{
    MRModeList* pList = (MRModeList*)pUser;
    if ((bpp != 16 && bpp != 32) || w < 640 || h < 480)
        return;
    for (RMode* p = pList->m_pHead; p; p = p->m_pNext)
        if (p->m_Width == w && p->m_Height == h && p->m_BitDepth == bpp)
            return;
    RMode* pMode = (RMode*)malloc(sizeof(RMode));
    if (!pMode)
        return;
    mr_FillMode(pMode, w, h, bpp);
    if (pList->m_pTail)
        pList->m_pTail->m_pNext = pMode;
    else
        pList->m_pHead = pMode;
    pList->m_pTail = pMode;
}

// Every display mode the system offers at 16 and 32 bits, 640x480 and up
// -- what d3d.ren's DirectDraw enumeration yields. Called with no device
// up: nothing here touches the device.
extern "C" RMode* GetSupportedModes(void)
{
    MRModeList list = { NULL, NULL };
    plat_EnumModes(mr_AddMode, &list);
    if (!list.m_pHead)
    {
        // No list at all: offer the one mode every setup has.
        list.m_pHead = (RMode*)malloc(sizeof(RMode));
        if (list.m_pHead)
            mr_FillMode(list.m_pHead, 640, 480, 32);
    }
    return list.m_pHead;
}

extern "C" void FreeModeList(RMode* pHead)
{
    while (pHead)
    {
        RMode* pNext = pHead->m_pNext;
        free(pHead);
        pHead = pNext;
    }
}

// ---- the slots ---------------------------------------------------------------

static void r_Term(void);

static int r_Init(RenderStructInit* pInit)
{
    pInit->m_nVersion = RENDERSTRUCT_VERSION;
#ifdef NOLFSRC_DEBUG
    s_nMainThread = GetCurrentThreadId();
#endif

    mr_RegisterVars();
    mr_ReadConsoleVariables();

    g.hWnd = pInit->m_hWnd;
    g.nWidth = pInit->m_Mode.m_Width;
    g.nHeight = pInit->m_Mode.m_Height;
    g.nBitDepth = pInit->m_Mode.m_BitDepth == 16 ? 16 : 32;
    const char* pWindowed = mr_GetVarString("windowed");
    g.bWindowed = pWindowed && atoi(pWindowed) == 1;
    g.bIn3D = g.bInOptimized2D = false;
    g.eBlend = LTSURFACEBLEND_ALPHA;
    g.hOptColor = 0xffffffff;
    g.nContextGen++;

    if (!g.hWnd || !g.nWidth || !g.nHeight)
        return 1;

    plat_OpenWindow(g.hWnd, g.nWidth, g.nHeight, &g.bWindowed);
    if (!rd_CreateDevice(g.hWnd, g.nWidth, g.nHeight, g.bWindowed, cvi(CV_VSync) ? 1 : 0,
                         cvi(CV_Antialiasing)))
    {
        r_Term();
        return 1;
    }
    g.bContext = true;

    // The names GetSupportedModes reports, so the display menu can find the
    // current mode in its list again.
    strcpy(pInit->m_Mode.m_InternalName, g_RDCard);
    strcpy(pInit->m_Mode.m_Description, g_RDCard);
    pInit->m_Mode.m_BitDepth = g.nBitDepth;
    g.pStruct->m_Width = g.nWidth;
    g.pStruct->m_Height = g.nHeight;

    mr_Print("%s: %ux%ux%u %s, %s", g_RDDll, g.nWidth, g.nHeight, g.nBitDepth,
             g.bWindowed ? "windowed" : "full screen", rd_Describe());
    if (g_RDCaps.nSamples > 1 || cvi(CV_Antialiasing) > 1)
        mr_Print("%s: antialiasing %dx (asked for %dx)", g_RDDll, g_RDCaps.nSamples, cvi(CV_Antialiasing));
    return 0;
}

static void r_Term(void)
{
    if (g.pScreenLock)
    {
        free(g.pScreenLock);
        g.pScreenLock = NULL;
    }
    if (g.bContext)
        rd_DestroyDevice();
    g.bContext = false;
    plat_CloseWindow();
    g.bIn3D = g.bInOptimized2D = false;
}

// The engine always passes a rect, clamped to the screen.
static void r_Clear(LTRect* pRect, uint32_t nFlags)
{
    if (!g.bContext)
        return;
    uint32_t bits = 0;
    if (nFlags & CLEARSCREEN_SCREEN)
        bits |= RD_CLEAR_COLOR;
    if (nFlags & CLEARSCREEN_RENDER)
        bits |= RD_CLEAR_DEPTH;
    if (!bits)
        return;
    bool bFull = !pRect || (pRect->left <= 0 && pRect->top <= 0 &&
                            pRect->right >= (int)g.nWidth && pRect->bottom >= (int)g.nHeight);
    if (!bFull)
    {
        rd_Enable(RD_SCISSOR_TEST);
        rd_Scissor(pRect->left, pRect->top, pRect->right - pRect->left, pRect->bottom - pRect->top);
    }
    rd_ColorMask(true);
    rd_DepthMask(true);
    rd_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    rd_Clear(bits);
    if (!bFull)
        rd_Disable(RD_SCISSOR_TEST);
}

static LTBOOL r_Start3D(void)
{
    g.bIn3D = true;
    return 1;
}

static LTBOOL r_End3D(void)
{
    if (g.bInOptimized2D)
        mr_EndOptimized2D();
    g.bIn3D = false;
    return 1;
}

static LTBOOL r_IsIn3D(void) { return g.bIn3D; }
static LTBOOL r_IsInOptimized2D(void) { return g.bInOptimized2D; }

static LTBOOL r_SetOptimized2DBlend(LTSurfaceBlend eBlend)
{
    g.eBlend = eBlend;
    if ((uint32_t)eBlend > LTSURFACEBLEND_MASKADD)
        return 0;
    if (g.bInOptimized2D)
        mr_ApplyBlend(eBlend);
    return 1;
}

static LTBOOL r_GetOptimized2DBlend(LTSurfaceBlend* pBlend)
{
    if (pBlend)
        *pBlend = g.eBlend;
    return 1;
}

static LTBOOL r_SetOptimized2DColor(HLTCOLOR hColor)
{
    g.hOptColor = (hColor & 0x00ffffff) | 0xff000000;
    return 1;
}

static LTBOOL r_GetOptimized2DColor(HLTCOLOR* pColor)
{
    if (pColor)
        *pColor = g.hOptColor;
    return 1;
}

static void r_RenderCommand(int argc, char** argv)
{
    if (argc < 1 || !g.bContext)
        return;
    if (!_stricmp(argv[0], "info") || !_stricmp(argv[0], "gl_strings"))
    {
        rd_PrintInfo(mr_Print);
        mr_Print("max texture %d, %d texture units, S3TC %s, anisotropy %s (%.0f), %dx antialiasing",
                 g_RDCaps.nMaxTextureSize, g_RDCaps.nTextureUnits, g_RDCaps.bS3TC ? "yes" : "no",
                 g_RDCaps.bAniso ? "yes" : "no", g_RDCaps.fMaxAniso, g_RDCaps.nSamples);
    }
}

// Named objects for the engine. The screen-mode movie players want
// DirectDraw ("LPDIRECTDRAW", "BACKBUFFER"): they get mr_movie.cpp's
// stand-ins. The modern engine asks for "FillScreenRect" (MODERN.md
// M-P0-03).
static constexpr char mrn_FillScreenRect[] = "FillScreenRect";
static void* r_GetHook(const char* pName)
{
    if (pName && !_stricmp(pName, "FillScreenRect"))
        return (void*)&MRLocked<mrn_FillScreenRect, mr_FillScreenRect>::Call;
    return mr_MovieHook(pName);
}

static void r_SwapBuffers(uint32_t nFlags)
{
    (void)nFlags;
    if (g.bContext)
        rd_Present();
}

static uint32_t r_GetInfoFlags(void) { return 0; }

// X8R8G8B8 at 32 bits and 565 at 16: the format every engine surface and
// the screen lock are in. The masks, bit counts and shifts are what the
// engine's converters read, so all three are filled.
static void r_GetScreenFormat(PFormat* pFormat)
{
    if (g.nBitDepth == 16)
    {
        static const uint32_t m[4] = { 0, 0xf800, 0x07e0, 0x001f };
        static const uint32_t b[4] = { 0, 5, 6, 5 }, s[4] = { 32, 11, 5, 0 };
        pFormat->m_nType = BPP_16;
        memcpy(pFormat->m_Masks, m, sizeof(m));
        memcpy(pFormat->m_nBits, b, sizeof(b));
        memcpy(pFormat->m_nShifts, s, sizeof(s));
    }
    else
    {
        static const uint32_t m[4] = { 0, 0x00ff0000, 0x0000ff00, 0x000000ff };
        static const uint32_t b[4] = { 0, 8, 8, 8 }, s[4] = { 32, 16, 8, 0 };
        pFormat->m_nType = BPP_32;
        memcpy(pFormat->m_Masks, m, sizeof(m));
        memcpy(pFormat->m_nBits, b, sizeof(b));
        memcpy(pFormat->m_nShifts, s, sizeof(s));
    }
}

// F8: the front buffer as a 24-bit .bmp, whose rows are bottom-up.
static void r_MakeScreenShot(const char* pFilename)
{
    if (!g.bContext || !pFilename)
        return;
    uint32_t nPitch = (g.nWidth * 3 + 3) & ~3u;
    uint8_t* pBits = (uint8_t*)calloc(nPitch, g.nHeight);
    uint32_t* pRead = (uint32_t*)malloc(g.nWidth * g.nHeight * 4);
    if (!pBits || !pRead)
    {
        free(pBits);
        free(pRead);
        return;
    }
    rd_ReadPixels(0, 0, g.nWidth, g.nHeight, RD_PIX_BGRA, pRead, true);
    for (uint32_t y = 0; y < g.nHeight; y++)
    {
        const uint32_t* pSrc = pRead + (g.nHeight - 1 - y) * g.nWidth;
        uint8_t* pDst = pBits + y * nPitch;
        for (uint32_t x = 0; x < g.nWidth; x++, pDst += 3)
        {
            pDst[0] = (uint8_t)pSrc[x];
            pDst[1] = (uint8_t)(pSrc[x] >> 8);
            pDst[2] = (uint8_t)(pSrc[x] >> 16);
        }
    }
    free(pRead);

    FILE* f = fopen(pFilename, "wb");
    if (f)
    {
        BITMAPFILEHEADER fh;
        BITMAPINFOHEADER ih;
        memset(&fh, 0, sizeof(fh));
        memset(&ih, 0, sizeof(ih));
        fh.bfType = 0x4d42;
        fh.bfOffBits = sizeof(fh) + sizeof(ih);
        fh.bfSize = fh.bfOffBits + nPitch * g.nHeight;
        ih.biSize = sizeof(ih);
        ih.biWidth = g.nWidth;
        ih.biHeight = g.nHeight;
        ih.biPlanes = 1;
        ih.biBitCount = 24;
        ih.biCompression = BI_RGB;
        ih.biSizeImage = nPitch * g.nHeight;
        fwrite(&fh, sizeof(fh), 1, f);
        fwrite(&ih, sizeof(ih), 1, f);
        fwrite(pBits, nPitch, g.nHeight, f);
        fclose(f);
        mr_Print("Wrote screenshot %s", pFilename);
    }
    free(pBits);
}

static void r_ReadConsoleVariables(void)
{
    mr_ReadConsoleVariables();
}

// ---- the export --------------------------------------------------------------

// Every slot, and the function behind it: each is called with the lock held.
#define MR_SLOTS(X) \
    X(Init, r_Init) \
    X(Term, r_Term) \
    X(BindTexture, mr_BindTexture) \
    X(UnbindTexture, mr_UnbindTexture) \
    X(CreateContext, mr_CreateContext) \
    X(DeleteContext, mr_DeleteContext) \
    X(Clear, r_Clear) \
    X(Start3D, r_Start3D) \
    X(End3D, r_End3D) \
    X(IsIn3D, r_IsIn3D) \
    X(StartOptimized2D, mr_StartOptimized2D) \
    X(EndOptimized2D, mr_EndOptimized2D) \
    X(IsInOptimized2D, r_IsInOptimized2D) \
    X(SetOptimized2DBlend, r_SetOptimized2DBlend) \
    X(GetOptimized2DBlend, r_GetOptimized2DBlend) \
    X(SetOptimized2DColor, r_SetOptimized2DColor) \
    X(GetOptimized2DColor, r_GetOptimized2DColor) \
    X(RenderScene, mr_RenderScene) \
    X(RenderCommand, r_RenderCommand) \
    X(GetHook, r_GetHook) \
    X(SwapBuffers, r_SwapBuffers) \
    X(GetInfoFlags, r_GetInfoFlags) \
    X(GetScreenFormat, r_GetScreenFormat) \
    X(CreateSurface, mr_CreateSurface) \
    X(DeleteSurface, mr_DeleteSurface) \
    X(GetSurfaceInfo, mr_GetSurfaceInfo) \
    X(LockSurface, mr_LockSurface) \
    X(UnlockSurface, mr_UnlockSurface) \
    X(OptimizeSurface, mr_OptimizeSurface) \
    X(UnoptimizeSurface, mr_UnoptimizeSurface) \
    X(LockScreen, mr_LockScreen) \
    X(UnlockScreen, mr_UnlockScreen) \
    X(BlitToScreen, mr_BlitToScreen) \
    X(WarpToScreen, mr_WarpToScreen) \
    X(MakeScreenShot, r_MakeScreenShot) \
    X(ReadConsoleVariables, r_ReadConsoleVariables) \
    X(BlitFromScreen, mr_BlitFromScreen)
#define MR_SLOT_NAME(slot, fn) static constexpr char mrn_##slot[] = #slot;
MR_SLOTS(MR_SLOT_NAME)
#undef MR_SLOT_NAME

extern "C" void RenderDLLSetup(RenderStruct* pStruct)
{
    mr_InitLock();
    g.pStruct = pStruct;

#define MR_SLOT_SET(slot, fn) pStruct->slot = MRLocked<mrn_##slot, fn>::Call;
    MR_SLOTS(MR_SLOT_SET)
#undef MR_SLOT_SET
}
