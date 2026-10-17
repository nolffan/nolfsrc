// ----------------------------------------------------------------------- //
//
// MODULE  : p2_console.cpp
//
// PURPOSE : M-P2-01..06, the drop-down developer console (NOLFSRC_MODERN only)
//
// ----------------------------------------------------------------------- //
//
// The design follows NOLF Modernizer's console (credit for the idea and the
// behaviour is the Modernizer's: MODERN.md, P2); the code is ours, written
// for VC6 without containers: fixed rings of lines and of recalled commands.
//
//   M-P2-01  the console itself: drawing, edit line, recall, scroll-back,
//            pausing the game, swallowing input while open
//   M-P2-02  Enter runs the line: a cheat code as a cheat, otherwise as a
//            client console command, and (single player / host) on the
//            server as well, through MID_CONSOLE_COMMAND_CLIENT (M-P2-08)
//   M-P2-03  the rebindable ToggleConsole action (command 80), locks
//   M-P2-04  "Help": the engine's own list of console commands
//   M-P2-05  ConsoleBackdrop: 0/1 the retail menu's blank tag, 2 black
//   M-P2-06  "ConsoleHistoryToLog": the scroll-back into the modern log

#include "stdafx.h"
#include <stdlib.h>
#include "GameClientShell.h"
#include "InterfaceMgr.h"
#include "InterfaceResMgr.h"
#include "ClientUtilities.h"
#include "ClientSoundMgr.h"
#include "MessageMgr.h"
#include "CommandIDs.h"
#include "MsgIDs.h"
#include "VarTrack.h"
#include "cshell/p2_console.h"
#include "cshell/p2_log.h"
#include "cshell/p3_ttf.h"

extern CGameClientShell* g_pGameClientShell;
extern CCheatMgr*        g_pCheatMgr;

// VC6's winuser.h predates the name; it is the US layout's ` ~ key.
#ifndef VK_OEM_3
#define VK_OEM_3	0xC0
#endif

// DirectInput's scan code for that key: the default binding, "##41".
#define P2CONSOLE_DEFAULT_TRIGGER	"##41"

#define P2CON_MAX_LINES		1024	// scroll-back, in lines
#define P2CON_LINE_LEN		192		// characters per line kept
#define P2CON_MAX_RECALL	32		// commands Up/Down can recall
#define P2CON_EDIT_LEN		256		// the edit line

#define P2CON_COLOR_ECHO	SETRGB(160, 200, 255)
#define P2CON_COLOR_EDIT	SETRGB(255, 255, 255)

namespace
{
	struct P2ConLine
	{
		uint32	nColor;
		char	szText[P2CON_LINE_LEN];
	};

	// The scroll-back: a ring, s_nLineCount lines ending at s_nLineHead - 1.
	// The engine can print from any thread, so it is guarded.
	P2ConLine			s_Lines[P2CON_MAX_LINES];
	int					s_nLineHead = 0;
	int					s_nLineCount = 0;
	CRITICAL_SECTION	s_LinesCS;
	LTBOOL				s_bCSInit = LTFALSE;

	// Commands Up/Down recall: a ring as well.
	char				s_Recall[P2CON_MAX_RECALL][P2CON_EDIT_LEN];
	int					s_nRecallHead = 0;
	int					s_nRecallCount = 0;
	int					s_nRecallPos = -1;		// -1: not recalling

	char				s_szEdit[P2CON_EDIT_LEN];
	int					s_nEditLen = 0;

	LTBOOL				s_bInit = LTFALSE;
	LTBOOL				s_bVisible = LTFALSE;
	LTBOOL				s_bWePaused = LTFALSE;	// the console paused the game
	uint32				s_nLock = 0;
	int					s_nScroll = 0;			// lines scrolled back from the newest
	int					s_nVisibleLines = 1;	// lines the last frame had room for

	HSURFACE			s_hBackdrop = LTNULL;	// the blank tag, made on first use
	HSURFACE			s_hBlack = LTNULL;

	VarTrack			s_vtConsoleBackdrop;	// M-P2-05

