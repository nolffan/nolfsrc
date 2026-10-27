// *********************************************************************** //
//
//  MODULE:   CLTNEW (.CPP) -- scratch file, NOT a drop-in TU
//
//  PURPOSE:  The twenty-one CLTClient vtable slots (0x004a14e0) that had no
//            source at all.  Every one of them lives in the SAME shipped
//            translation unit: 0x00406610..0x00407d40 is one contiguous run
//            of CLTClient method bodies with nothing else in it, and the
//            blocks below are ordered by ADDRESS so that the file mirrors
//            that source order.
//
//  HOW THE SLOTS ARE NAMED.  slot == iltcsbase.h's virtual index for 0..70,
//  slot 71 is ~ILTClient, slot == 71 + iltclient.h's virtual index after
//  that -- with LT2 iltclient.h idx 33..38 (the six that merely duplicate
//  ILTCSBase) absent from the shipped header, which shifts LT2 idx 39..44
//  onto slots 104..109 and ends the table there.  That mapping is
//  CONFIRMED independently, twice, from bytes:
//
//    * 0x004074f0 (slot 87) calls `*0x154(%eax)` on its OWN vtable.
//      0x154/4 == 85, and slot 85 is 0x004074a0, which lt2new1.cpp already
//      has as StopVideo (iltclient idx 14).  A function that decides a video
//      is not playing and stops it is IsVideoPlaying (idx 16 -> slot 87).
//    * 0x00407610 (slot 89) calls `*0x168(%eax)` on its own vtable with a
//      second argument of 0x80.  0x168/4 == 90, and iltclient.h says
//      "EndMessage: Just calls EndMessage2 with MESSAGE_GUARANTEED" --
//      idx 18 -> slot 89 calling idx 19 -> slot 90, with MESSAGE_GUARANTEED
//      == 0x80.
//
//  Every argument count below was checked against the shipped `ret $N`
//  before the name was accepted.  ONE enumerated row was wrong and is fixed
//  here: slot 91 (0x00406c90) is 580 bytes, not 316 -- the row was cut at
//  0x00406dcc, which is the instruction before a branch target INSIDE the
//  packet-write loop; the real extent is 0x00406c90..0x00406ed4 and the
//  twelve bytes after it are `nop`s.  Its `ret` is `ret $0xc` (three
//  arguments), which is what the enumeration's "13218" should have read.
//
//  FIVE of the twenty-one are ILTCSBase slots 31..55.  Those are PURE in
//  iltcsbase.h, so they are CLTClient's own bodies and nothing here is
//  shared with server.dll -- but CLTServer's twins are compiled into this
//  same image at /O2 (vtable 0x004a2ef4) and were used as controls:
//  0x0046e820 CPrint, 0x0046e860 GetPointContainers, 0x0046e8c0
//  GetContainerCode, 0x0046e8f0 FormatString.  src/engine/server/
//  servermessage.cpp already reconstructs all four; where the client
//  differs, the banner says how.
//
//  GROUND TRUTH: the retail lithtech.exe, cl 8447 /O2.
//
// *********************************************************************** //

#include <stdio.h>            // _vsnprintf
#include <stdarg.h>           // va_list / va_start / va_end

#include "ltbasedefs.h"
#include "ltcodes.h"
#include "iltclient.h"        // LTParticle, CanvasDrawFn, NodeControlFn
#include "iltcommon.h"        // ILTCommon -- slots 0x14, 0x3c, 0x40
#include "iltvideomgr.h"      // ILTVideoMgr -- slots 0x08, 0x0c
#include "de_error.h"         // the RETURN_ERROR family
#include "collection.h"       // CMoArray<uint8> / LAlloc / g_DefAlloc
#include "packet.h"           // CPacket (the queue entry's payload)


// ======================================================================= //
//  What this run reaches, and how each piece was pinned
// ======================================================================= //

// ---- the client object ------------------------------------------------- //
//
// src/engine/lithtech/clientmgr.h's CClientObject, restated here because
// this file is not a drop-in TU.  Only the type byte and the two per-type
// blocks these methods touch are named.  0xbc is the type byte in both this
// image's client and server halves (0x0046e8c0, CLTServer::GetContainerCode,
// reads the same displacement).
class CClientObjectView
{
public:
	uint8  m_pad_00[0xb8];
	// 0x0b8 -- clientmgr.h's m_nServerID: the server object this one
	// mirrors, or 0xffff when the object is the client's own.  A WORD, which
	// is what RemoveObject's `cmpw $0xffff,0xb8(%eax)` says.
	uint16 m_nServerID;                      // 0x0b8
	uint8  m_pad_ba[0xbc - 0xba];
	char   m_ObjectType;                     // 0x0bc  OT_*
	uint8  m_pad_bd[0x1fc - 0xbd];
	// 0x1fc/0x200 -- the model node hook and its user pointer.  Written
	// only by ModelNodeControl, and only for OT_MODEL.
	NodeControlFn m_NodeControlFn;           // 0x1fc
	void*         m_pNodeControlUser;        // 0x200
};

// A container's code is a uint16 at 0x270 -- the same displacement
// CLTServer::GetContainerCode reads in this image (0x0046e8dd).
class CClientContainerView
{
public:
	uint8  m_pad_00[0x270];
	uint16 m_ContainerCode;                  // 0x270
};

// A canvas lays its own fields out from 0x1a8, like every other object type
// (clientmgr.h: the camera's rect, the light's radius, the particle
// system's list all start there).
class CClientCanvasView
{
public:
	uint8       m_pad_00[0x1a8];
	CanvasDrawFn m_DrawFn;                   // 0x1a8
	void*        m_pUserData;                // 0x1ac
	LTFLOAT      m_fRadius;                  // 0x1b0
};

// clientmgr.h's CClientParticle: LTParticle plus the four fields the engine
// keeps past the m_pNext the game walks.  0x3c bytes, which is what fixes
// StructBank's element size and what puts m_pParticleBank at 0x1e4.
class CClientParticleView : public LTParticle
{
public:
	LTVector             m_Pos;              // 0x024
	LTFLOAT              m_Lifetime;         // 0x030
	LTFLOAT              m_TotalLifetime;    // 0x034
	CClientParticleView* m_pPrev;            // 0x038
};

// stdlith's StructBank (src/LT2/lithshared/stdlith/struct_bank.h), which is the
// field order src/engine/lithtech/clientsoundmgr.cpp already pinned for this
// image: the free head is the LAST dword, at +0x18, and the cache size is
// the third field, at +0x08.  AddParticle reads exactly those two.
struct StructBankView
{
	uint32 m_StructSize;                     // +0x00
	uint32 m_AlignedStructSize;              // +0x04
	uint32 m_CacheSize;                      // +0x08
	uint32 m_nPages;                         // +0x0c
	uint32 m_nTotalObjects;                  // +0x10
	void*  m_pPageHead;                      // +0x14
	void*  m_FreeListHead;                   // +0x18
};

// 0x00490c20 -- struct_bank.h's page grower, __cdecl (clientsoundmgr.cpp and
// clienttexmgr.cpp both name it at this address).
int sb_AllocateNewStructPage(StructBankView* pBank, unsigned long nAllocations);

// struct_bank.h's own body, verbatim.  It has to be `inline` here: /Ob1
// expands only what is marked inline or defined in a class body, and the
// shipped AddParticle has it expanded -- head, grow, RE-READ of the head,
// unlink -- with the caller's own null test left over on top of it.
inline void* sb_Allocate(StructBankView* pBank)
{
	void* pRet;

	pRet = pBank->m_FreeListHead;
	if (!pRet)
	{
		sb_AllocateNewStructPage(pBank, pBank->m_CacheSize);
		pRet = pBank->m_FreeListHead;
		if (!pRet)
			return 0;
	}

	pBank->m_FreeListHead = *(void**)pRet;
	return pRet;
}

// The particle system, from clientmgr.h, plus the two things this file adds:
// the running position bound at 0x230/0x23c (six `fcomps` against six
// consecutive floats, min triple then max triple) and the second counter at
// 0x22c that AddParticle increments beside m_nParticles.
class CClientParticleSystemView
{
public:
	uint8               m_pad_00[0x1a8];
	// A node embedded in the system, used as the list's end marker: its own
	// m_pPrev is at 0x1e0 and the bank pointer that follows the node lands
	// at 0x1e4, which is what proves the node is 0x3c bytes.
	CClientParticleView m_ParticleTail;      // 0x1a8
	StructBankView*     m_pParticleBank;     // 0x1e4
	uint8               m_pad_1e8[0x228 - 0x1e8];
	uint32              m_nParticles;        // 0x228
	uint32              m_Unknown22c;        // 0x22c  incremented with it
	LTVector            m_MinPos;            // 0x230
	LTVector            m_MaxPos;            // 0x23c
	uint8               m_pad_248[0x24c - 0x248];
	LTFLOAT             m_fParticleRadius;   // 0x24c  -> the particle's m_Size
};


// ---- the message layer ------------------------------------------------- //
//
// src/engine/server/servermsg.h's LMessageImpl and ServerMessage, restated
// on views.  Two things matter here and both are the server header's
// already: the owning queue entry is at LMessageImpl+0x08, and everything
// from ServerMessage+0x34 down IS a CPacket.  The object is 0x64 bytes,
// which is 0x50 + a 0x14-byte CMoArray<uint8, DefaultCache> exactly.

