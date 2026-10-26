// *********************************************************************** //
//
//  MODULE:   DLLLOAD (.CPP)  -- scratch reconstruction, lithtech.exe only
//
//  PURPOSE:  What the survey of 0x0042da50..0x00430da7 found, and the part
//            of it that can be written down with the shipped bytes behind
//            every line.
//
//  THE RUN IS NOT ONE MODULE.  Six blocks, each proved by the strings its
//  code pushes, the globals it touches and (twice) by an existing source
//  file in this repository:
//
//    0x0042da50..0x0042dffa  cb_LoadServerObjects.  ALREADY SOURCED as
//                            src/engine/server/counter.cpp's
//                            cb_LoadServerObjects (server 0x1000e121):
//                            same six error names ("LoadServerObjects" /
//                            "LoadObjectsInDirectory" x LT_MISSINGSHELLDLL,
//                            LT_INVALIDSHELLDLL, LT_INVALIDSHELLDLLVERSION,
//                            LT_INVALIDOBJECTDLL,
//                            LT_INVALIDOBJECTDLLVERSION,
//                            LT_ERRORCOPYINGFILE), same "object.lto" /
//                            "sres.dll" / "ServerShell", same GetTempPathA
//                            + ".\\" fallback.  Nothing here.
//
//    0x0042de00..0x0042e10a  the render-mode list: the *.ren directory walk
//                            (0x0042de00), the per-DLL GetSupportedModes /
//                            FreeModeList pair (0x0042de60 -- already
//                            declared in clientmgr.h), the RMode list free
//                            (0x0042df20), ci_SetRenderMode (0x0042df60,
//                            already in clientinterface.h) and
//                            ci_ShutdownRender (0x0042e0c0).  None of these
//                            is an enumerated row here.
//
//    0x0042e110..0x0042e7c4  the CLIENT's dsi layer -- the temp-extract
//                            helper and InitClientShellDE ("cshell.dll",
//                            "cres.dll", "cresl.dll", "ClientShell"), then
//                            the small system services.  The small ones are
//                            in this file.
//
//    0x0042e7d0..0x0042ee6a  the texture module: TextureData's constructor
//                            (0x0042e7f0, vtable 0x004a22ac), destructor
//                            (0x0042e830), allocator (0x0042e8e0),
//                            "dtx_Create" (0x0042eab0) and its free
//                            (0x0042ee30).
//
//    0x0042eec0..0x0042f5c2  the five BUILT-IN SURFACE EFFECTS.  The table
//                            at 0x004a9e38 is
//                            `SurfaceEffectDesc g_BuiltIn[5]` (ltbasedefs.h
//                            declares the struct; iltclient.h's
//                            AddSurfaceEffect is what consumes it) and it
//                            hands every function in the block its exact
//                            signature -- see the block comment there.
//
//    0x0042f660..0x00430da7  the error log ("errorlogfile", "error.log",
//                            "- %s\n") and a message-record queue whose
//                            record is 28 bytes out of a StructBank.  Three
//                            functions in that last block --  0x0042f950,
//                            0x0042f9d0 and 0x0042fa50 -- are
//                            CPacket::ReadType<uint16>,
//                            ReadType<uint32> and WriteType<uint16>, and
//                            0x004303c0 is WriteType<uint32>.  ALL FOUR ARE
//                            ALREADY SOURCED, in src/engine/server/packet.h
//                            (they push "CPacket::ReadType" /
//                            "CPacket::WriteType" and "ERROR: Packet read
//                            overrun!").  Nothing here touches them.
//                            They do not pair yet: lithtech.exe's bodies
//                            EXPAND the grow step (collection.h's
//                            CMoArray<uint8>::Insert2 -- BaseNew 0x0040c060,
//                            BaseDelete 0x004152b0 on g_DefAlloc 0x004b4e88,
//                            m_nCacheSize +0x5c, m_nWantedCache +0x60)
//                            where packet.h calls an out-of-line
//                            CPacketData::SetAtGrow.  Closing them means
//                            making m_Data a real CMoArray<uint8> in
//                            packet.h, which pulls collection.h into every
//                            packet.h user (servermsg.h, smgr.cpp and
//                            cchandler.cpp carry their own conflicting
//                            views of the same allocator).
//
//  Build /O2, like the rest of src/engine/lithtech.
//
//  NAMING.  Where the shipped image gives a name (a pushed literal, an SDK
//  struct field, an export) it is used.  Where it does not, the address is
//  the name.  Nothing below is named from a guess about what it is for.
//
//  ONE COLLISION TO KNOW ABOUT: dsi_ServerError is defined here, and
//  src/engine/server/dsi.cpp defines a *different* function under the same
//  name (server.dll's 0x1000e3f9, which checks g_pServerMgr and dispatches
//  through the app handler).  The client's, at 0x0042e6f0, formats into a
//  500-byte buffer and prints to the console at colour 0xffff00.  Both TUs
//  are in the lithtech.exe object list; dsi.cpp's is compiled for
//  server.dll only (#ifdef LT_SERVER_BUILD), so the link sees this one.
//
// *********************************************************************** //

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <setjmp.h>
#include <windows.h>
#include "ltbasedefs.h"
#include "ltlink.h"
#include "de_error.h"
#include "packet.h"
#include "cluglob.h"                         // g_CluClientGlob, 0x004b0ac0

// windows.h renames engine methods out from under the SDK headers.
#undef PostMessage
#undef GetMessage
#undef PeekMessage
#undef CopyFile
#undef PlaySound
#undef GetClassName


// ---- the engine's allocator front ends ---------------------------------- //
//
// Two pairs are in use across this run and they are NOT interchangeable:
// 0x004290b0/0x004290c0 are servermgr.h's dalloc/dfree, and each is a
// one-argument forwarder onto 0x00428fd0/0x00429080 (the note in
// src/engine/lithtech/stdlithalloc.cpp establishes the two hops).  The
// shipped code picks one pair per allocation and frees with the matching
// one -- TextureData and its 0xc8 buffer go through dalloc/dfree, the
// texture's per-surface links and the queue record through the inner pair --
// so which one a line calls is read off the bytes, not chosen.
void* dalloc(unsigned long size);            // 0x004290b0
void  dfree(void* ptr);                      // 0x004290c0
void* sub_428fd0(unsigned long size);        // 0x00428fd0
void  sub_429080(void* ptr);                 // 0x00429080
void* sub_429050(unsigned long size);        // 0x00429050  0x00428fd0 then zero-filled


// ---- the client console's print entry ----------------------------------- //
//
// 0x0041b880 -- a three-argument cdecl forwarder onto a __thiscall method of
// the console object at 0x004b2af8, arguments in order.  Both callers below
// pass a colour first (0xffff00 for the error printer, 0xffffff for the
// message reporter) and the formatted line last; the middle argument is 0 at
// every site in this run, so nothing here says what it selects.
void sub_41b880(uint32 nColor, int nUnknown, char* pMsg);        // 0x0041b880

// 0x0041b870 -- `mov 0x4(%esp),%eax; mov %eax,0x004b2ea8; ret`.  One store,
// so this installs the console's line hook and nothing else.  Written in
// clientconsole.cpp as con_Sub41b870(int); the hook goes in as that int.
void con_Sub41b870(int nValue);                                  // 0x0041b870

// 0x0041b3e0 -- the console variable lookup: strlen(pName), then a hash
// lookup in pState+0x3c, then 0x0042d080 on the element.  servermgr.h
// declares the server's as cc_FindConsoleVar(ConsoleState*, char*); the
// client's is kept under its address here so this TU does not have to pull
// servermgr.h in.
class ConsoleStateView
{
public:
	uint8  m_pad_00[0x4c];
};

// The record it hands back.  Only +0x14 is read in this run, and
// servermgr.h's LTCommandVar already calls it m_pStringVal ("interned;
// comparing the pointers compares the text").
class LTCommandVarView
{
public:
	uint8  m_pad_00[0x14];
	char*  m_pStringVal;                     // 0x14
};

LTCommandVarView* sub_41b3e0(ConsoleStateView* pState, char* pName);  // 0x0041b3e0

// 0x004b2f00 -- the client's console state.  clientmgr.h already names it;
// it is servermgr.h's ConsoleState (declared, not included, so the view
// above is what this TU reads it through).
class ConsoleState;
extern ConsoleState g_ClientConsoleState;                        // 0x004b2f00


// ---- window and render state this file reads ---------------------------- //
//
// The main window (0x004b0b04, m_hMainWnd) and the message-box caption
// (0x004b0b10, m_pWindowTitle -- a char* in .data, not a literal: the push is
// `mov 0x004b0b10,%eax`, one dereference) are fields of clientwin.cpp's
// g_CluClientGlob (cluglob.h).
// The render-up flag (0x004b439c) is clientrender.cpp's g_RenderStruct's
// +0x44, reached through that definition.
class RenderStruct
{
public:
	uint8  m_pad_00[0x44];
	int    m_bInited;                        // 0x044  0x004b439c
};
extern RenderStruct g_RenderStruct;          // 0x004b4358


// ======================================================================= //
//
//  THE RENDER-MODE LIST (one function of it)
//
// ======================================================================= //

typedef RMode* (*GetSupportedModesFn)();
typedef void   (*FreeModeListFn)(RMode* pModes);

// 0x0042de60 -- 184 bytes.  ci_GetRenderModes' per-.ren worker (clientmgr.h
// declares it; its caller is clientinterface.cpp's ci_GetRenderModes at
// 0x0042de00).  Load the DLL, ask it for its modes, push a copy of each one
// onto the caller's list with the DLL's file name written into
// m_RenderDLL (`lea 0x4(%ebx)`, strncpy of 0xff), hand the DLL's list back
// to it and unload.  Each copy is 0x214 = sizeof(RMode) out of 0x00428fd0,
// the inner allocator -- the matching free is 0x00429080 in
// ci_RelinquishRenderModes (0x0042df20).
void sub_42de60(char* pFilename, RMode** ppModeList)
{
	HINSTANCE           hInstance;
	GetSupportedModesFn pGetModes;
	FreeModeListFn      pFreeModes;
	RMode*              pModes;
	RMode*              pCur;
	RMode*              pNew;

	hInstance = LoadLibraryA(pFilename);
	if (hInstance)
	{
		pGetModes  = (GetSupportedModesFn)GetProcAddress(hInstance, "GetSupportedModes");
		pFreeModes = (FreeModeListFn)GetProcAddress(hInstance, "FreeModeList");

		if (pGetModes && pFreeModes)
		{
			pModes = pGetModes();

			pCur = pModes;
			while (pCur)
			{
				pNew = (RMode*)sub_428fd0(sizeof(RMode));
				memcpy(pNew, pCur, sizeof(RMode));
				strncpy(pNew->m_RenderDLL, pFilename, 255);

				pNew->m_pNext = *ppModeList;
				*ppModeList = pNew;

				pCur = pCur->m_pNext;
			}

			pFreeModes(pModes);
		}

		FreeLibrary(hInstance);
	}
}


// ---- ci_GetSurfaceBounds (0x00407e30) ----------------------------------- //
//
// Its home is clientinterface.obj (it sits among the ci_ functions at
// 0x00407xxx; clientinterface.h declares it and clientinterface.cpp puts it
// in the interface table); written here, where 0x0042efa0 is its only
// caller, so this TU does not have to be shared.

// The world-BSP poly: the next poly of the same surface at +0x40, the
// vertex count at +0x50 and the vertices, 0x18 bytes each, from +0x54 --
// only each vertex's position pointer (its first dword) is read.
struct CPolyVertQ407
{
	LTVector* m_pVec;                        // 0x00
	uint8     m_pad_04[0x18 - 0x04];
};

struct CPolyQ407
{
	uint8         m_pad_00[0x40];
	uint16        m_iNextPoly;               // 0x40  0xffff ends the chain
	uint8         m_pad_42[0x50 - 0x42];
	uint16        m_nVerts;                  // 0x50
	uint8         m_pad_52[0x54 - 0x52];
	CPolyVertQ407 m_Verts[1];                // 0x54
};

struct CWorldBspQ407
{
	uint8       m_pad_00[0xa0];
	CPolyQ407** m_Polies;                    // 0xa0
};

struct CSurfaceQ407
{
	uint8  m_pad_00[0x40];
	uint16 m_iFirstPoly;                     // 0x40  0xffff: no polies
};

// g_pClientMgr, as far as this function reads it.
class CClientMgrQ407
{
public:
	uint8 m_pad_0000[0x1cc];
	void* m_pUnknown1cc;                     // 0x1cc  (tested only)
	uint8 m_pad_01d0[0x150c - 0x1d0];
	void* m_pConnection;                     // 0x150c (clientmgr.h's name)
};

