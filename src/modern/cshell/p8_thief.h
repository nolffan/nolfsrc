// ----------------------------------------------------------------------- //
//
// MODULE  : p8_thief.h
//
// PURPOSE : P8's client call-outs: the PS2 "Nine years ago" levels
//           (NOLFSRC_MODERN only)
//
// ----------------------------------------------------------------------- //

#ifndef __P8_THIEF_H__
#define __P8_THIEF_H__

// LoadWorld, once the mission and level are known (nLevel: -1 for a custom
// level). In a thief level (Worlds\thiefmission*) the loading screen shows
// "Nine years ago..." and the thief photo instead of the mission's (M-P8-04).
void P8_OnLoadWorld(const char* pWorldFile, int nLevel);

// The first-person hands skin of a thief level (M-P8-04), or LTNULL.
const char* P8_GetHandsSkin();

#endif // __P8_THIEF_H__
