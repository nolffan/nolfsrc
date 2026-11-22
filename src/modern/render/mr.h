// mr.h -- the renderer core's own declarations, shared by its files.
//
// The core draws through rdev.h, a fixed-function device in the shape of
// OpenGL 1.x (what the Mac release's renderer it follows uses); gl/ and
// d3d9/ implement it. See README.md.

#ifndef MR_H
#define MR_H

#include "mrplatform.h"
#include "rdev.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ltrender.h"

// ---- the device ------------------------------------------------------------

struct MRGlobals {
    RenderStruct* pStruct;          // the engine's table (RenderDLLSetup)
    void*    hWnd;                  // the engine's window
    bool     bContext;              // the device is up (Init succeeded)
    uint32_t nWidth, nHeight;       // the mode
    uint32_t nBitDepth;             // 16 or 32: the format engine surfaces are in
    bool     bWindowed;

    bool     bIn3D;
    bool     bInOptimized2D;
    LTSurfaceBlend eBlend;          // Optimized-2D blend, as last set
    HLTCOLOR hOptColor;             // Optimized-2D colour, 0xffRRGGBB

    uint32_t nContextGen;           // bumped by every Init: textures made before are gone

    uint8_t* pScreenLock;           // LockScreen's buffer, NULL when not locked
};
extern MRGlobals g;

// Console output through the engine; RenderDebug-gated chatter.
void mr_Print(const char* pFormat, ...);
void mr_Debug(int nLevel, const char* pFormat, ...);

// ---- one thread at a time --------------------------------------------------
//
// The engine calls the renderer from more than one thread: the game's
// loading screen draws whole frames from a thread of its own
// (LoadingScreen.cpp, CLoadingScreen::RunThread) while the main thread
// loads the level, and the main thread binds, frees and makes textures and
// the world's context in that time. The core's state and the device are
// not safe for that, so every way in (the RenderStruct slots, the
// FillScreenRect hook, the movie stand-ins' Blt) holds one recursive lock
// for its whole call (mr_main.cpp, MODERN.md M-P0-06).
struct MRLock {
    explicit MRLock(const char* pWhat);
    ~MRLock();
    MRLock(const MRLock&) = delete;
    MRLock& operator=(const MRLock&) = delete;
};
// The calling thread holds the lock (for the debug checks).
bool mr_LockHeld(void);

// ---- debug checks (NOLFSRC_DEBUG) ------------------------------------------
//
// In the debug build, MR_CHECK reports a condition that the code then
// handles (a bad rectangle from the caller, refused or clipped): once per
// site, to the console, stderr and the backend's log, whatever RenderDebug
// is. MR_ASSERT is for what must never reach the device (a lock or a draw
// out of bounds): it reports the same way, then stops the game with an
// illegal instruction, so the crash handler logs a stack in the renderer
// instead of in the driver. Both are nothing in the release build.
#ifdef NOLFSRC_DEBUG
void mr_CheckFailed(const char* pFile, int nLine, bool bFatal, const char* pFormat, ...);
#define MR_CHECK(c, ...)  ((c) ? (void)0 : mr_CheckFailed(__FILE__, __LINE__, false, __VA_ARGS__))
#define MR_ASSERT(c, ...) ((c) ? (void)0 : mr_CheckFailed(__FILE__, __LINE__, true, __VA_ARGS__))
#else
#define MR_CHECK(c, ...)  ((void)0)
#define MR_ASSERT(c, ...) ((void)0)
#endif

// mr_movie.cpp: DirectDraw stand-ins for the movie players (GetHook).
void* mr_MovieHook(const char* pName);

// ---- console variables -----------------------------------------------------

