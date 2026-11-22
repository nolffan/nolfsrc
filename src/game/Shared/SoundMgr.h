// ----------------------------------------------------------------------- //
//
// MODULE  : SoundMgr.h
//
// PURPOSE : SoundMgr definition - Controls sound
//
// CREATED : 2/08/99
//
// (c) 1999-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#ifndef __SOUND_MGR_H__
#define __SOUND_MGR_H__

#include "SoundTypes.h"
#include "GameButeMgr.h"
#include "ltbasedefs.h"

// #defines...

#define SMGR_DEFAULT_VOLUME		100

#ifdef NOLFSRC_MODERN
// Dialogue IDs that are text only (MODERN.md M-P5-25, issue #65). A
// dialogue ID gives both a line of text and the voice file
// Voice\<id>.wav. These IDs have text but no voice file in any release:
//
// - 9000-9017, IDS_TEXT_DIALOGUE_9000..9017 (src/game/ClientRes/ClientRes.rc):
//   Cate's choices and silent replies (". . .", "(Question one.)",
//   "(Torture him.)", "(What now?)" and others).
// - 4000, 4001 and 4008 (IDS_WEAPON_1, IDS_WEAPON_2, IDS_WEAPON_9) and 37
//   (IDS_NONE): weapon names. The choices of the weapon menu in T01S03 and
//   T08S01 ("Decisions1"), which the player "says".
//
// The game does not play a voice file for these IDs. A voice file that
// is missing for any other ID is still reported.
#define SMGR_TEXT_DIALOGUE_FIRST	9000	// IDS_TEXT_DIALOGUE_9000
#define SMGR_TEXT_DIALOGUE_LAST		9017	// IDS_TEXT_DIALOGUE_9017

inline bool IsTextOnlyDialogueId(uint32 dwId)
{
	if (dwId >= SMGR_TEXT_DIALOGUE_FIRST && dwId <= SMGR_TEXT_DIALOGUE_LAST) return true;

	// The weapon menu's choices (ClientRes.h: IDS_WEAPON_1, IDS_WEAPON_2,
	// IDS_WEAPON_9, IDS_NONE).
	return (dwId == 4000 || dwId == 4001 || dwId == 4008 || dwId == 37);
}
#endif

class CGameSoundMgr : public CGameButeMgr
{
	public :

        virtual LTBOOL Init(ILTCSBase *pInterface, const char* szAttributeFile);

        HLTSOUND    PlaySoundFromObject(HOBJECT hObject, char *pName, LTFLOAT fRadius,
            SoundPriority ePriority, uint32 dwFlags=0, uint8 nVolume=SMGR_DEFAULT_VOLUME, float fPitchShift=1.0f);

        HLTSOUND    PlaySoundFromPos(LTVector & vPos, char *pName, LTFLOAT fRadius,
            SoundPriority ePriority, uint32 dwFlags=0, uint8 nVolume=SMGR_DEFAULT_VOLUME, float fPitchShift=1.0f);

		CString		GetRandomSoundFilename(const char* pTag, const char* pAttributeBase);
		CString		GetSoundFilename(const char* pTag, const char* pAttribute);
        CString     GetSoundFilenameFromId(const char* pPathAttribute, uint32 dwId);
		uint32		GetSoundIdFromFilename(const char* pSoundFile);

	protected :

        CGameSoundMgr();
        virtual ~CGameSoundMgr();

		virtual	HLTSOUND PlaySound(PlaySoundInfo & playSoundInfo) = 0;
		virtual	LTVector GetObjectPos(HOBJECT hObj) = 0;

		ILTCSBase*	m_pInterface;
        LTFLOAT		m_fInnerRadiusPercent;  // Percent of outer radius used for inner radius
};

#endif // __SOUND_MGR_H__