// ----------------------------------------------------------------------- //
//
// MODULE  : p4_jukebox.cpp
//
// PURPOSE : M-P4-02, the jukebox (modern build only).
//
//           Options -> Jukebox lists the music themes on the left and, for
//           the theme picked, its pieces on the right (each piece is one
//           intensity of the theme's DirectMusic control file). Choosing a
//           piece loads the theme and plays that intensity; a line at the
//           bottom says what is playing. Leaving the folder puts back the
//           music that was playing when it was entered (menu music, or the
//           level's music when entered from the game).
//
//           What is listed comes from ATTRIBUTES/Jukebox.txt, written for
//           this project (src/modern/data/files/ATTRIBUTES/Jukebox.txt);
//           the layout from LAYOUT.txt's [FolderJukebox] section (M-P4-03).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "cshell/p4_jukebox.h"
#include "FolderMgr.h"
#include "FolderCommands.h"
#include "InterfaceMgr.h"
#include "LayoutMgr.h"
#include "GameSettings.h"
#include "GameClientShell.h"
#include "cres/p4.h"

extern CGameClientShell* g_pGameClientShell;

namespace
{
	// Commands: one per theme, one per piece of the shown theme.
	enum
	{
		CMD_P4_THEME = FOLDER_CMD_CUSTOM + 1,
		CMD_P4_PIECE = CMD_P4_THEME + P4_JUKEBOX_MAX_THEMES,
	};

	// Copies the string data of an HSTRING and frees it.
	void TakeString(HSTRING hStr, char *pBuf, int nBufLen)
	{
		pBuf[0] = '\0';
		if (!hStr) return;
		char *pData = g_pLTClient->GetStringData(hStr);
		if (pData)
		{
			strncpy(pBuf, pData, nBufLen - 1);
			pBuf[nBufLen - 1] = '\0';
		}
		g_pLTClient->FreeString(hStr);
	}

	// A layout value of this folder, or a default when LAYOUT.txt has none.
	LTRect LayoutRect(eFolderID eId, char *pAttribute, int l, int t, int r, int b)
	{
		if (g_pLayoutMgr->HasCustomValue(eId, pAttribute))
			return g_pLayoutMgr->GetFolderCustomRect(eId, pAttribute);
		return LTRect(l, t, r, b);
	}

	LTIntPt LayoutPoint(eFolderID eId, char *pAttribute, int x, int y)
	{
		if (g_pLayoutMgr->HasCustomValue(eId, pAttribute))
			return g_pLayoutMgr->GetFolderCustomPoint(eId, pAttribute);
		return LTIntPt(x, y);
	}

	int LayoutInt(eFolderID eId, char *pAttribute, int nDefault)
	{
		if (g_pLayoutMgr->HasCustomValue(eId, pAttribute))
			return g_pLayoutMgr->GetFolderCustomInt(eId, pAttribute);
		return nDefault;
	}
}


// ----------------------------------------------------------------------- //
//	CP4JukeboxButeMgr
// ----------------------------------------------------------------------- //

CP4JukeboxButeMgr::CP4JukeboxButeMgr()
{
	m_nThemes = 0;
	memset(m_aThemes, 0, sizeof(m_aThemes));
}

