// ----------------------------------------------------------------------- //
//
// MODULE  : p4_jukebox.h
//
// PURPOSE : M-P4-02, the jukebox (modern build only): an Options folder
//           that plays the game's music themes, one piece (intensity) at a
//           time, and the small attribute manager that lists them
//           (ATTRIBUTES/Jukebox.txt, a file of this project's own).
//
// ----------------------------------------------------------------------- //

#ifndef __MODERN_P4_JUKEBOX_H__
#define __MODERN_P4_JUKEBOX_H__

#include "BaseFolder.h"
#include "GameButeMgr.h"
#include "Music.h"

#define P4_JUKEBOX_MAX_THEMES	16
#define P4_JUKEBOX_MAX_PIECES	32

struct P4JukeboxPiece
{
	int		nIntensity;			// the control file's INTENSITY number
	char	szLabel[48];		// what the list shows
	char	szFile[64];			// M-P9-08: a WAV theme's file, in its directory
};

struct P4JukeboxTheme
{
	char			szName[64];
	char			szDirectory[64];	// under Music\ (CMusicState::szDirectory is 64)
	char			szControlFile[32];	// as CMusicState::szControlFile
	LTBOOL			bAvailable;			// its control file is in the game's data
	LTBOOL			bIsWAV;				// M-P9-08: WAV files, not DirectMusic
	int				nPieces;
	P4JukeboxPiece	aPieces[P4_JUKEBOX_MAX_PIECES];
};

// Reads ATTRIBUTES/Jukebox.txt:
//
//   [Theme<n>]                n = 0, 1, 2, ... (the list ends at the first gap)
//   NameId      = <CRes string id>   (or Name = "<text>")
//   Directory   = "<directory under Music\>"
//   ControlFile = "<control file in it>"
//   Intensity<k> = <intensity>      k = 0, 1, 2, ...: the pieces, in list
//   Label<k>     = "<text>"         order (the list ends at the first gap)
//
// A theme whose control file is not in the game's data (the Game of the Year
// theme without nolfgoty.rez) is kept but marked unavailable.
class CP4JukeboxButeMgr : public CGameButeMgr
{
	public :

		CP4JukeboxButeMgr();

        LTBOOL	Init(ILTCSBase *pInterface, const char* szAttributeFile = "Attributes\\Jukebox.txt");

		int						GetNumThemes() const	{ return m_nThemes; }
		const P4JukeboxTheme*	GetTheme(int nTheme) const;

	private :

		int				m_nThemes;
		P4JukeboxTheme	m_aThemes[P4_JUKEBOX_MAX_THEMES];
};

class CP4FolderJukebox : public CBaseFolder
{
	public :

		CP4FolderJukebox();
		virtual ~CP4FolderJukebox();

		virtual LTBOOL	Build();
		virtual uint32	OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2);
		virtual void	OnFocus(LTBOOL bFocus);

	private :

		void		ShowTheme(int nTheme);
		void		PlayPiece(int nPiece);
		void		RestoreMusic();
		void		UpdateNowPlaying();
		CMusic*		GetUsableMusic();

		CP4JukeboxButeMgr	m_Butes;
		LTBOOL				m_bButesLoaded;

		CStaticTextCtrl*	m_pPieceHeader;		// the shown theme's name, over the list
		CListCtrl*			m_pPieceList;		// the shown theme's pieces
		CStaticTextCtrl*	m_pNowPlaying;
		int					m_nPieceWidth;

		int					m_nShownTheme;		// theme whose pieces the list shows
		int					m_nPlayingTheme;	// what we started playing, or -1
		int					m_nPlayingPiece;

		CMusicState			m_SavedState;		// the music when the folder was entered
		LTBOOL				m_bChangedMusic;
};

// For CFolderMgr::AddFolder.
CBaseFolder* P4_CreateJukeboxFolder();

#endif // __MODERN_P4_JUKEBOX_H__