	// For "Help": the commands, gathered by the engine's enumerator.
	#define P2CON_MAX_HELP	512
	const char*			s_HelpNames[P2CON_MAX_HELP];
	uint32				s_HelpFlags[P2CON_MAX_HELP];
	int					s_nHelp = 0;
}


// ----------------------------------------------------------------------- //
//  The scroll-back
// ----------------------------------------------------------------------- //

static void AddLine(uint32 nColor, const char* pText, int nLen)
{
	P2ConLine* pLine;

	if (nLen >= P2CON_LINE_LEN)
		nLen = P2CON_LINE_LEN - 1;

	pLine = &s_Lines[s_nLineHead];
	pLine->nColor = nColor;
	memcpy(pLine->szText, pText, nLen);
	pLine->szText[nLen] = '\0';

	s_nLineHead = (s_nLineHead + 1) % P2CON_MAX_LINES;
	if (s_nLineCount < P2CON_MAX_LINES)
		s_nLineCount++;

	// Someone reading back stays on the line they were reading.
	if (s_nScroll > 0 && s_nScroll < s_nLineCount - 1)
		s_nScroll++;
}

// M-P0-02: the engine hands us every console line (ILTClient::
// SetConsoleOutputFn). Inside the engine console's lock: never print here.
static void OnEngineConsoleLine(void* pUser, uint32 nColor, int nLevel, const char* pText)
{
	const char* pStart;
	const char* pEnd;

	if (!pText || !s_bCSInit) return;

	// A colour of 0 means "the font's own colours" to CLTGUIFont::Draw:
	// draw those lines white, like the engine's own default.
	nColor &= 0x00ffffff;
	if (!nColor)
		nColor = SETRGB(255, 255, 255);

	EnterCriticalSection(&s_LinesCS);

	// One console line per '\n'-separated piece, as the engine's own
	// console stores them (a trailing '\n' does not make an empty line).
	pStart = pText;
	for (;;)
	{
		pEnd = strchr(pStart, '\n');
		if (!pEnd)
		{
			if (*pStart || pStart == pText)
				AddLine(nColor, pStart, strlen(pStart));
			break;
		}

		AddLine(nColor, pStart, pEnd - pStart);
		pStart = pEnd + 1;
		if (!*pStart)
			break;
	}

	LeaveCriticalSection(&s_LinesCS);
}


// ----------------------------------------------------------------------- //
//  Console programs: Help (M-P2-04), ConsoleHistoryToLog (M-P2-06)
// ----------------------------------------------------------------------- //

static void GatherHelpName(void* pUser, const char* pName, uint32 nFlags)
{
	if (s_nHelp < P2CON_MAX_HELP)
	{
		s_HelpNames[s_nHelp] = pName;
		s_HelpFlags[s_nHelp] = nFlags;
		s_nHelp++;
	}
}

static int CompareHelpNames(const void* p1, const void* p2)
{
	return stricmp(s_HelpNames[*(const int*)p1], s_HelpNames[*(const int*)p2]);
}

// Lists one group (game programs or engine commands), sorted, several names
// to a line.
static void PrintHelpGroup(const char* pTitle, uint32 nWantFlags)
{
	int  anOrder[P2CON_MAX_HELP];
	int  nCount = 0;
	int  i;
	char szLine[128];
	int  nLineLen;

	for (i = 0; i < s_nHelp; i++)
	{
		if ((s_HelpFlags[i] & 1) == nWantFlags)
			anOrder[nCount++] = i;
	}

	qsort(anOrder, nCount, sizeof(int), CompareHelpNames);

	g_pLTClient->CPrint("%s (%d):", pTitle, nCount);

	szLine[0] = '\0';
	nLineLen = 0;
	for (i = 0; i < nCount; i++)
	{
		const char* pName = s_HelpNames[anOrder[i]];
		int nLen = strlen(pName);

		if (nLineLen && nLineLen + 2 + nLen > 72)
		{
			g_pLTClient->CPrint("  %s", szLine);
			szLine[0] = '\0';
			nLineLen = 0;
		}

		if (nLen > (int)sizeof(szLine) - 3)
			continue;

		if (nLineLen)
		{
			strcat(szLine, ", ");
			nLineLen += 2;
		}
		strcat(szLine, pName);
		nLineLen += nLen;
	}

	if (nLineLen)
		g_pLTClient->CPrint("  %s", szLine);
}

