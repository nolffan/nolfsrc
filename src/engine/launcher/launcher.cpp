// *********************************************************************** //
//
//	MODULE:		LAUNCHER (.CPP)
//
//	PURPOSE:	NOLF.exe - the No One Lives Forever retail launcher.
//				Reconstructed from the shipped v1.003 binary; a Win32
//				dialog application over a static-MFC runtime, per-function
//				verified against the shipped binary.
//
// *********************************************************************** //

#include "stdafx.h"
#include "stdio.h"
#include "mmsystem.h"
#include "regmgr.h"
#include "dibmgr.h"
#include "RezFind.h"
#include "Utils.h"
#include "launcher.h"
#ifdef NOLFSRC_MODERN
#include "../../modern/launcher/mod_startup.h"
#endif


// Globals...

CWinApp		theApp("Launcher");

CString		g_csCommands;			// 0x4284
CString		g_csCaption;			// 0x43b4 - IDS 9
CString		g_csWindowTitle;		// 0x4410 - IDS 57
CString		g_csGameName;			// 0x43fc - IDS 58
CString		g_csRez1;				// 0x4288 - IDS 59 "\DATA\NOLF.REZ"
CString		g_csRez2;				// 0x4268 - IDS 60 "\DATA\NOLF2.REZ"
CString		g_csRezBase;			// 0x4390 - IDS 62 "NOLF"
CString		g_csSetupExe;			// 0x438c - IDS 61 "SETUP.EXE"
CString		g_csNumGames;			// 0x4264 - IDS 63 "1"
CString		g_csLanguage;			// 0x4280 - IDS 84 "English"
CString		g_csUpdateCmdLine;		// 0x43b8
CString		g_csContentCmdLine;		// 0x4260

HINSTANCE	g_hInstApp;				// 0x41f8
HINSTANCE	g_hInst;				// 0x4414

CRegMgr*	g_pRegMgr;				// 0x4424

int			g_bDisplayWarned;		// 0x4448
int			g_nNumRuns;				// 0x444c
int			g_bDisableSound;		// 0x4458
int			g_bDisableMusic;		// 0x445c
int			g_bDisableMovies;		// 0x4460
int			g_bDisableFog;			// 0x4464
int			g_bDisableJoystick;		// 0x4468
int			g_bDisableTripBuf;		// 0x446c
int			g_bDisableCursor;		// 0x4470
int			g_bDisableLoadscreen;	// 0x4474

int			s_nLastRandom = 1;		// 0x308 (initialized data)

char		g_sDir[256];			// 0x428c
char		g_sCdRomPath[64];		// 0x43bc
char		g_sGame[16];			// 0x4394

HCURSOR		g_hCursorHand;			// 0x4434
HCURSOR		g_hCursorClick;			// 0x4438
HCURSOR		g_hCursorArrow;			// 0x443c

char		g_chSomething;			// 0x4188

CDibMgr*	g_pDibMgr;				// 0x4418
CDib*		g_pDib;					// 0x4420
CDibPal*	g_pPal;					// 0x441c
CPreviewList*	g_pMovies;			// 0x4428
CPreviewList*	g_pDemos;			// 0x442c
CDisplayModes*	g_pDisplayModes;	// 0x4430


// *********************************************************************** //
//
//	ROUTINE:	ErrorMsg
//
//	PURPOSE:	Displays an error message loaded from the string table
//
// *********************************************************************** //

int ErrorMsg(HWND hWnd, int nStringID, const char* sCaption, UINT nType)
{
	char sText[512];
	strcpy(sText, "Error.");

	LoadString(g_hInst, nStringID, sText, 511);

	return(MessageBox(hWnd, sText, sCaption, nType));
}

int ErrorMsg(HWND hWnd, int nStringID, UINT nType)
{
	return(ErrorMsg(hWnd, nStringID, g_csCaption, nType));
}


// *********************************************************************** //
//
//	ROUTINE:	WinMain
//
//	PURPOSE:	Application entry point
//
// *********************************************************************** //

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef NOLFSRC_MODERN
	if (NolfModStartup()) return 0;
