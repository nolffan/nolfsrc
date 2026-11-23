// ----------------------------------------------------------------------- //
//
// MODULE  : p7_pad.h
//
// PURPOSE : The modern build's package P7 (MODERN.md): controller
//           support, the client's half.  The engine's ##gamepad device
//           (src/modern/engine/p7_gamepad.cpp) gives the buttons to the
//           bindings like any key's; this reads the pad itself for what
//           must not depend on them: the sticks (analog look, M-P7-03;
//           analog movement, M-P7-04), the menus' navigation and Start
//           (M-P7-05) and the buttons that close a popup (M-P7-10).  The
//           retail files call into these from #ifdef NOLFSRC_MODERN
//           blocks.
//
//           Compiled only in the modern build (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#ifndef __P7_PAD_H__
#define __P7_PAD_H__

#include "cres/p7.h"

// p7_pad.cpp

void	P7Pad_InitVars();						// with P1Config_InitVars: the Pad* variables
void	P7Pad_ApplyDefaultBindings();			// M-P7-02: defctrls.cfg's ##gamepad lines alone
void	P7Pad_RestoreDefaultBindings();			// M-P7-06: ##gamepad unbound, then the above
void	P7Pad_Update(float fFrameTime);			// CGameClientShell::Update, every frame
void	P7Pad_AddLook(float* pOffsets);			// M-P7-03: the right stick, into the camera's yaw/pitch
void	P7Pad_MoveFlags(uint32* pdwFlags);		// M-P7-04: the left stick's direction and run flags
void	P7Pad_VehicleFlags(uint32* pdwFlags);	// M-P7-04: on a vehicle, its throttle, brake and steering
bool	P7Pad_GetMove(float* pRight, float* pForward);	// the left stick, -1..1, past the dead zone
float	P7Pad_SpeedScale();						// M-P7-04: the share of the walk speed the stick asks for
void	P7Pad_SetCapture(bool bCapture);		// the Controls folder is waiting for a control
bool	P7Pad_AnyButtonDown();					// M-P3-27: a button held, or a trigger past half way
bool	P7Pad_PopupClosePressed();				// M-P7-10: a popup's close button pressed while it was up (once)
bool	P7Pad_PopupButtonDown();				// M-P7-10: a popup's close button held

#endif // __P7_PAD_H__
