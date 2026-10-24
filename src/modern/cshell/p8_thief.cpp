// ----------------------------------------------------------------------- //
//
// MODULE  : p8_thief.cpp
//
// PURPOSE : P8's client call-outs (p8_thief.h): the PS2 "Nine years ago"
//           levels (NOLFSRC_MODERN only)
//
// ----------------------------------------------------------------------- //
//
// The PS2 release played three flashbacks of young Cate as a thief inside
// missions 3, 5 and 13 (Worlds\thiefmission1-3, converted from the PS2 disc
// by build/ps2/). Its client knew them by mission and level; here they are
// known by world name, which also covers +runworld.

#include "stdafx.h"
#include "InterfaceMgr.h"
#include "LayoutMgr.h"
#include "FolderMgr.h"
#include "ClientRes.h"
#include "cshell/p8_thief.h"
#include "cres/p8.h"

static LTBOOL s_bThiefLevel = LTFALSE;

static LTBOOL P8_IsThiefWorld(const char* pWorldFile)
{
	if (!pWorldFile) return LTFALSE;

	const char* pName = strrchr(pWorldFile, '\\');
	const char* pSlash = strrchr(pWorldFile, '/');
	if (pSlash > pName) pName = pSlash;
	pName = pName ? pName + 1 : pWorldFile;

	return _strnicmp(pName, "thiefmission", 12) == 0;
}

void P8_OnLoadWorld(const char* pWorldFile, int nLevel)
{
	s_bThiefLevel = P8_IsThiefWorld(pWorldFile);
	if (!s_bThiefLevel) return;

	// As on the PS2: "Nine years ago...", the scene of the mission it is
	// played in, and the thief photo

	HSTRING hTxt = g_pLTClient->FormatString(IDS_P8_NINE_YEARS_AGO);
	if (nLevel >= 0)
	{
		char tmp[128];
		HSTRING hTxt2 = g_pLTClient->FormatString(IDS_SCENENUMBER, nLevel + 1);
		_snprintf(tmp, sizeof(tmp), "%s, %s", g_pLTClient->GetStringData(hTxt), g_pLTClient->GetStringData(hTxt2));
		tmp[sizeof(tmp) - 1] = '\0';
		HSTRING hWorld = g_pLTClient->CreateString(tmp);
		g_pInterfaceMgr->SetLoadLevelString(hWorld);
		g_pLTClient->FreeString(hWorld);
		g_pLTClient->FreeString(hTxt2);
	}
	else
	{
		g_pInterfaceMgr->SetLoadLevelString(hTxt);
	}
	g_pLTClient->FreeString(hTxt);

	g_pInterfaceMgr->SetLoadLevelPhoto("interface\\photos\\missions\\thief.pcx");
}

const char* P8_GetHandsSkin()
{
	return s_bThiefLevel ? "guns\\skins_pv\\ThiefHands_pv.dtx" : LTNULL;
}

LTBOOL P8_HaveYoungCateMenu()
{
	// build/ps2/data's LAYOUT recipe adds the folder's section, beside the
	// menu model the PS2 install makes (Chars\Models\IntHero_Thief.abc)
	return g_pLayoutMgr && g_pLayoutMgr->HasCustomValue(FOLDER_ID_MAIN_THIEF, "Character");
}
