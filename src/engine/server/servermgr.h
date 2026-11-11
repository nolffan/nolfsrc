// *********************************************************************** //
//
//  MODULE:   SERVERMGR (.H)
//
//  PURPOSE:  server.dll's concrete ServerInterface. The class name is not a
//            guess: one surviving parameter-check literal in the image reads
//            "MyServerInterface::OpenFile", so that is what Monolith called
//            it. Every other method in the file reports itself as
//            "ServerInterface::<name>" — the base class — which is why the
//            concrete name shows up exactly once.
//
//            The object is a thin shell. It holds one pointer, to the
//            CServerMgr that owns the actual engine state, and every method
//            starts by checking it and returning LT_NOTINITIALIZED. The
//            console, config-file and shell-message methods below then reach
//            straight into that manager.
//
//            Method definition order in the .cpp is not free: the shipped
//            check literals sit in .data in the order their call sites were
//            first compiled, which recovers the original source order.
//            OpenFile, RunConsoleString, GetConsoleVar, GetVarValueFloat,
//            GetVarValueString, LoadConfigFile, SaveConfigFile,
//            SetAppHandler, SendToServerShell.
//
// *********************************************************************** //

#ifndef __SERVERMGR_H__
#define __SERVERMGR_H__

#include <stdarg.h>
#include "server_interface.h"
#include "iservershell.h"
#include "iltsoundmgr.h"
#include "iltstream.h"
#include "commonphysics.h"
#include "lthread.h"
#include "ltlink.h"
#include "ltcodes.h"
#include "ltobject_internal.h"

// Console variables are handles to the engine's own command-variable records;
// the full record is declared with the console state below.
class LTCommandVar;

// The console's command/variable state. Bounded above by m_pAppHandler at
// 0xac0 in the manager, so it is at most 0x4c bytes; the initialization
// clears 0x44 of them.
//
// One console variable, and one binding between a named variable and the C
// variables that mirror it. A binding names the variable it wants and holds
// the addresses to keep in step; the engine's own table of them is what the
// state picks up at startup (names ours).
class LTCommandVar
{
public:
	uint8  m_pad_00[0x10];
	char*  m_pName;                          // 0x10  the hash key, in place
	char*  m_pStringVal;                     // 0x14  interned; comparing the
	                                         //       pointers compares the text
	float  m_FloatVal;                       // 0x18
	void*  m_hElement;                       // 0x1c  its own hash element
	uint32 m_Flags;                          // 0x20  bit 0 = save me
};

class ConsoleBinding
{
public:
	char*          m_pName;                  // 0x00
	float*         m_pFloat;                 // 0x04
	int*           m_pInt;                   // 0x08
	LTCommandVar** m_ppVar;                  // 0x0c
	char**         m_ppString;               // 0x10
};

// A console command as the built-in table declares it, and as the console
// keeps it once registered — the same three fields plus the link that files
// it (names ours).
class ConsoleCommandDef
{
public:
	char* m_pName;                           // 0x00
	void* m_pFn;                             // 0x04
	void* m_pUser;                           // 0x08
};

class ConsoleCommand
{
public:
	char*  m_pName;                          // 0x00
	void*  m_pFn;                            // 0x04
	LTLink m_Link;                           // 0x08  m_pData points back here
	void*  m_pUser;                          // 0x14
};

class ConsoleState
{
public:
	// Whatever else belongs in a saved config file: each writer is handed
	// the open file after the variables have gone out.
	void          (**m_pSavers)(void* pFile);  // 0x00
	int             m_nSavers;               // 0x04
	ConsoleBinding* m_pBindings;             // 0x08
	int             m_nBindings;             // 0x0c
	ConsoleCommandDef* m_pBuiltins;          // 0x10  registered at startup
	int             m_nBuiltins;             // 0x14
	LTLink          m_CommandHead;           // 0x18

	// Everything the console reaches the outside world through, so one
	// console can serve a server and a client from the same code.
	void  (*m_pPrint)(char* pFormat, ...);   // 0x24
	void* (*m_pAlloc)(uint32 nSize);         // 0x28
	void  (*m_pFree)(void* pMem);            // 0x2c
	void  (*m_pOnCreate)(class ConsoleState* pState, LTCommandVar* pVar);   // 0x30
	void  (*m_pOnChange)(class ConsoleState* pState, LTCommandVar* pVar);   // 0x34

	class CServerMgr* m_pServerMgr;          // 0x38
	void*  m_hVarHash;                       // +0x3c  name -> LTCommandVar
	void*  m_hStringHash;                    // +0x40  the interned values
	uint8  m_pad_44[0xac0 - 0xa74 - 0x44];
};

class SoundObjectRef
{
public:
	uint32          m_Unknown00;             // 0x00
	class LTObject* m_pObject;               // 0x04
};

// A playing sound on the server. Fields recovered from the ILTSoundMgr
// methods; the flags at 0x34 are the PLAYSOUND_ set the sound was started
// with.
class SoundInstance
{
public:
	// 0x100356f3 — set the instance up from the request.
	LTBOOL Init(PlaySoundInfo* pInfo, float fTime, void* hFile, void* pTrack);

	// Update's per-tick helpers (names ours). UpdateTimer runs the timer by
	// one frame (0x1003586c); GetObject resolves the attached object
	// (0x10029b24); Term releases the instance's resources (0x10032dab).
	void UpdateTimer(float fFrameTime);
	class LTObject* GetObject();
	void Term();

	LTLink   m_Link;                         // 0x00  in CServerMgr's sound list;
	                                         //       its prev slot doubles as the bank freelist link
	uint16*  m_pUnknown0c;                   // 0x0c  +8 holds the client-table index
	LTBOOL   m_bKill;                        // 0x10  set by KillSound
	uint8    m_pad_14[0x18 - 0x14];
	void*    m_hSample;                      // 0x18  the mixer's handle
	uint8    m_pad_1c[0x20 - 0x1c];
	float    m_fTimer;                       // 0x20  seconds left
	float    m_fDuration;                    // 0x24
	void*    m_Unknown28;                    // 0x28  keeps a killed sound updating
	uint16   m_Unknown2c;                    // 0x2c  nonzero defers removal; ORed into
	                                         //       each client's table by sm_ApplyChangeFlags
	uint8    m_pad_2e[0x30 - 0x2e];
	SoundInstance* m_pNextChanged;           // 0x30  the changed-sounds list at 0x914
	uint32   m_PlaySoundFlags;               // 0x34

	// What the sound is attached to, read two ways: an attached sound holds
	// a reference record whose object is at +4, and a client-local one holds
	// the object itself (names ours).
	union {
		class SoundObjectRef* m_pObjectRef;  // 0x38  PLAYSOUND_ATTACHED
		class LTObject*       m_pObject;     // 0x38  PLAYSOUND_CLIENTLOCAL
	};
	uint8    m_pad_3c[0x60 - 0x3c];          // record size from m_Bank28's sb_Init2
};

// One queued play-sound record for the clients (from 0x1002980d).
class SoundRecord
{
public:
	uint32        m_Unknown00;
	void*         m_hFile;                   // +0x04
	PlaySoundInfo m_Info;                    // +0x08, copied whole
};

void* sm_AddSoundRecord(class CServerMgr* pMgr, int n);     // 0x1002980d
// sb_Allocate is defined INLINE below, beside sb_Free -- see there.

// 0x100343f2 — tell the clients a kept sound changed.
void sm_SetSoundChangeFlag(class CServerMgr* pServerMgr, SoundInstance* pSound, uint32 dwFlag);

// CServerMgr's ILTSoundMgr personality. It lives at CServerMgr+0x04 — just a
// vptr — and its methods step back four bytes when they need the manager,
// which is how the shipped KillSoundLoop reaches it.
//
// NOT derived from the ILTSoundMgr in src/LT2/sdk/inc: that revision of the
// header adds a protected m_ClientServerType, which would make this object
// eight bytes and shift every CServerMgr field after it by four — dozens of
// matched functions say the shipped object is four. NOLF shipped against an
// earlier iltsoundmgr.h without the member (the same header drift
// _CVector::DistSqr shows). The five virtuals below are that revision's
// interface, in its order.
class CServerSoundMgr
{
public:
	virtual LTRESULT PlaySound(PlaySoundInfo *pPlaySoundInfo, HLTSOUND &hResult) = 0;
	virtual LTRESULT GetSoundDuration(HLTSOUND hSound, LTFLOAT &fDuration) = 0;
	virtual LTRESULT IsSoundDone(HLTSOUND hSound, LTBOOL &bDone) = 0;
	virtual LTRESULT KillSound(HLTSOUND hSound) = 0;
	virtual LTRESULT KillSoundLoop(HLTSOUND hSound) = 0;
};

// CServerMgr's primary base: a virtual destructor (slot 0, which is what
// CreateServer's and DeleteServer's `delete` reach), three pure slots and one
// implemented one. Its vtable is in the image at 0x1004e314 (slots 1-3 are
// the shared __purecall stub), stored during construction and then replaced
// by CServerMgr's own pair — which is how the constructor betrayed the
// hierarchy. The name and slot names are ours.
// What the net manager tells its owner about. The connection slots are
// reached from CNetMgr; the packet slot is how a packet the net layer did
// not understand gets out to the application and the game (names ours).
class CBaseMgr
{
public:
	// Inline and empty: CServerMgr's destructor EXPANDS it, and all the
	// expansion is is this class's vftable being restored. Defined out of
	// line it becomes a call there instead, one instruction too many.
	virtual ~CBaseMgr() {}                     // 0x1002c85e
	virtual LTBOOL _v04(void* pA, void* pB) = 0;
	virtual void     _v08(class NetConnection* pConn) = 0;
	virtual void     ProcessPacket(class CPacket* pPacket, uint8* senderAddr,
	                               uint16 senderPort) = 0;   // slot 0x0c
	// The manager-notification slot: CNetMgr tells its handler about net
	// events through it (a dead connection sends (5, name, 0)). The shared
	// 3-byte body at 0x10016971 is `ret 0xc` — three arguments, none read.
	//
	// void, not LTRESULT, as this was first written: the body puts nothing
	// in eax, and VC6 will not compile a value-returning function that
	// does not return (C4716 is an error, not a warning). The one call site
	// discards the result, so nothing depended on the wrong type.
	virtual void _v10(int nEvent, char* pName, int n);       // 0x10016971
};

// The bank the server messages live in (0x9f0): a vptr over a struct bank
// initialized for 0x54-byte records — exactly sizeof(ServerMessage) — with a
// 32-deep cache.
void sb_Init(class StructBank* pBank, uint32 structSize, uint32 cacheSize);  // 0x1003d170
// 0x1003d1f0 — hand a bank's chunks back. Declared here as well as below
// because the inline bank destructors above expand into it.
void sb_FreeChunks(class StructBank* pBank, int bAll);

void sb_Term(class StructBank* pBank);                                       // 0x1003d230
void sb_Init2(class StructBank* pBank, uint32 structSize, uint32 cacheSize,
              uint32 growSize);                                              // 0x1003d1c0

class StructBank
{
public:
	// +0x00 — the record size the CLEARED allocate memsets, which is not the
	// stride sb_Grow multiplies by (that is 0x04). Named from
	// sb_AllocateCleared, the only thing that reads it.
	uint32 m_StructSize;                     // +0x00
	uint32 m_Unknown04;                      // +0x04  sb_Grow's stride
	uint32 m_Unknown08;                      // Init pokes the message bank's to 0x40
	uint8  m_pad_0c[0x18 - 0x0c];
	void*  m_pFreeList;                      // +0x18  intrusive free list
};

// The free is inline in the header the engine used — the freed struct's first
// dword becomes the next-free link — which is why si_RemoveObjectFromList
// carries it rather than calling anything.
inline void sb_Free(StructBank* pBank, void* pStruct)
{
	*(void**)pStruct = pBank->m_pFreeList;
	pBank->m_pFreeList = pStruct;
}

// 0x1003e1e0 -- grows the bank by one chunk. Declared here only so the inline
// below can reach it; the body stays in structbank.cpp, which is right --
// both shipped images CALL it.
void sb_Grow(StructBank* pBank, uint32 nCount);

// 0x10001c86 / 0x00414a40 -- and inline for the same reason sb_Free is, which
// is why it sits here rather than in structbank.cpp.
//
// 39 bytes with a nested call: ABOVE the /Os budget and INSIDE the /Ot one.
// So server.dll CALLS it at every site and lithtech.exe EXPANDS it at every
// site, from one source. Proved in objbank.cpp, where a file-local inline copy
// was worth EIGHTEEN distinct CObjectBank<T>::Allocate instantiations plus
// am_CreateAttachment; this is that copy moved to where it belongs, so every
// translation unit that allocates from a struct bank gets it.
//
// `inline` does not cost server.dll the out-of-line body: /Os declines the
// expansion, so each TU that calls it still emits the COMDAT and each still
// matches 0x10001c86.
inline void* sb_Allocate(StructBank* pBank)
{
	void* pStruct = pBank->m_pFreeList;

	if (!pStruct)
	{
		sb_Grow(pBank, pBank->m_Unknown08);

		pStruct = pBank->m_pFreeList;
		if (!pStruct)
			return LTNULL;
	}

	pBank->m_pFreeList = *(void**)pStruct;
	return pStruct;
}

// The thread-message bank (name ours): one global instance behind every
// LThreadQueue. Allocate hands out a constructed LThreadMessage; the frees
// go straight back onto the struct bank's list.
class LThreadMessageBank
{
public:
	virtual ~LThreadMessageBank();

	class LThreadMessage* Allocate();        // 0x10015b2e

public:
	StructBank m_Bank;                       // +0x04
	uint32     m_Unknown20;                  // +0x20
};

// Same two-vftable shape as CNetBank: the manager's destructor restores this
// class's own and then 0x1004d260, so the three slots after the destructor
// belong to a base (name ours).
// (CNetQueueA, CUnknown8FC, CConnList and CDriverList used to have VIRTUAL
// destructors, and so `??_G` scalar-deleting destructors the shipped image
// has no counterpart for. None of them was ever polymorphic on a destructor:
// all four are GenList queues/arrays, whose vtables open with GenBegin.)
class CBankBase
{
public:
	virtual ~CBankBase() {}
	// Declared, not pure, so a derived bank need not name all three. And the
	// THIRD slot is Term, not a placeholder: CMessageBank's Term is slot 3
	// and CServerMgr::Term calls it virtually, so a base that named that
	// slot anything else pushes Term to slot 4 and costs CServerMgr::Term
	// its 638 bytes.
	//
	// SLOT 2 TAKES THE RECORD, established from the shipped bytes rather
	// than guessed. server.dll 0x10025ae7 is slot 2 of ELEVEN of these
	// tables and lithtech.exe 0x00483470 is slot 2 of fourteen -- the same
	// 29-byte body in both, and both end `ret $4`:
	//
	//     push esi ; mov 0x8(esp),esi ; push edi ; mov ecx,edi
	//     mov (esi),eax ; push $0 ; mov esi,ecx ; call *(eax)   <- ~record
	//     mov 0x1c(edi),eax ; mov eax,(esi) ; mov esi,0x1c(edi) <- free list
	//     pop edi ; pop esi ; ret $4
	//
	// i.e. `void _v08(void* pRec) { ((Item*)pRec)->~Item(); sb_Free(...); }`
	// with the record's destructor reached virtually. That is the generic
	// bank free, folded across every bank whose record has a virtual
	// destructor; CNetBank overrides it because its record's teardown is a
	// direct call (see below).
	virtual void _v04();
	virtual void _v08(void* pRec);
	virtual void Term();
};

class CMessageBank : public CBankBase
{
public:
	CMessageBank()
	{
		sb_Init(&m_Bank, 0x54, 0x20);
	}
	// Inline: CServerMgr's destructor expands it.
	virtual ~CMessageBank() { sb_FreeChunks(&m_Bank, 1); }
	virtual void _v04();
	virtual void _v08(void* pRec);
	virtual void Term();                     // slot 3; called by CServerMgr::Term

	StructBank m_Bank;                       // +0x04
	uint32     m_Unknown20;                  // +0x20
};

// Opaque constructed members, named by offset; only their constructors are
// reached from here.
// An eight-byte object Init news up and hands `this` (ctor 0x10034fb9);
// freed raw at Term, so no destructor.
// The helper seeded into every new message: it translates objects to and from
// the ids that go on the wire, and hands out the world. Lives at
// CServerMgr+0x804, and its vftable is the four-slot one at 0x1004e900 --
// bounded exactly, with a float constant (0x477de800) immediately below it and
// another class's vftable immediately above. Same shape as ServerModelLT and
// CServerLightAnim: a vptr and nothing but a back pointer to the manager,
// which every slot reaches through as 0x4(%ecx). Name ours.
//
// There is NO virtual destructor, which is why Term deletes the object raw:
// all four slots are accounted for below and none of them is a destructor.
class CMessageObjectHelper
{
public:
	// 0x10034fcb — read an id off a message and resolve it to an object.
	virtual LTRESULT ReadObjectID(class IMessageWrite* pMsg,
	                              class LTObject** ppObject);