static void HelpFn(int argc, char** argv)
{
	s_nHelp = 0;
	g_pLTClient->EnumConsoleCommands(GatherHelpName, LTNULL);

	PrintHelpGroup("Game commands", 1);
	PrintHelpGroup("Engine commands", 0);
	g_pLTClient->CPrint("Typing a console variable's name and a value sets it; \"Cheats\" lists the cheat codes.");
}

// GetPos: where the player is, as SetPos takes it.
static void GetPosFn(int argc, char** argv)
{
	HOBJECT hPlayer = g_pGameClientShell->GetMoveMgr()->GetObject();
	LTVector vPos;

	if (!hPlayer)
	{
		g_pLTClient->CPrint("GetPos: no player");
		return;
	}

	g_pLTClient->GetObjectPos(hPlayer, &vPos);
	g_pLTClient->CPrint("Player at %.1f %.1f %.1f", vPos.x, vPos.y, vPos.z);
	ModernLog("GetPos: %.1f %.1f %.1f", vPos.x, vPos.y, vPos.z);
}

// SetPos <x> <y> <z>: put the player there (single player: the client
// moves its own object and the server follows), for testing a place
// without playing to it; GetPos (or mppos) shows where one is.
static void SetPosFn(int argc, char** argv)
{
	HOBJECT hPlayer = g_pGameClientShell->GetMoveMgr()->GetObject();

	if (argc < 3 || !hPlayer || g_pGameClientShell->IsMultiplayerGame())
	{
		g_pLTClient->CPrint("SetPos <x> <y> <z>  (single player)");
		return;
	}

	LTVector vPos((LTFLOAT)atof(argv[0]), (LTFLOAT)atof(argv[1]), (LTFLOAT)atof(argv[2]));
	LTVector vZero(0.0f, 0.0f, 0.0f);
	g_pLTClient->SetObjectPos(hPlayer, &vPos);
	g_pPhysicsLT->SetVelocity(hPlayer, &vZero);
	g_pLTClient->CPrint("Player at %.1f %.1f %.1f", vPos.x, vPos.y, vPos.z);
}

static void HistoryToLogFn(int argc, char** argv)
{
	int i, nFirst;

	EnterCriticalSection(&s_LinesCS);

	ModernLog("---- console history (%d lines) ----", s_nLineCount);
	nFirst = (s_nLineHead - s_nLineCount + P2CON_MAX_LINES) % P2CON_MAX_LINES;
	for (i = 0; i < s_nLineCount; i++)
	{
		ModernLog("%s", s_Lines[(nFirst + i) % P2CON_MAX_LINES].szText);
	}
	ModernLog("---- end of console history ----");

	LeaveCriticalSection(&s_LinesCS);

	g_pLTClient->CPrint("Console history written to %s.", MODERN_LOG_FILE);
}

// M-P2-09: the cheat list lives with the cheats (p2_cheats.cpp).
void P2Cheats_ListFn(int argc, char** argv);


// ----------------------------------------------------------------------- //
//  Init / Term
// ----------------------------------------------------------------------- //

