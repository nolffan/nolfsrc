// ----------------------------------------------------------------------- //
//
// MODULE  : p2_client.h
//
// PURPOSE : P2's call-outs from GameClientShell.cpp and LoadingScreen.cpp
//           (NOLFSRC_MODERN only)
//
// ----------------------------------------------------------------------- //
//
// GameClientShell.cpp is shared by several packages (MODERN.md, hot spots):
// P2 reaches it only through these calls, each in its own #ifdef block.

#ifndef __P2_CLIENT_H__
#define __P2_CLIENT_H__

// OnEngineInitialized, once the interface is up: the console (M-P2-01..06,
// M-P0-02), BigHeadMode (M-P2-10), the speed-run block (M-P2-12).
void P2_OnEngineInitialized();

// OnEngineTerm, first thing: everything the engine could call back into.
void P2_OnEngineTerm();

// OnEnterWorld: single player or host -- tell the server our BigHeadMode.
void P2_OnEnterWorld();

// Every client frame (Update).
void P2_Update();

// CLoadingScreen::Show / Hide (M-P2-03 lock, M-P2-12 is-loading flag).
void P2_OnLoadingScreen(LTBOOL bShown);

// M-P3-27: the level is loaded, and the loading screen only waits for a
// key: the is-loading flag goes to 0 before the screen goes away.
void P2_OnLoadingDone();

// LTTRUE: the developer console took this command (GameClientShell's
// OnCommandOn returns at once).
LTBOOL P2_OnCommandOn(int nCommand);

#endif // __P2_CLIENT_H__