	// 0x1003506e — write one object's id, or 0xffff for none.
	virtual LTRESULT WriteObjectID(class IMessageWrite* pMsg,
	                               class LTObject* pObject);

	// 0x100350bc — format a message-table string and put it on the message
	// length-prefixed, then release the raw. Three fixed arguments, not
	// varargs: the tail is `ret 0xc` and the caller has done the va_start.
	virtual LTRESULT WriteFormattedString(class IMessageWrite* pMsg,
	                                      uint32 messageCode,
	                                      va_list* pMarker);

	// 0x1003511c — hand out the manager's embedded world.
	virtual LTRESULT GetServerWorld(class CServerWorld** ppWorld);

	// 0x10034fb9 — install the vftable and keep the manager.
	CMessageObjectHelper(class CServerMgr* pServerMgr);

	// 0x00 is the vptr.
	class CServerMgr* m_pServerMgr;          // +0x04
};

// An eight-byte hook Init constructs inline: a vtable and a back-pointer,
// and no destructor (Term frees it raw). It is the object mover (vtable
// 0x1004e8c8, thirteen slots); Init now constructs it through smgr.cpp's
// full CObjectMoverQ47C, and this one-slot view is only the member's type.
class CUnknown6A8Obj
{
public:
	CUnknown6A8Obj(class CServerMgr* pServerMgr)
	{
		m_pServerMgr = pServerMgr;
	}
	virtual LTRESULT _v00();

	class CServerMgr* m_pServerMgr;
};

class CUnknown390
{
public:
	CUnknown390();                           // 0x1002449b
	// 0x1002ca7c — the eleven object banks. objbank.cpp carries the wide
	// view of this object as CObjBankSet; here it is only the call.
	~CUnknown390();
	// (the class runs at least to 0x68; the pad below keeps the embedded
	// copy in CServerMgr at its original 4-byte footprint via the pad_394)
	uint8 m_pad[0x04];
};

// One slot of the object-id map at CUnknown8FC+0x04: a flag byte and the
// record the id resolves to (names ours).
class CObjectMapEntry
{
public:
	uint8  m_nFlags;                         // +0x00  zeroed on removal
	uint8  m_pad_01[0x04 - 0x01];
	void*  m_pRecord;                        // +0x04  zeroed on removal
};

// The object map, CServerMgr+0x8fc: collection.h's CMoArray<CObjectMapEntry>
// itself, as CConnList and CDriverList already are, and not a look-alike of
// it. The shipped constructor (0x1002cbe3) is the template's -- vptr
// 0x1004e328, Clear()'s four zero stores, Init(0, 0) -- and that vptr is a
// full twelve-slot GenList table (GenBegin 0x1000ba8f, GenIsValid
// 0x1000b2b7, ...), so the class has no virtual destructor and no `??_G`.
// Init (0x1000c4cf) is the DefaultCache Init every other instantiation
// shares, SetSize (0x1002ccee) is its SetSize2 and AllocItems (0x1002cecf)
// its _AllocateTArray. The hand-written class that stood here (a virtual
// destructor, its own Init/SetSize/AllocItems over m_pEntries / m_nEntries /
// m_nSlack / m_nGrowBy) compiled a `??_G`, a second constructor and a second
// Init that the shipped images do not have. A typedef, not a derived class:
// an (empty) derived class has a vtable of its own, and CServerMgr's
// constructor then stores it after calling 0x1002cbe3, which the shipped one
// (0x1002c7f7) does not.
typedef CMoArray<CObjectMapEntry> CUnknown8FC;

class CUnknownA14
{
public:
	CUnknownA14();                           // 0x1003d500
	~CUnknownA14();                          // 0x1003d580
	void Init(int n);                        // 0x1003d600
	uint8 m_pad[0xa2c - 0xa14];
};

// The object at 0x210 (only its initializer is reached; 0x10039baa).
class CUnknown210
{
public:
	void Init(CUnknown390* p390);            // 0x10039baa
	// 0x10039c43 — re-seat one object in the container (name ours);
	// SetDeactivationTime calls it to wake the object's cell.
	void BumpObject(class LTObject* pObj, int nFlag);
	uint8 m_pad[0x04];
};

// The attachment manager's spare-record list head sits 0x64 into the 0x390
// object; a freed Attachment's first dword becomes its free link.
class CAttachMgrView
{
public:
	uint8 m_pad_00[0x64];
	class Attachment* m_pFreeList;           // +0x64
};

// The server's worker thread (constructed 0x1003512f); its outgoing queue at
// +0x2c is what sm_ProcessThreadMessages drains.
class CServerThread : public LThread
{
public:
	CServerThread();                         // 0x1003512f
	LTBOOL Init(class CServerMgr* pMgr);     // 0x10035145
	// 0x10035152 — is a request for this file already waiting? The queue is
	// searched rather than a flag kept, so asking twice for one model costs
	// a walk and not a second load (name ours).
	LTBOOL IsRequestQueued(void* pFile);
	// 0x100351d9 — slot 0x0c, the only one this thread overrides: slots
	// 0x04 and 0x08 of its vftable (0x1004e910) are LThread's own
	// 0x100158d9 and 0x10015a2b, so ThreadTerm and ThreadRun are inherited.
	void ProcessMessage(LThreadMessage* pMsg);

	class CServerMgr* m_pServerMgr;          // +0x60
};

// Objects CServerMgr::Term releases through vtables; only the slots it
// reaches are declared. CUnknown928Obj's deleting destructor sits at slot 71.
class CUnknown808Obj
{
public:
	virtual void _v00() = 0;
	virtual void Release() = 0;              // +0x04
};

class CUnknown928Obj
{
public:
	virtual void _p00() = 0;
	virtual void _p04() = 0;
	virtual void _p08() = 0;
	virtual void _p0c() = 0;
	virtual void _p10() = 0;
	virtual void _p14() = 0;
	virtual void _p18() = 0;
	virtual void _p1c() = 0;
	virtual void _p20() = 0;
	virtual void _p24() = 0;
	virtual void _p28() = 0;
	virtual void _p2c() = 0;
	virtual void _p30() = 0;
	virtual void _p34() = 0;
	virtual void _p38() = 0;
	virtual void _p3c() = 0;
	virtual void _p40() = 0;
	virtual void _p44() = 0;
	virtual void _p48() = 0;
	virtual void _p4c() = 0;
	virtual void _p50() = 0;
	virtual void _p54() = 0;
	virtual void _p58() = 0;
	virtual void _p5c() = 0;
	virtual void _p60() = 0;
	virtual void _p64() = 0;
	virtual void _p68() = 0;
	virtual void _p6c() = 0;
	virtual void _p70() = 0;
	virtual void _p74() = 0;
	virtual void _p78() = 0;
	virtual void _p7c() = 0;
	virtual void _p80() = 0;
	virtual void _p84() = 0;
	virtual void _p88() = 0;
	virtual void _p8c() = 0;
	virtual void _p90() = 0;
	virtual void _p94() = 0;
	virtual void _p98() = 0;
	virtual void _p9c() = 0;
	virtual void _pa0() = 0;
	virtual void _pa4() = 0;
	virtual void _pa8() = 0;
	virtual void _pac() = 0;
	virtual void _pb0() = 0;
	virtual void _pb4() = 0;
	virtual void _pb8() = 0;
	virtual void _pbc() = 0;
	virtual void _pc0() = 0;
	virtual void _pc4() = 0;
	virtual void _pc8() = 0;
	virtual void _pcc() = 0;
	virtual void _pd0() = 0;
	virtual void _pd4() = 0;
	virtual void _pd8() = 0;
	virtual void _pdc() = 0;
	virtual void _pe0() = 0;
	virtual void _pe4() = 0;
	virtual void _pe8() = 0;
	virtual void _pec() = 0;
	virtual void _pf0() = 0;
	virtual void _pf4() = 0;
	virtual void _pf8() = 0;
	virtual void _pfc() = 0;
	virtual void _p100() = 0;
	virtual void _p104() = 0;
	virtual void _p108() = 0;
	virtual void _p10c() = 0;
	virtual void _p110() = 0;
	virtual void _p114() = 0;
	virtual void _p118() = 0;
	virtual ~CUnknown928Obj();               // slot 71 (+0x11c)

	// 0x0c — a dword, not a slot: 0x10032332 reads it as
	// `mov 0xc(%eax),%eax` straight off the object, with no vtable
	// indirection in front of it (name ours).
	uint8  m_pad_04[0x0c - 0x04];
	uint32 m_n0c;                            // 0x0c
};

// This is Model — commonmodel.h describes the same class from the other side.
// se_LoadModelData allocates 0x2c4 bytes and constructs one here with the
// same two-allocator constructor, then loads a model file into it, which is
// what identifies the two as one class. What the manager keeps at 0xb28 is
// therefore a Model: the placeholder that stands in for one still loading.
class Model;
typedef class Model CUnknownB28Obj;

// The class-module manager embedded at 0x80c (object.lto binding); opaque,
// bounded above by m_pServerShell at 0x820. Its address is also published as
// a global by SetupGlobals.
class CClassInfo;

// One per-class tick/count record (0x4c bytes; the array hangs off the
// manager at 0x828, one per registered class). Names ours, read off
// UpdateObjects' bookkeeping.
// One loaded class: the bank its instances come out of, the class it came
// from, and this frame's profile counters. Term hands the bank back and
// clears the back-reference the class holds to this record.
class CClassStats
{
public:
	uint8       m_pad_00[0x08];
	StructBank  m_Bank;                      // 0x08  instances of this class
	class CClassInfo* m_pClass;              // 0x24  back-pointer; first field is the name
	uint8       m_pad_28[0x2c - 0x28];
	uint32      m_Ticks;                     // 0x2c  profile ticks this frame
	uint32      m_nUpdated;                  // 0x30  objects updated this frame
	uint32      m_nObjects;                  // 0x34  objects of this class in the world
	uint32      m_nFrames;                   // 0x38  frames accumulated; reset at 600
	uint32      m_TotalTicks;                // 0x3c
	uint32      m_MaxTicks;                  // 0x40
	uint32      m_bPrinted;                  // 0x44  scratch for the sorted dump
	uint8       m_pad_48[0x4c - 0x48];
};

// Per-class info reached from SObjData+0x24: the name is the first field and
// the per-server stats records sit at +0x24 (indexed by the manager's 0x834
// slot). Name ours.
class CClassInfo
{
public:
	char*        m_pClassName;               // 0x00
	uint8        m_pad_04[0x24 - 0x04];
	// 0x24: one internal pointer table. Slot 0 is the SDK ClassDef — HCLASS
	// is a CClassInfo*, and the reflection methods (GetNumClassProps,
	// GetClassProp, GetClassName) walk the property chain through it —
	// while CClassMgr::m_nIndex indexes the same table for the
	// per-class tick records, so the stats slot is never zero.
	union {
		struct ClassDef* m_pClassDef;        // 0x24
		CClassStats*     m_Stats[1];         // 0x24 + 4*m_ClassStatsSlot
	};
	uint8        m_pad_28[0x48 - 0x28];
	class LTObject* m_pStaticObject;         // 0x48  si_GetStaticObject's answer;
	                                         //       null for most classes
};

// The engine-internal counter is eight bytes — one QueryPerformanceCounter
// snapshot — not the SDK LTCounter's four dwords (header drift again). The
// constructor (0x10005f38) starts it for modes 1 and 2 and does nothing for
// 0; cnt_StartCounter/cnt_EndCounter (0x10005fe3/0x10005fec) are the C
// wrappers the server interface exposes. Names ours.
class CBaseCounter
{
public:
	CBaseCounter(int nMode);                 // 0x10005f38

	void   Start();                          // 0x10005f5d
	uint32 Read();                           // 0x10005f65  milliseconds
	uint32 ReadRaw();                        // 0x10005fb0  the fixed scale

	uint32 m_Data[2];
};
void   cnt_StartCounter(CBaseCounter* pCounter);
uint32 cnt_EndCounter(CBaseCounter* pCounter);

// Scoped timing: starts a counter on the way in and adds however long the
// block took to a running total on the way out. The engine leaves these in
// release builds — the totals are what the timing console commands report.
// Names ours.
class CountAdder
{
public:
	CountAdder(uint32* pTotal) : m_Counter(0)
	{
		m_pTotal = pTotal;
		cnt_StartCounter(&m_Counter);
	}
	~CountAdder();                           // 0x10001ed7

	CBaseCounter m_Counter;                  // 0x00
	uint32*      m_pTotal;                   // 0x08
};

// 0x10058778 — the counter the net trace timestamps its lines from.
extern CBaseCounter g_NetCounter;

// The loaded class module and everything that came out of it: 0x30 bytes at
// CServerMgr+0x80c, cleared whole by both the constructor and Init, which is
// what fixes the size. The manager's shell pointer, per-class records, class
// hash and module slot all live in here rather than beside it — Term is what
// shows that, reaching every one of them off the same `this` (names ours).
//
// The shell is released through a function pointer the module handed over
// rather than by delete: the object belongs to the game DLL, so the game DLL
// has to be the one to let go of it.
class CClassMgr
{
public:
	CClassMgr();                             // 0x10001791
	LTBOOL Init(class CServerMgr* pMgr);     // 0x100017aa
	void   Term();                           // 0x100017c8
	~CClassMgr();                            // 0x100017a5

	void*  m_pUnknown00;                     // +0x00 (0x80c)
	void*  m_hModule;                        // +0x04 (0x810)  the loaded class
	                                         //        module; FormatString's
	                                         //        resource source
	void*  m_pUnknown08;                     // +0x08 (0x814)
	uint8  m_pad_0c[0x10 - 0x0c];            //       (0x818)
	void   (*m_pDeleteShell)(class IServerShell*);   // +0x10 (0x81c)
	class IServerShell* m_pServerShell;      // +0x14 (0x820)
	uint32 m_Unknown18;                      // +0x18 (0x824)
	class CClassStats* m_pClassStats;        // +0x1c (0x828)  one 0x4c record
	                                         //        per class, each with its
	                                         //        own instance bank at +8
	int    m_nClasses;                       // +0x20 (0x82c)
	void*  m_hClassHash;                     // +0x24 (0x830)  name -> CClassInfo
	uint32 m_nIndex;                         // +0x28 (0x834)  this module's slot
	                                         //        in each class's table
	class CServerMgr* m_pServerMgr;          // +0x2c (0x838)
};

int cb_LoadModule(CClassMgr* pClassMgr);                     // 0x10001894

// A loaded DLL and how many places still want it. Only the last holder
// unloads it, and the record itself always goes back to the engine's
// allocator (names ours).
class CModuleRef
{
public:
	void* m_hModule;                         // 0x00
	int   m_nRefs;                           // 0x04
};

// What Term hands the manager's remaining pieces to (names ours; each takes
// one pointer and cleans up after itself). The two wrappers hold their
// module reference at different offsets, which is the only difference
// between them.
void cb_FreeClassHash(void* p);                              // 0x1000dbe0
void cb_ReleaseModule(CModuleRef* pRef);                     // 0x100015b2
void cb_FreeSoundTable(void* p);                             // 0x10034f85
void cb_FreeModule(void* p);                                 // 0x1000171a

// The engine allocator pair (0x1000d2ef, 0x1000d398).
void* dalloc(unsigned long size);
void  dfree(void* ptr);

// 0x1000deeb — format a message from the resource module (ltmsg.dll).
int dsi_SetupMessage(char* pMsg, int maxLen, int messageCode, va_list marker);

// 0x1002bc5e — mount resource trees onto the file manager.
void fs_AddResources(class CFileMgr* pFileMgr, char** pResources,
                     unsigned long nResources, void** pTrees, int* pnTrees);

// A connected client and the network connection under it.
// A sliding rate window ({value, time} advanced per frame): constructed by
// 0x100263d2, aged by 0x100263e7; value/time is what the stats print (names
// ours).
class CRateTracker
{
public:
	CRateTracker();                          // 0x100263d2
	void Update(float fDelta);               // 0x100263e7
	void Rollover();                         // 0x10026413 (name ours)

	float m_Value;                           // +0x00
	float m_Time;                            // +0x04
	float m_Unknown08;                       // +0x08
};

// A connection's four packet queues are all LMessageList-shaped, and three
// of them are LMessageList; the awaiting-ack queue is a separate class of
// the same shape, which is what the constructor shows — one vtable address
// is loaded once and stored into three of the members, and the fourth is
// stored as its own immediate. Not derived from LMessageList: a derived
// class would construct its base first and put the zeros before the vtable,
// and the shipped order is the other way round (name ours).
// The same twelve-slot queue as LMessageList, and the same two overrides:
// its vtable (0x1004dcfc) names the base's ten bodies and its own GenAppend
// at 0x10023f72 and GenAppendList at 0x10023f87. No virtual destructor --
// slot 0 there is GenBegin.
class CNetSentQueue : public CMessageQueueBase
{
public:
	// Zeroes after the vptr store, as LMessageList() does (0x1002123a).
	CNetSentQueue() { m_nMessages = 0; m_pTail = LTNULL; }