// The write-message vtable, only as far as the one slot this file calls.
// 0x8c is Verify (servermsg.h); every sender in both halves of the engine
// validates through it.
class IMessageWriteView
{
public:
	virtual LTRESULT _p00()=0; virtual LTRESULT _p01()=0; virtual LTRESULT _p02()=0; virtual LTRESULT _p03()=0;
	virtual LTRESULT _p04()=0; virtual LTRESULT _p05()=0; virtual LTRESULT _p06()=0; virtual LTRESULT _p07()=0;
	virtual LTRESULT _p08()=0; virtual LTRESULT _p09()=0; virtual LTRESULT _p10()=0; virtual LTRESULT _p11()=0;
	virtual LTRESULT _p12()=0; virtual LTRESULT _p13()=0; virtual LTRESULT _p14()=0; virtual LTRESULT _p15()=0;
	virtual LTRESULT _p16()=0; virtual LTRESULT _p17()=0; virtual LTRESULT _p18()=0; virtual LTRESULT _p19()=0;
	virtual LTRESULT _p20()=0; virtual LTRESULT _p21()=0; virtual LTRESULT _p22()=0; virtual LTRESULT _p23()=0;
	virtual LTRESULT _p24()=0; virtual LTRESULT _p25()=0; virtual LTRESULT _p26()=0; virtual LTRESULT _p27()=0;
	virtual LTRESULT _p28()=0; virtual LTRESULT _p29()=0; virtual LTRESULT _p30()=0; virtual LTRESULT _p31()=0;
	virtual LTRESULT _p32()=0; virtual LTRESULT _p33()=0; virtual LTRESULT _p34()=0;
	virtual LTRESULT Verify()=0;                          // 0x8c
};

// packet.h's CPacket itself.  server.dll (/O1) leaves the grow as a call
// to Insert2; lithtech.exe (/O2) EXPANDS WriteType, Append, Insert and
// Insert2 -- the two copy loops that reload m_pArray every iteration, the
// `SetCacheSize(GetCacheSize() - 1)` arm and the trailing `++m_nElements`
// -- and CALLS the two innermost helpers, BaseNew (0x0040c060) and
// _DeleteAndDestroyArray (0x00413050, thiscall on &m_Data).  That depth is
// what packet.h's own spelling gives (`m_Data.Append(0)`, the report through
// ReportWriteOverrun); a local copy that called Insert2 directly expanded
// one level too many and folded nothing (576 bytes against 580).
typedef CPacket CPacketView;

// The queue entry.  Nothing is added past the packet: servermsg.h's
// m_pData at 0x54 IS the array's m_pArray, and SendToServer stamps the
// message type into element zero of it.
typedef CPacketView ServerMessageView;

// servermsg.h's LMessageImpl, as far as SendToServer reaches it.
class LMessageView
{
public:
	void*              m_pVTable;            // 0x00
	void*              m_pObjectIO;          // 0x04
	ServerMessageView* m_pOwner;             // 0x08
};


// ---- the client manager ------------------------------------------------ //

// The object container the world hangs off.  CClientMgr+0x6c is
// clientmgr.h's m_World and slot 3 of its vtable is the point query:
// CLTServer's twin in this same image (0x0046e860) makes the identical
// `call *0xc(%edx)` on g_pServerMgr+0x210, which servermgr.h already names
// IObjectQuery::FindPointContainers.
class CObjectQueryView
{
public:
	virtual void _v00()=0;
	virtual void _v01()=0;
	virtual void _v02()=0;
	// slot 3 (+0x0c) -- calls pFn(pObject, pUser) once per container the
	// point is inside.
	virtual void FindPointContainers(LTVector* pPoint,
	                                 void (*pFn)(void* pObj, void* pUser),
	                                 void* pUser, int nFlags)=0;
};

// The gather record the client hands that query.  It is NOT the server's
// PointContainerGather: the server keeps the count as a MEMBER at +0x08 and
// puts the position last, the client puts the position FIRST and keeps only
// a POINTER to the count at +0x14 (the callback at 0x00407c50 does
// `mov 0x14(%esi),%eax; mov (%eax),%ecx`, and increments through it).
struct ClientPointContainerGather
{
	LTVector m_Position;                     // 0x00
	HOBJECT* m_pList;                        // 0x0c
	uint32   m_MaxListSize;                  // 0x10
	uint32*  m_pnElements;                   // 0x14
	uint32   m_Unused18;                     // 0x18 -- never written or read;
	                                         // it is what makes the frame 0x1c
};

// 0x00407c50 -- that callback.  __cdecl, two arguments, no result (it falls
// out of `pop %edi; pop %esi; ret` with eax whatever the last thing to touch
// it left, exactly like the server's sm_GatherContainers).
void cm_GatherContainers(void* pObj, void* pUser);

// The network manager sub-object at CClientMgr+0x50c.  One method is
// reached: 0x00456360, a __thiscall with three arguments (`ret $0xc`, and
// `mov %ecx,%ebp` on entry).  src/engine/lithtech/clientmgr2.cpp calls the
// same address net_SendTo from CClientShell::SendToServer with the shell's
// own connection id and a fixed 0x80.
class CClientNetMgrView
{
public:
	LTRESULT SendTo(ServerMessageView* pMsg, uint32 nConnID, uint32 flags);
	uint8    m_pad_00[0x100];
};

// The manager's main network driver, CNetMgr+0x100 -- which is
// CClientMgr+0x50c+0x100 == 0x60c, the displacement all four query methods
// load.  servermgr.h names the field m_pMainDriver and leaves slots 0x20,
// 0x24, 0x28 and 0x2c as _p20.._p2c; this run names those four, because the
// three neighbours of ours carry their own error tags: "StartQuery"
// (0x00406950, `*0x20`), "UpdateQuery" (0x004069b0, `*0x24`),
// "GetQueryResults" (0x00406a00, `*0x28`), and ours is "EndQuery"
// (0x00406a60, `*0x2c`) -- four consecutive slots in the header's order.
class CBaseDriverQueryView
{
public:
	virtual void _p00()=0; virtual void _p01()=0; virtual void _p02()=0; virtual void _p03()=0;
	virtual void _p04()=0; virtual void _p05()=0; virtual void _p06()=0; virtual void _p07()=0;
	virtual LTRESULT StartQuery(char* pInfo)=0;                              // 0x20
	virtual LTRESULT UpdateQuery()=0;                                        // 0x24
	virtual LTRESULT GetQueryResults(void** ppListHead)=0;                   // 0x28
	virtual LTRESULT EndQuery()=0;                                           // 0x2c
};

// The connection object at CClientMgr+0x150c (clientmgr.h).  +0x34 is the
// word SendToServer hands the net manager as its second argument, which is
// the same thing CClientShell keeps at its own +0x?? and calls nConnID.
class CClientConnectionView
{
public:
	uint8  m_pad_00[0x34];
	uint32 m_nConnID;                        // 0x34
};

class CClientMgrView
{
public:
	uint8                  m_pad_00[0x6c];
	// 0x06c -- the world, passed whole to the intersector and to the
	// physics mover elsewhere; here its vptr answers the point query.
	uint8                  m_World[0x508 - 0x6c];
	uint8                  m_pad_508[0x50c - 0x508];
	CClientNetMgrView      m_NetMgr;         // 0x50c .. 0x60b
	CBaseDriverQueryView*  m_pMainDriver;    // 0x60c  (CNetMgr's own +0x100)
	// 0x610/0x614 -- two string-resource module records, tried in that
	// order by FormatString (0x614 first).  Each is a POINTER to a record
	// whose first dword is the HINSTANCE: 0x00484780 dereferences it
	// (clientstreams.cpp).  Named by address; nothing says which module is
	// which.
	void*                  m_pStringModule610;   // 0x610
	void*                  m_pStringModule614;   // 0x614
	uint8                  m_pad_618[0x150c - 0x618];
	CClientConnectionView* m_pConnection;    // 0x150c
};

extern CClientMgrView* g_pClientMgr;         // 0x004b17e0


// ---- free functions and globals this run calls ------------------------- //

// 0x0041b880 -- the client console's print entry, __cdecl (colour, a word
// that is 0 at every site in this run, the line).  clientdsi.cpp names it.
void sub_41b880(uint32 nColor, int nUnknown, char* pMsg);

// 0x00484780 -- one formatted string out of a resource module.
// clientstreams.cpp reconstructed it and gives the signature: the module
// record, the message code, a POINTER to the caller's va_list, and a length
// out-parameter it always writes.
void* str_FormatMessage(void* pModule, int messageCode, va_list* pMarker, uint32* pLen);
#ifdef NOLFSRC_MODERN
// M-P9-02: a mod's cres.dll, resources only (clientdsi.cpp), or null
extern void* g_pP9ModStrings;
#endif

// 0x00484850 / 0x00484840 -- clientstreams.cpp's pair: wrap a raw
// FormatMessage buffer in an HSTRING, then LocalFree the buffer.
HSTRING sb_CreateStringRaw(char* pRaw);
void    str_FreeMessage(void* pRaw);