#endif
	g_hInstApp = hInstance;

	AfxWinInit(hInstance, NULL, lpCmdLine, 0);


	// Make sure we aren't already running...

	char sModule[128];
	char sName[128];
	char sExt[32];
	char sExePath[128];

	GetModuleFileName(NULL, sModule, 125);
	_splitpath(sModule, NULL, NULL, sName, sExt);
	sprintf(sExePath, "%s%s", sName, sExt);

	if (ExistProcess(sExePath, 2, NULL))
	{
		return(FALSE);
	}

	g_hInst = hInstance;
	AfxGetModuleState()->m_hCurrentResourceHandle = hInstance;


	// Get the current directory...

	g_sDir[0] = '\0';
	GetCurrentDirectory(254, g_sDir);

	int nLen = strlen(g_sDir);
	if (nLen > 0)
	{
		if (g_sDir[nLen-1] == '\\') g_sDir[nLen-1] = '\0';
	}


	// Load the global strings...

	g_csCaption.LoadString(9);
	g_csGameName.LoadString(58);
	g_csRez1.LoadString(59);
	g_csRez2.LoadString(60);
	g_csRezBase.LoadString(62);
	g_csSetupExe.LoadString(61);
	g_csNumGames.LoadString(63);
	g_csWindowTitle.LoadString(57);
	g_csLanguage.LoadString(84);


	// Make sure the game itself isn't running...

	char sLithTech[128] = { g_chSomething };

	GetCurrentDirectory(125, sLithTech);
	if (strlen(sLithTech) > 3) strcat(sLithTech, "\\");

	sprintf(sExePath, "%sLITHTECH.EXE", sLithTech);

	if (ExistProcess(sExePath, 1, NULL))
	{
		ErrorMsg(NULL, 64, g_csCaption, MB_ICONASTERISK);
		return(FALSE);
	}


	// Init the registry manager...

	g_pRegMgr = new CRegMgr;
	if (!g_pRegMgr->Init("Monolith Productions", g_csGameName, "1.0", NULL, HKEY_LOCAL_MACHINE, NULL))
	{
		return(FALSE);
	}


	// Find the CD-ROM drive...

	BOOL bOtherCd;
	char chCdRom = FindCdRom(&bOtherCd);
	sprintf(g_sCdRomPath, "%c:\\", chCdRom);


	// Read the settings from the registry...

	g_bDisplayWarned     = g_pRegMgr->Get("Display Warned", 0);
	g_nNumRuns           = g_pRegMgr->Get("Num Launcher Runs", 0);
	s_nLastRandom        = g_pRegMgr->Get("Last Random", s_nLastRandom);
	g_bDisableSound      = g_pRegMgr->Get("Disable Sound", 0);
	g_bDisableMusic      = g_pRegMgr->Get("Disable Music", 0);
	g_bDisableMovies     = g_pRegMgr->Get("Disable Movies", 0);
	g_bDisableFog        = g_pRegMgr->Get("Disable Fog", 0);
	g_bDisableJoystick   = g_pRegMgr->Get("Disable Joystick", 0);
	g_bDisableTripBuf    = g_pRegMgr->Get("Disable TripBuf", 0);
	g_bDisableCursor     = g_pRegMgr->Get("Disable Cursor", 0);
	g_bDisableLoadscreen = g_pRegMgr->Get("Disable Loadscreen", 0);

	char sBuf[256];
	UINT dwSize = 256;
	g_csUpdateCmdLine = g_pRegMgr->Get("Update Command Line", sBuf, dwSize, NULL);
	dwSize = 256;
	g_csContentCmdLine = g_pRegMgr->Get("Content Command Line", sBuf, dwSize, NULL);
#ifdef NOLFSRC_MODERN
	// M-P1-16: the 1.003 update and the GOTY content, which the retail
	// installers named in the registry (and linux-run.sh does under
	// Wine), from the game directory when nothing names them: a copied or
	// unpacked install (a Steam Deck, a friend's machine) has no registry.
	if (g_csUpdateCmdLine.IsEmpty() && GetFileAttributes("nolfu003.rez") != (DWORD)-1)
		g_csUpdateCmdLine = "-rez nolfu003.rez";
	if (g_csContentCmdLine.IsEmpty() && GetFileAttributes("nolfgoty.rez") != (DWORD)-1)
		g_csContentCmdLine = "-rez nolfgoty.rez";
