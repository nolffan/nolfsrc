// p3_ui.cpp -- the modern build's menus and HUD at any resolution
// (MODERN.md, package P3).  See p3_ui.h.

#include "stdafx.h"
#include "VarTrack.h"
#include "InterfaceResMgr.h"
#include "InterfaceMgr.h"
#include "LayoutMgr.h"
#include "FolderMgr.h"
#include "FXStructs.h"
#include "cshell/p3_ui.h"

VarTrack g_vtP3UIScale;
VarTrack g_vtP3ShowFramerate;
VarTrack g_vtP3NoFunMenus;
VarTrack g_vtP3Cinematics4x3;
VarTrack g_vtP3UseGotyMenu;

void P3_InitVars()
{
	if (g_vtP3UIScale.IsInitted()) return;

	g_vtP3UIScale.Init(g_pLTClient, "UIScale", LTNULL, 0.5f);
	g_vtP3ShowFramerate.Init(g_pLTClient, "ShowFramerate", LTNULL, 0.0f);
	g_vtP3NoFunMenus.Init(g_pLTClient, "NoFunMenus", LTNULL, 0.0f);
	g_vtP3Cinematics4x3.Init(g_pLTClient, "RestrictCinematicsTo4x3", LTNULL, 0.0f);
	g_vtP3UseGotyMenu.Init(g_pLTClient, "UseGotyMenu", LTNULL, 1.0f);
}

// ---------------------------------------------------------------------------
// M-P3-01: the 4:3 menu area

int P3_MenuMargin(int nWidth, int nHeight)
{
	// the width a 4:3 area of the screen's height takes, centred
	return (nWidth - (nHeight * 4) / 3) / 2;
}

float P3_MenuScale()
{
	return g_pInterfaceResMgr->GetYRatio();
}

// The layout puts things it hides at negative positions (-64,-64 for an
// unused arrow, -100,-100 for an unused title): those stay off the screen,
// rather than landing in the side margin.
int P3_MenuX(int x)
{
	int xo = g_pInterfaceResMgr->GetXOffset();
	if (x < 0)
		return (int)((float)x * P3_MenuScale()) - (xo > 0 ? xo : 0);
	return (int)((float)x * P3_MenuScale()) + xo;
}

int P3_MenuY(int y)
{
	int yo = g_pInterfaceResMgr->GetYOffset();
	if (y < 0)
		return (int)((float)y * P3_MenuScale()) - (yo > 0 ? yo : 0);
	return (int)((float)y * P3_MenuScale()) + yo;
}

LTIntPt P3_MenuPos(LTIntPt pos)
{
	return LTIntPt(P3_MenuX(pos.x), P3_MenuY(pos.y));
}

LTRect P3_MenuRect(const LTRect &rect)
{
	return LTRect(P3_MenuX(rect.left), P3_MenuY(rect.top),
				  P3_MenuX(rect.right), P3_MenuY(rect.bottom));
}

int P3_MenuLen(int n)
{
	return (int)((float)n * P3_MenuScale());
}

// Mouse points reach the folders as the retail game passes them: relative
// to the menu area's offset (CInterfaceMgr subtracts it).
void P3_PointToCtrl(CLTGUICtrl *pCtrl, int &x, int &y)
{
	if (!pCtrl) return;
	LTIntPt pos = pCtrl->GetPos();
	x = pos.x + (x + g_pInterfaceResMgr->GetXOffset() - P3_MenuX(pos.x));
	y = pos.y + (y + g_pInterfaceResMgr->GetYOffset() - P3_MenuY(pos.y));
}

LTBOOL P3_PointOnCtrl(CLTGUICtrl *pCtrl, int x, int y, LTBOOL bRow)
{
	if (!pCtrl) return LTFALSE;
	x += g_pInterfaceResMgr->GetXOffset();
	y += g_pInterfaceResMgr->GetYOffset();
	LTIntPt pos = pCtrl->GetPos();
	int nLeft = P3_MenuX(pos.x);
	int nTop = P3_MenuY(pos.y);
	int nWidth = pCtrl->GetWidth();
	int nHeight = pCtrl->GetHeight();

	// A control's size is its text's, which is drawn at the menu scale
	// (M-P3-23): on the screen it covers just that.
	(void)bRow;

	return (x >= nLeft && x <= nLeft + nWidth &&
			y >= nTop && y <= nTop + nHeight);
}

