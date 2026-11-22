// d3d9_device.cpp -- rdev.h on Direct3D 9: d3d9.ren's backend.
//
// rdev.h is a small OpenGL 1.x, and this is that much of OpenGL on the
// Direct3D 9 fixed-function pipeline, the way GL-on-D3D wrappers do it: the
// GL state is kept here (with glPushAttrib's groups), handed to the device
// when something is drawn; the matrix stacks are kept here, the projection
// moved to Direct3D's depth range and pixel centres; immediate-mode
// vertices are gathered and drawn with DrawPrimitiveUP. Texture units map
// onto the stages in order, skipping disabled units as GL does (a disabled
// stage ends Direct3D's cascade). Textures are managed-pool textures, made
// at their first use from the levels uploaded since, so a texture gets
// exactly the levels GL would sample.
//
// Antialiasing draws into a multisampled render target and depth surface,
// resolved into the back buffer (StretchRect) before a present or a read:
// the back buffer itself stays single-sampled, so the COPY swap effect
// (which multisampling would rule out) still keeps the last frame.
//
// Under Proton the device is DXVK on Vulkan; under plain Wine, wined3d.

#include "../mr.h"
#include <d3d9.h>
#include <math.h>

const char g_RDDll[] = "d3d9.ren";
const char g_RDLog[] = "d3d9.log";
const char g_RDCard[] = "Direct3D9";
RDCaps g_RDCaps;

#define D9_UNITS 4

static IDirect3D9*           s_pD3D;
static IDirect3DDevice9*     s_pDev;
static D3DPRESENT_PARAMETERS s_pp;
static IDirect3DSurface9*    s_pReadback;      // system-memory copy of the back buffer
static IDirect3DSurface9*    s_pMSColor;       // the multisampled target, NULL without antialiasing
static IDirect3DSurface9*    s_pMSDepth;
static D3DMULTISAMPLE_TYPE   s_eMS;
static bool                  s_bInScene;
static int                   s_nWidth, s_nHeight;
static char                  s_szDesc[256];

// ---- the device's own state, set only when it changes -----------------------------------

static DWORD s_RS[256];
static bool  s_bRS[256];
static DWORD s_TSS[8][33];
static bool  s_bTSS[8][33];
static DWORD s_Samp[8][14];
static bool  s_bSamp[8][14];
static IDirect3DBaseTexture9* s_pStageTex[8];

static void d9_ForgetDeviceState(void)
{
    memset(s_bRS, 0, sizeof(s_bRS));
    memset(s_bTSS, 0, sizeof(s_bTSS));
    memset(s_bSamp, 0, sizeof(s_bSamp));
    memset(s_pStageTex, 0xff, sizeof(s_pStageTex));
}

static void d9_RS(D3DRENDERSTATETYPE t, DWORD v)
{
    if (s_bRS[t] && s_RS[t] == v)
        return;
    s_RS[t] = v;
    s_bRS[t] = true;
    s_pDev->SetRenderState(t, v);
}

static void d9_TSS(int nStage, D3DTEXTURESTAGESTATETYPE t, DWORD v)
{
    if (s_bTSS[nStage][t] && s_TSS[nStage][t] == v)
        return;
    s_TSS[nStage][t] = v;
    s_bTSS[nStage][t] = true;
    s_pDev->SetTextureStageState(nStage, t, v);
}

static void d9_Samp(int nStage, D3DSAMPLERSTATETYPE t, DWORD v)
{
    if (s_bSamp[nStage][t] && s_Samp[nStage][t] == v)
        return;
    s_Samp[nStage][t] = v;
    s_bSamp[nStage][t] = true;
    s_pDev->SetSamplerState(nStage, t, v);
}

static void d9_SetTexture(int nStage, IDirect3DBaseTexture9* pTex)
{
    if (s_pStageTex[nStage] == pTex)
        return;
    s_pStageTex[nStage] = pTex;
    s_pDev->SetTexture(nStage, pTex);
}

