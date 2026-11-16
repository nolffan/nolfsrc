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
}

const char* P8_GetHandsSkin()
{
	return s_bThiefLevel ? "guns\\skins_pv\\ThiefHands_pv.dtx" : LTNULL;
}
