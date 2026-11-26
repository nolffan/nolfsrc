// *********************************************************************** //
//
//	MODULE:		LAUNCHER (.H)
//
//	PURPOSE:	NOLF.exe launcher - shared declarations
//
// *********************************************************************** //

#ifndef _LAUNCHER_H_
#define _LAUNCHER_H_


// Forward declarations...

class CRegMgr;
class CDibMgr;
class CDib;
class CDibPal;


// Classes...

class CPreview
{
public:
	CPreview() { m_csName.Empty(); m_csFile.Empty(); m_csDir.Empty(); }
	~CPreview() { m_csName.Empty(); m_csFile.Empty(); m_csDir.Empty(); }

	CString		m_csName;			// +0
	CString		m_csFile;			// +4
	CString		m_csDir;			// +8

	BOOL		Init(const char* sName, const char* sFile, const char* sDir);
	CString		GetPath();
	BOOL		Run();
	CString		GetName();
};

class CPreviewList
{
public:
	CPreviewList() { m_csName.Empty(); m_csDir.Empty(); }
	~CPreviewList() { Term(); }

	CString		m_csName;			// +0
	CString		m_csDir;			// +4
	CPtrList	m_list;				// +8

	BOOL		Init(const char* sIniFile, const char* sDir);
	void		Term();
	CPreview*	AddPreview(const char* sName, const char* sFile);
	BOOL		FillListBox(HWND hList);
	BOOL		RunSelected(HWND hList);
};

// The renderer DLL's mode descriptor, as published by the engine SDK
// (src/LT2/sdk/inc/ltbasedefs.h).  The launcher links no engine code, so it
// carries its own copy.

struct RMode
{
	int			m_bHardware;
	char		m_RenderDLL[256];		// What DLL this comes from.
	char		m_InternalName[128];	// This is what the DLLs use to identify a card.
	char		m_Description[128];		// This is a 'friendly' string describing the card.
	unsigned	m_Width, m_Height, m_BitDepth;
	RMode*		m_pNext;
};

// The ID of the description string contained in render DLLs.
#define RDLL_DESCRIPTION_STRINGID	5

class CDisplayMode
{
public:
	CDisplayMode() { m_nWidth = 0; m_nHeight = 0; }
	~CDisplayMode() { m_nWidth = 0; m_nHeight = 0; }

	int			m_nWidth;			// +0x00
	int			m_nHeight;			// +0x04
	int			m_nDepth;			// +0x08
	CString		m_csName;			// +0x0c

	BOOL		Init(int nWidth, int nHeight, int nDepth);
	CString		GetName();
	CString		GetDisplayName();
};

class CCard
{
public:
	CCard() { m_csName.Empty(); m_bHardware = 0; m_pCurMode = NULL; }
	~CCard() { Term(); }

	CString			m_csName;		// +0x00
	CString			m_csDesc;		// +0x04
	int				m_bHardware;	// +0x08
	CPtrList		m_modes;		// +0x0c
	CDisplayMode*	m_pCurMode;		// +0x28
	POSITION		m_pos;			// +0x2c - the class's own list iterator

	BOOL			Init(const char* sName, const char* sDesc, int bHardware);
	void			Term();
	CString			GetName();
	CString			GetDesc();
	CString			GetDisplayName();
	LRESULT			GetListSelData(HWND hList);
	CDisplayMode*	AddMode(int nWidth, int nHeight, int nDepth);
	CDisplayMode*	FindMode(int nWidth, int nHeight, int nDepth);
	BOOL			SetCurMode(int nWidth, int nHeight, int nDepth);
	BOOL			SelectCurMode(HWND hList);
	BOOL			FillModeList(HWND hList);
};

class CRenderer
{
public:
	CRenderer() { m_csDll.Empty(); m_csDesc.Empty(); m_pCurCard = NULL; }
	~CRenderer() { Term(); }

	CString		m_csDll;			// +0x00
	CString		m_csDesc;			// +0x04
	CPtrList	m_cards;			// +0x08
	CCard*		m_pCurCard;			// +0x24
	POSITION	m_pos;				// +0x28 - the class's own list iterator

	BOOL		Init(const char* sDll);
	void		Term();
	CString		GetName();
	CString		GetDesc();
	CString		GetListName();
	LRESULT		GetListSelData(HWND hList);
	CCard*		AddCard(const char* sName, const char* sDesc, int bHardware);
	CCard*		AddCard(RMode* pMode);
	CCard*		FindCard(const char* sName);
	CCard*		FindCard(RMode* pMode);
	BOOL		SetCurCard(const char* sName);
	BOOL		SelectCurCard(HWND hList);
	BOOL		FillCardList(HWND hList);
};


struct SOUNDPROVIDER
{
	int				nProvider;		// +0x000 - Miles provider handle
	int				nType;			// +0x004 - the engine's provider number
	char			sName[261];		// +0x008
	int				bEAX;			// +0x110
	SOUNDPROVIDER*	pNext;			// +0x114
};

