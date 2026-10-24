// p8.h -- the modern build's P8 strings (the PS2 release's levels):
// IDs 31700-31799 (MODERN.md). Used only by the modern build.
//
// The menu strings (31740 on) are ours, in p8.rc. The rest is the PS2
// disc's text: build/ps2/ps2strings.py writes it (with the
// PS2 strings that keep their own IDs: 5326, 6535-6537, 14001-14074) into a
// generated, uncommitted .rc at build time, as build/retailextract.py does
// for the Game of the Year strings. Map: build/ps2/ps2-string-map.txt.

#ifndef __MODERN_CRES_P8_H__
#define __MODERN_CRES_P8_H__

// loading screen of a thiefmission* level (PS2 239)
#define IDS_P8_NINE_YEARS_AGO           31700

// FolderIntel names of the new intel types (PS2 667-669; "%1!d!")
#define IDS_P8_INTEL_MONEYBAG           31701
#define IDS_P8_INTEL_HANDBAG            31702
#define IDS_P8_INTEL_MONEY              31703

// WEAPONS.txt [Weapon45] / [Weapon46] / [Ammo54] (PS2 4033/4083, 4034/4084, 4146/4346)
#define IDS_P8_WEAPON_SAFECRACKER       31710
#define IDS_P8_WEAPON_SAFECRACKER_DESC  31711
#define IDS_P8_WEAPON_LOCKPICK          31712
#define IDS_P8_WEAPON_LOCKPICK_DESC     31713
#define IDS_P8_AMMO_SAFECRACKER         31714
#define IDS_P8_AMMO_SAFECRACKER_DESC    31715

// objectives (PS2 5047, 5048, 5089, 5212-5216), used by the converted worlds
#define IDS_P8_OBJ_TM1_HANDBAGS         31720
#define IDS_P8_OBJ_TM1_FLAT             31721
#define IDS_P8_OBJ_TM2_SAFES            31722
#define IDS_P8_OBJ_TM3_DIAMOND          31723
#define IDS_P8_OBJ_TM3_TRASH            31724
#define IDS_P8_OBJ_TM3_REPLACE          31725
#define IDS_P8_OBJ_TM3_CASE             31726
#define IDS_P8_OBJ_TM3_EXIT             31727

// intelligence text (PS2 20541-20547), Intelligence InfoId in the converted worlds
#define IDS_P8_INTEL_TEXT_FIRST         31730
#define IDS_P8_INTEL_TEXT_LAST          31736

// M-P8-10: the main menu's Cate (Interface options)
#define IDS_P8_MENU_CATE                31740
#define IDS_P8_HELP_MENU_CATE           31741
#define IDS_P8_MENU_CATE_SEASONAL       31742
#define IDS_P8_MENU_CATE_ORIGINAL       31743
#define IDS_P8_MENU_CATE_CASUAL         31744
#define IDS_P8_MENU_CATE_WINTER         31745
#define IDS_P8_MENU_CATE_YOUNG          31746

#endif // __MODERN_CRES_P8_H__