LTBOOL CP4JukeboxButeMgr::Init(ILTCSBase *pInterface, const char* szAttributeFile)
{
	m_nThemes = 0;
	if (!pInterface || !szAttributeFile) return LTFALSE;

	// CGameButeMgr::Parse shuts the game down when the file is missing; a
	// missing jukebox list only leaves the jukebox empty.
	char szFile[128];
	strncpy(szFile, szAttributeFile, sizeof(szFile) - 1);
	szFile[sizeof(szFile) - 1] = '\0';
	ILTStream *pStream = LTNULL;
	if (pInterface->OpenFile(szFile, &pStream) != LT_OK || !pStream)
	{
		g_pLTClient->CPrint("Jukebox: %s not found", szAttributeFile);
		return LTFALSE;
	}
	pStream->Release();

	if (!Parse(pInterface, szAttributeFile)) return LTFALSE;

	char szTag[32];
	char szAtt[32];
	while (m_nThemes < P4_JUKEBOX_MAX_THEMES)
	{
		sprintf(szTag, "Theme%d", m_nThemes);
		if (!m_buteMgr.Exist(szTag)) break;

		P4JukeboxTheme *pTheme = &m_aThemes[m_nThemes];
		memset(pTheme, 0, sizeof(*pTheme));

		int nNameId = m_buteMgr.GetInt(szTag, "NameId", 0);
		if (nNameId)
			TakeString(g_pLTClient->FormatString(nNameId), pTheme->szName, sizeof(pTheme->szName));
		if (!pTheme->szName[0])
			m_buteMgr.GetString(szTag, "Name", szTag, pTheme->szName, sizeof(pTheme->szName));

		char szDir[64];
		m_buteMgr.GetString(szTag, "Directory", "", szDir, sizeof(szDir));
		_snprintf(pTheme->szDirectory, sizeof(pTheme->szDirectory) - 1, "Music\\%s", szDir);
		m_buteMgr.GetString(szTag, "ControlFile", "", pTheme->szControlFile, sizeof(pTheme->szControlFile));

		// Listed only if its music is in the game's data (the Game of the
		// Year theme needs nolfgoty.rez).
		char szCtrl[160];
		_snprintf(szCtrl, sizeof(szCtrl) - 1, "%s\\%s", pTheme->szDirectory, pTheme->szControlFile);
		szCtrl[sizeof(szCtrl) - 1] = '\0';
		pStream = LTNULL;
		if (szDir[0] && pTheme->szControlFile[0] &&
			pInterface->OpenFile(szCtrl, &pStream) == LT_OK && pStream)
		{
			pStream->Release();
			pTheme->bAvailable = LTTRUE;
		}

		while (pTheme->nPieces < P4_JUKEBOX_MAX_PIECES)
		{
			sprintf(szAtt, "Intensity%d", pTheme->nPieces);
			if (!m_buteMgr.Exist(szTag, szAtt)) break;

			P4JukeboxPiece *pPiece = &pTheme->aPieces[pTheme->nPieces];
			pPiece->nIntensity = m_buteMgr.GetInt(szTag, szAtt, 0);

			char szDefault[32];
			sprintf(szDefault, "%d", pPiece->nIntensity);
			sprintf(szAtt, "Label%d", pTheme->nPieces);
			m_buteMgr.GetString(szTag, szAtt, szDefault, pPiece->szLabel, sizeof(pPiece->szLabel));

			pTheme->nPieces++;
		}

		m_nThemes++;
	}

	return LTTRUE;
}

const P4JukeboxTheme* CP4JukeboxButeMgr::GetTheme(int nTheme) const
{
	if (nTheme < 0 || nTheme >= m_nThemes) return LTNULL;
	return &m_aThemes[nTheme];
}


// ----------------------------------------------------------------------- //
//	CP4FolderJukebox
// ----------------------------------------------------------------------- //

CP4FolderJukebox::CP4FolderJukebox()
{
	m_bButesLoaded	= LTFALSE;
	m_pPieceHeader	= LTNULL;
	m_pPieceList	= LTNULL;
	m_pNowPlaying	= LTNULL;
	m_nPieceWidth	= 0;
	m_nShownTheme	= -1;
	m_nPlayingTheme	= -1;
	m_nPlayingPiece	= -1;
	m_bChangedMusic	= LTFALSE;
}

CP4FolderJukebox::~CP4FolderJukebox()
{
}