	// 0x10023f72 — byte-for-byte LMessageList's, and a separate function
	// because it is a separate class's member. /OPT:ICF is off, so the
	// linker kept both.
	virtual LTBOOL GenAppend(LThreadMessage*& toAppend)
	{
		Prepend(toAppend);

		return LTTRUE;
	}

	// 0x10023f87
	virtual LTBOOL GenAppendList(const GenList<LThreadMessage*>& other)
	{
		GenListPos pos = other.GenBegin();

		while (other.GenIsValid(pos))
			Prepend(other.GenGetNext(pos));

		return LTTRUE;
	}
};

class NetConnection
{
public:
	NetConnection();                         // 0x10021200
	void Term();                             // 0x1002126a (name ours)

	// 0x1002132a — has the far end gone quiet for too long? Silent while
	// the connection is still coming up (name ours).
	LTBOOL CheckAckTimeout();

	CRateTracker  m_SendBPS;                 // +0x00  the stats print fixes the
	CRateTracker  m_SendPPS;                 // +0x0c  pairings: BPS first in each
	CRateTracker  m_RecvBPS;                 // +0x18  push sequence
	CRateTracker  m_RecvPPS;                 // +0x24
	class CBaseDriver* m_pDriver;            // +0x30
	LMessageList  m_SendQueue;               // +0x34  count printed as "Q Size"
	LMessageList  m_GuaranteedQueue;         // +0x40  arrived out of order
	CNetSentQueue m_SentList;                // +0x4c  awaiting ack; resent on a timer
	LMessageList  m_RecvList;                // +0x58  arrived, not yet handed out
	uint32        m_Unknown64;               // +0x64
	uint32        m_Unknown68;               // +0x68
	uint32        m_Unknown6C;               // +0x6c
	uint32        m_Flags;                   // +0x70  bit 0 = ?, bit 2 = kill me
	uint32        m_RemoteAckID;             // +0x74
	uint8         m_pad_78[0x7c - 0x78];
	uint32        m_PacketID;                // +0x7c
	uint8         m_pad_80[0x84 - 0x80];
	float         m_RecvWait;                // +0x84  doubles as the timeout clock
	float         m_SendWait;                // +0x88
	uint8         m_pad_8c[0x94 - 0x8c];
	float         m_AckTimer;                // +0x94
	float         m_Unknown98;               // +0x98
	float         m_Ping;                    // 0x9c
	uint8         m_pad_a0[0xac - 0xa0];
	CBaseCounter  m_Counter;                 // +0xac  constructed with mode 0
	float         m_UnknownB4;               // +0xb4  counts down per frame
};

// Each client keeps a 2x2 set of packet slots at +0x134; EndWorld rewinds
// every packet in them to one byte.
class ClientPacketSet
{
public:
	// Client's constructor runs this over all four slots -- a four-trip loop
	// zeroing the first dword of each 12-byte record, emitted between the
	// rate governor's constructor and the message list's, which is exactly
	// where declaration order puts it and nowhere the body could be.
	ClientPacketSet() { m_pPacket = LTNULL; }

	class CPacket* m_pPacket;                // +0x00
	uint32         m_Unknown04;
	uint32         m_Unknown08;
};

// One entry in a client's per-object table (dword stride at Client+0x190):
// the low word collects the change flags the client still has to hear about.
class CObjInfo
{
public:
	uint16 m_ChangeFlags;                    // +0x00
	uint8  m_Unknown02;                      // +0x02  cleared at world entry
	uint8  m_Unknown03;                      // +0x03
};

// The per-client send-rate governor at Client+0x118 (names ours): Update
// says whether it is time to send, Get/SetRate carry the smoothed rate.
class CRateController
{
public:
	// 0x10015f57 — a cold counter and no rate, so a controller nobody has
	// set a rate on never fires. Declared here now rather than on a view in
	// counter.cpp, because Client's own constructor CALLS it and a view
	// cannot make that call appear.
	CRateController();
	LTBOOL Update();                         // 0x10015f87
	float  GetRate();                        // 0x10015f83
	void   SetRate(float fRate);             // 0x10015f6f

	CBaseCounter m_Counter;                  // +0x00  when the window opened
	float        m_Rate;                     // +0x08  hertz; zero means never
	float        m_Accum;                    // +0x0c  the remainder carried on
};

// One of the two ping-pong sent-object id lists at Client+0x194.
class ClientIDList
{
public:
	uint16  m_nIDs;                          // +0x00
	uint16  m_nMaxIDs;                       // +0x02  grown 200 at a time
	uint16* m_pIDs;                          // +0x04
};

// The internal client flag word at 0x3c0. Three of the public CIF_ bits sit
// at the same value and three do not, which is why the info-flag accessors
// translate one bit at a time (names ours).
#define CLIENTFLAG_LOCAL            0x0002
#define CLIENTFLAG_FULLRES          0x0004
#define CLIENTFLAG_SENDCOBJROTATION 0x0008
#define CLIENTFLAG_FORCENEXTUPDATE  0x0010
#define CLIENTFLAG_PLAYBACK         0x0040
#define CLIENTFLAG_AUTOACTIVATE     0x0200
#define CLIENTFLAG_SKYCHANGED       0x0400

#define MAX_CLIENTCOMMANDS 0xff

class Client
{
public:
	// 0x100270d4 — sixty bytes, and every one of them is member
	// initialisation: the client data pointer nulled, the rate governor
	// constructed, all four packet slots cleared and the queued-message list
	// tied off. The body is empty.
	Client();

	LTLink          m_Link;                  // 0x000  ties it into m_ClientList;
	                                         //        m_pData points back here
	uint8           m_pad_00c[0x10c - 0x00c];
	uint32          m_Unknown10C;            // 0x10c  zeroed at handshake
	void*           m_pClientData;           // 0x110  handed to OnClientEnterWorld
	uint32          m_ClientDataLen;         // 0x114
	CRateController m_RateControl;           // 0x118  the send-rate governor
	float           m_Unknown128;            // 0x128  rate scale
	uint8           m_pad_12c[0x134 - 0x12c];
	ClientPacketSet m_PacketSets[2][2];      // 0x134
	uint8           m_pad_164[0x170 - 0x164];
	Client*         m_pViewClient;           // 0x170  whose eyes this client uses;
	                                         //        set skips the first update send
	LTVector        m_Position;              // 0x174  the update gather centers here
	uint8           m_pad_180[0x18c - 0x180];
	void*           m_pUnknown18C;           // 0x18c  unhandled packets go here
	CObjInfo*       m_pObjInfos;             // 0x190  indexed by object id
	ClientIDList    m_IDLists[2];            // 0x194  ping-pong sent-object id lists
	int             m_CurIDList;             // 0x1a4  which of the two is current
	uint8           m_CommandStates[2][MAX_CLIENTCOMMANDS];  // 0x1a8  double-buffered
	uint8           m_pad_3a6[0x3a8 - 0x3a6];
	uint32          m_nCurCommands;          // 0x3a8  which page is live

	LTObject*       m_pUnknown3AC;           // 0x3ac  detached by sm_EndWorldC
	void*           m_pUserData;             // 0x3b0  the game's, untouched
	uint16         m_ClientID;               // 0x3b4
	uint8          m_pad_3b6[0x3b8 - 0x3b6];
	int            m_State;                  // 0x3b8  1 and 2 get extra work in sm_UpdateClient;
	                                         //        3 receives change flags
	int            m_Unknown3BC;             // 0x3bc  handshake stage: 1 before the
	                                         //        file info goes out, then 2
	uint32         m_ClientFlags;            // 0x3c0  0x20 = the sky changed;
	                                         //        2 = resend the handshake
	char*          m_pName;                  // 0x3c4
	NetConnection* m_pConnection;            // 0x3c8
	LTList         m_List3CC;                // 0x3cc  queued messages, flushed into
	                                         //        each update packet
	// 0x3dc — the client's own sound hash, which smgr.cpp:8536 already
	// describes as "the client's own at +0x3dc" without the class having a
	// member for it. sm_AddClientForConnection is what pins it: the shipped
	// `new Client` pushes 0x3e0 (lithtech 0x00463797) where the class as
	// written spans only 0x3dc, and 0x00463a29 fills exactly these four
	// bytes with dhash_Create(500, 0).
	void*          m_hSoundHash;             // 0x3dc
};

// The console entry points the interface forwards to — all __cdecl.
LTRESULT c_CommandHandler(ConsoleState* pState, char* pStr);
HCONSOLEVAR cc_FindConsoleVar(ConsoleState* pState, char* pName);
void cc_RunCommand(ConsoleState* pState, char* pCommand, int nSource);
LTBOOL cc_LoadConfigFile(ConsoleState* pState, char* pFilename, LTBOOL bJustVars, LTBOOL bCreate);
LTBOOL cc_SaveConfigFile(ConsoleState* pState, char* pFilename);


// What the world loader is handed. Zeroed by its own inline constructor and
// then once more by LoadWorld's explicit memset — the shipped bytes contain
// both. The hook slot holds a no-op function; /OPT:ICF folded every empty
// function in the image into one body (shared with dsi_OnReturnError), so
// which no-op the source named is unrecoverable.
class WorldLoadRequest
{
public:
	WorldLoadRequest()
	{
		m_pStream = 0;
		m_pUserData = 0;
		m_LoadHook = 0;
		m_Unknown0c = 0;
	}

	ILTStream* m_pStream;                    // +0x00
	void*      m_pUserData;                  // +0x04
	void     (*m_LoadHook)(int);             // +0x08
	uint32     m_Unknown0c;                  // +0x0c
};

// The server's world, embedded at 0x1a4. Eight bytes — the light-animation
// array at 0x1ac bounds it — so it is a handle onto state owned elsewhere.
class CServerWorld
{
public:
	CServerWorld();                          // 0x10007f0e
	// 0x1002c9e3 — nine sub-objects. commonmodel.cpp carries the wide view
	// of this object as CServerWorldFull; here it is only the call.
	~CServerWorld();
	WorldPoly* FindPoly(HPOLY hPoly);        // 0x1002dad1
	void       Term();                       // 0x10009be5
	void       TermObjects();                // 0x10009d1e (name ours)
	int        Load(WorldLoadRequest* pRequest);   // 0x1000804e
	int        LoadObjects(ILTStream* pStream);    // 0x10009f82
	// 0x10009e31 — find the named world model in the loaded level and hang
	// its geometry on the object. The name is matched against the world's
	// own list, so a world model is not a file of its own: it came in with
	// the level (name ours).
	int        AttachWorldModel(class LTObject* pObj, char* pName);

public:
	uint8 m_pad_00[0x08];
};

// The network manager embedded in the server at 0x6e0 (the name is attested
// by a check literal elsewhere in the image). Sized by its neighbours: the
// driver array pointer follows at 0x720.
// The bank the net packet records live in (0x70-byte records, 32 deep):
// the same vptr-over-StructBank shape as CMessageBank (names ours).
// CNetBank's destructor restores TWO vftables -- its own and then
// 0x1004d260, which is the same one the eleven object banks and the message
// bank restore -- so it shares CBankBase above, and the three slots after
// the destructor are that base's.
class CNetBank : public CBankBase
{
public:
	CNetBank()
	{
		sb_Init(&m_Bank, 0x70, 0x20);
	}
	// Inline: CNetMgr's destructor expands it.
	virtual ~CNetBank() { sb_FreeChunks(&m_Bank, 1); }

	// Slot 2, and it is a FORWARDER, not the work. The two images disagree
	// about that slot and the disagreement is the whole shape:
	//
	//   server.dll  vftable 0x1004dd8c = { 0x10024291 ??_G,
	//                                      0x10023fd6 = `jmp 0x10024162`,
	//                                      0x10023fdb = `push 0x4(esp) ;
	//                                                    call 0x10024226 ;
	//                                                    ret $4`,
	//                                      0x10024242 Term }
	//   lithtech.exe vftable 0x004a2b30 = { 0x0045a120 ??_G, 0x00459c30,
	//                                       0x00459dc0, 0x0045a000 }
	//
	// 0x10024162 is Allocate and 0x10024226 is Free, both of which this
	// object already matches. So the shipped class is a pair of one-line
	// virtual wrappers over two non-virtual workers, and /Os keeps the
	// wrappers as the 5- and 12-byte thunks above while /Ot expands the
	// worker into each of them (0x00459c30 is Allocate with ServerMessage's
	// constructor inlined; 0x00459dc0 is Free with the record teardown
	// inlined, 49 bytes). Declaring _v08 here is what gives netmgr.cpp
	// somewhere to put 0x10023fdb; it changes no existing byte, because the
	// only thing it moves is which symbol slot 2 of the vftable relocates
	// to, and relocation bytes are masked.
	//
	// _v04 is deliberately NOT declared: its /O1 body is a bare 5-byte
	// `jmp`, which has one unmasked byte and would be claimed by shape
	// against the first same-sized jump thunk in the image -- exactly the
	// coincidental match a masked shape comparison makes. See netmgr.cpp.
	virtual void _v08(void* pRec);           // 0x10023fdb

	void Free(void* pRec);                   // 0x10024226

	StructBank m_Bank;                       // +0x04
	uint32     m_Unknown20;                  // +0x20
};

// The one allocator instance the manager's collections are handed. It is
// stdlith's `LAlloc` (collection.h) and `g_MoAlloc` is `g_DefAlloc` -- the
// same object at 0x10058c7c -- but worldbsp.h, worldmodels.cpp, smgr.cpp
// and tcpdriver.cpp still spell it this way, so the old name stays declared
// here until each of them is converted in turn.
class CMoAllocator
{
public:
	virtual ~CMoAllocator();
	virtual void* Alloc(uint32 nBytes);      // slot 4
	virtual void  Free(void* pMem);          // slot 8
};

extern CMoAllocator g_MoAlloc;               // 0x10058c7c

// The two drivers the manager knows how to build (names ours).
class CBaseDriver* ldrv_Create();            // 0x10014d65
class CBaseDriver* tcp_Create();             // 0x10037c32

// The manager's connection and driver lists are stdlith's growable array --
// `CMoArray<T, DefaultCache>`, collection.h -- instantiated twice, once per
// element type. They were hand-rolled here once; what replaced
// them is a typedef apiece, and every fact below is from the shipped bytes.
//
//  * Layout. `CMoArray<T, DefaultCache>` is 0x14 bytes -- the `GenList<T>`
//    vptr, m_pArray, m_nElements and `DefaultCache`'s {m_CacheSize,
//    m_WantedCache} -- in the order the hand-rolled class guessed. Its
//    m_nSlack IS m_CacheSize and its m_nGrowBy IS m_WantedCache.
//
//  * The vtables. server.dll 0x1004dd9c (connections) and 0x1004ddcc
//    (drivers) are TWELVE slots, `GenList<T>`'s twelve pure virtuals in
//    declaration order, with NO destructor slot -- GenList has none, so the
//    `virtual ~CConnList()` this file used to carry was the wrong shape and
//    the two `??_G` deleting destructors it dragged in were never in the
//    image. In lithtech.exe the same two vtables (0x004a2b00, 0x004a2ad0)
//    name the SAME twelve addresses as each other while staying two
//    distinct tables: identical code folded, identical data not. That is
//    two instantiations of one template, not one class used twice, and not
//    a derivation -- see the constructor below. (server.dll's pair differs
//    in slot 9 only, where GenAppendList has an unfolded second copy at
//    0x10024009 whose 142 bytes are byte-identical to 0x1000b207 but for
//    the two call displacements. That is the link, not the source.)
//
//  * The constructor settles derivation-versus-instantiation on its own.
//    0x10023fe7 and 0x10024097 (34 bytes each) store ONE vftable, then
//    `Clear()`'s four zeroes, then `call Init`; lithtech.exe's CNetMgr
//    constructor at 0x00455310 expands the whole thing and still stores one
//    vftable, at the TOP, ahead of Clear's stores at +4/+8/+0xc/+0x10, the
//    first `SetSize2(0, g_DefAlloc)` (Init's `Term()`), `_InitArray`'s
//    +4/+8/+0x10/+0xc and the second `SetSize2` (Init's `SetSize(0)`).
//    A base class with a constructor would put ITS vftable store first and
//    the derived one last, and the store would survive the intervening call
//    -- which is exactly what CNetBank/CBankBase do at 0x10021377 and
//    0x10021449. So neither list derives from anything with a constructor;
//    both are `CMoArray<T, DefaultCache>` itself.
//
//  * The destructor says the same thing from the other side: ~CNetMgr
//    (0x100213ee) expands each list as one vftable store and one
//    `SetSize2(0, &g_DefAlloc)` -- `~CMoArray() { Term(); }` -- where a
//    derived class over a destructor-bearing base leaves two stores, as
//    CNetBank does at 0x10021449/0x10021456.
//
//  * The bodies. 0x1000c38c is `Init(size, cacheSize)` (it returns BOOL,
//    which the old `void Init(int,int)` did not); 0x1000c470/0x1000be59 is
//    `SetSize2`; 0x1002431f is `Insert2` and 0x1000cd8e `Remove2` -- both
//    of which the hand-rolled InsertAt/RemoveAt could only get NEAR and
//    collection.cpp already matches exactly; 0x100243de is
//    `_AllocateTArray` and 0x1000d020 `_DeleteAndDestroyArray`, the old
//    AllocItems/FreeItems, with `BaseNew`/`BaseDelete` inlined into them.
//
// Why the change had to happen here rather than in netmgr.cpp: a non-
// template member declared out-of-class is the one construct /Ob1 never
// expands, so the hand-rolled spelling could match server.dll forever and
// could never match lithtech.exe, where the whole body is expanded into its
// callers (0x004561e0, RemoveDriver, carries a complete inlined Remove2 and
// the only call it has left is the leaf `BaseNew<T>` at 0x004131d0).
//
// The element types are the lists' own, not `void*`: two instantiations are
// what the two vtables need, and typing them keeps every call site's casts
// off. m_pItems/m_nItems are now collection.h's m_pArray/m_nElements, which
// renames two reads in each of servermgr.cpp (MyServerInterface::SendTo,
// around line 362) and servermessage.cpp (around line 1209) -- the only
// places outside netmgr.cpp that walk either list. Nothing else changes in
// them: the element type is still CBaseDriver*, so no cast is needed.
class CBaseDriver;