// 0x004207a0 / 0x00420670 / 0x00411bf0 -- clientmgr.h already declares the
// first two at these addresses (sub_4207a0 sets an object's position, with
// the fourth argument the "force it however small" flag; sub_420670 is
// reached with an object whose bounding radius has just changed).  The
// third has no name in the tree yet: it is __cdecl (pClientMgr, hObj), it
// answers an LTRESULT, and RemoveObject returns whatever it says.
void     sub_4207a0(CClientMgrView* pClientMgr, HLOCALOBJ hObj, LTVector* pPos, int bForce);
void     sub_420670(CClientMgrView* pClientMgr, HLOCALOBJ hObj);
LTRESULT sub_411bf0(CClientMgrView* pClientMgr, HOBJECT hObj);

// 0x004b4488 -- g_RenderStruct + 0x130, clientrender.cpp's
// m_GlobalLightDir.  Reached as that member of the one struct (a view cut to
// it), so it links to clientrender.cpp's g_RenderStruct; three dword loads
// then three dword stores is the memberwise copy of an LTVector and nothing
// else.
class RenderStruct
{
public:
	uint8    m_pad_000[0x130];
	LTVector m_GlobalLightDir;                  // 0x130  0x004b4488
};

extern RenderStruct g_RenderStruct;             // 0x004b4358


// ======================================================================= //
//  CLTClient
//
//  WHAT PINS THE CLASS.  Five displacements are read off `this` in this
//  run, and four of them are already established in the tree:
//    0x008  ILTCommon* m_pCommonLT   -- iltcsbase.h's protected block is
//           `ILTMath m_MathLT; ILTCommon *m_pCommonLT; ILTPhysics *; ...`,
//           so after the vptr those land at 0x04, 0x08, 0x0c, 0x10, 0x14
//           (lt2new4.cpp's arithmetic).  Confirmed here: slots 72/73 call
//           *0x3c and *0x40 through it, which are iltcommon.h's
//           GetPointStatus and GetPointShade -- the same two names the
//           slots have.
//    0x360  ILTVideoMgr* m_pVideoMgr } ltnew1.cpp's StartVideo pinned both
//    0x3c4  HVIDEO       m_hVideo    } of these.
//    0x3c0  CClientMgr*  m_pClientMgr  -- lt2new4.cpp.
//
//  NONE OF THESE IS DECLARED VIRTUAL, on purpose: a virtual and a
//  non-virtual member compile to the same bytes, and declaring them virtual
//  would make VC6 demand the other eighty-nine bodies.  The two calls the
//  shipped code makes through its OWN vtable go through the view below
//  instead, which is the same device src/engine/server/servermessage.cpp
//  uses (ILTServerSendView).
// ======================================================================= //

// Slots 85 and 90 of 0x004a14e0, reached as `call *0x154(%eax)` and
// `call *0x168(%eax)` off `this`.
class ILTClientSelfView
{
public:
	virtual void _v00()=0; virtual void _v01()=0; virtual void _v02()=0; virtual void _v03()=0;
	virtual void _v04()=0; virtual void _v05()=0; virtual void _v06()=0; virtual void _v07()=0;
	virtual void _v08()=0; virtual void _v09()=0; virtual void _v10()=0; virtual void _v11()=0;
	virtual void _v12()=0; virtual void _v13()=0; virtual void _v14()=0; virtual void _v15()=0;
	virtual void _v16()=0; virtual void _v17()=0; virtual void _v18()=0; virtual void _v19()=0;
	virtual void _v20()=0; virtual void _v21()=0; virtual void _v22()=0; virtual void _v23()=0;
	virtual void _v24()=0; virtual void _v25()=0; virtual void _v26()=0; virtual void _v27()=0;
	virtual void _v28()=0; virtual void _v29()=0; virtual void _v30()=0; virtual void _v31()=0;
	virtual void _v32()=0; virtual void _v33()=0; virtual void _v34()=0; virtual void _v35()=0;
	virtual void _v36()=0; virtual void _v37()=0; virtual void _v38()=0; virtual void _v39()=0;
	virtual void _v40()=0; virtual void _v41()=0; virtual void _v42()=0; virtual void _v43()=0;
	virtual void _v44()=0; virtual void _v45()=0; virtual void _v46()=0; virtual void _v47()=0;
	virtual void _v48()=0; virtual void _v49()=0; virtual void _v50()=0; virtual void _v51()=0;
	virtual void _v52()=0; virtual void _v53()=0; virtual void _v54()=0; virtual void _v55()=0;
	virtual void _v56()=0; virtual void _v57()=0; virtual void _v58()=0; virtual void _v59()=0;
	virtual void _v60()=0; virtual void _v61()=0; virtual void _v62()=0; virtual void _v63()=0;
	virtual void _v64()=0; virtual void _v65()=0; virtual void _v66()=0; virtual void _v67()=0;
	virtual void _v68()=0; virtual void _v69()=0; virtual void _v70()=0; virtual void _v71()=0;
	virtual void _v72()=0; virtual void _v73()=0; virtual void _v74()=0; virtual void _v75()=0;
	virtual void _v76()=0; virtual void _v77()=0; virtual void _v78()=0; virtual void _v79()=0;
	virtual void _v80()=0; virtual void _v81()=0; virtual void _v82()=0; virtual void _v83()=0;
	virtual void _v84()=0;
	virtual LTRESULT StopVideo()=0;                                          // 0x154, slot 85
	virtual void _v86()=0; virtual void _v87()=0; virtual void _v88()=0; virtual void _v89()=0;
	virtual LTRESULT EndMessage2(HMESSAGEWRITE hMessage, uint32 flags)=0;    // 0x168, slot 90
};

class CLTClient
{
public:
	// --- ILTCSBase's pure block, slots 31..55 --- //
	void        CPrint(char* pMsg, ...);                                     // 0x00407890  slot  39
	uint32      GetPointContainers(LTVector* pPoint, HOBJECT* pList,
	                               uint32 maxListSize);                      // 0x004078d0  slot  40
	LTBOOL      GetContainerCode(HOBJECT hObj, uint16* pCode);               // 0x00407950  slot  41
	HSTRING     FormatString(int messageCode, ...);                          // 0x00407980  slot  44
	LTRESULT    RemoveObject(HOBJECT hObj);                                  // 0x00407b60  slot  55

	// --- ILTClient's own, slot == 71 + iltclient.h index --- //
	LTRESULT    GetPointStatus(LTVector* pPoint);                            // 0x00407530  slot  72
	LTRESULT    GetPointShade(LTVector* pPoint, LTVector* pColor);           // 0x00407540  slot  73
	LTRESULT    SetGlobalLightDir(LTVector dir);                             // 0x00407200  slot  79
	LTRESULT    UpdateVideo();                                               // 0x004074d0  slot  86
	LTRESULT    IsVideoPlaying();                                            // 0x004074f0  slot  87
	LTRESULT    EndMessage(HMESSAGEWRITE hMessage);                          // 0x00407610  slot  89
	LTRESULT    SendToServer(IMessageWriteView& msg, uint8 msgID, uint32 flags);  // 0x00406c90  slot  91
	LTRESULT    GetAttachments(HLOCALOBJ hObj, HLOCALOBJ* inList, uint32 inListSize,
	                           uint32& outListSize, uint32& outNumAttachments);   // 0x004073a0  slot  92
	LTRESULT    SetObjectPos(HLOCALOBJ hObj, LTVector* pPos, LTBOOL bForce);  // 0x00406720  slot  94
	LTParticle* AddParticle(HLOCALOBJ hObj, LTVector* pPos, LTVector* pVelocity,
	                        LTVector* pColor, float lifeTime);               // 0x004067c0  slot  97
	LTRESULT    GetCanvasFn(HOBJECT hCanvas, CanvasDrawFn& fn, void*& pUserData);  // 0x00407020  slot  99
	LTRESULT    SetCanvasFn(HOBJECT hCanvas, CanvasDrawFn fn, void* pUserData);    // 0x00407090  slot 100
	LTRESULT    GetCanvasRadius(HOBJECT hCanvas, float& radius);             // 0x00407100  slot 101
	LTRESULT    SetCanvasRadius(HOBJECT hCanvas, float radius);              // 0x00407160  slot 102
	LTRESULT    ModelNodeControl(HOBJECT hObj, NodeControlFn fn, void* pUserData); // 0x00406fb0 slot 103
	LTRESULT    EndQuery();                                                  // 0x00406a60  slot 109

public:
	uint8            m_pad_0000[0x08];
	ILTCommon*       m_pCommonLT;            // 0x008
	uint8            m_pad_000c[0x360 - 0x0c];
	ILTVideoMgr*     m_pVideoMgr;            // 0x360
	uint8            m_pad_0364[0x3c0 - 0x364];
	CClientMgrView*  m_pClientMgr;           // 0x3c0
	HVIDEO           m_hVideo;               // 0x3c4
};

