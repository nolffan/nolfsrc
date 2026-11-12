// *********************************************************************** //
//
//  MODULE:   CLU402F90 (.CPP)  -- lithtech.exe 0x00402f90 .. 0x0040488d
//
//  WHAT THIS RUN IS.  Two adjacent client-only modules that share one
//  contiguous .data literal pool (0x004a74e4 .. 0x004a779e).  The pool is
//  what names them: its literals appear in function-address order and the
//  run reads, top to bottom,
//
//      "%s%s" "-"                                (0x00402ec0, already sourced
//                                                 in src/engine/lithtech/ltnew2.cpp)
//      "Out of memory" "Running for %.1f seconds"
//      "Setting process to high priority" "breakonerror" "windowtitle"
//      "LithTech" "Unknown error initializing engine."
//      "Unable to load ltmsg.dll." "Error" "workingdir" "DebugStructBanks"
//      "LithTech build %s initialized in %.2f seconds.\n"
//      "autoexec.cfg" "config" "noinput" "-rez" "world" "host" "rb"
//      "%s%d.bmp"
//      "Lost focus... Sending LOSTFOCUS event.\n"
//      "Regained focus... Sending GAINEDFOCUS event.\n"
//      "Losing focus.. shutting down renderer.\n"
//      "Regained focus.. initializing renderer.\n"
//      "Unable to load launch.dll." "GetLithTechCommandLine" "launch.dll"
//      "launch" "Could not parse cmdfile." "r" "-cmdfile"
//                                                --- the WINDOWS CLIENT SHELL
//      "c:\\de_cache" "Unable to find server file: %s" "ShowTicks"
//                                                --- the CLIENT FILE MANAGER
//
//  So 0x00402f90..0x00403f8f is the client's WINDOWS ENTRY LAYER: WinMain,
//  the command-line parser and its -cmdfile reader, window class + main
//  message pump, WndProc, and the global ClientGlob at 0x004b0ac0 that all
//  of it writes.  In Monolith's tree this is sys/win/winclientde.cpp -- the
//  name is not in the image, so nothing below is named from it.
//
//  0x00403fd0..0x0040488d is the CLIENT FILE MANAGER, the 0xe88-byte object
//  CClientMgr keeps at +0x1534 (src/engine/lithtech/clientmgr.h already
//  declares that member as `CFileMgr* m_pFileMgr` and notes "the client
//  keeps its file manager behind a pointer; the server has one inline").
//  It is NOT the server's CFileMgr: src/engine/server/servermgr.h's is 0x3c
//  bytes with four fields, this one carries two struct-bank record pools,
//  a 100-bucket and a 200-bucket hash of LTLink chains, a string holder and
//  a "c:\de_cache" tree.  None of its strings exists in the retail server.dll
//  ("c:\\de_cache", "Unable to find server file: %s" and "ShowTicks" are all
//  absent), so it is client-only code and src/engine/server/filemgr.cpp --
//  which reconstructs the archive layer at server 0x1000xxxx -- does not
//  cover a byte of it.
//
//  ALREADY RECONSTRUCTED?  No.  Every address in the run was grepped across
//  src/engine/ and src/lithshared/: the only hits are DECLARATIONS of the
//  neighbours (clientmgr2.cpp declares sub_404480, clientinterface.cpp
//  declares sub_404610, ltnew2.cpp defines sub_00402f70 and sub_004041d0,
//  clientshell.cpp names 0x004040e0).  None of the fourteen bodies below has
//  a definition anywhere in the tree, and the server cross-check is vacuous
//  because none of these strings is in server.dll.
//
//  WHAT 0x00403dcf TURNED OUT TO BE.  Shape (a): an interior
//  arm cut off from its entry test.  The joined range
//  0x00403d60..0x00403e2e has THREE rets and 0x00403da0 looks like a
//  SEPARATE function after alignment padding; taken at
//  0x00403da0 size 143 the two remaining rets (0x00403dce, 0x00403e2d)
//  collapse into one body that ends flush at the stated end.  The function
//  is dsi_ParseCommandFile below: fopen fails, it puts up "Could not parse
//  cmdfile.", returns LTFALSE through the `ret` at 0x00403dce, and the
//  SUCCESS arm -- jumped to by the `75 16` at 0x00403db7 -- begins at
//  0x00403dcf with `push %ebx; push %edi`, which is what the enumerator read
//  as a prologue.  No padding, no shared fail block, no entry test: one
//  function, 0x00403da0..0x00403e2e, 143 bytes.
//
//  NAMING.  Everything file-scope here carries a `Clu` / `clu_` prefix or an
//  address-derived name so that nothing collides with the existing
//  src/engine/lithtech translation units, several of which already declare
//  g_hMainWnd, g_pWindowTitle, dalloc, dfree and sub_429080 at these same
//  addresses.  Relocations are masked by the verifier, so two spellings of
//  one global cost nothing.
//
//  Build /O2, like the rest of src/engine/lithtech.
//
//  ONE THING THAT IS NOT SOURCE-REACHABLE and is called out where it bites:
//  0x00402f90 and 0x004034b0..0x004034b0 aside, only TWO functions in the run
//  carry a `push %ebp; mov %esp,%ebp` frame, and both have a cause that the
//  source controls: 0x00402f90 calls setjmp (VC6 turns FPO off for any
//  function that does, because longjmp restores %ebp), and 0x00403330 has an
//  `__asm` block (src/engine/d3d/d3dbatchd.cpp also records that
//  __asm forces the frame).  Everything else in the run is frameless.
//
// *********************************************************************** //

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include "ltbasedefs.h"
#include "ltlink.h"
#include "nolfsrc_crash.h"     // debug build only: crash backtraces (src/nolfsrc/)

// windows.h renames engine methods out from under the SDK headers.
#undef PostMessage
#undef GetMessage
#undef PeekMessage
#undef CopyFile
#undef PlaySound
#undef GetClassName


// ======================================================================= //
//  Things this run calls but does not define
// ======================================================================= //

// The two allocator front ends, the pair-per-allocation rule that
// src/engine/lithtech/clientdsi.cpp spells out.  Which pair a line uses is
// read off the bytes: the command-line vector and the -cmdfile buffer go
// through dalloc/dfree, WinMain's argument block and the file manager's tree
// records through the inner pair.
// The outer pair IS the engine's global operator new / operator delete
// (dalloc.cpp; 0x004290b0 / 0x004290c0 are one-line forwards to the inner
// pair), so the clu_ spellings are those two called by name.
#define clu_dalloc(nSize)  operator new(nSize)               // 0x004290b0
#define clu_dfree(pData)   operator delete(pData)            // 0x004290c0
void* clu_sub_428fd0(unsigned long nSize);                   // 0x00428fd0
void  clu_sub_429080(void* pData);                           // 0x00429080

// 0x00402ec0 -- the command-line lookup worker.  ltnew2.cpp pins the
// prototype: __cdecl (pName, argc, argv), argc second, argv third, and it
// answers the NEXT entry after a hit, g_LTEmptyString when the hit is last,
// or NULL when there is no hit.
char* clu_sub_402ec0(char* pName, int argc, char** argv);    // 0x00402ec0

// 0x00402f70 -- the same lookup over the GLOBAL vector; ltnew2.cpp defines
// it as sub_00402f70.
char* clu_sub_402f70(char* pName);                           // 0x00402f70

// 0x0042e6f0 -- the CLIENT's dsi_ServerError, defined in
// src/engine/lithtech/clientdsi.cpp.  Formats into a 500-byte buffer and
// prints to the console at colour 0xffff00.
void clu_dsi_ServerError(char* pFormat, ...);                // 0x0042e6f0

// 0x0041b8c0 -- the console's white printf (colour 0xffffff, selector 0).
void clu_ConPrintf(char* pFormat, ...);                      // 0x0041b8c0

// 0x0042e6a0 -- clientmgr.h declares it; the message reporter.  WM_DESTROY
// passes a null.
// clientdsi.cpp defines it under that name.
void sub_42e6a0(char* pMsg);                                 // 0x0042e6a0
#define clu_sub_42e6a0 sub_42e6a0

// 0x0042e790 -- clientdsi.cpp defines it: MessageBoxA(g_hMainWnd, pText,
// pCaption, 0).
int clu_sub_42e790(char* pText, char* pCaption);             // 0x0042e790

// 0x0042d770 -- lt2new3.cpp pins it: the resource-DLL message formatter,
// __cdecl (pBuf, nBufSize, nMessageCode, va_list).
// servermsg.cpp defines it as dsi_SetupMessage (its answer unread here).
int dsi_SetupMessage(char* pBuf, int nBufSize, int nMessageCode, va_list args);
#define clu_sub_42d770 dsi_SetupMessage

// 0x0042d870 / 0x0042d8e0 -- engine start-up and teardown.  Init answers 0
// on success; the two error texts below say 1 means "ltmsg.dll".
int  clu_sub_42d870();                                       // 0x0042d870
void clu_sub_42d8e0();                                       // 0x0042d8e0

// 0x0042f740 / 0x0042f780 / 0x0042f7c0 / 0x0042f7d0 -- the message-record
// queue clientdsi.cpp's survey found at 0x0042f660..0x00430da7.  Create
// takes a 28-byte descriptor and copies it to the object's +0x08; the user
// pointer lives at +0x24 and 0x0042f7c0/0x0042f7d0 are its get/set.
struct CluQueueDesc
{
	int   (*m_pfnOnServerFile)(void* pQueue, char* pName, uint32 nUnused, uint32 nID);
	int   (*m_pfn04)();
	void  (*m_pfn08)();
	void  (*m_pfn0c)();
	void  (*m_pfn10)();
	void*  m_pNetMgr;
	int    m_nUnknown18;
};

void* clu_sub_42f740(CluQueueDesc* pDesc);                   // 0x0042f740
void  clu_sub_42f780(void* pQueue);                          // 0x0042f780
void* clu_sub_42f7c0(void* pQueue);                          // 0x0042f7c0
void  clu_sub_42f7d0(void* pQueue, void* pUser);             // 0x0042f7d0

// The archive/tree layer this run reaches.  0x00420b70 opens a tree by name
// and hands it back through its second argument (it clears *ppTree first);
// 0x00420cd0 closes one; 0x00420d30 answers the type word a tree opens with
// (1 for a plain directory) or 1 when there is no tree; 0x00420d50 fills a
// 0x110-byte find record for one name inside one tree.
struct CluFileInfo { uint8 m_Data[0x110]; };

int  clu_fs_OpenTree(char* pName, void** ppTree);            // 0x00420b70
void clu_fs_CloseTree(void* pTree);                          // 0x00420cd0
int  clu_fs_GetTreeType(void* pTree);                        // 0x00420d30
int  clu_fs_GetFileInfo(void* pTree, char* pName,
                        CluFileInfo* pInfo);                 // 0x00420d50

// 0x00436520 -- turn a set of open trees into the FileEntry chain
// ILTClient::GetFileList hands out (the client's own; the server's
// fs_BuildFileList is a different address entirely).
void* clu_fs_BuildFileList(void** pTrees, int nTrees, char* pDirName);

// stdlith's struct bank.  sb_Grow is the only piece of it that is a call
// here; sb_Allocate is inline in the header the client used, which is why
// the two record allocations below expand rather than call.
class CluStructBank
{
public:
	uint32 m_StructSize;                     // +0x00
	uint32 m_Unknown04;                      // +0x04
	uint32 m_GrowCount;                      // +0x08
	uint8  m_pad_0c[0x18 - 0x0c];
	void*  m_pFreeList;                      // +0x18
};

void clu_sb_Init(CluStructBank* pBank, uint32 nStructSize, uint32 nCache);
void clu_sb_Init2(CluStructBank* pBank, uint32 nStructSize, uint32 nCache,
                  uint32 nGrow);                             // 0x00490ba0
void clu_sb_FreeChunks(CluStructBank* pBank, int bAll);      // 0x00490bd0
void clu_sb_Grow(CluStructBank* pBank, uint32 nCount);       // 0x00490c20

inline void* clu_sb_Allocate(CluStructBank* pBank)
{
	void* pStruct = pBank->m_pFreeList;

	if (!pStruct)
	{
		clu_sb_Grow(pBank, pBank->m_GrowCount);

		pStruct = pBank->m_pFreeList;
		if (!pStruct)
			return LTNULL;
	}

	pBank->m_pFreeList = *(void**)pStruct;
	return pStruct;
}

// The 0x18-byte string holder the file manager keeps at +0x0c: a vptr
// (0x004a3c24) over four dwords and a uint16 chunk size at +0x14.
// It is stdlith's CStringHolder (StdLith.lib), so this is that class under
// its own name, cut to the four methods used.
class CStringHolder
{
public:
	CStringHolder();                         // 0x00490f60
	~CStringHolder();                        // 0x00490fe0
	void  SetAllocSize(uint16 nSize);        // 0x00491060
	// stdlith's CStringHolder::AddString(const char*, LTBOOL) -- the
	// parameter is const there, so it is here.
	char* AddString(const char* pString, int bUnique);   // 0x00491070