typedef CMoArray<NetConnection*> CConnList;    // vtable 0x1004dd9c
typedef CMoArray<CBaseDriver*>   CDriverList;  // vtable 0x1004ddcc

// The two shapes a packet record from CNetMgr's bank is used in (names
// ours): received payloads carry a pointer and length; sent-awaiting-ack
// records age a resend timer over an inline payload.
class CNetRecvPacket
{
public:
	void Term();                             // 0x100242c1 (name ours)

	uint32          m_Unknown00;
	CNetRecvPacket* m_pNext;                 // +0x04
	uint8*          m_pData;                 // +0x08
	int             m_nAlloced;              // +0x0c  what the payload cost
	uint32          m_Len;                   // +0x10
	uint32          m_Unknown14;
	uint32          m_ID;                    // +0x18  the guaranteed sequence
};

class CNetSentPacket
{
public:
	uint32          m_Unknown00;
	CNetSentPacket* m_pNext;                 // +0x04
	float           m_ResendTimer;           // +0x08
	uint8           m_Data0C[0x70 - 0x0c];   // +0x0c  the wire payload
};

// The manager's two message queues. The first is an LMessageList: CNetMgr's
// constructor stores 0x1004dd2c into +0xb8 (0x1002141c), and 0x1004dd2c is
// ??_7LMessageList@@6B@ (NetConnection's constructor stores it too, see
// netmgr.cpp). So the class derives from LMessageList and adds nothing
// virtual -- it used to be a standalone class with a VIRTUAL destructor,
// whose `??_G` the shipped image does not have (slot 0 of every queue
// vtable is GenBegin). It exists as a class of its own only for its
// destructor: ~CNetMgr expands it as the vptr restore and the two clears,
// m_pTail before m_nMessages, which LMessageList itself must not have (it
// stays trivially destructible; see LThreadMsgList). The constructor repeats
// the zeros, __forceinline, for the reason CNetQueueB's does below.
class CNetQueueA : public LMessageList
{
public:
	__forceinline CNetQueueA() { m_nMessages = 0; m_pTail = LTNULL; }

	~CNetQueueA() { m_pTail = LTNULL; m_nMessages = 0; }
};

// Its vtable (0x1004dd5c) names CNetSentQueue's GenAppendList at 0x10023f87
// but its own GenAppend at 0x100240db, so the class derives from that queue
// and overrides the one slot -- the way LThreadMsgList sits under
// LMessageList. The constructor repeats the zeros because the shipped one
// (0x100213b6) stores this class's vptr FIRST and the zeros after it: the
// base's vptr and zeros are dead-store-eliminated. __forceinline because
// plain in-class inline is declined at /O1 once the base constructor is
// expanded into it (r22 measured a 17-byte out-of-line ??0CNetQueueB and
// CNetMgr() near); with it CNetMgr() stays identical on both images and
// 0x100240db is credited. The destructor is not virtual (slot 0 of the
// vtable is GenBegin) and ~CNetMgr expands it: vptr, m_pTail, m_nMessages.
class CNetQueueB : public CNetSentQueue
{
public:
	__forceinline CNetQueueB() { m_nMessages = 0; m_pTail = LTNULL; }

	// 0x100240db
	virtual LTBOOL GenAppend(LThreadMessage*& toAppend)
	{
		Prepend(toAppend);

		return LTTRUE;
	}

	// Inline, and NOT empty: the manager's destructor expands it and the
	// expansion clears both fields, m_pTail before m_nMessages.
	~CNetQueueB() { m_pTail = LTNULL; m_nMessages = 0; }
};

// What the manager's second queue holds: four polymorphic pieces it owns
// outright, the packet id it is waiting on, and the connection it belongs
// to. Term lets the four pieces go; the record itself is freed by whoever
// takes it off the queue (names ours).
// Each piece is released through slot 8 of its own vtable; nothing else
// about it is reached from here (name ours).
class CNetPiece
{
public:
	virtual void _v00() = 0;
	virtual void _v04() = 0;
	virtual void Release() = 0;
};

class CNetPending
{
public:
	void Term();                             // 0x10023569

	// Two link words, not a full LTLink: the queue walks prev and next and
	// never asks a record for a data pointer.
	class LThreadMessage* m_pNext;           // 0x00
	class LThreadMessage* m_pPrev;           // 0x04
	class CNetPiece* m_pPieces[4];           // 0x08, 0x0c, 0x10, 0x14
	uint32          m_PacketID;              // 0x18
	class NetConnection* m_pConnection;      // 0x1c
};

class CNetMgr
{
public:
	CNetMgr();                                           // 0x1002136b
	// 0x100213ee — the body is Term() and nothing else; the five members
	// below are handed back by the compiler, in reverse declaration order.
	~CNetMgr();
	LTRESULT Init();                                     // 0x100214ea
	void     Term();                                     // 0x100214ba
	LTBOOL   SetNetName(char* pName);                    // 0x10021462 (name ours)
	LTRESULT GetServiceList(NetService*& pListHead);     // 0x10021531
	LTRESULT FreeServiceList(NetService* pListHead);     // 0x1002155d
	// The session list is the client's half of the same interface: shared
	// code, but only lithtech.exe calls it, so the addresses given are
	// lithtech.exe's rather than server.dll's.
	LTRESULT GetSessionList(NetSession*& pListHead, char* pInfo);  // 0x004556b0
	LTRESULT FreeSessionList(NetSession* pListHead);               // 0x004556e0
	LTRESULT SelectService(HNETSERVICE hNetService);     // 0x1002157f
	LTRESULT GetSessionName(char* sName, uint32 dwBufferSize);  // 0x100215ce
	LTRESULT UpdateSessionName(const char* sName);       // 0x100215f3
	LTRESULT GetTcpIpAddress(char* sAddress, uint32 dwBufferSize,
	                         unsigned short& hostPort);  // 0x1002160e
	void     Disconnect(NetConnection* pConn, uint32 nReason, uint32 flags);  // 0x10021c18
	void     Update(char* pTraceName, float fCurTime, int bAllowTimeout);      // 0x10021637 (name ours)
	void     StartGettingPackets();                                             // 0x10021cb6 (name ours)
	void     EndGettingPackets();                                               // 0x10021cba (name ours)
	LTBOOL   GetPacket(class ServerMessage* pMsg);                              // 0x100227c5 (name ours)

	// Internals (names ours, from what they do).
	void  TermDrivers();                                              // 0x1002150e
	void  RemoveDriver(class CBaseDriver* pDriver);                   // 0x10021bbc (name ours)
	class CBaseDriver* AddDriver(char* pDriverName);                  // 0x10021b39
	class CBaseDriver* FindDriver(char* pDriverName);                 // 0x00459b00 (client)
	void  TermQueue(LMessageList* pQueue);                            // 0x10023cbe
	void  TermConnections();                                          // 0x10023586
	void  RemoveConnection(NetConnection* pConn);                     // 0x100235a3
	// The app guid the drivers advertise; a null argument clears it.
	void  SetGuid(LTGUID* pGuid);                                     // 0x10023d93
	void  AddRecvBytes(NetConnection* pConn, uint32 nBytes);          // 0x10022a4c
	void  ReleaseQueued(class CNetPending* pRec);                     // 0x10023542 (name ours)
	LTBOOL AddConnection(NetConnection* pConn);                       // 0x10022ade (name ours)
	void  CloseConnection(NetConnection* pConn);                      // 0x10022bc8 (name ours)
	int   TranslatePacket(class ServerMessage* pMsg, NetConnection* pConn,
	                      int bRaw);                                  // 0x1002307f
	// A received packet goes back into the connection's guaranteed queue
	// in sequence order, so the resolve loop below can pull them out in
	// order; the walk gives up at the first record that belongs after it.
	void  FreeRecvPacket(CNetRecvPacket* pRec);                       // 0x10023bb5
	// The queue is ordered by packet ID, so the walk gives up as soon as it
	// passes the one asked for (name ours).
	void* FindQueuedPacket(LMessageList* pList, uint32 nID);          // 0x10023c1f
	void  SendPacket(class ServerMessage* pMsg, NetConnection* pConn, int n);  // 0x10022990
	void  SendPacketData(void* pData, NetConnection* pConn);          // 0x10022d6c
	void  WriteAckPacket(NetConnection* pConn, class ServerMessage* pMsg);     // 0x100219db
	// Varargs, so cdecl with `this` as a hidden stack argument (like
	// CServerMgr::SetupError).
	void  DebugPrint(NetConnection* pConn, int nLevel, char* pFormat, ...);    // 0x10023e1b

public:
	uint32        m_Flags;                   // +0x00  bit 0 = getting packets
	CNetBank      m_PacketBank;              // +0x04
	CConnList     m_Connections;             // +0x28
	CDriverList   m_Drivers;                 // +0x3c
	char          m_Name[100];               // +0x50
	class CBaseMgr* m_pHandler;              // +0xb4  the CServerMgr; told through
	                                         //        its vtable when a connection dies
	CNetQueueA    m_QueueB8;                 // +0xb8
	CNetQueueB    m_QueueC4;                 // +0xc4
	char*         m_pTraceName;              // +0xd0  "Server: " while updating
	float         m_FrameTime;               // +0xd4
	uint32        m_UnknownD8;               // +0xd8
	uint32        m_UnknownDC;               // +0xdc
	CRateTracker  m_RateE0;                  // +0xe0
	float         m_LastTime;                // +0xec
	LTGUID        m_Guid;                    // +0xf0  the app guid CreateServer copies in
	class CBaseDriver* m_pMainDriver;        // +0x100
};

// A network driver. Two vtable slots and one field are reached from the
// interface wrappers; bit 0 of the flags marks the tcp/ip driver.
#define NETDRIVER_TCPIP 1

// The internal record an HNETSERVICE names: its owning driver is the one
// field CNetMgr reaches (name ours).
class CNetService
{
public:
	uint32             m_Unknown00;
	class CBaseDriver* m_pDriver;            // +0x04
};

class CBaseDriver
{
public:
	// Slot 0 is the scalar-deleting destructor, which is what AddDriver and
	// RemoveDriver reach through `delete`; slot 4 is the driver's own
	// initialization, and a driver that answers false there is destroyed
	// rather than kept (names ours).
	virtual ~CBaseDriver()=0;
	virtual LTBOOL Init()=0;
	virtual void Term()=0;                               // 0x08  (name ours)
	virtual void Update()=0;                             // 0x0c  once per CNetMgr::Update
	virtual void _p10()=0;
	virtual void GetServiceList(NetService** ppList)=0;  // 0x14
	virtual int  SelectService(HNETSERVICE hService)=0;  // 0x18  nonzero = failed
	virtual void _p1c()=0; virtual void _p20()=0;
	virtual void _p24()=0; virtual void _p28()=0; virtual void _p2c()=0;
	virtual LTRESULT HostSession(NetHost* pHostInfo)=0;  // 0x30
	virtual void _p34()=0;
	virtual LTRESULT UpdateSessionName(const char* sName)=0;  // 0x38
	virtual LTRESULT GetSessionName(char* sName, uint32 dwBufferSize)=0;  // 0x3c
	// 0x40 (name ours): whether the driver was started by a lobby launcher,
	// which is the whole of ILTClient::IsLobbyLaunched.
	virtual LTBOOL IsLobbyLaunched()=0;
	// 0x44 (name ours): the lobby launcher's data block, if the driver was
	// started by one; ILTClient::GetLobbyLaunchInfo is nothing but this.
	virtual LTBOOL GetLobbyLaunchInfo(void** ppData)=0;
	virtual void _p48()=0;
	virtual void _p4c()=0;
	virtual LTRESULT GetTcpIpAddress(char* sAddress, uint32 dwBufferSize,
	                                 unsigned short& hostPort)=0;  // 0x50
	virtual void _p54()=0;
	virtual LTRESULT SendTo(const void* pData, uint32 len,
	                        const char* sAddr, uint32 port)=0;  // 0x58
	virtual void Disconnect(NetConnection* pConn, uint32 nReason,
	                        uint32 flags)=0;             // 0x5c
	virtual void _p60()=0;
	virtual LTBOOL ReceivePacket(class ServerMessage* pMsg)=0;  // 0x64
	virtual uint32 GetRecvSize()=0;                      // 0x68

public:
	uint8  m_pad_04[0x44 - 0x04];
	uint32 m_DriverFlags;                    // 0x44
	class CNetMgr* m_pNetMgr;                // 0x48  the manager that owns the
	                                         //       driver. TCPDriver reads the
	                                         //       app GUID out of it, which is
	                                         //       what identified the field —
	                                         //       CLocalDriver's use had read
	                                         //       as a loopback pipe.
	float  m_Timeout;                        // 0x4c  idle seconds before the kill
};

// One property of the object currently being created, as the world file
// spelled it. The readers all reach the same three fields, which is what
// gives the layout: a PT_ type code, an inline name, the next link, and the
// value (names ours).
class ObjectProp
{
public:
	uint32      m_Type;                      // 0x00  PT_*
	char        m_Name[0x54 - 0x04];         // 0x04  inline, not a pointer
	ObjectProp* m_pNext;                     // 0x54
	union {                                  // 0x58
		char  m_String[0xc0 - 0x58];         // the record is 0xc0 bytes, which is
		float m_Float;                       // where CreateObjectProps' cap on the
		char  m_Bool;                        // value comes from
		float m_Vec[3];                      // also the euler angles of PT_ROTATION
	};
};

// The GenericProp fillers (names ours). ltbasedefs.h defines the record
// itself, and the clear confirms it: the shipped code zeroes offsets 0/4/8
// and 0x11c/0x120/0x124, which is m_Vec and m_Rotation exactly where the
// header puts them.
void gp_Clear(struct GenericProp* pProp);                          // 0x10011172
void gp_SetString(struct GenericProp* pProp, char* pStr);          // 0x100111b7
void gp_SetVector(struct GenericProp* pProp, LTVector* pVec);      // 0x1001126d
void gp_SetFloat(struct GenericProp* pProp, float fValue);         // 0x100112ba
void gp_SetRotation(struct GenericProp* pProp, LTVector* pAngles); // 0x100112d6

// 0x1001186d — build a rotation from three euler angles (name ours; the
// rotation is the last argument, not the first).
void rot_SetupEuler(float x, float y, float z, LTRotation* pRot);

// The game console's state at 0xa74. Only the three entry points the
// interface forwards to are reached (names ours).
void  con_RunString(class ConsoleState* pState, char* pString);              // 0x100055f9
void  con_SetVar(class ConsoleState* pState, char* pName, char* pVal);       // 0x100059ec

// The console's own internals (names ours).
LTBOOL          con_ICompare(char* p1, char* p2);                            // 0x10005851
char*           con_InternString(ConsoleState* pState, char* pStr);          // 0x10005892
LTCommandVar*   con_CreateVar(ConsoleState* pState, char* pName);            // 0x100058d9
void            con_ApplyBinding(ConsoleState* pState, LTCommandVar* pVar);  // 0x1000595c
ConsoleBinding* con_FindBinding(ConsoleState* pState, char* pName);          // 0x100059ad
void            con_Init(ConsoleState* pState);                              // 0x10005a6b
void            con_Term(ConsoleState* pState);                              // 0x10005ac4
ConsoleCommand* con_AddCommand(ConsoleState* pState, char* pName, void* pFn,
                               void* pUser);                                 // 0x10005b91