// M-P2-03: make sure the action exists, and give it its default key (the
// one left of 1, DirectInput's "##41") when no keyboard key is bound to it.
// A fresh defctrls.cfg (M-P4-06) does the same; this covers configs written
// before it, and the playtest's retail defaults.cfg. Binding replaces what
// that key had, so it happens only while the action has no key at all:
// whoever moves it elsewhere keeps their choice.
static void EnsureConsoleAction()
{
	DeviceBinding* pBindings;
	DeviceBinding* pBinding;
	GameAction*    pAction;
	LTBOOL         bBound = LTFALSE;
	char           szCmd[128];

	sprintf(szCmd, "AddAction ToggleConsole %d", COMMAND_ID_TOGGLE_CONSOLE);
	g_pLTClient->RunConsoleString(szCmd);

	pBindings = g_pLTClient->GetDeviceBindings(DEVICETYPE_KEYBOARD);
	for (pBinding = pBindings; pBinding && !bBound; pBinding = pBinding->pNext)
	{
		for (pAction = pBinding->pActionHead; pAction; pAction = pAction->pNext)
		{
			if (pAction->nActionCode == COMMAND_ID_TOGGLE_CONSOLE)
				bBound = LTTRUE;
		}
	}
	if (pBindings)
		g_pLTClient->FreeDeviceBindings(pBindings);

	if (!bBound)
	{
		sprintf(szCmd, "rangebind \"##keyboard\" \"%s\" 0 0 \"ToggleConsole\"",
		        P2CONSOLE_DEFAULT_TRIGGER);
		g_pLTClient->RunConsoleString(szCmd);
	}

	GetCommandKeyStr(COMMAND_ID_TOGGLE_CONSOLE, szCmd, sizeof(szCmd));
	ModernLog("Console key: \"%s\" (virtual key %d)%s", szCmd,
	          GetCommandKey(COMMAND_ID_TOGGLE_CONSOLE), bBound ? "" : ", the default");
}

void P2Console_Init()
{
	if (s_bInit) return;

	if (!s_bCSInit)
	{
		InitializeCriticalSection(&s_LinesCS);
		s_bCSInit = LTTRUE;
	}

	s_vtConsoleBackdrop.Init(g_pLTClient, "ConsoleBackdrop", LTNULL, 0.0f);

	EnsureConsoleAction();

	g_pLTClient->RegisterConsoleProgram("Help", HelpFn);
	g_pLTClient->RegisterConsoleProgram("Cheats", P2Cheats_ListFn);
	g_pLTClient->RegisterConsoleProgram("ConsoleHistoryToLog", HistoryToLogFn);
	g_pLTClient->RegisterConsoleProgram("SetPos", SetPosFn);
	g_pLTClient->RegisterConsoleProgram("GetPos", GetPosFn);

	g_pLTClient->SetConsoleOutputFn(OnEngineConsoleLine, LTNULL);

	s_bInit = LTTRUE;

	g_pLTClient->CPrint("Developer console ready: \"Help\" lists the commands.");
}

void P2Console_Term()
{
	if (!s_bInit) return;

	// The engine outlives this DLL: it must not call into it afterwards.
	g_pLTClient->SetConsoleOutputFn(LTNULL, LTNULL);

	g_pLTClient->UnregisterConsoleProgram("Help");
	g_pLTClient->UnregisterConsoleProgram("Cheats");
	g_pLTClient->UnregisterConsoleProgram("ConsoleHistoryToLog");
	g_pLTClient->UnregisterConsoleProgram("SetPos");
	g_pLTClient->UnregisterConsoleProgram("GetPos");

	if (s_hBackdrop)
	{
		g_pLTClient->DeleteSurface(s_hBackdrop);
		s_hBackdrop = LTNULL;
	}
	if (s_hBlack)
	{
		g_pLTClient->DeleteSurface(s_hBlack);
		s_hBlack = LTNULL;
	}

	s_bVisible = LTFALSE;
	s_bInit = LTFALSE;
}


// ----------------------------------------------------------------------- //
//  Opening and closing
// ----------------------------------------------------------------------- //

static LTBOOL CanOpen()
{
	if (!s_bInit || s_nLock) return LTFALSE;
	if (!g_pGameClientShell || !g_pInterfaceMgr || !g_pInterfaceResMgr) return LTFALSE;

	// Single player only: in multiplayer the game cannot pause, and the
	// console's server half (M-P2-08) belongs to the host alone.
	if (g_pGameClientShell->IsMultiplayerGame()) return LTFALSE;

	if (g_pInterfaceMgr->GetGameState() == GS_LOADINGLEVEL) return LTFALSE;

	return g_pInterfaceResMgr->GetSmallFont() != LTNULL;
}

