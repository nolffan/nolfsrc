// p3_folderinterface.h -- the Interface options folder (MODERN.md, M-P3-13).
// Modern build only: reached from Options.

#ifndef P3_FOLDERINTERFACE_H
#define P3_FOLDERINTERFACE_H

#include "BaseFolder.h"

class CFolderInterface : public CBaseFolder
{
public:
	CFolderInterface();
	virtual ~CFolderInterface();

	// Build the folder
    LTBOOL  Build();
    void    OnFocus(LTBOOL bFocus);

protected:
	int		m_nHUDScale;		// UIScale, in percent
	int		m_nFramerate;		// FramerateLimit, as its choice (M-P1-25)
	LTBOOL	m_bShowFramerate;	// ShowFramerate
	LTBOOL	m_bCinematics4x3;	// RestrictCinematicsTo4x3
	LTBOOL	m_bGotyMenu;		// UseGotyMenu
	int		m_nMenuCate;		// MainMenuCate (M-P8-10)
};

#endif // P3_FOLDERINTERFACE_H