extern CClientMgrQ407* g_pClientMgrQ407;     // 0x004b17e0

// 0x00407e30 -- 340 bytes.  The bounding box of every vertex of every poly
// on the surface's chain, starting from +/-100000.
LTBOOL ci_GetSurfaceBounds(SurfaceData* pSurface, LTVector* pMin, LTVector* pMax)
{
	CWorldBspQ407* pBsp;
	CSurfaceQ407*  pSurf;
	CPolyQ407*     pPoly;
	LTVector*      pVec;
	uint16         iPoly;
	uint16         i;

	if (!g_pClientMgrQ407->m_pConnection || !g_pClientMgrQ407->m_pUnknown1cc ||
	    !pSurface || !pMin || !pMax)
		return LTFALSE;

	pSurf = (CSurfaceQ407*)pSurface->m_pInternalSurface;
	pBsp  = (CWorldBspQ407*)pSurface->m_pInternalWorldBsp;

	pMin->x = 100000.0f;
	pMin->y = 100000.0f;
	pMin->z = 100000.0f;
	pMax->x = -100000.0f;
	pMax->y = -100000.0f;
	pMax->z = -100000.0f;

	iPoly = pSurf->m_iFirstPoly;
	while (iPoly != 0xffff)
	{
		pPoly = pBsp->m_Polies[iPoly];

		for (i = 0; i < pPoly->m_nVerts; i++)
		{
			pVec = pPoly->m_Verts[i].m_pVec;

			VEC_MIN(*pMin, *pMin, *pVec);
			VEC_MAX(*pMax, *pMax, *pVec);
		}

		iPoly = pPoly->m_iNextPoly;
	}

	return LTTRUE;
}


// ---- ci_SetRenderMode (0x0042df60) and the extract helper (0x0042e110) -- //

class CClientMgrQ42D;

// 0x004b17e0 -- g_pClientMgr, seen as the thing r_InitRender/r_TermRender
// take and as the owner of the file manager at +0x1534.
class CClientMgrQ42D
{
public:
	uint8 m_pad_0000[0x1534];
	void* m_pFileMgr;                        // 0x1534
};

extern CClientMgrQ42D* g_pClientMgrQ42D;     // 0x004b17e0

// clientrender.cpp's pair.  Both return LTRESULT.
LTRESULT r_TermRenderQ42D(CClientMgrQ42D* pClientMgr, LTBOOL bReport);  // 0x004627d0
LTRESULT r_InitRenderQ42D(CClientMgrQ42D* pClientMgr, RMode* pMode);    // 0x004623d0

// 0x004b4140 -- the current render mode (clientmgr.h's g_CurRenderMode).
extern RMode g_CurRenderMode;

// 0x004b0b30 is g_CluClientGlob.m_bRenderShutdown (cluglob.h).

// 0x0042d770 -- lt2new3.cpp's resource-DLL message formatter, and
// 0x0042e6a0 -- clientmgr.h's message reporter.
void sub_0042d770Q42D(char* pBuf, uint32 nBufSize, int nMessageCode,
                      va_list marker);
void sub_42e6a0Q42D(char* pMsg);

// 0x0042df60 -- 341 bytes.  ci_SetRenderMode (clientinterface.h).  Shut
// the renderer down (complaining with resource message 48 if the screen
// cannot be restored), remember the current mode (`rep movsl` of 0x85
// dwords = sizeof(RMode) into the local at +0x100), bring the renderer up
// in the new mode and, if that fails, in the old one again.  Frame 0x314 =
// a 256-byte message buffer + the RMode copy.
LTRESULT ci_SetRenderMode(RMode* pMode)
{
	char  msg[256];
	RMode oldMode;

	if (r_TermRenderQ42D(g_pClientMgrQ42D, LTTRUE) != LT_OK)
	{
		sub_0042d770Q42D(msg, 255, LT_UNABLETORESTOREVIDEO, LTNULL);
		sub_42e6a0Q42D(msg);
		RETURN_ERROR(0, "SetRenderMode", LT_UNABLETORESTOREVIDEO);
	}

	oldMode = g_CurRenderMode;

	if (r_InitRenderQ42D(g_pClientMgrQ42D, pMode) != LT_OK)
	{
		if (r_InitRenderQ42D(g_pClientMgrQ42D, &oldMode) != LT_OK)
		{
			RETURN_ERROR(0, "SetRenderMode", LT_UNABLETORESTOREVIDEO);
		}

		RETURN_ERROR(1, "SetRenderMode", LT_KEPTSAMEMODE);
	}

	g_CluClientGlob.m_bRenderShutdown = LTFALSE;
	return LT_OK;
}


// The file identifier 0x00404680 takes (clientwin.cpp's CluFileIdent).
struct CFileIdentQ42E
{
	uint32 m_nType;                          // 0x00
	char*  m_pName;                          // 0x04
	uint16 m_nID;                            // 0x08
};

// The record 0x00404680 hands back (clientwin.cpp's CluFileRec); only the
// tree at +0x10 is read here.
struct CFileRecQ42E
{
	uint8 m_pad_00[0x10];
	void* m_pTree;                           // 0x10
};

// clientwin.cpp's clu_fm_GetFileRec, clu_fs_GetTreeType and the directory
// tree's path builder; lt2new2.cpp's file-manager CopyFile.
CFileRecQ42E* fm_GetFileRecQ42E(void* pFileMgr, CFileIdentQ42E* pIdent,
                                uint8 nFlag);                   // 0x00404680
int  fs_GetTreeTypeQ42E(void* pTree);                           // 0x00420d30
int  fs_GetFilePathQ42E(void* pTree, char* pFilename, char* pOutPath,
                        uint32 nOutLen);                        // 0x00420e80
int  fm_CopyFileQ42E(void* pFileMgr, char* pSrc, char* pDest);  // 0x00404950