void            con_RemoveCommand(ConsoleState* pState, ConsoleCommand* pCmd);  // 0x10005bd0

// 0x1000e4fd and 0x10000e503: the engine's own table of bindings and its
// length, which the console picks up at startup (names ours).
ConsoleBinding* dsi_GetConsoleBindings();
int             dsi_GetNumConsoleBindings();

// 0x10053d80 — the eight commands the console starts with (name ours).
extern ConsoleCommandDef g_ConsoleCommands[];

// 0x10028e97 and 0x10028f42: the two notifications the server hangs off
// console-variable creation and variable change (names ours).
//
// The allocator the console variables come from is at 0x1000d25d, and that
// address is NOT a console function: it is the eleven-byte `operator new`
// forwarder in front of dalloc at 0x1000d2ef. /OPT:ICF is off in this image
// -- the two byte-identical 76-byte SetSize2 bodies at 0x10019e14 and
// 0x1001a03d are both present -- so there is exactly one function there and
// it already has a name.
//
// The name below is kept anyway, and this is the reason: the console stores
// the address in a `void* (*)(uint32)` field, and VC6 will not let the
// address of an overloaded `operator new` be taken or cast to it (C2440).
// So `con_Alloc` is a stand-in the source can spell, not a claim that such a
// function exists -- and because the store is a masked relocation, no
// comparison can tell the two spellings apart. A call-target check catches
// the equivalent mistake at CALL sites; a function POINTER stored to data is
// still outside every oracle we have.
void* con_Alloc(uint32 nSize);            // really 0x1000d25d = operator new
void  cc_OnVarCreated(ConsoleState* pState, LTCommandVar* pVar);
void  cc_OnVarChanged(ConsoleState* pState, LTCommandVar* pVar);
HCONVAR con_FindVar(class ConsoleState* pState, char* pName);                // 0x10005bf8

// The bank the file records come from: the same vptr-over-StructBank shape
// as CMessageBank and CNetBank (names ours).
class CFileBank : public CBankBase
{
public:
	CFileBank()
	{
		sb_Init(&m_Bank, 0x14, 0x20);
	}
	// Inline: CFileMgr's destructor expands it and CServerMgr's expands
	// that, which is why the teardown reaches CServerMgr+0xa44 directly.
	virtual ~CFileBank() { sb_FreeChunks(&m_Bank, 1); }

	StructBank m_Bank;                       // +0x04
};

// The file manager embedded at 0xa2c, opaque from here; these are the
// entry points the interface forwards to, all cdecl.
class CFileMgr
{
public:
	CFileMgr();                              // 0x1002cb99

	// Inline and empty: CServerMgr's destructor expands it and the only
	// thing it expands to is the bank at +0x18 going back.
	~CFileMgr() {}

	LTLink      m_Trees;                     // 0x00  the open resource trees
	uint32      m_nFiles;                    // 0x0c
	void*       m_hFileHash;                 // 0x10  filename -> file record
	CServerMgr* m_pServerMgr;                // 0x14

	CFileBank   m_Bank;                      // 0x18  0x14-byte file records
	uint8       m_pad_38[0x3c - 0x38];
};

void       fs_Term(CFileMgr* pFileMgr);                           // 0x1002bc12
FileEntry* fs_GetFileList(CFileMgr* pFileMgr, char* pDirName);    // 0x1002be41
ILTStream* fs_OpenFile(CFileMgr* pFileMgr, char* pFilename);      // 0x1002bcd9
void       fs_FreeFileList(FileEntry* pList);                     // 0x10013899
char*      fs_GetFileName(CFileMgr* pFileMgr, void* pFile);       // 0x1002bfcd (name ours)

// 0x10005dc6 — the tokenizer ILTServer::Parse exposes (name ours).
int parse_Command(char* pCommand, char** pNewCommandPos, char* argBuffer,
                  char** argPointers, int* nArgs);

// The console's own entry points onto it (names ours). con_RunString and its
// three- and four-argument forms all reach the same runner; con_RunFile
// feeds it a file a line at a time.
void   con_RunStringEx(ConsoleState* pState, char* pString, int nSource, int n);  // 0x10005623
void   con_RunString3(ConsoleState* pState, char* pString, int nSource);          // 0x1000560c
LTBOOL con_RunFile(ConsoleState* pState, char* pFilename, int nSource, int n);    // 0x10005c25
LTBOOL con_SaveFile(ConsoleState* pState, char* pFilename);                       // 0x10005c95
void   con_PrintVar(ConsoleState* pState, LTCommandVar* pVar);                    // 0x10005d1e

// The engine's command tokenizer, used here to walk the caller's address
// list. What pins the layout is the call the shipped Next makes: the same
// five arguments ILTServer::Parse takes (pCommand, pNewCommandPos, argBuffer,
// argPointers, nArgs), with the object itself as argPointers — so the
// pointers open the object, the count sits after them, and the buffer runs
// to the end (names ours).
class CommandParser
{
public:
	int Next();                              // 0x10005d3d

	// 0x10005d74 — walk on until a command whose first argument is the given
	// name turns up. A zero minimum still means one: a command with no
	// arguments at all has nothing to match against (name ours).
	// The flag is CASE SENSITIVE, not ignore-case; see console.cpp.
	LTBOOL Find(char* pName, LTBOOL bCaseSensitive, int nMinArgs);

	char* m_ArgPointers[64];                 // 0x0000
	int   m_nArgs;                           // 0x0100
	char* m_pPos;                            // 0x0104
	char  m_ArgBuffer[0x1400];               // 0x0108
};


// 0x10035a8d — open a file by name and wrap the handle in the 12-byte
// ILTStream the interface hands out (name ours; the shipped body is fopen
// plus a three-field allocation).
ILTStream* streamsim_Open(char* pFilename, char* pAccess);

// The object save/restore pair (0x1000f93a, 0x1001027c) lives in a module we
// have not reached yet; ILTServer::SaveObjects/RestoreObjects are the thin
// wrappers that open the stream for it. Prefix ours.
LTRESULT sav_SaveObjects(CServerMgr* pMgr, ILTStream* pStream, ObjectList* pList,
                         uint32 dwParam, uint32 flags);
LTRESULT sav_RestoreObjects(CServerMgr* pMgr, ILTStream* pStream,
                            uint32 dwParam, uint32 flags);
LTRESULT   fs_CopyFile(CFileMgr* pFileMgr, const char* pszSourceFile,
                       const char* pszDestFile);                  // 0x1002bd97

// 0x1000d926 — format a line and hand it to the debugger. The interface
// table's DebugOut entry points straight at it (name ours).
void con_Printf(char* pMsg, ...);

// 0x10051c74 — the queue depth past which a client is left alone for a
// frame, and 0x1002132a — the net layer's own view of the same question
// (names ours).
extern uint32 g_MaxQueueSize;

// The net layer's own knobs (names ours): how long a connection may go
// unacknowledged before it is declared timed out (0x10051c68), whether the
// timeout is announced (0x100584dc), and the two the connection trace reads
// — the level below which a line is printed (0x100584ec) and whether the
// trace covers connections that are still coming up (0x100584e0).
extern float  g_AckTimeout;
extern int    g_NetShowTimeouts;
extern int    g_NetDebugLevel;
extern int    g_NetDebugAll;

// 0x100383b6 — the current time, in seconds, as the engine keeps it.
float dsi_GetTime();

// One light animation baked by the preprocessor. Records are 0x60 bytes and
// live in one array on the server.
class LightAnim
{
public:
	uint8    m_pad_00[0x20];
	LTBOOL   m_bShadowMap;                   // 0x20  fixed when it was baked
	uint8    m_pad_24[0x28 - 0x24];
	uint32   m_nFrames;                      // 0x28
	uint8    m_pad_2c[0x34 - 0x2c];
	uint32   m_iFrames[2];                   // 0x34
	float    m_fPercentBetween;              // 0x3c
	float    m_fBlendPercent;                // 0x40
	LTVector m_vLightPos;                    // 0x44
	LTVector m_vLightColor;                  // 0x50
	float    m_fLightRadius;                 // 0x5c
};

// The engine-side server. Only the fields the interface touches are named.
// Its exact size, 0xb2c, is not a guess: CreateServer's `new CServerMgr`
// pushes it. It is polymorphic — CreateServer's failure path and DeleteServer
// both `delete` it through vtable slot 0 — so offset 0 is the vptr and every
// pad below starts four bytes in.
class CServerMgr : public CBaseMgr, public CServerSoundMgr
{
public:
	CServerMgr();                            // 0x1002c761
	// 0x1002c929, with the deleting form at 0x1002c90d that both exports
	// delete through. The body is Term(); the nine sub-objects are the
	// compiler's, in reverse declaration order.
	virtual ~CServerMgr();

	// The rest of the primary overrides (0x10029005, 0x10029025, 0x1002903d).
	virtual LTBOOL _v04(void* pA, void* pB);
	virtual void     _v08(class NetConnection* pConn);
	virtual void     ProcessPacket(class CPacket* pPacket, uint8* senderAddr,
	                               uint16 senderPort);
	void _v04();
	void _v08();
	void _v0c();

	// The ILTSoundMgr personality (the second base at +0x04). These receive
	// the subobject pointer and step back four bytes for the manager, which
	// is why the shipped KillSoundLoop adjusts inline.
	LTRESULT PlaySound(PlaySoundInfo *pPlaySoundInfo, HLTSOUND &hResult);    // 0x10034037
	void* GetSoundTrack(void* hFile);                                        // 0x1003448f
	LTRESULT GetSoundDuration(HLTSOUND hSound, LTFLOAT &fDuration);          // 0x10034225
	LTRESULT IsSoundDone(HLTSOUND hSound, LTBOOL &bDone);                    // 0x10034274
	void SetGlobalLightObject(HOBJECT hObj);                                 // 0x10034e4f
	// 0x1003448f — record a file for the current cache pass (name ours).
	class CCacheRec* AddCachedFile(void* hFile);
	LTRESULT KillSound(HLTSOUND hSound);                                     // 0x100342d5
	LTRESULT KillSoundLoop(HLTSOUND hSound);                                 // 0x10034359

	LTBOOL Init();                           // 0x10031f5f
	void InitMessageOwner(class ServerMessage* pMsg);   // 0x10034e3f (name ours)
	// 0x10033fca — tell one client which of its command bits changed this
	// frame, as a list of indices and a count (name ours).
	void NotifyCommandChanges(class Client* pClient, uint8* pChanged,
	                          uint32 nChanged);
	LPBASECLASS CreateObject(HCLASS hClass, ObjectCreateStruct* pStruct);  // 0x10033126
	void   Term();                           // 0x1003233f

	ClientServerType   m_ClientServerType;   // 0x008  = ServerType, set by the ctor
	StructBank         m_Bank0C;             // 0x00c
	StructBank         m_Bank28;             // 0x028
	LTList             m_List44;             // 0x044
	LTList             m_List54;             // 0x054
	int                m_ErrorCode;          // 0x064
	char               m_ErrorString[300];   // 0x068  SetupError formats into it
	uint8              m_pad_194[0x198 - 0x194];
	uint32             m_Unknown198;          // 0x198  cleared when a world starts running
	uint32             m_ServerFlags;        // 0x19c  SS_PAUSED, and nothing else
	uint32             m_Unknown1a0;          // 0x1a0  flag bits; bit 0 set while
	                                         //        removing objects, bit 0x10 while
	                                         //        the shell caches files
	CServerWorld       m_World;              // 0x1a4  exactly fills 8 bytes
	LightAnim*         m_pLightAnims;        // 0x1ac
	uint32             m_nLightAnims;        // 0x1b0
	uint8              m_pad_1b4[0x210 - 0x1b4];  // (see also m_Flags1A0 below)
	CUnknown210        m_Unknown210;         // 0x210
	uint8              m_pad_214[0x2a0 - 0x214];
	uint8              m_ShadeState[0x30c - 0x2a0];  // 0x2a0  what the point-shade
	                                                 //        lookup runs on; only
	                                                 //        its address is reached
	LTVector           m_WorldBoxMin;        // 0x30c  the loaded world's bounding box
	LTVector           m_WorldBoxMax;        // 0x318  (runs right up to 0x324)
	uint32             m_Unknown324;         // 0x324  bit 0 set once a world is up
	ILTStream*         m_pWorldStream;       // 0x328  kept open across the world
	uint8              m_pad_32c[0x330 - 0x32c];
	class CFileTree**  m_pFileTrees;         // 0x330  fs_AddResources' trees
	uint32             m_nFileTrees;         // 0x334
	uint8              m_pad_338[0x370 - 0x338];
	void*              m_pWorldFile;          // 0x370  null until a world is started
	uint8              m_pad_374[0x390 - 0x374];
	CUnknown390        m_Unknown390;         // 0x390
	// 0x394 .. 0x660 — the object lists. Two links here are walked by the
	// reconstruction and the rest of the region is unaccounted for.
	//
	// 0x10034fcb reads this region as an ARRAY: it sweeps
	// `esi = 0; esi < 0xb0; esi += 0x10` taking a list head at `+0x5b4 + esi`
	// and its first link at `+0x5b8 + esi`, which is eleven LTLists of stride
	// 0x10 running 0x5b0 .. 0x660 — and 0x660 is exactly where m_bWorldLoaded
	// starts, so that upper bound is exact. Eleven is also NUM_OBJECTTYPES, and
	// the two links below fall on entries 1 and 2 of such an array.
	//
	// It is NOT declared as one, though, and the constructor is the proof:
	// declaring `LTList m_ObjectLists[11]` here emits eleven tie-offs and costs
	// CServerMgr::CServerMgr (0x1002c76a, 281 bytes) its byte identity, because
	// the shipped constructor ties off only the two links below. So either the
	// other nine are initialised somewhere else, or the region holds individual
	// members that 0x10034fcb happens to be able to stride over. Until one of
	// those is settled the two known links stay as they are.
	uint8              m_pad_394[0x5c4 - 0x394];
	LTLink             m_Objects;             // 0x5c4  data links to LTObject;
	                                          //        array entry 1, if it is one
	uint8              m_pad_5d0[0x5d4 - 0x5d0];
	LTLink             m_WorldModelObjects;   // 0x5d4  data links to the world-model
	                                          //        LTObjects; GetHPolyObject walks it
	                                          //        matching m_pWorldBsp to the 0x330
	                                          //        record's +0x04 pointer.
	                                          //        Array entry 2, if it is one
	uint8              m_pad_5e0[0x660 - 0x5e0];

	// Kept as accessors so the call sites read the same either way, and so
	// that settling the array question later touches only these two lines.
	LTLink*            ObjectListHead()      { return &m_Objects; }
	LTLink*            WorldModelListHead()  { return &m_WorldModelObjects; }

	// The same region reached the way 0x10034fcb reaches it -- as eleven
	// LTLists from 0x5b0 -- without declaring the array that would cost the
	// constructor its byte identity. Cast rather than member so the layout
	// above is untouched.
	LTList*            ObjectLists()         { return (LTList*)((uint8*)this + 0x5b0); }

	int                m_bWorldLoaded;        // 0x660  set by LoadWorld
	void*              m_hWorldFile;          // 0x664  identifier of the loaded world file
	GlobalForceControl m_ForceControl;       // 0x668  SPhysicsLT's "world" force state:
	                                         //        its m_pWorld points at this manager
	// The record ends at 0x6a8: GlobalForceControl's frame-move answer at its
	// own +0x34 is the manager's 0x69c-0x6a7, so nothing is left over.
	CUnknown6A8Obj*    m_pUnknown6a8;        // 0x6a8  deleted raw by Term
	CollisionInfo*     m_pCollisionInfo;     // 0x6ac  the last collision; the flat
	                                         //        GetLastCollision copies it out
	float              m_FrameTime;          // 0x6b0
	float              m_UpdatesPerSec;      // 0x6b4  reset from g_UpdatesPerSec
	float              m_Unknown6b8;         // 0x6b8
	float              m_Unknown6bc;         // 0x6bc  world start time
	float              m_Unknown6c0;         // 0x6c0
	uint32             m_Unknown6c4;         // 0x6c4  fixed-step updates run
	float              m_Unknown6c8;         // 0x6c8
	float              m_Time;               // 0x6cc
	int                m_Unknown6d0;         // 0x6d0  updates this world
	float              m_Unknown6d4;         // 0x6d4
	float              m_Unknown6d8;         // 0x6d8  last update time
	uint32             m_Unknown6dc;         // 0x6dc

