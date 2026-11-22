// ----------------------------------------------------------------------- //
//
// MODULE  : p2_client.cpp
//
// PURPOSE : P2's client call-outs (p2_client.h) and M-P2-12, the speed-run
//           block (NOLFSRC_MODERN only)
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "GameClientShell.h"
#include "cshell/p2_client.h"
#include "cshell/p2_console.h"
#include "cshell/p2_bighead.h"
#include "cshell/p2_log.h"

extern CGameClientShell* g_pGameClientShell;


// ----------------------------------------------------------------------- //
//  M-P2-12: speed-run helpers
// ----------------------------------------------------------------------- //
//
// Load-removing timers and auto-splitters for speed runs read the current
// mission, the current level and whether a loading screen is up straight
// out of the game's memory (NOLF Modernizer logs the addresses of its
// three variables for them). Ours are kept together in one block, which
// CShell.dll exports by name and which starts with a signature, so a tool
// can find it from the export table or by scanning memory; the modern log
// gives the addresses at startup. The values are copied in every frame and
// whenever a loading screen comes up or goes away.

struct P2SpeedRunInfo
{
	char	szSignature[16];	// "NOLFSPEEDRUNINFO", no terminating zero
	int32	nVersion;			// this layout: 1
	int32	nMission;			// mission index (CGameClientShell::GetCurrentMission), -1 none
	int32	nLevel;				// level within it (GetCurrentLevel), -1 none
	int32	bLoading;			// 1 while a loading screen is up and the level loads
								// (0 while M-P3-27's prompt waits for a key)
};

extern "C" __declspec(dllexport) P2SpeedRunInfo NolfSpeedRunInfo =
{
	{ 'N','O','L','F','S','P','E','E','D','R','U','N','I','N','F','O' },
	1, -1, -1, 0
};

static void SpeedRunSample()
{
	if (!g_pGameClientShell) return;

	NolfSpeedRunInfo.nMission = g_pGameClientShell->GetCurrentMission();
	NolfSpeedRunInfo.nLevel   = g_pGameClientShell->GetCurrentLevel();
}


// ----------------------------------------------------------------------- //
//  The call-outs
// ----------------------------------------------------------------------- //

void P2_OnEngineInitialized()
{
	P2BigHead_Init();
	P2Console_Init();

	ModernLog("Speed-run block (CShell.dll export NolfSpeedRunInfo) at %p: "
	          "mission %p, level %p, loading %p (32-bit integers)",
	          &NolfSpeedRunInfo, &NolfSpeedRunInfo.nMission,
	          &NolfSpeedRunInfo.nLevel, &NolfSpeedRunInfo.bLoading);
}

void P2_OnEngineTerm()
{
	P2Console_Term();
}

void P2_OnEnterWorld()
{
	SpeedRunSample();
	P2BigHead_SendToServer();
}

void P2_Update()
{
	SpeedRunSample();
}

void P2_OnLoadingScreen(LTBOOL bShown)
{
	SpeedRunSample();
	NolfSpeedRunInfo.bLoading = bShown ? 1 : 0;

	P2Console_SetLock(P2CONSOLE_LOCK_LOADING, bShown);
}

void P2_OnLoadingDone()
{
	SpeedRunSample();
	NolfSpeedRunInfo.bLoading = 0;
}

LTBOOL P2_OnCommandOn(int nCommand)
{
	return P2Console_OnCommandOn(nCommand);
}
