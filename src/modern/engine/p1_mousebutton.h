// ----------------------------------------------------------------------- //
//
// MODULE  : p1_mousebutton.h
//
// PURPOSE : Package P1 (MODERN.md), M-P1-33: fixed names for the mouse
//           buttons, in lithtech.exe's input code (clientinput.cpp).
//
//           A binding names a DirectInput object by its name.  The axes
//           and the wheel have names that do not change ("##x-axis",
//           "##y-axis", "##z-axis"), but the retail configs name the
//           buttons by DirectInput's own names ("Button 0" ...).  Those are
//           the names on an English Windows; Windows in another language,
//           or a mouse driver, can give other names.  When the names are different, the button
//           bindings do not attach, the config save drops them, and the
//           mouse cannot fire (issue #59, RETAIL_BUGS.md).
//
//           Here a mouse button also has a fixed name by its number,
//           "##button0" .. "##button7": the number is the button's place
//           among the mouse's buttons, in the order of their DirectInput
//           instance numbers (DIDFT_GETINSTANCE), not its name.  A binding
//           that names a mouse button in one of these ways attaches to it,
//           and the config save writes the fixed name:
//
//             "##button<N>"            the fixed name
//             the button's own name    DirectInput's name, in any language
//             "Button <N>"             the retail name (also "##Button <N>")
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#ifndef __P1_MOUSEBUTTON_H__
#define __P1_MOUSEBUTTON_H__

#include <windows.h>
#include <dinput.h>

// The most buttons a mouse has here (DIMOUSESTATE2's eight).
#define P1_MOUSE_BUTTONS		8

// The number (0 .. P1_MOUSE_BUTTONS - 1) of the button of this mouse that
// pName names, or -1 when it names no button of the mouse.  pRealName, if
// not null, gets the button's DirectInput name (nRealName bytes at most).
int P1Mouse_ButtonNumber(LPDIRECTINPUTDEVICE2A pDevice, const char* pName,
                         char* pRealName, int nRealName);

// The fixed name of button nButton, "##button<N>" (pOut: 16 bytes or more).
void P1Mouse_FixedName(int nButton, char* pOut);

// Is pName a fixed name ("##button" and a number)?  It must not reach the
// retail "##" parse, which reads "##button2" as instance 0 (atoi), the X
// axis.
BOOL P1Mouse_IsFixedName(const char* pName);

#ifdef NOLFSRC_DEBUG
// Debug build only, for the test of M-P1-33: with NOLFSRC_TEST_MOUSENAMES
// set to a printf format (for example "Taste %d"), the system mouse's
// buttons have the names that format gives, by button number, as on a
// Windows in another language.  pDevice is the created device; the result
// is that device, or a wrapper around it (it takes the reference).
LPDIRECTINPUTDEVICEA P1Mouse_TestWrap(REFGUID guid, LPDIRECTINPUTDEVICEA pDevice);
#endif

#endif // __P1_MOUSEBUTTON_H__
