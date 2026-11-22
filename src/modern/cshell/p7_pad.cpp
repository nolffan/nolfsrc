// ----------------------------------------------------------------------- //
//
// MODULE  : p7_pad.cpp
//
// PURPOSE : Package P7 (MODERN.md): the controller, the client's half.
//           See p7_pad.h.
//
//           The pad is read once a frame (P7Pad_Update), while the game's
//           window is the foreground one, through engine/p7_padread.h --
//           the same reader the engine's ##gamepad device uses, so the
//           test pad (NOLFSRC_PADTEST) drives both.
//
//           Look (M-P7-03): the right stick past a radial dead zone
//           (PadLookDeadZone), the rest of its travel rescaled to 0..1
//           and raised to PadLookCurve (fine aim near the centre), is a
//           turn rate: 180 degrees a second across, 120 up and down, times
//           PadLookSpeed, times the frame time.  It is added to the
//           mouse's yaw and pitch offsets, so it slows with the zoom as
//           they do; PadInvertY turns the stick's pitch over (the mouse's
//           own invert applies to both).
//
//           Movement (M-P7-04): the left stick past PadMoveDeadZone is a
//           direction and a deflection.  The direction sets the movement
//           flags (by octant: what the animations and the server see) and
//           the acceleration; the deflection sets the speed: past
//           three-quarters it runs (the Speed command, held, turns that
//           over, as Shift does the keyboard's run), below it walks at a
//           share of the walk speed, never less than a third.
//
//           On a vehicle (the motorcycle, the snowmobile) the stick is
//           its throttle and brake (up, down) and steering (left, right),
//           the flags VehicleMgr reads; boost is the Speed button's still.
//
//           Menus (M-P7-05): outside play, the d-pad or the left stick is
//           the arrow keys (repeating while held), A is Enter, B and
//           Start are Escape; in play Start is Escape (the menu), and A,
//           in a cut-scene or on the death screen, Space (skip, go on).  The
//           keys go through the client shell's own OnKeyDown / OnKeyUp
//           (IClientShell's), as the engine delivers a key.  Not while the Controls folder
//           is waiting for a control to bind.
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "GameClientShell.h"
#include "InterfaceMgr.h"
#include "CommandIDs.h"
#include "SharedMovement.h"
#include "VarTrack.h"
#include "cshell/p7_pad.h"
#include "engine/p7_padread.h"

#include <math.h>

extern CGameClientShell*	g_pGameClientShell;
extern HWND					g_hMainWnd;			// GameClientShell.cpp

namespace
{
	VarTrack	s_vtLookSpeed;
	VarTrack	s_vtLookCurve;
	VarTrack	s_vtLookDeadZone;
	VarTrack	s_vtMoveDeadZone;
	VarTrack	s_vtInvertY;

	const float	c_fYawRate		= 3.14159265f;			// 180 degrees a second
	const float	c_fPitchRate	= 3.14159265f * 2 / 3;	// 120
	const float	c_fRunAt		= 0.75f;				// of the stick's travel past the dead zone
	const float	c_fMinWalk		= 1.0f / 3;
	const float	c_fRepeatDelay	= 0.4f;					// menu keys: first repeat, then every
	const float	c_fRepeatRate	= 0.12f;

	P7PadState	s_Pad;
	bool		s_bPad;									// connected, and the game has the foreground
	float		s_fFrameTime;
	float		s_fMoveX, s_fMoveY, s_fMoveMag;			// past the dead zone; 0 when not moving
	float		s_fLookX, s_fLookY;
	bool		s_bCapture;

	// The menu keys: what is held, and the repeat timer.
	WORD		s_wPrevButtons;
	int			s_nHeldKey;
	float		s_fHeldTime;
	float		s_fNextRepeat;

	float ReadVar(VarTrack& vt, float fDefault)
	{
		return vt.IsInitted() ? vt.GetFloat() : fDefault;
	}

	// A stick, -32768..32767 a side (y up positive), past a radial dead
	// zone: its direction kept, its length rescaled from the dead zone's
	// edge to 1.  Returns the length.
	float Stick(SHORT nX, SHORT nY, float fDeadZone, float* pX, float* pY)
	{
		float fX = nX / 32767.0f, fY = nY / 32767.0f;
		float fLen = (float)sqrt(fX * fX + fY * fY);
		float fScaled;

		if (fDeadZone < 0.0f) fDeadZone = 0.0f;
		if (fDeadZone > 0.9f) fDeadZone = 0.9f;

		if (fLen <= fDeadZone)
		{
			*pX = *pY = 0.0f;
			return 0.0f;
		}

		fScaled = (fLen - fDeadZone) / (1.0f - fDeadZone);
		if (fScaled > 1.0f)
			fScaled = 1.0f;

		*pX = fX / fLen * fScaled;
		*pY = fY / fLen * fScaled;
		return fScaled;
	}