static void ClearEdit()
{
	s_szEdit[0] = '\0';
	s_nEditLen = 0;
	s_nRecallPos = -1;
}

static void Open()
{
	if (s_bVisible || !CanOpen()) return;

	ClearEdit();
	s_nScroll = 0;
	s_bVisible = LTTRUE;

	// Pause a running game; leave one that is already paused (a menu, the
	// pause screen) to whoever paused it.
	s_bWePaused = LTFALSE;
	if (g_pInterfaceMgr->GetGameState() == GS_PLAYING && !g_pGameClientShell->IsGamePaused())
	{
		g_pGameClientShell->PauseGame(LTTRUE, LTTRUE);
		s_bWePaused = LTTRUE;
	}
}

static void Close()
{
	if (!s_bVisible) return;

	s_bVisible = LTFALSE;
	ClearEdit();

	if (s_bWePaused)
	{
		s_bWePaused = LTFALSE;
		if (g_pGameClientShell && g_pGameClientShell->IsGamePaused())
			g_pGameClientShell->PauseGame(LTFALSE);
	}
}

LTBOOL P2Console_IsVisible()
{
	return s_bVisible;
}

void P2Console_SetLock(uint32 nReason, LTBOOL bLock)
{
	if (bLock)
	{
		s_nLock |= nReason;
		Close();
	}
	else
	{
		s_nLock &= ~nReason;
	}
}


// ----------------------------------------------------------------------- //
//  Running a line (M-P2-02)
// ----------------------------------------------------------------------- //

static void Remember(const char* pLine)
{
	int nLast = (s_nRecallHead - 1 + P2CON_MAX_RECALL) % P2CON_MAX_RECALL;

	// Not twice in a row.
	if (s_nRecallCount && strcmp(s_Recall[nLast], pLine) == 0)
		return;

	strncpy(s_Recall[s_nRecallHead], pLine, P2CON_EDIT_LEN - 1);
	s_Recall[s_nRecallHead][P2CON_EDIT_LEN - 1] = '\0';
	s_nRecallHead = (s_nRecallHead + 1) % P2CON_MAX_RECALL;
	if (s_nRecallCount < P2CON_MAX_RECALL)
		s_nRecallCount++;
}

static void Submit()
{
	char szLine[P2CON_EDIT_LEN];
	char* pLine;

	strcpy(szLine, s_szEdit);
	ClearEdit();
	s_nScroll = 0;

	// Leading blanks are not part of the command.
	for (pLine = szLine; *pLine == ' '; pLine++)
		;
	if (!*pLine)
		return;

	Remember(pLine);

	g_pLTClient->CPrint("> %s", pLine);

	// A cheat code: run it as one, with the sound the chat box plays.
	if (g_pCheatMgr && g_pCheatMgr->Check(pLine))
	{
		if (g_pClientSoundMgr)
			g_pClientSoundMgr->PlayInterfaceSound("Menu\\Snd\\Cheat.wav");
		return;
	}

	// Always on the client: client variables and programs.
	g_pLTClient->RunConsoleString(pLine);

	// Single player, or the host of a game: on the server too, so server
	// variables (BigHeadMode, DisplayTriggers, ...) can be set. The server
	// runs it only for a local client (M-P2-08).
	if (g_pGameClientShell && g_pLTClient->IsConnected() &&
	    (!g_pGameClientShell->IsMultiplayerGame() || g_pGameClientShell->IsHosting()))
	{
		HSTRING hCmd = g_pLTClient->CreateString(pLine);
		if (hCmd)
		{
			HMESSAGEWRITE hMsg = g_pLTClient->StartMessage(MID_CONSOLE_COMMAND_CLIENT);
			g_pLTClient->WriteToMessageHString(hMsg, hCmd);
			g_pLTClient->EndMessage(hMsg);
			g_pLTClient->FreeString(hCmd);
		}
	}
}