#endif

	LoadSoundSettings(g_pRegMgr);


	// Load the cursors...

	g_hCursorHand  = LoadCursor(g_hInst, "WEBHAND");
	g_hCursorClick = LoadCursor(g_hInst, "WEBCLICK");
	g_hCursorArrow = LoadCursor(NULL, IDC_ARROW);


	// Load the other settings...

	LoadCommands();
	LoadRezFiles();
	ParseAutoexec("autoexec.cfg");


	// Find the rez files...

	FindRezFiles(FindCdRom(&bOtherCd));


	// Check the command line...

	char sCmdLine[256];
	strcpy(sCmdLine, lpCmdLine);
	_strupr(sCmdLine);

	BOOL bGoMulti = FALSE;
	BOOL bGoGame = FALSE;

	if (strstr(sCmdLine, "-GOMULTI")) bGoMulti = TRUE;
	if (strstr(sCmdLine, "-GOGAME")) bGoGame = TRUE;
	if (strstr(sCmdLine, "CONNECT")) bGoMulti = TRUE;


	// Pick the game to display...

	int nNumGames = atoi(g_csNumGames);
	if (nNumGames <= 0) nNumGames = 1;

	strcpy(g_sGame, "GAME");

	srand(timeGetTime());

	if (nNumGames > 1)
	{
		int nGame = rand() % nNumGames;

		for (int i = 20; i != 0; i--)
		{
			if (nGame == s_nLastRandom) nGame = rand() % nNumGames;
		}

		sprintf(g_sGame, "GAME%i", nGame + 1);
		s_nLastRandom = nGame;
	}
	else
	{
		sprintf(g_sGame, "GAME%i", 1);
	}


	// Do the main dialog loop...

	while (1)
	{
		int nRet;

		if (bGoMulti)
		{
			nRet = 5;
		}
		else if (bGoGame)
		{
			nRet = 1;
		}
		else
		{
			nRet = DialogBoxParam(hInstance, "GAME", NULL, (DLGPROC)GameDlgProc, 0);
		}

		bGoGame = FALSE;
		bGoMulti = FALSE;

		switch (nRet)
		{
			case 5:
			{
				if (LaunchGame(lpCmdLine, TRUE)) goto Done;
				break;
			}

			case 2:
			{
				if (RunSetup()) goto Done;
				ErrorMsg(NULL, 4, g_csCaption, MB_ICONHAND);
				break;
			}

			case 1:
			{
				if (CheckCd(FALSE))
				{
					if (LaunchGame(lpCmdLine, FALSE)) goto Done;
				}
				break;
			}

			default:
			{
				goto Done;
			}
		}
	}

Done:


	// Save the settings to the registry...

	g_nNumRuns++;

	g_pRegMgr->Set("Display Warned", g_bDisplayWarned);
	g_pRegMgr->Set("Num Launcher Runs", g_nNumRuns);
	g_pRegMgr->Set("Last Random", s_nLastRandom);
	g_pRegMgr->Set("Disable Sound", g_bDisableSound);
	g_pRegMgr->Set("Disable Music", g_bDisableMusic);
	g_pRegMgr->Set("Disable Movies", g_bDisableMovies);
	g_pRegMgr->Set("Disable Fog", g_bDisableFog);
	g_pRegMgr->Set("Disable Joystick", g_bDisableJoystick);
	g_pRegMgr->Set("Disable TripBuf", g_bDisableTripBuf);
	g_pRegMgr->Set("Disable Cursor", g_bDisableCursor);
	g_pRegMgr->Set("Disable Loadscreen", g_bDisableLoadscreen);
	g_pRegMgr->Set("Language", g_csLanguage);


	// Clean up...

	if (g_pDibMgr)
	{
		delete g_pDibMgr;
		g_pDibMgr = NULL;
	}

	if (g_pRegMgr)
	{
		delete g_pRegMgr;
		g_pRegMgr = NULL;
	}

	if (g_pMovies)
	{
		delete g_pMovies;
		g_pMovies = NULL;
	}

	if (g_pDemos)
	{
		delete g_pDemos;
		g_pDemos = NULL;
	}

	if (g_pDisplayModes)
	{
		delete g_pDisplayModes;
		g_pDisplayModes = NULL;
	}

	g_pDib = NULL;
	g_pPal = NULL;

	return(TRUE);
}


// *********************************************************************** //
//
//	ROUTINE:	LaunchGame
//
//	PURPOSE:	Builds the full command line, writes nolfcmds.txt and
//				spawns lithtech.exe
//
// *********************************************************************** //

#ifdef NOLFSRC_MODERN
// Does the config name a whole display mode (a "bitdepth" line, which the
// game writes into autoexec.cfg on exit and the shipped defaults lack)?
static BOOL ModernConfigHasMode(const char* sFile)
{
	FILE* pFile = fopen(sFile, "rt");
	if (!pFile) return(FALSE);
	BOOL bFound = FALSE;
	char sLine[256];
	while (!bFound && fgets(sLine, sizeof(sLine), pFile))
	{
		char* p = sLine;
		while (*p == ' ' || *p == '\t' || *p == '"') p++;
		if (_strnicmp(p, "bitdepth", 8) == 0) bFound = TRUE;
	}
	fclose(pFile);
	return(bFound);
}

