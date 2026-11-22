// ----------------------------------------------------------------------- //
//
// MODULE  : p3_loadprompt.h
//
// PURPOSE : M-P3-27 (MODERN.md): the loading screen stays up after a
//           level has loaded, with "Press any key to continue.", until
//           the player presses a key, the left mouse button or a
//           controller button -- as No One Lives Forever 2 and Contract
//           J.A.C.K. do (their CScreenPostload).  Compiled only in the
//           modern build (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //
//
// NOLF2 and Contract J.A.C.K. (ScreenPostload.cpp, InterfaceMgr.cpp's
// UpdateLoadingLevelState, LoadingScreen.cpp's NeedsPostLoadScreen):
//  - single player only (their multiplayer screen waits for the server,
//    not for a key; NOLF1 has no co-operative mode);
//  - only a new level, not a saved game (their briefing, which the prompt
//    goes with, is skipped when IsRestoringLevel); no prompt on a custom
//    level (it has no briefing);
//  - any key, or the left mouse button; no time-out;
//  - the console variable LoadScreenWait (default 1) turns it off;
//  - the prompt flashes, its colour and the inverse in turn, every 0.333 s;
//  - the game does not run meanwhile (their server stays paused until the
//    client says it is loaded).

#ifndef __P3_LOADPROMPT_H__
#define __P3_LOADPROMPT_H__

class CLoadingScreen;

// CGameClientShell::DoLoadWorld, before the loading state: whether this
// load is one to wait for a key after (a new single player mission level,
// not a saved game, a custom level or a demo).
void	P3LoadPrompt_OnLoadWorld(LTBOOL bNewMissionLevel);

// CInterfaceMgr::UpdateLoadingLevelState, once the player is in the
// world: LTTRUE while the loading screen is to stay up (it is drawn
// here); LTFALSE to go on into play as retail does.
LTBOOL	P3LoadPrompt_Wait(CLoadingScreen & screen);

// CGameClientShell::OnCommandOn: LTTRUE while the prompt waits.  The
// player is in the world, so the engine gives the bound commands (Activate
// would skip the level's first cinematic once the game runs again).
LTBOOL	P3LoadPrompt_IsWaiting();

// CLoadingScreen::Update: the prompt, while it waits.
void	P3LoadPrompt_Draw(HSURFACE hDest);

#endif // __P3_LOADPROMPT_H__