static void Recall(int nDir)
{
	int nPos;

	if (!s_nRecallCount) return;

	// s_nRecallPos counts back from the newest: 0 is the last command.
	nPos = s_nRecallPos + nDir;
	if (nPos >= s_nRecallCount) nPos = s_nRecallCount - 1;

	if (nPos < 0)
	{
		ClearEdit();
		return;
	}

	s_nRecallPos = nPos;
	strcpy(s_szEdit, s_Recall[(s_nRecallHead - 1 - nPos + 2 * P2CON_MAX_RECALL) % P2CON_MAX_RECALL]);
	s_nEditLen = strlen(s_szEdit);
}

static void Scroll(int nLines)
{
	int nMax = s_nLineCount - s_nVisibleLines;
	if (nMax < 0) nMax = 0;

	s_nScroll += nLines;
	if (s_nScroll > nMax) s_nScroll = nMax;
	if (s_nScroll < 0) s_nScroll = 0;
}


// ----------------------------------------------------------------------- //
//  Input
// ----------------------------------------------------------------------- //

// The virtual key the ToggleConsole action is bound to, or -1.
static int ConsoleKey()
{
	return GetCommandKey(COMMAND_ID_TOGGLE_CONSOLE);
}

LTBOOL P2Console_OnKeyDown(int nKey, int nRep)
{
	int nConsoleKey;

	if (!s_bInit) return LTFALSE;

	nConsoleKey = ConsoleKey();

	if (!s_bVisible)
	{
		// M-P2-15: Shift+Esc opens it too, whatever the keyboard layout
		// puts where the console key is.
		if (((nKey == nConsoleKey && nKey > 0) ||
		     (nKey == VK_ESCAPE && (GetKeyState(VK_SHIFT) & 0x8000))) && CanOpen())
		{
			Open();
			return LTTRUE;
		}
		return LTFALSE;
	}

	// Open: every key is the console's.
	if (nKey == VK_ESCAPE || (nKey == nConsoleKey && nKey > 0))
	{
		Close();
		return LTTRUE;
	}

	switch (nKey)
	{
		case VK_RETURN:	Submit();							break;
		case VK_BACK:
			if (s_nEditLen > 0) s_szEdit[--s_nEditLen] = '\0';
			break;
		case VK_UP:		Recall(1);							break;
		case VK_DOWN:	Recall(-1);							break;
		case VK_PRIOR:	Scroll(s_nVisibleLines - 1);		break;
		case VK_NEXT:	Scroll(-(s_nVisibleLines - 1));		break;
		case VK_HOME:	Scroll(s_nLineCount);				break;
		case VK_END:	Scroll(-s_nLineCount);				break;
		default:											break;
	}

	return LTTRUE;
}

LTBOOL P2Console_OnChar(char c)
{
	int nConsoleKey;

	if (!s_bVisible) return LTFALSE;

	// The key that opened the console types its character too: drop it.
	nConsoleKey = ConsoleKey();
	if (nConsoleKey > 0)
	{
		UINT nChar = MapVirtualKey(nConsoleKey, 2) & 0x7fff;
		if (nChar && (char)nChar == c) return LTTRUE;
		if (nConsoleKey == VK_OEM_3 && (c == '`' || c == '~')) return LTTRUE;
	}

	if ((unsigned char)c >= ' ' && (unsigned char)c < 127 && s_nEditLen < P2CON_EDIT_LEN - 1)
	{
		s_szEdit[s_nEditLen++] = c;
		s_szEdit[s_nEditLen] = '\0';
		s_nRecallPos = -1;
	}

	return LTTRUE;
}

LTBOOL P2Console_OnCommandOn(int nCommand)
{
	if (s_bVisible) return LTTRUE;		// no game command while it is open

	// A key GetCommandKey cannot name (one not in its key-name table) still
	// reaches the action: open the console from the command then.
	if (nCommand == COMMAND_ID_TOGGLE_CONSOLE && ConsoleKey() < 0)
	{
		Open();
		return LTTRUE;
	}

	return LTFALSE;
}


// ----------------------------------------------------------------------- //
//  Drawing
// ----------------------------------------------------------------------- //