class CSoundProviders
{
public:
	CSoundProviders();
	~CSoundProviders();

	SOUNDPROVIDER*	m_pProviders;	// +0

	void	SetPreferences();
	BOOL	Enumerate(SOUNDPROVIDER** ppProviders, BOOL bCheckEAX);
	void	FreeProviders(SOUNDPROVIDER* pProviders);
};

class CDisplayModes
{
public:
	CDisplayModes() { m_pCurRenderer = NULL; }
	~CDisplayModes() { Term(); }

	CString		m_csRenderDll;		// +0x00
	CPtrList	m_renderers;		// +0x04
	CRenderer*	m_pCurRenderer;		// +0x20
	POSITION	m_pos;				// +0x24 - the class's own list iterator

	BOOL		Init(const char* sDir, const char* sFile);
	void		Term();
	LRESULT		GetListSelData(HWND hList);
	CRenderer*	AddRenderer(const char* sDll);
	CRenderer*	FindRenderer(const char* sDll);
	BOOL		SelectCurRenderer(HWND hList);
	BOOL		FillRendererList(HWND hList);
	int			FindRenderers(const char* sDir);
	void		SetDefaultModes();
	BOOL		FillLists(HWND hRenderer, HWND hCard, HWND hMode);
	BOOL		UpdateLists(HWND hRenderer, HWND hCard, HWND hMode);
	BOOL		ReadDisplayConfig(const char* sFile);
	CString		GetCommands();
};


// Prototypes...

BOOL	CheckCd(HWND hWnd);
int		HitTest(HWND hWnd, UINT nFlags, int x, int y);
extern RECT g_rcWebLith;
extern RECT g_rcWebOperative;
extern RECT g_rcWebNews;
BOOL	LaunchGame(char* sCmdLine, BOOL bMultiplayer);
int		DoDetailDialog(HWND hWnd);
CString	GetAdvancedCommands();
CString	Get3DSoundCommands();
CString	GetCustomRezCommands();
BOOL	DoPreviewDialog(HINSTANCE hInst, HWND hWnd);
BOOL	ExistFileInDir(const char* sDir, const char* sFile);
BOOL	RunProgram(const char* sDir, const char* sFile);
BOOL	RunSetup();
void	PaintSplash(HWND hWnd, BOOL bForceBackground);
int		ErrorMsg(HWND hWnd, int nStringID, const char* sCaption, UINT nType);
int		ErrorMsg(HWND hWnd, int nStringID, UINT nType);
char	FindCdRom(BOOL* pbOtherCd);
extern char g_chCdRom;
void	LoadCommands();
void	LoadRezFiles();
BOOL	ParseAutoexec(const char* sFile);
void	LoadSoundSettings(CRegMgr* pRegMgr);
int		Do3DSoundSetup(HINSTANCE hInst, HWND hWnd, int bWarnIfNone);
BOOL	CALLBACK GameDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


// Externs...

extern CWinApp		theApp;
extern CString		g_csCommands;
extern CString		g_csCaption;
extern CString		g_csWindowTitle;
extern CString		g_csGameName;
extern CString		g_csRez1;
extern CString		g_csRez2;
extern CString		g_csRezBase;
extern CString		g_csSetupExe;
extern CString		g_csNumGames;
extern CString		g_csLanguage;
extern CString		g_csUpdateCmdLine;
extern CString		g_csContentCmdLine;

extern HINSTANCE	g_hInstApp;
extern HINSTANCE	g_hInst;

extern CRegMgr*		g_pRegMgr;
extern CDibMgr*		g_pDibMgr;
extern CDib*		g_pDib;
extern CDibPal*		g_pPal;
extern CPreviewList*	g_pMovies;
extern CPreviewList*	g_pDemos;
extern CDisplayModes*	g_pDisplayModes;

extern int			g_bDisplayWarned;
extern int			g_nNumRuns;
extern int			g_bDisableSound;
extern int			g_bDisableMusic;
extern int			g_bDisableMovies;
extern int			g_bDisableFog;
extern int			g_bDisableJoystick;
extern int			g_bDisableTripBuf;
extern int			g_bDisableCursor;
extern int			g_bDisableLoadscreen;
#ifdef NOLFSRC_MODERN
extern int			g_bDisableLauncherSound;	// M-P1-29
#endif

extern char			g_sDir[256];
extern char			g_sCdRomPath[64];
extern char			g_sGame[16];

extern HCURSOR		g_hCursorHand;
extern HCURSOR		g_hCursorClick;
extern HCURSOR		g_hCursorArrow;

extern int			g_nNumRunsCfg;
extern int			g_bRestoreDefaults;
extern int			g_bDisplaySet;
extern int			g_bPerformanceSet;
extern int			g_nPerformanceLevel;


// EOF...

#endif