// ======================================================================= //
//  1.  0x00406720, 105 bytes -- slot 94, iltclient.h idx 23
//      LTRESULT SetObjectPos(HLOCALOBJ hObj, LTVector *pPos, LTBOOL bForce)
//
//  WHAT PINS IT
//    * `ret $0xc` -- three arguments, which is what idx 23 declares (the
//      default on bForce is a call-site thing and costs the callee nothing).
//    * BOTH `je`s go to ONE error block, so there is ONE check site and it
//      tests two things.  That is an `||`, not two statements: two
//      statements would give two REPORT_ERROR expansions and two literals.
//    * the name is pushed as `mov 0x004a7a68,%ecx; push %ecx` -- a POINTER
//      variable, not a literal -- and the code is reported as the string
//      "LT_INVALIDPARAMS" (0x004a77c8) rather than as "60".  Per de_error.h
//      that pair is RETURN_ERROR_NAMED, and the pointer at 0x004a7a68 reads
//      0x004a7a50, which is "CLTClient::SetObjectPos".
//    * `cmp $0x2,0x004b0ab0` -- debug level 2.
//    * the tail is `push bForce; push pPos; push hObj; push m_pClientMgr;
//      call 0x004207a0; add $0x10,%esp` then `xor %eax,%eax`, so the helper
//      is __cdecl with the manager first and the answer is LT_OK, not the
//      helper's (the helper returns void; clientmgr.h says so too).
// ======================================================================= //
LTRESULT CLTClient::SetObjectPos(HLOCALOBJ hObj, LTVector* pPos, LTBOOL bForce)
{
	if (!hObj || !pPos)
		RETURN_ERROR_NAMED(2, "CLTClient::SetObjectPos", LT_INVALIDPARAMS);

	sub_4207a0(m_pClientMgr, hObj, pPos, bForce);

	return LT_OK;
}


// ======================================================================= //
//  2.  0x004067c0, 385 bytes -- slot 97, iltclient.h idx 26
//      LTParticle* AddParticle(HLOCALOBJ, LTVector*, LTVector*, LTVector*, float)
//
//  WHAT PINS IT
//    * `ret $0x14` -- five arguments, and the last one is stored as a raw
//      dword into two adjacent float fields, which is the `float lifeTime`
//      of idx 26.  No other slot in this table takes five.
//    * `cmpb $0x6,0xbc(%esi)` -- OT_PARTICLESYSTEM.  The two failure exits
//      (null handle, wrong type) tail-merge onto ONE block at the very END
//      of the function, after the success return; that is the
//      positively-guarded form with the failure answer last, NOT
//      `if (!hObj || type != 6) return LTNULL;` (which hosts the merged
//      block before the body).
//    * the allocation is struct_bank.h's sb_Allocate expanded off
//      `0x1e4(%esi)`: head at +0x18, grow through 0x00490c20 with the cache
//      size at +0x08, head RE-READ, unlink.  The redundant `test %ecx,%ecx`
//      that follows the unlink is the CALLER's own null test on a value the
//      inline already proved non-null -- which is why that third failure
//      exit is a separate block (it has an extra `pop %edi` to do) instead
//      of merging with the other two.
//    * six `flds`/`fcomps` pairs against 0x230..0x244, `test $0x01,%ah` for
//      the first three (C0 -- strictly below) and `test $0x41,%ah` for the
//      last three (C0|C3 -- below-or-equal, so the source test is `>`).
//      Three consecutive floats then three more: a min triple and a max
//      triple, i.e. a running bound over the particle positions.
//    * the stores are integer moves (`mov (%edx),%eax; mov %eax,0x230(%esi)`)
//      -- VC6's float memory-to-memory copy, not an `fstp`.
//    * `movl $0x3f800000,0x18(%ecx)` is m_Alpha = 1.0f (LTParticle's m_Alpha
//      is at 0x18, after m_Velocity and m_Color).
//    * `mov 0x24c(%esi),%edx; mov %edx,0x1c(%ecx)` -- m_Size comes from the
//      system's particle radius, not from any argument.
//    * the link-in re-reads `0x38(%ecx)` AFTER storing through the tail
//      node, so the last statement is written as a member expression
//      (`pParticle->m_pPrev->m_pNext = pParticle`) and not off a cached
//      local: the compiler has to assume the tail store aliased it.
//    * two counters go up, 0x228 and 0x22c, read-read-inc-inc-store-store.
// ======================================================================= //
LTParticle* CLTClient::AddParticle(HLOCALOBJ hObj, LTVector* pPos, LTVector* pVelocity,
                                   LTVector* pColor, float lifeTime)
{
	CClientParticleSystemView* pSystem = (CClientParticleSystemView*)hObj;
	CClientParticleView*       pTail;
	LTParticle*                pRet;

	if (hObj && ((CClientObjectView*)hObj)->m_ObjectType == OT_PARTICLESYSTEM)
	{
		CClientParticleView* pParticle =
			(CClientParticleView*)sb_Allocate(pSystem->m_pParticleBank);

		// One result local set on both arms and returned once, with the
		// outer refusal a separate `return LTNULL`: that makes the inner
		// block single-exit, so `push %edi` (the bank) is shrink-wrapped
		// below the type test and the allocation failure gets its own
		// `pop %edi; xor %eax,%eax` exit (0x00406805), as shipped.  An early
		// `return LTNULL` here, or any shape sharing one exit with the
		// outer refusal, hoists the push to entry.
		if (!pParticle)
		{
			pRet = LTNULL;
		}
		else
		{
			if (pPos->x < pSystem->m_MinPos.x)
				pSystem->m_MinPos.x = pPos->x;
			if (pPos->y < pSystem->m_MinPos.y)
				pSystem->m_MinPos.y = pPos->y;
			if (pPos->z < pSystem->m_MinPos.z)
				pSystem->m_MinPos.z = pPos->z;

			if (pPos->x > pSystem->m_MaxPos.x)
				pSystem->m_MaxPos.x = pPos->x;
			if (pPos->y > pSystem->m_MaxPos.y)
				pSystem->m_MaxPos.y = pPos->y;
			if (pPos->z > pSystem->m_MaxPos.z)
				pSystem->m_MaxPos.z = pPos->z;

			pParticle->m_Pos      = *pPos;
			pParticle->m_Color    = *pColor;
			pParticle->m_Velocity = *pVelocity;
			pParticle->m_Alpha    = 1.0f;

			pParticle->m_Lifetime      = lifeTime;
			pParticle->m_TotalLifetime = lifeTime;

			pParticle->m_Size = pSystem->m_fParticleRadius;

			// The first read stays a member expression (`mov 0x1e0(%esi)`);
			// pTail named after it is the one `lea 0x1a8(%esi),%eax`
			// (0x00406909) that is both the stored m_pNext and the base of
			// the tail's m_pPrev store.
			pParticle->m_pPrev = pSystem->m_ParticleTail.m_pPrev;
			pTail = &pSystem->m_ParticleTail;
			pParticle->m_pNext = pTail;

			pTail->m_pPrev = pParticle;
			pParticle->m_pPrev->m_pNext = pParticle;

			pSystem->m_nParticles++;
			pSystem->m_Unknown22c++;

			pRet = (LTParticle*)pParticle;
		}

		return pRet;
	}

	return LTNULL;
}


// ======================================================================= //
//  3.  0x00406a60, 76 bytes -- slot 109, iltclient.h idx 44
//      LTRESULT EndQuery()
//
//  WHAT PINS IT
//    * bare `ret` after the error arm and a TAIL `jmp *0x2c(%edx)` on the
//      success arm, so no arguments and the driver's answer is the answer.
//    * the three functions immediately before it are the same six
//      instructions with 0x20, 0x24 and 0x28 in place of 0x2c, and their
//      error tags read "StartQuery", "UpdateQuery" and "GetQueryResults".
//      Ours reads "EndQuery" (0x004a7a94), pushed as a LITERAL -- so the
//      plain RETURN_ERROR, and the code prints as "73" (0x004a7168), which
//      is the numeric-stringize form and LT_NOTINITIALIZED.
//    * `cmp $0x1,0x004b0ab0` -- level 1, where most of this file uses 2.
//    * `mov 0x3c0(%ecx),%eax; mov 0x60c(%eax),%ecx` -- the driver is a
//      field of the MANAGER at 0x60c == CNetMgr's m_pMainDriver.
// ======================================================================= //
LTRESULT CLTClient::EndQuery()
{
	// One load, kept: the shipped code reads 0x60c once and reuses %ecx for
	// the test, the vtable fetch and the jump.
	CBaseDriverQueryView* pDriver = m_pClientMgr->m_pMainDriver;

	// Positive guard: shipped falls through `je` into the tail jump and
	// puts the report after it (0x00406a6d).
	if (pDriver)
		return pDriver->EndQuery();

	RETURN_ERROR(1, "EndQuery", LT_NOTINITIALIZED);
}