static inline DWORD d9_F2DW(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

static void d9_BeginScene(void)
{
    if (!s_bInScene && s_pDev)
    {
        s_pDev->BeginScene();
        s_bInScene = true;
    }
}

static void d9_EndScene(void)
{
    if (s_bInScene)
    {
        s_pDev->EndScene();
        s_bInScene = false;
    }
}

// ---- textures --------------------------------------------------------------------------

#define D9_MAXLEVELS 16

struct D9Texture {
    bool               m_bUsed;
    IDirect3DTexture9* m_pTex;
    D3DFORMAT          m_Fmt;
    bool               m_bAlpha;            // the format has alpha (else the texture's alpha is 1)
    int                m_nLevels;           // in m_pTex
    // Levels uploaded since level 0 was, waiting for the texture to be made,
    // in m_Fmt's layout.
    uint8_t*           m_pStage[D9_MAXLEVELS];
    int                m_StageW[D9_MAXLEVELS], m_StageH[D9_MAXLEVELS];
    // The texture's parameters, GL's defaults to start with.
    RDFilter           m_Min, m_Mag;
    bool               m_bRepeatS, m_bRepeatT;
    int                m_nMaxLevel;
    float              m_fAniso;
};
static D9Texture* s_pTextures;
static uint32_t   s_nTextures;

static D9Texture* d9_Tex(uint32_t n)
{
    return n && n <= s_nTextures && s_pTextures[n - 1].m_bUsed ? &s_pTextures[n - 1] : NULL;
}

static void d9_FreeStage(D9Texture* t)
{
    for (int i = 0; i < D9_MAXLEVELS; i++)
    {
        free(t->m_pStage[i]);
        t->m_pStage[i] = NULL;
    }
}

static void d9_FreeTexture(D9Texture* t)
{
    if (t->m_pTex)
    {
        for (int i = 0; i < 8; i++)
            if (s_pStageTex[i] == t->m_pTex)
                d9_SetTexture(i, NULL);
        t->m_pTex->Release();
        t->m_pTex = NULL;
    }
    d9_FreeStage(t);
}

static void d9_FreeTextures(void)
{
    for (uint32_t i = 0; i < s_nTextures; i++)
        if (s_pTextures[i].m_bUsed)
            d9_FreeTexture(&s_pTextures[i]);
    free(s_pTextures);
    s_pTextures = NULL;
    s_nTextures = 0;
}

static bool d9_IsDXT(D3DFORMAT f) { return f == D3DFMT_DXT1 || f == D3DFMT_DXT3 || f == D3DFMT_DXT5; }

static uint32_t d9_BytesPerPixel(D3DFORMAT f) { return f == D3DFMT_R5G6B5 ? 2 : 4; }

// Bytes in one level: rows of pixels, or of 4x4 blocks.
static uint32_t d9_LevelBytes(D3DFORMAT f, int w, int h, uint32_t* pRowBytes, int* pRows)
{
    if (d9_IsDXT(f))
    {
        *pRowBytes = ((w + 3) / 4) * (f == D3DFMT_DXT1 ? 8 : 16);
        *pRows = (h + 3) / 4;
    }
    else
    {
        *pRowBytes = w * d9_BytesPerPixel(f);
        *pRows = h;
    }
    return *pRowBytes * *pRows;
}

// One row of rdev.h pixels into the texture's format.
static void d9_ConvertRow(D3DFORMAT f, RDPixels ePixels, const void* pSrc, void* pDst, int w)
{
    if (f == D3DFMT_R5G6B5)
    {
        uint16_t* d = (uint16_t*)pDst;
        if (ePixels == RD_PIX_565)
        {
            memcpy(d, pSrc, w * 2);
            return;
        }
        for (int x = 0; x < w; x++)
        {
            uint32_t r, g, b;
            const uint8_t* s = (const uint8_t*)pSrc;
            if (ePixels == RD_PIX_BGRA)      { b = s[x * 4]; g = s[x * 4 + 1]; r = s[x * 4 + 2]; }
            else if (ePixels == RD_PIX_RGBA) { r = s[x * 4]; g = s[x * 4 + 1]; b = s[x * 4 + 2]; }
            else                             { r = s[x * 3]; g = s[x * 3 + 1]; b = s[x * 3 + 2]; }
            d[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        }
        return;
    }
    uint32_t* d = (uint32_t*)pDst;
    switch (ePixels)
    {
    case RD_PIX_BGRA:
        memcpy(d, pSrc, w * 4);
        break;
    case RD_PIX_RGBA:
        for (int x = 0; x < w; x++)
        {
            const uint8_t* s = (const uint8_t*)pSrc + x * 4;
            d[x] = ((uint32_t)s[3] << 24) | (s[0] << 16) | (s[1] << 8) | s[2];
        }
        break;
    case RD_PIX_RGB:
        for (int x = 0; x < w; x++)
        {
            const uint8_t* s = (const uint8_t*)pSrc + x * 3;
            d[x] = 0xff000000 | (s[0] << 16) | (s[1] << 8) | s[2];
        }
        break;
    case RD_PIX_565:
        for (int x = 0; x < w; x++)
        {
            uint32_t p = ((const uint16_t*)pSrc)[x];
            uint32_t r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, b = p & 0x1f;
            d[x] = 0xff000000 | ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
        }
        break;
    }
}

static uint32_t d9_PixelBytes(RDPixels e) { return e == RD_PIX_565 ? 2 : e == RD_PIX_RGB ? 3 : 4; }

// Make the texture from its staged levels, if it has new ones.
static IDirect3DTexture9* d9_Realise(D9Texture* t)
{
    if (!t->m_pStage[0])
        return t->m_pTex;
    // Direct3D sizes every level below the first itself, each half the one
    // above (at least 1), and LockRect hands back a buffer that size. GL takes
    // whatever sizes it is given (a chain that does not halve just leaves the
    // texture incomplete), so a staged level of any other size ends the chain
    // here: copied into Direct3D's smaller level it would write past the end
    // of its buffer (heap corruption, a crash later at a level change).
    int nLevels = 0;
    while (nLevels < D9_MAXLEVELS && nLevels <= t->m_nMaxLevel && t->m_pStage[nLevels])
    {
        int nW = t->m_StageW[0] >> nLevels, nH = t->m_StageH[0] >> nLevels;
        if (nW < 1) nW = 1;
        if (nH < 1) nH = 1;
        if (nLevels > 0 && (t->m_StageW[nLevels] != nW || t->m_StageH[nLevels] != nH))
        {
            mr_Debug(1, "texture %dx%d: level %d is %dx%d, not %dx%d; levels from it dropped",
                     t->m_StageW[0], t->m_StageH[0], nLevels, t->m_StageW[nLevels],
                     t->m_StageH[nLevels], nW, nH);
            break;
        }
        if (nW == 1 && nH == 1)
        {
            nLevels++;
            break;                      // the last level a chain can have
        }
        nLevels++;
    }
    if (t->m_pTex)
    {
        t->m_pTex->Release();
        t->m_pTex = NULL;
    }
    if (FAILED(s_pDev->CreateTexture(t->m_StageW[0], t->m_StageH[0], nLevels, 0, t->m_Fmt,
                                     D3DPOOL_MANAGED, &t->m_pTex, NULL)))
    {
        mr_Debug(1, "CreateTexture %dx%d format %d failed", t->m_StageW[0], t->m_StageH[0], t->m_Fmt);
        d9_FreeStage(t);
        return NULL;
    }
    for (int i = 0; i < nLevels; i++)
    {
        // Copy no more than the level Direct3D made holds, whatever was staged.
        D3DSURFACE_DESC desc;
        if (FAILED(t->m_pTex->GetLevelDesc(i, &desc)))
            continue;
        uint32_t nRowBytes, nDevRowBytes;
        int nRows, nDevRows;
        d9_LevelBytes(t->m_Fmt, t->m_StageW[i], t->m_StageH[i], &nRowBytes, &nRows);
        d9_LevelBytes(t->m_Fmt, (int)desc.Width, (int)desc.Height, &nDevRowBytes, &nDevRows);
        uint32_t nCopy = nRowBytes < nDevRowBytes ? nRowBytes : nDevRowBytes;
        int nCopyRows = nRows < nDevRows ? nRows : nDevRows;
        MR_ASSERT(nCopy == nDevRowBytes && nCopyRows == nDevRows,
                  "texture %dx%d level %d: staged %dx%d, Direct3D's %ux%u", t->m_StageW[0], t->m_StageH[0],
                  i, t->m_StageW[i], t->m_StageH[i], desc.Width, desc.Height);
        D3DLOCKED_RECT lr;
        if (FAILED(t->m_pTex->LockRect(i, &lr, NULL, 0)))
            continue;
        if ((uint32_t)lr.Pitch < nCopy)
            nCopy = (uint32_t)lr.Pitch;
        for (int y = 0; y < nCopyRows; y++)
            memcpy((uint8_t*)lr.pBits + y * lr.Pitch, t->m_pStage[i] + y * nRowBytes, nCopy);
        t->m_pTex->UnlockRect(i);
    }
    t->m_nLevels = nLevels;
    d9_FreeStage(t);
    return t->m_pTex;
}

// ---- the GL state -----------------------------------------------------------------------

struct D9Unit {
    bool      m_bEnable;
    uint32_t  m_nTex;
    int       m_eEnv;                   // RD_ENV_MODULATE, RD_ENV_REPLACE, or 2: m_Combine
    RDCombine m_Combine;
};

struct D9State {
    bool        m_Caps[RD_CAP_COUNT];   // RD_TEXTURE_2D is per unit, in m_Units
    RDBlend     m_eSrc, m_eDst;
    float       m_fAlphaRef;
    RDDepthFunc m_eDepthFunc;
    bool        m_bDepthMask, m_bColorMask;
    float       m_Fog[3], m_fFogStart, m_fFogEnd;
    int         m_Viewport[4], m_Scissor[4];
    float       m_DepthRange[2];
    float       m_NearDepth[4];         // rd_NearDepth: clip, near, depth0, depth1
    float       m_Clear[4];
    int         m_nActive;
    D9Unit      m_Units[D9_UNITS];
};
static D9State s_S;

struct D9Push { uint32_t m_nMask; D9State m_S; };
static D9Push s_Stack[16];
static int    s_nStack;

static void d9_DefaultState(void)
{
    memset(&s_S, 0, sizeof(s_S));
    s_S.m_Caps[RD_DEPTH_TEST] = true;
    s_S.m_eSrc = RD_ONE;
    s_S.m_eDst = RD_ZERO;
    s_S.m_eDepthFunc = RD_LEQUAL;
    s_S.m_bDepthMask = s_S.m_bColorMask = true;
    s_S.m_fFogEnd = 1.0f;
    s_S.m_Viewport[2] = s_S.m_Scissor[2] = s_nWidth;
    s_S.m_Viewport[3] = s_S.m_Scissor[3] = s_nHeight;
    s_S.m_DepthRange[1] = 1.0f;
    s_S.m_Clear[3] = 1.0f;
    s_nStack = 0;
}

void rd_Enable(RDCap eCap)
{
    if (eCap == RD_TEXTURE_2D)
        s_S.m_Units[s_S.m_nActive].m_bEnable = true;
    else
        s_S.m_Caps[eCap] = true;
}

void rd_CullBack(bool bOn)
{
    if (s_pDev)
        d9_RS(D3DRS_CULLMODE, bOn ? D3DCULL_CCW : D3DCULL_NONE);
}

void rd_Disable(RDCap eCap)
{
    if (eCap == RD_TEXTURE_2D)
        s_S.m_Units[s_S.m_nActive].m_bEnable = false;
    else
        s_S.m_Caps[eCap] = false;
}

void rd_BlendFunc(RDBlend eSrc, RDBlend eDst) { s_S.m_eSrc = eSrc; s_S.m_eDst = eDst; }
void rd_AlphaGreater(float fRef)              { s_S.m_fAlphaRef = fRef; }
void rd_DepthFunc(RDDepthFunc eFunc)          { s_S.m_eDepthFunc = eFunc; }
void rd_DepthMask(bool bWrite)                { s_S.m_bDepthMask = bWrite; }
void rd_ColorMask(bool bWrite)                { s_S.m_bColorMask = bWrite; }

void rd_FogColor(float r, float g, float b)
{
    s_S.m_Fog[0] = r;
    s_S.m_Fog[1] = g;
    s_S.m_Fog[2] = b;
}

void rd_FogRange(float fStart, float fEnd)
{
    s_S.m_fFogStart = fStart;
    s_S.m_fFogEnd = fEnd;
}

void rd_Viewport(int x, int y, int w, int h)
{
    s_S.m_Viewport[0] = x; s_S.m_Viewport[1] = y; s_S.m_Viewport[2] = w; s_S.m_Viewport[3] = h;
}

void rd_DepthRange(float fNear, float fFar)
{
    s_S.m_DepthRange[0] = fNear; s_S.m_DepthRange[1] = fFar;
}

void rd_NearDepth(float fClip, float fNear, float fDepth0, float fDepth1)
{
    s_S.m_NearDepth[0] = fClip; s_S.m_NearDepth[1] = fNear;
    s_S.m_NearDepth[2] = fDepth0; s_S.m_NearDepth[3] = fDepth1;
}

void rd_Scissor(int x, int y, int w, int h)
{
    s_S.m_Scissor[0] = x; s_S.m_Scissor[1] = y; s_S.m_Scissor[2] = w; s_S.m_Scissor[3] = h;
}

void rd_PushAttrib(uint32_t nMask)
{
    if (s_nStack >= (int)(sizeof(s_Stack) / sizeof(s_Stack[0])))
    {
        mr_Debug(1, "attribute stack overflow");
        return;
    }
    s_Stack[s_nStack].m_nMask = nMask;
    s_Stack[s_nStack].m_S = s_S;
    s_nStack++;
}

// Restore what the push's groups hold, as glPopAttrib.
void rd_PopAttrib(void)
{
    if (s_nStack <= 0)
        return;
    s_nStack--;
    uint32_t m = s_Stack[s_nStack].m_nMask;
    const D9State& o = s_Stack[s_nStack].m_S;
    if (m & RD_ATTR_COLOR)
    {
        s_S.m_Caps[RD_BLEND] = o.m_Caps[RD_BLEND];
        s_S.m_Caps[RD_ALPHA_TEST] = o.m_Caps[RD_ALPHA_TEST];
        s_S.m_eSrc = o.m_eSrc;
        s_S.m_eDst = o.m_eDst;
        s_S.m_fAlphaRef = o.m_fAlphaRef;
        s_S.m_bColorMask = o.m_bColorMask;
        memcpy(s_S.m_Clear, o.m_Clear, sizeof(s_S.m_Clear));
    }
    if (m & RD_ATTR_DEPTH)
    {
        s_S.m_Caps[RD_DEPTH_TEST] = o.m_Caps[RD_DEPTH_TEST];
        s_S.m_eDepthFunc = o.m_eDepthFunc;
        s_S.m_bDepthMask = o.m_bDepthMask;
    }
    if (m & RD_ATTR_FOG)
    {
        s_S.m_Caps[RD_FOG] = o.m_Caps[RD_FOG];
        memcpy(s_S.m_Fog, o.m_Fog, sizeof(s_S.m_Fog));
        s_S.m_fFogStart = o.m_fFogStart;
        s_S.m_fFogEnd = o.m_fFogEnd;
    }
    if (m & RD_ATTR_ENABLE)
    {
        memcpy(s_S.m_Caps, o.m_Caps, sizeof(s_S.m_Caps));
        for (int i = 0; i < D9_UNITS; i++)
            s_S.m_Units[i].m_bEnable = o.m_Units[i].m_bEnable;
    }
    if (m & RD_ATTR_TEXTURE)
    {
        s_S.m_nActive = o.m_nActive;
        memcpy(s_S.m_Units, o.m_Units, sizeof(s_S.m_Units));
    }
    if (m & RD_ATTR_SCISSOR)
    {
        s_S.m_Caps[RD_SCISSOR_TEST] = o.m_Caps[RD_SCISSOR_TEST];
        memcpy(s_S.m_Scissor, o.m_Scissor, sizeof(s_S.m_Scissor));
    }
    if (m & RD_ATTR_VIEWPORT)
    {
        memcpy(s_S.m_Viewport, o.m_Viewport, sizeof(s_S.m_Viewport));
        memcpy(s_S.m_DepthRange, o.m_DepthRange, sizeof(s_S.m_DepthRange));
    }
}

// ---- texture units ------------------------------------------------------------------------

void rd_ActiveTexture(int nUnit)
{
    if (nUnit >= 0 && nUnit < D9_UNITS)
        s_S.m_nActive = nUnit;
}

void rd_TexEnv(RDTexEnv eEnv) { s_S.m_Units[s_S.m_nActive].m_eEnv = eEnv; }

void rd_TexCombine(const RDCombine& c)
{
    D9Unit& u = s_S.m_Units[s_S.m_nActive];
    u.m_eEnv = 2;
    u.m_Combine = c;
}

// ---- textures ---------------------------------------------------------------------------------

uint32_t rd_GenTexture(void)
{
    uint32_t i = 0;
    while (i < s_nTextures && s_pTextures[i].m_bUsed)
        i++;
    if (i == s_nTextures)
    {
        uint32_t n = s_nTextures ? s_nTextures * 2 : 256;
        D9Texture* p = (D9Texture*)realloc(s_pTextures, n * sizeof(D9Texture));
        if (!p)
            return 0;
        memset(p + s_nTextures, 0, (n - s_nTextures) * sizeof(D9Texture));
        s_pTextures = p;
        s_nTextures = n;
    }
    D9Texture* t = &s_pTextures[i];
    memset(t, 0, sizeof(*t));
    t->m_bUsed = true;
    t->m_Min = RD_NEAREST_MIPMAP_LINEAR;
    t->m_Mag = RD_LINEAR;
    t->m_bRepeatS = t->m_bRepeatT = true;
    t->m_nMaxLevel = 1000;
    t->m_fAniso = 1.0f;
    return i + 1;
}

void rd_DeleteTexture(uint32_t nTex)
{
    D9Texture* t = d9_Tex(nTex);
    if (!t)
        return;
    d9_FreeTexture(t);
    t->m_bUsed = false;
    for (int i = 0; i < D9_UNITS; i++)
        if (s_S.m_Units[i].m_nTex == nTex)
            s_S.m_Units[i].m_nTex = 0;
}

void rd_BindTexture(uint32_t nTex) { s_S.m_Units[s_S.m_nActive].m_nTex = nTex; }

static D9Texture* d9_Bound(void) { return d9_Tex(s_S.m_Units[s_S.m_nActive].m_nTex); }

// Stage a level. Level 0 starts the texture over, as glTexImage2D
// redefining it does.
static uint8_t* d9_StageLevel(D9Texture* t, int nLevel, D3DFORMAT f, int w, int h)
{
    if (nLevel < 0 || nLevel >= D9_MAXLEVELS)
        return NULL;
    if (nLevel == 0)
    {
        d9_FreeTexture(t);
        t->m_Fmt = f;
        t->m_bAlpha = f == D3DFMT_A8R8G8B8 || f == D3DFMT_DXT3 || f == D3DFMT_DXT5;
    }
    else if (f != t->m_Fmt || !t->m_pStage[0])
        return NULL;
    uint32_t nRowBytes;
    int nRows;
    uint8_t* p = (uint8_t*)realloc(t->m_pStage[nLevel], d9_LevelBytes(f, w, h, &nRowBytes, &nRows));
    if (!p)
        return NULL;
    t->m_pStage[nLevel] = p;
    t->m_StageW[nLevel] = w;
    t->m_StageH[nLevel] = h;
    return p;
}

void rd_TexImage(int nLevel, RDFormat eFormat, int w, int h, RDPixels ePixels, const void* pData,
                 int nRowPixels)
{
    D9Texture* t = d9_Bound();
    if (!t)
        return;
    D3DFORMAT f = eFormat == RD_FMT_RGBA8 ? D3DFMT_A8R8G8B8 :
                  eFormat == RD_FMT_RGB && ePixels == RD_PIX_565 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
    uint8_t* p = d9_StageLevel(t, nLevel, f, w, h);
    if (!p)
        return;
    if (!pData)
    {
        memset(p, 0, w * h * d9_BytesPerPixel(f));     // GL leaves it undefined; black is kinder
        return;
    }
    uint32_t nSrcPitch = (nRowPixels ? nRowPixels : w) * d9_PixelBytes(ePixels);
    uint32_t nDstPitch = w * d9_BytesPerPixel(f);
    for (int y = 0; y < h; y++)
        d9_ConvertRow(f, ePixels, (const uint8_t*)pData + y * nSrcPitch, p + y * nDstPitch, w);
}

void rd_TexSubImage(int nLevel, int x, int y, int w, int h, RDPixels ePixels, const void* pData)
{
    D9Texture* t = d9_Bound();
    if (!t || d9_IsDXT(t->m_Fmt) || nLevel < 0 || nLevel >= D9_MAXLEVELS)
        return;
    uint32_t bpp = d9_BytesPerPixel(t->m_Fmt);
    uint32_t nSrcPitch = w * d9_PixelBytes(ePixels);
    if (t->m_pStage[nLevel])
    {
        // GL refuses a rectangle outside the level; never write past it.
        if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
            x + w > t->m_StageW[nLevel] || y + h > t->m_StageH[nLevel])
        {
            MR_CHECK(false, "texture %dx%d level %d: sub-image %d,%d %dx%d outside it; refused",
                     t->m_StageW[nLevel], t->m_StageH[nLevel], nLevel, x, y, w, h);
            mr_Debug(1, "texture %dx%d level %d: sub-image %d,%d %dx%d outside it; refused",
                     t->m_StageW[nLevel], t->m_StageH[nLevel], nLevel, x, y, w, h);
            return;
        }
        uint32_t nDstPitch = t->m_StageW[nLevel] * bpp;
        for (int r = 0; r < h; r++)
            d9_ConvertRow(t->m_Fmt, ePixels, (const uint8_t*)pData + r * nSrcPitch,
                          t->m_pStage[nLevel] + (y + r) * nDstPitch + x * bpp, w);
        return;
    }
    if (!t->m_pTex || nLevel >= t->m_nLevels)
        return;
    // The same refusal as for a staged level: Direct3D's retail runtime does
    // not check a LockRect rectangle against the level, and a row written
    // past the level's end goes into the runtime's copy of the texture.
    D3DSURFACE_DESC desc;
    if (FAILED(t->m_pTex->GetLevelDesc(nLevel, &desc)))
        return;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > (int)desc.Width || y + h > (int)desc.Height)
    {
        MR_CHECK(false, "texture %ux%u level %d: sub-image %d,%d %dx%d outside it; refused",
                 desc.Width, desc.Height, nLevel, x, y, w, h);
        mr_Debug(1, "texture %ux%u level %d: sub-image %d,%d %dx%d outside it; refused",
                 desc.Width, desc.Height, nLevel, x, y, w, h);
        return;
    }
    RECT rc = { x, y, x + w, y + h };
    D3DLOCKED_RECT lr;
    if (FAILED(t->m_pTex->LockRect(nLevel, &lr, &rc, 0)))
        return;
    MR_ASSERT(lr.Pitch >= (INT)(w * bpp), "texture level %d: pitch %d for %d pixels", nLevel, lr.Pitch, w);
    for (int r = 0; r < h; r++)
        d9_ConvertRow(t->m_Fmt, ePixels, (const uint8_t*)pData + r * nSrcPitch, (uint8_t*)lr.pBits + r * lr.Pitch, w);
    t->m_pTex->UnlockRect(nLevel);
}

