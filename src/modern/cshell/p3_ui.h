// p3_ui.h -- the modern build's menus and HUD at any resolution
// (MODERN.md, package P3).  Compiled only into the modern build; retail
// files reach it from #ifdef NOLFSRC_MODERN blocks.
//
// The retail interface is laid out on a 640x480 page.  The menus put that
// page in the screen's centred 4:3 area, scaled by the screen's height ratio
// (the "menu scale"), and fill the side margins; the 3D parts of the menus
// stay as the interface camera draws them.  Text is drawn at its bitmap
// font's size, so only positions and the widths between columns scale.

#ifndef P3_UI_H
#define P3_UI_H

class CLTGUICtrl;
class VarTrack;
struct CScaleFX;

// ---- Console variables (MODERN.md, "New console / config variables")

extern VarTrack g_vtP3UIScale;          // UIScale
extern VarTrack g_vtP3ShowFramerate;    // ShowFramerate
extern VarTrack g_vtP3NoFunMenus;       // NoFunMenus
extern VarTrack g_vtP3Cinematics4x3;    // RestrictCinematicsTo4x3
extern VarTrack g_vtP3UseGotyMenu;      // UseGotyMenu
extern VarTrack g_vtP3MainMenuCate;     // MainMenuCate (M-P8-10)

// MainMenuCate's values: the Cate of the main menu
enum
{
	P3_MENUCATE_SEASONAL = 0,	// the day's: winter 15-31 December, casual on Fridays
	P3_MENUCATE_ORIGINAL,
	P3_MENUCATE_CASUAL,
	P3_MENUCATE_WINTER,
	P3_MENUCATE_YOUNG,			// the PS2 release's young Cate (with its levels)
	P3_MENUCATE_COUNT
};

void        P3_InitVars();

// ---- M-P3-01: the 4:3 menu area

// Called by CInterfaceResMgr::ScreenDimsChanged: the side margin of the
// centred 4:3 area (negative on screens narrower than 4:3).
int         P3_MenuMargin(int nWidth, int nHeight);

float       P3_MenuScale();                 // layout units -> pixels
int         P3_MenuX(int x);                // layout x -> screen x
int         P3_MenuY(int y);                // layout y -> screen y
LTIntPt     P3_MenuPos(LTIntPt pos);
LTRect      P3_MenuRect(const LTRect &rect);
int         P3_MenuLen(int n);              // a layout length in pixels
int         P3_LayoutLen(int n);            // a length in pixels in layout units

// A control is kept at its layout position and is put at its place on the
// screen only while it draws.  A mouse point (as the folders get it: the
// screen point less the menu area's offset) handed to a control is moved
// into the same frame: the control's layout position plus the point's
// offset from where the control was drawn.
void        P3_PointToCtrl(CLTGUICtrl *pCtrl, int &x, int &y);
// Whether a mouse point (as the folders get it) is on a control drawn at its
// layout position; a row of a folder's list (bRow) covers its scaled size.
LTBOOL      P3_PointOnCtrl(CLTGUICtrl *pCtrl, int x, int y, LTBOOL bRow = LTFALSE);
// Back from a control's frame to the screen (for a control such as the list
// control, whose parts are placed on the screen when it draws).
int         P3_CtrlToScreenX(CLTGUICtrl *pCtrl, int x);
int         P3_CtrlToScreenY(CLTGUICtrl *pCtrl, int y);

// The layout x/y at which something w (h) pixels wide (high) must be placed
// so that it is centred on the layout point cx (cy).
int         P3_MenuCentreX(int cx, int w);
int         P3_MenuCentreY(int cy, int h);

// ---- M-P3-02: the side margins

// Fill the side margins of the 4:3 area with a colour (LTNULL = black).
void        P3_DrawSideBars(HSURFACE hDest, HLTCOLOR hColor);

// The layout's shade colour, used for the menus' side margins.
void        P3_DrawMenuSideBars(HSURFACE hDest);

// ---- M-P3-05: a 640x480 background drawn into the 4:3 area

void        P3_DrawBackground(HSURFACE hDest, HSURFACE hBack);

// ---- M-P3-08: 4:3 cinematics

LTBOOL      P3_Cinematics4x3();

// ---- M-P3-09: overlays drawn for 4:3

// How much a 4:3 overlay must grow to cover the screen's width (1 at 4:3).
float       P3_WideScreenCover();

// ---- M-P3-10: original or Game of the Year main menu art

void        P3_MenuArt(CScaleFX *pScaleFX);

// ---- M-P3-11: seasonal main menus

int         P3_MainFolder(int nFolderID);  // FOLDER_ID_MAIN -> today's main menu
LTBOOL      P3_IsMainFolder(int nFolderID); // one of the main menus (M-P8-10)

// ---- M-P3-12: HUD scaling

float       P3_HUDScale();
// Draw a surface scaled by fScale with its top-left corner at (x, y).
void        P3_DrawScaled(HSURFACE hDest, HSURFACE hSrc, int x, int y,
                          float fScale, HLTCOLOR hTrans);
void        P3_DrawScaledSolid(HSURFACE hDest, HSURFACE hSrc, int x, int y, float fScale);

// ---- M-P3-14: frame-rate counter

void        P3_DrawFramerate(HSURFACE hScreen);

#endif // P3_UI_H