	// 0x6e0-0x7e3: the whole network manager. What used to be carved here as
	// direct CServerMgr fields (the driver array at 0x720, m_pSelf794, the
	// bandwidth pair at 0x7c0, the app guid at 0x7d0, the main driver at
	// 0x7e0) are CNetMgr members reached through it.
	CNetMgr            m_NetMgr;             // 0x6e0
	LTList             m_ClientList;         // 0x7e4  data links to Client
	uint32             m_Unknown7f4;         // 0x7f4
	uint32             m_Unknown7f8;         // 0x7f8
	uint32             m_Unknown7fc;         // 0x7fc  bytes queued this frame;
	                                         //        zeroed by the ctor and each flush
	uint32             m_Unknown800;         // 0x800  bytes dropped this frame;
	                                         //        zeroed by the ctor and each flush
	CMessageObjectHelper* m_Unknown804;          // 0x804  seeded into every new message; deleted raw by Term
	CUnknown808Obj*    m_pUnknown808;        // 0x808  Released by Term
	// 0x80c-0x83b: the class manager, which owns the shell pointer, the
	// per-class records, the class hash and this module's slot.
	CClassMgr          m_ClassMgr;           // 0x80c
	LTLink             m_Link83c;            // 0x83c  the queued-object list; retied per world
	StructBank         m_Bank848;            // 0x848
	StructBank         m_Bank864;            // 0x864
	StructBank         m_PropBank;          // 0x880  0xc0-byte ObjectProp records
	StructBank         m_Bank89C;            // 0x89c
	uint32             m_Unknown8b8;         // 0x8b8  = 200 at Init
	uint32             m_Unknown8bc;         // 0x8bc  count for the list below
	void*              m_hHash8c0;           // 0x8c0  freed like the model hash
	LTLink             m_Link8c4;            // 0x8c4  drained onto g_FreeLinks by Term
	LTLink             m_Link8d0;            // 0x8d0
	LTList             m_List8dc;            // 0x8dc  every live object, threaded
	                                         //        through SObjData::m_StateLink;
	                                         //        inactive ones live at the tail
	LTList             m_List8ec;            // 0x8ec
	// 0x8fc-0x90f: the object map, whose grow-by is what Init sets to 100.
	CUnknown8FC        m_Unknown8fc;         // 0x8fc
	LTObject*          m_pChangedObjects;    // 0x910  intrusive list through SObjData
	SoundInstance*     m_pChangedSounds;     // 0x914  linked through m_pNextChanged
	void*              m_hModelHash;          // 0x918  one entry per model reference
	void*              m_Unknown91c;         // 0x91c  dfree'd by Term
	uint32             m_Unknown920;         // 0x920
	uint32             m_Unknown924;         // 0x924
	CUnknown928Obj*    m_pUnknown928;        // 0x928  deleted through slot 71
	SkyDef             m_SkyDef;             // 0x92c  the SDK sky box: Init sets
	                                         //        (-10,-10,-10)/(+10,+10,+10),
	                                         //        view box zeroed
	uint16             m_SkyObjects[30];     // 0x95c  object ids; 0xffff = free slot
	                                         //        (memset to 0xff per world)
	HOBJECT            m_hGlobalLightObject; // 0x998  Get/SetGlobalLightObject
	StructBank         m_ObjectListBank;    // 0x99c  8-byte structs: ObjectList
	StructBank         m_ObjectLinkBank;    // 0x9b8  8-byte structs: ObjectLink
	StructBank         m_Bank9D4;            // 0x9d4  runs flush against the message bank
	CMessageBank       m_MessageBank;        // 0x9f0
	CUnknownA14        m_UnknownA14;         // 0xa14
	CFileMgr           m_FileMgr;            // 0xa2c
	class ObjectProp*  m_pCurProps;          // 0xa68  the property list of the
	                                         //        object being created
	void*              m_pGameInfo;          // 0xa6c  dalloc'd copy
	uint32             m_GameInfoLen;        // 0xa70
	ConsoleState       m_ConsoleState;       // 0xa74
	ServerAppHandler*  m_pAppHandler;        // 0xac0
	CServerThread      m_Thread;             // 0xac4
	class Model*       m_pPlaceholderModel;  // 0xb28  the object's last member;
	                                         //        stands in for a model that
	                                         //        is still loading

	// 0x100327d9 — walks the client list rooted at 0x7e8.
	Client* FindClient(uint32 nClientId);

	// Publish this manager through the globals (0x100330fc); every API entry
	// calls it first.
	void SetupGlobals();                                         // 0x100330fc

	// Record an error code and format its ltmsg string into m_ErrorString
	// (0x10034660). Varargs, so cdecl with `this` as a hidden stack argument.
	LTRESULT SetupError(int nError, ...);

	// The engine-side halves the interface wrappers forward to.
	LTBOOL AddResources(char** pResources, uint32 nResources);   // 0x10032631
	LTBOOL LoadBinaries();                                       // 0x10032696
	void   SetGameInfo(void* pGameInfo, uint32 nGameInfoLen);    // 0x100326b2
	int    GetNumClients();                                      // 0x100326f4
	LTBOOL GetClientName(int index, char* pName, int maxChars);  // 0x100326fb
	void   GetClientName(class Client* pClient, char* pName, int maxChars);  // 0x1003401a
	LTBOOL GetClientInfo(int index, ClientInfo* pInfo);          // 0x10032740
	LTBOOL BootClient(uint32 dwClientID);                        // 0x100327ad
	int    GetErrorCode();                                       // 0x1000ab4f
	void   GetErrorString(char* pString, int maxLen);            // 0x1003310f
	LTBOOL Update(long flags, float fTime);                      // 0x10032e69
	// 0x10032dcd — the hook for a model that finished loading on this thread
	// rather than on the worker's; it takes the same message the worker
	// would have sent back (name ours).
	void OnModelLoaded(class LThreadMessage* pMsg);

	class ServerMessage* StartMessage();                         // 0x10034ddd
	LTRESULT FreeUnusedModels();                                 // 0x10033d9d
	LTRESULT LoadWorld(ILTStream* pStream, char* pWorldName);    // 0x1003318d
	LTRESULT DoStartWorld(char* pszWorldName, uint32 flags, float fTime);  // 0x100339a4
	void     PrecacheModels();                                   // 0x10033e76 (name ours)
	LTRESULT DoRunWorld();                                       // 0x10033c1e
	void     EndWorld(int nMode);                                // 0x10033c9f
	void     ProcessThreadMessages();                            // 0x10032e2f

	// Update's helpers (names ours, from what they walk). UpdateSounds runs
	// each live SoundInstance's timer; UpdateObjects is the 868-byte tick
	// body; RemoveDeadSounds unlinks killed/expired sounds and returns them
	// to the bank at 0x28.
	void UpdateSounds(float fFrameTime);                         // 0x10032ceb
	void UpdateObjects();                                        // 0x10032854
	void RemoveDeadSounds();                                     // 0x10032d25
};

// The default update rate, 30.0f in .data at 0x10051d20 (the name is ours).
extern float g_UpdatesPerSec;

// 1.0f at 0x10051d24, scales the fixed-step frame time (the name is ours).
extern float g_TimeScale;

// Update's console toggles and profile counters (names ours; the last two
// are printed as "ILTServer::FindObjectsTouchingSphere ticks/count").
extern int   g_ShowGameTime;        // 0x10058534
extern int   g_ShowTickCounts;      // 0x1005853c
extern float g_Unknown585a0;        // 0x100585a0
extern uint32 g_nIsectQueries;     // 0x100585a4 (objbank.cpp)
extern uint32 g_nIsectTime;        // 0x100585a8 (objbank.cpp)
extern int   g_Unknown58728;        // 0x10058728
extern int   g_Unknown5872c;        // 0x1005872c
extern int   g_FindSphereTicks;     // 0x10058be8
extern int   g_FindSphereCount;     // 0x10058be4

// Update's cdecl helpers (names ours). sm_ProcessPackets services the net
// drivers and incoming packets, returning nonzero to abort the update;
// sm_UpdateClients walks the client list (variable-step path only).
int  sm_ProcessPackets(CServerMgr* pMgr);                        // 0x10029e00
void sm_UpdateClients(CServerMgr* pMgr);                         // 0x10032bf0

// The console printer again (0x1000e3f9, the same function dsi_ServerError
// names), but through the fixed two-argument prototype Update's tick-count
// stats were compiled against: VC6 defers stack cleanup across consecutive
// fixed-prototype cdecl calls (one add esp,0x10) where varargs call sites
// pop after each call — the shipped bytes say these two sites had a fixed
// prototype. The name is ours.
void dsi_PrintStat(char* pFormat, int nValue);

// Per-client half of sm_UpdateClients (0x10027782) and the detach notice a
// dying attached sound sends through 0x100139d0 (names ours).
void sm_UpdateClient(CServerMgr* pMgr, class Client* pClient);

// One object's update (0x1002a431, name ours) — UpdateObjects times it.
void sm_UpdateObject(CServerMgr* pMgr, class LTObject* pObject);

// The flush cluster around sm_FlushObjectQueue (names ours).
// sm_DrainObjectQueue processes every queued object once (more may queue
// while it runs, so the caller loops); sm_ApplyChangeFlags folds the
// changed-object and changed-sound lists into each in-world client's table;
// sm_SendClientUpdates writes and sends each client's update packet.
void sm_DrainObjectQueue(CServerMgr* pMgr);                       // 0x1002ac7c
void sm_ApplyChangeFlags(CServerMgr* pMgr);                       // 0x10032c54
void sm_SendClientUpdates(CServerMgr* pMgr);                      // 0x10032c1b
LTRESULT sm_RemoveObjectFromWorld(CServerMgr* pMgr, class BaseClass* pClass);  // 0x100336c6 (attested name)
// A world portal record (names ours): only the flags word is reached.
class WorldPortal
{
public:
	uint8  m_pad_00[0x06];
	uint16 m_Flags;                          // +0x06
};
WorldPortal* w_FindPortal(CServerWorld* pWorld, char* pName,
                          uint32* pnWorld, uint32* pnPortal);   // 0x10007dfa

// Attach/detach one client's view to another (names ours): the flat
// AttachClient/DetachClient entries forward here.
LTRESULT sm_AttachClient(CServerMgr* pMgr, class Client* pParent, class Client* pChild);  // 0x10027155
LTRESULT sm_DetachClient(CServerMgr* pMgr, class Client* pClient);                        // 0x10027259

// The attachment manager embedded at 0x390 (names ours): create takes the
// parent, the child's network id, and the socket/node index, handing back
// the record; remove is the named "ILTServer::RemoveAttachment" worker.
LTRESULT am_CreateAttachment(CUnknown390* pAttachMgr, LTObject* pParent, uint16 nChildID,
                             int iNode, LTVector* pOffset, LTRotation* pRotOffset,
                             class Attachment** ppAttachment);          // 0x10025984
LTRESULT am_RemoveAttachment(CUnknown390* pAttachMgr, LTObject* pParent,
                             class Attachment* pAttachment);            // 0x10025a1b

// 0x1005852c (name ours) — the auto-deactivation gate SetObjectState and
// SetDeactivationTime consult.
extern int g_bAutoDeactivate;

// The world-model move/rotate request (names ours): only the fields the
// rotator reads are mapped; the last dword is cleared with the pointers.
struct WMMoveState
{
	CUnknown210*          m_pContainer;      // +0x00  &pMgr->m_Unknown210
	class CUnknown6A8Obj* m_pUnknown04;      // +0x04  pMgr->m_pUnknown6a8
	LTObject*             m_pObj;            // +0x08
	uint32                m_nUnknown0C;      // +0x0c  (uint8)obj->m_BlockingPriority
	// +0x10/+0x14 — a caller-supplied array of objects to push, and its
	// count. 0x004548c3 indexes +0x10 as (%edx,%esi,4) against the unsigned
	// count at +0x14; when the array is null the container's box query runs
	// instead, with 0x00454f00 as the per-object callback (names ours).
	// TYPED uint32, NOT LTObject**, DELIBERATELY.  The pointer type reads
	// better and costs a match: commonphysics.cpp casts this field on use,
	// and with the member already a pointer that cast becomes a no-op, which
	// moves wm_PushWithGeometry and takes 0x100253f0 with it.  Cast
	// placement is source-reachable -- measured, 1651 -> 1650.
	uint32                m_pPushList;       // +0x10
	uint32                m_nPushCount;      // +0x14
	uint8                 m_pad_18[0x54 - 0x18];
	// +0x54 is unused by this struct. The gate the per-object push filter
	// (0x1002115c) tests for zero is read off ARGUMENT ONE of 0x00454f00 —
	// the object the container's box query hands the callback — so it is
	// LTObject::m_Unknown54 in ltobject_internal.h, not a field here.
	uint32                m_nUnknown54;      // +0x54
	uint8                 m_pad_58[0x60 - 0x58];
	// 0x60 — the rotation itself, as a 4x4. wm_RotatePushObject runs each
	// overlapping object's position through it to find where the rotation
	// would put that object (name ours).
	class LTMatrix*       m_pRotation;       // +0x60
	uint32                m_nUnknown64;      // +0x64
};
// 0x10020a53 — rotate a world model through the simulation (name ours).
void wm_RotateWorldModel(WMMoveState* pState, LTRotation* pRotation, int bUnknown);

// 0x1002ade4 — move an object between the active and inactive halves of the
// 0x8dc list (names ours): nStateBits is 8/0x10/0x20, the SObjData change
// bits GetNextObject's 0x38 mask tests.
void sm_SetObjectState(CServerMgr* pMgr, LTObject* pObj, uint32 nStateBits);
// 0x1002a239 — is the object one the sweep may deactivate? (name ours)
// 0x1002a239 -- INLINE and in this header, which is where the original had
// it. lithtech.exe's /Ot expands it at both sites (0x0047c568 inside
// CObjectMover::CanDeactivate and 0x00469588) and has no standalone copy at
// all; server.dll's /Os declines it and calls 0x1002a239 from both sites,
// so this cannot move server codegen. The body was in smgr.cpp, where
// /Ob1 would never have expanded it and where it sat BELOW its own caller.
// The pSData local is what makes /Os decline: without it server.dll expands
// the body at both sites too (46/70 bytes against shipped 13/52); with it
// /Os calls the COMDAT, /Ot still expands, and the codegen is unchanged. A
// single result local also makes /Os decline but gives the wrong
// standalone (one `ret`, 13 insns against 14).
inline LTBOOL sm_CanDeactivate(LTObject* pObj)
{
	SObjData* pSData;
	uint32 nFlags;

	pSData = pObj->m_pSData;
	if (!pSData->m_Unknown2c)
	{
		nFlags = pObj->m_Flags;

		if (nFlags & 0x21213001)
		{
			if (nFlags & 0x10000000)
				return LTTRUE;
		}
		else
		{
			return LTTRUE;
		}
	}

	return LTFALSE;
}

// 0x1002a205 — refresh the object's boxes in the world containers after a
// flag change (name ours). LTRESULT, not void as this first read it: the
// shipped body ends `xor %eax,%eax; ret` on every path, so it answers LT_OK
// deliberately, including where it did nothing. Its one call site discards
// the result.
LTRESULT sm_UpdateObjectBBox(CServerMgr* pMgr, LTObject* pObj);
// 0x1003491b — reactivate what the object can see (PingObjects' worker).
void sm_PingObjects(CServerMgr* pMgr, LTObject* pObj);

// The inter-object link workers (names ours); the flat entries forward here.
// One inter-object link, threaded through both ends' SObjData::m_Links. Type 1
// is the container relationship the two container walks read (names ours).
#define INTERLINK_CONTAINER 1

class InterObjectLink
{
public:
	uint32    m_Type;                        // +0x00
	LTObject* m_pOwner;                      // +0x04  the container end
	LTObject* m_pLinked;                     // +0x08  the contained end
};

LTRESULT sm_CreateInterObjectLink(CServerMgr* pMgr, LTObject* pOwner,
                                  LTObject* pLinked, int nFlags);       // 0x10013b63
void     sm_BreakInterObjectLink(CServerMgr* pMgr, LTObject* pOwner,
                                 LTObject* pLinked, int nFlags);        // 0x100139d0

// The named SetPortalFlags worker (0x10034985) and the console broadcast
// BPrint rides (0x10034521; names ours).
LTRESULT sm_SetPortalFlags(CServerMgr* pMgr, char* pPortalName, uint32 flags);
void     sm_BroadcastPrint(char* pMsg);

// 0x100297b8 — queue one message to every client (names ours).
void sm_BroadcastToClients(CServerMgr* pMgr, int nPacketID, class ServerMessage* pMsg);

// 0x10034cf9 — tell every client to cache one file (names ours); the gate
// at 0x10051c60 is set only inside ServerShell::CacheFiles.
void sm_BroadcastCacheFile(CServerMgr* pMgr, uint32 fileType, uint16 nFileID);
extern int g_bCachingEnabled;                    // 0x10051c60

// One record of the current cache pass (names ours).
class CCacheRec
{
public:
	uint8  m_pad_00[0x18];
	uint32 m_bWanted;                        // +0x18
};