// ======================================================================= //
//  4.  0x00406c90, 580 bytes -- slot 91, iltclient.h idx 20
//      LTRESULT SendToServer(ILTMessage &msg, uint8 msgID, uint32 flags)
//
//  THE ROW WAS MIS-SIZED.  The enumeration says 316 bytes and an argument count
//  of "13218"; the bytes say 0x00406c90..0x00406ed4 (580) and `ret $0xc`.
//  0x00406dcd, where the row was cut, is the join of the packet-grow loop.
//
//  WHAT PINS IT
//    * `test %ebx,%ebx` on the first argument and then `call *0x8c(%eax)`
//      through it -- servermsg.h's IMessageWrite::Verify, the slot every
//      sender in both halves of the engine validates through.  Testing a
//      REFERENCE parameter for null is what `if (!&msg || ...)` gives, and
//      it is exactly what CLTServer::SendToClient does in server.dll.
//    * TWO check sites with DIFFERENT macros, which de_error.h says really
//      does happen: the LT_INVALIDPARAMS one pushes the name through the
//      pointer at 0x004a7aec and reports "LT_INVALIDPARAMS"
//      (RETURN_ERROR_NAMED), the LT_NOTINITIALIZED one pushes the literal
//      0x004a7af0 and reports "73" (RETURN_ERROR).  Both at level 2, and
//      the pointer and the literal are the same string,
//      "CLTClient::SendToServer".
//    * `mov 0x8(%ebx),%ecx; mov 0x54(%ecx),%edx; movb $0xb,(%edx)` -- the
//      owning queue entry is at LMessageImpl+0x08 (servermsg.h), 0x54 is its
//      raw payload pointer, and byte zero gets 11.  The server's twin passes
//      its type (0x0d) to sm_QueueMessageToClient instead; the client's send
//      path takes no type, so the client stamps it itself.
//    * everything from 0x00406cc2 to 0x00406de5 is CPacket::WriteType<uint8>
//      EXPANDED, grow loop and all: `m_Pos + 1 < m_MaxDataLen`, the rotated
//      `while (m_DataLen + 1 >= m_Data.GetSize())` with its condition
//      duplicated top and bottom, CMoArray<uint8>::Insert2 inlined inside
//      it, and the "ERROR: Packet write overrun!" / "CPacket::WriteType" /
//      LT_ERROR report on the else arm.  server.dll (/O1) leaves both the
//      template and the array insert as calls; /O2 expands both.
//    * the tail is `lea 0x50c(%eax),%ecx` -- a __thiscall on the manager's
//      CNetMgr sub-object -- with three arguments and no caller cleanup,
//      then `xor %eax,%eax`.  So the answer is LT_OK and not the send's.
// ======================================================================= //
LTRESULT CLTClient::SendToServer(IMessageWriteView& msg, uint8 msgID, uint32 flags)
{
	LMessageView* pMsg = (LMessageView*)&msg;

	if (!&msg || msg.Verify() != LT_OK)
		RETURN_ERROR_NAMED(2, "CLTClient::SendToServer", LT_INVALIDPARAMS);

	// The client-to-server message type, stamped straight into the packet
	// rather than handed to the sender.
	pMsg->m_pOwner->m_Data.m_pArray[0] = 11;

	pMsg->m_pOwner->WriteType(msgID);

	CClientConnectionView* pConn = m_pClientMgr->m_pConnection;

	if (pConn)
	{
		m_pClientMgr->m_NetMgr.SendTo(pMsg->m_pOwner, pConn->m_nConnID, flags);

		return LT_OK;
	}

	RETURN_ERROR(2, "CLTClient::SendToServer", LT_NOTINITIALIZED);
}


// ======================================================================= //
//  5.  0x00406fb0, 101 bytes -- slot 103, iltclient.h idx 32
//      LTRESULT ModelNodeControl(HOBJECT hObj, NodeControlFn fn, void *pUserData)
//
//  WHAT PINS IT
//    * `ret $0xc`, `cmpb $0x1,0xbc(%eax)` (OT_MODEL), two dword stores at
//      0x1fc and 0x200.  iltclient.h idx 32 is the only three-argument slot
//      in this neighbourhood that installs a callback and a user pointer.
//    * the name is the pointer at 0x004a7b94 -> "CLTClient::ModelNodeControl"
//      with the code reported symbolically, so RETURN_ERROR_NAMED, level 2.
//    * both guards jump to ONE block at the END, after `xor %eax,%eax;
//      ret $0xc` -- the AND-merged shape again.
// ======================================================================= //
LTRESULT CLTClient::ModelNodeControl(HOBJECT hObj, NodeControlFn fn, void* pUserData)
{
	CClientObjectView* pObj = (CClientObjectView*)hObj;

	if (hObj && pObj->m_ObjectType == OT_MODEL)
	{
		pObj->m_NodeControlFn    = fn;
		pObj->m_pNodeControlUser = pUserData;

		return LT_OK;
	}

	RETURN_ERROR_NAMED(2, "CLTClient::ModelNodeControl", LT_INVALIDPARAMS);
}


// ======================================================================= //
//  6..9.  0x00407020 / 0x00407090 / 0x00407100 / 0x00407160
//         slots 99..102, iltclient.h idx 28..31 -- the canvas four
//
//  WHAT PINS THEM
//    * they are consecutive in .text, consecutive in the table, and the
//      argument counts run 3, 3, 2, 2 -- which is iltclient.h's
//      GetCanvasFn / SetCanvasFn / GetCanvasRadius / SetCanvasRadius
//      exactly, in declaration order.
//    * all four test `cmpb $0xa,0xbc(%eax)` -- OT_CANVAS.
//    * the getters write through their argument (`mov %ecx,(%edx)`), so the
//      out-parameters are references; the fields are 0x1a8 (the function),
//      0x1ac (the user pointer) and 0x1b0 (the radius), which is the same
//      "each object type starts its own fields at 0x1a8" layout the camera,
//      the light and the particle system use.
//    * GetCanvasRadius/SetCanvasRadius move the radius as a raw dword, which
//      is VC6's float copy.
//    * SetCanvasRadius additionally calls `0x00420670(m_pClientMgr, hObj)`
//      __cdecl AFTER the store -- clientmgr.h already has that address --
//      and still answers LT_OK.
//    * ALL FOUR report through a name POINTER and a symbolic code, so all
//      four are RETURN_ERROR_NAMED at level 2.  SetCanvasRadius's pointer
//      (0x004a7bf0) is a DIFFERENT static from GetCanvasRadius's
//      (0x004a7bec) but they hold the SAME literal, 0x004a7bd0
//      "CLTClient::GetCanvasRadius".  Two statics with one pooled string is
//      what /GF gives for the same spelling written twice -- so the shipped
//      source really does name the wrong function in the setter, and the
//      copy-and-paste is reproduced below deliberately.
// ======================================================================= //
LTRESULT CLTClient::GetCanvasFn(HOBJECT hCanvas, CanvasDrawFn& fn, void*& pUserData)
{
	CClientCanvasView* pObj = (CClientCanvasView*)hCanvas;

	if (hCanvas && ((CClientObjectView*)hCanvas)->m_ObjectType == OT_CANVAS)
	{
		fn        = pObj->m_DrawFn;
		pUserData = pObj->m_pUserData;

		return LT_OK;
	}

	RETURN_ERROR_NAMED(2, "CLTClient::GetCanvasFn", LT_INVALIDPARAMS);
}

LTRESULT CLTClient::SetCanvasFn(HOBJECT hCanvas, CanvasDrawFn fn, void* pUserData)
{
	CClientCanvasView* pObj = (CClientCanvasView*)hCanvas;

	if (hCanvas && ((CClientObjectView*)hCanvas)->m_ObjectType == OT_CANVAS)
	{
		pObj->m_DrawFn    = fn;
		pObj->m_pUserData = pUserData;

		return LT_OK;
	}

	RETURN_ERROR_NAMED(2, "CLTClient::SetCanvasFn", LT_INVALIDPARAMS);
}

LTRESULT CLTClient::GetCanvasRadius(HOBJECT hCanvas, float& radius)
{
	CClientCanvasView* pObj = (CClientCanvasView*)hCanvas;

	if (hCanvas && ((CClientObjectView*)hCanvas)->m_ObjectType == OT_CANVAS)
	{
		radius = pObj->m_fRadius;

		return LT_OK;
	}

	RETURN_ERROR_NAMED(2, "CLTClient::GetCanvasRadius", LT_INVALIDPARAMS);
}

LTRESULT CLTClient::SetCanvasRadius(HOBJECT hCanvas, float radius)
{
	CClientCanvasView* pObj = (CClientCanvasView*)hCanvas;

	if (hCanvas && ((CClientObjectView*)hCanvas)->m_ObjectType == OT_CANVAS)
	{
		pObj->m_fRadius = radius;

		sub_420670(m_pClientMgr, hCanvas);

		return LT_OK;
	}

	// Not a typo of ours: the shipped static at 0x004a7bf0 holds the same
	// pooled literal the getter's static at 0x004a7bec does.
	RETURN_ERROR_NAMED(2, "CLTClient::GetCanvasRadius", LT_INVALIDPARAMS);
}


// ======================================================================= //
//  10.  0x00407200, 180 bytes -- slot 79, iltclient.h idx 8
//       LTRESULT SetGlobalLightDir(LTVector dir)
//
//  WHAT PINS IT
//    * `ret $0xc` with all three slots read as FLOATS and written back to:
//      the vector comes in BY VALUE, which is what idx 8 declares (its
//      getter, slot 78 at 0x004071d0, takes a reference and is `ret $4`).
//    * 0x004071d0 reads the same three globals -- 0x004b4488/8c/90 -- and
//      hands them out; clientrender.cpp already has that address down as
//      g_RenderStruct+0x130's getter, so this is its setter.
//    * the first block is ltvector.h's `Norm()` expanded verbatim:
//      `mag = Mag()` (the fsqrt), `if (mag == 0.0f) return` (fcoms against
//      0.0f at 0x004a14d8, `test $0x40,%ah` -- C3, equality -- and a jump
//      PAST the block), `inv = 1.0f / mag` (fdivrs against 1.0f at
//      0x004a1a08 -- a division into the reciprocal, which a plain
//      `dir /= mag` would spell as three fdivs), then three
//      `flds/fmul %st(1)/fstps`.  The `fstp %st(0)` at the join is the
//      inlined function's value being dropped.
//    * the second block recomputes the sum WITHOUT the sqrt and compares it
//      against 0.001f (0x004a1a54) with `test $0x01,%ah` -- C0, strictly
//      below -- so it is `MagSqr() < 0.001f`, and the body writes 1.0f, 0.0f,
//      0.0f into the three slots in x,y,z order, which is Init's order.
//    * the store out is three loads then three stores, i.e. the memberwise
//      copy of an LTVector, not three separate assignments (those would
//      interleave load and store).
// ======================================================================= //
LTRESULT CLTClient::SetGlobalLightDir(LTVector dir)
{
	dir.Norm();

	if (dir.MagSqr() < 0.001f)
		dir.Init(1.0f, 0.0f, 0.0f);

	g_RenderStruct.m_GlobalLightDir = dir;

	return LT_OK;
}


