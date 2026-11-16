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
// level): notes whether it is a thief level (Worlds\thiefmission*), for its
// hands (M-P8-04). Its loading screen is P9_OnLoadWorld's (M-P9-06).
void P8_OnLoadWorld(const char* pWorldFile, int nLevel);

// The first-person hands skin of a thief level (M-P8-04), or LTNULL.
const char* P8_GetHandsSkin();

#endif // __P8_THIEF_H__
