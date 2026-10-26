// ----------------------------------------------------------------------- //
//
// MODULE  : p9_mods.cpp
//
// PURPOSE : P9's client call-outs (p9_mods.h): mods made for the NOLF
//           Modernizer (NOLFSRC_MODERN only)
//
// ----------------------------------------------------------------------- //
//
// The Thief mod (ThiefV1.0.rez) is data for the Modernizer: it adds the PS2
// release's thief levels (Worlds\thiefmission1-3) and the PS2 soundtrack
// (Music\PS2Music\*.wav, named per level by ATTRIBUTES\LevelMusic.txt).
// Everything here is keyed on that data being present, so without the mod
// nothing changes. The console variable keeps the Modernizer's name,
// UsePS2Music, so a config written by either game means the same.

#include "stdafx.h"
#include "GameClientShell.h"
#include "InterfaceMgr.h"
#include "GameSettings.h"
#include "ClientSoundMgr.h"
#include "ClientRes.h"
#include "VarTrack.h"
#include "LayoutMgr.h"
#include "FolderMgr.h"
#include "cshell/p9_mods.h"
#include "cshell/p2_log.h"

extern CGameClientShell* g_pGameClientShell;

namespace
{
	// The Thief mod's own string, from its CRes.dll (M-P9-02)
	const int kP9NineYearsAgo = 959;

	VarTrack	s_vtUsePS2Music;

	char		s_szWorld[64];			// the world's name, without path or extension
	HLTSOUND	s_hLevelTrack	= LTNULL;
	HLTSOUND	s_hJukeboxWAV	= LTNULL;
	int			s_nTrackVolume	= -1;	// what s_hLevelTrack was started at

	// ATTRIBUTES\LevelMusic.txt: [Level<n>] Name (a world), WAV (a file
	// under Music\PS2Music\), n = 0, 1, 2, ... to the first missing
	class CP9LevelMusicMgr : public CGameButeMgr
	{
		public :

			CP9LevelMusicMgr() : m_bTried(LTFALSE), m_bLoaded(LTFALSE) {}

			LTBOOL Init(ILTCSBase *pInterface, const char* szAttributeFile = "Attributes\\LevelMusic.txt")
			{
				// CGameButeMgr::Parse shuts the game down when the file is
				// missing; without the mod it is
				ILTStream *pStream = LTNULL;
				if (pInterface->OpenFile((char*)szAttributeFile, &pStream) != LT_OK || !pStream)
					return LTFALSE;
				pStream->Release();
				return Parse(pInterface, szAttributeFile);
			}

			LTBOOL Loaded()
			{
				if (!m_bTried)
				{
					m_bTried = LTTRUE;
					m_bLoaded = Init(g_pLTClient);
				}
				return m_bLoaded;
			}

			LTBOOL GetTrack(const char* pWorld, char* pBuf, int nBufLen)
			{
				if (!Loaded() || !pWorld[0]) return LTFALSE;

				char szTag[32], szName[64];
				for (int i = 0; ; i++)
				{
					sprintf(szTag, "Level%d", i);
					if (!m_buteMgr.Exist(szTag)) return LTFALSE;

					m_buteMgr.GetString(szTag, "Name", "", szName, sizeof(szName));
					if (stricmp(szName, pWorld) == 0)
					{
						m_buteMgr.GetString(szTag, "WAV", "", pBuf, nBufLen);
						return pBuf[0] != '\0';
					}
				}
			}

		private :

			LTBOOL	m_bTried;
			LTBOOL	m_bLoaded;
	};

	CP9LevelMusicMgr s_LevelMusic;

	// The world's file name without its path or extension.
	void WorldName(const char* pWorldFile, char* pBuf, int nBufLen)
	{
		pBuf[0] = '\0';
		if (!pWorldFile) return;

		const char* pName = strrchr(pWorldFile, '\\');
		const char* pSlash = strrchr(pWorldFile, '/');
		if (pSlash > pName) pName = pSlash;
		pName = pName ? pName + 1 : pWorldFile;

		strncpy(pBuf, pName, nBufLen - 1);
		pBuf[nBufLen - 1] = '\0';
		char* pDot = strrchr(pBuf, '.');
		if (pDot) *pDot = '\0';
	}