	void*  m_pVTable;                        // 0x00
	void*  m_pUnknown04;                     // 0x04
	void*  m_pUnknown08;                     // 0x08
	void*  m_pUnknown0c;                     // 0x0c
	void*  m_pUnknown10;                     // 0x10
	uint16 m_nChunkSize;                     // 0x14
	uint16 m_nUnknown16;                     // 0x16
};

// The two record pools.  Same vptr-over-StructBank shape servermgr.h calls
// CBankBase: base vtable 0x004a14c8 (slots 1..3 are _purecall), derived
// 0x004a14b8 and 0x004a14a8.  ltnew2.cpp already reads the same three
// tables out of the image.
class CluBankBase
{
public:
	virtual ~CluBankBase() {}
	virtual void* Allocate() = 0;
	virtual void  Free(void* pObj) = 0;
	virtual void  Init() = 0;
};

class CluFileRecBank : public CluBankBase                    // vtable 0x004a14b8
{
public:
	CluFileRecBank() { clu_sb_Init(&m_Bank, 0x20, 0x20); }
	virtual ~CluFileRecBank() { clu_sb_FreeChunks(&m_Bank, 1); }
	virtual void* Allocate();                // 0x004049d0
	virtual void  Free(void* pObj);          // 0x004049c0
	virtual void  Init();                    // 0x00444820 (ICF)

	CluStructBank m_Bank;                    // +0x04
	uint32        m_Unknown20;               // +0x20
};

class CluNetFileRecBank : public CluBankBase                 // vtable 0x004a14a8
{
public:
	CluNetFileRecBank() { clu_sb_Init(&m_Bank, 0x28, 0x20); }
	virtual ~CluNetFileRecBank() { clu_sb_FreeChunks(&m_Bank, 1); }
	virtual void* Allocate();                // 0x004049d0 (ICF)
	virtual void  Free(void* pObj);          // 0x004049c0 (ICF)
	virtual void  Init();                    // 0x00404a00

	CluStructBank m_Bank;                    // +0x04
	uint32        m_Unknown20;               // +0x20
};

// The banks' three own slots, read off the two vtables: slot 1 at 0x004049d0
// (the struct-bank pop, sb_Allocate), slot 2 at 0x004049c0 (push back on the
// free list at +0x1c), slot 3 the reset -- free every chunk, then sb_Init
// again with the record size (0x00404a00 with 0x28; 0x00444820, the
// 0x20-byte one, is objbank.cpp's CObjectBank<CBankElem20>::Init, which
// /OPT:ICF kept in place of this body).  Slots 1 and 2 are the same bytes for
// both classes and for every objbank.cpp instantiation; ICF keeps the first,
// in this run.
void* CluFileRecBank::Allocate()
{
	return clu_sb_Allocate(&m_Bank);
}

void CluFileRecBank::Free(void* pObj)
{
	*(void**)pObj = m_Bank.m_pFreeList;
	m_Bank.m_pFreeList = pObj;
}

void CluFileRecBank::Init()
{
	clu_sb_FreeChunks(&m_Bank, 1);
	clu_sb_Init(&m_Bank, 0x20, 0x20);
}

void* CluNetFileRecBank::Allocate()
{
	return clu_sb_Allocate(&m_Bank);
}

void CluNetFileRecBank::Free(void* pObj)
{
	*(void**)pObj = m_Bank.m_pFreeList;
	m_Bank.m_pFreeList = pObj;
}

void CluNetFileRecBank::Init()
{
	clu_sb_FreeChunks(&m_Bank, 1);
	clu_sb_Init(&m_Bank, 0x28, 0x20);
}

// The game DLL's interface, reached ONLY through its vtable and only from
// two slots.  Both call sites are `mov <shell>,%ecx; mov (%ecx),%edx; push
// ...; call *0xNN(%edx)` with NO caller cleanup, so these are __thiscall
// virtuals and the arguments below are exactly what each site pushes --
// 0x4c takes two, 0x60 takes three.  The slots in between are declared only
// to put those two at the right index (0x4c/4 = 19, 0x60/4 = 24); nothing
// here claims to know what they are.
class CluClientShell
{
public:
	virtual void Slot00() = 0;  virtual void Slot01() = 0;
	virtual void Slot02() = 0;  virtual void Slot03() = 0;
	virtual void Slot04() = 0;  virtual void Slot05() = 0;
	virtual void Slot06() = 0;  virtual void Slot07() = 0;
	virtual void Slot08() = 0;  virtual void Slot09() = 0;
	virtual void Slot10() = 0;  virtual void Slot11() = 0;
	virtual void Slot12() = 0;  virtual void Slot13() = 0;
	virtual void Slot14() = 0;  virtual void Slot15() = 0;
	virtual void Slot16() = 0;  virtual void Slot17() = 0;
	virtual void Slot18() = 0;
	virtual void OnEvent(int nEventCode, int nParam) = 0;    // +0x4c
	virtual void Slot20() = 0;  virtual void Slot21() = 0;
	virtual void Slot22() = 0;  virtual void Slot23() = 0;
	virtual void OnMessage(int nCode, char* pName, int nParam) = 0;   // +0x60
};

// The object CClientMgr keeps at +0x1614: one virtual is reached, slot
// 0x1c (index 7), with no arguments.
class CluUnknown1614
{
public:
	virtual void Slot00() = 0;  virtual void Slot01() = 0;
	virtual void Slot02() = 0;  virtual void Slot03() = 0;
	virtual void Slot04() = 0;  virtual void Slot05() = 0;
	virtual void Slot06() = 0;
	virtual void OnFocusGained() = 0;                        // +0x1c
};

// The client sound manager, reached as a SUB-OBJECT at CClientMgr+0x774 and
// as the whole of ci_GetSoundMgr's answer.  All three entries are
// __thiscall with no arguments (`mov ...,%ecx; call`, nothing pushed), so
// they are methods and not the cdecl helpers a first reading suggests.
// They are clientsoundmgr.cpp's CSoundMgrView methods under that file's
// names (each returns an LTRESULT nobody here reads), so this is that class,
// cut to the three.
class CSoundMgrView
{
public:
	uint32 sub_481e70();                     // 0x00481e70  pause
	uint32 sub_481ef0();                     // 0x00481ef0  resume
	uint32 StopAllSounds();                  // 0x00480ca0
};

// CClientMgr+0x11d0: `sprintf(buf, "%lu.%lu", this[1], this[0])` bounded by
// the caller's size.  __thiscall, two arguments.
class CluVersionView
{
public:
	void GetString(char* pBuf, int nSize);   // 0x0048a4b0
};

// The music manager ci_GetMusicMgr answers with.  +0x44 is called with ONE
// argument and cleaned by the caller (`push $0; call *0x44(%eax); add
// $0x4,%esp`) -- a cdecl function POINTER field, not a virtual: a virtual
// would have gone through `mov (%eax),%ecx` first.
struct CluMusicMgrView
{
	uint8 m_pad_00[0x44];
	void (*m_pfnUnknown44)(int);             // 0x44
	uint8 m_pad_48[0x5c - 0x48];
	void* m_pUnknown5c;                      // 0x5c
};

// CClientMgr+0x1508's function table (clientmgr.h's CClientInput), cut to
// the one entry ClearInput calls: `call *0x1c(%eax)` with nothing pushed.
struct CluInputView
{
	uint8 m_pad_00[0x1c];
	void (*m_pfnClearInput)();               // 0x1c
};

// The client manager, opaque except for the six offsets this run reads.
class CluClientMgrView
{
public:
	LTRESULT Init(char** pRezFiles, uint32 nRezFiles, char* pConfigFile);
	LTRESULT Update();                                       // 0x004106c0
	void     Term();                                         // 0x00410ec0
	LTRESULT ClearInput();                                   // 0x00410900
	// clientmgr3.cpp's CCmgClientMgr::UpdateCounters.  __thiscall.
	void     UpdateCounters(LTBOOL bForce);                  // 0x00410860

	uint8            m_pad_000[0x50c];
	uint8            m_NetMgr[0x61c - 0x50c];        // 0x50c
	CluClientShell*  m_pClientShell;                 // 0x61c
	uint8            m_pad_620[0x774 - 0x620];
	uint8            m_SoundMgr[0x898 - 0x774];      // 0x774
	void*            m_pUnknown898;                  // 0x898
	uint8            m_pad_89c[0x11d0 - 0x89c];
	uint8            m_Version[0x1508 - 0x11d0];     // 0x11d0
	// clientmgr.h's m_pInput: a table of plain cdecl function pointers.
	CluInputView*    m_pInput;                       // 0x1508
	uint8            m_pad_150c[0x15a4 - 0x150c];
	// Inside clientmgr3.cpp's CCmgDemoMgr (0x158c) at its +0x18.
	uint32           m_Unknown15a4;                  // 0x15a4
	uint8            m_pad_15a8[0x1614 - 0x15a8];
	CluUnknown1614*  m_pUnknown1614;                 // 0x1614
};

// clientdsi.cpp's names for the two queue clears.
void sub_42e5e0();                                           // 0x0042e5e0
void sub_42e5f0();                                           // 0x0042e5f0
void dsi_Sub42e600();                                        // 0x0042e600

// ======================================================================= //
//  0x00410900 -- CClientMgr::ClearInput, 48 bytes.  REAL HOME: the
//  CClientMgr TU clientmgr3.cpp (between UpdateCounters 0x00410860 and
//  PlaySound 0x00410960); written here because this file owns its callers.
//  Off the bytes: a positive guard on +0x15a4 (`jne` straight to the single
//  `xor %eax,%eax` at 0x0041092c), then UpdateCounters(TRUE) with ecx still
//  `this`, the three dsi queue flushes, and the input table's +0x1c entry
//  called with nothing pushed.
// ======================================================================= //
LTRESULT CluClientMgrView::ClearInput()
{
	if (!m_Unknown15a4)
	{
		UpdateCounters(LTTRUE);             // 0x0041090f
		sub_42e5e0();                       // 0x00410914
		sub_42e5f0();                       // 0x00410919
		dsi_Sub42e600();                    // 0x0041091e
		m_pInput->m_pfnClearInput();        // 0x00410929
	}

	return LT_OK;
}

CluClientMgrView* clu_sub_410a70();                          // 0x00410a70
void clu_r_TermRender(CluClientMgrView* pMgr, int nReason);  // 0x004627d0
// clientrender.cpp's r_InitRender, clientinterface.cpp's two getters: their
// own declarations, the real classes forward-declared (the views above are
// cast to).
class  CClientMgrView;
struct RMode;
class  ILTClientSoundMgr;
class  CClientMusicMgr;
uint32             r_InitRender(CClientMgrView* pMgr, RMode* pMode);   // 0x004623d0
ILTClientSoundMgr* ci_GetSoundMgr();                         // 0x0040f7f0
CClientMusicMgr*   ci_GetMusicMgr();                         // 0x0040f800

// 0x00403f90 -- the path normaliser this run's two lookups share: copy
// nBytes, force the last byte to 0, upper-case in place, then turn every
// '/' into '\\'.  Defined below, ahead of 0x00403fd0.
void clu_sub_403f90(char* pSrc, char* pDest, int nBytes);    // 0x00403f90

// 0x004a02ed -- strlen / malloc / strcpy: the CRT's _strdup (string.h).
#define clu_strdup _strdup                                   // 0x004a02ed

// 0x00404410 -- the by-id probe over the 100-bucket table (defined
// below, ahead of 0x00404520).  Its parameter is a
// uint16: the callee does the `and $0xffff` itself (then the SIGNED divide
// of int promotion) and compares with `cmp %si,0x10(%eax)`, and both
// callers push without widening -- 0x0040469a loads the ident's word with
// `mov 0x8(%esi),%ax` straight into the push, and 0x004042e2 pushes
// 0x00404280's uint32 id unmasked.
struct CluNetFileRec;
class  CluFileMgr;
CluNetFileRec* clu_sub_404410(CluFileMgr* pFileMgr, uint16 nID);

// 0x00404460 / 0x00404470 -- the queue descriptor's other four slots:
// `xor %eax,%eax; ret` and a bare `ret`.  The last three slots are THREE
// separate empty functions that the linker's /OPT:ICF folded onto
// 0x00404470 (the same address also answers ci_DeleteColor, cc_BlastServer,
// rs_Empty and cmg_Handler404470).  0x0040424a..0x0040425a store the address
// as three separate immediates; one function named three times is CSE'd
// into %eax by /O2 instead.
//
// They are this run's own (0x00404460/0x00404470 sit between 0x00404410 and
// 0x00404480), so they are defined here, and their shipped bodies are what
// is written: `xor %eax,%eax; ret` and `ret`.
int  clu_sub_404460() { return 0; }                          // 0x00404460
void clu_sub_404470() {}                                     // 0x00404470
void clu_fm_Empty0c() {}                                     // 0x00404470 (ICF)
void clu_fm_Empty10() {}                                     // 0x00404470 (ICF)