	// Through IClientShell, where the engine calls them (public there).
	void PressKey(int nKey)
	{
		IClientShell* pShell = g_pGameClientShell;

		pShell->OnKeyDown(nKey, 1);
		pShell->OnKeyUp(nKey);
	}

	bool InPlay()
	{
		return g_pInterfaceMgr && g_pInterfaceMgr->GetGameState() == GS_PLAYING;
	}

	// The arrow key the d-pad or the left stick holds, or 0.
	int MenuDirection()
	{
		float fX, fY;

		if (s_Pad.wButtons & P7PAD_DPAD_UP)		return VK_UP;
		if (s_Pad.wButtons & P7PAD_DPAD_DOWN)	return VK_DOWN;
		if (s_Pad.wButtons & P7PAD_DPAD_LEFT)	return VK_LEFT;
		if (s_Pad.wButtons & P7PAD_DPAD_RIGHT)	return VK_RIGHT;

		if (Stick(s_Pad.sThumbLX, s_Pad.sThumbLY, 0.5f, &fX, &fY) <= 0.0f)
			return 0;

		if (fabs(fY) >= fabs(fX))
			return fY > 0.0f ? VK_UP : VK_DOWN;

		return fX > 0.0f ? VK_RIGHT : VK_LEFT;
	}

	void UpdateMenus(WORD wPressed)
	{
		int nKey;

		if (InPlay())
		{
			if (wPressed & P7PAD_START)
				PressKey(VK_ESCAPE);

			// A skips a cut-scene, and goes on from the death screen: both
			// take Space (any key, for the second), and A's own binding
			// (Jump) is a command, not a key.
			if ((wPressed & P7PAD_A) &&
				(g_pGameClientShell->IsUsingExternalCamera() || g_pGameClientShell->IsPlayerDead()))
				PressKey(VK_SPACE);

			s_nHeldKey = 0;
			return;
		}

		if (s_bCapture)
		{
			s_nHeldKey = 0;
			return;
		}

		if (wPressed & P7PAD_A)
			PressKey(VK_RETURN);

		if (wPressed & (P7PAD_B | P7PAD_START))
			PressKey(VK_ESCAPE);

		nKey = MenuDirection();

		if (nKey != s_nHeldKey)
		{
			s_nHeldKey    = nKey;
			s_fHeldTime   = 0.0f;
			s_fNextRepeat = c_fRepeatDelay;

			if (nKey)
				PressKey(nKey);
		}
		else if (nKey)
		{
			s_fHeldTime += s_fFrameTime;

			if (s_fHeldTime >= s_fNextRepeat)
			{
				s_fNextRepeat += c_fRepeatRate;
				PressKey(nKey);
			}
		}
	}
}

void P7Pad_InitVars()
{
	s_vtLookSpeed.Init(g_pLTClient, "PadLookSpeed", LTNULL, 1.0f);
	s_vtLookCurve.Init(g_pLTClient, "PadLookCurve", LTNULL, 2.0f);
	s_vtLookDeadZone.Init(g_pLTClient, "PadLookDeadZone", LTNULL, 0.15f);
	s_vtMoveDeadZone.Init(g_pLTClient, "PadMoveDeadZone", LTNULL, 0.2f);
	s_vtInvertY.Init(g_pLTClient, "PadInvertY", LTNULL, 0.0f);
}

void P7Pad_ApplyDefaultBindings()
{
	FILE* pFile = fopen("defctrls.cfg", "r");
	char  szLine[256];

	if (!pFile)
		return;

	while (fgets(szLine, sizeof(szLine), pFile))
	{
		char* p = szLine + strlen(szLine);

		while (p > szLine && (p[-1] == '\n' || p[-1] == '\r'))
			*--p = '\0';

		if (strstr(szLine, "\"##gamepad\""))
			g_pLTClient->RunConsoleString(szLine);
	}

	fclose(pFile);
}

void P7Pad_RestoreDefaultBindings()
{
	DeviceBinding* pBindings = g_pLTClient->GetDeviceBindings(DEVICETYPE_JOYSTICK);
	DeviceBinding* pBinding;

	for (pBinding = pBindings; pBinding; pBinding = pBinding->pNext)
	{
		char szCmd[160];

		if (_stricmp(pBinding->strDeviceName, "##gamepad"))
			continue;

		_snprintf(szCmd, sizeof(szCmd), "rangebind \"##gamepad\" \"%s\" 0 0 \"\"",
			pBinding->strTriggerName);
		szCmd[sizeof(szCmd) - 1] = '\0';
		g_pLTClient->RunConsoleString(szCmd);
	}

	if (pBindings)
		g_pLTClient->FreeDeviceBindings(pBindings);

	P7Pad_ApplyDefaultBindings();
}

