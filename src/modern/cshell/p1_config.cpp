// ----------------------------------------------------------------------- //
//
// MODULE  : p1_config.cpp
//
// PURPOSE : Package P1 (MODERN.md): the console variables P1 adds, and
//           the steps it takes when the client shell starts.
//
//             FramerateLimit     240  M-P1-25  the client's frames a second at
//                                           most (0: no limit; 60 always in
//                                           multiplayer)
//             OldMouseLook         0  M-P1-03  the retail mouse look
//             NoRawInput           0  M-P1-03  cursor motion, not raw input
//             EnableScreenTinting  1  M-P1-10  0: tint with a 2D overlay
//             EnableLightScale     1  M-P1-10  0: no light scaling
//             ModPatchNum          0  M-P1-12  config version last seen
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "VarTrack.h"
#include "ClientUtilities.h"
#include "CommandIDs.h"
#include "cshell/p1_modern.h"
#include "cshell/p7_pad.h"

namespace
{
	VarTrack	s_vtFramerateLimit;
	VarTrack	s_vtOldMouseLook;
	VarTrack	s_vtNoRawInput;
	VarTrack	s_vtScreenTinting;
	VarTrack	s_vtLightScale;
	VarTrack	s_vtModPatchNum;

	// A variable read before OnEngineInitialized created it reads as its
	// default.
	bool ReadFlag(VarTrack& vt, bool bDefault)
	{
		if (!vt.IsInitted())
			return bDefault;
		return vt.GetFloat() != 0.0f;
	}

	// Unbind every control of every device, so that reading defctrls.cfg
	// afterwards leaves exactly the default bindings (what Controls' own
	// "restore defaults" does).
	void UnbindAll()
	{
		static const uint32 s_aDevices[3] =
		{
			DEVICETYPE_KEYBOARD,
			DEVICETYPE_MOUSE,
			DEVICETYPE_JOYSTICK
		};

		for (int i = 0; i < 3; i++)
		{
			DeviceBinding* pBindings = g_pLTClient->GetDeviceBindings(s_aDevices[i]);
			DeviceBinding* pBinding;

			for (pBinding = pBindings; pBinding; pBinding = pBinding->pNext)
			{
				char szCmd[160];

				// ';' cannot appear in a console string: name it by code
				if (pBinding->strTriggerName[0] == ';')
					_snprintf(szCmd, sizeof(szCmd), "rangebind \"%s\" \"##39\" 0 0 \"\"",
						pBinding->strDeviceName);
				else
					_snprintf(szCmd, sizeof(szCmd), "rangebind \"%s\" \"%s\" 0 0 \"\"",
						pBinding->strDeviceName, pBinding->strTriggerName);
				szCmd[sizeof(szCmd) - 1] = '\0';

				g_pLTClient->RunConsoleString(szCmd);
			}

			if (pBindings)
				g_pLTClient->FreeDeviceBindings(pBindings);
		}
	}

	// M-P4-14: the crosshair toggle (CursorTog) has no key by default now;
	// it was on X. Unbind X only while it is exactly the old default: the
	// one key that does that and nothing else. A player who moved the
	// toggle to another key, or put X to another use, keeps the choice.
	void UnbindOldCrosshairKey()
	{
		DeviceBinding* pBindings = g_pLTClient->GetDeviceBindings(DEVICETYPE_KEYBOARD);
		DeviceBinding* pBinding;
		bool bOldDefault = false;

		for (pBinding = pBindings; pBinding; pBinding = pBinding->pNext)
		{
			GameAction* pAction = pBinding->pActionHead;

			if (_stricmp(pBinding->strTriggerName, "X") && _stricmp(pBinding->strTriggerName, "##45"))
				continue;

			if (pAction && !pAction->pNext && pAction->nActionCode == COMMAND_ID_CROSSHAIRTOGGLE)
				bOldDefault = true;
		}

		if (pBindings)
			g_pLTClient->FreeDeviceBindings(pBindings);

		if (bOldDefault)
			g_pLTClient->RunConsoleString("rangebind \"##keyboard\" \"##45\" 0 0 \"\"");
	}
}

// Called from OnEngineInitialized with the shell's other variables, before
// the first render mode is set.
void P1Config_InitVars()
{
	s_vtFramerateLimit.Init(g_pLTClient, "FramerateLimit", LTNULL, 240.0f);
	s_vtOldMouseLook.Init(g_pLTClient, "OldMouseLook", LTNULL, 0.0f);
	s_vtNoRawInput.Init(g_pLTClient, "NoRawInput", LTNULL, 0.0f);
	s_vtScreenTinting.Init(g_pLTClient, "EnableScreenTinting", LTNULL, 1.0f);
	s_vtLightScale.Init(g_pLTClient, "EnableLightScale", LTNULL, 1.0f);
	s_vtModPatchNum.Init(g_pLTClient, "ModPatchNum", LTNULL, 0.0f);
	P7Pad_InitVars();

	// M-P1-08: with surface optimisation off, text is drawn in black boxes,
	// and the setting can end up off in a config.
	g_pLTClient->RunConsoleString("optimizesurfaces 1");
}