void rd_CompressedTexImage(int nLevel, RDFormat eFormat, int w, int h, uint32_t nBytes, const void* pData)
{
    D9Texture* t = d9_Bound();
    if (!t)
        return;
    D3DFORMAT f = eFormat == RD_FMT_DXT3 ? D3DFMT_DXT3 : eFormat == RD_FMT_DXT5 ? D3DFMT_DXT5 : D3DFMT_DXT1;
    uint8_t* p = d9_StageLevel(t, nLevel, f, w, h);
    uint32_t nRowBytes;
    int nRows;
    uint32_t n = d9_LevelBytes(f, w, h, &nRowBytes, &nRows);
    if (p && pData)
        memcpy(p, pData, n < nBytes ? n : nBytes);
}

void rd_TexFilter(RDFilter eMin, RDFilter eMag)
{
    if (D9Texture* t = d9_Bound())
    {
        t->m_Min = eMin;
        t->m_Mag = eMag;
    }
}

void rd_TexWrap(bool bRepeatS, bool bRepeatT)
{
    if (D9Texture* t = d9_Bound())
    {
        t->m_bRepeatS = bRepeatS;
        t->m_bRepeatT = bRepeatT;
    }
}

void rd_TexMaxLevel(int nLevel)
{
    if (D9Texture* t = d9_Bound())
        t->m_nMaxLevel = nLevel;
}