LTBOOL CP4FolderJukebox::Build()
{
	eFolderID eId = (eFolderID)m_nFolderID;

	if (!m_bButesLoaded)
	{
		m_Butes.Init(g_pLTClient);
		m_bButesLoaded = LTTRUE;
	}

	CreateTitle(IDS_P4_TITLE_JUKEBOX);

	// The themes are the folder's ordinary items, in PageRect (LAYOUT.txt);
	// the pieces a list in PieceRect, with the theme's name over it.
	LTRect rcPieces	= LayoutRect(eId, "PieceRect", 330, 104, 600, 360);
	LTIntPt ptNow	= LayoutPoint(eId, "NowPlayingPos", 70, 372);
	int nNowWidth	= LayoutInt(eId, "NowPlayingWidth", 520);
	int nArrowWidth	= LayoutInt(eId, "ArrowWidth", 24);
	int nIndent		= LayoutInt(eId, "TextIndent", 4);

	CLTGUIFont *pHeaderFont = GetLargeFont();
	CLTGUIFont *pItemFont = GetMediumFont();

	// M-P3-23: the fonts' sizes are screen pixels, the layout's page units:
	// heights become page units for positions (P3_LayoutLen), page widths
	// screen pixels for the controls (P3_MenuLen)
	LTIntPt pos(GetPageLeft(), GetPageTop() - P3_LayoutLen(pHeaderFont->GetHeight()) - 4);
	CStaticTextCtrl *pCtrl = CreateStaticTextItem(IDS_P4_JUKEBOX_THEMES, LTNULL, LTNULL, 0, 0, LTTRUE, pHeaderFont);
	if (pCtrl)
	{
		AddFixedControl(pCtrl, pos, LTFALSE);
		pCtrl->Enable(LTFALSE);
	}

	for (int i = 0; i < m_Butes.GetNumThemes(); i++)
	{
		const P4JukeboxTheme *pTheme = m_Butes.GetTheme(i);
		if (!pTheme->bAvailable) continue;
		AddTextItem((char *)pTheme->szName, CMD_P4_THEME + i, IDS_P4_HELP_JUKEBOX_THEME);
	}

	m_nPieceWidth = P3_MenuLen((rcPieces.right - rcPieces.left) - nArrowWidth);
	pos = LTIntPt(rcPieces.left + nIndent, rcPieces.top);
	m_pPieceHeader = CreateStaticTextItem(" ", LTNULL, LTNULL, m_nPieceWidth, pHeaderFont->GetHeight(), LTTRUE, pHeaderFont);
	if (m_pPieceHeader)
	{
		AddFixedControl(m_pPieceHeader, pos, LTFALSE);
		m_pPieceHeader->Enable(LTFALSE);
	}

	pos.y += P3_LayoutLen(pHeaderFont->GetHeight()) + 4;
	int nListHeight = P3_MenuLen(rcPieces.bottom - pos.y);
	m_pPieceList = debug_new(CListCtrl);
	m_pPieceList->Create(nListHeight, LTTRUE, m_nPieceWidth);
	m_pPieceList->SetParam1(m_nPieceWidth);
	m_pPieceList->SetItemSpacing(2);
	m_pPieceList->EnableMouseMoveSelect(LTTRUE);
	m_pPieceList->SetHelpID(IDS_P4_HELP_JUKEBOX_PIECE);
	AddFixedControl(m_pPieceList, pos, LTTRUE);

	m_pNowPlaying = CreateStaticTextItem(IDS_P4_JUKEBOX_NOTHING, LTNULL, LTNULL, P3_MenuLen(nNowWidth), 0, LTTRUE, pItemFont);
	if (m_pNowPlaying)
	{
		AddFixedControl(m_pNowPlaying, ptNow, LTFALSE);
		m_pNowPlaying->Enable(LTFALSE);
	}

	// Make sure to call the base class
	if (!CBaseFolder::Build()) return LTFALSE;

	UseBack(LTTRUE, LTTRUE);
	return LTTRUE;
}

uint32 CP4FolderJukebox::OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2)
{
	if (dwCommand >= (uint32)CMD_P4_THEME && dwCommand < (uint32)(CMD_P4_THEME + P4_JUKEBOX_MAX_THEMES))
	{
		ShowTheme(dwCommand - CMD_P4_THEME);
		// on to the theme's pieces
		if (m_pPieceList && m_pPieceList->GetNum() > 0)
			SetSelection(GetIndex(m_pPieceList));
		return 1;
	}
	if (dwCommand >= (uint32)CMD_P4_PIECE && dwCommand < (uint32)(CMD_P4_PIECE + P4_JUKEBOX_MAX_PIECES))
	{
		PlayPiece(dwCommand - CMD_P4_PIECE);
		return 1;
	}
	return CBaseFolder::OnCommand(dwCommand, dwParam1, dwParam2);
}

void CP4FolderJukebox::OnFocus(LTBOOL bFocus)
{
	if (bFocus)
	{
		// Remember the music as it is, to put it back on the way out.
		m_SavedState.Copy(*g_pGameClientShell->GetMusic()->GetMusicState());
		m_bChangedMusic	= LTFALSE;
		m_nPlayingTheme	= -1;
		m_nPlayingPiece	= -1;

		// Show the first theme's pieces, so the list is never empty.
		int nFirst = -1;
		for (int i = 0; i < m_Butes.GetNumThemes() && nFirst < 0; i++)
		{
			if (m_Butes.GetTheme(i)->bAvailable) nFirst = i;
		}
		m_nShownTheme = -1;
		if (nFirst >= 0) ShowTheme(nFirst);
		UpdateNowPlaying();
	}
	else
	{
		RestoreMusic();
	}

	CBaseFolder::OnFocus(bFocus);
}

void CP4FolderJukebox::ShowTheme(int nTheme)
{
	const P4JukeboxTheme *pTheme = m_Butes.GetTheme(nTheme);
	if (!pTheme || !pTheme->bAvailable || !m_pPieceList) return;
	if (nTheme == m_nShownTheme) return;

	m_nShownTheme = nTheme;
	if (m_pPieceHeader) m_pPieceHeader->SetString((char *)pTheme->szName);

	m_pPieceList->RemoveAllControls();
	for (int i = 0; i < pTheme->nPieces; i++)
	{
		CStaticTextCtrl *pCtrl = CreateStaticTextItem((char *)pTheme->aPieces[i].szLabel, CMD_P4_PIECE + i,
			IDS_P4_HELP_JUKEBOX_PIECE, m_nPieceWidth, 0, LTFALSE, GetMediumFont());
		if (pCtrl) m_pPieceList->AddControl(pCtrl);
	}
	// so that the keyboard can walk the list (Up/Down, Return plays)
	if (pTheme->nPieces > 0) m_pPieceList->SelectItem(0);
}