// ======================================================================= //
//
//  THE CLIENT GLOB -- 0x004b0ac0, 0x924 bytes
//
//  WHAT PINS THE SIZE AND THE FIRST TWO FIELDS.  0x00402f90 opens with
//  `mov $0x4b0ac0,%edi; mov $0x249,%ecx; xor %eax,%eax; rep stos` -- an
//  inlined memset of 0x249*4 = 0x924 bytes -- and then hands 0x004b0ac4 to
//  0x00492a18, which stores %ebp/%ebx/%edi/%esi/%esp, the return address and
//  the literal 'VC20' and reads %fs:0.  That is _setjmp3(env, 0), so +0x04
//  is a jmp_buf; VC6's is sixteen dwords, which ends it at +0x44 -- exactly
//  where the HWND starts.  0x004b0ac0 + 0x924 = 0x004b13e4, and 0x004b13e4
//  is the separate start-time dword WinMain writes, so the struct ends
//  flush.
//
//  THE OFFSETS THAT ALREADY HAVE NAMES ELSEWHERE.  clientmgr.h calls
//  0x004b0b04 g_hMainWnd and clientdsi.cpp calls 0x004b0b10 g_pWindowTitle;
//  both are inside this struct (+0x44 and +0x50).  0x004b0b14 is the
//  CClientMgr* the WndProc reads through -- the same dword 0x00402f90 fills
//  from 0x00410a70 at +0x54.
//
// ======================================================================= //

// The struct itself is in cluglob.h, so the other TUs that read its fields
// share the one type.  DEFINED here: this run owns it.
#include "cluglob.h"

CluClientGlob g_CluClientGlob;               // 0x004b0ac0

// The command-line vector 0x00403bc0 builds and 0x00403d60 releases.  Both
// are loaded as values, so argv is a `char**` variable and not an array;
// ltnew2.cpp reads the same two globals the same way.
// Defined here (ltnew2.cpp's g_pLTArgList / g_nLTArgCount are these).
char** g_pCluArgList;                        // 0x004b0ab8
int    g_nCluArgCount;                       // 0x004b13e8

// 0x004b13e4 -- timeGetTime() at the moment WinMain finished parsing.  The
// "Running for %.1f seconds" line subtracts it.
uint32 g_nCluStartTime;                      // 0x004b13e4

// 0x004b3238 / 0x004b13ec -- how many times to run the client, and which run
// this is.  The limit is compared against 1 with `jle` (signed int) and
// against the counter with `jae`/`jb` (so the counter is unsigned).
// The limit is dsi.cpp's "PlayDemoReps" console variable; the counter is
// this run's own (demodtx.cpp's g_nCluCurRunQC5 is it).
extern int    g_CV_PlayDemoReps;             // 0x004b3238  (dsi.cpp)
#define g_nCluNumRuns g_CV_PlayDemoReps
uint32 g_nCluCurRun;                         // 0x004b13ec

// Console variables the pump reads every frame.  Each is compared against a
// zeroed register, never loaded for its value, so all that is known is that
// they are dwords.
// They are dsi.cpp's console bindings (lithtech's copy of the table lays
// them out as server.dll's does): "HighPriority", "ShowRunningTime",
// "CursorCenter" and "NullRender".
extern int g_CV_HighPriority;                // 0x004b32a8
extern int g_CV_ShowRunningTime;             // 0x004b32a0
extern int g_CV_CursorCenter;                // 0x004a9ec4
extern int g_CV_NullRender;                  // 0x004b32a4
#define g_bCluHighPriority  g_CV_HighPriority
#define g_bCluShowTicks     g_CV_ShowRunningTime
#define g_bCluCenterCursor  g_CV_CursorCenter
#define g_bCluFocusSimple   g_CV_NullRender

// 0x004b4d80 -- set to 1 when "DebugStructBanks" is on the command line.
// It is stdlith's own flag (struct_bank.cpp, in StdLith.lib).
extern int g_bDebugStructBanks;              // 0x004b4d80
#define g_bCluDebugStructBanks g_bDebugStructBanks

// 0x004b439c -- the render-up flag, g_RenderStruct's +0x44 (m_bInited in
// clientrender.cpp); reached through the view below, g_bCluRenderInited.

// 0x004b4140 -- the RMode the focus handler re-initialises the renderer
// with.  Its address is pushed, so it is an object and not a pointer.
// clientrender's g_CurRenderMode (an RMode, forward-declared above).
extern RMode g_CurRenderMode;                // 0x004b4140

// The three 100-entry key queues and their two uint16 counts, already
// declared at these addresses in src/engine/lithtech/clientdsi.cpp -- this
// is the code that FILLS them.
// They are FIELDS of the glob (0x00402f90's memset covers them), so these
// names are its members.
#define g_CluKeyDownVK   g_CluClientGlob.m_KeyDownVK      // 0x004b0d34
#define g_CluKeyUpVK     g_CluClientGlob.m_KeyUpVK        // 0x004b0ec4
#define g_CluKeyDownRep  g_CluClientGlob.m_KeyDownRep     // 0x004b1054
#define g_nCluKeyDowns   g_CluClientGlob.m_nKeyDowns      // 0x004b11e4
#define g_nCluKeyUps     g_CluClientGlob.m_nKeyUps        // 0x004b11e6

// The screen-shot pair.  0x004b4358 is tested by its ADDRESS (`mov
// $0x4b4358,%edx; test %edx,%edx`), which is what an `if (array)` on a
// file-scope array compiles to -- VC6 keeps the test even though it can
// never fail.  0x004a9f18 holds the mutable base name "Screenshot" and
// 0x004b444c is the renderer's own bitmap writer, reached through a data
// slot rather than a direct call.
//
// 0x004b4358 is clientrender.cpp's g_RenderStruct and 0x004b444c its +0xf4
// entry (clientworldinfo.cpp's table: referenced once, from 0x00403973), so
// the "directory" test is the address of the render struct and the writer
// is its member; 0x004a9f18 is clientconsole.cpp's g_szScreenshotName.
class RenderStruct;
extern RenderStruct g_RenderStruct;          // 0x004b4358
struct CluRenderStructView
{
	uint8 m_pad_00[0x44];
	int   m_bInited;                                         // +0x44  0x004b439c
	uint8 m_pad_48[0xf4 - 0x48];
	void (*m_pfnMakeScreenShot)(char* pFilename);            // +0xf4  0x004b444c
};
extern char  g_szScreenshotName[];           // 0x004a9f18
#define g_CluScreenShotDir    ((CluRenderStructView*)&g_RenderStruct)
#define g_CluScreenShotName   g_szScreenshotName
#define g_pCluMakeScreenShot  (g_CluScreenShotDir->m_pfnMakeScreenShot)
#define g_bCluRenderInited    (g_CluScreenShotDir->m_bInited)


// The argument block WinMain builds on its own stack and hands to the two
// workers.  0x004039f0 writes all three fields (+0x00 the vector, +0x04 the
// count, +0x08 the one block every string lives in), and WinMain frees +0x00
// and +0x08 on every exit.
struct CluCommandLineArgs
{
	char** m_pArgs;                          // 0x00
	int    m_nArgs;                          // 0x04
	char*  m_pArgBuffer;                     // 0x08
};

// launch.dll's export, called with six arguments and cleaned by the caller.
typedef int (*CluGetLithTechCommandLineFn)(int argc, char** argv, int* pnArgs,
                                           char** pArgs, int nMaxArgs,
                                           int nMaxArgLen);


// ======================================================================= //
//
//  1. THE WINDOWS ENTRY LAYER   0x00402f90 .. 0x00403f8f
//
// ======================================================================= //

LTBOOL clu_dsi_InitGame(CluClientGlob* pGlob, CluCommandLineArgs* pArgs);   // 0x00403330
LRESULT CALLBACK clu_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LTBOOL clu_dsi_BuildCommandLine(CluCommandLineArgs* pArgs);                 // 0x004039f0
void   clu_dsi_ParseCommandLine(char* pCmdLine);                            // 0x00403bc0
void   clu_dsi_FreeCommandLine();                                           // 0x00403d60
bool   clu_dsi_ParseCommandFile(char* pFilename);                           // 0x00403da0


