// server.dll — the performance counter and the rate governor
// (reconstruction). Build /O1.
//
// One counter is a single QueryPerformanceCounter snapshot: eight bytes, no
// accumulator, no state beyond the instant it was started at. Everything
// that times anything in the engine takes one of these, starts it, and reads
// the elapsed time back out; the read is where the frequency division
// happens, so the snapshot itself costs almost nothing.
//
// The rate governor on top of it is what paces per-client sends: it holds a
// rate in hertz and answers "is it time yet", carrying the remainder forward
// so a slow frame does not lose the sends it should have made.
//
// The file name is ours.
#include <windows.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// windows.h renames these methods out from under the interface classes.
#undef PostMessage
#undef CopyFile
#undef PlaySound
#undef GetClassName

#include "servermgr.h"
#include "servermodel.h"
#include "de_error.h"

// de_error.h's two globals, defined once, here. The format pointer is the
// first thing in the module's .data (0x100510c0), its string directly after
// it (0x100510c4) and this file's own error tags ("SetInstanceHandle",
// "bm_SetInstanceHandle", ...) right behind that; the level is the first
// dword of the zero-filled tail (0x10056ff0), and nothing ever raises it in
// a release build.
char* g_pErrorFormat = "LT ERROR: %s returned %s (%s)";
int   g_DebugLevel;

// The counter's raw reading is divided by a 64-bit scale in .data rather
// than by the queried frequency, so it comes back in whatever unit that
// scale was set up for (0x10051250; name ours). Initialised .data: the
// shipped quadword is 1, the value cnt_Init stores.
__int64 g_CounterScale = 1;


// Modes 1 and 2 both start the counter and 0 leaves it cold. The two arms
// call *two different functions* with the same body, and that is the whole
// reason this constructor is 37 bytes rather than 30: with one callee VC6
// merges the two tests into `je` onto a single call site and the function
// comes out seven bytes short. Two callees are two relocations, so there is
// nothing to merge — the shipped code keeps `cmp 2 / call / jmp` and
// `cmp 1 / mov ecx,esi / call` apart — and /Gy plus the linker's identical-
// COMDAT folding then puts both bodies at one address, 0x10005f5d, which is
// why the image looks as though only `Start` exists. This closed a documented
// 30-of-37 near; the same reasoning applies to a function *pointer*
// stored twice (CVisQuery::CVisQuery), and it turns out to hold for a direct
// `call` as well.
//
// The second symbol's original name is not recoverable: the linker discarded
// it, and both call sites reach the surviving body. It is declared here on a
// local view of the class — servermgr.h has no slot for it and this file must
// not add one — and reached by casting `this` through that view, which is
// free because the view's layout starts at the same offset. Which arm got
// which symbol is likewise not recoverable; mode 1 is given the attested
// `CBaseCounter::Start` because that is the one the rest of the engine calls.
class CBaseCounterMode2View
{
public:
	void   Start();                          // folded onto 0x10005f5d

	uint32 m_Data[2];
};

void CBaseCounterMode2View::Start()
{
	QueryPerformanceCounter((LARGE_INTEGER*)m_Data);
}

CBaseCounter::CBaseCounter(int nMode)
{
	if (nMode == 2)
		((CBaseCounterMode2View*)this)->Start();
	else if (nMode == 1)
		Start();
}

void CBaseCounter::Start()
{
	QueryPerformanceCounter((LARGE_INTEGER*)m_Data);
}

// Milliseconds since the start, worked out in 64-bit so a long interval on a
// fast counter does not wrap.
uint32 CBaseCounter::Read()
{
	LARGE_INTEGER now;
	LARGE_INTEGER freq;

	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&freq);

	return (uint32)((now.QuadPart - *(__int64*)m_Data) * 1000 / freq.QuadPart);
}

// The same reading against the fixed scale instead of the queried frequency.
//
// The division is UNSIGNED, and nothing but the callee says so: the shipped
// call goes to __aulldiv at 0x1003e8d0, where Read above goes to __alldiv at
// 0x1003e940 (which negates its operands and counts the sign flips in edi).
// Both were written `__int64` here and both were byte-identical, because
// the byte comparison masks the relocation a `call rel32` carries -- checking
// call targets is what separated them, by noticing that one symbol of ours reached two
// different shipped addresses.
uint32 CBaseCounter::ReadRaw()
{
	LARGE_INTEGER now;

	QueryPerformanceCounter(&now);

	return (uint32)((now.QuadPart - *(unsigned __int64*)m_Data)
	                / (unsigned __int64)g_CounterScale);
}

// 0x10005fab — five bytes: a bare `jmp` to Read. A thiscall member on the
// counter taking no arguments whose whole body is `return Read();`, which
// VC6 turns into a tail jump. Both call sites reach it with the counter in
// ecx and nothing pushed — CRateController::Update without even reloading
// ecx, because `this` is still there — so it cannot be one of the C
// wrappers below (those are cdecl and would have to move the argument).
// The name went with the tail call: only the thunk survives in the image,
// so it is named by its address, and it is declared on a view derived from
// CBaseCounter because servermgr.h has no slot for it and this file must
// not add one.
//
// Reproducing it matters even though five bytes are too few for a byte match to
// place: `Read` and this forwarder are two different relocation targets, and
// the oracle masks both to the same bytes. Update and DebugPrint had been
// calling Read directly and matching anyway.
class CBaseCounterReadView : public CBaseCounter
{
public:
	uint32 sub_10005fab();
};

uint32 CBaseCounterReadView::sub_10005fab()
{
	return Read();
}


// The C wrappers the rest of the engine reaches the counter through: both
// are a tail call, so neither leaves a frame behind.
void cnt_StartCounter(CBaseCounter* pCounter)
{
	pCounter->Start();
}

uint32 cnt_EndCounter(CBaseCounter* pCounter)
{
	return pCounter->ReadRaw();
}


// ---- the rate governor -------------------------------------------------- //