static HSURFACE GetBackdrop()
{
	// M-P2-05: 0 and 1 the retail menus' blank tag (0 was the NOLF demo's
	// console art in the Modernizer, which is not on the retail discs), 2
	// plain black.
	if ((int)s_vtConsoleBackdrop.GetFloat() == 2)
	{
		if (!s_hBlack)
		{
			s_hBlack = g_pLTClient->CreateSurface(8, 8);
			if (s_hBlack)
				g_pLTClient->FillRect(s_hBlack, LTNULL, SETRGB(0, 0, 0));
		}
		return s_hBlack;
	}

	if (!s_hBackdrop)
		s_hBackdrop = g_pLTClient->CreateSurfaceFromBitmap("menu\\art\\blanktag.pcx");

	return s_hBackdrop;
}

void P2Console_Draw()
{
	HSURFACE    hScreen;
	HSURFACE    hBack;
	CLTGUIFont* pFont;
	uint32      nWidth, nHeight;
	int         nConHeight, nLineHeight, nY, i;
	char        szEdit[P2CON_EDIT_LEN + 4];
	LTRect      rcCon;

	if (!s_bVisible) return;

	hScreen = g_pLTClient->GetScreenSurface();
	g_pLTClient->GetSurfaceDims(hScreen, &nWidth, &nHeight);

	// M-P3-25: Noto Sans Mono in English (the menus' small font, Bowlby One
	// SC since M-P3-24, is all capitals and wide); the small font otherwise
	pFont = (g_pInterfaceResMgr && g_pInterfaceResMgr->IsEnglish()) ? P3_TTF_ConsoleFont((int)nHeight) : LTNULL;
	if (!pFont)
		pFont = g_pInterfaceResMgr ? g_pInterfaceResMgr->GetSmallFont() : LTNULL;
	if (!pFont) return;

	nConHeight = nHeight / 2;
	nLineHeight = pFont->GetHeight() + 2;
	if (nLineHeight < 4) nLineHeight = 4;

	g_pLTClient->Start3D();
	g_pLTClient->StartOptimized2D();

	rcCon.left = 0;
	rcCon.top = 0;
	rcCon.right = nWidth;
	rcCon.bottom = nConHeight;

	hBack = GetBackdrop();
	if (hBack)
		g_pLTClient->ScaleSurfaceToSurface(hScreen, hBack, &rcCon, LTNULL);

	// The edit line at the bottom, with a blinking caret.
	nY = nConHeight - nLineHeight - 2;
	sprintf(szEdit, "> %s%s", s_szEdit, ((GetTickCount() / 400) & 1) ? "_" : "");
	pFont->Draw(szEdit, hScreen, 8, nY, LTF_JUSTIFY_LEFT, P2CON_COLOR_EDIT);

	// The scroll-back above it, newest last.
	s_nVisibleLines = (nY - 4) / nLineHeight;
	if (s_nVisibleLines < 1) s_nVisibleLines = 1;

	EnterCriticalSection(&s_LinesCS);

	if (s_nScroll > s_nLineCount - 1) s_nScroll = 0;

	for (i = 0; i < s_nVisibleLines; i++)
	{
		int nBack = s_nScroll + i;		// 0: the newest line
		P2ConLine* pLine;

		if (nBack >= s_nLineCount) break;

		pLine = &s_Lines[(s_nLineHead - 1 - nBack + 2 * P2CON_MAX_LINES) % P2CON_MAX_LINES];
		nY -= nLineHeight;
		pFont->Draw(pLine->szText, hScreen, 8, nY, LTF_JUSTIFY_LEFT, pLine->nColor);
	}

	LeaveCriticalSection(&s_LinesCS);

	// Scrolled back: say so where the newest line would be.
	if (s_nScroll > 0)
	{
		char szMore[48];
		sprintf(szMore, "-- %d more below (Page Down) --", s_nScroll);
		pFont->Draw(szMore, hScreen, nWidth - 8, nConHeight - nLineHeight - 2,
		            LTF_JUSTIFY_RIGHT, P2CON_COLOR_ECHO);
	}

	g_pLTClient->EndOptimized2D();
	g_pLTClient->End3D();
}