// ----------------------------------------------------------------------- //
//  0x00402f90 -- 918 bytes, 42 calls.  Register the window class, put the
//  window up, run the message pump until CClientMgr::Update says stop, then
//  tear the whole thing down.
//
//  WHAT PINS IT
//    * the ONLY function in the run with a frame pointer that is not an
//      __asm function.  `push %ebp; mov %esp,%ebp; sub $0x78,%esp` and
//      `mov %ebp,%esp; pop %ebp` at both exits: VC6 disables FPO for any
//      function that calls setjmp, because longjmp restores %ebp.  The
//      setjmp is at 0x00402fde.
//    * two parameters, read at 0x8(%ebp) and 0xc(%ebp); the first is stored
//      into the glob's +0x48 and then handed to RegisterClass as
//      WNDCLASS::hInstance, so it is the HINSTANCE.
//    * `neg %eax; sbb %eax,%eax; neg %eax` at 0x004030a3 is `!!x`, so
//      m_bBreakOnError is an int holding a 0/1 and not the pointer itself.
//    * the PeekMessageA import slot is loaded ONCE, at 0x004031a1, before
//      the pump -- a local function pointer, the same idiom clientdsi.cpp's
//      sub_42e600 uses.  A loop-invariant hoist would have re-loaded it.
//    * the desktop RECT is taken once at start-up and the CURSOR is centred
//      from it, while the window's own rect (a second RECT) is filled every
//      frame and never read.  That is in the shipped bytes -- 0x00403219
//      passes -0x34(%ebp) to GetWindowRect and 0x00403224 reads -0x18(%ebp),
//      which is the OTHER rect -- and it is preserved here rather than
//      tidied, because tidying it would delete a call.
//    * the two start-up MessageBoxA calls share `push $0` / `push "Error"`
//      before the branch and `push $0` / `call` after it: VC6 tail-merged
//      two separate calls that differ in one argument.
//    * `cmp %esi,%eax` (not `test`) after the DebugStructBanks lookup at
//      0x00402fc6: the result goes through the pStr local that the
//      workingdir lookup reuses.  "Running for" multiplies with `fmuls` --
//      the product is float, promoted only for the varargs push.  The
//      shutdown keeps m_pClientMgr in %edi across Term and dfree, so it
//      is read once into a local.
// ----------------------------------------------------------------------- //
int clu_dsi_RunClientApp(HINSTANCE hInstance, CluCommandLineArgs* pArgs)
{
	CluClientGlob* pGlob;
	WNDCLASS       wndClass;
	MSG            msg;
	RECT           rcWindow;
	RECT           rcDesktop;
	int            nRet;
	int            bOutOfMemory;
	int            nErr;
	char*          pStr;
	CluClientMgrView* pClientMgr;
	BOOL (WINAPI *pfnPeekMessage)(LPMSG, HWND, UINT, UINT, UINT);

	pGlob = &g_CluClientGlob;
	memset(pGlob, 0, sizeof(CluClientGlob));

	nRet = 0;

	pStr = clu_sub_402ec0("DebugStructBanks", pArgs->m_nArgs, pArgs->m_pArgs);
	if (pStr)
		g_bCluDebugStructBanks = 1;

	bOutOfMemory = 0;
	if (setjmp(g_CluClientGlob.m_MemoryJmp))
	{
		bOutOfMemory = 1;
		goto shutdown;
	}

	pStr = clu_sub_402ec0("workingdir", pArgs->m_nArgs, pArgs->m_pArgs);
	if (pStr)
		SetCurrentDirectoryA(pStr);

	pGlob->m_bInputEnabled = 1;

	nErr = clu_sub_42d870();
	if (nErr)
	{
		if (nErr == 1)
			MessageBoxA(NULL, "Unable to load ltmsg.dll.", "Error", 0);
		else
			MessageBoxA(NULL, "Unknown error initializing engine.", "Error", 0);

		clu_sub_42d8e0();
		return -1;
	}

	pGlob->m_hInstance = hInstance;
	pGlob->m_pClassName = "LithTech";
	pGlob->m_pClientMgr = clu_sub_410a70();
	pGlob->m_nUnknown60 = 1;

	pGlob->m_pWindowTitle = clu_sub_402ec0("windowtitle", pArgs->m_nArgs, pArgs->m_pArgs);
	if (!pGlob->m_pWindowTitle)
		pGlob->m_pWindowTitle = "LithTech";

	pGlob->m_bBreakOnError =
		(clu_sub_402ec0("breakonerror", pArgs->m_nArgs, pArgs->m_pArgs) != NULL);

	wndClass.style         = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;   // 0xb
	wndClass.lpfnWndProc   = clu_WndProc;
	wndClass.cbClsExtra    = 0;
	wndClass.cbWndExtra    = 0;
	wndClass.hInstance     = pGlob->m_hInstance;
	wndClass.hIcon         = LoadIconA(NULL, IDI_APPLICATION);
	wndClass.hCursor       = LoadCursorA(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.lpszMenuName  = NULL;
	wndClass.lpszClassName = pGlob->m_pClassName;
	RegisterClassA(&wndClass);

	GetWindowRect(GetDesktopWindow(), &rcDesktop);

	pGlob->m_hMainWnd = CreateWindowExA(0,
		pGlob->m_pClassName,
		pGlob->m_pWindowTitle,
		0x10810000,
		((rcDesktop.right - rcDesktop.left) - 320) / 2,
		((rcDesktop.bottom - rcDesktop.top) - 200) / 2,
		320,
		200,
		NULL,
		NULL,
		pGlob->m_hInstance,
		NULL);

	if (!clu_dsi_InitGame(pGlob, pArgs))
		goto shutdown;

	pGlob->m_bWindowUp = 1;
	ShowWindow(pGlob->m_hMainWnd, SW_SHOWNORMAL);
	UpdateWindow(pGlob->m_hMainWnd);

	pfnPeekMessage = PeekMessageA;

	while (pGlob->m_pClientMgr->Update() == 0)
	{
		if (g_bCluHighPriority)
		{
			clu_dsi_ServerError("Setting process to high priority");
			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		}

		if (g_bCluShowTicks)
		{
			clu_dsi_ServerError("Running for %.1f seconds",
			                    (timeGetTime() - g_nCluStartTime) * 0.001f);
		}

		if (g_bCluCenterCursor && !g_CluClientGlob.m_bAppLostFocus)
		{
			GetWindowRect(pGlob->m_hMainWnd, &rcWindow);
			SetCursorPos((rcDesktop.right - rcDesktop.left) / 2,
			             (rcDesktop.bottom - rcDesktop.top) / 2);
		}

		while (pfnPeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				nRet = msg.wParam;
				goto shutdown;
			}

			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}

shutdown:
	if (pGlob->m_ErrorString[0])
	{
		clu_r_TermRender(pGlob->m_pClientMgr, 2);
		clu_sub_42e790(pGlob->m_ErrorString, pGlob->m_pWindowTitle);
	}

	pGlob->m_bWindowUp = 0;

	pClientMgr = pGlob->m_pClientMgr;
	if (pClientMgr)
	{
		pClientMgr->Term();
		clu_dfree(pClientMgr);
	}

	pGlob->m_pClientMgr = NULL;
	DestroyWindow(pGlob->m_hMainWnd);
	clu_sub_42d8e0();

	if (bOutOfMemory)
	{
		ShowWindow(pGlob->m_hMainWnd, SW_HIDE);
		MessageBoxA(GetDesktopWindow(), "Out of memory", pGlob->m_pWindowTitle, 0);
	}

	return nRet;
}


// ----------------------------------------------------------------------- //
//  0x00403330 -- 372 bytes, 12 calls.  Read the game options off the
//  command line and bring CClientMgr up.
//
//  WHAT PINS IT
//    * the frame pointer and the 16-bit local at -0x2(%ebp) are the __asm
//      block: `fstcw`, `andw $0xfcff` and `fldcw` on the same two bytes.
//      0xfcff clears the x87 control word's PC field, i.e. 24-bit
//      (single) precision.  No C construct emits that trio, and __asm is
//      what forces `push %ebp` under /O2.
//    * two parameters: 0x8(%ebp) is written at +0x74/+0x7c/+0x72c and read
//      at +0x54, so it is the glob; 0xc(%ebp) is dereferenced at +0 and +4
//      for the vector and the count, so it is the argument block.
//    * `neg/sbb/neg` again at 0x0040335d: m_bHost is an int 0/1.
//    * the -rez scan bounds itself with `cmp $0x14,%ebx; jae`, an UNSIGNED
//      compare, so the counter is a uint32; the outer bound reloads
//      pArgs->m_nArgs every iteration and compares `jl`, so the index and
//      the count are signed ints.
//    * 0x0040345b's `mov 0x54(%esi),%ecx; add $0x11d0,%ecx` with two stack
//      arguments and no caller cleanup is a __thiscall on a sub-object of
//      CClientMgr at +0x11d0 -- the dword after clientmgr.h's SkyDef.
//    * `movl $0x0,-0x8(%ebp)` before `fildll -0xc(%ebp)` is VC6's
//      unsigned-int-to-double: zero the high dword and load the quadword;
//      the multiply by 0.001f is `fmuls`, so the product is float.
//    * the -rez collector is indexed (`pRezFiles[nRezFiles]`): VC6's own
//      strength-reduced pointer lives in the dead pArgs slot and the count
//      in %ebx, the reverse of what an explicit write pointer gives.
// ----------------------------------------------------------------------- //
LTBOOL clu_dsi_InitGame(CluClientGlob* pGlob, CluCommandLineArgs* pArgs)
{
	char*  pRezFiles[20];
	char   szVersion[32];
	char*  pStr;
	char*  pConfigFile;
	uint32 nRezFiles;
	int    i;
	uint32 nStartTime;
	short  nControlWord;

	__asm
	{
		fstcw nControlWord
		and   nControlWord, 0fcffh
		fldcw nControlWord
	}

	pGlob->m_bHost =
		(clu_sub_402ec0("host", pArgs->m_nArgs, pArgs->m_pArgs) != NULL);

	pGlob->m_pWorldName =
		clu_sub_402ec0("world", pArgs->m_nArgs, pArgs->m_pArgs);

	nRezFiles = 0;

	for (i = 0; i < pArgs->m_nArgs - 1; i++)
	{
		if (stricmp(pArgs->m_pArgs[i], "-rez") == 0)
		{
			pRezFiles[nRezFiles] = pArgs->m_pArgs[i + 1];
			nRezFiles++;
			if (nRezFiles >= 20)
				break;
		}
	}

	if (clu_sub_402ec0("noinput", pArgs->m_nArgs, pArgs->m_pArgs))
		pGlob->m_bInputEnabled = 0;

	pStr = clu_sub_402ec0("workingdir", pArgs->m_nArgs, pArgs->m_pArgs);
	if (pStr)
		SetCurrentDirectoryA(pStr);

	pConfigFile = clu_sub_402ec0("config", pArgs->m_nArgs, pArgs->m_pArgs);
	if (!pConfigFile)
		pConfigFile = "autoexec.cfg";

	nStartTime = timeGetTime();

	if (pGlob->m_pClientMgr->Init(pRezFiles, nRezFiles, pConfigFile))
		return LTFALSE;

	((CluVersionView*)pGlob->m_pClientMgr->m_Version)->GetString(szVersion, 32);

	printf("LithTech build %s initialized in %.2f seconds.\n",
	       szVersion,
	       (timeGetTime() - nStartTime) * 0.001f);

	return LTTRUE;
}


// ----------------------------------------------------------------------- //
//  0x004034b0 -- 1335 bytes, 35 calls.  The window procedure.
//
//  WHAT PINS IT
//    * `ret $0x10` and four stack reads: the standard WndProc signature.
//    * the switch is VC6's two-level form -- `cmp $0x100` splits it, the
//      low group goes through a 27-entry BYTE table at 0x004039cc into a
//      5-entry DWORD table at 0x004039b8, and the high group is an
//      if-chain (`sub $0x101`, `dec`, `sub $0x10`).  The byte table's
//      non-default entries are at indices 0, 13, 18 and 26 of `uMsg - 2`,
//      i.e. 0x02 WM_DESTROY, 0x0f WM_PAINT, 0x14 WM_ERASEBKGND and 0x1c
//      WM_ACTIVATEAPP; the high arm is 0x100/0x101/0x102/0x112.
//    * WM_CHAR's arm is a `je` into the middle of WM_DESTROY's tail: two
//      `return 0`s tail-merged, which is what says WM_CHAR's body is empty.
//    * every focus exit answers `sete` on `uMsg == 0x86`.  0x86 is
//      WM_NCACTIVATE, which this switch never routes here, so the four
//      copies are one source-level `return (uMsg == WM_NCACTIVATE);` that
//      VC6 duplicated into each tail.  It is written once, below.
//    * ci_GetSoundMgr/ci_GetMusicMgr are CALLED AGAIN for every use rather
//      than cached -- three calls to 0x0040f800 around two tests and one
//      indirect call.  binkvideo.cpp records the same idiom; the repeat is
//      in the source, not a rematerialisation.
//    * the key queues index with the uint16 count zero-extended
//      (`and $0xffff`) and bump it with `incw`, so the counts are uint16
//      and the compare `cmpw $0x64 ... jae` is unsigned against 100.
//    * the not-ready test GUARDS the switch rather than returning early:
//      both its `je`s land on the one DefWindowProcA call at 0x004038e2,
//      which is why hWnd (%ebp) and wParam (%edi) are loaded above the
//      tests.  An early `return DefWindowProcA(...)` is a second call and
//      loads them after.
//    * the focus handler reaches its one `return (uMsg == WM_NCACTIVATE)`
//      from every path through an if/else nest; with the return written at
//      each exit uMsg took a register of its own (four pushes, %ebx) and
//      the test layout moved.  Case bodies are laid out in source order:
//      WM_DESTROY's call sits after the focus handler (0x004037bd).
//
//  NEAR (1289 code bytes against 1285, 354 instructions each): the one
//  residual is WM_DESTROY's `return 0` -- shipped jumps WM_CHAR into it
//  (0x0040383b -> 0x004037c7, a short `je`), so its epilogue is `pop; pop;
//  xor; pop`; ours sends WM_CHAR to WM_KEYUP's final epilogue (a long
//  `je`, the four bytes) and schedules WM_DESTROY's as `xor; pop; pop; pop`.  Not moved by: WM_DESTROY falling through into
//  WM_CHAR's `return 0`, WM_CHAR first in the switch, a `return 0` after
//  the switch, or sixteen orders of the case bodies.
// ----------------------------------------------------------------------- //
LRESULT CALLBACK clu_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	char        szFile[200];
	char        szMsg[256];
	FILE*       pFile;
	int         i;

	if (g_CluClientGlob.m_bWindowUp && g_CluClientGlob.m_pClientMgr)
	switch (uMsg)
	{
		case WM_PAINT:
		{
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_ERASEBKGND:
		{
			if (g_bCluRenderInited)
				return 0;

			break;
		}

		case WM_ACTIVATEAPP:
		{
			if (!g_bCluFocusSimple)
			{
				if (!g_CluClientGlob.m_nUnknown6c)
				{
					if (wParam)
					{
						if (g_CluClientGlob.m_bAppLostFocus)
						{
							g_CluClientGlob.m_pClientMgr->m_pClientShell->OnEvent(7, 0);

							if (!g_CluClientGlob.m_bRenderShutdown)
							{
								OutputDebugStringA("Regained focus.. initializing renderer.\n");

								if (r_InitRender((CClientMgrView*)g_CluClientGlob.m_pClientMgr,
								                 &g_CurRenderMode))
								{
									clu_sub_42d770(szMsg, 255, 0x30, NULL);
									clu_sub_42e6a0(szMsg);
								}
							}

							if (g_CluClientGlob.m_pClientMgr->m_pUnknown898)
							{
								((CSoundMgrView*)
								 g_CluClientGlob.m_pClientMgr->m_SoundMgr)->sub_481ef0();
							}
						}

						g_CluClientGlob.m_pClientMgr->ClearInput();

						g_CluClientGlob.m_bAppLostFocus = 0;
						g_CluClientGlob.m_nUnknown60 = 1;

						if (g_CluClientGlob.m_pClientMgr->m_pUnknown1614)
							g_CluClientGlob.m_pClientMgr->m_pUnknown1614->OnFocusGained();
					}
					else
					{
						g_CluClientGlob.m_pClientMgr->m_pClientShell->OnEvent(6, 0);

						OutputDebugStringA("Losing focus.. shutting down renderer.\n");

						clu_r_TermRender(g_CluClientGlob.m_pClientMgr, 1);
						g_CluClientGlob.m_pClientMgr->ClearInput();

						g_CluClientGlob.m_bAppLostFocus = 1;
						g_CluClientGlob.m_nUnknown60 = 0;

						if (!g_CluClientGlob.m_bMinimizing)
						{
							if (g_CluClientGlob.m_pClientMgr->m_pUnknown898)
							{
								((CSoundMgrView*)
								 g_CluClientGlob.m_pClientMgr->m_SoundMgr)->sub_481e70();
							}
						}
					}
				}
			}
			else if (wParam)
			{
				if (g_CluClientGlob.m_bAppLostFocus)
				{
					OutputDebugStringA("Regained focus... Sending GAINEDFOCUS event.\n");

					g_CluClientGlob.m_pClientMgr->m_pClientShell->OnEvent(7, 0);
				}

				g_CluClientGlob.m_pClientMgr->ClearInput();
				g_CluClientGlob.m_bAppLostFocus = 0;
			}
			else if (!g_CluClientGlob.m_bMinimizing)
			{
				OutputDebugStringA("Lost focus... Sending LOSTFOCUS event.\n");

				g_CluClientGlob.m_pClientMgr->m_pClientShell->OnEvent(6, 0);

				ShowWindow(g_CluClientGlob.m_hMainWnd, SW_MINIMIZE);

				g_CluClientGlob.m_bAppLostFocus = 1;
			}

			return (uMsg == WM_NCACTIVATE);
		}

		case WM_DESTROY:
		{
			clu_sub_42e6a0(NULL);
			return 0;
		}

		case WM_KEYDOWN:
		{
			if (g_nCluKeyDowns < 100)
			{
				g_CluKeyDownVK[g_nCluKeyDowns] = wParam;
				g_CluKeyDownRep[g_nCluKeyDowns] = lParam & 0x7fff;
				g_nCluKeyDowns++;
			}

			return 0;
		}

		case WM_KEYUP:
		{
			if (wParam == VK_F8)
			{
				if (g_CluScreenShotDir)
				{
					for (i = 0; i < 3000; i++)
					{
						sprintf(szFile, "%s%d.bmp", g_CluScreenShotName, i);

						pFile = fopen(szFile, "rb");
						if (pFile)
						{
							fclose(pFile);
						}
						else
						{
							g_pCluMakeScreenShot(szFile);
							return 0;
						}
					}
				}

				return 0;
			}

			if (g_nCluKeyUps < 100)
			{
				g_CluKeyUpVK[g_nCluKeyUps] = wParam;
				g_nCluKeyUps++;
			}

			return 0;
		}

		case WM_CHAR:
		{
			return 0;
		}

		case WM_SYSCOMMAND:
		{
			if ((wParam & 0xfff0) == SC_KEYMENU)
				return 1;

			// NOT an else-if chain: when the window IS iconic the
			// screen-saver arm's `jne 0x00403893` lands on the SC_CLOSE
			// test rather than past it, so the three tests are sequential
			// statements in the source.
			if ((wParam & 0xfff0) == SC_SCREENSAVE ||
			    (wParam & 0xfff0) == SC_MONITORPOWER)
			{
				if (!IsIconic(hWnd))
					return 1;
			}

			if ((wParam & 0xfff0) == SC_CLOSE)
			{
				g_CluClientGlob.m_bMinimizing = 1;

				// ci_GetSoundMgr and ci_GetMusicMgr are CALLED AGAIN for
				// every use -- five calls across two tests and one
				// dispatch -- which is what the shipped bytes do and not a
				// rematerialisation: the source repeats the expression.
				if (*(void**)((char*)ci_GetSoundMgr() + 0x124))
					((CSoundMgrView*)ci_GetSoundMgr())->StopAllSounds();

				if (ci_GetMusicMgr())
				{
					if (((CluMusicMgrView*)ci_GetMusicMgr())->m_pUnknown5c)
						((CluMusicMgrView*)ci_GetMusicMgr())->m_pfnUnknown44(0);
				}
			}

			break;
		}
	}

	return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}


// ----------------------------------------------------------------------- //
//  0x004039f0 -- 454 bytes, 10 calls.  Turn the global command-line vector
//  into the compact block WinMain hands around, giving launch.dll first
//  refusal on the whole thing.
//
//  WHAT PINS THE FRAME.  `mov $0x778c,%eax; call 0x00492c60` is __chkstk on
//  a 30604-byte frame, and the frame is exactly three things: one dword at
//  +0x0000, 150 pointers at +0x0004 (0x258 bytes) and 150 by 200 bytes of
//  text at +0x025c (0x7530).  0x0004 + 0x258 = 0x025c and 0x025c + 0x7530 =
//  0x778c, flush.  The set-up loop at 0x00403a12 walks both at once --
//  `add $0x4,%ecx` against `add $0xc8,%eax`, 0x96 = 150 times -- which is
//  what fixes the two bounds.
//
//  WHAT PINS THE REST
//    * launch.dll's export is called with SIX arguments and cleaned with
//      `add $0x18,%esp`, pushed count-first: (argc, argv, &nArgs, pArgs,
//      150, 200).
//    * the plain path copies each global argument with strncpy (a CALL to
//      the CRT's 0x00492b60 at 0x00403ad6, reloading g_nCluArgCount after
//      it) and then writes a hard zero at [199] -- `movb $0x0,(%edi)`
//      where %edi is the buffer base plus 0xc7.
//    * the two `repnz scasb` blocks are VC6's strlen and strcpy
//      intrinsics; the second copies len+1 bytes, the NUL included.
//    * both allocations answer through the 0x00428fd0 pair, and BOTH are
//      tested before the copy loop, jumping to the same `xor %eax,%eax`
//      tail the launch.dll failure uses -- which is why the failure path
//      shows no message box.  One `&&` guard around the copy with a single
//      `return LTFALSE` after it gives that shared tail; two early
//      refusals each get their own epilogue.
// ----------------------------------------------------------------------- //
LTBOOL clu_dsi_BuildCommandLine(CluCommandLineArgs* pArgs)
{
	int    nArgs;
	char*  pArgPtrs[150];
	char   szArgs[150][200];
	HINSTANCE hLaunch;
	CluGetLithTechCommandLineFn pfnGetCommandLine;
	int    bGotArgs;
	int    i;
	uint32 nTotal;
	char*  pDest;

	nArgs = 0;

	for (i = 0; i < 150; i++)
	{
		pArgPtrs[i] = szArgs[i];
		szArgs[i][0] = 0;
	}

	if (clu_sub_402f70("launch"))
	{
		bGotArgs = 0;

		hLaunch = LoadLibraryA("launch.dll");
		if (hLaunch)
		{
			pfnGetCommandLine = (CluGetLithTechCommandLineFn)
				GetProcAddress(hLaunch, "GetLithTechCommandLine");

			if (pfnGetCommandLine)
			{
				bGotArgs = pfnGetCommandLine(g_nCluArgCount, g_pCluArgList,
				                             &nArgs, pArgPtrs, 150, 200);
			}

			FreeLibrary(hLaunch);
		}

		if (!bGotArgs)
		{
			MessageBoxA(NULL, "Unable to load launch.dll.", "LithTech", 0);
			return LTFALSE;
		}
	}
	else
	{
		for (i = 0; i < g_nCluArgCount; i++)
		{
			strncpy(szArgs[i], g_pCluArgList[i], 200);
			szArgs[i][199] = 0;
		}

		nArgs = g_nCluArgCount;
	}

	nTotal = 0;
	for (i = 0; i < nArgs; i++)
		nTotal += strlen(pArgPtrs[i]) + 1;

	pArgs->m_pArgBuffer = (char*)clu_sub_428fd0(nTotal);
	pArgs->m_pArgs = (char**)clu_sub_428fd0(nArgs * sizeof(char*));

	if (pArgs->m_pArgBuffer && pArgs->m_pArgs)
	{
		pDest = pArgs->m_pArgBuffer;

		for (i = 0; i < nArgs; i++)
		{
			strcpy(pDest, pArgPtrs[i]);
			pArgs->m_pArgs[i] = pDest;
			pDest += strlen(pArgPtrs[i]) + 1;
		}

		pArgs->m_nArgs = nArgs;
		return LTTRUE;
	}

	return LTFALSE;
}


// ----------------------------------------------------------------------- //
//  0x00403bc0 -- 414 bytes, 3 calls.  Split a raw command line into the
//  global vector.  Two passes over the same text: count the tokens, size
//  the vector, then copy each one.
//
//  WHAT PINS IT
//    * one parameter, and the second pass restarts from it (%ebx is never
//      reloaded from anywhere else).
//    * the scan's three terminators are ' ', '"' and NUL, tested in that
//      order, with the first character preloaded -- VC6's rotation of
//      `while (*p != ' ' && *p != '"' && *p) p++;`.
//    * the quoted branch is the FALL-THROUGH of `cmpb $0x22,(%edx)` and
//      the plain branch is the jumped-to arm, so the source guards on the
//      quote.  In the second pass the quoted branch moves the token start
//      PAST the opening quote (`mov %ebx,%esi` at 0x00403c76), and both
//      branches write a hard NUL at the delimiter before measuring.
//    * `sete %cl` at 0x00403cea saves "was the delimiter the end of the
//      string" BEFORE the NUL is stored, which is the only way the plain
//      branch can still tell; the quoted branch tests the byte after the
//      closing quote instead.
//    * the vector and every string come from dalloc, and dsi_FreeCommandLine
//      below frees them with the matching dfree.
//    * the count is bumped INSIDE the strcpy's argument (`[g_nCluArgCount++]`):
//      shipped reloads the count, increments it, indexes -4 and stores the
//      new count after the load (0x00403caa..0x00403cbd).
// ----------------------------------------------------------------------- //
void clu_dsi_ParseCommandLine(char* pCmdLine)
{
	char* pCur;
	char* pStart;
	int   bAtEnd;

	pCur = pCmdLine;

	while (*pCur)
	{
		pStart = pCur;

		while (*pCur != ' ' && *pCur != '"' && *pCur != 0)
			pCur++;

		if (*pStart == '"')
		{
			pCur++;

			while (*pCur != '"')
				pCur++;

			g_nCluArgCount++;
			pCur++;

			if (*pCur == 0)
				break;
		}
		else
		{
			g_nCluArgCount++;
		}

		pCur++;
	}

	g_pCluArgList = (char**)clu_dalloc(g_nCluArgCount * sizeof(char*));
	g_nCluArgCount = 0;

	pCur = pCmdLine;

	while (*pCur)
	{
		pStart = pCur;

		while (*pCur != ' ' && *pCur != '"' && *pCur != 0)
			pCur++;

		if (*pStart == '"')
		{
			pCur++;
			pStart = pCur;

			while (*pCur != '"')
				pCur++;

			*pCur = 0;
			g_pCluArgList[g_nCluArgCount] = (char*)clu_dalloc(strlen(pStart) + 1);
			strcpy(g_pCluArgList[g_nCluArgCount++], pStart);

			pCur++;

			if (*pCur == 0)
				break;
		}
		else
		{
			bAtEnd = (*pCur == 0);

			*pCur = 0;
			g_pCluArgList[g_nCluArgCount] = (char*)clu_dalloc(strlen(pStart) + 1);
			strcpy(g_pCluArgList[g_nCluArgCount++], pStart);

			if (bAtEnd)
				break;
		}

		pCur++;
	}
}


// ----------------------------------------------------------------------- //
//  0x00403d60 -- 56 bytes, 2 calls.  Release what the parser allocated.
//  NOT in this cluster (it is enumerated separately at
//  0x00403d60/56); it is here because it is the other half of the pair and
//  costs five lines.  The loop bound is reloaded from the global on every
//  iteration because dfree could touch it, and the vector itself goes back
//  after the elements.
// ----------------------------------------------------------------------- //
void clu_dsi_FreeCommandLine()
{
	int i;

	for (i = 0; i < g_nCluArgCount; i++)
		clu_dfree(g_pCluArgList[i]);

	clu_dfree(g_pCluArgList);
}


// ----------------------------------------------------------------------- //
//  0x00403da0 -- 143 bytes, 9 calls.  Read -cmdfile's file and feed its
//  first line to the parser.
//
//  THIS IS THE FUNCTION THE ENUMERATOR SPLIT.  0x00403dcf is the target of
//  the `jne` at 0x00403db7 -- the SUCCESS arm -- and it begins with
//  `push %ebx; push %edi`, which reads like a prologue because the failure
//  arm immediately above it ends in `ret` at 0x00403dce.  Shape (a).
//
//  WHAT PINS IT
//    * one parameter, handed straight to fopen with "r"; `xor %al,%al` and
//      `mov $0x1,%al` at the two exits make the return a BYTE -- a C++
//      bool, which is also why WinMain tests it with `test %al,%al`.
//    * fourteen dwords of arguments are pushed across eight calls and
//      cleaned by ONE `add $0x38,%esp` at 0x00403e25 -- /O2's deferred
//      pop, which is why the body reads as a single expression run.
//    * the read is 0x00492c8f, a three-argument CRT entry that locks the
//      stream, decrements the count and steps _cnt: fgets, not fread.  So
//      only the first line of the file is used, and the size it is given
//      is the file length plus 0x3ff (the buffer is length plus 0x400).
//    * the buffer comes from dalloc and goes back through dfree, the same
//      pair the parser uses for the vector.
// ----------------------------------------------------------------------- //
bool clu_dsi_ParseCommandFile(char* pFilename)
{
	FILE* pFile;
	long  nLen;
	char* pBuffer;

	pFile = fopen(pFilename, "r");
	if (!pFile)
	{
		MessageBoxA(NULL, "Could not parse cmdfile.", "Error", 0);
		return false;
	}

	g_nCluArgCount = 0;

	fseek(pFile, 0, SEEK_END);
	nLen = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	pBuffer = (char*)clu_dalloc(nLen + 0x400);

	fgets(pBuffer, nLen + 0x3ff, pFile);
	fclose(pFile);

	clu_dsi_ParseCommandLine(pBuffer);
	clu_dfree(pBuffer);

	return true;
}


// ----------------------------------------------------------------------- //
//  0x00403e30 -- 338 bytes, 13 calls.  WinMain.
//
//  WHAT PINS IT
//    * `ret $0x10` with four stack parameters, and the THIRD is the one fed
//      to _strdup and then to the parser: lpCmdLine.  hInstance (the first)
//      goes to RunClientApp.
//    * the three-dword argument block is zeroed field by field at entry and
//      +0x00 / +0x08 are freed -- with 0x00429080, the pair BuildCommandLine
//      allocated them from -- on every one of the three exits.  +0x04 is
//      never freed, which is what says it is the count and not a pointer.
//    * the -cmdfile scan BREAKS on the first hit: the success arm jumps to
//      0x00403e8f, past the loop, rather than back to the bump.
//    * the repeat-run loop increments g_nCluCurRun ONCE in its guard and
//      ONCE per body, and reloads the counter after every RunClientApp --
//      an increment written as a statement before the loop and again as the
//      first statement inside it, not a side-effecting condition.  A
//      side-effecting `while (++n < limit)` would have put the bump after
//      the call, and the loop's back edge lands ON the bump.
//    * `cmp $0x1,%eax; jle` is signed (the limit is an int) while
//      `cmp %eax,%edx; jb` is unsigned (the counter is a uint32).
// ----------------------------------------------------------------------- //
#ifdef NOLFSRC_MODERN
// M-P1-30: declare the process DPI-aware before any window exists. Retail
// declares nothing, so with display scaling above 100% Windows virtualises
// it: the desktop size from EnumDisplaySettings is in pixels while the
// cursor, ClipCursor, window rects and GetSystemMetrics are in scaled
// units, so the game's cursor is held to the top-left 1/scale of the
// screen and the system cursor is drawn enlarged (RETAIL_BUGS.md). Per-
// monitor (v2) where Windows has it, then per-monitor, then system-wide;
// each looked up at run time, as the oldest is Vista's.
static void P1_DeclareDpiAware()
{
	typedef BOOL (WINAPI *SetContextFn)(HANDLE);
	typedef HRESULT (WINAPI *SetAwarenessFn)(int);
	typedef BOOL (WINAPI *SetAwareFn)(void);

	HMODULE hUser = GetModuleHandleA("user32.dll");
	if (hUser)
	{
		SetContextFn pSetContext = (SetContextFn)GetProcAddress(hUser, "SetProcessDpiAwarenessContext");
		if (pSetContext && pSetContext((HANDLE)-4))   // PER_MONITOR_AWARE_V2
			return;
	}

	HMODULE hShcore = LoadLibraryA("shcore.dll");
	if (hShcore)
	{
		SetAwarenessFn pSetAwareness = (SetAwarenessFn)GetProcAddress(hShcore, "SetProcessDpiAwareness");
		if (pSetAwareness && SUCCEEDED(pSetAwareness(2)))   // PROCESS_PER_MONITOR_DPI_AWARE
			return;
	}

	if (hUser)
	{
		SetAwareFn pSetAware = (SetAwareFn)GetProcAddress(hUser, "SetProcessDPIAware");
		if (pSetAware)
			pSetAware();
	}
}
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
	CluCommandLineArgs args;
	int                nRet;
	int                i;

	NOLFSRC_INSTALL_CRASH_HANDLER();   // debug build only
#ifdef NOLFSRC_MODERN
	P1_DeclareDpiAware();
#endif

	args.m_pArgs = NULL;
	args.m_nArgs = 0;
	args.m_pArgBuffer = NULL;

	clu_dsi_ParseCommandLine(clu_strdup(lpCmdLine));

	for (i = 0; i < g_nCluArgCount; i++)
	{
		if (stricmp(g_pCluArgList[i], "-cmdfile") == 0)
		{
			if (!clu_dsi_ParseCommandFile(g_pCluArgList[i + 1]))
			{
				if (args.m_pArgs)
					clu_sub_429080(args.m_pArgs);

				if (args.m_pArgBuffer)
					clu_sub_429080(args.m_pArgBuffer);

				return -1;
			}

			break;
		}
	}

	g_nCluStartTime = timeGetTime();

	if (!clu_dsi_BuildCommandLine(&args))
	{
		if (args.m_pArgs)
			clu_sub_429080(args.m_pArgs);

		if (args.m_pArgBuffer)
			clu_sub_429080(args.m_pArgBuffer);

		return -1;
	}

	nRet = clu_dsi_RunClientApp(hInstance, &args);

	if (g_nCluNumRuns > 1)
	{
		g_nCluCurRun++;

		while (g_nCluCurRun < (uint32)g_nCluNumRuns)
		{
			g_nCluCurRun++;
			nRet = clu_dsi_RunClientApp(hInstance, &args);
		}
	}

	clu_dsi_FreeCommandLine();

	if (args.m_pArgs)
		clu_sub_429080(args.m_pArgs);

	if (args.m_pArgBuffer)
		clu_sub_429080(args.m_pArgBuffer);

	return nRet;
}