// 0x10015f57 — the governor's constructor: a cold counter and no rate, so a
// controller nobody has set a rate on never fires.
//
// It is declared in servermgr.h now, not on a view here: Client's own
// constructor calls it, and a view class cannot make that call appear. What
// the shipped bytes pin is the member order -- the two `fstps` put m_Rate at
// +8 before m_Accum at +0xc, with the embedded counter constructed at +0 by
// the initialiser list ahead of both.
CRateController::CRateController() : m_Counter(0)
{
	m_Rate = 0.0f;
	m_Accum = 0.0f;
}

// Setting a rate restarts the window: whatever was owed at the old rate is
// dropped rather than carried across.
void CRateController::SetRate(float fRate)
{
	m_Rate = fRate;
	m_Accum = 0.0f;

	m_Counter.Start();
}

float CRateController::GetRate()
{
	return m_Rate;
}

// A rate of zero means never. Otherwise the elapsed time joins whatever was
// carried over, and one interval's worth is spent — the remainder stays,
// which is what keeps a series of slow frames from drifting.
LTBOOL CRateController::Update()
{
	float fInterval;
	float fTotal;

	if (m_Rate == 0.0f)
		return LTFALSE;

	fInterval = 1.0f / m_Rate;
	fTotal = ((CBaseCounterReadView*)&m_Counter)->sub_10005fab() * 0.001f + m_Accum;

	if (fTotal > fInterval)
	{
		m_Accum = (float)fmod(fTotal, fInterval);
		m_Counter.Start();

		return LTTRUE;
	}

	return LTFALSE;
}


// ---- the class module manager ------------------------------------------- //
//
// Both the construction and the initialization clear the whole record; Init
// then remembers the server it belongs to and reports success unconditionally
// (there is nothing here that can fail).

CClassMgr::CClassMgr()
{
	memset(this, 0, sizeof(*this));
}

// 0x100017a5 — five bytes, `jmp 0x100017c8`: the destructor is Term and
// nothing else, the call become a tail jump. CServerMgr's destructor reaches
// it for the member at +0x80c (0x1002c9b3).
CClassMgr::~CClassMgr()
{
	Term();
}

LTBOOL CClassMgr::Init(CServerMgr* pMgr)
{
	memset(this, 0, sizeof(*this));

	m_pServerMgr = pMgr;

	return LTTRUE;
}

// Every loaded class hands its instance bank back and unhooks itself from
// its own definition — the definition outlives the module, so the slot this
// server filled in has to be cleared or the next load finds a stale record.
void CClassMgr::Term()
{
	int i;

	if (m_hClassHash)
	{
		cb_FreeClassHash(m_hClassHash);
		m_hClassHash = LTNULL;
	}

	if (m_pClassStats)
	{
		for (i = 0; i < m_nClasses; i++)
		{
			sb_Term(&m_pClassStats[i].m_Bank);

			m_pClassStats[i].m_pClass->m_Stats[m_nIndex] = LTNULL;
		}

		::operator delete(m_pClassStats);
		m_pClassStats = LTNULL;
	}

	if (m_pServerShell)
		m_pDeleteShell(m_pServerShell);

	if (m_pUnknown08)
		cb_FreeSoundTable(m_pUnknown08);

	if (m_pUnknown00)
		cb_FreeModule(m_pUnknown00);

	if (m_hModule)
	{
		cb_ReleaseModule((CModuleRef*)m_hModule);
		m_hModule = LTNULL;
	}

	m_pUnknown08 = LTNULL;
	m_pUnknown00 = LTNULL;
	m_pServerShell = LTNULL;
}

// servermgr.h's cb_Term, which CServerMgr::Term calls right after the class
// manager's own Term. Empty in the release build: the call lands on
// 0x10036d21, a lone `ret` that /OPT:ICF made the one body of every empty
// function in the image, so what it did in a debug build is not recoverable.
void cb_Term()
{
}


// ---- loading the class module ------------------------------------------- //
//
// Four of the five functions cb_LoadModule calls are outside the run this
// file covers, so they are declared and never defined; each is named for its
// address, because nothing at the call site says what it is called. What the
// shipped bytes do say about them:
//
//   0x1000e121 (728 bytes) — the first stage, and the only one whose result
//       is a status: nonzero comes straight back out of cb_LoadModule, so
//       whatever it is, it reports an LTRESULT-shaped failure of its own.
//   0x10001977 (341 bytes) — run after that succeeds; result unused.
//   0x10001741 (55 bytes)  — a linear search: the record at CClassMgr's
//       m_pUnknown00 carries a `ClassDef**` at its +0x0c and an `int` count
//       at its +0x10, and this walks it comparing 0x1003e1f0(entry->
//       m_ClassName, pName) against zero, returning the matching ClassDef or
//       LTNULL. So m_Unknown18 holds the ClassDef for "BaseClass".
//   0x10001ae6 (62 bytes)  — run once the base class is in hand.
//   0x10001b54 (131 bytes) — the last stage, and cb_LoadModule's return
//       value: this is a tail call in everything but the epilogue.
int  cb_LoadServerObjects(CClassMgr* pClassMgr);  // 0x1000e121
int  sub_10001977(CClassMgr* pClassMgr);

// The module record CClassMgr::m_pUnknown00 points at. Two of its five words
// are reached from outside, each through an eight-byte cdecl accessor of its
// own (0x10001739 and 0x10001731) as well as directly by the lookup, which is
// what says they are fields of a record and not two loose globals; the third
// is the DLL reference the record keeps alive, written by 0x10001635 and read
// by cb_FreeModule. The two words in front of it are set by 0x100016be, which
// is not reconstructed, so they have no names (names ours).
class CClassModuleView
{
public:
	uint8       m_pad_00[0x08];
	CModuleRef* m_pModuleRef;                // +0x08
	ClassDef**  m_pClasses;                  // +0x0c
	int         m_nClasses;                  // +0x10
};

// 0x10009be4 is a bare `ret` with a name elsewhere: it is commonmodel.cpp's
// w_NullLoadHook, the world load request's default hook, the only other
// reference to that address (commonmodel.cpp 0x10008084). Which no-op the
// source called here is not recoverable; the address is (names ours).
void w_NullLoadHook(int nParam);