#ifdef NOLFSRC_MODERN
// M-P9-01: the game's own DLLs are the ones in the game directory's
// custom\ folder (where the build installs them), whatever a mod archive
// listed after it carries. A mod packaged for the NOLF Modernizer (the
// Thief mod) ships its own cshell.dll, object.lto and cres.dll, built for
// the retail lithtech.exe; loaded by this engine they crash. 1: pOutPath is
// custom\<pFilename>, full path; 0: not there (the retail lookup decides).
static int P9_OwnGameFile(const char* pFilename, char* pOutPath, uint32 nOutLen)
{
	char  sPath[MAX_PATH];
	DWORD dwAttr;
	int   nLen = _snprintf(sPath, sizeof(sPath), "custom\\%s", pFilename);

	if (nLen < 0 || nLen >= (int)sizeof(sPath))
		return 0;

	dwAttr = GetFileAttributesA(sPath);
	if (dwAttr == 0xFFFFFFFF || (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
		return 0;

	nLen = (int)GetFullPathNameA(sPath, nOutLen, pOutPath, LTNULL);
	return nLen > 0 && (uint32)nLen < nOutLen;
}

// M-P9-02: a mod's own cres.dll (the one the rez stack would have given),
// as resources only -- LOAD_LIBRARY_AS_DATAFILE runs none of its code --
// for the text of the string ids ours lacks or holds only as a retail
// placeholder (CLTClient::FormatString). The first member is the module
// handle, as the engine's resource-module records have it.
struct P9ModStrings
{
	HINSTANCE m_hInst;
};
static P9ModStrings s_P9ModStrings;
void*        g_pP9ModStrings = LTNULL;
static int   s_bP9RezLookup  = 0;    // sub_42e110 as retail: the rez stack's file

int sub_42e110(CClientMgrQ42D* pMgr, char* pTempPath, char* pFilename,
               char* pOutPath, uint32 nOutLen);

static void P9_LoadModStrings(CClientMgrQ42D* pMgr, char* pTempPath)
{
	char sOurs[MAX_PATH], sMod[MAX_PATH];

	if (!P9_OwnGameFile("cres.dll", sOurs, sizeof(sOurs)))
		return;

	s_bP9RezLookup = 1;
	int nResult = sub_42e110(pMgr, pTempPath, "cres.dll", sMod, sizeof(sMod));
	s_bP9RezLookup = 0;

	char sModFull[MAX_PATH];
	if (nResult != 0 || !GetFullPathNameA(sMod, sizeof(sModFull), sModFull, LTNULL) ||
	    _stricmp(sModFull, sOurs) == 0)
		return;                                 // no mod's cres.dll above ours

	s_P9ModStrings.m_hInst = LoadLibraryExA(sModFull, LTNULL, LOAD_LIBRARY_AS_DATAFILE);
	if (s_P9ModStrings.m_hInst)
		g_pP9ModStrings = &s_P9ModStrings;
}

#endif
// 0x0042e110 -- 287 bytes.  The client's counterpart of counter.cpp's
// sub_1000e05b: get a named file onto disk where the loader can open it
// by path.  The lookup is by name (type 0, id 0) with flag 5.
int sub_42e110(CClientMgrQ42D* pMgr, char* pTempPath, char* pFilename,
               char* pOutPath, uint32 nOutLen)
{
	CFileIdentQ42E ident;
	char           sUnique[MAX_PATH];
	CFileRecQ42E*  pRec;
	int            nResult;
	int            nRetry;

	ident.m_nID   = 0;
	ident.m_nType = 0;
	ident.m_pName = pFilename;
#ifdef NOLFSRC_MODERN

	// M-P9-01: cshell.dll, cres.dll, cresl.dll from custom\ first
	if (!s_bP9RezLookup && P9_OwnGameFile(pFilename, pOutPath, nOutLen))
		return 0;
#endif

	pRec = fm_GetFileRecQ42E(pMgr->m_pFileMgr, &ident, 5);
	if (pRec && fs_GetTreeTypeQ42E(pRec->m_pTree) == 1)
	{
		fs_GetFilePathQ42E(pRec->m_pTree, pFilename, pOutPath, nOutLen);

		return 0;
	}
	else
	{
		sprintf(pOutPath, "%s%s", pTempPath, pFilename);

		nResult = fm_CopyFileQ42E(pMgr->m_pFileMgr, pFilename, pOutPath);

		if (nResult == 0)
		{
			return 0;
		}
		else if (GetTempFileNameA(pTempPath, pFilename, 0, sUnique))
		{
			nRetry = fm_CopyFileQ42E(pMgr->m_pFileMgr, pFilename, sUnique);

			if (nRetry != 0)
			{
				return nRetry;
			}
			else
			{
				strncpy(pOutPath, sUnique, nOutLen - 1);

				return 0;
			}
		}
		else
		{
			return nResult;
		}
	}
}


// ---- InitClientShellDE (0x0042e230) ------------------------------------ //

// The client manager as InitClientShellDE reaches it (view; name ours).
// +0x610/+0x614 are the two resource-DLL module handles (cres.dll,
// cresl.dll), +0x618 the shell DLL's module record, +0x61c the client
// shell the DLL's create function hands back (clientmgr.h's
// m_pClientShell), and +0x4c0 is what that create function is given.
class CClientMgrQC5
{
public:
	// clientrender.cpp's 0x0041fc60: a __cdecl VARARGS member, which is
	// why `this` is pushed LAST.
	LTRESULT SetupError(LTRESULT dResult, ...);         // 0x0041fc60

	uint8  m_pad_0000[0x4c0];
	void*  m_p4c0;                           // 0x4c0
	uint8  m_pad_04c4[0x610 - 0x4c4];
	void*  m_hResModule;                     // 0x610
	void*  m_hLocResModule;                  // 0x614
	void*  m_pShellModule;                   // 0x618
	void*  m_pClientShell;                   // 0x61c
};

typedef void* (*CreateClientShellFnQC5)(void* p4c0);

// counter.cpp's sub_10034ec6 / sub_10001583 / sub_10034f9b / cb_FreeSoundTable
// at their client addresses, under counter.cpp's own names and types (the
// two record types are only declared; the slots here are void*).
class CModuleRef;
class CModuleFuncs;
int  sub_10034ec6(char* pFilename, char* pExport, void** ppModule,
                  int nVersion, int* pVersion);                     // 0x00477dc0
int  sub_10001583(char* pFilename, CModuleRef** phModule);          // 0x004016b0
void sub_10034f9b(CModuleFuncs* pRec, void** ppCreate,
                  void** ppDelete);                                 // 0x00477f10
void cb_FreeSoundTable(void* pRec);                                 // 0x00477ef0
void sub_42f5a0(void* pClientMgr);                                  // 0x0042f5a0, below

// 0x0042e230 -- 813 bytes.  InitClientShellDE: the client's counterpart of
// counter.cpp's cb_LoadServerObjects ("cshell.dll", "ClientShell" at
// version 2, then "cres.dll" and "cresl.dll"), read off the bytes:
//   * the three module slots are cleared first, in offset order.
//   * a failed cres.dll extract OR load gives the shell record back and
//     clears the slot before reporting (0x0042e422); cresl.dll's extract
//     and load answers are not looked at.
//   * the shell's version failure is the fall-through of the 0 and 1
//     tests -- there is no `== 2` compare (0x0042e509).
//   * the create function gets +0x4c0; no shell back is
//     LT_CANTCREATECLIENTSHELL with no SetupError (0x0042e3cc).
//   * the success path is the BODY of `nResult == -1`: the three shell
//     errors sit after everything else (0x0042e461).
//   * the two LT_ERRORCOPYINGFILE reports share their tail from the
//     printer call on, hosted in the cshell.dll arm (the cres.dll arm jumps
//     back to 0x0042e2db).  VC6 cross-jumps onto the arm LAST in source
//     order, so the cshell.dll report is a `goto` target written at the
//     very end; it is still laid out inline after its test.  Written in
//     place (the obvious form) the host goes to the cres.dll arm: 813/813
//     with two blocks.
LTRESULT InitClientShellDE(CClientMgrQC5* pMgr)
{
	void* pCreate;
	int   nVersion;
	void* pDelete;
	char  sTempPath[200];
	char  sFilename[256];
	int   nResult;

	pMgr->m_hResModule = LTNULL;
	pMgr->m_hLocResModule = LTNULL;
	pMgr->m_pShellModule = LTNULL;

	if (!GetTempPath(sizeof(sTempPath), sTempPath))
		strcpy(sTempPath, ".\\");

	if (sub_42e110((CClientMgrQ42D*)pMgr, sTempPath, "cshell.dll", sFilename,
	               sizeof(sFilename)) != 0)
		goto CShellFailed;

	nResult = sub_10034ec6(sFilename, "ClientShell",
	                       &pMgr->m_pShellModule, 2, &nVersion);

	if (nResult == -1)
	{
		if (sub_42e110((CClientMgrQ42D*)pMgr, sTempPath, "cres.dll", sFilename,
		               sizeof(sFilename)) != 0 ||
		    sub_10001583(sFilename, (CModuleRef**)&pMgr->m_hResModule) != -1)
		{
			cb_FreeSoundTable(pMgr->m_pShellModule);
			pMgr->m_pShellModule = LTNULL;

			pMgr->SetupError(LT_ERRORCOPYINGFILE, "cres.dll");

			RETURN_ERROR_SYM_DESC(1, "InitClientShellDE", LT_ERRORCOPYINGFILE,
			                      "cres.dll")
		}

		sub_42e110((CClientMgrQ42D*)pMgr, sTempPath, "cresl.dll", sFilename,
		           sizeof(sFilename));
		sub_10001583(sFilename, (CModuleRef**)&pMgr->m_hLocResModule);
#ifdef NOLFSRC_MODERN

		// M-P9-02: a mod's strings, beside ours
		P9_LoadModStrings((CClientMgrQ42D*)pMgr, sTempPath);
#endif

		sub_10034f9b((CModuleFuncs*)pMgr->m_pShellModule, &pCreate, &pDelete);

		pMgr->m_pClientShell = ((CreateClientShellFnQC5)pCreate)(pMgr->m_p4c0);

		if (!pMgr->m_pClientShell)
			RETURN_ERROR(1, "InitClientShellDE", LT_CANTCREATECLIENTSHELL)

		sub_42f5a0(pMgr);

		return LT_OK;
	}

	if (nResult == 0)
	{
		pMgr->SetupError(LT_MISSINGSHELLDLL, "cshell.dll");

		RETURN_ERROR(1, "InitClientShellDE", LT_MISSINGSHELLDLL)
	}

	if (nResult == 1)
	{
		pMgr->SetupError(LT_INVALIDSHELLDLL, "cshell.dll");

		RETURN_ERROR(1, "InitClientShellDE", LT_INVALIDSHELLDLL)
	}

	pMgr->SetupError(LT_INVALIDSHELLDLLVERSION, "cshell.dll", nVersion, 2);

	RETURN_ERROR(1, "InitClientShellDE", LT_INVALIDSHELLDLLVERSION)

CShellFailed:
	pMgr->SetupError(LT_ERRORCOPYINGFILE, "cshell.dll");

	RETURN_ERROR_SYM_DESC(1, "InitClientShellDE", LT_ERRORCOPYINGFILE,
	                      "cshell.dll")
}


// ======================================================================= //
//
//  THE CLIENT'S dsi LAYER
//
//  0x0042e560..0x0042e7c4.  Nine of the small functions in the block are
//  already accounted for (dsi_OnOutOfMemory 0x0042e560, the two count
//  readers 0x0042e590/0x0042e5a0, 0x0042e580, 0x0042e670, 0x0042e690,
//  sub_42e6a0 0x0042e6a0 -- clientmgr.h declares that one -- 0x0042e730 and
//  0x0042e740); the rest are below.
//
// ======================================================================= //

// 0x0042e560 -- 12 bytes, no `ret`: `push $1; push $0x4b0ac4; call
// 0x00493f30`.  0x004b0ac4 is the jmp_buf clientwin.cpp's WinMain arms with
// setjmp (g_CluClientGlob+0x04, m_MemoryJmp), so this is longjmp(env, 1),
// noreturn -- the client's out-of-memory report is to unwind to WinMain.
// dsi.cpp's mem_NewHandler (0x00428f40) is the caller.

void dsi_OnOutOfMemory()
{
	longjmp(g_CluClientGlob.m_MemoryJmp, 1);
}


// Three parallel 100-entry dword tables and the two counts that go with
// them, contiguous in clientwin.cpp's g_CluClientGlob (cluglob.h's
// m_KeyDownVK / m_KeyUpVK / m_KeyDownRep, m_nKeyDowns / m_nKeyUps):
//
//   0x004b0d34 .. 0x004b0ec3   400 bytes
//   0x004b0ec4 .. 0x004b1053   400 bytes
//   0x004b1054 .. 0x004b11e3   400 bytes
//   0x004b11e4                 uint16, cleared by 0x0042e5e0, read 0x0042e590
//   0x004b11e6                 uint16, cleared by 0x0042e5f0, read 0x0042e5a0
//
// The 400/4 = 100 element count is the gap between them and nothing else, so
// it is documentation only: every accessor below is an indexed load off the
// glob's absolute address and the bound never reaches the code.
//
// 0x004b11e8 -- one dword (m_nUnknown728), read by 0x0042e670 and written by
// 0x0042e680.


// 0x0042e570 -- 16 bytes.  `test %eax,%eax; jbe` is an UNSIGNED test, so the
// parameter is unsigned and the source condition is `> 0`; with a signed int
// VC6 would have emitted `jle`.  Sleep comes straight off the import table
// with no cleanup, which is what says it is the __stdcall API and not a
// wrapper.
void sub_42e570(uint32 nMilliseconds)
{
	if (nMilliseconds > 0)
		Sleep(nMilliseconds);
}


// 0x0042e5b0 / 0x0042e5c0 / 0x0042e5d0 -- 12 bytes each.  `mov 0x4(%esp),%eax;
// mov table(,%eax,4),%eax; ret`: the index is scaled by four and never sign
// extended, so it is a uint32 and the element is a dword.  Three separate
// bodies for three separate tables -- the relocation is the only difference,
// which is why /OPT:ICF left all three standing.
int sub_42e5b0(uint32 nIndex)
{
	return g_CluClientGlob.m_KeyDownVK[nIndex];
}

int sub_42e5c0(uint32 nIndex)
{
	return g_CluClientGlob.m_KeyDownRep[nIndex];
}

int sub_42e5d0(uint32 nIndex)
{
	return g_CluClientGlob.m_KeyUpVK[nIndex];
}


// 0x0042e5e0 / 0x0042e5f0 -- 10 bytes each, `movw $0x0,addr; ret`.  A word
// store, so the counts are 16-bit; 0x0042e590 and 0x0042e5a0 read the same
// two addresses with `mov ax,addr` and confirm it.
void sub_42e5e0()
{
	g_CluClientGlob.m_nKeyDowns = 0;
}

void sub_42e5f0()
{
	g_CluClientGlob.m_nKeyUps = 0;
}


// 0x0042e670 -- 6 bytes, `mov 0x4b11e8,%eax; ret`: the getter.
// clientworld.cpp's CDemoMgrView::IsRunning tail-calls it.
uint32 sub_42e670()
{
	return g_CluClientGlob.m_nUnknown728;
}


// 0x0042e590 / 0x0042e5a0 -- 7 bytes each, `mov addr,%ax; ret`: the readers
// of the two counts above (clientmgr3.cpp drains the queues with them).
// (names ours)
uint16 dsi_GetNumCommands()
{
	return g_CluClientGlob.m_nKeyDowns;
}

uint16 dsi_GetNumKeyUps()
{
	return g_CluClientGlob.m_nKeyUps;
}


// 0x0042e580 / 0x0042e690 -- 6 bytes each, `mov addr,%eax; ret`: two dwords
// of clientwin.cpp's client glob, +0x72c (0x004b11ec, m_bInputEnabled) and
// +0x060 (0x004b0b20, the "active" flag the focus handler toggles).  Only
// those two fields are declared here; the struct is clientwin.cpp's, and the
// names are its.  clientmgr3.cpp and clientshell.cpp are the callers.

int dsi_IsInputEnabled()
{
	return g_CluClientGlob.m_bInputEnabled;
}

int dsi_IsClientActive()
{
	return g_CluClientGlob.m_nUnknown60;
}


// 0x0042e680 -- 10 bytes.  The setter for the dword 0x0042e670 reads.
void sub_42e680(uint32 nValue)
{
	g_CluClientGlob.m_nUnknown728 = nValue;
}


// 0x004b0ac0 / 0x004b11f0 -- two fields of clientwin.cpp's CluClientGlob:
// m_bWindowUp at +0x000 and the 500-byte m_ErrorString at +0x730, which
// runs to the struct's end (0x004b13e4).

// 0x0042e6a0 -- 66 bytes.  LithTech's dsi_OnClientShutdown: remember why
// the client is shutting down and ask the pump to quit.  clientmgr.h
// declares it as sub_42e6a0; clientwin.cpp's WM_DESTROY passes a null and
// its focus handler a formatted message.
//
// Read off the bytes: `push $0x1f3; push %eax; push $0x4b11f0; call
// 0x00492b60` is strncpy with 499, then `movb $0x0,0x4b13e3` terminates at
// [499]; a null OR empty message clears the first byte instead.
// PostQuitMessage(0) only while the window is up.
void sub_42e6a0(char* pMsg)
{
	if (pMsg && pMsg[0])
	{
		strncpy(g_CluClientGlob.m_ErrorString, pMsg, 499);
		g_CluClientGlob.m_ErrorString[499] = 0;
	}
	else
	{
		g_CluClientGlob.m_ErrorString[0] = 0;
	}

	if (g_CluClientGlob.m_bWindowUp)
		PostQuitMessage(0);
}


// 0x0042e600 -- 118 bytes.  Two identical drains, WM_KEYDOWN then WM_KEYUP,
// each bounded at five hundred messages.
//
// THE FUNCTION POINTER IS A LOCAL, not a hoist.  PeekMessageA's import slot
// is loaded ONCE, at 0x0042e605, before *both* loops; a loop-invariant hoist
// would have produced one load per loop.  `PeekMessageA` is dllimport, so
// taking its address is exactly the `mov 0x004a1208,%edi` the shipped code
// opens with.
//
// The loop shape is a `for` and not a rotated `while`: there is no bound
// test before the first PeekMessage call, because VC6 knows i is 0 there,
// and the `cmp $0x1f4` sits at the bottom.  One `i` serves both loops and
// holds esi across the pair, which is why esi is pushed in the prologue.
//
// The window handle is re-read from the global on every iteration -- a call
// through a function pointer can change it, so it cannot be hoisted, and
// that is the difference between the two loads.
void sub_42e600()
{
	MSG msg;
	int i;
	BOOL (WINAPI *pfnPeekMessage)(LPMSG, HWND, UINT, UINT, UINT);

	pfnPeekMessage = PeekMessageA;

	for (i = 0; i < 500; i++)
	{
		if (!pfnPeekMessage(&msg, g_CluClientGlob.m_hMainWnd, WM_KEYDOWN, WM_KEYDOWN, PM_REMOVE))
			break;
	}

	for (i = 0; i < 500; i++)
	{
		if (!pfnPeekMessage(&msg, g_CluClientGlob.m_hMainWnd, WM_KEYUP, WM_KEYUP, PM_REMOVE))
			break;
	}
}


// 0x0042e6f0 -- 53 bytes.  The CLIENT's dsi_ServerError, and the function
// every REPORT_ERROR site in lithtech.exe's own code calls: the four-push
// sequence `push desc; push code; push name; push g_pErrorFormat` lands
// here at 0x0042db44, 0x0042e2eb, 0x0042e6f0's own neighbours and forty-odd
// other sites in this run.  src/engine/server/de_error.h describes the
// idiom and declares the name.
//
// `sub $0x1f4` fixes the buffer at exactly 500 bytes -- the whole frame is
// the buffer, nothing else is spilled -- and `lea 0x1fc(%esp),%eax` is
// &pFormat + 4, i.e. the va_list.  The three arguments to the printer are
// never cleaned separately: VC6 folded their pop into the `add $0x20c,%esp`
// epilogue, which is what it does when the call is the last statement.
void dsi_ServerError(char* pFormat, ...)
{
	char    msg[500];
	va_list marker;

	va_start(marker, pFormat);
	vsprintf(msg, pFormat, marker);

	sub_41b880(0xffff00, 0, msg);
}


// 0x0042e730 and 0x0042e740 -- 6 bytes each, `mov <global>,%eax ; ret`:
// the module handle (0x004b0b08) and the main window (0x004b0b04), both
// fields of clientwin.cpp's client glob (m_hInstance, m_hMainWnd).
// clientinput.cpp and clientrender.cpp call them.

HINSTANCE dsi_GetInstanceHandle()
{
	return g_CluClientGlob.m_hInstance;
}

HWND dsi_GetMainWindow()
{
	return g_CluClientGlob.m_hMainWnd;
}


// 0x0042e750 -- 56 bytes.  Print the line white, and if the renderer never
// came up put it in a message box as well -- with the renderer running a
// modal box over a full-screen surface would be invisible, which is what
// g_bRenderInited is doing in the test.  Returns LT_OK: `xor %eax,%eax` sits
// before the `pop %esi`, so the zero is a return value and not a scratch
// clear.
LTRESULT sub_42e750(char* pMsg)
{
	sub_41b880(0xffffff, 0, pMsg);

	if (!g_RenderStruct.m_bInited)
		MessageBoxA(g_CluClientGlob.m_hMainWnd, pMsg, g_CluClientGlob.m_pWindowTitle, 0);

	return LT_OK;
}


// 0x0042e790 -- 26 bytes.  Both parameters are loaded before the first push
// and the window handle last, which is MessageBoxA(hWnd, pText, pCaption,
// 0) with the two strings in parameter order.  The `ret` follows the call
// with nothing in between, so whatever MessageBoxA answered is this
// function's answer.
int sub_42e790(char* pText, char* pCaption)
{
	return MessageBoxA(g_CluClientGlob.m_hMainWnd, pText, pCaption, 0);
}


// ======================================================================= //
//
//  THE TEXTURE MODULE (partial)
//
//  0x0042e7d0..0x0042ee6a.  "dtx_Create" is the only literal in the block
//  and it is pushed by 0x0042eab0, so that is the entry point and everything
//  it calls belongs to the texture loader.  TextureData is 0x18c bytes
//  (`push $0x18c; call dalloc` at 0x0042e8e0), has exactly ONE virtual --
//  the constructor at 0x0042e7f0 stores 0x004a22ac and the dword at
//  0x004a22ac is 0x0042e8a0, with a float constant immediately after it, so
//  the vtable is one slot long -- and ends with eight 0x18-byte mip records
//  at 0xcc (0xcc + 8*0x18 = 0x18c exactly, which is what fixes both the
//  array bound and the record size).
//
// ======================================================================= //

// One mip level.  The data pointer is at +0x08: the destructor at 0x0042e830
// walks `lea 0xd4(%esi),%edi` with `add $0x18,%edi`, and 0xd4 is 0xcc + 8.
class TextureMipData
{
public:
	TextureMipData();                        // 0x0042e7d0

	// Names from what 0x0042e8e0 stores and 0x0042eab0 reads: width,
	// height, pixels and pitch, then the bumpmap plane and its pitch.
	uint32 m_nUnknown00;                     // 0x00  width
	uint32 m_nUnknown04;                     // 0x04  height
	void*  m_pData;                          // 0x08
	uint32 m_nUnknown0c;                     // 0x0c  pitch
	uint32 m_nUnknown10;                     // 0x10  bumpmap data
	uint32 m_nUnknown14;                     // 0x14  bumpmap pitch
};

// 0x0042e7d0 -- 22 bytes.  `mov %ecx,%eax` first and `this` back in eax at
// the end: a constructor, called eight times in a row by the outer class's
// own constructor (0x0042e800..0x0042e80b is the array loop, `mov $0x8,%edi`
// down to zero with `add $0x18,%esi`), which is how the array member is
// proved.
//
// THE STORE ORDER IS THE SOURCE ORDER, and it is not the offset order: the
// shipped stores go 0x00, 0x04, 0x0c, 0x08, 0x10, 0x14.  VC6 keeps the zero
// in ecx and emits one store per assignment in the order written, so the
// body has to be written in that order to come out the same length and the
// same shape.
TextureMipData::TextureMipData()
{
	m_nUnknown00 = 0;
	m_nUnknown04 = 0;
	m_nUnknown0c = 0;
	m_pData = 0;
	m_nUnknown10 = 0;
	m_nUnknown14 = 0;
}

// The per-surface records the texture keeps in a singly linked list off
// +0xbc, threaded through +0x20.  Built at 0x0042ecdd (`sizeof + 0x27`
// through sub_428fd0) and released by the destructor.
class TextureLink
{
public:
	uint8        m_pad_00[0x20];             // the section header
	TextureLink* m_pNext;                    // 0x20
	uint8        m_Data[1];                  // 0x24  -> sizeof 0x28
};

// The 0x20-byte section header dtx_Create reads ahead of each section; only
// the payload length at +0x1c is used.
struct DtxSectionHeaderQ42E
{
	uint8  m_pad_00[0x1c];
	uint32 m_DataLen;                        // 0x1c
};

// The 0xa4-byte DTX file header, read whole by dtx_Create (`push $0xa4`)
// and copied into TextureData+0x08 (`mov $0x29,%ecx; rep movsl`).
struct DtxHeaderQ42E
{
	uint32 m_ResType;                        // 0x00  must be 0
	int32  m_Version;                        // 0x04  must be -5
	uint16 m_BaseWidth;                      // 0x08
	uint16 m_BaseHeight;                     // 0x0a
	uint16 m_nMipmaps;                       // 0x0c
	uint16 m_nSections;                      // 0x0e
	uint32 m_IFlags;                         // 0x10  8 = sections fixed
	uint32 m_UserFlags;                      // 0x14
	union
	{
		uint8  m_Extra[12];                  // 0x18  [2] = the bit depth
		uint32 m_ExtraLong[3];
	};
	char   m_CommandString[128];             // 0x24

	uint32 GetBPPIdent() { return m_Extra[2] == 0 ? 3 : m_Extra[2]; }
	void   SetBPPIdent(uint32 bpp) { m_Extra[2] = (uint8)bpp; }
};

class PFormatQ42E;                           // below, with dtx_SetupFormat

// The vptr is at 0x00, so every pad below subtracts four from where the
// field run starts.  Only the fields this file needs are named.
class TextureData
{
public:
	// vtable 0x004a22ac, one slot: 0x0042e8a0, a `ret $4` that forwards to
	// 0x0042ee50 (dtx_SetupFormat) with the byte at +0x22 (or 3 when that
	// byte is zero).  Written after dtx_SetupFormat below (name ours).
	virtual void SetupPFormat(PFormatQ42E* pFormat);  // +0x00  0x0042e8a0

	TextureData();                           // 0x0042e7f0
	~TextureData();                          // 0x0042e830

	// 0x0042e8e0's `new`: `push $0x18c; call dalloc` then the constructor
	// on a non-null result.
	void* operator new(unsigned int nSize) { return dalloc(nSize); }

	uint32         m_nUnknown04;             // 0x04
	DtxHeaderQ42E  m_Header;                 // 0x08  -> 0xac
	LTLink         m_Link;                   // 0xac  m_pData = this
	uint32         m_AllocSize;              // 0xb8
	TextureLink*   m_pLinks;                 // 0xbc
	void*          m_pSharedTexture;         // 0xc0  (clientrender.cpp)
	uint32         m_nUnknownc4;             // 0xc4
	void*          m_pUnknownc8;             // 0xc8  the pixel buffer
	TextureMipData m_Mips[8];                // 0xcc  -> 0x18c
};

// 0x0042e7f0 -- 51 bytes.  The eight member constructors run first (the
// `mov $0x8,%edi ... dec %edi; jne` loop over m_Mips), then the vptr, then
// the one field the body clears.
TextureData::TextureData()
{
	m_pUnknownc8 = LTNULL;
}

// ----------------------------------------------------------------------- //
// 0x0042e830 — 112 bytes, thiscall, no arguments.  TextureData's
// destructor (written first in demodtx.cpp, moved here to its class).
//
// THIS IS THE ROW THE ENUMERATION CALLS 0x0042e856 / 74 / `setup`, AND THAT
// ROW IS A PHANTOM.  0x0042e856 is the middle of this function (`push
// %eax` at the top of the mip loop's free).  The `setup` provenance fired
// because the dword at 0x00473fb8 reads 0x0042e856 — and 0x00473fb8 is
// not data at all, it is the bytes `56 e8 42 00` of
// `push %esi; call 0x00474000` inside a function at 0x00473fa5.  A full
// image sweep finds NO other reference to 0x0042e856: no call, no jump, no
// stored address in any section.  The row above it, 0x0042e830 / 38, is
// truncated at the same phantom boundary; the real extent is
// 0x0042e830..0x0042e89f, 112 bytes, which is the number
// clientdsi.cpp's own decode note already gives.
//
// Read off the bytes, and answering the two questions clientdsi.cpp's note
// left open:
//
//   * `mov 0x18(%esi),%al` BEFORE `movl $0x004a22ac,(%esi)`: the flag load
//     is HOISTED ABOVE the vptr store (different displacements off one
//     base, so VC6 can prove no alias) and once hoisted it must land in a
//     register, which is why it is not the shorter `testb $0x4,0x18(%esi)`.
//     A plain `uint32 m_Flags` is enough — width reduction
//     narrows the hoisted load to %al by itself.  No field-type change
//     is needed.
//   * `push %edi` / `pop %edi` INSIDE the guard is shrink-wrapping: edi is
//     live only in the mip loop.  That is allocation, not
//     source.
//   * `xor %eax,%eax; mov 0x14(%esi),%ax` inside the loop is the uint16
//     mip count RELOADED each iteration, so the bound is written against
//     the member rather than cached.
//   * the +0xc8 buffer is released with 0x004290c0 and the mip data and
//     the link records with 0x00429080.  Per clientdsi.cpp's own allocator
//     note those are `operator delete` and dfree respectively, so the
//     middle statement is a `delete` (its null test is delete's own) and
//     the other two are explicit dfree calls with explicit guards.
//   * the link walk saves the next pointer into %esi — `this` itself —
//     before the free, which is only legal because nothing after it reads
//     `this`.  A plain rotated `while` gives it.
// ----------------------------------------------------------------------- //
TextureData::~TextureData()
{
	TextureLink* pCur;
	TextureLink* pNext;
	uint32       i;

	if (m_Header.m_IFlags & 4)                        // 0x0042e83d
	{
		for (i = 0; i < m_Header.m_nMipmaps; i++)     // 0x0042e843
		{
			if (m_Mips[i].m_pData)                    // 0x0042e852
				sub_429080(m_Mips[i].m_pData);        // 0x0042e857
		}
	}

	// COIN-FLIP SETTLED BY BUILD, and the other way round. `delete p` emits
	// NO null test here -- VC6 leaves that to operator delete -- giving
	// `mov X(%esi),%ecx ; push %ecx ; call`. The shipped bytes DO test:
	// `mov X(%esi),%eax ; test %eax,%eax ; je ; push %eax ; call`. So the
	// original is the explicit guarded free, not a delete-expression, and
	// those are the four bytes.
	if (m_pUnknownc8)                                 // 0x0042e872
		operator delete(m_pUnknownc8);                // 0x0042e879

	pCur = m_pLinks;                                  // 0x0042e881

	while (pCur)
	{
		pNext = pCur->m_pNext;                        // 0x0042e88b

		sub_429080(pCur);                             // 0x0042e88f

		pCur = pNext;
	}
}


uint32 fm_GetImageDataSize(uint32 nFormatCode, uint32 nWidth, uint32 nHeight);  // 0x0045f660

// 0x0042e8e0 -- 455 bytes.  dtx_Alloc: size every mip level (plus a byte
// per pixel of bumpmap plane when asked), allocate the TextureData and one
// buffer for all its levels, fill in a default header and carve the buffer
// into the mips.  lttexmod.cpp calls it (3, w, h, 1, 0, 0, bBumpmap) and
// dtx_Create (bpp, w, h, mips, &allocSize, &dataSize, 0).
//
// IDENTICAL.  What the bytes pinned, each measured:
//   * the per-level size is its own local (`add %eax,%ebx` once per level
//     at 0x0042e91f, not once for the image and once for the bumpmap);
//   * the bit depth goes in through an inline SetBPPIdent: the store
//     through the header's `this` is what makes VC6 RELOAD +0xc8 for
//     pOutData (0x0042e9eb) instead of forwarding dalloc's %eax.  A bare
//     `m_Extra[2] =` forwards and is 6 bytes short;
//   * m_Extra is zeroed as three dwords, [2] down to [0] (0x0042e9d9..df),
//     through the header's m_ExtraLong view -- memset comes out ascending
//     through a `lea` and reshuffles every register;
//   * m_AllocSize is set after m_pLinks: first, it keeps dataSize+0x18c in
//     %ebx across the header stores and loads the height into %eax;
//   * the pitch is an if-chain (a switch lays the arms out the other way).
TextureData* dtx_Alloc(uint32 bpp, uint32 width, uint32 height, uint32 nMipmaps,
                       uint32* pAllocSize, uint32* pTextureDataSize, LTBOOL bBumpmap)
{
	TextureData*    pRet;
	TextureMipData* pMip;
	uint32          dataSize, mipSize, iMipmap, curWidth, curHeight;
	uint8*          pOutData;

	curWidth = width;
	curHeight = height;
	dataSize = 0;
	for (iMipmap = 0; iMipmap < nMipmaps; iMipmap++)
	{
		mipSize = fm_GetImageDataSize(bpp, curWidth, curHeight);
		if (bBumpmap)
			mipSize += curWidth * curHeight;
		dataSize += mipSize;
		curWidth >>= 1;
		curHeight >>= 1;
	}

	pRet = new TextureData;
	if (!pRet)
		return LTNULL;

	pRet->m_pUnknownc8 = dalloc(dataSize);
	if (!pRet->m_pUnknownc8)
	{
		pRet->~TextureData();
		dfree(pRet);
		return LTNULL;
	}

	pRet->m_nUnknown04 = 0;
	pRet->m_Header.m_ResType = 0;
	pRet->m_Header.m_IFlags = 8;
	pRet->m_Header.m_UserFlags = 0;
	pRet->m_Header.m_nMipmaps = (uint16)nMipmaps;
	pRet->m_Header.m_BaseWidth = (uint16)width;
	pRet->m_Header.m_BaseHeight = (uint16)height;
	pRet->m_Header.m_Version = -5;
	pRet->m_Header.m_CommandString[0] = 0;
	pRet->m_Link.m_pData = pRet;
	pRet->m_pLinks = LTNULL;
	pRet->m_AllocSize = dataSize + sizeof(TextureData);
	pRet->m_Header.m_nSections = 0;
	pRet->m_Header.m_ExtraLong[2] = 0;
	pRet->m_Header.m_ExtraLong[1] = 0;
	pRet->m_Header.m_ExtraLong[0] = 0;
	pRet->m_Header.SetBPPIdent(bpp);
	pRet->m_pSharedTexture = LTNULL;
	pRet->m_nUnknownc4 = 0;

	curWidth = width;
	curHeight = height;
	pOutData = (uint8*)pRet->m_pUnknownc8;
	for (iMipmap = 0; iMipmap < nMipmaps; iMipmap++)
	{
		pMip = &pRet->m_Mips[iMipmap];

		pMip->m_nUnknown00 = curWidth;
		pMip->m_nUnknown04 = curHeight;
		pMip->m_pData = pOutData;
		if (bpp == 3)
			pMip->m_nUnknown0c = curWidth * 4;
		else if (bpp == 7)
			pMip->m_nUnknown0c = curWidth;
		else
			pMip->m_nUnknown0c = 0;

		pOutData += fm_GetImageDataSize(bpp, curWidth, curHeight);

		if (bBumpmap)
		{
			pMip->m_nUnknown10 = (uint32)pOutData;
			pMip->m_nUnknown14 = curWidth;
			pOutData += curWidth * curHeight;
		}

		curWidth >>= 1;
		curHeight >>= 1;
	}

	if (pAllocSize)
		*pAllocSize = pRet->m_AllocSize;

	if (pTextureDataSize)
		*pTextureDataSize = dataSize;

	return pRet;
}

void sub_42edd0Q42C(int bSkip, ILTStream* pStream, void* pData, uint32 nSize);  // 0x0042edd0 (demodtx.cpp)
void dtx_Destroy(TextureData* pTexture);                                        // 0x0042ee30

// 0x0042eab0 -- 786 bytes (0x0042eab0..0x0042edc1; the enumeration's 327 stops
// at an interior jump target, 0x0042ebf7).  dtx_Create: read the header,
// validate it, allocate, read each mip (row by row for 32-bit), then the
// sections, and check the stream at the end.
//
// IDENTICAL.  The one ordering fact: m_pSharedTexture is cleared BEFORE the
// header memcpy (0x0042ec05, ahead of the `rep movsl`).
LTRESULT dtx_Create(ILTStream* pStream, TextureData** ppOut, LTBOOL bLoadSections,
                    LTBOOL bSkipImageData)
{
	DtxHeaderQ42E        hdr;
	DtxSectionHeaderQ42E sectionHdr;
	TextureData*         pRet;
	TextureMipData*      pMip;
	TextureLink*         pSection;
	uint32               allocSize, textureDataSize, iMipmap, y, i;

	pStream->Read(&hdr, sizeof(hdr));

	if (hdr.m_ResType != 0)
		RETURN_ERROR(1, "dtx_Create", LT_INVALIDDATA);

	if (hdr.m_Version != -5)
		RETURN_ERROR(1, "dtx_Create", LT_INVALIDVERSION);

	if (hdr.m_nMipmaps == 0 || hdr.m_nMipmaps > 15)
		RETURN_ERROR(1, "dtx_Create", LT_INVALIDDATA);

	pRet = dtx_Alloc(hdr.GetBPPIdent(), hdr.m_BaseWidth, hdr.m_BaseHeight,
	                 hdr.m_nMipmaps, &allocSize, &textureDataSize, 0);
	if (!pRet)
		RETURN_ERROR(1, "dtx_Create", LT_OUTOFMEMORY);

	pRet->m_pSharedTexture = LTNULL;
	memcpy(&pRet->m_Header, &hdr, sizeof(hdr));

	for (iMipmap = 0; iMipmap < hdr.m_nMipmaps; iMipmap++)
	{
		pMip = &pRet->m_Mips[iMipmap];

		if (hdr.GetBPPIdent() == 3)
		{
			for (y = 0; y < pMip->m_nUnknown04; y++)
			{
				sub_42edd0Q42C(bSkipImageData, pStream,
				               (uint8*)pMip->m_pData + y * pMip->m_nUnknown0c,
				               pMip->m_nUnknown00 * 4);
			}
		}
		else
		{
			sub_42edd0Q42C(bSkipImageData, pStream, pMip->m_pData,
			               fm_GetImageDataSize(hdr.GetBPPIdent(),
			                                   pMip->m_nUnknown00, pMip->m_nUnknown04));
		}
	}

	if (hdr.m_IFlags & 8)
	{
		if (bLoadSections)
		{
			for (i = 0; i < hdr.m_nSections; i++)
			{
				pStream->Read(&sectionHdr, sizeof(sectionHdr));

				pSection = (TextureLink*)sub_428fd0(sizeof(TextureLink) - 1 + sectionHdr.m_DataLen);
				memcpy(pSection, &sectionHdr, sizeof(sectionHdr));
				pStream->Read(pSection->m_Data, sectionHdr.m_DataLen);

				pSection->m_pNext = pRet->m_pLinks;
				pRet->m_pLinks = pSection;
			}
		}
	}
	else
	{
		pRet->m_Header.m_nSections = 0;
		pRet->m_Header.m_IFlags |= 8;
	}

	if (pStream->ErrorStatus() != LT_OK)
	{
		dtx_Destroy(pRet);
		RETURN_ERROR(1, "dtx_Create", LT_INVALIDDATA);
	}

	*ppOut = pRet;
	return LT_OK;
}

// 0x0042ee30 -- 27 bytes.  NOT `delete`: the free is dfree (0x004290c0),
// the allocation at 0x0042e8e0 was dalloc, and `delete` in this build
// reaches 0x00429080 instead.  So the source spells the destructor call and
// the free separately, which is also why the null test is explicit rather
// than VC6's own `delete` guard.
void dtx_Destroy(TextureData* pTexture)
{
	if (pTexture)
	{
		pTexture->~TextureData();

		dfree(pTexture);
	}
}

// The pixel format (0x38 bytes, vptr 0x004a1ab4; fmconvert.cpp and
// lttexmod.cpp keep their own views).  Only what 0x0042ee50 reaches is
// declared: the one virtual, Init (0x0045f0e0, vtable slot 0, thiscall with
// the type and the four masks), and the non-virtual 0x0045f1b0 that calls it
// with type 3 and the A8R8G8B8 masks.  With no constructor used here, the
// undefined virtual emits no vtable.
class PFormatQ42E
{
public:
	virtual void Init(uint32 nType, uint32 aMask, uint32 rMask,
	                  uint32 gMask, uint32 bMask);   // 0x0045f0e0
	void         InitPValueFormat();                 // 0x0045f1b0
};

// 0x0042ee50 -- 32 bytes.  Set a PFormat up for a texture's bit depth:
// type 3 (32-bit) goes to the ARGB helper -- shipped TAIL-JUMPS to
// 0x0045f1b0, since it takes no stack arguments and ecx is already the
// format -- and anything else to Init with no masks.  Its two callers are
// TextureData's virtual at 0x0042e8a0, one with the literal 3 and one with
// the byte at +0x22 zero-extended, so the type is a full dword.
void dtx_SetupFormat(uint32 nType, PFormatQ42E* pFormat)
{
	if (nType == 3)
		pFormat->InitPValueFormat();
	else
		pFormat->Init(nType, 0, 0, 0, 0);
}

// 0x0042e8a0 -- 51 bytes, TextureData's one virtual: set a PFormat up for
// this texture's bit depth.  The two shipped arms (`mov $3,%eax` and
// `and $0xff,%eax`, each with its own push/call/`ret $4`) are the header's
// in-class GetBPPIdent ternary, tail-duplicated by /O2 -- one call in the
// source.
void TextureData::SetupPFormat(PFormatQ42E* pFormat)
{
	dtx_SetupFormat(m_Header.GetBPPIdent(), pFormat);
}


// ======================================================================= //
//
//  THE FIVE BUILT-IN SURFACE EFFECTS
//
//  0x004a9e38 is `SurfaceEffectDesc g_BuiltInSurfaceEffects[5]`, and it is
//  the single most useful object in this range because it types every
//  function it points at.  ltbasedefs.h declares the struct:
//
//      char  *m_pName;
//      void* (*InitEffect)(SurfaceData*, int argc, char **argv);
//      int   (*UpdateEffect)(SurfaceData*, void *pData);
//      void  (*TermEffect)(void *pData);
//
//  and the five rows, read out of .data, are
//
//      "Pan"     0x0042ef50  0x0042f120  0x0042f200
//      "Rotate"  0x0042f220  0x0042f2e0  0x0042f460
//      "Warble"  0x0042f470  0x0042f4d0  0x0042f590
//      "Portal"  0x0042eec0  0x00463d20  0x00404470
//      "Mirror"  0x0042ef00  0x00463d20  0x00404470
//
//  SurfaceData is public SDK (ltbasetypes.h): `LTVector O, P, Q;` at 0x00 /
//  0x0c / 0x18 then m_pInternalWorld, m_pInternalWorldBsp,
//  m_pInternalSurface at 0x24 / 0x28 / 0x2c.  Every offset the five Init
//  functions touch is one of those, which is the independent confirmation
//  that the table really is this struct.
//
//  0x0042efa0 is the shared allocator all three of the animated effects
//  call: 0x40 bytes out of 0x00429050, O/P/Q copied in, 0x24..0x2f zeroed,
//  and -- when its second argument is nonzero and argc > 0 -- argv[0] read
//  as a three-character per-axis origin spec ('C' centres, 'B' takes the
//  low corner) against ci_GetSurfaceBounds (0x00407e30, already declared in
//  clientinterface.h).  Written below, after Pan's InitEffect; the third
//  12-byte aggregate in its frame is operator-'s return temporary.
//
// ======================================================================= //

// What 0x0042efa0 hands back.  0x40 bytes, and the first 0x24 mirror
// SurfaceData's O/P/Q exactly because that is what 0x0042efa0 copies:
// (%esi)->(%edi), +0x0c and +0x18 three dwords each, then +0x24/+0x28/+0x2c
// zeroed.  The four trailing floats are what the effects animate.
class SurfaceEffectData
{
public:
	LTVector m_O;                            // 0x00  copy of SurfaceData::O
	LTVector m_P;                            // 0x0c  copy of SurfaceData::P
	LTVector m_Q;                            // 0x18  copy of SurfaceData::Q
	LTVector m_vAxis;                        // 0x24  zeroed; Rotate fills it
	float    m_fRate;                        // 0x30
	float    m_fUnknown34;                   // 0x34
	float    m_fUnknown38;                   // 0x38
	float    m_fUnknown3c;                   // 0x3c
};

SurfaceEffectData* sub_42efa0(SurfaceData* pSurfaceData, int nParseOrigin,
                              int argc, char** argv);         // 0x0042efa0

// The engine-side Surface behind SurfaceData::m_pInternalSurface.  Only the
// word at +0x42 is reached from this block: Portal writes an id into it and
// Mirror writes 0x7ffe and then sets the top bit.
class SurfaceView
{
public:
	uint8  m_pad_00[0x42];
	uint16 m_nUnknown42;                     // 0x42
};

// 0x00422180 -- 104 bytes.  The world's name lookup Portal resolves argv[0]
// through.  Its home is commonmodel.obj's lithtech run: it sits right after
// w_FindPortalInWorld (0x004220a0) and w_FindPortal (0x00422120), and before
// the name builder at 0x004221f0 -- but it has NO server.dll twin (server
// 0x10007dfa is followed directly by 0x10007e44), so it is client-only and
// is written here.
//
// Same shape as w_FindPortalInWorld: a subscripted walk (VC6 keeps a byte
// offset, `add $0x38`, beside the index it compares), the optional index
// written on a hit, and the record's address recomputed from base and index
// (`lea (,%esi,8)`, `sub %esi`, `lea (%edx,%ecx,8)` = i * 0x38).  The
// differences: the records are 0x38 bytes at +0x1b0 with the count at
// +0x1b4, the NAME IS INLINE at +0x00 (the record's address, not a loaded
// pointer, is what is pushed), and the compare is _stricmp (0x00492f30),
// record first.
class CWorldRec422180
{
public:
	char   m_Name[0x38];                     // 0x00 (only the name is read)
};

class CWorld422180View
{
public:
	uint8            m_pad_00[0x1b0];
	CWorldRec422180* m_pRecs;                // 0x1b0
	uint32           m_nRecs;                // 0x1b4
};

CWorldRec422180* sub_422180(void* pWorld, char* pName, uint32* pIndex)
{
	CWorld422180View* pThis = (CWorld422180View*)pWorld;
	uint32            i;

	for (i = 0; i < pThis->m_nRecs; i++)
	{
		if (stricmp(pThis->m_pRecs[i].m_Name, pName) == 0)
		{
			if (pIndex)
				*pIndex = i;

			return &pThis->m_pRecs[i];
		}
	}

	return LTNULL;
}

// The three Term slots.  Each is 12 bytes -- `mov 0x4(%esp),%eax; push %eax;
// call 0x0042f210; pop %ecx; ret` -- and there are THREE separate identical
// bodies at 0x0042f200, 0x0042f460 and 0x0042f590 rather than one folded
// one.  Defined below the Update slots; declared here so the table can
// name them.
extern "C" void sub_42f200(void* pData);     // 0x0042f200  "Pan" TermEffect
extern "C" void sub_42f460(void* pData);     // 0x0042f460  "Rotate"
extern "C" void sub_42f590(void* pData);     // 0x0042f590  "Warble"

// Portal and Mirror share an UpdateEffect and a TermEffect, and both live
// outside this range.  0x00463d20 is `mov $1,%eax; ret`, written in
// lt2new3.cpp (C linkage there too).  0x00404470 is the image's one bare
// `ret` that every empty slot points at; clientinterface.cpp's
// ci_DeleteColor is the definition of it (a one-dword cdecl no-op, so the
// cast is only for the table's slot type).
extern "C" int  sub_463d20(SurfaceData* pSurfaceData, void* pData);  // 0x00463d20
void ci_DeleteColor(uint32 hColor);                                  // 0x00404470
#define sub_404470 ((void (*)(void*))ci_DeleteColor)

// The three animated Update slots, all written below.
extern "C" int  sub_42f120(SurfaceData* pSurfaceData, void* pData);  // 0x0042f120
extern "C" int  sub_42f2e0(SurfaceData* pSurfaceData, void* pData);  // 0x0042f2e0
extern "C" int  sub_42f4d0(SurfaceData* pSurfaceData, void* pData);  // 0x0042f4d0

// g_pClientMgr (0x004b17e0; clientmgr.h names it) -- only the frame time at
// +0x1258 is read here, with `flds`, so it is a float.
class CClientMgrFrameView
{
public:
	uint8  m_pad_00[0x1258];
	float  m_FrameTime;                      // 0x1258
};

extern CClientMgrFrameView* g_pClientMgr;    // 0x004b17e0


// 0x0042eec0 -- 63 bytes.  "Portal" InitEffect.
//
// Two guards then one body then one shared `return LTNULL`, and the guards
// are in the order argc-first, world-second because that is the order the
// shipped tests appear in.  VC6 duplicated the `xor %eax,%eax; ret` (once at
// 0x0042eef8 after `pop %esi`, once at 0x0042eefc without) because esi is
// only live inside the body -- that is the register save splitting the exit,
// not two returns in the source.
//
// THE OUT-PARAMETER IS THE argc SLOT.  The function allocates no frame at
// all, and `lea 0xc(%esp),%ecx` with esi already pushed is [esp_entry+8],
// i.e. the second parameter.  So the source hands &argc to the lookup and
// reads the answer back out of it; a separate local would have cost a
// `sub $4,%esp` that is not there.
void* sub_42eec0(SurfaceData* pSurfaceData, int argc, char** argv)
{
	// IDENTICAL.  The surface pointer is a local declared INSIDE the block,
	// and the block ends in a `return LTNULL` of its own besides the one
	// after it (the sub_42ef00 lever): the two epilogues stay apart, so
	// `push %esi` is shrink-wrapped past both guards and argc is compared
	// straight out of memory.  One shared return gave 62 bytes.
	if (argc >= 1 && pSurfaceData->m_pInternalWorld)
	{
		SurfaceView* pSurface = (SurfaceView*)pSurfaceData->m_pInternalSurface;

		if (sub_422180(pSurfaceData->m_pInternalWorld, argv[0], (uint32*)&argc))
			pSurface->m_nUnknown42 = (uint16)argc;

		return LTNULL;
	}

	return LTNULL;
}


// 0x0042ef00 -- 65 bytes.  "Mirror" InitEffect.
//
// The 0x7ffe store is unconditional and lands BEFORE the argc test in the
// shipped schedule; the flag is `orb $0x80,0x43(%esi)`, the top byte of the
// same word, which is VC6 narrowing `|= 0x8000` on a uint16 to the one byte
// it can change.  0x00492f30 is the case-insensitive compare -- its inner
// loop folds 'A'..'Z' with `sub $0x41 / cmp $0x1a / sbb / and $0x20` on both
// operands, which is _stricmp and not strcmp.
void* sub_42ef00(SurfaceData* pSurfaceData, int argc, char** argv)
{
	SurfaceView* pSurface;

	// IDENTICAL.  The GUARDED form with a `return LTNULL` of its own at the
	// end of the body AND the one after it.  Shipped keeps the two
	// epilogues apart (0x0042ef3a with `pop %esi`, 0x0042ef3e without), so
	// %esi is pushed only on the path that uses it -- that is the
	// shrink-wrapped `push %esi`; one shared return merges them and hoists
	// the push to entry (62 bytes).  An early `if (!...) return` instead
	// inverts the first branch.
	if (pSurfaceData->m_pInternalWorld)
	{
		pSurface = (SurfaceView*)pSurfaceData->m_pInternalSurface;

		pSurface->m_nUnknown42 = 0x7ffe;

		if (argc >= 1)
		{
			if (stricmp(argv[0], "OVERLAY") == 0)
				pSurface->m_nUnknown42 |= 0x8000;
		}

		return LTNULL;
	}

	return LTNULL;
}


// 0x0042ef50 -- 79 bytes.  "Pan" InitEffect.  The second argument to the
// shared allocator is 0 here and 1 in Rotate and Warble, which is the whole
// difference between an effect that reads an origin spec out of argv[0] and
// one that does not -- and it is why Pan's own numbers start at argv[0]
// while the other two start at argv[1].
//
// 0x00493515 is atof: it returns in st(0) and the caller `fstps` straight
// into a float member, so the multiply-free arms are a plain narrowing
// store.  Both `jl`s land on the shared `mov %esi,%eax` exit, which is a run
// of guarded assignments inside one wrapping `if (pData)`.
void* sub_42ef50(SurfaceData* pSurfaceData, int argc, char** argv)
{
	SurfaceEffectData* pData;

	pData = sub_42efa0(pSurfaceData, 0, argc, argv);

	if (pData)
	{
		if (argc >= 1)
			pData->m_fRate = (float)atof(argv[0]);

		if (argc >= 2)
			pData->m_fUnknown34 = (float)atof(argv[1]);
	}

	return pData;
}


// 0x0042efa0 -- 377 bytes.  The shared effect-data allocator.  0x40 bytes
// out of 0x00429050 with NO null test, P and Q copied in, the axis zeroed;
// then, when asked to and argv[0] exists, the surface origin is moved by a
// three-letter spec against the surface's bounds: a leading 'C' centres it
// (max - min through a temporary, then min + half of that per axis), and
// otherwise each letter picks the low corner ('B') or the high one per
// axis.  The origin is copied into the data LAST, after any of that.
// 0x00493ffa is toupper.
SurfaceEffectData* sub_42efa0(SurfaceData* pSurfaceData, int nParseOrigin,
                              int argc, char** argv)
{
	SurfaceEffectData* pData;
	LTVector           vMin, vMax;

	pData = (SurfaceEffectData*)sub_429050(sizeof(SurfaceEffectData));

	pData->m_P = pSurfaceData->P;
	pData->m_Q = pSurfaceData->Q;
	// Init(), not three component stores: those let VC6 hoist the x store
	// above the last Q dword (size-exact, 2 blocks).
	pData->m_vAxis.Init();

	if (nParseOrigin && argc > 0 &&
	    ci_GetSurfaceBounds(pSurfaceData, &vMin, &vMax))
	{
		if (toupper(argv[0][0]) == 'C')
		{
			pSurfaceData->O = vMax - vMin;
			VEC_ADDSCALED(pSurfaceData->O, vMin, pSurfaceData->O, 0.5f);
		}
		else
		{
			pSurfaceData->O.x = (toupper(argv[0][0]) == 'B') ? vMin.x : vMax.x;
			pSurfaceData->O.y = (toupper(argv[0][1]) == 'B') ? vMin.y : vMax.y;
			pSurfaceData->O.z = (toupper(argv[0][2]) == 'B') ? vMin.z : vMax.z;
		}
	}

	pData->m_O = pSurfaceData->O;

	return pData;
}


// 0x0042f220 -- 185 bytes.  "Rotate" InitEffect.
//
// 0x004a22b0 holds 0x3c8efa35 = 0.01745329f, degrees to radians, and it is
// applied with `fmuls` to atof's double still in st(0) -- so the constant is
// a float and the narrowing happens at the store, which is exactly
// MATH_DEGREES_TO_RADIANS from ltbasedefs.h applied to the double.
//
// THE CROSS PRODUCT IS P.Cross(Q), and the direction is proved twice over.
// The by-value argument copy the shipped code makes is of SurfaceData::Q
// (three dwords out of `lea 0x18(%esi)` into the frame), so Q is the
// argument and P is the object; and `de e9`, which objdump prints as
// `fsubrp %st,%st(1)` under the AT&T/Intel mnemonic swap, is Intel FSUBP --
// ST(1) - ST(0) - giving Q.y*P.z - Q.z*P.y for the first component.  That is
// ltvector.h's `(v.y * z) - (v.z * y)` with v = Q and this = P.  Taking the
// other order would flip all three signs.
//
// The frame is 0x18 = two 12-byte aggregates, and BOTH are compiler
// temporaries -- the argument copy and Cross's return value.  No local is
// declared, which is why nothing has to be ordered here.
void* sub_42f220(SurfaceData* pSurfaceData, int argc, char** argv)
{
	SurfaceEffectData* pData;

	pData = sub_42efa0(pSurfaceData, 1, argc, argv);

	if (pData)
	{
		if (argc >= 2)
			pData->m_fRate = MATH_DEGREES_TO_RADIANS((float)atof(argv[1]));
			// The cast goes INSIDE: shipped emits `fmuls <float>`, so the
			// narrowing happens before the multiply. Casting the result
			// instead keeps the multiply in double and emits `fmull` against
			// a __real@8@ constant.

		pData->m_vAxis = pSurfaceData->P.Cross(pSurfaceData->Q);
	}

	return pData;
}


// 0x0042f470 -- 87 bytes.  "Warble" InitEffect.  Same shape as Pan with the
// argument indices shifted by one (the origin spec occupies argv[0]) and one
// unconditional trailing store: 0x3f000000 is 0.5f, written as an immediate
// dword rather than through the FPU because it is a constant.
void* sub_42f470(SurfaceData* pSurfaceData, int argc, char** argv)
{
	SurfaceEffectData* pData;

	pData = sub_42efa0(pSurfaceData, 1, argc, argv);

	if (pData)
	{
		if (argc >= 2)
			pData->m_fRate = (float)atof(argv[1]);

		if (argc >= 3)
			pData->m_fUnknown34 = (float)atof(argv[2]);

		pData->m_fUnknown3c = 0.5f;
	}

	return pData;
}


// 0x0042f120 -- 217 bytes.  "Pan" UpdateEffect.  Both accumulators advance
// by frame time times their rate (g_pClientMgr is re-read for the second:
// the first float store could alias it), then the surface origin is rebuilt
// from the two axes and the original origin -- three separate stores to O,
// so three statements.
extern "C" int sub_42f120(SurfaceData* pSurfaceData, void* pVoid)
{
	SurfaceEffectData* pData = (SurfaceEffectData*)pVoid;

	pData->m_fUnknown38 += g_pClientMgr->m_FrameTime * pData->m_fRate;
	pData->m_fUnknown3c += g_pClientMgr->m_FrameTime * pData->m_fUnknown34;

	pSurfaceData->O  = pSurfaceData->P * pData->m_fUnknown38;
	pSurfaceData->O += pSurfaceData->Q * pData->m_fUnknown3c;
	pSurfaceData->O += pData->m_O;

	return 1;
}

// 0x0042f2e0 -- 383 bytes.  "Rotate" UpdateEffect.  The accumulator at
// +0x34 advances by frame time times the rate, then ltmatrix.h's
// Mat_SetupRot builds the rotation about m_vAxis (its nine stores land in a
// 0x40-byte LTMatrix at 0x10(%esp) in the SDK's column-by-column order;
// the identity fill is dead and dropped) and MatVMul_3x3 rotates the
// original P and Q into the SURFACE's P and Q (%ecx is pSurfaceData, not
// g_pClientMgr as the older note below had it).
extern "C" int sub_42f2e0(SurfaceData* pSurfaceData, void* pVoid)
{
	SurfaceEffectData* pData = (SurfaceEffectData*)pVoid;
	LTMatrix           mat;

	pData->m_fUnknown34 += g_pClientMgr->m_FrameTime * pData->m_fRate;

	Mat_SetupRot(&mat, &pData->m_vAxis, pData->m_fUnknown34);
	MatVMul_3x3(&pSurfaceData->P, &mat, &pData->m_P);
	MatVMul_3x3(&pSurfaceData->Q, &mat, &pData->m_Q);

	return 1;
}

// 0x0042f4d0 -- 191 bytes.  "Warble" UpdateEffect.  Same two accumulators;
// then P is the original P scaled by cos(+0x38) + 1.2f (0x004a22b8) and Q
// the original Q scaled by sin(+0x3c) + 1.3f (0x004a22b4).  `fadds` against
// a 4-byte constant says the sum is float.
extern "C" int sub_42f4d0(SurfaceData* pSurfaceData, void* pVoid)
{
	SurfaceEffectData* pData = (SurfaceEffectData*)pVoid;
	float              fScaleP;
	float              fScaleQ;

	pData->m_fUnknown38 += g_pClientMgr->m_FrameTime * pData->m_fRate;
	pData->m_fUnknown3c += g_pClientMgr->m_FrameTime * pData->m_fUnknown34;

	fScaleP = (float)cos(pData->m_fUnknown38) + 1.2f;
	fScaleQ = (float)sin(pData->m_fUnknown3c) + 1.3f;

	pSurfaceData->P = pData->m_P * fScaleP;
	pSurfaceData->Q = pData->m_Q * fScaleQ;

	return 1;
}


// The three TermEffect slots, 12 bytes each -- declared above.  Each hands
// its data to 0x0042f210 (a null-guarded dfree, `test; je` then
// `call 0x00429080`).  Three separate bodies, as shipped.
void sub_42f210(void* pData);                // 0x0042f210

extern "C" void sub_42f200(void* pData)
{
	sub_42f210(pData);
}

extern "C" void sub_42f460(void* pData)
{
	sub_42f210(pData);
}

extern "C" void sub_42f590(void* pData)
{
	sub_42f210(pData);
}

// 0x0042f210 -- 16 bytes.  `mov 0x4(%esp),%eax; test %eax,%eax; je; push
// %eax; call 0x00429080; pop %ecx; ret`: free the effect's data if there is
// any.  Defined below its three callers so it is called, not expanded.
void sub_42f210(void* pData)
{
	if (pData)
		sub_429080(pData);
}


// The table itself, at 0x004a9e38.  Reconstructed from .data so that the
// registration loop below has something with a known extent to walk; the
// bytes of a data object are not what verification checks, but the array's
// address and its 0x50-byte length ARE what the loop's two immediates are.
static SurfaceEffectDesc g_BuiltInSurfaceEffects[5] =
{
	{ "Pan",    sub_42ef50, sub_42f120, sub_42f200 },
	{ "Rotate", sub_42f220, sub_42f2e0, sub_42f460 },
	{ "Warble", sub_42f470, sub_42f4d0, sub_42f590 },
	{ "Portal", sub_42eec0, sub_463d20, sub_404470 },
	{ "Mirror", sub_42ef00, sub_463d20, sub_404470 },
};

// 0x0041fbe0 -- what a SurfaceEffectDesc gets registered through;
// clientmgr.h already declares it as
// sub_41fbe0(CClientMgr*, SurfaceEffectDesc*).  The manager is taken as
// void* here so this TU does not need CClientMgr's layout.
LTRESULT sub_41fbe0(void* pClientMgr, SurfaceEffectDesc* pDesc);  // 0x0041fbe0

// 0x0042f5a0 -- 35 bytes, called from InitClientShellDE at 0x0042e40e.
//
// NOTE THE ENUMERATION IS OFF BY TWO HERE: the row says 0x0042f5a2/33, but
// 0x0042f5a0 is where the `push %esi; push %edi` prologue starts and where
// InitClientShellDE's call lands.  The function is 0x0042f5a0..0x0042f5c2.
//
// The loop is a pointer walk against a literal end address (`add $0x10,%esi`
// then `cmp $0x004a9e88,%esi; jb`), and 0x004a9e88 is table + 5 * 0x10 to
// the byte.  Written as an index it would come out the same after strength
// reduction, but the pointer form is what the bytes say.
void sub_42f5a0(void* pClientMgr)
{
	SurfaceEffectDesc* pDesc;

	for (pDesc = g_BuiltInSurfaceEffects;
	     pDesc < g_BuiltInSurfaceEffects + 5;
	     pDesc++)
	{
		sub_41fbe0(pClientMgr, pDesc);
	}
}


// ======================================================================= //
//
//  THE ERROR LOG
//
//  0x0042f660..0x0042f737.  Four globals and three functions.  "errorlogfile"
//  names the console variable, "error.log" is the default, "wtc" is the fopen
//  mode (text, commit-on-flush) and "- %s\n" is the line format.
//
// ======================================================================= //

// 0x004b32ec and 0x004b32f0 -- two console-bound switches: the first gates
// the whole log, the second forces a flush after every line.  Read with
// `mov` into a register and then `test`, at all three sites.  They are the
// "ErrorLog" and "AlwaysFlushLog" bindings (lithtech.exe's table entries
// 0x004aa300 / 0x004aa314), defined in src/engine/server/dsi.cpp.
extern int g_CV_ErrorLog;                    // 0x004b32ec
extern int g_CV_AlwaysFlushLog;              // 0x004b32f0

// 0x004b3318 -- the open log.  Re-read from the global after fprintf and
// again for fclose, which is what says it is a global and not a local.
// Defined here: the error log is this file's (zeroed .bss).
FILE* g_pErrorLogFile;                       // 0x004b3318


// 0x0042f680 -- 60 bytes.  The console line hook: both switches are tested
// against the same file handle and the flush is nested inside the write, so
// a log that is enabled but failed to open costs one test per line.
static void errorlog_Print(char* pMsg)
{
	if (g_CV_ErrorLog && g_pErrorLogFile)
	{
		fprintf(g_pErrorLogFile, "- %s\n", pMsg);

		if (g_CV_AlwaysFlushLog)
			fflush(g_pErrorLogFile);
	}
}


// 0x0042f6c0 -- 70 bytes.  Resolve the file name, open it if the log is on,
// and install the hook unconditionally.
//
// The default is materialised into ecx BEFORE the null test on the variable
// and overwritten on the other arm, which is the `char* p = default; if (v)
// p = v->m_pStringVal;` shape.  The hook is installed whether or not the
// file opened -- errorlog_Print tests for that itself.
void errorlog_Init()
{
	LTCommandVarView* pVar;
	char*             pFilename;

	pVar = sub_41b3e0((ConsoleStateView*)&g_ClientConsoleState, "errorlogfile");

	pFilename = "error.log";
	if (pVar)
		pFilename = pVar->m_pStringVal;

	if (g_CV_ErrorLog)
		g_pErrorLogFile = fopen(pFilename, "wtc");

	con_Sub41b870((int)errorlog_Print);
}


// 0x0042f710 -- 40 bytes.  Flush, close, forget.  The explicit fflush ahead
// of fclose is in the shipped bytes as its own call and is not fclose's own.
void errorlog_Term()
{
	if (g_pErrorLogFile)
	{
		fflush(g_pErrorLogFile);
		fclose(g_pErrorLogFile);

		g_pErrorLogFile = LTNULL;
	}
}


// ======================================================================= //
//
//  ODDS AND ENDS
//
// ======================================================================= //

// 0x0042f5d0 -- 35 bytes.  A singly linked list hanging off the client
// manager at +0x11d8, threaded through +0x0c, released one node at a time
// with 0x00429080.  `test` at the top and `jne` at the bottom is a rotated
// `while`, and the next pointer is read before the free -- which is the only
// reason the list survives being walked.
class CClientMgrView;

class CList11d8Node
{
public:
	uint8          m_pad_00[0x0c];
	CList11d8Node* m_pNext;                  // 0x0c
};

class CClientMgrView
{
public:
	uint8          m_pad_00[0x11d8];
	CList11d8Node* m_pList11d8;              // 0x11d8
};

void sub_42f5d0(CClientMgrView* pClientMgr)
{
	CList11d8Node* pCur;
	CList11d8Node* pNext;

	pCur = pClientMgr->m_pList11d8;

	while (pCur)
	{
		pNext = pCur->m_pNext;

		sub_429080(pCur);

		pCur = pNext;
	}
}


// ---- the message-record queue ------------------------------------------- //
//
// 0x0042fbc0 builds it: 0x88 bytes out of dalloc, an LTLink head at +0x3c
// tied off to itself, counters at +0x48/+0x4c, 0x47c35000 (100000.0f) at
// +0x58, and an embedded object at +0x18 whose vptr is 0x004a22bc and whose
// StructBank starts at +0x1c, initialised for 0x1c-byte records thirty-two
// deep.  0x1c is also the size of one queued record, which is how the two
// halves are tied together.
//
// Only the lookup below is reconstructed.  Everything else in the block
// (0x0042fd10, 0x0042fe40, 0x004301c0, 0x004302c0) drives CPacket message
// ids 0x32/0x34/0x35/0x36 through 0x00456360 and is left alone.  (0x004305f0
// onwards is NOT this block: it is objbank.cpp's w_IntersectSegment, whose
// two per-poly tests are 0x004308f0 (Full) and 0x004309d0 (Cheap), and
// 0x00430a60 its per-object callback.)
class CQueuedRecord
{
public:
	LTLink m_Link;                           // 0x00  m_pData points at itself
	uint32 m_nID;                            // 0x0c
	uint32 m_nUnknown10;                     // 0x10
	uint32 m_nUnknown14;                     // 0x14
	uint16 m_nFlags;                         // 0x18
};

class CRecordQueueView
{
public:
	uint8  m_pad_00[0x3c];
	LTLink m_Head;                           // 0x3c
	uint32 m_nUnknown48;                     // 0x48
	uint32 m_nUnknown4c;                     // 0x4c
	uint32 m_nUnknown50;                     // 0x50
	uint8  m_pad_54[0x80 - 0x54];
	uint32 m_nUnknown80;                     // 0x80
};

CQueuedRecord* sub_430190(CRecordQueueView* pQueue, uint16 nID);  // 0x00430190

// 0x00430080 -- 266 bytes, the queue's packet handler: servermessage.cpp's
// xfer_HandleMessage, whose CPacket::ReadType<uint16> this image expands
// (0x00468ed8 in sm_ProcessPackets is its one caller).  Written there; the
// transcription that stood here is gone.

// 0x00430190 -- 44 bytes.  Walk the circular list and answer the record
// whose id matches, or null.
//
// The id parameter is loaded from its slot and masked with `and $0xffff`
// ONCE, above the loop, and then compared against a full dword field -- so
// the parameter is a uint16 and the field is not.  The list-empty test is
// hoisted above that mask, which is why the two `xor %eax,%eax` / `ret`
// paths share one exit.
CQueuedRecord* sub_430190(CRecordQueueView* pQueue, uint16 nID)
{
	LTLink*        pCur;
	CQueuedRecord* pRecord;

	for (pCur = pQueue->m_Head.m_pNext;
	     pCur != &pQueue->m_Head;
	     pCur = pCur->m_pNext)
	{
		pRecord = (CQueuedRecord*)pCur->m_pData;

		if (pRecord->m_nID == nID)
			return pRecord;
	}

	return LTNULL;
}


// ---- the single registered hook ----------------------------------------- //
//
// 0x004b331c holds one record of 0x28 bytes: a pointer at +0x00, a word at
// +0x04, then the caller's 28-byte table of seven function pointers copied
// in whole at +0x08, then one dword at +0x24.  The three functions below are
// the whole of its life: create, dispatch, destroy.
//
// What it is for is not readable from this block -- nothing here names it and
// its function pointers all lead out of the range -- so the record, its
// fields and its slots keep their addresses for names.

// The 28 bytes memcpy'd in at +0x08.  Only two of the seven are called from
// this range: the first (record +0x08) with four arguments, and the fifth
// (record +0x18) with three.
class CHookFuncs
{
public:
	void (*m_pfn00)(void* pRec, void* pUnknown, uint32 nUnknown2,
	                uint32 nUnknown3);       // rec +0x08
	void*  m_pfn04;                          // rec +0x0c
	void*  m_pfn08;                          // rec +0x10
	void*  m_pfn0c;                          // rec +0x14
	void (*m_pfn10)(void* pRec, void* pUnknown, uint32 nUnknown);
	                                         // rec +0x18
	void*  m_pfn14;                          // rec +0x1c
	void*  m_pfn18;                          // rec +0x20
};

class CHookRecord
{
public:
	void*      m_pUnknown00;                 // 0x00
	uint16     m_nUnknown04;                 // 0x04
	uint16     m_nUnknown06;                 // 0x06
	CHookFuncs m_Funcs;                      // 0x08 .. 0x23
	uint32     m_nUnknown24;                 // 0x24
};

// Defined here, next to the log's file handle (0x004b3318, zeroed .bss):
// these three functions are its only users.
CHookRecord* g_pHookRecord;                  // 0x004b331c


// 0x0042f740 -- 57 bytes.  The zero fill and the copy are both inlined
// (`rep stosl` with ecx = 0xa, i.e. all forty bytes, then `rep movsl` with
// ecx = 7 into +0x08), and the global is only assigned on the success arm
// while `return` is shared -- `mov %edx,%eax` appears on both paths because
// the record pointer lives in edx throughout.
CHookRecord* sub_42f740(CHookFuncs* pFuncs)
{
	CHookRecord* pRecord;

	pRecord = (CHookRecord*)sub_428fd0(sizeof(CHookRecord));

	if (pRecord)
	{
		memset(pRecord, 0, sizeof(CHookRecord));
		memcpy(&pRecord->m_Funcs, pFuncs, sizeof(CHookFuncs));

		g_pHookRecord = pRecord;
	}

	return pRecord;
}


// 0x0042f780 -- 51 bytes.  Give the hook its close-down call if it has
// anything to close down, then release the record and clear the global.  The
// word at +0x04 is zero extended into a register before the push, which is
// what says the field is 16 bits and the parameter is not.
void sub_42f780(CHookRecord* pRecord)
{
	if (pRecord)
	{
		if (pRecord->m_pUnknown00)
		{
			pRecord->m_Funcs.m_pfn10(pRecord, pRecord->m_pUnknown00,
			                         pRecord->m_nUnknown04);
		}

		sub_429080(pRecord);

		g_pHookRecord = LTNULL;
	}
}


// 0x0042f920 -- 38 bytes.  The dispatch.  The arguments come out REVERSED:
// the pushes are (uint16)nA, nB, pC, record, so the callee sees
// (record, pC, nB, nA).  nA is masked with `and $0xffff` on the way through,
// so it arrives here as a uint16 and is widened for a 32-bit parameter.
void sub_42f920(uint16 nA, uint32 nB, void* pC)
{
	if (g_pHookRecord)
		g_pHookRecord->m_Funcs.m_pfn00(g_pHookRecord, pC, nB, nA);
}


// ======================================================================= //
//
//  DECODED BUT DELIBERATELY NOT WRITTEN
//
//  Everything below is understood well enough to be worth recording and not
//  well enough to be worth compiling.  A near costs more than a gap.
//
//  0x0042f120  217  "Pan" UpdateEffect.  Two accumulators advance by
//                   g_pClientMgr(0x004b17e0)->+0x1258 (the frame time) times
//                   m_fRate / m_fUnknown34, then
//                       pSurfaceData->O  = pSurfaceData->P * pData->+0x38;
//                       pSurfaceData->O += pSurfaceData->Q * pData->+0x3c;
//                       pSurfaceData->O += pData->O;
//                   and `return 1`.  Three stores to O, so it is three
//                   statements and not one expression -- but which of the
//                   two LTVector operand orders VC6 was given for each
//                   multiply is not decidable from the bytes, because the
//                   scalar is held in st(0) across all three components
//                   either way.
//
//  0x0042f4d0  191  "Warble" UpdateEffect.  Same accumulator, then
//                   fcos + 1.2f (0x004a22b8) scales P and fsin + 1.3f
//                   (0x004a22b4) scales Q.
//
//  0x0042e830  112  TextureData::~TextureData.  Now written (after the
//                   constructor above); both open questions were settled
//                   by building.
//
//  0x0042e8a0   51  TextureData's one virtual (the enumeration splits it into
//                   29 + 22 at 0x0042e8bd; it is one body, and its `ret $4`
//                   pair is the give-away).  Two separate calls to
//                   0x0042ee50, one with the literal 3 and one with the byte
//                   at +0x22 -- so the source really does write the call
//                   twice rather than using a ternary.  0x0042ee50 itself
//                   is now written (dtx_SetupFormat, above): the five
//                   uncleaned pushes are a thiscall virtual, PFormat::Init,
//                   with ecx still the format.  Now written too
//                   (TextureData::SetupPFormat, after dtx_SetupFormat).
//
//  0x0042f600   47  and 0x0042f630, 0x00430590, 0x004305b0, 0x004305d0.
//                   ALL FIVE HAVE THE SAME UNEXPLAINED SHAPE: a five-byte
//                   `jmp` forward, then eleven nops, then the body -- three
//                   dword stores for the first two, a bare `ret` for the
//                   last three.  If the bodies were separate functions
//                   /OPT:ICF would have folded the three identical `ret`s
//                   into one address, so entry and body are one COMDAT and
//                   therefore one function.  No source shape I can name
//                   produces `jmp` over its own alignment padding, and
//                   guessing at it would put five wrong bodies in the tree.
//
//  0x0042e110  287  the client's extract-to-temp helper (the counterpart of
//                   the `sub_1000e05b` counter.cpp declares), and
//  0x0042e230  813  InitClientShellDE.  Both are readable -- the second is
//                   cb_LoadServerObjects with "cshell.dll"/"cres.dll"/
//                   "cresl.dll"/"ClientShell" and LT_CANTCREATECLIENTSHELL
//                   in place of the server's names -- but they need
//                   CClientMgr fields at +0x610..+0x61c and +0x4c0 that
//                   clientmgr.h does not carry.  The right home for them is a client-side
//                   counterpart of counter.cpp's block, written against
//                   clientmgr.h once those fields are added.
//                   (0x0042e230 is now written above, after sub_42e110,
//                   through a view: a size-exact near.)
//
//  THE ENUMERATION HAS SIX PHANTOM ROWS.  0x0042ffe0 is one function running to
//  0x00430073 (its `ret` is at 0x00430073, not at 0x0042ffe0+33), and the
//  rows 0x00430001/14, 0x0043000f/11, 0x0043001a/14, 0x00430028/30 and
//  0x00430046/46 are all addresses inside it -- five of them mid
//  instruction.  0x0042e8bd is inside 0x0042e8a0 the same way, and
//  0x0042ebf7 is inside dtx_Create (0x0042eab0..0x0042edc1).  Nothing should
//  be written against any of those eight addresses.
//
// ======================================================================= //


// 0x0042e7b0, 21 bytes — the engine's own version, read out of its own
// module. The last function in this file's block: its final byte is
// 0x0042e7c4, which is exactly where the range declared at the top of this
// file ends.
//
//      8b 44 24 04         mov 0x4(%esp),%eax          ; pVersion
//      8b 0d 08 0b 4b 00   mov 0x004b0b08,%ecx         ; the module handle
//      50 51               push %eax ; push %ecx
//      e8 ..               call 0x0048a510
//      83 c4 08            add $0x8,%esp
//      c3                  ret
//
// WHY HERE AND NOT transform.cpp. That file carves out a "third module"
// holding 0x0048a4b0 and 0x0048a510 and lists 0x0042e7bc among its OUTSIDE
// callers — 0x0042e7bc is inside this row, so this is the caller, not part of
// that module. The global it reads, 0x004b0b08, has exactly one other reader
// in the image: 0x0042e730, dsi_GetInstanceHandle, ten instructions above and
// already counted as this file's.
//
// The argument is loaded BEFORE the global (0x0042e7b0 then 0x0042e7b4) and
// pushed first — plain right-to-left evaluation of a two-argument call, no
// local needed.
//
// 0x0048a510 reads the eight-byte RCDATA resource 0x303a through
// FindResourceA / LoadResource / SizeofResource / LockResource, so the
// version is a pair of dwords. EXTENT CORRECTION on that callee, filed while
// reading it: it is 121 bytes, not the 36 the CSV carries.
struct CEngineVersionQ42E
{
	uint32 m_nMajor;                         // 0x00
	uint32 m_nMinor;                         // 0x04
};

// 0x004b0b08 — the module handle dsi_GetInstanceHandle hands out
// (g_CluClientGlob.m_hInstance).

// 0x0048a510 — transform.cpp's pair-of-dwords resource reader.
LTRESULT ver_GetModuleVersionQ42E(void* hModule, CEngineVersionQ42E* pVersion);

LTRESULT dsi_GetEngineVersion(CEngineVersionQ42E* pVersion)
{
	return ver_GetModuleVersionQ42E(g_CluClientGlob.m_hInstance, pVersion);
}