// Called at the end of OnEngineInitialized.
void P1Config_AfterInit()
{
	// M-P1-08: a CursorCenter left on by the retail mouse look fights the
	// new one.  P1Input_UseCursor turns it back on when OldMouseLook asks.
	g_pLTClient->RunConsoleString("CursorCenter 0");

	// M-P1-12: one-time changes for configs older than this build.
	float fSeen = s_vtModPatchNum.IsInitted() ? s_vtModPatchNum.GetFloat() : 0.0f;
	if (fSeen >= P1_CONFIG_VERSION)
		return;

	if (fSeen < 1.0f)
	{
		// More bullet holes stay on the walls.
		if (GetConsoleFloat("BulletHoles", 0.0f) < 100.0f)
			WriteConsoleFloat("BulletHoles", 100.0f);

		// The modern build's default controls (defctrls.cfg) replace the
		// bindings, as Controls' "restore defaults" would.
		if (g_pLTClient->ReadConfigFile("defctrls.cfg") != LT_ERROR)
		{
			UnbindAll();
			g_pLTClient->ReadConfigFile("defctrls.cfg");
		}
	}

	if (fSeen < 1.1f)
	{
		// The modern build's d3d.ren is d3d7.ren now, and its default
		// renderer d3d9.ren (which the engine already loaded for a config
		// naming d3d.ren): say so in the config.
		HCONSOLEVAR hDll = g_pLTClient->GetConsoleVar("RenderDll");
		const char* sDll = hDll ? g_pLTClient->GetVarValueString(hDll) : LTNULL;
		if (sDll && !_stricmp(sDll, "d3d.ren"))
			g_pLTClient->RunConsoleString("RenderDll d3d9.ren");
	}

	if (fSeen >= 1.0f && fSeen < 1.2f)
	{
		// M-P7-02: the controller's default bindings, alone (a config
		// from before 1.0 has just had all of defctrls.cfg): the
		// keyboard's and the mouse's are the player's by now.
		P7Pad_ApplyDefaultBindings();
	}

	if (fSeen < 1.3f)
	{
		// M-P1-22: a window as large as the desktop was borderless by
		// itself; it is the window mode "borderless window" now.
		int nDeskW, nDeskH;
		P1Window_DesktopSize(&nDeskW, &nDeskH);
		if (GetConsoleInt("Windowed", 0) == 1 &&
			GetConsoleInt("screenwidth", 0) >= nDeskW && GetConsoleInt("screenheight", 0) >= nDeskH)
			P1Window_SetMode(P1_WINDOW_BORDERLESS);
	}

	if (fSeen < 1.4f)
	{
		// M-P1-24: 32-bit textures by default (Display options' texture
		// depth); defaults.cfg says so to a new config, this to an old one.
		WriteConsoleInt("32BitTextures", 1);
	}

	if (fSeen < 1.5f)
	{
		// M-P1-25: the frame-rate cap is a number now (FramerateLimit);
		// FramerateLock 0 was "unlimited".
		if (GetConsoleInt("FramerateLock", 1) == 0)
			WriteConsoleInt("FramerateLimit", 0);
	}

	if (fSeen < 1.6f)
	{
		// M-P6-16: multiplayer updates 30 times a second each way; a
		// config of 2001's defaults (UpdateRate 6, Player setup's
		// slowest connections) had the other players skipping
		if (GetConsoleInt("UpdateRate", 30) < 30)
			WriteConsoleInt("UpdateRate", 30);
		if (GetConsoleFloat("CSendRate", 30.0f) < 30.0f)
			WriteConsoleFloat("CSendRate", 30.0f);
	}

	if (fSeen < 1.7f)
	{
		// M-P4-14: X toggled the crosshair off by accident (issue #67);
		// the toggle has no key by default now.
		UnbindOldCrosshairKey();
	}

	WriteConsoleFloat("ModPatchNum", P1_CONFIG_VERSION);
	g_pLTClient->WriteConfigFile("autoexec.cfg");
}

int P1Config_FramerateLimit()
{
	int nLimit = s_vtFramerateLimit.IsInitted() ? (int)s_vtFramerateLimit.GetFloat() : 240;
	return nLimit < 0 ? 0 : nLimit;
}
bool P1Config_OldMouseLook()	{ return ReadFlag(s_vtOldMouseLook, false); }
bool P1Config_NoRawInput()		{ return ReadFlag(s_vtNoRawInput, false); }
// M-P1-10: the Intel graphics fix is about Intel's DirectX 7 driver, which
// only d3d7.ren (our d3d.ren) draws through; d3d9.ren and opengl.ren draw
// light scaling and tints themselves.  So it applies with d3d7.ren alone.
bool P1Config_IntelFixRenderer(const char* pRenderDll)
{
	return pRenderDll && (!_stricmp(pRenderDll, "d3d7.ren") || !_stricmp(pRenderDll, "d3d.ren"));
}

bool P1Config_IntelFixOn()
{
	return !ReadFlag(s_vtLightScale, true) || !ReadFlag(s_vtScreenTinting, true);
}

namespace
{
	bool IntelFixApplies()
	{
		RMode mode;
		return g_pLTClient->GetRenderMode(&mode) == LT_OK && P1Config_IntelFixRenderer(mode.m_RenderDLL);
	}
}

bool P1Config_ScreenTinting()	{ return ReadFlag(s_vtScreenTinting, true) || !IntelFixApplies(); }
bool P1Config_LightScale()		{ return ReadFlag(s_vtLightScale, true) || !IntelFixApplies(); }
