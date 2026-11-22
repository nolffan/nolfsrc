// ----------------------------------------------------------------------- //
//
// MODULE  : p1_modern.h
//
// PURPOSE : The modern build's package P1 (MODERN.md): timing, input,
//           window and display options.  The retail files call into
//           these from #ifdef NOLFSRC_MODERN blocks; everything else
//           lives in src/modern/cshell/p1_*.cpp.
//
//           Compiled only in the modern build (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#ifndef __P1_MODERN_H__
#define __P1_MODERN_H__

#include "cres/p1.h"

// winuser.h for Windows 98/2000 and later; VC6's predates them.
#ifndef SM_XVIRTUALSCREEN
#define SM_XVIRTUALSCREEN		76
#define SM_YVIRTUALSCREEN		77
#define SM_CXVIRTUALSCREEN		78
#define SM_CYVIRTUALSCREEN		79
#endif

// The config version this build upgrades autoexec.cfg to (ModPatchNum,
// M-P1-12).  Raise it, and add a step to P1Config_AfterInit, for every new
// one-time change.
#define P1_CONFIG_VERSION		1.7f

// p1_config.cpp -- the console variables and the start-up steps.

void	P1Config_InitVars();			// M-P1-01/03/10/12 variables, M-P1-08 optimizesurfaces
void	P1Config_AfterInit();			// M-P1-08 CursorCenter, M-P1-12 one-time upgrade
int		P1Config_FramerateLimit();		// FramerateLimit: frames a second, 0 none
bool	P1Config_OldMouseLook();		// OldMouseLook
bool	P1Config_NoRawInput();			// NoRawInput
bool	P1Config_ScreenTinting();		// EnableScreenTinting, with d3d7.ren (else always)
bool	P1Config_LightScale();			// EnableLightScale, with d3d7.ren (else always)
bool	P1Config_IntelFixOn();			// either of the two off, whatever the renderer
bool	P1Config_IntelFixRenderer(const char* pRenderDll);	// M-P1-10 applies to it

// p1_timing.cpp -- M-P1-01 frame limiter, M-P1-05 clock.

float	P1Timing_GetTime();							// seconds, high resolution
void	P1Timing_SuspendFrameLimit(bool bSuspend);	// while the loading screen runs
void	P1Timing_EndFrame();						// end of each client frame
void	P1Timing_SendDontLockServer(bool bHosting);	// M-P1-02, on joining a game

// p1_input.cpp -- M-P1-03 raw mouse look.

void	P1Input_UseCursor(bool bUseCursor);			// from CInterfaceMgr::UseCursor
void	P1Input_GetAxisOffsets(float* pOffsets);	// yaw, pitch, (unused) for the camera
void	P1Input_OnWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void	P1Input_OnWindowUnhooked();
void	P1Input_OnFocus(bool bFocus);

// p1_window.cpp -- M-P1-06 window placement, M-P1-22 window mode.

enum
{
	P1_WINDOW_FULLSCREEN = 0,
	P1_WINDOW_WINDOWED,
	P1_WINDOW_BORDERLESS
};

void	P1Window_Apply();	// after a mode change, or when the engine may have moved the window
void	P1Window_Update();	// every frame: a borderless window's mode
int		P1Window_GetMode();				// P1_WINDOW_*, from Windowed and WindowMode
void	P1Window_SetMode(int nMode);	// writes both (the mode must then be set again)
void	P1Window_DesktopSize(int* pWidth, int* pHeight);

// Client events (CGameClientShell::OnEvent), for all of the above.
void	P1_OnClientEvent(uint32 dwEventID);

#endif // __P1_MODERN_H__