void rd_TexAniso(float fAniso)
{
    if (D9Texture* t = d9_Bound())
        t->m_fAniso = fAniso;
}

// ---- matrices ----------------------------------------------------------------------------------

struct D9Matrix { float m[16]; };
static D9Matrix s_MV[32], s_Proj[8], s_TexM[D9_UNITS][8];
static int s_nMV, s_nProj, s_nTexM[D9_UNITS];
static RDMatrixMode s_eMode;

static const D9Matrix s_Identity = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };

static D9Matrix* d9_Cur(void)
{
    switch (s_eMode)
    {
    case RD_PROJECTION: return &s_Proj[s_nProj];
    case RD_TEXTURE:    return &s_TexM[s_S.m_nActive][s_nTexM[s_S.m_nActive]];
    default:            return &s_MV[s_nMV];
    }
}

// a = a * b, column-major
static void d9_Mul(D9Matrix* a, const float* b)
{
    D9Matrix r;
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            r.m[col * 4 + row] = a->m[0 * 4 + row] * b[col * 4 + 0] + a->m[1 * 4 + row] * b[col * 4 + 1] +
                                 a->m[2 * 4 + row] * b[col * 4 + 2] + a->m[3 * 4 + row] * b[col * 4 + 3];
    *a = r;
}

static bool d9_IsIdentity(const D9Matrix* a) { return !memcmp(a, &s_Identity, sizeof(*a)); }

void rd_MatrixMode(RDMatrixMode eMode)  { s_eMode = eMode; }
void rd_LoadIdentity(void)              { *d9_Cur() = s_Identity; }
void rd_LoadMatrix(const float* m)      { memcpy(d9_Cur()->m, m, 64); }
void rd_MultMatrix(const float* m)      { d9_Mul(d9_Cur(), m); }

void rd_PushMatrix(void)
{
    int* pn = s_eMode == RD_PROJECTION ? &s_nProj : s_eMode == RD_TEXTURE ? &s_nTexM[s_S.m_nActive] : &s_nMV;
    int nMax = s_eMode == RD_PROJECTION ? 7 : s_eMode == RD_TEXTURE ? 7 : 31;
    if (*pn >= nMax)
        return;
    D9Matrix m = *d9_Cur();
    (*pn)++;
    *d9_Cur() = m;
}

void rd_PopMatrix(void)
{
    int* pn = s_eMode == RD_PROJECTION ? &s_nProj : s_eMode == RD_TEXTURE ? &s_nTexM[s_S.m_nActive] : &s_nMV;
    if (*pn > 0)
        (*pn)--;
}

void rd_Ortho(double l, double r, double b, double t, double n, double f)
{
    float m[16] = {
        (float)(2.0 / (r - l)), 0, 0, 0,
        0, (float)(2.0 / (t - b)), 0, 0,
        0, 0, (float)(-2.0 / (f - n)), 0,
        (float)(-(r + l) / (r - l)), (float)(-(t + b) / (t - b)), (float)(-(f + n) / (f - n)), 1,
    };
    d9_Mul(d9_Cur(), m);
}

// ---- handing the state to the device ------------------------------------------------------------

static const DWORD s_Blend[] = {
    D3DBLEND_ZERO, D3DBLEND_ONE, D3DBLEND_SRCCOLOR, D3DBLEND_INVSRCCOLOR, D3DBLEND_DESTCOLOR,
    D3DBLEND_INVDESTCOLOR, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA, D3DBLEND_DESTALPHA,
    D3DBLEND_INVDESTALPHA, D3DBLEND_SRCALPHASAT
};

static DWORD d9_Arg(RDCombineArg e)
{
    return e == RD_ARG_TEXTURE ? D3DTA_TEXTURE : e == RD_ARG_PRIMARY ? D3DTA_DIFFUSE : D3DTA_CURRENT;
}

static DWORD d9_Op(RDCombineOp e, float fScale)
{
    switch (e)
    {
    case RD_OP_REPLACE:    return D3DTOP_SELECTARG1;
    case RD_OP_ADD:        return D3DTOP_ADD;
    case RD_OP_ADD_SIGNED: return fScale >= 2.0f ? D3DTOP_ADDSIGNED2X : D3DTOP_ADDSIGNED;
    default:               return fScale >= 4.0f ? D3DTOP_MODULATE4X : fScale >= 2.0f ? D3DTOP_MODULATE2X : D3DTOP_MODULATE;
    }
}