void sm_MarkSkyChanged(CServerMgr* pMgr);                         // 0x10028685 (name ours)
LTRESULT sm_RemoveObjectFromSky(CServerMgr* pMgr, class LTObject* pObject);     // 0x10028858 (name ours)
class LTObject* sm_FindObjectByID(CServerMgr* pMgr, uint16 nID);  // 0x1002acd3 (name ours)

// sm_RemoveObjectFromWorld's teardown helpers (names ours, by module):
// break the object's attachments (0x10013a87, the sm_DetachSound region),
// free the thing at SObjData+0x28 (0x10031f2f), unlink the model trackers
// (0x1001e7ef/0x1001e828), release engine-side object memory (0x1000d8a8),
// final smgr bookkeeping (0x100333fc), remove from the world tree
// (0x1002bab8), and drop it from the 0x390 container (0x1002592e).
void sm_BreakAttachments(CServerMgr* pMgr, class LTObject* pObject, int a, int b);  // 0x10013a87
void sm_Free28(CServerMgr* pMgr, void* p);                        // 0x10031f2f

// The two the manager's connection slots forward to (names ours): a new
// connection becomes a client, and a dead one stops being one.
// Returns the client it made: 0x00463cb1 `mov eax,esi` puts it in %eax on the
// way out. Declared LTRESULT here for as long as it was undefined.
class Client* sm_AddClientForConnection(CServerMgr* pMgr,
                                        class NetConnection* pConn,
                                        int bLocal);                      // 0x10026d23
void     sm_DropConnection(CServerMgr* pMgr, class NetConnection* pConn);  // 0x10027135

// 0x100338b8 — hand the world over to the shell to cache what it wants, then
// let go of every model nothing is using (name ours).
LTRESULT sm_CacheShellFiles(CServerMgr* pMgr);

// 0x1003481c — whatever the queued-object list still owes, done now; and
// 0x1003471d hands its nodes back afterwards (names ours).
void sm_FlushList44(CServerMgr* pMgr);
void sm_ReturnList44(CServerMgr* pMgr);

// 0x10034757 — the sound side of the same sweep (name ours).
void sm_FreeUnusedSounds(CServerMgr* pMgr);
void obj_UnlinkTrackersA(class LTObject* pObject);                // 0x1001e7ef
void obj_UnlinkTrackersB(class LTObject* pObject);                // 0x1001e828
void obj_FreeData(class LTObject* pObject);                       // 0x1000d8a8
void sm_ObjectRemoved(CServerMgr* pMgr, class LTObject* pObject); // 0x100333fc
// Per-object-type handlers, two entries each; only the first is reached
// from here (name ours). 0x1005402c.
class ObjTypeHandlers
{
public:
	uint32 (*m_pApplyFilenames)(class CServerMgr* pMgr, class LTObject* pObject,
	                            ObjectCreateStruct* pStruct);   // 0x00
	uint32 (*m_pRemove)(class CServerMgr* pMgr, class LTObject* pObject);  // 0x04
};

extern ObjTypeHandlers g_ObjTypeHandlers[];

uint32 sm_RemoveFromWorldTree(CServerMgr* pMgr, class LTObject* pObject);  // 0x1002bab8
LTRESULT un390_RemoveObject(class CUnknown390* p, class LTObject* pObject);  // 0x1002592e
void sm_ClearChangedObjects(CServerMgr* pMgr);                    // 0x10032805
void sm_ClearChangedSounds(CServerMgr* pMgr);                     // 0x10032832
void sm_WriteClientUpdate(CServerMgr* pMgr, class Client* pClient);   // 0x10027e8b
void sm_FlushClientPackets(CServerMgr* pMgr, class Client* pClient);  // 0x100277c2
int  sm_SetClientState(CServerMgr* pMgr, class Client* pClient, int nState);   // 0x100274dc
LTRESULT sm_UpdateClientState2(CServerMgr* pMgr, class Client* pClient);  // 0x10026859

// Client state transitions (names ours): prep may refuse the world entry,
// enter announces it, exit is run when a client leaves state 3.
// 0x1002759a (enter) is declared in smgr.cpp, as sm_ClientEnterWorld2 returning
// LTRESULT. A stale `void sm_ClientEnterWorld` prototype used to sit here and
// had no definition. Adding or removing any declaration in this header moves
// TCPDriver::HandleConnPacket (0x10036ee5), whose two parameter loads flip
// order at a symbol-count threshold. Measure it before adding one.
int  sm_PrepClientEnterWorld(CServerMgr* pMgr, class Client* pClient);  // 0x10027533
void sm_ClientExitWorld(CServerMgr* pMgr, class Client* pClient);       // 0x100276eb

// The file-tree view the handshake walks (names ours): each tree at 0x330
// holds a list whose records are 0x24 apart with a flags word at +0x06.
class CFileRec
{
public:
	uint8  m_pad_00[0x06];
	uint16 m_nFlags;                         // +0x06  nonzero files get announced
	uint8  m_pad_08[0x24 - 0x08];
};

class CFileList
{
public:
	uint8     m_pad_00[0x74];
	uint32    m_nPolies;                     // +0x74  GetNextPoly's per-world bound
	uint8     m_pad_78[0xb0 - 0x78];
	uint8*    m_pFiles;                      // +0xb0  CFileRec records, 0x24 apart
	uint32    m_nFiles;                      // +0xb4
};

class CFileTree
{
public:
	uint8      m_pad_00[0x04];
	CFileList* m_pList;                      // +0x04
};

// One record of m_List8ec (names ours): objects a late-joining client still
// has to be told about, keyed by object id, dfree'd when claimed.
class CUnknown8ECRec
{
public:
	LTLink m_Link;                           // +0x00
	uint32 m_Flags;                          // +0x0c  bit 1 is the one the
	                                         //        info-flag reader hands back
	                                         //        (a dword: lithtech 0x004713b9
	                                         //        loads it `mov eax; and 0ffh`)
	uint16 m_nObjectID;                      // +0x10
	char   m_Name[1];                        // +0x12  inline, not a pointer
};

// The world-entry senders (names ours; purposes inferred from position) and
// the direct-to-client send (0x10029653). 0x10026b4d is sm_SendObjectInfo2,
// declared in smgr.cpp (LTRESULT); the stale void prototype that stood here
// is gone (see the note at sm_PrepClientEnterWorld).
void sm_SendWorldInfo(CServerMgr* pMgr, class Client* pClient);   // 0x1002666c
void sm_SendSkyInfo(CServerMgr* pMgr, class Client* pClient);     // 0x100286a9
void sm_SendToClient(CServerMgr* pMgr, class Client* pClient, int nPacketID,
                     class ServerMessage* pMsg, int nFlag, int nPriority);   // 0x10029653
// The queued send the flat SendToClient rides (names ours).
//
// DECLARED HERE AND NEVER DEFINED, and that is deliberate until someone can
// tell it apart from sm_QueueMessageToClient2 (smgr.cpp, 0x100296d3), which
// has the same role and a different signature -- LTRESULT/int against this
// void/uint8. It has been proposed that they are one function with a stale
// declaration, on the grounds that "the shipped mangling says LTRESULT and
// int". THAT ARGUMENT IS CIRCULAR: server.dll carries no symbols at all, so
// the only mangled name available is the one OUR compiler produced from THIS
// declaration.
//
// It was tried anyway and MEASURED: reconciling them costs a match.
// sm_SFXSendToOwner (0x10034b49, 49 bytes) passes pInfo->m_nPacketID here and
// reproduces exactly while the parameter is uint8; widening it to int breaks
// that function and nothing else improves. So the callers want the narrow
// spelling whatever the definition wants, and the two stay separate until
// real evidence -- not a mangling we generated ourselves -- says otherwise.
void sm_QueueMessageToClient(CServerMgr* pMgr, class Client* pClient, uint8 nPacketID,
                             class ServerMessage* pMsg, uint32 flags);       // 0x100296d3
// SFX plumbing (names ours): stow an object's SFX message; send a positioned
// SFX message to every client that can see the position.
void sm_SetObjectSFXMessage(CServerMgr* pMgr, LTObject* pObj,
                            class ServerMessage* pMsg);                      // 0x1002ad4a
LTRESULT sm_SendSFXMessage(CServerMgr* pMgr, int nPacketID, class ServerMessage* pMsg,
                           LTObject* pObj, LTVector* pPos, uint32 flags);    // 0x10034a6d
uint16 sm_GetObjectChangeFlags(CServerMgr* pMgr, class LTObject* pObject);   // 0x1002a264

// Packet dispatch (names ours): the handler table at 0x100587ac, indexed by
// the packet's first byte & 0x3f; a nonzero handler return aborts the whole
// update. Unhandled packets from a known client are queued raw. 256 slots
// although only 64 are reachable: the reset clears 0x400 bytes (smgr.cpp).
typedef int (*PacketHandlerFn)(CServerMgr* pMgr, class ServerMessage* pMsg,
                               class Client* pClient);
extern PacketHandlerFn g_PacketHandlers[256];
class Client* sm_FindClientByConnection(CServerMgr* pMgr, NetConnection* pConn);  // 0x10028658

// Client+0x18c is the file-transfer state servermessage.cpp calls
// CClientFiles, and these two are that family's message handler and its
// frame pump — not a packet queue. Both were called msg_QueuePacket /
// msg_FlushPackets here on nothing but the shape of their call sites; the
// bodies (reconstructed in servermessage.cpp, 0x1000e9ee byte-identical and
// 0x1000e941 a documented near) read the xfer state's own fields and call
// the xfer_* family. The handler ANSWERS whether it took the packet, which
// is what every other entry in the dispatch table answers too.
class CClientFiles;
int  xfer_HandleMessage(CClientFiles* pState, class ServerMessage* pMsg);  // 0x1000e941
void xfer_Update(CClientFiles* pState, float fTime);                       // 0x1000e9ee

// ---- sm_WriteClientUpdate's machinery (names ours) ---------------------- //

class CUpdateContext;

// The visibility gather handed to the world query (ctor 0x100283c6): center,
// radius, two callbacks and their context.
class CVisQuery
{
public:
	CVisQuery();

	uint32   m_Unknown00;                    // +0x00  zeroed by the ctor
	LTVector m_Position;                     // +0x04
	float    m_Radius;                       // +0x10
	// void, not int: the default the constructor installs at +0x14 is a
	// one-byte `ret` at 0x10024cac, and sm_VisCallbackA puts nothing in eax
	// either -- neither could be written to return a value, because VC6
	// makes C4716 an error. m_Fn2's default is null, so nothing constrains
	// its return type yet and it keeps the int it was given.
	void (*m_Fn1)(void*, void*);             // +0x14  0x10027c3c
	int (*m_Fn2)(void*, void*);              // +0x18  0x100283ff
	CUpdateContext* m_pUser;                 // +0x1c
	// Three more the constructor writes, which the pad here used to hide:
	// another no-op default and a predicate defaulting to "yes" (0x10024cad
	// answers 1), with a context word after each (names ours).
	void  (*m_pFn20)(void*, void*);          // +0x20
	int   (*m_pFn24)(void*, void*);          // +0x24
	uint32  m_Unknown28;                     // +0x28
	uint8   m_pad_2c[0x48 - 0x2c];
};

// The object container at 0x210 answers sphere queries through its vtable
// (slot 5); the embedded member stays plain, so the call goes through this
// view of its vptr.
class IObjectQuery
{
public:
	virtual void _v00() = 0;
	// +0x04 — box query: calls pFn(pObject, pUser) for every object whose
	// bounds overlap the box (the sphere walk narrows the hits itself).
	virtual void FindObjectsInBox(LTVector* pMin, LTVector* pMax,
	                              int (*pFn)(LTObject* pObj, void* pUser),
	                              void* pUser, int nFlags) = 0;
	virtual void _v08() = 0;
	// +0x0c — point query: calls pFn(pObject, pUser) per container the point
	// is inside (GetPointContainers hands it a gather record).
	virtual void FindPointContainers(LTVector* pPoint,
	                                 int (*pFn)(LTObject* pObj, void* pUser),
	                                 void* pUser, int nFlags) = 0;
	virtual void _v10() = 0;
	virtual void FindObjects(CVisQuery* pQuery) = 0;   // +0x14
};

// The gather record GetPointContainers hands the point query (names ours).
struct PointContainerGather
{
	HOBJECT* m_pList;
	uint32   m_MaxListSize;
	uint32   m_nElements;
	LTVector m_Position;
};
int sm_GatherContainersCB(LTObject* pObj, void* pUser);      // 0x1002faaa

// One queued outgoing message on a client's 0x3cc list; Release is
// 0x10031d47.
class CQueuedMsg
{
public:
	void Release();

	uint32 m_Unknown00;                      // 0x00  nonzero suppresses the write
	LTLink m_Link;                           // 0x04
};

// 0x10029bdf — the write itself; 0x10029bc4 is the gate in front of it
// (names ours).
// The first argument is a SoundRecord, not a CQueuedMsg: all eleven
// PlaySoundInfo members fall out of the offsets with nothing left over, each
// gated by the exact PLAYSOUND_ bit ltbasedefs.h says gates it. The gate at
// 0x10029bc4 passes what it calls a CQueuedMsg and matches anyway, because it
// only tests +0x00 and the pointer type never reaches its codegen.
void sm_WriteQueuedMessageBody(class SoundRecord* pRec, class Client* pClient,
                               class ServerMessage* pMsg);

int  sm_SkipClientUpdate(class Client* pClient);                            // 0x100285de
void sm_WriteUpdateHeader(CServerMgr* pMgr, CUpdateContext* pCtx,
                          class Client* pClient);                           // 0x10027c97
void sm_Unknown26755(CServerMgr* pMgr, class Client* pClient);              // 0x10026755
void sm_WriteChangedObjects(CServerMgr* pMgr, class CUnknown390* p390,
                            CUpdateContext* pCtx);                          // 0x10027e26
void sm_WriteObjectUpdate(CUpdateContext* pCtx, class LTObject* pObject,
                          CObjInfo* pInfo);                                 // 0x100279d9
void sm_EndUpdatePacket(CUpdateContext* pCtx, class ServerMessage* pMsg,
                        int nA, int nB);                                    // 0x100278ad
void sm_WriteQueuedMessage(CServerMgr* pMgr, CQueuedMsg* pRec,
                           class Client* pClient, class ServerMessage* pMsg);  // 0x10029bc4
void sm_WriteRemovedObjects(CUpdateContext* pCtx, class ServerMessage* pMsg);  // 0x10028454
void sm_WriteIDList(CUpdateContext* pCtx, ClientIDList* pList);             // 0x10027d39
void sm_Unknown27c71(CServerMgr* pMgr, class Client* pClient,
                     class ServerMessage* pMsg);                            // 0x10027c71
void sm_Unknown2873d(CServerMgr* pMgr, class Client* pClient);              // 0x1002873d

// The two gather callbacks the query calls per candidate object.
void sm_VisCallbackA(void* pUser, void* pObject);                            // 0x10027c3c
int sm_VisCallbackB(void* pUser, void* pObject);                            // 0x100283ff

// CNetMgr's globals and C helpers (names ours).
extern int   g_NetLogPackets;                // 0x100584f0
extern float g_NetResendTime;                // 0x100584b8
extern float g_NetAckTime;                   // 0x10053368  1/3 second
extern int   g_NetShowStats;                 // 0x100584ac
extern int   g_NetTimeouts;                  // 0x10051c80
extern char  g_NetNameBuf[];                 // 0x10056ff4
// 0x10028ace — the net layer's release. Not specific to a service: the
// packet queues hand their records and their payloads to the same function
// (name ours).
void  net_Free(void* pMem);
// The third argument is a BYTE. 0x10021cef and 0x10021d16 are both
// `cmpb $0x0,0x10(%ebp)`, which an `int` parameter cannot produce -- and
// LTBOOL will not do either, since the engine typedefs it 32 bits wide.
void  net_LogPacket(class ServerMessage* pMsg, uint32 nLen, uint8 bOut);  // 0x10021cbe
float dsi_GetTime();                         // 0x100383b6


extern int           g_NetBandwidth;        // 0x10058484  target bytes/sec
extern ClientIDList* g_pFillIDList;         // 0x100587a4  the list being refilled
extern uint32        g_Unknown587a0;        // 0x100587a0  gather-tick accumulator
extern int           g_Unknown5845c;        // 0x1005845c

// UpdateObjects' console toggles and accumulator (names ours).
extern int g_bAutoDeactivate;  // 0x1005852c  gates the next-update countdown (declared above too; "AutoDeactivate")
extern int g_ShowClassTicks;   // 0x10058538
extern int g_TotalTicks;       // 0x10058438  frame sum of every class's ticks

// More fixed prototypes of the 0x1000e3f9 printer (see dsi_PrintStat): the
// class-tick dump batches its stack cleanups, so none of these sites was
// compiled varargs. Names ours.
void dsi_PrintString(char* pString);
void dsi_PrintClassRow(char* pFormat, char* pName, uint32 nObjects,
                       uint32 nUpdated, uint32 nAvg, uint32 nMax, uint32 nTicks);