struct MRVar {
    const char* m_pName;
    float       m_fDefault;
    float       m_fValue;
};
// The renderer's variables, registered with the engine (created if missing)
// at Init and re-read by ReadConsoleVariables.
enum {
    CV_RenderDebug, CV_Bilinear, CV_Trilinear, CV_Anisotropic, CV_MipmapOffset,
    CV_FilterOptimized, CV_MaxTextureSize, CV_S3TCEnable,
    CV_GroupOffset0, CV_GroupOffset9 = CV_GroupOffset0 + 9,
    CV_VSync, CV_LightMap,
    CV_DrawWorld, CV_DrawSky, CV_DrawWorldModels, CV_DrawAll, CV_SkyScale, CV_LightAddPoly,
    CV_FogEnable, CV_FogR, CV_FogG, CV_FogB, CV_FogNearZ, CV_FogFarZ,
    CV_SkyFogEnable, CV_SkyFogNearZ, CV_SkyFogFarZ, CV_FixTJunc, CV_DrawSorted,
    CV_Saturate, CV_DetailTextures, CV_DetailTextureAdd, CV_DetailTextureScale,
    CV_DrawModels, CV_DrawGuns, CV_TextureModels, CV_ModelSaturation, CV_MaxModelLights,
    CV_ModelLODOffset, CV_DrawSprites, CV_32BitTextures,
    CV_DrawParticles, CV_DrawPolyGrids, CV_DrawLineSystems, CV_DrawCanvases, CV_DynamicLight,
    CV_LightSaturate, CV_FastLight, CV_ModelApplySun, CV_ModelSunVariance,
    CV_EnvMapWorld, CV_EnvMapEnable, CV_EnvMapAll, CV_EnvScale, CV_EnvPanSpeed, CV_EnvMapPolyGrids,
    CV_MaxModelShadows, CV_ModelShadowAlpha, CV_ModelShadowOffset, CV_Antialiasing,
    CV_Count
};
extern MRVar g_Vars[CV_Count];
inline float cvf(int i) { return g_Vars[i].m_fValue; }
inline int   cvi(int i) { return (int)g_Vars[i].m_fValue; }
void mr_RegisterVars(void);
void mr_ReadConsoleVariables(void);
const char* mr_GetVarString(const char* pName);

// ---- pixels in the screen format ---------------------------------------------

inline uint32_t mr_BytesPerPixel(void) { return g.nBitDepth == 16 ? 2 : 4; }
// One screen-format pixel to 0xAARRGGBB with alpha 0xff.
inline uint32_t mr_PixelToARGB(uint32_t p)
{
    if (g.nBitDepth == 16)
    {
        uint32_t r = (p >> 11) & 0x1f, gg = (p >> 5) & 0x3f, b = p & 0x1f;
        return 0xff000000 | ((r << 3 | r >> 2) << 16) | ((gg << 2 | gg >> 4) << 8) | (b << 3 | b >> 2);
    }
    return 0xff000000 | (p & 0x00ffffff);
}

// ---- 2D (mr_2d.cpp) ------------------------------------------------------------

void  mr_Begin2D(void);            // ortho over the screen, depth/fog off
void  mr_End2D(void);
void  mr_ApplyBlend(LTSurfaceBlend eBlend);
void* mr_CreateSurface(uint32_t nWidth, uint32_t nHeight);
void  mr_DeleteSurface(void* pSurface);
void  mr_GetSurfaceInfo(void* pSurface, uint32_t* pWidth, uint32_t* pHeight, uint32_t* pPitch);
void* mr_LockSurface(void* pSurface);
void  mr_UnlockSurface(void* pSurface);
LTBOOL mr_OptimizeSurface(void* pSurface, HLTCOLOR hTransColor);
void  mr_UnoptimizeSurface(void* pSurface);
LTBOOL mr_LockScreen(int left, int top, int right, int bottom, void** ppPixels, uint32_t* pPitch);
void  mr_UnlockScreen(void);
void  mr_BlitToScreen(RenderBlitRequest* pRequest);
LTBOOL mr_WarpToScreen(RenderBlitRequest* pRequest);
void  mr_BlitFromScreen(RenderBlitRequest* pRequest);
LTBOOL mr_StartOptimized2D(void);
void  mr_EndOptimized2D(void);
LTBOOL mr_FillScreenRect(LTRect* pRect, uint32_t nPixel);

// ---- textures (mr_texture.cpp) -------------------------------------------------

struct MRTexture {
    uint32_t m_Id;
    uint32_t m_nWidth, m_nHeight;   // of the first level uploaded
    uint32_t m_nBaseWidth, m_nBaseHeight;   // of the full-size image (TextureData mip 0)
    uint32_t m_DtxFlags;            // DTX flags (DTX_FULLBRITE, ...)
    uint32_t m_nBytes;
    bool     m_bAlpha;              // the data carries an alpha channel worth testing
    // World texture coordinates are in texels of the full-size image, scaled
    // by the DTX's coordinate shift (r_SetupTextureMip): u * m_fUMul.
    float    m_fUMul, m_fVMul;
    // How this texture's detail texture lies on it (RTexture+0x34..+0x3c).
    float    m_fDetailScale, m_fDetailCos, m_fDetailSin;
};
void mr_BindTexture(SharedTexture* pTexture, LTBOOL bChanged);
void mr_UnbindTexture(SharedTexture* pTexture);
// The device texture of a SharedTexture, binding it on first use; 0 if it has none.
uint32_t mr_TextureId(SharedTexture* pTexture);

// ---- the scene (mr_scene.cpp) ---------------------------------------------------

void*  mr_CreateContext(RenderContextInit* pInit);
void   mr_DeleteContext(void* pContext);
LTBOOL mr_RenderScene(SceneDesc* pScene);

#endif