// ======================================================================= //
//  11.  0x004073a0, 112 bytes -- slot 92, iltclient.h idx 21
//       LTRESULT GetAttachments(HLOCALOBJ, HLOCALOBJ*, uint32, uint32&, uint32&)
//
//  WHAT PINS IT
//    * `ret $0x14` -- five arguments, and idx 21 is the only five-argument
//      declaration in ILTClient other than AddParticle (which is already
//      spoken for and takes a float).
//    * `mov 0x8(%ecx),%ecx` then `call *0x14(%edi)` -- m_pCommonLT, and
//      0x14 is iltcommon.h index 5, which is ILTCommon::GetAttachments with
//      the SAME five arguments in the same order.  The five pushes go out
//      right-to-left with hObj last, and the result is not touched
//      afterwards, so the forward is the return.
//    * the checks are on arguments 2, 4 and 5 -- inList and the two
//      out-references -- and NOT on hObj, which the common layer validates
//      itself.  All three land on one block at the end.
//    * name through the pointer at 0x004a7c10 -> "CLTClient::GetAttachments",
//      symbolic code, level 2: RETURN_ERROR_NAMED.
// ======================================================================= //
LTRESULT CLTClient::GetAttachments(HLOCALOBJ hObj, HLOCALOBJ* inList, uint32 inListSize,
                                   uint32& outListSize, uint32& outNumAttachments)
{
	if (inList && &outListSize && &outNumAttachments)
		return m_pCommonLT->GetAttachments(hObj, inList, inListSize,
		                                   outListSize, outNumAttachments);

	RETURN_ERROR_NAMED(2, "CLTClient::GetAttachments", LT_INVALIDPARAMS);
}


// ======================================================================= //
//  12.  0x004074d0, 21 bytes -- slot 86, iltclient.h idx 15
//       LTRESULT UpdateVideo()
//
//  WHAT PINS IT
//    * bare `ret`, one push, and the push is `this+0x3c4` -- ltnew1.cpp's
//      m_hVideo.  iltvideomgr.h slot 2 (0x08) is UpdateVideo(HVIDEO), and
//      the ILTClient slot that forwards to it is idx 15.
//    * `mov %ecx,%eax` before both loads: the two members are read off one
//      copy of `this`, which is what an ordinary pair of member expressions
//      gives when neither can alias the other.
// ======================================================================= //
LTRESULT CLTClient::UpdateVideo()
{
	return m_pVideoMgr->UpdateVideo(m_hVideo);
}


// ======================================================================= //
//  13.  0x004074f0, 49 bytes -- slot 87, iltclient.h idx 16
//       LTRESULT IsVideoPlaying()
//
//  WHAT PINS IT
//    * `call *0xc(%eax)` on m_pVideoMgr with m_hVideo -- iltvideomgr.h slot
//      3, GetVideoStatus.
//    * then `call *0x154(%eax)` on this object's OWN vtable with no
//      arguments.  0x154/4 == 85, and slot 85 (0x004074a0) is StopVideo,
//      already reconstructed in lt2new1.cpp.  A method that asks the video
//      manager for a status and stops the video when the answer is not
//      LT_OK is IsVideoPlaying, iltclient.h idx 16 -> slot 87.  Going
//      through the vtable rather than calling StopVideo directly is what
//      the source spells as a virtual call on itself.
//    * the two answers are 12 and 13, which ltcodes.h names VIDEO_PLAYING
//      and VIDEO_NOTPLAYING -- not LTRESULTs at all.  That is the whole
//      confirmation of the name.
//    * the success arm (status == LT_OK) is the FALL-THROUGH of a `jne`, so
//      the source tests the failing case; the returning-12 block sits
//      first, in the middle of the function, which is the ordinary
//      `if (x != LT_OK) { ...; return B; } return A;` layout inverted by
//      the compiler putting the short arm inline.
// ======================================================================= //
LTRESULT CLTClient::IsVideoPlaying()
{
	// GUARD DIRECTION. Shipped duplicates the epilogue for an early
	// `return VIDEO_PLAYING`; writing the failure as the guard gives the
	// opposite layout. /O2 preserves duplicated epilogues.
	if (m_pVideoMgr->GetVideoStatus(m_hVideo) == LT_OK)
		return VIDEO_PLAYING;

	((ILTClientSelfView*)this)->StopVideo();

	return VIDEO_NOTPLAYING;
}


// ======================================================================= //
//  14/15.  0x00407530 (16 bytes) and 0x00407540 (21 bytes)
//          slots 72 and 73, iltclient.h idx 1 and 2
//
//  WHAT PINS THEM
//    * `mov 0x8(%ecx),%ecx` -- m_pCommonLT -- then `call *0x3c` and
//      `call *0x40`, which are iltcommon.h indices 15 and 16:
//      GetPointStatus(LTVector*) and GetPointShade(LTVector*, LTVector*).
//      ILTClient idx 1 and 2 carry those two names with those two
//      signatures, and the argument counts agree (`ret $4`, `ret $8`).
//      Two independent lists agreeing on two adjacent names is the whole
//      proof; there is nothing else in either body.
//    * both are pure forwards with the callee's answer returned.
// ======================================================================= //
LTRESULT CLTClient::GetPointStatus(LTVector* pPoint)
{
	return m_pCommonLT->GetPointStatus(pPoint);
}

LTRESULT CLTClient::GetPointShade(LTVector* pPoint, LTVector* pColor)
{
	return m_pCommonLT->GetPointShade(pPoint, pColor);
}


// ======================================================================= //
//  16.  0x00407610, 21 bytes -- slot 89, iltclient.h idx 18
//       LTRESULT EndMessage(HMESSAGEWRITE hMessage)
//
//  WHAT PINS IT
//    * `ret $4`, and the only thing in the body is a call through this
//      object's own vtable at 0x168 with the handle and the literal 0x80.
//      0x168/4 == 90, the very next slot, and iltclient.h's comment on idx
//      18 says in as many words "Just calls EndMessage2 with
//      MESSAGE_GUARANTEED".  MESSAGE_GUARANTEED is 0x80.
// ======================================================================= //
LTRESULT CLTClient::EndMessage(HMESSAGEWRITE hMessage)
{
	return ((ILTClientSelfView*)this)->EndMessage2(hMessage, MESSAGE_GUARANTEED);
}


// ======================================================================= //
//  17.  0x00407890, 58 bytes -- slot 39, iltcsbase.h idx 39
//       void CPrint(char *pMsg, ...)
//
//  WHAT PINS IT
//    * a varargs member, so __cdecl with `this` as a hidden first stack
//      argument: `mov 0x8(%esp),%ecx` reads pMsg from the SECOND slot and
//      the function cleans nothing up (`ret`, not `ret $N`).  That is why
//      the enumeration reports zero popped arguments.
//    * CLTServer's twin in this same image at 0x0046e820 is the same
//      thirteen instructions with a different printer: same 0x1f4 frame,
//      same `push $0x1f3`, same `lea 0x200(%esp)` va_start, same
//      0x0049318e.  servermessage.cpp already has that body, including the
//      absent va_end.
//    * the only difference is the tail: the server pushes one argument to
//      dsi_ServerError, the client pushes three to 0x0041b880 --
//      `push msg; push $0; push $0xffffff` -- so the colour is white and
//      the middle word is the zero every site in the client passes.
//      `add $0x210,%esp` == 0x1f4 + four vsnprintf arguments + three of
//      these, which is how the argument count is fixed.
// ======================================================================= //
void CLTClient::CPrint(char* pMsg, ...)
{
	char    msg[500];
	va_list marker;

	// No va_end: neither this nor the server's twin ever had one.
	va_start(marker, pMsg);
	_vsnprintf(msg, sizeof(msg) - 1, pMsg, marker);

	sub_41b880(0xffffff, 0, msg);
}