// What a game DLL exports, all three resolved by name at load time.
//
// ObjectDLLSetup hands back the module's class table, writes the class count
// through its first argument and the interface version it was built against
// through its third; the second is the engine-side object the DLL is given.
// The Get<name>Functions / Get<name>Version pair is the generic plug-in
// protocol 0x10034ec6 implements: the name is formatted into the export, so
// "ServerShell" resolves GetServerShellFunctions and GetServerShellVersion.
typedef ClassDef** (*ObjectDLLSetupFn)(int* pnClasses, class CUnknown928Obj* pObj,
                                       int* pnVersion);
typedef void (*GetModuleFunctionsFn)(void** ppCreate, void** ppDelete);
typedef int  (*GetModuleVersionFn)();

// The twelve-byte record 0x10034ec6 builds for a plug-in DLL: the module
// reference plus the two entry points its Get<name>Functions export reported.
// cb_FreeSoundTable is what takes one down — the "sound table" in that name
// is the previous reconstruction's guess and is wrong; it is this record
// (names ours).
class CModuleFuncs
{
public:
	CModuleRef* m_pModuleRef;                // +0x00
	void*       m_pfnCreate;                 // +0x04
	void*       m_pfnDelete;                 // +0x08
};


// 0x100016be — sanitise every property the object DLL declares, run once
// over the whole class table as soon as it arrives.
//
// A property type outside the SDK's PT_STRING..PT_ROTATION range — above it
// or negative — is rewritten to 8, which is one past the last PT_ and so is
// not a valid type either. So the engine does not reject a bad type, it
// normalises every bad type to the same out-of-range value; whatever reads
// the type afterwards is what has to know that 8 means "none of the above".
//
// Both counts are signed and the widths are the SDK's: the class count is a
// 32-bit compare against CClassModuleView's int, the property count a 16-bit
// one against ClassDef::m_nProps, sign-extended with `movswl` only on the
// inner back edge where the loop counter it is compared against is an int.
// Nothing is cached across an iteration — the class table is re-read from
// the record and re-indexed on every pass of the inner loop — which is what
// VC6 strength-reduces into the `&m_pClasses[i]` it keeps in a register.
// The one thing that *is* a named local is the property: the shipped code
// computes its address once and both reads and the store go through it.
void sub_100016be(CClassModuleView* pModule)
{
	int i;
	int j;

	for (i = 0; i < pModule->m_nClasses; i++)
	{
		for (j = 0; j < pModule->m_pClasses[i]->m_nProps; j++)
		{
			PropDef* pProp = &pModule->m_pClasses[i]->m_Props[j];

			if (pProp->m_PropType >= 8 || pProp->m_PropType < 0)
				pProp->m_PropType = 8;
		}
	}
}

#ifdef NOLFSRC_MODERN
// fixed: the property-type guard normalises to a value that is also out of
// range (RETAIL_BUGS.md). The modern build checks first, and an object DLL
// declaring any type outside PT_STRING..PT_ROTATION is refused as invalid.
static bool cnt_PropTypesValid(CClassModuleView* pModule)
{
	int i;
	int j;

	for (i = 0; i < pModule->m_nClasses; i++)
	{
		for (j = 0; j < pModule->m_pClasses[i]->m_nProps; j++)
		{
			short nType = pModule->m_pClasses[i]->m_Props[j].m_PropType;

			if (nType < PT_STRING || nType > PT_ROTATION)
				return false;
		}
	}

	return true;
}
#endif

// 0x10001583 — load a DLL and wrap it in a reference-counted record. Zero
// means it would not load; -1 means it did, the same "nothing to report"
// convention 0x10001635 and 0x10034ec6 answer with.
//
// The reference starts at one, and cb_ReleaseModule is what spends it: the
// record is always freed and the library only unloaded on the last one. The
// allocation is twelve bytes for two fields, so a word of the record is never
// touched anywhere in the image.
int sub_10001583(char* pFilename, CModuleRef** ppRef)
{
	HMODULE     hModule;
	CModuleRef* pRef;

	hModule = LoadLibrary(pFilename);

	if (!hModule)
		return 0;

	pRef = (CModuleRef*)dalloc(12);

	pRef->m_hModule = hModule;
	pRef->m_nRefs = 1;

	*ppRef = pRef;

	return -1;
}

// 0x100015b2 is cb_ReleaseModule, above. 0x10001624 — resolve one export off
// a module reference; a tail call, so it leaves no frame (name ours).
void* sub_10001624(CModuleRef* pRef, char* pName)
{
	return GetProcAddress((HMODULE)pRef->m_hModule, pName);
}

// 0x100015ce — "bm_SetInstanceHandle", from its own error tag, and the only
// function in this cluster whose name survived. A DLL that exports
// SetInstanceHandle is handed its own module handle through it, which is how
// a game DLL gets an HINSTANCE without a DllMain of its own. A DLL that does
// not export it is not an error; a null reference is.
LTRESULT bm_SetInstanceHandle(CModuleRef* pRef)
{
	void (*pfn)(void* hModule);

	if (!pRef)
		RETURN_ERROR(1, bm_SetInstanceHandle, LT_INVALIDPARAMS);

	pfn = (void (*)(void*))GetProcAddress((HMODULE)pRef->m_hModule,
	                                      "SetInstanceHandle");

	if (pfn)
		pfn(pRef->m_hModule);

	return LT_OK;
}