#endif
BOOL LaunchGame(char* sCmdLine, BOOL bMultiplayer)
{
	// Make sure the performance settings have been chosen...

	if (g_nNumRunsCfg <= 0 || g_nNumRuns <= 0 || g_bRestoreDefaults)
	{
		if (!DoDetailDialog(NULL)) return(FALSE);
	}


	// Make sure we have the game rez file...

	char* sGameRez = GetGameRezFile();
	char* sSoundRez = GetSoundRezFile();

	if (!sGameRez || sGameRez[0] == '\0')
	{
		ErrorMsg(NULL, 17, g_csCaption, MB_ICONEXCLAMATION);
		return(FALSE);
	}


	// Build the command line...

	CString csCmd;
	csCmd = "LITHTECH.EXE";
	csCmd += " -windowtitle \"";
	csCmd += g_csWindowTitle;
	csCmd += "\"";
	csCmd += " -rez ";
	csCmd += sGameRez;

	if (sSoundRez && sSoundRez[0] != '\0')
	{
		csCmd += " -rez ";
		csCmd += sSoundRez;
	}

	csCmd += " -rez ";
	csCmd += g_csRezBase;
	csCmd += "2.rez";
	csCmd += " -rez ";
	csCmd += g_csRezBase;
	csCmd += "dll.rez";
	csCmd += " -rez ";
	csCmd += g_csRezBase;
	csCmd += "l.rez";
	csCmd += " -rez custom";

	if (!g_csUpdateCmdLine.IsEmpty())
	{
		csCmd += " ";
		csCmd += g_csUpdateCmdLine;
	}

	if (!g_csContentCmdLine.IsEmpty())
	{
		csCmd += " ";
		csCmd += g_csContentCmdLine;
	}
#ifdef NOLFSRC_MODERN
	// custom/ -- the game code built from source -- last, over whatever the
	// update and content archives carry (the engine searches last-first).
	csCmd += " -rez custom";
#endif

	if (bMultiplayer)
	{
		csCmd += " +multiplayer 1";
	}
	else
	{
		csCmd += " +multiplayer 0";
	}

	CString csAdv = GetAdvancedCommands();
	if (csAdv.GetLength() != 0)
	{
		csCmd += " ";
		csCmd += csAdv;
	}

#ifdef NOLFSRC_MODERN
	// M-P1-17: a fresh install -- autoexec.cfg is still the shipped
	// defaults.cfg, which names no colour depth; the game writes one on its
	// first exit -- starts at the launcher's default mode (the largest 4:3
	// one that fits the desktop, a Steam Deck's own 1280x800), not the
	// defaults' 640x480, without the Display dialog having been opened.
	if (!g_bDisplaySet && !ModernConfigHasMode("autoexec.cfg"))
	{
		if (!g_pDisplayModes)
		{
			g_pDisplayModes = new CDisplayModes;
			if (!g_pDisplayModes->Init("", "autoexec.cfg"))
			{
				delete g_pDisplayModes;
				g_pDisplayModes = NULL;
			}
		}
		// The shipped defaults name the renderer but no card, which only
		// the Display dialog picks: the config's renderer (else the first)
		// and its first hardware card (else its first).
		CRenderer* pRenderer = g_pDisplayModes ? g_pDisplayModes->m_pCurRenderer : NULL;
		if (g_pDisplayModes && !pRenderer && !g_pDisplayModes->m_renderers.IsEmpty())
			pRenderer = (CRenderer*)g_pDisplayModes->m_renderers.GetHead();
		if (pRenderer && !pRenderer->m_pCurCard)
		{
			for (POSITION pos = pRenderer->m_cards.GetHeadPosition(); pos; )
			{
				CCard* pCard = (CCard*)pRenderer->m_cards.GetNext(pos);
				if (pCard && (!pRenderer->m_pCurCard || (pCard->m_bHardware && !pRenderer->m_pCurCard->m_bHardware)))
					pRenderer->m_pCurCard = pCard;
			}
		}
		if (pRenderer && pRenderer->m_pCurCard && pRenderer->m_pCurCard->m_pCurMode)
		{
			g_pDisplayModes->m_pCurRenderer = pRenderer;
			g_bDisplaySet = TRUE;
		}
	}
#endif
	if (g_bDisplaySet && g_pDisplayModes)
	{
		CString csDisplay = g_pDisplayModes->GetCommands();
		if (csDisplay.GetLength() != 0)
		{
			csCmd += " ";
			csCmd += csDisplay;
		}
	}

	CString csSound = Get3DSoundCommands();
	if (csSound.GetLength() != 0)
	{
		csCmd += " ";
		csCmd += csSound;
	}

	CString csRez = GetCustomRezCommands();
	if (csRez.GetLength() != 0)
	{
		csCmd += csRez;
	}

	if (g_bPerformanceSet)
	{
		CString csPerf;
		csPerf.Format(" ++PerformanceLevel %i", g_nPerformanceLevel);
		csCmd += csPerf;
	}

	csCmd += " ";
	csCmd += sCmdLine;
	csCmd += " ";
	csCmd += g_csCommands;


	// Holding SHIFT shows the command line...

	if (GetAsyncKeyState(VK_SHIFT) & 0x80000000)
	{
		if (MessageBox(NULL, csCmd, "Command Line String", MB_OKCANCEL) == IDCANCEL)
		{
			return(FALSE);
		}
	}


	// Write the command file and spawn the game...

	STARTUPINFO si;
	GetStartupInfo(&si);

	char sCmds[1024];
	strcpy(sCmds, csCmd);

	FILE* pFile = fopen("nolfcmds.txt", "wt");
	if (!pFile) return(FALSE);

	fprintf(pFile, sCmds);
	fclose(pFile);

	PROCESS_INFORMATION pi;
#ifdef NOLFSRC_MODERN
	// fixed: on some Windows 10/11 machines the retail call fails although
	// lithtech.exe starts fine by hand. It hands on the launcher's own
	// STARTUPINFO (which carries whatever Explorer, a shortcut or Steam
	// started the launcher with) and leaves Windows to search for a bare
	// "lithtech.exe". Start it from a clean STARTUPINFO, by its full path
	// beside the launcher, and if it still fails say why.
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	char sExe[MAX_PATH + 16];
	DWORD nLen = GetModuleFileName(NULL, sExe, MAX_PATH);
	char* pSlash = (nLen > 0 && nLen < MAX_PATH) ? strrchr(sExe, '\\') : NULL;
	if (pSlash) strcpy(pSlash + 1, "lithtech.exe");
	else strcpy(sExe, "lithtech.exe");

	char sLine[MAX_PATH + 48];
	sprintf(sLine, "\"%s\" -cmdfile nolfcmds.txt", sExe);

	if (!CreateProcess(sExe, sLine, NULL, NULL, FALSE,
			CREATE_DEFAULT_ERROR_MODE | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi))
	{
		DWORD nErr = GetLastError();
		char sText[1024];
		strcpy(sText, "Error.");
		LoadString(g_hInst, 6, sText, 511);
		int n = strlen(sText);
		n += sprintf(sText + n, "\n\n%s\nWindows error %lu: ", sExe, nErr);
		if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, nErr, 0, sText + n, sizeof(sText) - n, NULL))
			strcpy(sText + n, "(no description)");
		MessageBox(NULL, sText, g_csCaption, MB_ICONEXCLAMATION);
		return(FALSE);
	}

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
#else
	if (!CreateProcess(NULL, "lithtech.exe -cmdfile nolfcmds.txt", NULL, NULL, FALSE,
			CREATE_DEFAULT_ERROR_MODE | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi))
	{
		ErrorMsg(NULL, 6, g_csCaption, MB_ICONEXCLAMATION);
		return(FALSE);
	}