void dsi_PrintFrameTotals(char* pFormat, int nObjects, int nUpdated, int nTicks);
void sm_DetachSound(CServerMgr* pMgr, class LTObject* pObject,
                    SoundInstance* pSound, int nFlag);

// 0x100257f2 — tear down the object at 0x390.
// int, not void: `xor %eax,%eax` sits inside the epilogue at 0x1002585b,
// exactly as its sibling un390_Init's does. objtypes.cpp had to drop this
// header entirely over the conflict, because a return type is not
// overloadable.
int un390_Term(class CUnknown390* p);

// 0x1000dbe0 — free a hash table outright.
void dhash_Free(void* hTable);              // 0x1000dbe0

// The hash key kinds, in the order dhash_Create indexes its function tables
// (names ours).
#define DHASHKEY_WORD     0
#define DHASHKEY_STRING   1
#define DHASHKEY_NOCASE   2
#define DHASHKEY_FILENAME 3
#define DHASHKEY_COUNT    4

// The engine's shared LTLink free pool (0x10058bdc): Term threads spent links
// onto it through their m_pPrev. Not a global of its own -- it is the free
// list of the link bank at 0x10058bc0 (+0x1c), which is how smgr.cpp reaches
// it; nothing defines or references this name any more.
extern LTLink* g_FreeLinks;

// 0x10035c81 — install (or clear) the console print hook; Term passes the
// server's own hook at 0x10032617 and NULL user data.
void dsi_SetErrorHook(void* pFn, void* pUser);
void sm_ErrorHook(char* pMsg);               // 0x10032617

// 0x10028fbb — tear down the console state.
void cc_TermState(ConsoleState* pState);

// Empty in release; /OPT:ICF folded it with every other empty function, so
// the identity is unrecoverable (the ctor pairs it with the class manager).
void cb_Term();

// The generic hash container the model references live in (0x1000dxxx,
// beside dalloc/dfree; the names are ours).
void* dhash_GetFirst(void* hTable);              // 0x1000de33
void* dhash_GetNext(void** phIter);              // 0x1000de74 — returns the element, advances the iterator
void* dhash_GetData(void* hElement);             // 0x1000de16
void  dhash_Remove(void* hTable, void* hElement);// 0x1000dcf1
// Keyed lookup over the same tables (names ours): the object-name hash at
// CServerMgr+0x8c0 is what FindNamedObjects walks with these two.
void* dhash_FindElement(void* hTable, void* pKey, uint32 keyLen);                     // 0x1000dd38
void* dhash_FindNextElement(void* hTable, void* hElement, void* pKey, uint32 keyLen); // 0x1000dda2

// ILTServer's flat sphere gather and its release (the SDK's
// FindObjectsTouchingSphere / RelinquishList function-pointer entries).
// What the box walk carries for the sphere test: the list being filled and
// the sphere itself, pre-squared and pre-doubled (names ours).
class SphereQuery
{
public:
	struct ObjectList* m_pList;              // +0x00
	LTVector*          m_pPosition;          // +0x04
	float              m_Radius;             // +0x08
	float              m_RadiusSqr;          // +0x0c
	float              m_Diameter;           // +0x10
};

// The two world queries the interface forwards to (names ours). The segment
// test takes the manager's object container and a flag; the shade lookup
// answers in bytes, blue first.
LTBOOL w_IntersectSegment(IntersectQuery* pQuery, IntersectInfo* pInfo,
                          class CUnknown210* pObjects, int nFlags);   // 0x1000edd7
void   w_GetPointShade(uint8* pShadeState, LTVector* pPoint, uint8* pColor);  // 0x1002cf33

// 0x1002f861 — one candidate from the box walk (name ours).
int sm_SphereGatherCB(LTObject* pObj, void* pUser);

struct ObjectList* sm_FindObjectsTouchingSphere(LTVector* pPosition, float radius);   // 0x1002f90d
void sm_RelinquishList(struct ObjectList* pList);                                     // 0x1002f9f4

// DoRunWorld's helpers, all cdecl over the manager (names ours, from what
// they do): 0x100338e1 caches every object's model and skin files;
// 0x100338b8 runs the shell's CacheFiles and frees unused models;
// 0x10032bb8 drains the queued-object list at 0x83c.
LTRESULT sm_CacheObjectFiles(CServerMgr* pMgr);
LTRESULT sm_CacheFiles(CServerMgr* pMgr);

LPBASECLASS sm_AllocateObject(CServerMgr* pMgr, struct ClassDef* pClassDef);   // 0x10001c54
LTRESULT    sm_CreateObjectInWorld(CServerMgr* pMgr, LPBASECLASS pObject,
                                   struct ClassDef* pClassDef, ObjectCreateStruct* pStruct,
                                   uint32 nObjectID, float fData,
                                   LTObject** ppObject);                       // 0x100334fe
void        sm_FreeObject(CServerMgr* pMgr, struct ClassDef* pClassDef,
                          LPBASECLASS pObject);                                // 0x1002abf8
LTRESULT sm_CacheFile(CServerMgr* pMgr, uint32 fileType, char* pFilename);   // 0x10034b7a
void     sm_FlushObjectQueue(CServerMgr* pMgr);

// Term's helpers (names ours, from what they do): remove one client
// (0x10027308, logs "Removing client..."), remove objects (0x100325bd;
// LTTRUE removes even the protected ones), free every model at shutdown
// (0x1002a365), and return m_List44's nodes to their bank (0x1003471d).
void sm_RemoveClient(CServerMgr* pMgr, Client* pClient);
void sm_EndWorldA(CServerMgr* pMgr);                        // 0x1002af34
void sm_EndWorldB(CServerMgr* pMgr);                        // 0x1002aec2
void sm_ResetClient(CServerMgr* pMgr, Client* pClient, int n);  // 0x100274dc
void sm_EndWorldC(CServerMgr* pMgr);                        // 0x10033d65 (in smgr.cpp)
void sm_EndWorldD(CServerMgr* pMgr);                        // 0x100347a4
void sm_StartWorldA(CServerMgr* pMgr, int n);               // 0x1003385a

// Init's helpers.
int   un390_Init(CUnknown390* p, int n);                    // 0x100256c7
// One bucket of a hash table and the table itself: a three-dword header and
// then one 0x18-byte bucket per slot, which is where the create's
// 24n+12 allocation comes from (names ours).
class CHashSlot
{
public:
	class CHashTable* m_pTable;              // +0x00
	uint32            m_nIndex;              // +0x04  its own slot number
	uint32            m_nElements;           // +0x08
	LTLink            m_Head;                // +0x0c
};

// One element: the link that files it in its bucket, the bucket it is in,
// the caller's payload, and the key stored inline after the header. A name
// lookup hands the key straight back, which is how an object identifier
// yields its name (names ours).
class CHashElement
{
public:
	LTLink     m_Link;                       // 0x00  m_pData points back here
	CHashSlot* m_pSlot;                      // 0x0c
	void*      m_pData;                      // 0x10
	uint16     m_nKeyLen;                    // 0x14
	char       m_Key[1];                     // 0x16
};

class CHashTable
{
public:
	uint32    m_nKeyType;                    // 0x00  DHASHKEY_*
	uint32    m_Unknown04;                   // 0x04
	uint32    m_nSlots;                      // 0x08
	CHashSlot m_Slots[1];                    // 0x0c
};

void* dhash_Create(int nSlots, int n);                      // 0x1000d9b4

// The bank word-keyed elements come out of: the same vptr-over-StructBank
// shape as the others, and its Allocate is the shared forward 0x1000ed6f
// (objbank.h: the element's LTLink has an empty constructor, nothing more)
// (name ours).
//
// Recycled word-keyed elements go back onto the bank's own free list:
// 0x10058404, which dhash.cpp's frees write, is g_HashElementBank + 0x1c,
// i.e. m_Bank.m_pFreeList, not a global of its own.
//
// The constructor and destructor are only DECLARED here, and dhash.cpp
// defines them inline (its static-initialisation pair expands both). An
// in-class body, or any other added declaration in this header, flips
// tcpdriver.cpp's HandleConnPacket (a header symbol-count
// threshold); a declared-only constructor, paid for by dropping the old
// g_pFreeHashElements extern, does not.
class CHashElementBank : public CBankBase
{
public:
	CHashElementBank();
	virtual ~CHashElementBank();

	CHashElement* Allocate();                // 0x1000ed6f

	StructBank m_Bank;                       // +0x04
	uint32     m_Unknown20;                  // +0x20
};

extern CHashElementBank g_HashElementBank;   // 0x100583e8

typedef uint32 (*DHashFn)(void* pKey, uint32 nKeyLen);
typedef LTBOOL (*DHashCmpFn)(void* pKey1, void* pKey2, uint32 nKeyLen);

// 0x1005841c and 0x1005840c (names ours).
extern DHashFn    g_DHashFns[DHASHKEY_COUNT];
extern DHashCmpFn g_DHashCmpFns[DHASHKEY_COUNT];

uint32 dhash_GetSlot(class CHashTable* pTable, void* pKey, uint32 nKeyLen);  // 0x1000dccd
char*  dhash_GetKey(void* hElement, uint32* pLen);                          // 0x1000ddfb
void*  dhash_AddElement(class CHashTable* pTable, void* pKey, uint32 nKeyLen);  // 0x1000dc42
void*  dhash_FindElement(class CHashTable* pTable, void* pKey, uint32 nKeyLen); // 0x1000dd38
void*  dhash_GetData(void* hElement);                                       // 0x1000de16
void   dhash_SetData(void* hElement, void* pData);                          // 0x1000de23
void  cc_InitState(CServerMgr* pMgr, ConsoleState* pState); // 0x10028f52
void  cc_TermState(ConsoleState* pState);                   // 0x10028fbb (name ours)
CUnknown928Obj* sm_CreateUnknown928(CServerMgr* pMgr);      // 0x1002d34a
void  fs_Init(CFileMgr* pFileMgr, CServerMgr* pMgr);        // 0x1002bbc9
void  sm_ResetProfileCounters();                            // 0x10029ee7
extern uint32 g_Unknown58bec;                               // 0x10058bec
LTRESULT sm_LoadWorldObjects(CServerMgr* pMgr, ILTStream* pStream,
                             char* pszWorldName, uint32 bFlag);  // 0x1002a8b6

// File-manager helpers reached from DoStartWorld.
char* fs_GetFilename(CFileMgr* pFileMgr, void* hFile);           // 0x1002bfcd
int   fs_GetFileIdentifier(CFileMgr* pFileMgr, char* pFilename,
                           int nType, void** phFile);            // 0x1002be8c

// The record behind fs_GetFileIdentifier's handle (opaque elsewhere); the
// client file messages carry its id word (names ours).
class CFileIdentifier
{
public:
	uint8  m_pad_00[0x08];
	uint16 m_FileID;                         // +0x08
};

// 0x1002b4dc — load (or find) a model by name, handing back the Model and
// its file identifier (names ours; ThreadLoadModel's entry).
LTRESULT sm_LoadModel(CServerMgr* pMgr, char* pFilename, class Model** ppModel,
                      CFileIdentifier** ppFile, int bThreaded, int nFlags);

// 0x1002b5fe — unload a model by name (names ours; UnloadModel's entry).
LTRESULT sm_UnloadModel(CServerMgr* pMgr, char* pFilename, CFileIdentifier* pFile);
void sm_RemoveObjects(CServerMgr* pMgr, int bRemoveAll);
void sm_TermModels(CServerMgr* pMgr);
void sm_FreeList44(CServerMgr* pMgr);

// A second global CServerMgr pointer, in .data at 0x10058bf0, used by the
// old-API wrappers (GetTime, RemoveObject) instead of going through the
// interface object. SetupGlobals refreshes it and its class-manager
// companion (0x10058bf4) on every API entry.
extern CServerMgr* g_pServerMgr;
extern CClassMgr*  g_pClassMgr;

// 0x1002ac20 — remove an object from the world.
void sm_RemoveObject(CServerMgr* pServerMgr, HOBJECT hObj);

// The system interface (dsi_) init/term pair (0x1000dfb0, 0x1000e00e), and
// the five module pairs they bracket. The names are ours, taken from the
// module each one sits in: nothing here says what they do beyond the order
// they run in and the order they are undone in.
int  dsi_Init();
void dsi_Term();

void mem_Init();                             // 0x1000d268
void mem_Term();                             // 0x1000d2b1
void stream_Init();                          // 0x10035c2a
void stream_Term();                          // 0x10035c49
void fs_InitModule();                        // 0x10006e57
void fs_TermModule();                        // 0x10036d21 (empty in release)
void memtrack_Init();                        // 0x1000d402
void memtrack_Term();                        // 0x1000d423
void netbank_Init();                         // 0x10025fc6
void netbank_Term();                         // 0x10025fcd

// 0x1000dff4 and 0x1000e043 — load and unload ltmsg.dll (names ours).
int  dsi_LoadMessageModule();
void dsi_FreeMessageModule();

// 0x1005842c — whether this module was the one that started COM (name ours).
extern int g_bComInitialized;
// dsi_Init returns 0 on success, 1 when the resource module cannot load
// (ltmsg.dll — CreateServer turns that 1 into SI_CANTLOADRESOURCEMODULE),
// and anything else for a general failure.
int  dsi_Init();
void dsi_Term();



class MyServerInterface : public ServerInterface
{
public:
	// g_ServerInterface's initializer 0x1002bff1 (expanded: the two zero
	// stores, +0x08 first, then the vptr). Shipped also has an empty
	// non-virtual destructor (atexit thunk 0x1002c016, the vptr re-store
	// alone; the vtable at 0x1004e24c starts with SetAppHandler). Declared
	// below: with it this TU emits the shipped four-piece initializer group
	// (0x1002bfe7 call/jmp, 0x1002bff1 constructor, 0x1002c00a the atexit
	// registration, 0x1002c016 the vptr re-store). It used to be left out
	// because one more declaration here flipped tcpdriver.cpp's
	// HandleConnPacket (a header symbol-count threshold); measured
	// with it back, HandleConnPacket (0x10036ee5) stays identical.
	__forceinline MyServerInterface()
	{
		m_bInUpdate  = LTFALSE;
		m_pServerMgr = LTNULL;
	}
	~MyServerInterface() {}


	LTRESULT SetAppHandler(ServerAppHandler *pHandler);
	LTRESULT RunConsoleString(char *pStr);
	LTRESULT GetConsoleVar(char *pName, HCONSOLEVAR *hVar, char *pDefaultVal);
	LTRESULT GetVarValueFloat(HCONSOLEVAR hVar, float *val);
	LTRESULT GetVarValueString(HCONSOLEVAR hVar, char *pStr, uint32 maxLen);
	LTRESULT LoadConfigFile(char *pFilename);
	LTRESULT SaveConfigFile(char *pFilename);
	LTRESULT SendToServerShell(char *pInfo, int nLen);
	LTBOOL AddResources(char **pResources, uint32 nResources);
	LTBOOL LoadBinaries();
	FileEntry* GetFileList(char *pDirName);
	void FreeFileList(FileEntry *pList);
	LTRESULT OpenFile(char *pFilename, ILTStream **pStream);
	LTRESULT CopyFile(const char *pszSourceFile, const char *pszDestFile);
	LTBOOL SetGameInfo(void *pGameInfo, uint32 nGameInfoLen);
	int GetNumClients();
	LTBOOL GetClientName(int index, char *pName, int maxChars);
	LTBOOL GetClientInfo(int index, ClientInfo* pInfo);
	LTBOOL BootClient(uint32 dwClientID);
	LTRESULT GetClientPing(uint32 nClientId, float &ping);
	int GetErrorCode();
	void GetErrorString(char *pString, int maxLen);
	LTBOOL Update(long flags);
	LTBOOL InitNetworking(char *pDriver, uint32 dwFlags);
	LTBOOL GetServiceList(NetService *&pListHead);
	LTBOOL FreeServiceList(NetService *pListHead);
	LTBOOL SelectService(HNETSERVICE hNetService);
	LTBOOL UpdateSessionName(const char* sName);
	LTBOOL HostGame(NetHost &hostInfo);
	LTBOOL GetTcpIpAddress(char* sAddress, uint32 dwBufferSize, unsigned short &hostPort);
	LTRESULT SendTo(const void *pData, uint32 len, const char *sAddr, uint32 port);

public:
	CServerMgr* m_pServerMgr;                // 0x04 (0x00 is the vtable)
	LTBOOL      m_bInUpdate;                 // 0x08  set across CServerMgr::Update
};

// The one server interface, in .data at 0x10058bb0 (server.cpp). Its
// m_pServerMgr (0x10058bb4) doubles as CreateServer's already-instanced flag.
extern MyServerInterface g_ServerInterface;

#endif  // __SERVERMGR_H__