// 0x10001635 — load the game's object DLL and take its class table.
//
// -1 means "nothing to report". 0 is a DLL that would not load at all, 1 a
// DLL with no ObjectDLLSetup export, and 2 a DLL that reported an interface
// version other than 1 — and all three release the module reference on the
// way out, so a rejected DLL is unloaded before the caller sees the code.
//
// The module reference lives in the dead pFilename parameter slot for the
// whole function. The record is twenty bytes: two words 0x100016be owns, the
// reference, the class table and the count — and the count is filled by
// ObjectDLLSetup writing through a pointer into the record itself, which is
// why the shipped code takes the address of a field rather than of a local.
//
// The three failures are written out as separate `cb_ReleaseModule(pRef);
// return N;` statements, not as one shared tail with a status variable. VC6
// merges them itself and does it by materialising the differing constant
// into a callee-saved register — `push $1 / pop %esi`, `push $2 / pop %esi`
// — so the release, the `mov %esi,%eax` and the jump to the epilogue are
// written once. A status local plus one exit sinks all three cold blocks
// past the success path instead and misses by three blocks at the same size.
int sub_10001635(char* pFilename, class CUnknown928Obj* pObj,
                 CClassMgr* pClassMgr, int* pnVersion)
{
	CModuleRef*       pRef;
	ObjectDLLSetupFn  pfnSetup;
	CClassModuleView* pModule;

	if (!sub_10001583(pFilename, &pRef))
		return 0;

	pfnSetup = (ObjectDLLSetupFn)sub_10001624(pRef, "ObjectDLLSetup");

	if (!pfnSetup)
	{
		cb_ReleaseModule(pRef);

		return 1;
	}

	pModule = (CClassModuleView*)malloc(0x14);

	pModule->m_pModuleRef = pRef;
	pModule->m_pClasses = pfnSetup(&pModule->m_nClasses, pObj, pnVersion);

	if (*pnVersion != 1)
	{
		free(pModule);

		cb_ReleaseModule(pRef);

		return 2;
	}

#ifdef NOLFSRC_MODERN
	if (!cnt_PropTypesValid(pModule))
	{
		free(pModule);

		cb_ReleaseModule(pRef);

		return 1;
	}
#endif
	sub_100016be(pModule);
	w_NullLoadHook((int)pModule);

	pClassMgr->m_pUnknown00 = pModule;

	return -1;
}

// 0x10034ec6 — the generic plug-in load, and the one cb_LoadServerObjects
// uses for the server shell. The export names are built from the caller's
// own name, so "ServerShell" resolves GetServerShellFunctions and
// GetServerShellVersion; the version export is called first and its answer
// written back to the caller whether or not it is the one asked for, which
// is what lets the caller report "wanted 2, found 3".
//
// Four answers: 0 no DLL, 1 no such export pair, 2 the wrong version, -1
// fine. The last three are three separate `cb_ReleaseModule(pRef); return N;`
// statements which VC6 tail-merges into one, materialising the constant into
// %esi with `push $imm / pop %esi` so the release and the epilogue jump are
// written once — the same shape as 0x10001635 above. The success path is the
// function's last statement and lands *after* that merged tail.
//
// The record it leaves behind is twelve bytes and the module reference is
// its first field, which is what cb_FreeSoundTable then releases.
int sub_10034ec6(char* pFilename, char* pFnName, void** ppRec,
                 int nWantVersion, int* pnVersion)
{
	char                 sExport[100];
	CModuleRef*          pRef;
	GetModuleFunctionsFn pfnFuncs;
	GetModuleVersionFn   pfnVersion;
	void*                pfnCreate;
	void*                pfnDelete;
	CModuleFuncs*        pRec;
	int                  nVersion;

	if (!sub_10001583(pFilename, &pRef))
		return 0;

	bm_SetInstanceHandle(pRef);

	sprintf(sExport, "Get%sFunctions", pFnName);

	pfnFuncs = (GetModuleFunctionsFn)sub_10001624(pRef, sExport);

	if (!pfnFuncs)
	{
		cb_ReleaseModule(pRef);

		return 1;
	}

	sprintf(sExport, "Get%sVersion", pFnName);

	pfnVersion = (GetModuleVersionFn)sub_10001624(pRef, sExport);

	if (!pfnVersion)
	{
		cb_ReleaseModule(pRef);

		return 1;
	}

	nVersion = pfnVersion();

	*pnVersion = nVersion;

	if (nVersion != nWantVersion)
	{
		cb_ReleaseModule(pRef);

		return 2;
	}

	pfnFuncs(&pfnCreate, &pfnDelete);

	pRec = (CModuleFuncs*)malloc(12);

	pRec->m_pModuleRef = pRef;
	pRec->m_pfnCreate = pfnCreate;
	pRec->m_pfnDelete = pfnDelete;

	*ppRec = pRec;

	return -1;
}

// 0x10034f9b — hand the two entry points back out of the record above. Both
// out-parameters are unconditional; there is no null check on either.
void sub_10034f9b(CModuleFuncs* pRec, void** ppCreate, void** ppDelete)
{
	*ppCreate = pRec->m_pfnCreate;
	*ppDelete = pRec->m_pfnDelete;
}

ClassDef** cb_GetModuleClasses(void* pModule);   // 0x10001739 (name ours)
int        cb_GetModuleClassCount(void* pModule);  // 0x10001731 (name ours)


// 0x10001741 — the module's linear class lookup: walk its table comparing
// each definition's name against the one asked for and hand back the first
// that matches, LTNULL if none does.
//
// The count is *signed* (`jle` on the entry test, `jl` on the back edge), and
// the loop is rotated, which is why the entry test compares memory against
// the zero already in edi while the back edge compares the other way round.
// The table and the index are re-read in the return block rather than kept
// live — `pMod->m_pClasses[i]` written twice, once in the comparison and once
// in the `return`.
ClassDef* sub_10001741(void* pModule, char* pName)
{
	CClassModuleView* pMod = (CClassModuleView*)pModule;
	int i;

	for (i = 0; i < pMod->m_nClasses; i++)
	{
		if (strcmp(pMod->m_pClasses[i]->m_ClassName, pName) == 0)
			return pMod->m_pClasses[i];
	}

	return LTNULL;
}

// 0x10001739 and 0x10001731 — the two fields above, each behind a cdecl
// accessor of its own (names ours).
ClassDef** cb_GetModuleClasses(void* pModule)
{
	return ((CClassModuleView*)pModule)->m_pClasses;
}

int cb_GetModuleClassCount(void* pModule)
{
	return ((CClassModuleView*)pModule)->m_nClasses;
}