// ======================================================================= //
//
//  2. THE CLIENT FILE MANAGER   0x00403fd0 .. 0x0040488d
//
//  THE OBJECT IS 0xe88 BYTES and every field below is fixed by an offset
//  the code touches:
//
//    0x000  LTLink            the open-tree list         (walked at 0x004040f5,
//                                                         0x004043c0, 0x00404620)
//    0x00c  string holder     ctor 0x00490f60, chunk size 0x00491060,
//                             AddString 0x00491070, dtor 0x00490fe0
//    0x024  record bank       vptr 0x004a14b8, sb_Init(0x20, 0x20) --
//                             0x20-byte records, free list at 0x040
//    0x048  record bank       vptr 0x004a14a8, sb_Init(0x28, 0x20) --
//                             0x28-byte records, free list at 0x064
//    0x06c  void*             the "c:\de_cache" tree
//    0x070  LTLink[100]       hashed by server file id   (0x070 + 100*12 = 0x520)
//    0x520  LTLink[200]       hashed by path             (0x520 + 200*12 = 0xe80)
//    0xe80  void*             the message queue 0x0042f740 makes
//    0xe84  CClientMgr*       the owner
//
//  0x00404098 and 0x004040ba both walk their array with `mov %eax,0x4(%eax);
//  mov %eax,(%eax); add $0xc,%eax` -- LTLink::TieOff writes m_pNext then
//  m_pPrev because ltlink.h spells it `m_pPrev = m_pNext = (LTLink*)this`.
//  The 12-byte stride and the 0x64 / 0xc8 counts are what fix the bounds.
//
//  0x00401000 is the compiler's own `vector constructor iterator`:
//  (pArray, nStride, nCount, pfnCtor), with LTLink's do-nothing constructor
//  at 0x004040e0 (`mov %ecx,%eax; ret`).  It appears because ltlink.h gives
//  LTLink a user-declared constructor, so an ARRAY member of it has to be
//  constructed element by element -- which is the strongest single piece of
//  evidence that these really are LTLink arrays and not raw dword triples.
//
// ======================================================================= //