int P3_CtrlToScreenX(CLTGUICtrl *pCtrl, int x)
{
	LTIntPt pos = pCtrl->GetPos();
	return x - pos.x + P3_MenuX(pos.x);
}

int P3_CtrlToScreenY(CLTGUICtrl *pCtrl, int y)
{
	LTIntPt pos = pCtrl->GetPos();
	return y - pos.y + P3_MenuY(pos.y);
}

int P3_LayoutLen(int n)
{
	return (int)((float)n / P3_MenuScale());
}

int P3_MenuCentreX(int cx, int w)
{
	return cx - (int)((float)(w / 2) / P3_MenuScale());
}

int P3_MenuCentreY(int cy, int h)
{
	return cy - (int)((float)(h / 2) / P3_MenuScale());
}

// ---------------------------------------------------------------------------
// M-P3-02: the side margins

void P3_DrawSideBars(HSURFACE hDest, HLTCOLOR hColor)
{
	int xo = g_pInterfaceResMgr->GetXOffset();
	if (xo <= 0 || !hDest) return;

	int nWidth = (int)g_pInterfaceResMgr->GetScreenWidth();
	int nHeight = (int)g_pInterfaceResMgr->GetScreenHeight();

	LTRect rect(0, 0, xo, nHeight);
	g_pLTClient->FillRect(hDest, &rect, hColor);
	rect.Init(nWidth - xo, 0, nWidth, nHeight);
	g_pLTClient->FillRect(hDest, &rect, hColor);
}

// Black, not the layout's shade colour (M-P3-22): the margins are beside
// the page, not part of it.
void P3_DrawMenuSideBars(HSURFACE hDest)
{
	P3_DrawSideBars(hDest, SETRGB(0,0,0));
}

// ---------------------------------------------------------------------------
// M-P3-05: a 640x480 background drawn into the 4:3 area

void P3_DrawBackground(HSURFACE hDest, HSURFACE hBack)
{
	if (!hDest || !hBack) return;

	uint32 nWidth = 0, nHeight = 0;
	g_pLTClient->GetSurfaceDims(hBack, &nWidth, &nHeight);
	LTRect rcSrc(0, 0, (int)nWidth, (int)nHeight);

	// the image's 640x480 frame, scaled into the 4:3 area
	LTRect rcDst(P3_MenuX(0), P3_MenuY(0),
				 P3_MenuX((int)nWidth), P3_MenuY((int)nHeight));
	g_pLTClient->ScaleSurfaceToSurface(hDest, hBack, &rcDst, &rcSrc);

	P3_DrawMenuSideBars(hDest);
}

// ---------------------------------------------------------------------------
// M-P3-08: 4:3 cinematics

LTBOOL P3_Cinematics4x3()
{
	return g_vtP3Cinematics4x3.GetFloat() != 0.0f;
}

// ---------------------------------------------------------------------------
// M-P3-09: overlays drawn for 4:3

float P3_WideScreenCover()
{
	float fWidth = (float)g_pInterfaceResMgr->GetScreenWidth();
	float fHeight = (float)g_pInterfaceResMgr->GetScreenHeight();
	if (fHeight <= 0.0f) return 1.0f;

	float fCover = (fWidth / fHeight) / (4.0f / 3.0f);
	return (fCover > 1.0f) ? fCover : 1.0f;
}

// ---------------------------------------------------------------------------
// M-P3-10: original or Game of the Year main menu art
//
// The Game of the Year archive replaces the main menu's background and logo
// textures.  The modern build's data recipes install the original edition's
// two textures under new names (with sprite files pointing at them), and the
// two scale FX that show them are switched between the two sprites.

namespace
{
	struct P3MenuArt
	{
		const char	*szFX;			// the scale FX
		const char	*szGoty;		// its retail sprite
		const char	*szOriginal;	// the original edition's, from our recipe
	};

	const P3MenuArt s_aMenuArt[] =
	{
		{ "InterfaceSpr1",	"menu\\Spr\\mainmenu1.spr",	"menu\\spr\\mainmenu1b.spr" },
		{ "mainnolftext",	"menu\\Spr\\nolf.spr",		"menu\\spr\\nolfb.spr" },
	};
}