// 0x10001b24 — fill one inherited slot of a class definition from its
// parent's, recursively. The slot is named by its byte offset because that
// is how the caller names it: 0x0c and 0x10 in a ClassDef are
// m_ConstructFn and m_DestructFn, and the offset arrives as an ordinary
// integer argument (it ends up as the *base* of the final load's
// `mov (%ebx,%eax,1)`, with the parent pointer as the index).
//
// A slot that is already filled is left alone, and a class with no parent
// has nothing to inherit; the recursion fills the parent's slot first so
// that a chain resolves in one pass. The parent is re-read after the
// recursive call because the call could have written it (name ours).
void sub_10001b24(ClassDef* pDef, int nOffset)
{
	void** ppSlot = (void**)((uint8*)pDef + nOffset);

	if (!*ppSlot && pDef->m_ParentClass)
	{
		sub_10001b24(pDef->m_ParentClass, nOffset);

		*ppSlot = *(void**)((uint8*)pDef->m_ParentClass + nOffset);
	}
}


// 0x10001ae6 — run once the base class is in hand: give every class in the
// module the constructor and destructor it does not declare itself.
//
// The table and the count come out of the module record through their own
// accessors, and the record is read off the manager twice — the shipped code
// pushes `(%edi)` at both calls rather than caching it.
void sub_10001ae6(CClassMgr* pClassMgr)
{
	ClassDef** ppClasses = cb_GetModuleClasses(pClassMgr->m_pUnknown00);
	int        nClasses  = cb_GetModuleClassCount(pClassMgr->m_pUnknown00);
	int        i;

	for (i = 0; i < nClasses; i++)
	{
		ClassDef* pClass = ppClasses[i];

		sub_10001b24(pClass, 0x0c);
		sub_10001b24(pClass, 0x10);
	}
}


// The three CClassStats fields servermgr.h leaves in padding, on a view of
// the whole record so one cast serves every use: the class definition the
// record belongs to, the record's own index within the module (a uint16 —
// the store is `mov %ax,0x28(%esi)`), and the static instance at +0x48.
// The stride the shipped code walks is 0x4c, so the view has to come out at
// exactly the record's size (names ours).
class CClassStatsView
{
public:
	CClassStatsView();                       // 0x10001acc

	uint8           m_pad_00[0x08];
	StructBank      m_Bank;                  // 0x08  instances of this class
	ClassDef*       m_pClass;                // 0x24
	uint16          m_nClassIndex;           // 0x28
	uint8           m_pad_2a[0x2c - 0x2a];
	uint32          m_Ticks;                 // 0x2c
	uint32          m_nUpdated;              // 0x30
	uint32          m_nObjects;              // 0x34
	uint32          m_nFrames;               // 0x38
	uint32          m_TotalTicks;            // 0x3c
	uint32          m_MaxTicks;              // 0x40
	uint32          m_bPrinted;              // 0x44
	class LTObject* m_pStaticObject;         // 0x48
};

// 0x10001acc — the record's constructor, which clears the seven profile
// counters and nothing else. It is reached only as the per-element body of
// the array `new` in sub_10001977 (which then memsets the whole block over
// the top of it, so none of this survives).
//
// `inline`, out of class: server.dll (/O1) still emits it and calls it, but
// lithtech.exe (/O2) expands it into the vector-new loop -- seven `mov
// %ebp,..(%ecx)` stores at 0x00402afc..0x00402b0d and no call.  Measured:
// without `inline` the lithtech twin 0x00402a50 is 454 bytes against 444.
inline CClassStatsView::CClassStatsView()
{
	m_Ticks      = 0;
	m_nUpdated   = 0;
	m_nObjects   = 0;
	m_nFrames    = 0;
	m_TotalTicks = 0;
	m_MaxTicks   = 0;
	m_bPrinted   = 0;
}


// 0x10001977 — "InitExtraClassData" (the name is the one its own error site
// reports): give the module's class table a per-server stats record each and
// index them by name.
//
// The module can be shared between a server and a local client, so the first
// thing the function does is claim one of the two internal slots every
// ClassDef carries at +0x24 — slot 0 if it is free, slot 1 otherwise, and if
// both are taken there is nowhere to put the records and the load fails. The
// claimed slot number goes on the manager and every record is filed through
// it.
//
// The records come out of one array `new`, which is why the count-guarded
// constructor loop is inline (`lea -0x1(%edi),%eax; test; jl; inc`) with the
// element constructor left as the call on server.dll (inlined on lithtech,
// see the constructor): that is VC6's vector-constructor
// expansion. It also leaves an unreferenced `??_H` vector-constructor-
// iterator COMDAT behind, which is the one symbol this file emits that the
// shipped image does not contain — the linker dropped it there for the same
// reason, nothing referenced it. It is counted as unmatched and should be
// read as "not in the image" rather than as wrong. The
// block is then memset to zero over the top of the constructors that just
// ran, so the seven counters are cleared twice.
//
// Both failure sites report the same name and the same code, so VC6 folds
// them into one block that sits after the loop; both `return 1` and the
// function's own `return 1` share the epilogue's `push $1; pop %eax`.
//
// Three induction variables run the main loop — the index (in the dead
// `pClassMgr` parameter slot, because it is needed as a uint16 for the
// record's own index field), the class-table pointer, and the byte offset
// into the stats array — because both arrays are *indexed* in the source and
// VC6 strength-reduces each. The manager's fields are re-read from memory at
// every use: the stats table, the hash and the slot number are all memory
// operands inside the loop.
int sub_10001977(CClassMgr* pClassMgr)
{
	ClassDef** ppClasses = cb_GetModuleClasses(pClassMgr->m_pUnknown00);
	int        nClasses  = cb_GetModuleClassCount(pClassMgr->m_pUnknown00);

	if (nClasses > 0)
	{
		CClassStatsView* pStats;
		int              i;

		if (ppClasses[0]->m_pInternal[0])
		{
			if (ppClasses[0]->m_pInternal[1])
				RETURN_ERROR_SYM(1, "InitExtraClassData", 1)

			pClassMgr->m_nIndex = 1;
		}
		else
		{
			pClassMgr->m_nIndex = 0;
		}

		pClassMgr->m_hClassHash = dhash_Create(0x32, 2);

		pStats = new CClassStatsView[nClasses];

		pClassMgr->m_pClassStats = (CClassStats*)pStats;
		pClassMgr->m_nClasses    = nClasses;

		memset(pStats, 0, nClasses * sizeof(CClassStatsView));

		for (i = 0; i < nClasses; i++)
		{
			ClassDef*        pClass = ppClasses[i];
			CClassStatsView* pRec =
				&((CClassStatsView*)pClassMgr->m_pClassStats)[i];
			void*            hElement;

			sb_Init2(&pRec->m_Bank, pClass->m_ClassObjectSize, 0x20, 8);

			// Index first: lithtech 0x00402b6a stores the word before
			// 0x00402b6e stores the class; server.dll matches either way.
			pRec->m_nClassIndex = (uint16)i;
			pRec->m_pClass      = pClass;

			pClass->m_pInternal[pClassMgr->m_nIndex] = pRec;

			hElement = dhash_AddElement((CHashTable*)pClassMgr->m_hClassHash,
			                            pClass->m_ClassName,
			                            strlen(pClass->m_ClassName));

			if (!hElement)
				RETURN_ERROR_SYM(1, "InitExtraClassData", 1)

			dhash_SetData(hElement, pRec);
		}
	}

	return 1;
}