void P7Pad_Update(float fFrameTime)
{
	WORD wPressed;

	s_fFrameTime = fFrameTime;

	s_bPad = P7PadRead_Available() && GetForegroundWindow() == g_hMainWnd &&
	         P7PadRead_State(&s_Pad);

	if (!s_bPad)
		memset(&s_Pad, 0, sizeof(s_Pad));

	wPressed       = s_Pad.wButtons & ~s_wPrevButtons;
	s_wPrevButtons = s_Pad.wButtons;

	if (InPlay())
	{
		s_fMoveMag = Stick(s_Pad.sThumbLX, s_Pad.sThumbLY, ReadVar(s_vtMoveDeadZone, 0.2f),
		                   &s_fMoveX, &s_fMoveY);
		Stick(s_Pad.sThumbRX, s_Pad.sThumbRY, ReadVar(s_vtLookDeadZone, 0.15f), &s_fLookX, &s_fLookY);
	}
	else
	{
		s_fMoveX = s_fMoveY = s_fMoveMag = 0.0f;
		s_fLookX = s_fLookY = 0.0f;
	}

	UpdateMenus(wPressed);
}

void P7Pad_AddLook(float* pOffsets)
{
	float fLen = (float)sqrt(s_fLookX * s_fLookX + s_fLookY * s_fLookY);
	float fScale, fCurve;

	if (fLen <= 0.0f)
		return;

	// The curve on the length, so the direction stays the stick's.
	fCurve = ReadVar(s_vtLookCurve, 2.0f);
	if (fCurve < 1.0f)
		fCurve = 1.0f;

	fScale = (float)pow(fLen, fCurve) / fLen * ReadVar(s_vtLookSpeed, 1.0f) * s_fFrameTime;

	pOffsets[0] += s_fLookX * fScale * c_fYawRate;

	// The stick's up is the camera's pitch down (negative) unless inverted.
	if (ReadVar(s_vtInvertY, 0.0f) != 0.0f)
		pOffsets[1] += s_fLookY * fScale * c_fPitchRate;
	else
		pOffsets[1] -= s_fLookY * fScale * c_fPitchRate;
}

bool P7Pad_AnyButtonDown()
{
	return s_Pad.wButtons != 0 || s_Pad.bLeftTrigger > 128 || s_Pad.bRightTrigger > 128;
}

bool P7Pad_GetMove(float* pRight, float* pForward)
{
	if (s_fMoveMag <= 0.0f)
		return false;

	*pRight   = s_fMoveX;
	*pForward = s_fMoveY;
	return true;
}

void P7Pad_MoveFlags(uint32* pdwFlags)
{
	const float c_fOctant = 0.3827f;		// sin(22.5 degrees)
	bool bRun;

	if (s_fMoveMag <= 0.0f)
		return;

	if (s_fMoveY >  c_fOctant * s_fMoveMag) *pdwFlags |= BC_CFLG_FORWARD;
	if (s_fMoveY < -c_fOctant * s_fMoveMag) *pdwFlags |= BC_CFLG_REVERSE;
	if (s_fMoveX >  c_fOctant * s_fMoveMag) *pdwFlags |= BC_CFLG_STRAFE_RIGHT;
	if (s_fMoveX < -c_fOctant * s_fMoveMag) *pdwFlags |= BC_CFLG_STRAFE_LEFT;

	// The deflection decides walking and running, not RunLock; Speed held
	// turns it over.
	bRun = s_fMoveMag >= c_fRunAt;
	if (g_pLTClient->IsCommandOn(COMMAND_ID_RUN))
		bRun = !bRun;

	if (bRun)
		*pdwFlags |= BC_CFLG_RUN;
	else
		*pdwFlags &= ~BC_CFLG_RUN;
}

void P7Pad_VehicleFlags(uint32* pdwFlags)
{
	const float c_fOctant = 0.3827f;		// sin(22.5 degrees)

	if (s_fMoveMag <= 0.0f)
		return;

	// Forward and reverse as VehicleMgr has them: reverse wins.
	if (s_fMoveY < -c_fOctant * s_fMoveMag)
	{
		*pdwFlags |= BC_CFLG_REVERSE;
		*pdwFlags &= ~BC_CFLG_FORWARD;
	}
	else if (s_fMoveY > c_fOctant * s_fMoveMag)
	{
		*pdwFlags |= BC_CFLG_FORWARD;
	}

	if (s_fMoveX >  c_fOctant * s_fMoveMag) *pdwFlags |= BC_CFLG_RIGHT;
	if (s_fMoveX < -c_fOctant * s_fMoveMag) *pdwFlags |= BC_CFLG_LEFT;
}

float P7Pad_SpeedScale()
{
	float fScale;

	if (s_fMoveMag <= 0.0f || s_fMoveMag >= c_fRunAt)
		return 1.0f;

	fScale = s_fMoveMag / c_fRunAt;
	return fScale < c_fMinWalk ? c_fMinWalk : fScale;
}

void P7Pad_SetCapture(bool bCapture)
{
	s_bCapture = bCapture;
}
