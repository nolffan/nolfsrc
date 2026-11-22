// ----------------------------------------------------------------------- //
//
// MODULE  : p3_loadprompt.cpp
//
// PURPOSE : M-P3-27: "Press any key to continue." on the loading screen
//           once a level has loaded (p3_loadprompt.h).  Compiled only in
//           the modern build (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "GameClientShell.h"
#include "InterfaceMgr.h"
#include "LoadingScreen.h"
#include "MsgIDs.h"
#include "VarTrack.h"
#include "WinUtil.h"
#include "cres/p3.h"
#include "cshell/p2_client.h"
#include "cshell/p3_ui.h"
#include "cshell/p3_loadprompt.h"
#include "cshell/p7_pad.h"

extern CGameClientShell*	g_pGameClientShell;
extern HWND					g_hMainWnd;			// GameClientShell.cpp

namespace
{
	enum EState
	{
		STATE_NONE,			// no prompt for this load
		STATE_WANTED,		// a prompt once the level has loaded
		STATE_WAITING		// the prompt is up
	};

	// NOLF2's CScreenPostload: the text flashes, its colour and the
	// inverse in turn, every third of a second.
	const float	c_fFlashTime	= 0.333f;

	// The prompt's place on the loading screen's 640x480 page: centred,
	// under the level's name (LAYOUT.txt LoadStringPos, 320,420).
	const int	c_nPromptX		= 320;
	const int	c_nPromptY		= 450;

	VarTrack	s_vtLoadScreenWait;
	EState		s_eState;
	HSTRING		s_hPrompt;
	LTBOOL		s_bFlash;
	float		s_fNextFlash;
	LTBOOL		s_bSeenUp;		// every key and button was up since the prompt came
	LTBOOL		s_bPressed;		// then one went down: go on when all are up again

	void SendToServer(uint8 nMsg)
	{
		HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(nMsg);
		g_pLTClient->EndMessage(hMessage);
	}

	// A key or the left mouse button held, or pressed since the last
	// call; a controller button held.  Not the right, middle or extra
	// mouse buttons: NOLF2's screen takes the left button only.
	LTBOOL AnyInputDown()
	{
		LTBOOL bDown = LTFALSE;

		if (GetForegroundWindow() != g_hMainWnd)
			return LTFALSE;

		for (int nKey = 1; nKey < 0xFF; nKey++)
		{
			if (nKey == VK_RBUTTON || nKey == VK_CANCEL || nKey == VK_MBUTTON ||
				nKey == 0x05 || nKey == 0x06)		// VK_XBUTTON1, VK_XBUTTON2
				continue;

			// (every key is read: the bit "pressed since the last call"
			// is cleared for each)
			if (GetAsyncKeyState(nKey) & 0x8001)
				bDown = LTTRUE;
		}

		if (P7Pad_AnyButtonDown())
			bDown = LTTRUE;

		return bDown;
	}

	void StartPrompt(CLoadingScreen & screen)
	{
		// The loading screen's thread stops; from now on this draws it.
		screen.Pause();

		if (!s_hPrompt)
			s_hPrompt = g_pLTClient->FormatString(IDS_P3_PRESS_ANY_KEY);

		// The game does not run while the prompt waits (NOLF2's server
		// stays paused until the client has gone on from the screen).
		SendToServer(MID_GAME_PAUSE);

		// The level is loaded: M-P2-12's is-loading flag goes to 0.
		P2_OnLoadingDone();

		s_bFlash     = LTFALSE;
		s_fNextFlash = CWinUtil::GetTime() + c_fFlashTime;
		s_bSeenUp    = LTFALSE;
		s_bPressed   = LTFALSE;
		AnyInputDown();		// forget what was pressed while it loaded

		s_eState = STATE_WAITING;
	}

	void EndPrompt()
	{
		SendToServer(MID_GAME_UNPAUSE);

		// The key that went on is not a command in play.
		g_pLTClient->ClearInput();

		s_eState = STATE_NONE;
	}
}

void P3LoadPrompt_OnLoadWorld(LTBOOL bNewMissionLevel)
{
	if (!s_vtLoadScreenWait.IsInitted())
		s_vtLoadScreenWait.Init(g_pLTClient, "LoadScreenWait", LTNULL, 1.0f);

	// A load that starts while a prompt waits (it cannot, but a failed
	// load can leave one): the server it paused is gone.
	s_eState = STATE_NONE;

	if (bNewMissionLevel && s_vtLoadScreenWait.GetFloat() != 0.0f)
		s_eState = STATE_WANTED;
}

LTBOOL P3LoadPrompt_Wait(CLoadingScreen & screen)
{
	switch (s_eState)
	{
		case STATE_WANTED:
		{
			if (g_pGameClientShell->IsMultiplayerGame())
			{
				s_eState = STATE_NONE;
				return LTFALSE;
			}

			StartPrompt(screen);
			screen.Update();
			return LTTRUE;
		}

		case STATE_WAITING:
		{
			if (AnyInputDown())
			{
				if (s_bSeenUp)
					s_bPressed = LTTRUE;
			}
			else
			{
				s_bSeenUp = LTTRUE;

				// Go on when the key is let go, so that it does not go on
				// into play (Space would jump, the left button fire).
				if (s_bPressed)
				{
					EndPrompt();
					return LTFALSE;
				}
			}

			screen.Update();
			return LTTRUE;
		}

		default:
			return LTFALSE;
	}
}

LTBOOL P3LoadPrompt_IsWaiting()
{
	return s_eState == STATE_WAITING;
}

void P3LoadPrompt_Draw(HSURFACE hDest)
{
	if (s_eState != STATE_WAITING || !s_hPrompt)
		return;

	float fTime = CWinUtil::GetTime();
	if (fTime > s_fNextFlash)
	{
		s_bFlash     = !s_bFlash;
		s_fNextFlash = fTime + c_fFlashTime;
	}

	HLTCOLOR hColor  = s_bFlash ? kBlack : kWhite;
	HLTCOLOR hShadow = s_bFlash ? kWhite : kBlack;

	// The menus' medium font: smaller than the level's name above it, as
	// NOLF2's prompt (size 18) is smaller than its title (22).
	CLTGUIFont *pFont = g_pInterfaceResMgr->GetMediumFont();
	if (!pFont)
		return;

	int x = P3_MenuX(c_nPromptX);
	int y = P3_MenuY(c_nPromptY);

	pFont->Draw(s_hPrompt, hDest, x + 1, y + 1, LTF_JUSTIFY_CENTER, hShadow);
	pFont->Draw(s_hPrompt, hDest, x, y, LTF_JUSTIFY_CENTER, hColor);
}