// 0x10001b54 — the last stage of the module load, and cb_LoadModule's
// return value: every class the module declares with bit 2 of its
// m_ClassFlags gets its single instance made now, at load time.
//
// The create struct is cleared twice over: once by its own constructor on
// the way into the function and once at the head of every iteration. It is
// the SDK's ObjectCreateStruct, whose in-class Clear() server.dll's /Os
// calls out of line (0x10001bd7) and lithtech.exe's /O2 expands at both
// sites (0x00402c90); sm_AllocateObjectInline (servermodel.h) is likewise
// called here and expanded there. One spelling, both images.
//
// The class definition comes out of the stats table (CClassStats+0x24, the
// back-pointer whose first fields are a ClassDef's), and the table itself is
// re-read off the manager for the out-parameter rather than kept live. The
// manager's server pointer is likewise pushed straight out of memory at both
// calls. Nothing looks at what the creation returned, and the function
// itself always reports zero.
int sub_10001b54(CClassMgr* pClassMgr)
{
	ObjectCreateStruct ocs;
	int i;

	for (i = 0; i < pClassMgr->m_nClasses; i++)
	{
		ClassDef*   pClass;
		LPBASECLASS pObject;

		ocs.Clear();

		pClass = (ClassDef*)pClassMgr->m_pClassStats[i].m_pClass;

		if (pClass->m_ClassFlags & 4)
		{
			pObject = sm_AllocateObjectInline(pClassMgr->m_pServerMgr, pClass);

			sm_CreateObjectInWorld(pClassMgr->m_pServerMgr, pObject, pClass,
			                       &ocs, 0xffff, 0.0f,
			                       &((CClassStatsView*)
			                             pClassMgr->m_pClassStats)[i].m_pStaticObject);
		}
	}

	return 0;
}

// The shell *creator*. servermgr.h leaves CClassMgr+0x0c inside the pad in
// front of m_pDeleteShell because nothing else in the reconstruction reaches
// it; cb_LoadModule calls through it, so this file needs a view of the record
// that names it. Its one argument is the manager's 0x928 object.
class CClassMgrCreateView
{
public:
	uint8 m_pad_00[0x0c];
	class IServerShell* (*m_pCreateShell)(class CUnknown928Obj* pArg);
};

// The three file-manager entry points this file reaches, all of them in the
// file manager's own run and declared here rather than defined (names by
// address where nothing at the call site names them).
//
//   0x1002bde9 — walk the mounted trees for a named file (serverengine.h's
//       fs_FindFileInTrees; both out-parameters optional).
//   0x10006fa5 — the tree's own type word, with a null handle answering 1.
//   0x100070b6 — build the on-disk path of a file inside a *directory* tree:
//       it re-checks the type word, sprintf's "%s\%s" from the tree's own
//       directory name (at its +0x20) and the file name, and answers 1. Its
//       fourth argument, the caller's buffer length, is pushed and never
//       read — so the caller here can overrun pOutPath and the "%s%s" path
//       below can too. Only the GetTempFileName fallback bounds its copy.
int fs_FindFileInTrees(CFileMgr* pFileMgr, char* pFilename,
                       void** ppTreeRec, uint32* pUnknown);

// serverengine.h's CFileTreeRec, repeated here so this file need not include
// it: the only field anything reads is the tree the record stands for.
class CFileTreeRecView
{
public:
	uint8 m_pad_00[0x0c];
	void* m_pTree;                           // 0x0c
};

void* sub_10006fa5(void* hTree);
int   sub_100070b6(void* hTree, char* pFilename, char* pOutPath,
                   uint32 nOutLen);


// 0x1000e05b — put a named file somewhere the loader can open it by path,
// and hand that path back. Zero means it is there.
//
// Three ways that can happen, in order. If a mounted tree has the file and
// that tree is a plain directory on disk, the file is already unpacked and
// the answer is just the directory joined to the name — no copy at all. Any
// other tree (a .rez) has to be extracted, so the name is appended to the
// caller's temp path and the file copied out. If *that* copy fails — the
// usual reason being that the name is already taken by a file somebody has
// open — Windows is asked for a unique name in the same directory and the
// copy retried, and only then is the caller's buffer length respected.
//
// A failure returns the *first* copy's error code, not the retry's: the
// retry's own failure returns its own code, but a GetTempFileName that
// cannot produce a name at all falls back to what the first attempt said.
//
// The tree record is an address-taken local and the shipped build parks it
// in the dead pMgr parameter slot, which is also why pMgr itself is read
// once, at the top, into the file-manager pointer and never again.
//
// Three paths return zero and the shipped build keeps the shared `xor eax;
// jmp` at the FIRST of them (the tree arm, 0x1000e0ad), sinking `return
// nResult` to the end. That is the if/else-chain form of the shared-exit
// rule: every test an explicit `else`, every arm ending in its own return.
// Written as a run of guarded early returns instead, VC6 hosts the zero at
// the last one (the strncpy arm) -- same size, one block out.
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

