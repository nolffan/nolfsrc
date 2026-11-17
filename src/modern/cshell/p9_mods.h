// ----------------------------------------------------------------------- //
//
// MODULE  : p9_mods.h
//
// PURPOSE : The modern build's support for mods made for the NOLF
//           Modernizer (package P9 of MODERN.md): the client side of the
//           Thief mod's levels and its PS2 soundtrack. Compiled only with
//           NOLFSRC_MODERN.
//
// ----------------------------------------------------------------------- //

#ifndef __P9_MODS_H__
#define __P9_MODS_H__

// Called once from CGameClientShell::OnEngineInitialized: creates P9's
// console variables.
void		P9_OnEngineInitialized();

// LoadWorld, once the mission and level are known (nLevel: -1 for a custom
// level). M-P9-06: a thief level's loading screen; M-P9-08: notes the world
// for its PS2 track.
void		P9_OnLoadWorld(const char* pWorldFile, int nLevel);

// M-P9-08: the PS2 soundtrack, the mod's Music\PS2Music\*.wav by
// ATTRIBUTES\LevelMusic.txt, in place of the level's DirectMusic when
// UsePS2Music is set.
LTBOOL		P9_HavePS2Music();				// the mod's LevelMusic.txt is in the data
LTBOOL		P9_StartLevelMusic();			// FirstUpdate: TRUE if a PS2 track now plays
LTBOOL		P9_IsPS2MusicPlaying();			// (the server's music messages are then ignored)
void		P9_StopLevelMusic();			// OnExitWorld
void		P9_OnMusicSettingsChanged();	// Sound options: UsePS2Music or the music volume

// M-P9-09: the main menu can show young Cate: a mod brings her menu model
// (Chars\Models\IntHero_Thief.abc, the IntHero model in the "Thief" style).
LTBOOL		P9_HaveThiefMenu();

// M-P9-08: a WAV of the mod's jukebox (a Music\ path), looped at the music
// volume; LTNULL stops the one playing.
LTBOOL		P9_PlayJukeboxWAV(const char* pFile);

#endif // __P9_MODS_H__