// ======================================================================= //
//  18.  0x004078d0, 128 bytes -- slot 40, iltcsbase.h idx 40
//       uint32 GetPointContainers(LTVector *pPoint, HOBJECT *pList, uint32 maxListSize)
//
//  WHAT PINS IT
//    * `ret $0xc` and the same `call *0xc(%edx)` on the world container that
//      CLTServer::GetPointContainers makes in this image (0x0046e860, on
//      g_pServerMgr+0x210 rather than g_pClientMgr+0x6c).  Same four
//      arguments in the same order: the point, a callback, a gather record,
//      and a zero.  servermgr.h names that slot FindPointContainers.
//    * the callback (0x00407c50) is what fixes the record's layout, and it
//      is NOT the server's: it reads the list at +0x0c, the size at +0x10
//      and then DEREFERENCES +0x14 to get the count, incrementing through
//      it.  So the count is a separate local whose address the record
//      carries -- which is also why the frame is 0x1c and not 0x18: the
//      record is 0x18 bytes and the counter is a fourth local, hosted in
//      the dead pPoint parameter slot (`lea 0x2c(%esp)`
//      and `movl $0,0x30(%esp)` both name E0+4) with its reserved four
//      bytes left unused.  See the COIN FLIPS note at the foot of this file.
//    * the world pointer is `lea 0x6c(%eax)` and then TESTED, which is what
//      `g_pClientMgr->m_World` gives when m_World is an inline array: the
//      compiler cannot fold the test away because g_pClientMgr may be null.
//    * four guards, all `je` to ONE block that sits after the success
//      return, so the AND-merged form with `return 0;` last.
// ======================================================================= //
uint32 CLTClient::GetPointContainers(LTVector* pPoint, HOBJECT* pList, uint32 maxListSize)
{
	uint32                     nElements;
	ClientPointContainerGather gather;
	CObjectQueryView*          pQuery = (CObjectQueryView*)g_pClientMgr->m_World;

	if (pQuery && pPoint && pList && maxListSize)
	{
		// Position first and the zeroing last: this is the order that
		// schedules as 0x004078fc..0x00407927 (pList, max, x, y, &count,
		// count = 0, z) and puts pPoint in edi / the copy in ebx.
		gather.m_Position    = *pPoint;
		gather.m_pList       = pList;
		gather.m_MaxListSize = maxListSize;
		gather.m_pnElements  = &nElements;
		nElements = 0;

		pQuery->FindPointContainers(pPoint, cm_GatherContainers, &gather, 0);

		return nElements;
	}

	return 0;
}


// ======================================================================= //
//  19.  0x00407950, 48 bytes -- slot 41, iltcsbase.h idx 41
//       LTBOOL GetContainerCode(HOBJECT hObj, uint16 *pCode)
//
//  WHAT PINS IT
//    * `ret $8`, `cmpb $0x9,0xbc(%ecx)` (OT_CONTAINER), and a uint16 read
//      from 0x270 written through the second argument -- the identical
//      field CLTServer::GetContainerCode reads at 0x0046e8dd in this image.
//    * THE SHAPE IS NOT THE SERVER'S, and the pair is worth stating: the
//      server twin (also /O2, also in this image) leaves each `return
//      LTFALSE` inline where it falls -- it even reuses the null hObj in
//      %eax as the answer -- which puts one of them BETWEEN the type test
//      and the body.  The client's three failures are all `je` forward to a
//      single block AFTER `mov $0x1,%eax; ret $8`.  So the server's flat
//      `if (!x) return LTFALSE;` statements are the wrong shape here and the
//      AND-merged guard is the right one; the extra pCode check is the third
//      condition.  (The client cannot play the eax trick because pCode
//      wants that register.)
// ======================================================================= //
LTBOOL CLTClient::GetContainerCode(HOBJECT hObj, uint16* pCode)
{
	if (hObj && ((CClientObjectView*)hObj)->m_ObjectType == OT_CONTAINER && pCode)
	{
		*pCode = ((CClientContainerView*)hObj)->m_ContainerCode;

		return LTTRUE;
	}

	return LTFALSE;
}


// ======================================================================= //
//  20.  0x00407980, 164 bytes -- slot 44, iltcsbase.h idx 44
//       HSTRING FormatString(int messageCode, ...)
//
//  WHAT PINS IT
//    * varargs again, so `this` on the stack: `mov 0x1c(%esp),%edx` reads
//      the message code out of the SECOND slot and `lea 0x1c(%esp),%ecx`
//      (one slot further on) is va_start(marker, messageCode).
//    * 0x00484780 with four arguments is clientstreams.cpp's
//      str_FormatMessage, and the idiom that follows -- 0x00484850 then
//      0x00484840 on the same pointer, both __cdecl, both cleaned by one
//      `add $0x8,%esp` -- is the one that file documents: wrap the
//      FormatMessage buffer in an HSTRING, then LocalFree it.
//    * `mov %edi,0x8(%esp)` with %edi zeroed is va_end(marker), and it
//      appears TWICE, once per call.  CLTServer::FormatString has the same.
//    * TWO resource modules, 0x614 tried first and 0x610 second, and the
//      global is RE-READ between them (`mov 0x004b17e0,%eax` at 0x004079c6)
//      -- so the source names g_pClientMgr again rather than keeping it.
//    * the shape is two identical success tails cross-jumped into one: the
//      first block's `jne` goes FORWARD into the second block's copy, which
//      is the usual direction (of two identical return sites
//      VC6 keeps the second inline).  Both `return LTNULL`s merge onto the
//      block at the very end.  Writing it as a single tail after the two
//      attempts moves the merge; writing it nested, as below, does not.
// ======================================================================= //
HSTRING CLTClient::FormatString(int messageCode, ...)
{
	va_list marker;
	uint32  nLen;
	void*   pRaw;

	if (g_pClientMgr->m_pStringModule614)
	{
		va_start(marker, messageCode);
		pRaw = str_FormatMessage(g_pClientMgr->m_pStringModule614, messageCode,
		                         &marker, &nLen);
		va_end(marker);

		if (pRaw)
		{
			HSTRING hString = sb_CreateStringRaw((char*)pRaw);
			str_FreeMessage(pRaw);
			return hString;
		}
	}

	if (g_pClientMgr->m_pStringModule610)
	{
		va_start(marker, messageCode);
		pRaw = str_FormatMessage(g_pClientMgr->m_pStringModule610, messageCode,
		                         &marker, &nLen);
		va_end(marker);
#ifdef NOLFSRC_MODERN

		// M-P9-02: an id ours lacks, or holds only as the retail game's
		// placeholder ("<insert mission objective here>", "<Insert
		// Intelligence Item Text Here>"), is a mod's to fill in
		if (g_pP9ModStrings && (!pRaw || _strnicmp((char*)pRaw, "<insert ", 8) == 0))
		{
			void* pModRaw;
			va_start(marker, messageCode);
			pModRaw = str_FormatMessage(g_pP9ModStrings, messageCode, &marker, &nLen);
			va_end(marker);
			if (pModRaw)
			{
				if (pRaw) str_FreeMessage(pRaw);
				pRaw = pModRaw;
			}
		}
#endif

		if (pRaw)
		{
			HSTRING hString = sb_CreateStringRaw((char*)pRaw);
			str_FreeMessage(pRaw);
			return hString;
		}
	}

	return LTNULL;
}


// ======================================================================= //
//  21.  0x00407b60, 153 bytes -- slot 55, iltcsbase.h idx 55
//       LTRESULT RemoveObject(HOBJECT hObj)
//
//  WHAT PINS IT
//    * the literal at 0x004a7c48 says "CLTClient::RemoveObject" outright,
//      and both sites push it as a LITERAL with a NUMERIC code string, so
//      both are the plain RETURN_ERROR -- at level 1, not the 2 most of
//      this file uses.
//    * `cmpw $0xffff,0xb8(%eax)` -- clientmgr.h's m_nServerID, 0xffff for a
//      client-side object.  The second error's code string is at
//      0x004a7c44 and reads "26", which ltcodes.h gives as
//      LT_CANTREMOVESERVEROBJECT ("Can't remove a server object from the
//      client") -- the check and the code name the same rule.
//    * the two checks are separate statements, not an `||`: each has its own
//      REPORT_ERROR expansion with its own code and its own `mov $N,%eax;
//      ret $4`, and the second's block sits after the first's.
//    * the tail is `push hObj; push g_pClientMgr; call 0x00411bf0;
//      add $8; ret $4` with NO `xor %eax,%eax`, so the helper's answer IS
//      the answer.  Contrast CLTServer::RemoveObject, whose sm_RemoveObject
//      returns void and which then answers LT_OK.
// ======================================================================= //
LTRESULT CLTClient::RemoveObject(HOBJECT hObj)
{
	if (!hObj)
		RETURN_ERROR(1, "CLTClient::RemoveObject", LT_INVALIDPARAMS);

	if (((CClientObjectView*)hObj)->m_nServerID != 0xffff)
		RETURN_ERROR(1, "CLTClient::RemoveObject", LT_CANTREMOVESERVEROBJECT);

	return sub_411bf0(g_pClientMgr, hObj);
}


