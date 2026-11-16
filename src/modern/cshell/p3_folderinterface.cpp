// p3_folderinterface.cpp -- the Interface options folder (MODERN.md,
// M-P3-13): the HUD's size (M-P3-12), the frame-rate cap (M-P1-01's
// FramerateLimit, M-P1-25), the frame-rate counter (M-P3-14), the cutscenes' framing
// (M-P3-08) and the main menu's art (M-P3-10).

#include "stdafx.h"
#include "FolderMgr.h"
#include "FolderCommands.h"
#include "ClientRes.h"
#include "InterfaceMgr.h"
#include "VarTrack.h"
#include "cshell/p3_ui.h"
#include "cshell/p1_modern.h"
#include "cshell/p3_folderinterface.h"
#include "cres/p3.h"
#include "cres/p1.h"
#include "cres/p8.h"
#include "cshell/p9_mods.h"

namespace
{
	int kGap = 225;
	int kWidth = 200;

	// M-P1-25: the frame-rate limit's choices, FramerateLimit's values
	const int s_aFramerates[] = { 60, 120, 144, 240, 0 };
	const int s_nFramerates = sizeof(s_aFramerates) / sizeof(s_aFramerates[0]);
}

CFolderInterface::CFolderInterface()
{
	m_nHUDScale = 50;
	m_nFramerate = 3;
	m_bShowFramerate = LTFALSE;
	m_bCinematics4x3 = LTFALSE;
	m_bGotyMenu = LTTRUE;
	m_nMenuCate = 0;
}

CFolderInterface::~CFolderInterface()
{
}

LTBOOL CFolderInterface::Build()
{
	CreateTitle(IDS_P3_TITLE_INTERFACE);

	if (g_pLayoutMgr->HasCustomValue(FOLDER_ID_INTERFACE,"ColumnWidth"))
		kGap = g_pLayoutMgr->GetFolderCustomInt(FOLDER_ID_INTERFACE,"ColumnWidth");
	if (g_pLayoutMgr->HasCustomValue(FOLDER_ID_INTERFACE,"SliderWidth"))
		kWidth = g_pLayoutMgr->GetFolderCustomInt(FOLDER_ID_INTERFACE,"SliderWidth");
	kGap = P3_MenuLen(kGap);
	kWidth = P3_MenuLen(kWidth);

	CSliderCtrl *pSlider = AddSlider(IDS_P3_HUD_SCALE, IDS_P3_HELP_HUD_SCALE, kGap, kWidth, &m_nHUDScale);
	pSlider->SetSliderRange(25, 100);
	pSlider->SetSliderIncrement(5);
	pSlider->SetNumericDisplay(LTTRUE);

	// M-P1-25: 60, 120, 144 or 240 frames a second, or no limit
	CCycleCtrl *pFramerate = AddCycleItem(IDS_P3_FRAMERATE_LOCK, IDS_P3_HELP_FRAMERATE_LOCK, kGap, 25, &m_nFramerate);
	pFramerate->AddString(IDS_P1_FPS_60);
	pFramerate->AddString(IDS_P1_FPS_120);
	pFramerate->AddString(IDS_P1_FPS_144);
	pFramerate->AddString(IDS_P1_FPS_240);
	pFramerate->AddString(IDS_P3_FPS_UNLOCKED);

	CToggleCtrl *pToggle;

	pToggle = AddToggle(IDS_P3_SHOW_FRAMERATE, IDS_P3_HELP_SHOW_FRAMERATE, kGap, &m_bShowFramerate);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	pToggle = AddToggle(IDS_P3_CINEMATICS, IDS_P3_HELP_CINEMATICS, kGap, &m_bCinematics4x3);
	pToggle->SetOnString(IDS_P3_CINEMATICS_4X3);
	pToggle->SetOffString(IDS_P3_CINEMATICS_WIDE);

	pToggle = AddToggle(IDS_P3_MENU_STYLE, IDS_P3_HELP_MENU_STYLE, kGap, &m_bGotyMenu);
	pToggle->SetOnString(IDS_P3_MENU_GOTY);
	pToggle->SetOffString(IDS_P3_MENU_ORIGINAL);

	// M-P8-10: the main menu's Cate (young Cate with the PS2 levels)
	CCycleCtrl *pCate = AddCycleItem(IDS_P8_MENU_CATE, IDS_P8_HELP_MENU_CATE, kGap, 25, &m_nMenuCate);
	pCate->AddString(IDS_P8_MENU_CATE_SEASONAL);
	pCate->AddString(IDS_P8_MENU_CATE_ORIGINAL);
	pCate->AddString(IDS_P8_MENU_CATE_CASUAL);
	pCate->AddString(IDS_P8_MENU_CATE_WINTER);
	if (P9_HaveThiefMenu())
		pCate->AddString(IDS_P8_MENU_CATE_YOUNG);

	// Make sure to call the base class
	if (!CBaseFolder::Build()) return LTFALSE;

	UseBack(LTTRUE, LTTRUE);
	return LTTRUE;
}

void CFolderInterface::OnFocus(LTBOOL bFocus)
{
	if (bFocus)
	{
		m_nHUDScale = (int)(100.0f * g_vtP3UIScale.GetFloat(0.5f) + 0.5f);
		// the choice for the limit in use; one not among them, the next
		// higher (or none)
		int nLimit = P1Config_FramerateLimit();
		for (m_nFramerate = 0; m_nFramerate < s_nFramerates - 1; m_nFramerate++)
		{
			if (nLimit > 0 && s_aFramerates[m_nFramerate] >= nLimit)
				break;
		}
		m_bShowFramerate = (g_vtP3ShowFramerate.GetFloat() != 0.0f);
		m_bCinematics4x3 = (g_vtP3Cinematics4x3.GetFloat() != 0.0f);
		m_bGotyMenu = (g_vtP3UseGotyMenu.GetFloat(1.0f) != 0.0f);
		m_nMenuCate = (int)g_vtP3MainMenuCate.GetFloat();
		if (m_nMenuCate < 0 || m_nMenuCate >= P3_MENUCATE_COUNT ||
			(m_nMenuCate == P3_MENUCATE_YOUNG && !P9_HaveThiefMenu()))
			m_nMenuCate = P3_MENUCATE_SEASONAL;

		UpdateData(LTFALSE);
	}
	else
	{
		UpdateData();

		WriteConsoleFloat("UIScale", (LTFLOAT)m_nHUDScale / 100.0f);
		WriteConsoleInt("FramerateLimit", s_aFramerates[m_nFramerate]);
		WriteConsoleInt("ShowFramerate", m_bShowFramerate ? 1 : 0);
		WriteConsoleInt("RestrictCinematicsTo4x3", m_bCinematics4x3 ? 1 : 0);
		WriteConsoleInt("UseGotyMenu", m_bGotyMenu ? 1 : 0);
		WriteConsoleInt("MainMenuCate", m_nMenuCate);

		g_pLTClient->WriteConfigFile("autoexec.cfg");
	}
	CBaseFolder::OnFocus(bFocus);
}