#endif
int sub_1000e05b(CServerMgr* pMgr, char* pTempPath, char* pFilename,
                 char* pOutPath, uint32 nOutLen)
{
	char      sUnique[MAX_PATH];
	CFileMgr* pFileMgr = &pMgr->m_FileMgr;
	void*     pTreeRec;
	int       nResult;
	int       nRetry;
#ifdef NOLFSRC_MODERN

	// M-P9-01: object.lto and sres.dll from custom\ first
	if (P9_OwnGameFile(pFilename, pOutPath, nOutLen))
		return 0;
#endif

	if (fs_FindFileInTrees(pFileMgr, pFilename, &pTreeRec, LTNULL) &&
	    sub_10006fa5(((CFileTreeRecView*)pTreeRec)->m_pTree) == (void*)1)
	{
#ifdef NOLFSRC_MODERN
		// fixed: the object-DLL temp name, one copy unterminated, two
		// unbounded (RETAIL_BUGS.md). A path that does not fit the caller's
		// buffer fails like a copy that did not work.
		if (!sub_100070b6(((CFileTreeRecView*)pTreeRec)->m_pTree, pFilename,
		                  pOutPath, nOutLen))
			return 1;
#else
		sub_100070b6(((CFileTreeRecView*)pTreeRec)->m_pTree, pFilename,
		             pOutPath, nOutLen);
#endif

		return 0;
	}
	else
	{
#ifdef NOLFSRC_MODERN
		int nLen = _snprintf(pOutPath, nOutLen, "%s%s", pTempPath, pFilename);

		if (nLen < 0 || (uint32)nLen >= nOutLen)
		{
			if (nOutLen)
				pOutPath[nOutLen - 1] = '\0';

			return 1;
		}
#else
		sprintf(pOutPath, "%s%s", pTempPath, pFilename);
#endif

		nResult = fs_CopyFile(pFileMgr, pFilename, pOutPath);

		if (nResult == 0)
		{
			return 0;
		}
		else if (GetTempFileName(pTempPath, pFilename, 0, sUnique))
		{
			nRetry = fs_CopyFile(pFileMgr, pFilename, sUnique);

			if (nRetry != 0)
			{
				return nRetry;
			}
			else
			{
				strncpy(pOutPath, sUnique, nOutLen - 1);
#ifdef NOLFSRC_MODERN
				pOutPath[nOutLen - 1] = '\0';
#endif

				return 0;
			}
		}
		else
		{
			return nResult;
		}
	}
}


// 0x1000e121 — "LoadServerObjects": get the game's object DLL and its
// server shell onto disk and into the process.
//
// Both DLLs travel inside the resource trees, so each is extracted to the
// temp directory first (0x1000e05b) and loaded from there. The object DLL
// goes first; if it reports anything other than -1 the load has failed and
// the code it reported becomes one of the LT_INVALIDOBJECTDLL* errors. Then
// the shell DLL: its "ServerShell" export is resolved against version 2, its
// create and delete entry points are taken out of the module, and finally
// the string-resource DLL is extracted and loaded. Only that last step
// returning -1 — its own "nothing to report" — is success, and the function
// answers 0.
//
// Four readings worth writing down.
//
// The two error names are both call-site literals and they disagree:
// four sites say "LoadServerObjects" and the two that report the object
// DLL's own code say "LoadObjectsInDirectory". de_error.h's macros take the
// name as an argument, so this is source, not a mistake — the object-DLL
// arm was written where the reporting for that helper lives.
//
// The status-0 and status-1 arms of the object-DLL failure are the *same*
// error, written out twice; VC6 keeps both SetupError/dsi_OnReturnError
// pairs and folds only the tail from the debug-level test onward. That fold
// is why one of them compares g_DebugLevel against an immediate and the
// other against %ebx: the literal 1 is materialised into a register for the
// `== 1` test and then serves the level compare and SetupError's expected-
// version argument as well. The shell's version arm does the same thing with
// the 2 that its own `== 2` test just proved.
//
// `pMgr` is a plain local: it starts in edi, is spilled to the dead
// `pClassMgr` parameter slot, and edi is then reused for the second local —
// a pointer to the manager's shell-module slot at +0x08, which four
// different statements reach. The two error blocks that sit *before* that
// reuse still push %edi for the manager.
//
// The description every site passes is the *packed* file name, not the temp
// path SetupError formats with. Both are live at once, which is what the
// separate esi is for.
// NEAR on lithtech.exe only (0x0042da50, 939/939, two blocks): /O2 hosts the
// shared `return LT_INVALIDOBJECTDLL` tail of the status-0/1 pair in the
// status-0 arm, where ours (and server.dll's /O1, identical) hosts it in the
// status-1 arm -- only the `mov $0x36,%eax` moves across the pops. Measured
// and not it: `else if` for the second arm (no change), the two arms in the
// other order and a switch over them (both regress server.dll).
int cb_LoadServerObjects(CClassMgr* pClassMgr)
{
	char sTempPath[200];
	char sFilename[256];
	int  nVersion;

	CServerMgr* pMgr = pClassMgr->m_pServerMgr;
	void**      ppShellModule;
	int         nStatus;
	int         nResult;

	if (!GetTempPath(sizeof(sTempPath), sTempPath))
		strcpy(sTempPath, ".\\");

	if (sub_1000e05b(pMgr, sTempPath, "object.lto", sFilename,
	                 sizeof(sFilename)) == 0)
	{
		nStatus = sub_10001635(sFilename, pMgr->m_pUnknown928, pClassMgr,
		                       &nVersion);

		if (nStatus == -1)
		{
			ppShellModule = &pClassMgr->m_pUnknown08;

			nResult = sub_10034ec6(sFilename, "ServerShell", ppShellModule, 2,
			                       &nVersion);

			if (nResult == 0)
			{
				pMgr->SetupError(LT_MISSINGSHELLDLL, sFilename);

				RETURN_ERROR_SYM_DESC(1, "LoadServerObjects",
				                      LT_MISSINGSHELLDLL, "object.lto")
			}

			if (nResult == 1)
			{
				pMgr->SetupError(LT_INVALIDSHELLDLL, sFilename);

				RETURN_ERROR_SYM_DESC(1, "LoadServerObjects",
				                      LT_INVALIDSHELLDLL, "object.lto")
			}

			if (nResult == 2)
			{
				pMgr->SetupError(LT_INVALIDSHELLDLLVERSION, sFilename,
				                 nVersion, 2);

				RETURN_ERROR_SYM_DESC(1, "LoadServerObjects",
				                      LT_INVALIDSHELLDLLVERSION, "object.lto")
			}

			sub_10034f9b((CModuleFuncs*)*ppShellModule,
			             (void**)&((CClassMgrCreateView*)pClassMgr)->m_pCreateShell,
			             (void**)&pClassMgr->m_pDeleteShell);

			if (sub_1000e05b(pMgr, sTempPath, "sres.dll", sFilename,
			                 sizeof(sFilename)) == 0)
			{
				if (sub_10001583(sFilename,
				                 (CModuleRef**)&pClassMgr->m_hModule) == -1)
					return 0;
			}

			cb_FreeSoundTable(*ppShellModule);

			*ppShellModule = LTNULL;

			pMgr->SetupError(LT_ERRORCOPYINGFILE, "sres.dll");

			RETURN_ERROR_SYM_DESC(1, "LoadServerObjects", LT_ERRORCOPYINGFILE,
			                      "sres.dll")
		}

		if (nStatus == 0)
		{
			pMgr->SetupError(LT_INVALIDOBJECTDLL, "object.lto");

			RETURN_ERROR_SYM_DESC(1, "LoadObjectsInDirectory",
			                      LT_INVALIDOBJECTDLL, "object.lto")
		}

		if (nStatus == 1)
		{
			pMgr->SetupError(LT_INVALIDOBJECTDLL, "object.lto");

			RETURN_ERROR_SYM_DESC(1, "LoadObjectsInDirectory",
			                      LT_INVALIDOBJECTDLL, "object.lto")
		}

		pMgr->SetupError(LT_INVALIDOBJECTDLLVERSION, "object.lto", nVersion, 1);

		RETURN_ERROR_SYM_DESC(1, "LoadObjectsInDirectory",
		                      LT_INVALIDOBJECTDLLVERSION, "object.lto")
	}

	pMgr->SetupError(LT_ERRORCOPYINGFILE, "object.lto");

	RETURN_ERROR_SYM_DESC(1, "LoadServerObjects", LT_ERRORCOPYINGFILE,
	                      "object.lto")
}