CMusic* CP4FolderJukebox::GetUsableMusic()
{
	CMusic *pMusic = g_pGameClientShell->GetMusic();
	if (!pMusic) return LTNULL;

	// Music switched off in the options (or by the console): leave it so.
	if (!g_pInterfaceMgr->GetSettings()->MusicEnabled()) return LTNULL;
	if (!(g_pInterfaceMgr->GetAdvancedOptions() & AO_MUSIC)) return LTNULL;

	if (!pMusic->IsInitialized() && !pMusic->Init(g_pLTClient)) return LTNULL;
	if (!pMusic->MusicEnabled()) return LTNULL;

	return pMusic;
}

void CP4FolderJukebox::PlayPiece(int nPiece)
{
	const P4JukeboxTheme *pTheme = m_Butes.GetTheme(m_nShownTheme);
	if (!pTheme || nPiece < 0 || nPiece >= pTheme->nPieces) return;

	CMusic *pMusic = GetUsableMusic();
	if (!pMusic)
	{
		if (m_pNowPlaying) m_pNowPlaying->SetString(IDS_P4_JUKEBOX_NO_MUSIC);
		return;
	}

	CMusicState *pState = pMusic->GetMusicState();
	if (!pMusic->IsLevelInitialized() ||
		stricmp(pState->szDirectory, pTheme->szDirectory) != 0 ||
		stricmp(pState->szControlFile, pTheme->szControlFile) != 0)
	{
		// Another theme: stop, then load this one's control file.
		pMusic->Stop();
		m_bChangedMusic = LTTRUE;

		CMusicState newState;
		strcpy(newState.szDirectory, pTheme->szDirectory);
		strcpy(newState.szControlFile, pTheme->szControlFile);
		if (!pMusic->RestoreMusicState(newState))
		{
			// (no DirectMusic: CMusic has no music manager)
			m_nPlayingTheme = -1;
			if (m_pNowPlaying) m_pNowPlaying->SetString(IDS_P4_JUKEBOX_CANT_PLAY);
			return;
		}
	}

	// ChangeIntensity starts the music if it is stopped.
	pMusic->ChangeIntensity(pTheme->aPieces[nPiece].nIntensity, LTDMEnactImmediatly);
	g_pInterfaceMgr->GetSettings()->ImplementMusicVolume();

	m_bChangedMusic	= LTTRUE;
	m_nPlayingTheme	= m_nShownTheme;
	m_nPlayingPiece	= nPiece;
	UpdateNowPlaying();
}

void CP4FolderJukebox::RestoreMusic()
{
	if (!m_bChangedMusic) return;
	m_bChangedMusic	= LTFALSE;
	m_nPlayingTheme	= -1;
	m_nPlayingPiece	= -1;

	CMusic *pMusic = g_pGameClientShell->GetMusic();
	if (!pMusic || !pMusic->IsInitialized()) return;

	pMusic->Stop();

	if (!m_SavedState.szDirectory[0] || !m_SavedState.szControlFile[0])
	{
		// Nothing was loaded before.
		pMusic->TermLevel();
		return;
	}

	if (!pMusic->RestoreMusicState(m_SavedState)) return;

	if (m_SavedState.bPlaying)
	{
		// (starts it again, as in PlayPiece)
		pMusic->ChangeIntensity(m_SavedState.nIntensity, m_SavedState.nIntensityEnact);
		if (m_SavedState.bPaused) pMusic->Pause();
		g_pInterfaceMgr->GetSettings()->ImplementMusicVolume();
	}
	else
	{
		// Keep the intensity it had, for whoever starts it next.
		pMusic->GetMusicState()->nIntensity = m_SavedState.nIntensity;
		pMusic->GetMusicState()->nIntensityEnact = m_SavedState.nIntensityEnact;
	}
}

void CP4FolderJukebox::UpdateNowPlaying()
{
	if (!m_pNowPlaying) return;

	const P4JukeboxTheme *pTheme = m_Butes.GetTheme(m_nPlayingTheme);
	if (!pTheme || m_nPlayingPiece < 0 || m_nPlayingPiece >= pTheme->nPieces)
	{
		m_pNowPlaying->SetString(IDS_P4_JUKEBOX_NOTHING);
		return;
	}

	HSTRING hStr = g_pLTClient->FormatString(IDS_P4_JUKEBOX_NOW_PLAYING,
		pTheme->szName, pTheme->aPieces[m_nPlayingPiece].szLabel);
	m_pNowPlaying->SetString(hStr);
	g_pLTClient->FreeString(hStr);
}

CBaseFolder* P4_CreateJukeboxFolder()
{
	return debug_new(CP4FolderJukebox);
}