#endif

	return(TRUE);
}


// *********************************************************************** //
//
//	ROUTINE:	ParseAutoexec   [0x405d90]
//
//	PURPOSE:	Pulls the NumRuns setting back out of autoexec.cfg
//
// *********************************************************************** //

BOOL ParseAutoexec(const char* sFile)
{
	if (!sFile) return(FALSE);

	OFSTRUCT of;
	if (OpenFile(sFile, &of, OF_EXIST) == HFILE_ERROR) return(FALSE);

	FILE* pFile = fopen(sFile, "r");
	if (!pFile) return(FALSE);

	char sLine[512];
	char sValue[64];

	strcpy(sValue, "0");

	while (fgets(sLine, 512, pFile))
	{
		char* pPos = sLine;
		if (sLine[0] == '"')
		{
			pPos = &sLine[1];
		}

		if (strnicmp(pPos, "NumRuns", 7) == 0)
		{
			pPos += 7;
			while (*pPos == '"' || *pPos == ' ')
			{
				pPos++;
			}

			strcpy(sValue, pPos);

			char* pEnd = &sValue[strlen(sValue) - 1];
			while ((*pEnd == '"' || *pEnd == '\n') && pEnd > sValue)
			{
				pEnd--;
			}

			pEnd[1] = '\0';

			break;
		}
	}

	g_nNumRunsCfg = atoi(sValue);

	fclose(pFile);

	return(TRUE);
}


// EOF...