// 0x10001894 — bring the class module up: load it, find the root class the
// engine needs from it, and build the game's server shell.
//
// Three things the bytes say that are worth writing down.
//
// The lookup goes the long way round. The module record it searches is
// `pClassMgr->m_pUnknown00`, but the shipped code does not read it off the
// manager it was handed — it goes out through m_pServerMgr and back in
// through CServerMgr+0x80c, which *is* this same CClassMgr. Two loads for a
// field one load away, and reproducing it is the difference between matching
// and not. It also needs the named local: written inline the expression is a
// simple memory operand and VC6 pushes it in place (`push 0x80c(%eax)`),
// which is a byte shorter than the `mov 0x80c(%eax),%eax; push %eax` the
// shipped code has.
//
// Both failure sites report twice: once into the manager's own error string
// (CServerMgr::SetupError, which the interface's GetErrorString hands back)
// and once through the debug printer, at threshold 1. The first passes the
// class name as the printer's description argument rather than the usual lone
// space, which is what says the description really is a per-site argument.
//
// The second site's return code is a bare `1`, not the LT_CANTCREATESERVERSHELL
// (53) it has just recorded in the error string — the code string in .data
// reads "1", so the source wrote the literal. So a caller that only looks at
// the return value cannot tell a missing shell from any other failure; the
// distinction lives only in the error string.
int cb_LoadModule(CClassMgr* pClassMgr)
{
	int nStatus;

	nStatus = cb_LoadServerObjects(pClassMgr);
	if (nStatus != 0)
		return nStatus;

	sub_10001977(pClassMgr);

	void* pModule = pClassMgr->m_pServerMgr->m_ClassMgr.m_pUnknown00;

	pClassMgr->m_Unknown18 = (uint32)sub_10001741(pModule, "BaseClass");

	if (!pClassMgr->m_Unknown18)
	{
		pClassMgr->m_pServerMgr->SetupError(LT_MISSINGCLASS, "%s", "BaseClass");

		RETURN_ERROR_SYM_DESC(1, "LoadServerBinaries", LT_MISSINGCLASS, "BaseClass")
	}

	sub_10001ae6(pClassMgr);

	pClassMgr->m_pServerShell = ((CClassMgrCreateView*)pClassMgr)->m_pCreateShell(
		pClassMgr->m_pServerMgr->m_pUnknown928);

	if (!pClassMgr->m_pServerShell)
	{
		pClassMgr->m_pServerMgr->SetupError(LT_CANTCREATESERVERSHELL, "");

		RETURN_ERROR_SYM(1, "LoadServerBinaries", 1)
	}

	return sub_10001b54(pClassMgr);
}


// 0x10005f17 — set the scale up. The queried frequency is read and thrown
// away: the scale is left at one, so the raw read is counter ticks (name
// ours).
void cnt_Init()
{
	LARGE_INTEGER freq;

	QueryPerformanceFrequency(&freq);

	g_CounterScale = 1;
}


// The last holder of a module is the one that unloads it; the record itself
// always goes back to the engine's allocator.
void cb_ReleaseModule(CModuleRef* pRef)
{
	if (pRef->m_nRefs == 1)
		FreeLibrary((HMODULE)pRef->m_hModule);

	dfree(pRef);
}

// The two wrappers around a module reference. Both let the module go and
// hand their own record back to the CRT rather than to the engine — they
// were not allocated through it.
void cb_FreeModule(void* p)
{
	cb_ReleaseModule(((CModuleRef**)p)[2]);

	free(p);
}

void cb_FreeSoundTable(void* p)
{
	cb_ReleaseModule(*(CModuleRef**)p);

	free(p);
}