// One open tree.  16 bytes from the 0x00428fd0 pair, its own link, and
// m_pData pointing back at itself.
struct CluTreeNode
{
	LTLink m_Link;                           // 0x00
	void*  m_pTree;                          // 0x0c
};

// The 0x20-byte record, hashed by path into m_ByName.
struct CluFileRec
{
	uint32 m_nUnknown00;                     // 0x00  cleared, never written
	LTLink m_Link;                           // 0x04  m_pData points at the record
	void*  m_pTree;                          // 0x10
	uint16 m_nUnknown14;                     // 0x14  set to 0xffff at birth
	uint16 m_nNameLen;                       // 0x16
	uint8  m_nFlag;                          // 0x18  0xff matches anything
	uint8  m_pad_19[0x1c - 0x19];
	char*  m_pName;                          // 0x1c  interned by the string holder
};

// The 0x28-byte record, hashed by server file id into m_ByID.  0x004045d0
// (not on this list) answers ILTClient's file name out of +0x1c, which is
// what says +0x18/+0x1c/+0x24 all hold the same interned path.
struct CluNetFileRec
{
	LTLink      m_Link;                      // 0x00  m_pData points at the record
	void*       m_pTree;                     // 0x0c
	uint16      m_nID;                       // 0x10
	uint16      m_pad_12;
	CluFileRec* m_pFileRec;                  // 0x14  filled in elsewhere
	char*       m_pName18;                   // 0x18
	char*       m_pName;                     // 0x1c
	uint16      m_nNameLen;                  // 0x20
	uint16      m_pad_22;
	char*       m_pName24;                   // 0x24
};

// What ILTClient hands the manager to name a file: either a path, or a type
// 2 that names a server file by id and carries the path as a fallback.
struct CluFileIdent
{
	int    m_nType;                          // 0x00  2 == a server file id
	char*  m_pName;                          // 0x04
	uint16 m_nID;                            // 0x08
};

class CluFileMgr
{
public:
	LTLink            m_TreeList;                    // 0x000
	CStringHolder     m_StringHolder;                // 0x00c
	CluFileRecBank    m_FileRecBank;                 // 0x024
	CluNetFileRecBank m_NetFileRecBank;              // 0x048
	void*             m_pCacheTree;                  // 0x06c
	LTLink            m_ByID[100];                   // 0x070
	LTLink            m_ByName[200];                 // 0x520
	void*             m_pQueue;                      // 0xe80
	CluClientMgrView* m_pClientMgr;                  // 0xe84
};

// 0x00404480 -- drop every id record back on its bank and close the queue.
// Declared, not defined: it is enumerated separately (160 bytes) and
// src/engine/lithtech/clientmgr2.cpp already carries a declaration of it.
void clu_sub_404480(CluFileMgr* pFileMgr);                   // 0x00404480

CluTreeNode* clu_fm_FindTreeWithFile(LTLink* pTreeList, char* pName);   // 0x004043b0
int clu_fm_OnServerFile(void* pQueue, char* pName, uint32 nUnused, uint32 nID);


// ----------------------------------------------------------------------- //
//  0x00403f90 -- 60 bytes.  The path normaliser declared above: strncpy
//  (0x00492b60) into pDest, terminate at nBytes - 1, _strupr (0x004a0320),
//  then every '/' becomes '\\'.
// ----------------------------------------------------------------------- //

void clu_sub_403f90(char* pSrc, char* pDest, int nBytes)
{
	strncpy(pDest, pSrc, nBytes);
	pDest[nBytes - 1] = 0;
	_strupr(pDest);

	// the cursor is pDest itself (`inc %esi`, no copy)
	while (*pDest)
	{
		if (*pDest == '/')
			*pDest = '\\';

		pDest++;
	}
}


// ----------------------------------------------------------------------- //
//  0x00403fd0 -- 261 bytes, 11 calls.  Make one.
//
//  WHAT PINS IT.  `push $0xe88; call dalloc; test; je` with the whole
//  member-construction run inside the `if` and a `xor %esi,%esi` in the
//  else is VC6's `new` -- the guard is the compiler's, which is why the
//  code AFTER it dereferences the pointer without checking it.  The
//  constructor itself is inlined: string holder, then the two banks (each
//  writing its own vptr then sb_Init), then two `vector constructor
//  iterator` calls for the LTLink arrays, in declaration order.  m_TreeList
//  is an LTLink too but its constructor is empty and generates nothing.
//
//  The two banks are then RE-initialised with sb_Init2 after their chunks
//  are handed back -- a deliberate resize past what the constructors set,
//  0x40/0x400 and 0x80/0x400 against the constructors' 0x20.
//
//  BOTH `vector constructor iterator` calls (0x0040401a and 0x0040402c) are
//  there only because /O2's per-function inline budget runs out before the
//  arrays' empty LTLink constructors are reached; with budget left VC6
//  inlines the empty constructor loop to nothing.  Plain dl_TieOff at all
//  three sites leaves one iterator (m_ByName's) and loses m_ByID's; the one
//  extra inline expansion of AsLTLink() on the list head is what makes both
//  come out (a second TieOff loop does the same, so it is budget, not shape).
// ----------------------------------------------------------------------- //
CluFileMgr* clu_fm_Create(CluClientMgrView* pClientMgr)
{
	CluFileMgr* pMgr;
	int         i;

	pMgr = new CluFileMgr;

	pMgr->m_pCacheTree = LTNULL;
	pMgr->m_pQueue = LTNULL;

	pMgr->m_StringHolder.SetAllocSize(0x1000);

	clu_sb_FreeChunks(&pMgr->m_FileRecBank.m_Bank, 1);
	clu_sb_Init2(&pMgr->m_FileRecBank.m_Bank, 0x20, 0x40, 0x400);

	clu_sb_FreeChunks(&pMgr->m_NetFileRecBank.m_Bank, 1);
	clu_sb_Init2(&pMgr->m_NetFileRecBank.m_Bank, 0x28, 0x80, 0x400);

	for (i = 0; i < 100; i++)
		dl_TieOff(&pMgr->m_ByID[i]);

	dl_TieOff(pMgr->m_TreeList.AsLTLink());
	pMgr->m_pClientMgr = pClientMgr;

	for (i = 0; i < 200; i++)
		dl_TieOff(&pMgr->m_ByName[i]);

	return pMgr;
}