	// The music volume as a sound volume, 0-100; 0 when music is off.
	int MusicVolume()
	{
		CGameSettings* pSettings = g_pInterfaceMgr->GetSettings();
		if (!pSettings->MusicEnabled()) return 0;
		if (!(g_pInterfaceMgr->GetAdvancedOptions() & AO_MUSIC)) return 0;

		// musicvolume runs -5000 to 5000 (FolderAudio.cpp)
		int nVolume = (int)((pSettings->MusicVolume() + 5000.0f) / 100.0f + 0.5f);
		return nVolume < 0 ? 0 : (nVolume > 100 ? 100 : nVolume);
	}

	// A looping music WAV, local to the listener, at the music volume.
	HLTSOUND PlayMusicWAV(const char* pFile, int nVolume)
	{
		if (!pFile || nVolume <= 0) return LTNULL;

		ILTStream *pStream = LTNULL;
		if (g_pLTClient->OpenFile((char*)pFile, &pStream) != LT_OK || !pStream)
		{
			ModernLog("PS2 music: %s is not in the data", pFile);
			return LTNULL;
		}
		pStream->Release();

		return g_pClientSoundMgr->PlaySoundLocal((char*)pFile, SOUNDPRIORITY_MISC_MEDIUM,
			PLAYSOUND_LOOP | PLAYSOUND_GETHANDLE | PLAYSOUND_CLIENT, (uint8)nVolume);
	}

	void KillSound(HLTSOUND & hSound)
	{
		if (hSound)
		{
			g_pLTClient->KillSoundLoop(hSound);
			hSound = LTNULL;
		}
	}
}

void P9_OnEngineInitialized()
{
	if (!s_vtUsePS2Music.IsInitted())
	{
		s_vtUsePS2Music.Init(g_pLTClient, "UsePS2Music", NULL, 0.0f);
	}
}

// ----------------------------------------------------------------------- //
//
//	M-P9-06: the thief levels' loading screen
//
// ----------------------------------------------------------------------- //

void P9_OnLoadWorld(const char* pWorldFile, int nLevel)
{
	WorldName(pWorldFile, s_szWorld, sizeof(s_szWorld));

	// As the PS2 release had it: "Nine years ago...", the scene of the
	// mission it is played in, and the thief photo. The text is the mod's;
	// without its CRes.dll the mission's own screen stays.
	if (_strnicmp(s_szWorld, "thiefmission", 12) != 0) return;

	HSTRING hTxt = g_pLTClient->FormatString(kP9NineYearsAgo);
	if (!hTxt) return;

	char* pTxt = g_pLTClient->GetStringData(hTxt);
	if (pTxt && pTxt[0])
	{
		if (nLevel >= 0)
		{
			char tmp[128];
			HSTRING hTxt2 = g_pLTClient->FormatString(IDS_SCENENUMBER, nLevel + 1);
			// ("Nine years ago... Scene 2": the mod's text ends in dots)
			int nTxtLen = strlen(pTxt);
			const char* pSep = (pTxt[nTxtLen - 1] == '.') ? " " : ", ";
			_snprintf(tmp, sizeof(tmp), "%s%s%s", pTxt, pSep, hTxt2 ? g_pLTClient->GetStringData(hTxt2) : "");
			tmp[sizeof(tmp) - 1] = '\0';
			HSTRING hWorld = g_pLTClient->CreateString(tmp);
			g_pInterfaceMgr->SetLoadLevelString(hWorld);
			g_pLTClient->FreeString(hWorld);
			if (hTxt2) g_pLTClient->FreeString(hTxt2);
		}
		else
		{
			g_pInterfaceMgr->SetLoadLevelString(hTxt);
		}

		ILTStream *pStream = LTNULL;
		char szPhoto[] = "interface\\photos\\missions\\thief.pcx";
		if (g_pLTClient->OpenFile(szPhoto, &pStream) == LT_OK && pStream)
		{
			pStream->Release();
			g_pInterfaceMgr->SetLoadLevelPhoto(szPhoto);
		}
	}
	g_pLTClient->FreeString(hTxt);
}