void P3_MenuArt(CScaleFX *pScaleFX)
{
	if (!pScaleFX) return;

	LTBOOL bGoty = (g_vtP3UseGotyMenu.GetFloat(1.0f) != 0.0f);
	for (int i = 0; i < sizeof(s_aMenuArt) / sizeof(s_aMenuArt[0]); i++)
	{
		if (stricmp(pScaleFX->szName, s_aMenuArt[i].szFX) == 0)
		{
			const char *szFile = bGoty ? s_aMenuArt[i].szGoty : s_aMenuArt[i].szOriginal;
			strncpy(pScaleFX->szFile, szFile, sizeof(pScaleFX->szFile) - 1);
			pScaleFX->szFile[sizeof(pScaleFX->szFile) - 1] = '\0';
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// M-P3-11: seasonal main menus

int P3_MainFolder(int nFolderID)
{
	if (nFolderID != FOLDER_ID_MAIN) return nFolderID;
	if (g_vtP3NoFunMenus.GetFloat() != 0.0f) return FOLDER_ID_MAIN;

	SYSTEMTIME st;
	GetLocalTime(&st);

	// 15 to 31 December, both included
	if (st.wMonth == 12 && st.wDay >= 15)
		return FOLDER_ID_MAIN_WINTER;

	// Fridays
	if (st.wDayOfWeek == 5)
		return FOLDER_ID_MAIN_CASUAL;

	return FOLDER_ID_MAIN;
}

// ---------------------------------------------------------------------------
// M-P3-12: HUD scaling

float P3_HUDScale()
{
	float fScale = g_vtP3UIScale.GetFloat(0.5f);
	if (fScale > 4.0f) fScale = 4.0f;
	fScale *= g_pInterfaceResMgr->GetYRatio();

	// never smaller than the retail game draws it
	return (fScale < 1.0f) ? 1.0f : fScale;
}

void P3_DrawScaled(HSURFACE hDest, HSURFACE hSrc, int x, int y, float fScale, HLTCOLOR hTrans)
{
	if (!hDest || !hSrc) return;

	uint32 nWidth = 0, nHeight = 0;
	g_pLTClient->GetSurfaceDims(hSrc, &nWidth, &nHeight);

	LTRect rcSrc(0, 0, (int)nWidth, (int)nHeight);
	LTRect rcDst(x, y, x + (int)((float)nWidth * fScale), y + (int)((float)nHeight * fScale));
	g_pLTClient->ScaleSurfaceToSurfaceTransparent(hDest, hSrc, &rcDst, &rcSrc, hTrans);
}

void P3_DrawScaledSolid(HSURFACE hDest, HSURFACE hSrc, int x, int y, float fScale)
{
	if (!hDest || !hSrc) return;

	uint32 nWidth = 0, nHeight = 0;
	g_pLTClient->GetSurfaceDims(hSrc, &nWidth, &nHeight);

	LTRect rcSrc(0, 0, (int)nWidth, (int)nHeight);
	LTRect rcDst(x, y, x + (int)((float)nWidth * fScale), y + (int)((float)nHeight * fScale));
	g_pLTClient->ScaleSurfaceToSurface(hDest, hSrc, &rcDst, &rcSrc);
}

// ---------------------------------------------------------------------------
// M-P3-14: frame-rate counter

namespace
{
	const int	kFrameSamples = 10;
	float		s_fFrameTimeSum = 0.0f;
	int			s_nFrameSamples = 0;
	float		s_fFramerate = 0.0f;
}

void P3_DrawFramerate(HSURFACE hScreen)
{
	if (g_vtP3ShowFramerate.GetFloat() == 0.0f || !hScreen) return;

	// the average over the last ten frames
	s_fFrameTimeSum += g_pLTClient->GetFrameTime();
	if (++s_nFrameSamples >= kFrameSamples)
	{
		s_fFramerate = (s_fFrameTimeSum > 0.0f) ? (float)s_nFrameSamples / s_fFrameTimeSum : 0.0f;
		s_fFrameTimeSum = 0.0f;
		s_nFrameSamples = 0;
	}

	CLTGUIFont *pFont = g_pInterfaceResMgr->GetHelpFont();
	if (!pFont) return;

	char szText[32];
	sprintf(szText, "%.0f fps", s_fFramerate);

	int x = 8;
	int y = (int)g_pInterfaceResMgr->GetScreenHeight() - pFont->GetHeight() - 8;
	pFont->Draw(szText, hScreen, x + 1, y + 1, LTF_JUSTIFY_LEFT, kBlack);
	pFont->Draw(szText, hScreen, x, y, LTF_JUSTIFY_LEFT, kWhite);
}