// ----------------------------------------------------------------------- //
//  0x004040f0 -- 179 bytes, 7 calls.  Take one down.
//
//  WHAT PINS IT
//    * the null test wraps EVERYTHING, and `push %ebx; push %ebp; push
//      %edi` sit inside it -- the three extra registers are saved only on
//      the taken path, which is what /O2 does for a body under one guard.
//    * the tree walk reads m_pNext BEFORE the node is freed and keeps it in
//      %ebx, the textbook safe-erase shape.
//    * the record sweep does not tie the buckets off afterwards; it only
//      threads each record onto the bank's free list, which is sb_Free's
//      two stores inlined (servermgr.h carries it as an inline for exactly
//      this reason).
//    * the tail -- derived vptr, member dtor, base vptr, next member, then
//      dfree -- is the compiler's own inline destructor followed by
//      `operator delete`, i.e. one `delete pMgr;` in the source.
//    * IDENTICAL.  The tree walk reads m_pData TWICE (0x0040410f and
//      again after the CloseTree call at 0x0040411e), so there is no node
//      local -- a named one CSE'd it into %edi.  The record sweep reads
//      m_pData ONCE into a local, after m_pNext (0x00404142/0x00404145);
//      writing pCur->m_pData at both stores reloaded it and cost %ebp.
// ----------------------------------------------------------------------- //
void clu_fm_Destroy(CluFileMgr* pMgr)
{
	LTLink*      pCur;
	LTLink*      pNext;
	void*        pRec;
	int          i;

	if (!pMgr)
		return;

	clu_sub_404480(pMgr);

	pCur = pMgr->m_TreeList.m_pNext;
	while (pCur != &pMgr->m_TreeList)
	{
		pNext = pCur->m_pNext;

		clu_fs_CloseTree(((CluTreeNode*)pCur->m_pData)->m_pTree);
		clu_sub_429080(pCur->m_pData);

		pCur = pNext;
	}

	for (i = 0; i < 200; i++)
	{
		pCur = pMgr->m_ByName[i].m_pNext;

		while (pCur != &pMgr->m_ByName[i])
		{
			pNext = pCur->m_pNext;

			pRec = pCur->m_pData;
			*(void**)pRec = pMgr->m_FileRecBank.m_Bank.m_pFreeList;
			pMgr->m_FileRecBank.m_Bank.m_pFreeList = pRec;

			pCur = pNext;
		}
	}

	delete pMgr;
}


// ----------------------------------------------------------------------- //
//  0x004041f0 -- 140 bytes, 3 calls.  Start the manager serving.
//
//  WHAT PINS IT.  The 28 bytes written at 0x0040423a..0x00404236 are one
//  descriptor, and 0x0042f740 copies exactly seven dwords out of it into
//  its object's +0x08, so the struct is five function pointers, the
//  CNetMgr sub-object (CClientMgr + 0x50c, which clientmgr.h already
//  names) and the caller's second argument.  Three of the five slots are
//  the SAME function, 0x00404470 (a bare `ret`), and 0x00404460 is `xor
//  %eax,%eax; ret` -- /OPT:ICF is off in this image, so those really are
//  three references to one stub and not three folded ones.
//
//  The 100-bucket table is tied off AGAIN here, after the constructor
//  already did it.  That is in the bytes; it is not a mistake in the
//  reconstruction.
// ----------------------------------------------------------------------- //
void clu_fm_Init(CluFileMgr* pMgr, int nUnknown)
{
	CluQueueDesc desc;
	int          i;

	if (!pMgr)
		return;

	for (i = 0; i < 100; i++)
		dl_TieOff(&pMgr->m_ByID[i]);

	clu_fs_OpenTree("c:\\de_cache", &pMgr->m_pCacheTree);

	desc.m_pNetMgr = pMgr->m_pClientMgr->m_NetMgr;
	desc.m_nUnknown18 = nUnknown;
	desc.m_pfnOnServerFile = clu_fm_OnServerFile;
	desc.m_pfn04 = clu_sub_404460;
	desc.m_pfn08 = clu_sub_404470;
	desc.m_pfn0c = clu_fm_Empty0c;
	desc.m_pfn10 = clu_fm_Empty10;

	pMgr->m_pQueue = clu_sub_42f740(&desc);
	clu_sub_42f7d0(pMgr->m_pQueue, pMgr);
}


// ----------------------------------------------------------------------- //
//  0x004043b0 -- 89 bytes, 1 call.  Which open tree holds this name?
//
//  WHAT PINS IT.  `sub $0x110,%esp` and nothing else on the frame: the
//  whole frame is the 0x110-byte find record 0x00420d50 fills, the same
//  record src/engine/server/filemgr.cpp calls "the 0x110-byte record".  The
//  answer is the NODE (%edi, the link's m_pData), not the tree, and NULL
//  when the walk runs out -- `xor %eax,%eax` sits on the exhausted path
//  only.
// ----------------------------------------------------------------------- //
CluTreeNode* clu_fm_FindTreeWithFile(LTLink* pTreeList, char* pName)
{
	CluFileInfo  info;
	LTLink*      pCur;
	CluTreeNode* pNode;

	for (pCur = pTreeList->m_pNext; pCur != pTreeList; pCur = pCur->m_pNext)
	{
		pNode = (CluTreeNode*)pCur->m_pData;

		if (clu_fs_GetFileInfo(pNode->m_pTree, pName, &info))
			return pNode;
	}

	return LTNULL;
}


// ----------------------------------------------------------------------- //
//  0x00404280 -- 302 bytes, 8 calls.  The queue's first callback: the
//  server has named a file, find it and remember it under its id.
//
//  WHAT PINS THE SIGNATURE.  It is stored as slot 0 of the descriptor above
//  and is __cdecl with four arguments, of which the THIRD is never read --
//  nothing in the body touches that slot.  The first goes to 0x0042f7c0,
//  which is the queue's user-pointer getter, so it is the queue; the SECOND
//  (0x21c(%esp) at 0x0040428e) is the path -- it reaches the 0x200-byte
//  normaliser and the "Unable to find server file: %s" line.  The fourth is
//  a full uint32: 0x004042e2 passes it to 0x00404410 with no `and $0xffff`,
//  the bucket is an unsigned `xor %edx,%edx; div`, and only the store into
//  the record (`mov %bp,0x10(%esi)`) narrows it.  The caller, ltnew2.cpp's
//  sub_0042f7e0, calls it as (record, name, unknown, id).
//
//  Every exit is `xor %eax,%eax` before the pops, so the answer is a
//  constant zero and not a fall-out of the last call.
//
//  The record's three name fields are written 0x24, 0x1c, 0x18 -- the order
//  a right-to-left chained assignment produces.
// ----------------------------------------------------------------------- //
int clu_fm_OnServerFile(void* pQueue, char* pName, uint32 nUnused, uint32 nID)
{
	CluFileMgr*    pMgr;
	CluTreeNode*   pNode;
	CluNetFileRec* pRec;
	char           szPath[0x200];

	pMgr = (CluFileMgr*)clu_sub_42f7c0(pQueue);

	pNode = clu_fm_FindTreeWithFile(&pMgr->m_TreeList, pName);
	if (!pNode)
	{
		clu_ConPrintf("Unable to find server file: %s", pName);

		if (pMgr->m_pClientMgr->m_pClientShell)
			pMgr->m_pClientMgr->m_pClientShell->OnMessage(1, pName, 0);

		return 0;
	}

	clu_sub_403f90(pName, szPath, 0x200);

	if (clu_sub_404410(pMgr, nID))
		return 0;

	pRec = (CluNetFileRec*)clu_sb_Allocate(&pMgr->m_NetFileRecBank.m_Bank);
	memset(pRec, 0, sizeof(CluNetFileRec));

	pRec->m_pName18 = pRec->m_pName = pRec->m_pName24 =
		pMgr->m_StringHolder.AddString(szPath, 1);

	pRec->m_nID = (uint16)nID;
	pRec->m_pTree = pNode->m_pTree;
	pRec->m_nNameLen = (uint16)strlen(szPath);
	pRec->m_Link.m_pData = pRec;

	dl_Insert(&pMgr->m_ByID[nID % 100], &pRec->m_Link);

	return 0;
}


// ----------------------------------------------------------------------- //
//  0x00404410 -- 83 bytes.  The by-id probe declared above: walk the
//  m_ByID bucket for nID % 100 (signed idiv of the promoted word, then the
//  bucket index masked back to a word) and answer the record whose +0x10
//  id matches, or NULL.  The record is answered in %eax straight out of
//  the loop (0x00404444 jumps to the `pop %esi`).
// ----------------------------------------------------------------------- //

CluNetFileRec* clu_sub_404410(CluFileMgr* pFileMgr, uint16 nID)
{
	LTLink*        pHead;
	LTLink*        pCur;
	CluNetFileRec* pRec;
	uint16         iBucket;

	iBucket = nID % 100;
	pHead = &pFileMgr->m_ByID[iBucket];

	for (pCur = pHead->m_pNext; pCur != pHead; pCur = pCur->m_pNext)
	{
		pRec = (CluNetFileRec*)pCur->m_pData;
		if (pRec->m_nID == nID)
			return pRec;
	}

	return NULL;
}


// ----------------------------------------------------------------------- //
//  0x00404520 -- 172 bytes, 3 calls.  Mount a list of resource names.
//
//  WHAT PINS IT
//    * five arguments; the FIRST is null-tested and everything hangs off
//      it, the third is the count (`jle` -- a signed int), the fourth is
//      written only through a moving pointer that advances in lockstep
//      with the added count, and the fifth takes that count at the end.
//    * fs_OpenTree's out-parameter lands in a stack slot that MSVC placed
//      on top of the first parameter; %esi already holds the manager, so
//      the reuse is free.  Nothing in the source says "reuse a parameter".
//    * the node's m_pData is stored FIRST and the tree SECOND, then the
//      six stores of ltlink.h's AddAfter follow in its exact order.  The
//      source assigns the tree first all the same: that is what makes /O2
//      load the tree (0x0040456e) BEFORE the m_pData store; written in
//      store order the load lands after it.
//    * the type probe runs only when the caller asked for types, but the
//      count and the write pointer advance for every tree that opened.
// ----------------------------------------------------------------------- //
void clu_fm_AddTrees(CluFileMgr* pMgr, char** pNames, int nNames,
                     int* pTypes, int* pnAdded)
{
	CluTreeNode* pNode;
	void*        pTree;
	int          nAdded;
	int          i;

	if (!pMgr)
		return;

	nAdded = 0;

	for (i = 0; i < nNames; i++)
	{
		clu_fs_OpenTree(pNames[i], &pTree);

		if (pTree)
		{
			pNode = (CluTreeNode*)clu_sub_428fd0(sizeof(CluTreeNode));

			pNode->m_pTree = pTree;
			pNode->m_Link.m_pData = pNode;

			dl_Insert(&pMgr->m_TreeList, &pNode->m_Link);

			if (pTypes)
				pTypes[nAdded] = clu_fs_GetTreeType(pTree);

			nAdded++;
		}
	}

	if (pnAdded)
		*pnAdded = nAdded;
}


// ----------------------------------------------------------------------- //
//  0x004045d0 -- 49 bytes.  ILTClient's file-name answer (clientmgr3.cpp's
//  fm_GetName, startload.cpp's fm_GetFilenameQ2B): no manager -> the empty
//  string at 0x004b0ab4 (its ADDRESS, `mov $imm,%eax`); a type-2 identifier
//  -> the id record's +0x1c interned name, or the empty string when the id
//  is unknown; anything else -> the identifier's own +0x04 name.
// ----------------------------------------------------------------------- //

extern char g_CluEmptyString[];                              // 0x004b0ab4

char* clu_fm_GetFilename(CluFileMgr* pFileMgr, CluFileIdent* pIdent)
{
	CluNetFileRec* pRec;

	if (!pFileMgr)
		return g_CluEmptyString;

	if (pIdent->m_nType == 2)
	{
		pRec = clu_sub_404410(pFileMgr, pIdent->m_nID);
		if (pRec)
			return pRec->m_pName;

		return g_CluEmptyString;
	}

	return pIdent->m_pName;
}