static void d9_Sampler(int nStage, const D9Texture* t)
{
    bool bMip = t->m_Min >= RD_NEAREST_MIPMAP_NEAREST;
    bool bMinLinear = t->m_Min == RD_LINEAR || t->m_Min == RD_LINEAR_MIPMAP_NEAREST || t->m_Min == RD_LINEAR_MIPMAP_LINEAR;
    DWORD nMip = !bMip ? D3DTEXF_NONE :
                 (t->m_Min == RD_NEAREST_MIPMAP_LINEAR || t->m_Min == RD_LINEAR_MIPMAP_LINEAR) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    bool bAniso = t->m_fAniso > 1.0f && bMinLinear;
    d9_Samp(nStage, D3DSAMP_MINFILTER, bAniso ? D3DTEXF_ANISOTROPIC : bMinLinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    d9_Samp(nStage, D3DSAMP_MAGFILTER, t->m_Mag == RD_NEAREST ? D3DTEXF_POINT : D3DTEXF_LINEAR);
    d9_Samp(nStage, D3DSAMP_MIPFILTER, nMip);
    d9_Samp(nStage, D3DSAMP_MAXANISOTROPY, bAniso ? (DWORD)t->m_fAniso : 1);
    d9_Samp(nStage, D3DSAMP_ADDRESSU, t->m_bRepeatS ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
    d9_Samp(nStage, D3DSAMP_ADDRESSV, t->m_bRepeatT ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
}

static bool s_bUnitQ[D9_UNITS];         // a vertex of this draw has r or q of its own

static void d9_Apply(void)
{
    const D9State& s = s_S;
    d9_RS(D3DRS_ALPHABLENDENABLE, s.m_Caps[RD_BLEND]);
    d9_RS(D3DRS_SRCBLEND, s_Blend[s.m_eSrc]);
    d9_RS(D3DRS_DESTBLEND, s_Blend[s.m_eDst]);
    d9_RS(D3DRS_ALPHATESTENABLE, s.m_Caps[RD_ALPHA_TEST]);
    // GL tests alpha/255 > ref; Direct3D alpha > ref in 0..255.
    int nRef = (int)floorf(s.m_fAlphaRef * 255.0f);
    d9_RS(D3DRS_ALPHAREF, nRef < 0 ? 0 : nRef > 255 ? 255 : nRef);
    d9_RS(D3DRS_ZENABLE, s.m_Caps[RD_DEPTH_TEST] ? D3DZB_TRUE : D3DZB_FALSE);
    d9_RS(D3DRS_ZFUNC, s.m_eDepthFunc == RD_LESS ? D3DCMP_LESS : s.m_eDepthFunc == RD_ALWAYS ? D3DCMP_ALWAYS : D3DCMP_LESSEQUAL);
    d9_RS(D3DRS_ZWRITEENABLE, s.m_bDepthMask);
    d9_RS(D3DRS_COLORWRITEENABLE, s.m_bColorMask ? 0xf : 0);
    d9_RS(D3DRS_FOGENABLE, s.m_Caps[RD_FOG]);
    if (s.m_Caps[RD_FOG])
    {
        DWORD c = D3DCOLOR_COLORVALUE(s.m_Fog[0], s.m_Fog[1], s.m_Fog[2], 1.0f);
        d9_RS(D3DRS_FOGCOLOR, c);
        d9_RS(D3DRS_FOGSTART, d9_F2DW(s.m_fFogStart));
        d9_RS(D3DRS_FOGEND, d9_F2DW(s.m_fFogEnd));
    }
    d9_RS(D3DRS_SCISSORTESTENABLE, s.m_Caps[RD_SCISSOR_TEST]);
    if (s.m_Caps[RD_SCISSOR_TEST])
    {
        RECT rc = { s.m_Scissor[0], s.m_Scissor[1], s.m_Scissor[0] + s.m_Scissor[2], s.m_Scissor[1] + s.m_Scissor[3] };
        s_pDev->SetScissorRect(&rc);
    }

    MR_CHECK(s.m_Viewport[0] >= 0 && s.m_Viewport[1] >= 0 && s.m_Viewport[2] > 0 && s.m_Viewport[3] > 0 &&
             s.m_Viewport[0] + s.m_Viewport[2] <= s_nWidth && s.m_Viewport[1] + s.m_Viewport[3] <= s_nHeight,
             "viewport %d,%d %dx%d outside the %dx%d target (SetViewport refuses it)", s.m_Viewport[0],
             s.m_Viewport[1], s.m_Viewport[2], s.m_Viewport[3], s_nWidth, s_nHeight);
    D3DVIEWPORT9 vp = { (DWORD)s.m_Viewport[0], (DWORD)s.m_Viewport[1],
                        (DWORD)s.m_Viewport[2], (DWORD)s.m_Viewport[3],
                        s.m_DepthRange[0], s.m_DepthRange[1] };
    s_pDev->SetViewport(&vp);

    // GL's projection to Direct3D's: depth from -w..w to 0..w, and the
    // pixel centres half a pixel over (Direct3D 9 samples at integers).
    float dx = vp.Width ? 1.0f / vp.Width : 0.0f, dy = vp.Height ? 1.0f / vp.Height : 0.0f;
    float A[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 0.5f, 0,
        -dx, dy, 0.5f, 1,
    };
    D9Matrix P = { { 0 } };
    memcpy(P.m, A, 64);
    d9_Mul(&P, s_Proj[s_nProj].m);
    // A column-major GL matrix read as Direct3D's row-major one is its
    // transpose, which is what row vectors want.
    s_pDev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)P.m);
    s_pDev->SetTransform(D3DTS_WORLD, (const D3DMATRIX*)s_MV[s_nMV].m);

    // The enabled units with a texture, onto consecutive stages.
    int nStage = 0;
    for (int u = 0; u < g_RDCaps.nTextureUnits; u++)
    {
        const D9Unit& un = s.m_Units[u];
        if (!un.m_bEnable)
            continue;
        D9Texture* t = d9_Tex(un.m_nTex);
        IDirect3DTexture9* pTex = t ? d9_Realise(t) : NULL;
        if (!pTex)
            continue;                   // GL: an incomplete texture turns the unit off
        d9_SetTexture(nStage, pTex);
        d9_Sampler(nStage, t);
        d9_TSS(nStage, D3DTSS_TEXCOORDINDEX, u);
        const D9Matrix* pTM = &s_TexM[u][s_nTexM[u]];
        if (!d9_IsIdentity(pTM) || s_bUnitQ[u])
        {
            s_pDev->SetTransform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + nStage), (const D3DMATRIX*)pTM->m);
            d9_TSS(nStage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT4 | D3DTTFF_PROJECTED);
        }
        else
            d9_TSS(nStage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        if (un.m_eEnv == 2)
        {
            const RDCombine& c = un.m_Combine;
            d9_TSS(nStage, D3DTSS_COLOROP, d9_Op(c.eColorOp, c.fScale));
            d9_TSS(nStage, D3DTSS_COLORARG1, d9_Arg(c.eColor0));
            d9_TSS(nStage, D3DTSS_COLORARG2, d9_Arg(c.eColor1));
            d9_TSS(nStage, D3DTSS_ALPHAOP, d9_Op(c.eAlphaOp, 1.0f));
            d9_TSS(nStage, D3DTSS_ALPHAARG1, d9_Arg(c.eAlpha0));
            d9_TSS(nStage, D3DTSS_ALPHAARG2, d9_Arg(c.eAlpha1));
        }
        else if (un.m_eEnv == RD_ENV_REPLACE)
        {
            // GL's REPLACE keeps the incoming alpha when the texture has none.
            d9_TSS(nStage, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            d9_TSS(nStage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            d9_TSS(nStage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            d9_TSS(nStage, D3DTSS_ALPHAARG1, t->m_bAlpha ? D3DTA_TEXTURE : D3DTA_CURRENT);
        }
        else
        {
            d9_TSS(nStage, D3DTSS_COLOROP, D3DTOP_MODULATE);
            d9_TSS(nStage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            d9_TSS(nStage, D3DTSS_COLORARG2, D3DTA_CURRENT);
            d9_TSS(nStage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            d9_TSS(nStage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            d9_TSS(nStage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
        }
        nStage++;
    }
    if (!nStage)
    {
        d9_SetTexture(0, NULL);
        d9_TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        d9_TSS(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        d9_TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        d9_TSS(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        nStage = 1;
    }
    if (nStage < 8)
    {
        d9_SetTexture(nStage, NULL);
        d9_TSS(nStage, D3DTSS_COLOROP, D3DTOP_DISABLE);
        d9_TSS(nStage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }
}

// ---- drawing -------------------------------------------------------------------------------------

struct D9Vertex {
    float x, y, z;
    DWORD c;
    float t[D9_UNITS][4];
};
#define D9_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX4 | D3DFVF_TEXCOORDSIZE4(0) | \
                D3DFVF_TEXCOORDSIZE4(1) | D3DFVF_TEXCOORDSIZE4(2) | D3DFVF_TEXCOORDSIZE4(3))
#define D9_MAXVERTS 8192
static_assert(sizeof(D9Vertex) == 3 * 4 + 4 + D9_UNITS * 4 * 4, "D9_FVF's stride");

static D9Vertex s_Verts[D9_MAXVERTS];
static D9Vertex s_Quads[D9_MAXVERTS / 4 * 6];
static int      s_nVerts;
static RDPrim   s_ePrim;
static bool     s_bInBegin;
static D9Vertex s_Cur = { 0, 0, 0, 0xffffffff, { { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 } } };

// Depth clamping, which Direct3D 9 does not have: the vertices
// transformed here with the same matrices, depth clamped at the near plane
// (and the far), and drawn pre-transformed, which Direct3D does not clip
// in depth. Per vertex where GL clamps per pixel, which is the same for
// what lies wholly nearer than the near plane: the really-close objects
// (the player's weapon, the diving mask), all in front of the eye. With
// rd_NearDepth, what is nearer than the near plane keeps its order in
// depth instead (by 1/w, which the screen interpolates exactly): only a
// triangle that crosses the near plane is approximate.
struct D9TVertex {
    float x, y, z, rhw;
    DWORD c;
    DWORD fog;                          // alpha: the fog factor, as GL's linear fog on eye depth
    float t[D9_UNITS][2];
};
#define D9_TFVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX4)
static_assert(sizeof(D9TVertex) == 4 * 4 + 4 + 4 + D9_UNITS * 2 * 4, "D9_TFVF's stride");
static D9TVertex s_TVerts[D9_MAXVERTS / 4 * 6 > D9_MAXVERTS * 3 ? D9_MAXVERTS / 4 * 6 : D9_MAXVERTS * 3];

static void d9_Xform(const D9Matrix& M, const float* v, float* o)
{
    for (int r = 0; r < 4; r++)
        o[r] = M.m[r] * v[0] + M.m[4 + r] * v[1] + M.m[8 + r] * v[2] + M.m[12 + r] * v[3];
}

// A vertex in clip space, before the divide.
struct D9CVertex {
    float c[4];
    float col[4];
    float t[D9_UNITS][4];
};

static void d9_Lerp(const D9CVertex& a, const D9CVertex& b, float f, D9CVertex& o)
{
    const float* pa = a.c;
    const float* pb = b.c;
    float* po = o.c;
    for (int i = 0; i < (int)(sizeof(D9CVertex) / sizeof(float)); i++)
        po[i] = pa[i] + (pb[i] - pa[i]) * f;
}

static void d9_Project(const D9CVertex& v, D9TVertex& t)
{
    const int* vp = s_S.m_Viewport;
    float iw = 1.0f / v.c[3];
    float z = (v.c[2] * iw + 1.0f) * 0.5f;                 // GL's -1..1 to 0..1
    const float* nd = s_S.m_NearDepth;
    t.x = vp[0] + (v.c[0] * iw + 1.0f) * 0.5f * vp[2] - 0.5f;
    t.y = vp[1] + (1.0f - v.c[1] * iw) * 0.5f * vp[3] - 0.5f;
    if (z < 0.0f && nd[0] > 0.0f && nd[1] > nd[0])
    {
        // Nearer than the near plane: w is the eye depth, from nd[0]
        // (depth0) to the near plane nd[1] (depth1).
        float g = (1.0f / nd[0] - iw) / (1.0f / nd[0] - 1.0f / nd[1]);
        g = g < 0.0f ? 0.0f : g > 1.0f ? 1.0f : g;
        t.z = nd[2] + g * (nd[3] - nd[2]);
    }
    else
    {
        z = z < 0.0f ? 0.0f : z > 1.0f ? 1.0f : z;
        t.z = s_S.m_DepthRange[0] + z * (s_S.m_DepthRange[1] - s_S.m_DepthRange[0]);
    }
    t.rhw = iw;
    // Direct3D fogs pre-transformed vertices by the factor they carry.
    float fFog = 1.0f;
    if (s_S.m_fFogEnd != s_S.m_fFogStart)
        fFog = (s_S.m_fFogEnd - v.c[3]) / (s_S.m_fFogEnd - s_S.m_fFogStart);
    fFog = fFog < 0.0f ? 0.0f : fFog > 1.0f ? 1.0f : fFog;
    t.fog = (DWORD)(fFog * 255.0f + 0.5f) << 24;
    uint8_t b[4];
    for (int i = 0; i < 4; i++)
        b[i] = (uint8_t)(v.col[i] <= 0.0f ? 0 : v.col[i] >= 255.0f ? 255 : (int)(v.col[i] + 0.5f));
    t.c = D3DCOLOR_ARGB(b[3], b[0], b[1], b[2]);
    for (int u = 0; u < D9_UNITS; u++)
    {
        float q = v.t[u][3] != 0.0f ? 1.0f / v.t[u][3] : 1.0f;
        t.t[u][0] = v.t[u][0] * q;
        t.t[u][1] = v.t[u][1] * q;
    }
}

static void d9_DrawClamped(D3DPRIMITIVETYPE eType, const D9Vertex* pVerts, int nPrims)
{
    D9Matrix MVP = s_Proj[s_nProj];
    d9_Mul(&MVP, s_MV[s_nMV].m);
    const float fMinW = s_S.m_NearDepth[0] > 0.0f ? s_S.m_NearDepth[0] : 1.0e-4f;
    int nPer = eType == D3DPT_LINELIST ? 2 : 3;
    int nOut = 0;
    const int nMaxOut = (int)(sizeof(s_TVerts) / sizeof(s_TVerts[0]));
    for (int p = 0; p < nPrims; p++)
    {
        D9CVertex in[3];
        for (int k = 0; k < nPer; k++)
        {
            const D9Vertex& v = eType == D3DPT_TRIANGLEFAN ? pVerts[k == 0 ? 0 : p + k] : pVerts[p * nPer + k];
            float pos[4] = { v.x, v.y, v.z, 1.0f };
            d9_Xform(MVP, pos, in[k].c);
            in[k].col[0] = (float)((v.c >> 16) & 0xff);
            in[k].col[1] = (float)((v.c >> 8) & 0xff);
            in[k].col[2] = (float)(v.c & 0xff);
            in[k].col[3] = (float)(v.c >> 24);
            for (int u = 0; u < D9_UNITS; u++)
                d9_Xform(s_TexM[u][s_nTexM[u]], v.t[u], in[k].t[u]);
        }
        // Clip against w > 0 (GL clips there; with clamping, nowhere else in
        // depth), or at rd_NearDepth's eye depth.
        D9CVertex poly[4];
        int nPoly = 0;
        for (int k = 0; k < nPer; k++)
        {
            const D9CVertex& a = in[k];
            const D9CVertex& b = in[(k + 1) % nPer];
            bool bIn = a.c[3] >= fMinW, bNextIn = b.c[3] >= fMinW;
            if (bIn)
                poly[nPoly++] = a;
            if (bIn != bNextIn && (nPer == 3 || k == 0))
                d9_Lerp(a, b, (fMinW - a.c[3]) / (b.c[3] - a.c[3]), poly[nPoly++]);
        }
        if (nPer == 2)
        {
            if (nPoly != 2 || nOut + 2 > nMaxOut)
                continue;
            d9_Project(poly[0], s_TVerts[nOut++]);
            d9_Project(poly[1], s_TVerts[nOut++]);
            continue;
        }
        for (int k = 1; k + 1 < nPoly && nOut + 3 <= nMaxOut; k++)
        {
            d9_Project(poly[0], s_TVerts[nOut++]);
            d9_Project(poly[k], s_TVerts[nOut++]);
            d9_Project(poly[k + 1], s_TVerts[nOut++]);
        }
    }
    if (!nOut)
        return;
    MR_ASSERT(nOut <= nMaxOut && nOut % nPer == 0, "clamped draw: %d vertices of %d, %d each", nOut, nMaxOut, nPer);
    for (int i = 0; i < 8; i++)
        d9_TSS(i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    d9_RS(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    s_pDev->SetFVF(D9_TFVF);
    s_pDev->DrawPrimitiveUP(eType == D3DPT_LINELIST ? D3DPT_LINELIST : D3DPT_TRIANGLELIST,
                            nOut / nPer, s_TVerts, sizeof(D9TVertex));
    d9_RS(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
}

static void d9_Draw(int n)
{
    if (!s_pDev)
        return;
    int nPrims;
    D3DPRIMITIVETYPE eType;
    const D9Vertex* pVerts = s_Verts;
    switch (s_ePrim)
    {
    case RD_TRIANGLES:    eType = D3DPT_TRIANGLELIST; nPrims = n / 3; break;
    case RD_TRIANGLE_FAN: eType = D3DPT_TRIANGLEFAN; nPrims = n - 2; break;
    case RD_LINES:        eType = D3DPT_LINELIST; nPrims = n / 2; break;
    default:
        // Each quad as two triangles, 0-1-2 and 0-2-3.
        eType = D3DPT_TRIANGLELIST;
        nPrims = 0;
        for (int i = 0; i + 3 < n; i += 4)
        {
            D9Vertex* q = &s_Quads[nPrims * 3];
            q[0] = s_Verts[i]; q[1] = s_Verts[i + 1]; q[2] = s_Verts[i + 2];
            q[3] = s_Verts[i]; q[4] = s_Verts[i + 2]; q[5] = s_Verts[i + 3];
            nPrims += 2;
        }
        pVerts = s_Quads;
        break;
    }
    if (nPrims <= 0)
        return;
    MR_ASSERT(mr_LockHeld(), "draw without the renderer's lock (thread %lu)", (unsigned long)GetCurrentThreadId());
#ifdef NOLFSRC_DEBUG
    {
        int nNeed = eType == D3DPT_TRIANGLEFAN ? nPrims + 2 : eType == D3DPT_LINELIST ? nPrims * 2 : nPrims * 3;
        int nHave = pVerts == s_Quads ? (int)(sizeof(s_Quads) / sizeof(s_Quads[0])) : n;
        MR_ASSERT(n >= 0 && n <= D9_MAXVERTS && nNeed <= nHave,
                  "draw of %d primitives (type %d) needs %d vertices, has %d (%d gathered)", nPrims, (int)eType,
                  nNeed, nHave, n);
    }
#endif
    d9_BeginScene();
    d9_Apply();
    if (s_S.m_Caps[RD_DEPTH_CLAMP])
    {
        d9_DrawClamped(eType, pVerts, nPrims);
        return;
    }
    s_pDev->SetFVF(D9_FVF);
    s_pDev->DrawPrimitiveUP(eType, nPrims, pVerts, sizeof(D9Vertex));
}

void rd_Begin(RDPrim ePrim)
{
    s_ePrim = ePrim;
    s_nVerts = 0;
    s_bInBegin = true;
    memset(s_bUnitQ, 0, sizeof(s_bUnitQ));
}

void rd_End(void)
{
    s_bInBegin = false;
    d9_Draw(s_nVerts);
    s_nVerts = 0;
}

void rd_Color4ub(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { s_Cur.c = D3DCOLOR_ARGB(a, r, g, b); }

void rd_Color4f(float r, float g, float b, float a)
{
    float c[4] = { r, g, b, a };
    uint8_t v[4];
    for (int i = 0; i < 4; i++)
        v[i] = (uint8_t)(c[i] <= 0.0f ? 0 : c[i] >= 1.0f ? 255 : (int)(c[i] * 255.0f + 0.5f));
    s_Cur.c = D3DCOLOR_ARGB(v[3], v[0], v[1], v[2]);
}

void rd_TexCoord2f(float s, float t)
{
    s_Cur.t[0][0] = s; s_Cur.t[0][1] = t; s_Cur.t[0][2] = 0; s_Cur.t[0][3] = 1;
}

void rd_TexCoord4f(float s, float t, float r, float q)
{
    s_Cur.t[0][0] = s; s_Cur.t[0][1] = t; s_Cur.t[0][2] = r; s_Cur.t[0][3] = q;
}

void rd_MultiTexCoord2f(int nUnit, float s, float t)
{
    if (nUnit < 0 || nUnit >= D9_UNITS)
        return;
    s_Cur.t[nUnit][0] = s; s_Cur.t[nUnit][1] = t; s_Cur.t[nUnit][2] = 0; s_Cur.t[nUnit][3] = 1;
}

void rd_Vertex2f(float x, float y) { rd_Vertex3f(x, y, 0.0f); }

void rd_Vertex3f(float x, float y, float z)
{
    if (!s_bInBegin)
        return;
    if (s_nVerts == D9_MAXVERTS)
    {
        // Draw what makes whole primitives and carry on; a fan keeps its
        // centre and its last vertex.
        int nGroup = s_ePrim == RD_TRIANGLES ? 3 : s_ePrim == RD_QUADS ? 4 : s_ePrim == RD_LINES ? 2 : 0;
        if (nGroup)
        {
            int n = s_nVerts - s_nVerts % nGroup;
            d9_Draw(n);
            memmove(s_Verts, s_Verts + n, (s_nVerts - n) * sizeof(D9Vertex));
            s_nVerts -= n;
        }
        else
        {
            d9_Draw(s_nVerts);
            s_Verts[1] = s_Verts[s_nVerts - 1];
            s_nVerts = 2;
        }
    }
    MR_ASSERT(s_nVerts >= 0 && s_nVerts < D9_MAXVERTS, "vertex %d of %d", s_nVerts, D9_MAXVERTS);
    D9Vertex& v = s_Verts[s_nVerts++];
    v = s_Cur;
    v.x = x;
    v.y = y;
    v.z = z;
    for (int u = 0; u < D9_UNITS; u++)
        if (v.t[u][2] != 0.0f || v.t[u][3] != 1.0f)
            s_bUnitQ[u] = true;
}

// ---- the frame buffer -------------------------------------------------------------------------------

void rd_ClearColor(float r, float g, float b, float a)
{
    s_S.m_Clear[0] = r; s_S.m_Clear[1] = g; s_S.m_Clear[2] = b; s_S.m_Clear[3] = a;
}

// GL clears the scissor rect (or everything), whatever the viewport;
// Direct3D clears within the viewport.
void rd_Clear(uint32_t nWhat)
{
    if (!s_pDev)
        return;
    DWORD nFlags = 0;
    if (nWhat & RD_CLEAR_COLOR)
        nFlags |= D3DCLEAR_TARGET;
    if (nWhat & RD_CLEAR_DEPTH)
        nFlags |= D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
    if (!nFlags)
        return;
    D3DRECT rc = { 0, 0, s_nWidth, s_nHeight };
    if (s_S.m_Caps[RD_SCISSOR_TEST])
    {
        rc.x1 = s_S.m_Scissor[0] > 0 ? s_S.m_Scissor[0] : 0;
        rc.y1 = s_S.m_Scissor[1] > 0 ? s_S.m_Scissor[1] : 0;
        rc.x2 = s_S.m_Scissor[0] + s_S.m_Scissor[2];
        rc.y2 = s_S.m_Scissor[1] + s_S.m_Scissor[3];
        if (rc.x2 > s_nWidth) rc.x2 = s_nWidth;
        if (rc.y2 > s_nHeight) rc.y2 = s_nHeight;
        if (rc.x2 <= rc.x1 || rc.y2 <= rc.y1)
            return;
    }
    d9_BeginScene();
    D3DVIEWPORT9 vp = { 0, 0, (DWORD)s_nWidth, (DWORD)s_nHeight, 0.0f, 1.0f };
    s_pDev->SetViewport(&vp);
    d9_RS(D3DRS_SCISSORTESTENABLE, FALSE);
    d9_RS(D3DRS_ZWRITEENABLE, TRUE);
    d9_RS(D3DRS_COLORWRITEENABLE, 0xf);
    const float* c = s_S.m_Clear;
    s_pDev->Clear(1, &rc, nFlags, D3DCOLOR_COLORVALUE(c[0], c[1], c[2], c[3]), 1.0f, 0);
}

static IDirect3DSurface9* s_pAutoDepth;      // the implicit depth surface, while s_pMSDepth is bound

static void d9_DestroyTarget(void)
{
    // The back buffer and the implicit depth surface bound again first, so
    // the device holds no reference to the surfaces released here.
    if (s_pMSColor && s_pDev)
    {
        IDirect3DSurface9* pBack = NULL;
        if (SUCCEEDED(s_pDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBack)))
        {
            s_pDev->SetRenderTarget(0, pBack);
            pBack->Release();
        }
    }
    if (s_pAutoDepth)
    {
        s_pDev->SetDepthStencilSurface(s_pAutoDepth);
        s_pAutoDepth->Release();
        s_pAutoDepth = NULL;
    }
    if (s_pMSColor)
    {
        s_pMSColor->Release();
        s_pMSColor = NULL;
    }
    if (s_pMSDepth)
    {
        s_pMSDepth->Release();
        s_pMSDepth = NULL;
    }
}

// The multisampled colour and depth surfaces everything draws into, for
// s_eMS (none: the back buffer and the automatic depth buffer).
static void d9_CreateTarget(void)
{
    if (s_eMS == D3DMULTISAMPLE_NONE)
        return;
    if (FAILED(s_pDev->CreateRenderTarget(s_nWidth, s_nHeight, s_pp.BackBufferFormat, s_eMS, 0, FALSE,
                                          &s_pMSColor, NULL)) ||
        FAILED(s_pDev->CreateDepthStencilSurface(s_nWidth, s_nHeight, D3DFMT_D24S8, s_eMS, 0, TRUE,
                                                 &s_pMSDepth, NULL)))
    {
        mr_Print("d3d9.ren: no %dx multisampled target; antialiasing off", (int)s_eMS);
        d9_DestroyTarget();
        s_eMS = D3DMULTISAMPLE_NONE;
        return;
    }
    s_pDev->GetDepthStencilSurface(&s_pAutoDepth);
    s_pDev->SetRenderTarget(0, s_pMSColor);
    s_pDev->SetDepthStencilSurface(s_pMSDepth);
}

static void d9_Resolve(void)
{
    if (!s_pMSColor)
        return;
    d9_EndScene();
    IDirect3DSurface9* pBack = NULL;
    if (SUCCEEDED(s_pDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBack)))
    {
        s_pDev->StretchRect(s_pMSColor, NULL, pBack, NULL, D3DTEXF_NONE);
        pBack->Release();
    }
}

void rd_ReadPixels(int x, int y, int w, int h, RDPixels ePixels, void* pDest, bool bFront)
{
    // The swap effect is COPY, so after a flip the back buffer still holds
    // what is on the screen: the front buffer is read from it too.
    (void)bFront;
    if (!s_pDev || w <= 0 || h <= 0)
        return;
    // Only what lies on the surface is read: the caller's rectangle can lie
    // partly off it (BlitFromScreen takes the engine's), and a row read past
    // the system-memory copy reads past its end. Pixels off the surface come
    // back black.
    if (x < 0 || y < 0 || x + w > s_nWidth || y + h > s_nHeight)
    {
        MR_CHECK(false, "read-back %d,%d %dx%d outside the %dx%d screen; clipped", x, y, w, h, s_nWidth, s_nHeight);
        memset(pDest, 0, (size_t)w * h * (ePixels == RD_PIX_565 ? 2 : 4));
    }
    d9_EndScene();
    if (!bFront)
        d9_Resolve();
    IDirect3DSurface9* pBack = NULL;
    if (FAILED(s_pDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBack)))
        return;
    if (!s_pReadback)
        s_pDev->CreateOffscreenPlainSurface(s_nWidth, s_nHeight, s_pp.BackBufferFormat,
                                            D3DPOOL_SYSTEMMEM, &s_pReadback, NULL);
    D3DLOCKED_RECT lr;
    if (s_pReadback && SUCCEEDED(s_pDev->GetRenderTargetData(pBack, s_pReadback)) &&
        SUCCEEDED(s_pReadback->LockRect(&lr, NULL, D3DLOCK_READONLY)))
    {
        int x0 = x < 0 ? -x : 0, x1 = x + w > s_nWidth ? s_nWidth - x : w;
        for (int r = 0; r < h; r++)
        {
            if (y + r < 0 || y + r >= s_nHeight || x1 <= x0)
                continue;
            const uint32_t* pSrc = (const uint32_t*)((const uint8_t*)lr.pBits + (y + r) * lr.Pitch) + x;
            if (ePixels == RD_PIX_565)
            {
                uint16_t* d = (uint16_t*)pDest + r * w;
                for (int i = x0; i < x1; i++)
                {
                    uint32_t p = pSrc[i];
                    d[i] = (uint16_t)((((p >> 16) & 0xff) >> 3) << 11 | (((p >> 8) & 0xff) >> 2) << 5 | ((p & 0xff) >> 3));
                }
            }
            else
            {
                uint32_t* d = (uint32_t*)pDest + r * w;
                for (int i = x0; i < x1; i++)
                    d[i] = pSrc[i] | 0xff000000;
            }
        }
        s_pReadback->UnlockRect();
    }
    pBack->Release();
}

// ---- the device ---------------------------------------------------------------------------------------

// What every draw starts from, on a new or reset device.
static void d9_StartingStates(void)
{
    d9_ForgetDeviceState();
    d9_RS(D3DRS_LIGHTING, FALSE);
    d9_RS(D3DRS_CULLMODE, D3DCULL_NONE);
    d9_RS(D3DRS_DITHERENABLE, FALSE);
    d9_RS(D3DRS_SPECULARENABLE, FALSE);
    d9_RS(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    d9_RS(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    d9_RS(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
    d9_RS(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    d9_RS(D3DRS_RANGEFOGENABLE, FALSE);
    d9_RS(D3DRS_CLIPPING, TRUE);
    for (int i = 0; i < 8; i++)
        d9_Samp(i, D3DSAMP_MAXMIPLEVEL, 0);
    s_pDev->SetTransform(D3DTS_VIEW, (const D3DMATRIX*)s_Identity.m);
}

bool rd_CreateDevice(void* hWnd, uint32_t nWidth, uint32_t nHeight, bool bWindowed, int nSwapInterval,
                     int nSamples)
{
    (void)bWindowed;
    s_nWidth = (int)nWidth;
    s_nHeight = (int)nHeight;
    s_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!s_pD3D)
    {
        mr_Print("d3d9.ren: Direct3D 9 is not available");
        return false;
    }

    // A windowed device on the window mr_window.cpp sized (the whole
    // display at the mode, for full screen), copying on present so the back
    // buffer keeps the last frame for screenshots.
    memset(&s_pp, 0, sizeof(s_pp));
    s_pp.BackBufferWidth = nWidth;
    s_pp.BackBufferHeight = nHeight;
    s_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    s_pp.BackBufferCount = 1;
    s_pp.SwapEffect = D3DSWAPEFFECT_COPY;
    s_pp.hDeviceWindow = (HWND)hWnd;
    s_pp.Windowed = TRUE;
    s_pp.EnableAutoDepthStencil = TRUE;
    s_pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    s_pp.PresentationInterval = nSwapInterval ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

    // FPU_PRESERVE: Direct3D leaves the FPU alone; RenderScene sets the
    // single precision the engine runs at, as retail d3d.ren does
    // (mr_SetFPUPrecision24, mr_scene.cpp). MULTITHREADED: the loading
    // screen draws from a thread of its own (mr.h, MRLock); the core's lock
    // already lets one thread in at a time, and this makes the runtime and
    // the driver expect calls from more than one thread.
    HRESULT hr = s_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)hWnd,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED,
                                      &s_pp, &s_pDev);
    if (FAILED(hr))
        hr = s_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)hWnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED,
                                  &s_pp, &s_pDev);
    if (FAILED(hr))
    {
        mr_Print("d3d9.ren: could not create a Direct3D 9 device (%08lx)", (unsigned long)hr);
        s_pD3D->Release();
        s_pD3D = NULL;
        return false;
    }

    D3DCAPS9 caps;
    s_pDev->GetDeviceCaps(&caps);
    memset(&g_RDCaps, 0, sizeof(g_RDCaps));
    g_RDCaps.nMaxTextureSize = (int)(caps.MaxTextureWidth < caps.MaxTextureHeight ? caps.MaxTextureWidth : caps.MaxTextureHeight);
    g_RDCaps.nTextureUnits = caps.MaxSimultaneousTextures < D9_UNITS ? (int)caps.MaxSimultaneousTextures : D9_UNITS;
    g_RDCaps.bCombine = (caps.TextureOpCaps & (D3DTEXOPCAPS_ADDSIGNED | D3DTEXOPCAPS_MODULATE2X)) ==
                        (D3DTEXOPCAPS_ADDSIGNED | D3DTEXOPCAPS_MODULATE2X);
    g_RDCaps.bS3TC = true;
    static const D3DFORMAT dxt[3] = { D3DFMT_DXT1, D3DFMT_DXT3, D3DFMT_DXT5 };
    for (int i = 0; i < 3; i++)
        if (FAILED(s_pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0,
                                             D3DRTYPE_TEXTURE, dxt[i])))
            g_RDCaps.bS3TC = false;
    g_RDCaps.bAniso = (caps.RasterCaps & D3DPRASTERCAPS_ANISOTROPY) && caps.MaxAnisotropy > 1;
    g_RDCaps.fMaxAniso = g_RDCaps.bAniso ? (float)caps.MaxAnisotropy : 1.0f;
    g_RDCaps.bDepthClamp = true;
    g_RDCaps.bNearDepth = true;

    // The most samples asked for that the device has.
    s_eMS = D3DMULTISAMPLE_NONE;
    for (int n = nSamples > 16 ? 16 : nSamples; n >= 2; n--)
        if (SUCCEEDED(s_pD3D->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, s_pp.BackBufferFormat,
                                                         TRUE, (D3DMULTISAMPLE_TYPE)n, NULL)) &&
            SUCCEEDED(s_pD3D->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_D24S8,
                                                         TRUE, (D3DMULTISAMPLE_TYPE)n, NULL)))
        {
            s_eMS = (D3DMULTISAMPLE_TYPE)n;
            break;
        }
    d9_CreateTarget();
    g_RDCaps.nSamples = s_eMS == D3DMULTISAMPLE_NONE ? 1 : (int)s_eMS;

    D3DADAPTER_IDENTIFIER9 id;
    if (SUCCEEDED(s_pD3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id)))
        snprintf(s_szDesc, sizeof(s_szDesc), "%s (Direct3D 9, %s)", id.Description, id.Driver);
    else
        snprintf(s_szDesc, sizeof(s_szDesc), "Direct3D 9");

    d9_DefaultState();
    for (int u = 0; u < D9_UNITS; u++)
    {
        s_nTexM[u] = 0;
        s_TexM[u][0] = s_Identity;
    }
    s_nMV = s_nProj = 0;
    s_MV[0] = s_Proj[0] = s_Identity;
    s_eMode = RD_MODELVIEW;
    d9_StartingStates();
    rd_Clear(RD_CLEAR_COLOR | RD_CLEAR_DEPTH);
    return true;
}

void rd_DestroyDevice(void)
{
    d9_EndScene();
    d9_FreeTextures();
    d9_DestroyTarget();
    if (s_pReadback)
    {
        s_pReadback->Release();
        s_pReadback = NULL;
    }
    if (s_pDev)
    {
        s_pDev->Release();
        s_pDev = NULL;
    }
    if (s_pD3D)
    {
        s_pD3D->Release();
        s_pD3D = NULL;
    }
}

void rd_Present(void)
{
    if (!s_pDev)
        return;
    MR_ASSERT(mr_LockHeld(), "present without the renderer's lock (thread %lu)", (unsigned long)GetCurrentThreadId());
    d9_EndScene();
    d9_Resolve();
    HRESULT hr = s_pDev->Present(NULL, NULL, NULL, NULL);
    if (hr != D3DERR_DEVICELOST)
        return;
    // Lost (another program took the display): wait until it can be reset,
    // then reset it. Managed textures come back by themselves.
    if (s_pDev->TestCooperativeLevel() != D3DERR_DEVICENOTRESET)
        return;
    if (s_pReadback)
    {
        s_pReadback->Release();
        s_pReadback = NULL;
    }
    d9_DestroyTarget();
    if (SUCCEEDED(s_pDev->Reset(&s_pp)))
    {
        d9_CreateTarget();
        d9_StartingStates();
        mr_Debug(1, "device reset");
    }
}

const char* rd_Describe(void) { return s_szDesc; }

void rd_PrintInfo(void (*pPrint)(const char* pFormat, ...))
{
    pPrint("Direct3D 9: %s", s_szDesc);
}