// ======================================================================= //
//  22.  0x00407ca0, 397 bytes -- cm_PointInsideWorldModel (name ours)
//
//  Moved here from src/engine/lithtech/cltapi.cpp (its BLOCK C), where it
//  was compiled against shim types; the notes come with it.
//
//  0x00407ca0 has exactly ONE caller, 0x00407c78, and that caller is
//  cm_GatherContainers -- the callback cltnew.cpp already declares at line
//  ~310 and never defines.  The caller settles both arguments:
//
//      407c76  push %edi          ; the LTObject the query handed us
//      407c77  push %esi          ; the gather record, whose first member is
//      407c78  call 0x407ca0      ; ClientPointContainerGather::m_Position
//
//  so __cdecl (LTVector* pPoint, LTObject* pObj) and the result is tested
//  `test %eax,%eax; je` -- an LTBOOL.  The caller only adds the object to
//  its list when this answers true, so this is "is the point inside this
//  container".
//
//  THE /O1 ORACLE NAMES THE BODY.  server.dll's world_PointInsideCB
//  (0x10012c9c, reconstructed in src/engine/server/servercommon.cpp) is the
//  same test written for the server:
//
//      MatVMul_H(&vLocal, &pObj->m_Transform230, &pQuery->m_vPoint);
//      if (!bsp_PointInside(pObj->m_pWorldBsp->GetRoot(), &vLocal)) ...
//
//  The client differs in three visible ways, all of them source facts:
//    * it has a CHEAP BOUNDING REJECT the server does not -- the squared
//      distance from the object's position against the squared diagonal of
//      the world BSP's box (0x00407ca9..0x00407d52);
//    * MatVMul_H is EXPANDED (0x00407d58..0x00407e01) where /O1 calls
//      0x1000f878.  That is the plain /O2-vs-/O1 split
//      for an SDK header inline, and clienttexmgr.cpp records the same pair;
//    * the BSP root is a direct field read `mov 0x9c(%esi),%edx`
//      (0x00407e05) rather than the server's virtual `call *0x4(%eax)`.
//    * the object-type and busy tests the server's callback makes through
//      the vtable are already done by 0x00407c50 on the client side.
//
//  ltobject_internal.h's LTObject already carries m_Pos at 0x18c,
//  m_pWorldBsp at 0x1a8 and WorldModelObject::m_Transform230 at 0x230, and
//  worldbsp.h's CWorldBsp already carries m_vBoxMin at 0xc4 and m_vBoxMax
//  at 0xd0 -- every displacement this function reads is an offset the tree
//  had already established on the SERVER side.  Only +0x9c is new.
// ======================================================================= //
class BSPNode;
class CClientWorldBspView
{
public:
	uint8        m_pad_00[0x9c];
	BSPNode*     m_pRoot;                    // 0x9c
	uint8        m_pad_a0[0xc4 - 0xa0];
	LTVector     m_vBoxMin;                  // 0xc4
	LTVector     m_vBoxMax;                  // 0xd0
};
class CClientWorldModelView
{
public:
	uint8                m_pad_00[0x54];
	void*                m_p54;              // 0x054 -- non-null: skip it
	uint8                m_pad_58[0xbc - 0x58];
	char                 m_ObjectType;       // 0x0bc
	uint8                m_pad_bd[0x18c - 0xbd];
	LTVector             m_Pos;              // 0x18c
	uint8                m_pad_198[0x1a8 - 0x198];
	CClientWorldBspView* m_pWorldBsp;        // 0x1a8
	uint8                m_pad_1ac[0x230 - 0x1ac];
	LTMatrix             m_Transform230;     // 0x230
};

// 0x00435540 -- worldsim.cpp's bsp_PointInside, __cdecl (BSPNode*, LTVector*).
int bsp_PointInside(BSPNode* pNode, LTVector* pPoint);

LTBOOL cm_PointInsideWorldModel(LTVector* pPoint, CClientWorldModelView* pObj);

// 0x00407c50, 71 bytes -- cm_GatherContainers, the GetPointContainers
// callback declared near the top.  Only a container (type 9 at +0xbc) whose
// +0x54 is null is considered; while the list has room, and when the point
// is inside it, it is appended through the record's count pointer.
void cm_GatherContainers(void* pObj, void* pUser)
{
	CClientWorldModelView*      pObject = (CClientWorldModelView*)pObj;
	ClientPointContainerGather* pGather;

	if (!pObject->m_p54 && pObject->m_ObjectType == OT_CONTAINER)
	{
		pGather = (ClientPointContainerGather*)pUser;
		if (*pGather->m_pnElements < pGather->m_MaxListSize)
		{
			if (cm_PointInsideWorldModel(&pGather->m_Position, pObject))
			{
				pGather->m_pList[*pGather->m_pnElements] = (HOBJECT)pObject;
				(*pGather->m_pnElements)++;
			}
		}
	}
}

// 0x00407ca0, 397 bytes.  Extent verified: 0x00407ca0..0x00407e2c with nops
// to 0x00407e2f; the only e8 reaching it is its own caller's; the two `ret`s
// at 0x00407e24 and 0x00407e2c are /O2 duplicated epilogues.
//
// SHAPE NOTES
//   * The frame is `sub $0xc` -- ONE twelve-byte local.  vDelta and vSize
//     never touch memory: their six components live in %st, except vSize.z,
//     which spills into the by-value slot at 0x00407d19 and is read back at
//     0x00407d2d.  That slot is the operator's dead by-value parameter and
//     is then reused a third time as vLocal (0x00407da8 onwards) -- the
//     "dead parameter slot hosts a temporary" pattern.
//   * Both subtractions copy their RIGHT operand into that slot as three
//     dwords off a `lea` of the member (0x00407ca9 and 0x00407cda), which is
//     `operator-`'s by-value argument and fixes the operand order:
//     `*pPoint - pObj->m_Pos` and `m_vBoxMax - m_vBoxMin`.
//   * Each squared sum emits its terms z, y, x -- a FLAT three-term sum, so
//     VEC_MAGSQR (which is the flat sum written out) and not an accumulator.
//   * `fcompp` at 0x00407d41 compares the SECOND sum against the first, and
//     `test $0x41,%ah; jne` leaves on C0|C3, i.e. continues only when
//     vSize's magnitude strictly exceeds vDelta's -- `distSqr < sizeSqr`.
//   * The two failures share ONE block at 0x00407e25 which sits after the
//     success return at 0x00407e1a, so the guards are AND-merged positives
//     with `return LTFALSE;` as the trailing statement.
//   * MatVMul_H's return value is discarded: `fstp %st(0)` at 0x00407e0c.
//
// A size-exact near (397/397, zero mnemonic blocks).  All that is left is in
// the expanded MatVMul_H (0x00407d58..0x00407df4): every row takes its
// pSrc->y and pSrc->z products in the opposite order to ours (offsets
// 4<->8 and m[k][1]<->m[k][2] swapped, x term unchanged).  That is VC6's
// tie-break between two operands of equal weight, not source order: the
// ltmatrix.h body is quoted verbatim.  Measured, none helps: `vLocal =
// m_Transform230 * *pPoint` (LTMatrix::operator*) gives 457 bytes; a
// `LTMatrix* pMat` local and named float locals for the two magnitudes
// stay 397 with 18 differing instructions.
//
// Second pass (here, against the real ltvector.h / ltmatrix.h): the move
// alone changes nothing (same 16 instructions).  What does reach the y/z
// order, in a standalone probe TU: the SOURCE order in which pPoint->z and
// pPoint->y are first mentioned -- `ext2(pPoint->z, pPoint->y);` ahead of
// MatVMul_H emits exactly shipped's z-first rows, `ext2(pPoint->y,
// pPoint->z)` does not.  Nothing in this function mentions z first, and
// these all stay near: the three MatVMul_H terms spelled x,z,y by hand
// (term order inside the sum is canonicalised; 18 differ via the pMat
// local); `pPoint->DistSqr(m_Pos) < (max - min).MagSqr()` (18 differ, x
// term flips); LTMatrix::Apply4x4(*pPoint, vLocal) (18, same flip); the
// delta's MAGSQR written z,y,x (16, unchanged); `vDelta = *pPoint;
// vDelta -= m_Pos` (423 bytes); vLocal block-scoped (39 differ, 9 blocks);
// `MAGSQR(vSize) > MAGSQR(vDelta)` (393 bytes).
LTBOOL cm_PointInsideWorldModel(LTVector* pPoint, CClientWorldModelView* pObj)
{
	LTVector             vLocal;
	CClientWorldBspView* pBsp = pObj->m_pWorldBsp;

	LTVector vDelta = *pPoint - pObj->m_Pos;
	LTVector vSize  = pBsp->m_vBoxMax - pBsp->m_vBoxMin;

	if (VEC_MAGSQR(vDelta) < VEC_MAGSQR(vSize))
	{
		MatVMul_H(&vLocal, &pObj->m_Transform230, pPoint);

		if (!bsp_PointInside(pBsp->m_pRoot, &vLocal))
			return LTTRUE;
	}

	return LTFALSE;
}


// ======================================================================= //
//  COIN FLIPS -- what could not be settled without a build
// ======================================================================= //
//
//  1-5 SETTLED BY BUILD (all four functions byte-identical on lithtech.exe):
//      SendToServer's grow is packet.h's CPacket used directly -- its
//      `m_Data.Append(0)` depth, not an explicit Insert2 call, is what
//      folds Insert2's index check and leaves _DeleteAndDestroyArray a
//      call.  GetPointContainers' 0x1c frame is a fifth record field
//      (m_Unused18), and its stores are position-first, count-zero last.
//      AddParticle names pTail after the first tail read, and returns
//      through one result local.
//
//  6.  SendToServer keeps m_pClientMgr in one register across the
//      connection test and the `lea 0x50c`.  Written above as two member
//      expressions off `this`; if VC6 reloads 0x3c0, the fix is
//      `CClientMgrView* pMgr = m_pClientMgr;` at the top of the tail block.
//
//  7.  FormatString's two locals.  The frame is exactly eight bytes with
//      the va_list low and the length high; scope and not
//      declaration order assigns slots, so the order written above (the
//      server's) should be indifferent -- but if it is not, swap them.