// ----------------------------------------------------------------------- //
//  0x00404680 -- 526 bytes, 5 calls.  The manager's main lookup: turn a
//  file identifier into a cached record, creating one if the name resolves
//  in some open tree.
//
//  WHAT PINS IT
//    * three arguments; the third is read with `mov 0x120(%esp),%dl` and
//      compared against 0xff -- a BYTE, and 0xff is its wildcard.
//    * the identifier is the same three-field block 0x004045d0 reads: an
//      int type at +0x00 (2 selects the id path), a char* at +0x04 and a
//      uint16 at +0x08.
//    * the path hash multiplies each character by its INDEX (`movsbl` then
//      `imul %ecx`), sums into a uint32 and takes it modulo 200 with an
//      unsigned `div`.  The 0x7d00 bound is a separate guard on the index,
//      not on the string.
//    * the in-bucket compare is a THREE-part match: the cached length
//      first (a uint16 compare, cheap), then an inline strcmp, then the
//      flag byte with 0xff on either side matching anything.
//    * the new record's stores come out in the order m_pTree, m_nNameLen,
//      m_nUnknown14, m_Link.m_pData, m_nFlag, m_pName -- which is the
//      order they are written below, because VC6 emits one store per
//      assignment in source order.
// ----------------------------------------------------------------------- //
CluFileRec* clu_fm_GetFileRec(CluFileMgr* pMgr, CluFileIdent* pIdent, uint8 nFlag)
{
	char           szName[256];
	CluNetFileRec* pNetRec;
	CluFileRec*    pRec;
	CluTreeNode*   pNode;
	LTLink*        pBucket;
	LTLink*        pCur;
	char*          pName;
	char*          pChar;
	uint32         nHash;
	uint32         nLen;
	uint32         i;

	if (!pMgr)
		return LTNULL;

	if (pIdent->m_nType == 2)
	{
		pNetRec = clu_sub_404410(pMgr, pIdent->m_nID);
		if (!pNetRec)
			return LTNULL;

		if (pNetRec->m_pTree == pMgr->m_pCacheTree)
		{
			pRec = pNetRec->m_pFileRec;
			if (pRec)
			{
				if (nFlag == 0xff)
					return pRec;

				if (pRec->m_nFlag == 0xff)
					return pRec;

				if (pRec->m_nFlag == nFlag)
					return pRec;
			}

			pName = pIdent->m_pName;
		}
		else
		{
			pName = pNetRec->m_pName;
		}
	}
	else
	{
		pName = pIdent->m_pName;
	}

	if (!pName)
		return LTNULL;

	clu_sub_403f90(pName, szName, 256);

	nHash = 0;
	nLen = 0;

	pChar = szName;
	for (i = 0; i < 32000; i++)
	{
		if (*pChar == 0)
			break;

		nHash += *pChar * i;
		pChar++;
		nLen++;
	}

	pBucket = &pMgr->m_ByName[nHash % 200];

	for (pCur = pBucket->m_pNext; pCur != pBucket; pCur = pCur->m_pNext)
	{
		pRec = (CluFileRec*)pCur->m_pData;

		if (pRec->m_nNameLen == (uint16)nLen && strcmp(pRec->m_pName, szName) == 0)
		{
			if (pRec->m_nFlag == nFlag)
				return pRec;

			if (nFlag == 0xff)
				return pRec;

			if (pRec->m_nFlag == 0xff)
				return pRec;
		}
	}

	pNode = clu_fm_FindTreeWithFile(&pMgr->m_TreeList, szName);
	if (!pNode)
		return LTNULL;

	pRec = (CluFileRec*)clu_sb_Allocate(&pMgr->m_FileRecBank.m_Bank);
	memset(pRec, 0, sizeof(CluFileRec));

	pRec->m_pTree = pNode->m_pTree;
	pRec->m_nNameLen = (uint16)nLen;
	pRec->m_nUnknown14 = 0xffff;
	pRec->m_Link.m_pData = pRec;
	pRec->m_nFlag = nFlag;
	pRec->m_pName = pMgr->m_StringHolder.AddString(szName, 1);

	dl_Insert(pBucket, &pRec->m_Link);

	return pRec;
}


// ----------------------------------------------------------------------- //
//  0x004048c0 / 0x00404950 -- the run continues past 0x0040488d: after
//  0x00404890 (lt2new2.cpp's sub_00404890) come the two workers ILTClient's
//  OpenFile (slot 42, 0x00406610) and CopyFile (slot 43, 0x004066f0) call
//  with m_pFileMgr first.  Both walk m_TreeList exactly as
//  clu_fm_FindTreeWithFile does and open through 0x00420ec0
//  (fs_OpenStreamInTree: tree, name, flags -- the flags a literal 0).
// ----------------------------------------------------------------------- //
#include "iltstream.h"

// fsstream.cpp's fs_OpenStreamInTree, the tree type forward-declared.
class COpenFileTree;
ILTStream* fs_OpenStreamInTree(COpenFileTree* pTree, char* pFilename,
                               int nFlag);                   // 0x00420ec0
#define clu_fs_OpenStreamInTree(pTree, pName, nFlags) \
	fs_OpenStreamInTree((COpenFileTree*)(pTree), (pName), (nFlags))
int clu_fs_CopyStreamToFile(ILTStream* pStream,
                            const char* pszDestFile);        // 0x00421670

// 0x004048c0 -- 132 bytes.  Open the file an ident names.  Type 2 goes by
// server id through 0x00404410 (the uint16 at +0x08 pushed unwidened) and
// opens the id record's own tree/name (+0x0c/+0x1c); anything else tries
// every tree in order, and type 0 finally tries the "c:\de_cache" tree at
// +0x6c.  Every miss shares the one `xor %eax,%eax` at 0x0040493e.
ILTStream* clu_fm_OpenFile(CluFileMgr* pMgr, CluFileIdent* pIdent)
{
	LTLink*        pCur;
	CluTreeNode*   pNode;
	CluNetFileRec* pRec;
	ILTStream*     pStream;

	if (!pMgr)
		return LTNULL;

	if (pIdent->m_nType == 2)
	{
		pRec = clu_sub_404410(pMgr, pIdent->m_nID);
		if (!pRec)
			return LTNULL;

		return clu_fs_OpenStreamInTree(pRec->m_pTree, pRec->m_pName, 0);
	}

	for (pCur = pMgr->m_TreeList.m_pNext; pCur != &pMgr->m_TreeList; pCur = pCur->m_pNext)
	{
		pNode = (CluTreeNode*)pCur->m_pData;
		pStream = clu_fs_OpenStreamInTree(pNode->m_pTree, pIdent->m_pName, 0);
		if (pStream)
			return pStream;
	}

	if (pIdent->m_nType == 0)
	{
		pStream = clu_fs_OpenStreamInTree(pMgr->m_pCacheTree, pIdent->m_pName, 0);
		if (pStream)
			return pStream;
	}

	return LTNULL;
}

// 0x00404950 -- 109 bytes.  Copy the first tree's copy of a file out to
// disk.  LT_ERROR (1) for no manager, LT_NOTFOUND (0x3d) when no tree has
// it; otherwise the stream is released BEFORE the copy's answer is tested
// (`mov %eax,%edi; call *0x4(%edx)`) and that answer becomes `sete`.
LTRESULT clu_fm_CopyFile(CluFileMgr* pMgr, char* pszSourceFile, char* pszDestFile)
{
	LTLink*      pCur;
	CluTreeNode* pNode;
	ILTStream*   pStream;
	int          bCopied;

	if (!pMgr)
		return LT_ERROR;

	for (pCur = pMgr->m_TreeList.m_pNext; pCur != &pMgr->m_TreeList; pCur = pCur->m_pNext)
	{
		pNode = (CluTreeNode*)pCur->m_pData;
		pStream = clu_fs_OpenStreamInTree(pNode->m_pTree, pszSourceFile, 0);
		if (pStream)
		{
			bCopied = clu_fs_CopyStreamToFile(pStream, pszDestFile);
			pStream->Release();
			return bCopied ? LT_OK : LT_ERROR;
		}
	}

	return LT_NOTFOUND;
}


// ======================================================================= //
//
//  WHAT IS IN THE RUN AND WHAT IS NOT
//
//    0x00402f90  918 b  42 c  clu_dsi_RunClientApp
//    0x00403330  372 b  12 c  clu_dsi_InitGame
//    0x004034b0 1335 b  35 c  clu_WndProc
//    0x004039f0  454 b  10 c  clu_dsi_BuildCommandLine
//    0x00403bc0  414 b   3 c  clu_dsi_ParseCommandLine
//    0x00403d60   56 b   2 c  clu_dsi_FreeCommandLine      (bonus, not on the list)
//    0x00403da0  142 b  10 c  clu_dsi_ParseCommandFile     (csv row merged with 0x00403dcf)
//    0x00403e30  338 b  13 c  WinMain
//    0x00403fd0  261 b  11 c  clu_fm_Create
//    0x004040f0  179 b   7 c  clu_fm_Destroy
//    0x004041f0  140 b   3 c  clu_fm_Init
//    0x00404280  302 b   8 c  clu_fm_OnServerFile
//    0x004043b0   89 b   1 c  clu_fm_FindTreeWithFile
//    0x00404520  172 b   3 c  clu_fm_AddTrees
//    0x00404680  526 b   5 c  clu_fm_GetFileRec
//
//  Direct/indirect call counts were taken from the shipped image and
//  every body above issues exactly that many:
//
//    0x00402f90  18 direct / 24 indirect      0x00403fd0  11 / 0
//    0x00403330   9 /  3                      0x004040f0   7 / 0
//    0x004034b0  19 / 16                      0x004041f0   3 / 0
//    0x004039f0   5 /  5                      0x00404280   7 / 1
//    0x00403bc0   3 /  0                      0x004043b0   1 / 0
//    0x00403da0   9 /  1                      0x00404520   3 / 0
//    0x00403e30  12 /  1                      0x00404680   5 / 0
//
//  Four of those counts depend on the compiler doing something rather than
//  on what is written, and they are the four places to look first if a
//  count comes back wrong:
//
//    * 0x00402f90's start-up error box.  The shipped image has ONE
//      MessageBoxA call site for two source-level calls -- VC6 hoisted the
//      two shared pushes above the branch and tail-merged the rest.  If the
//      merge does not happen the indirect count reads 25, not 24.
//    * 0x00403e30's two cleanup blocks.  The -cmdfile failure and the
//      BuildCommandLine failure are written out separately here and the
//      shipped image tail-merges them into one pair of frees.  Without the
//      merge the direct count reads 14.
//    * strlen / strcpy / memcpy / memset.  Every one of them in this file is
//      expected to expand to the `repnz scasb` / `rep movs` / `rep stos`
//      intrinsic and cost no call; 0x004039f0's 200-byte copy is a real
//      call because it is strncpy (0x00492b60), which has no intrinsic.  /Oi is implied by /O2, so this
//      should hold, but if a call appears where a `rep` was expected that is
//      what happened.
//    * new / delete in clu_fm_Create and clu_fm_Destroy.  The shipped
//      allocation goes straight to 0x004290b0 and the release to
//      0x004290c0, with no `operator new` forwarder in between, so whatever
//      the build links `operator new` to has to be that pair for the call to
//      land on the right address.  The displacement is masked either way, so
//      this cannot cost byte-identity -- only the call's resolution.
//
//  WHAT COULD NOT BE PINNED
//
//    * ClientGlob +0x058, +0x060, +0x068, +0x06c, +0x070 and +0x078.  Each
//      is only ever compared against zero or stored 0/1, so the names above
//      describe the CONTROL FLOW that reads them, not what they mean.
//      +0x060 in particular is set to 1 at start-up and then toggled by the
//      focus handler in the opposite sense to m_bAppLostFocus, which is
//      suggestive and no more.
//    * 0x004b32a4, 0x004b32a8, 0x004b32a0 and 0x004a9ec4 are console
//      variables by their use (read every frame, never written here) but
//      nothing in this run names them.  "ShowTicks" at 0x004a7794 is the
//      only literal near them and it is not pushed by any function in the
//      run -- it is almost certainly the registration of 0x004b32a0, done
//      elsewhere, which is why g_bCluShowTicks carries that spelling.
//    * 0x004b4358, tested by its address in the WM_KEYUP screen-shot arm.
//      It is an array of something; what, the bytes do not say.
//    * CluNetFileRec's +0x18 and +0x24.  Both take the same interned path as
//      +0x1c and nothing in this run reads either of them.  They could be
//      two more names, or a union, or fields a later revision stopped using.
//    * the second argument of clu_fm_Init and the third of
//      clu_fm_OnServerFile.  The first is copied into the queue descriptor's
//      last dword and never read here; the second is not read at all.
//    * whether the two halves of this file were one translation unit in
//      2000.  The literal pool is contiguous and in address order across
//      both, which is consistent with one TU and also with two adjacent
//      ones; the linker concatenates either way.  Verification is per
//      function, so it does not matter for scoring -- but it is the reason
//      this file has two clearly separated sections rather than one.
//
// ======================================================================= //