// ----------------------------------------------------------------------- //
//
//	M-P9-08: the PS2 soundtrack
//
// ----------------------------------------------------------------------- //

LTBOOL P9_HavePS2Music()
{
	return s_LevelMusic.Loaded();
}

LTBOOL P9_StartLevelMusic()
{
	KillSound(s_hLevelTrack);

	if (g_pGameClientShell->GetGameType() != SINGLE) return LTFALSE;
	if (!s_vtUsePS2Music.IsInitted() || s_vtUsePS2Music.GetFloat() == 0.0f) return LTFALSE;

	char szWAV[64];
	if (!s_LevelMusic.GetTrack(s_szWorld, szWAV, sizeof(szWAV))) return LTFALSE;

	char szFile[128];
	_snprintf(szFile, sizeof(szFile), "Music\\PS2Music\\%s", szWAV);
	szFile[sizeof(szFile) - 1] = '\0';

	s_nTrackVolume = MusicVolume();
	s_hLevelTrack = PlayMusicWAV(szFile, s_nTrackVolume);
	if (!s_hLevelTrack) return LTFALSE;
	ModernLog("PS2 music: %s for %s", szFile, s_szWorld);

	// in place of the level's DirectMusic
	CMusic* pMusic = g_pGameClientShell->GetMusic();
	if (pMusic->IsInitialized()) pMusic->Stop();
	return LTTRUE;
}

LTBOOL P9_IsPS2MusicPlaying()
{
	return s_hLevelTrack != LTNULL;
}

void P9_StopLevelMusic()
{
	KillSound(s_hLevelTrack);
}

void P9_OnMusicSettingsChanged()
{
	if (!g_pGameClientShell->IsInWorld() || g_pGameClientShell->GetGameType() != SINGLE) return;

	LTBOOL bWanted = s_vtUsePS2Music.IsInitted() && s_vtUsePS2Music.GetFloat() != 0.0f;
	if (s_hLevelTrack && bWanted && MusicVolume() == s_nTrackVolume) return;

	LTBOOL bWasPlaying = (s_hLevelTrack != LTNULL);
	if (bWanted && P9_StartLevelMusic())
	{
		// (from the start, at the new volume; it plays on in the menus, as
		// the level's DirectMusic does)
		return;
	}
	KillSound(s_hLevelTrack);

	if (bWasPlaying)
	{
		// Back to the level's DirectMusic, as FirstUpdate sets it up
		CMusic* pMusic = g_pGameClientShell->GetMusic();
		char* pDir = g_pLTClient->GetServerConVarValueString("MusicDirectory");
		char* pCtrl = g_pLTClient->GetServerConVarValueString("MusicControlFile");
		if (pMusic->IsInitialized() && pDir && pCtrl)
		{
			CMusicState MusicState;
			strcpy(MusicState.szDirectory, pDir);
			strcpy(MusicState.szControlFile, pCtrl);
			if (pMusic->RestoreMusicState(MusicState)) pMusic->Play();
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	M-P9-09: young Cate on the main menu
//
// ----------------------------------------------------------------------- //

LTBOOL P9_HaveThiefMenu()
{
	static int s_nHave = -1;
	if (s_nHave < 0)
	{
		ILTStream *pStream = LTNULL;
		char szModel[] = "Chars\\Models\\IntHero_Thief.abc";
		s_nHave = (g_pLTClient->OpenFile(szModel, &pStream) == LT_OK && pStream) ? 1 : 0;
		if (pStream) pStream->Release();
	}
	return s_nHave == 1 && g_pLayoutMgr->HasCustomValue(FOLDER_ID_MAIN_THIEF, "Character");
}

LTBOOL P9_PlayJukeboxWAV(const char* pFile)
{
	KillSound(s_hJukeboxWAV);
	if (!pFile) return LTFALSE;

	s_hJukeboxWAV = PlayMusicWAV(pFile, MusicVolume());
	return s_hJukeboxWAV != LTNULL;
}
