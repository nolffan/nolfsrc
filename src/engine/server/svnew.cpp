// server.dll — functions with no reconstruction anywhere else (recon). /O1.
//
// This file is a HOLDING PEN, not a translation unit the shipped build had.
// Every function below carries a MOVE-ME banner naming the file it belongs
// beside; each was reconstructed here without touching the existing sources.
// Before any of it is
// counted, read the two warnings:
//
//   * an isolated harness TU can invent a residual —
//     VC6's decision to merge two identical blocks depends on what else in
//     the TU uses the string literals they push. Nothing here pushes a
//     string literal, which is why the risk is judged acceptable; it is not
//     zero.
//
//   * `src/engine/server/*.cpp` is compiled a SECOND time into lithtech.exe
//     at /O2. A new file is picked up by the SERVER build only until the
//     client file list names it. Nothing below changes an existing header or
//     an existing .cpp, so no already-matched function in either image moves.
//     The moment a function here is moved to its real home that stops being
//     true and the client side has to be re-measured.
//
// Layout views: where the owning class is not identified, the function is
// written against a local view class carrying the exact offsets the shipped
// bytes read. That is this project's standing idiom (ltobject_internal.h,
// tcpdriver.cpp, objbank.cpp all do it) and it costs nothing in bytes — a
// member's offset is all the codegen sees.
#include "servermgr.h"
#include "objbank.h"


// ======================================================================= //
//  0x1003d1c0 / 0x1003d170 / 0x1003d1f0 — the struct-bank entry points.
//  Declared by servermgr.h; sb_FreeChunks is not, so it is repeated here in
//  the same terms filemgr.cpp and fsstream.cpp use.
// ======================================================================= //
void sb_FreeChunks(StructBank* pBank, int bAll);

// 0x1001457f — servermath.cpp's basis-vector builder.
void math_GetAxisBasisVectors(uint32 iBasis, LTVector* pDir,
                              LTVector* pOut1, LTVector* pOut2);



// *********************************************************************** //
//  MOVE-ME: worldbsp.cpp / commonmodel.cpp — the world-geometry side.
//
//  Two slots of one vftable: 0x1004d344 and 0x1004d360, seven slots apart.
//  The first body sits immediately after CWorldBsp::GetPolyRefs (0x1000af37,
//  four bytes) and the second is 0x169c bytes lower, next to
//  w_ReadWorldLights, so the class straddles at least two shipped
//  translation units and neither of the two candidate homes is settled.
//  What IS settled is every offset each one reads.
// *********************************************************************** //

// The fields the two slots touch, at the offsets the shipped bytes name.
// 0x34/0x38 are exactly ltobject_internal.h's WorldBspData (m_pPlane,
// m_nFlags); 0x68..0x74 are a centre and a radius. Whether those are one
// class or two is the open question — the vftable says one.
class CGeomView
{
public:
	virtual void _v00() = 0;

	// 0x1000af3b — vftable 0x1004d344: worldbsp.cpp's CWorldSurface.
	virtual void GetAxisBasisVectors(LTVector* pOut1, LTVector* pOut2);
	// 0x1000989f — vftable 0x1004d360: commonmodel.cpp's CWorldLight.
	virtual void GetDims(LTVector* pMin, LTVector* pMax);

public:
	uint8    m_pad_004[0x34 - 0x04];
	// The polygon's plane. Its first twelve bytes are the normal, which is
	// why the shipped code hands the plane pointer straight to a routine
	// that wants an LTVector* — `push 0x34(%ecx)`, no `lea`.
	LTPlane* m_pPlane;                       // 0x34
	// The basis index is the top three bits: `shr $0x1d` on the whole word,
	// unsigned (a signed field would `sar`).
	uint32   m_nFlags;                       // 0x38
	uint8    m_pad_03c[0x68 - 0x3c];
	LTVector m_vCentre;                      // 0x68
	float    m_fRadius;                      // 0x74
};

// 0x1000af3b (29 bytes) is defined in worldbsp.cpp, as the one virtual of
// CWorldSurface (its one-slot vtable 0x1004d344); a second body here would
// put a second copy of it into the linked DLL.

// 0x1000989f (114 bytes), the bounding box of the bounding sphere, is
// defined in commonmodel.cpp as CWorldLight::GetDims: the vftable
// 0x1004d354 its slot sits in (0x1004d360) is the one CWorldLight's
// constructor 0x10009878 stores.



// *********************************************************************** //
//  MOVE-ME: commonmodel.cpp (the world-polygon side).
//
//  0x10007d8d (24 bytes), called from the loop at 0x10007cf7 immediately
//  above it. That caller does `lea 0x54(%esi),%ebx`, calls this, and then
//  `lea (%eax,%eax,2),%eax; lea (%ebx,%eax,8),%edx` — the return value times
//  twenty-four added to this+0x54. So the answer is a COUNT of the
//  twenty-four-byte records at +0x54, and ltobject_internal.h already names
//  that run: WorldPolyOwner::m_Vertices, with m_nVertices at +0x50.
//
//  What the header does not carry is the pointer at +0x4c or the second word
//  at +0x52, and this function is the only thing in the image that reads
//  either. Its shape says what they are for: when the pointer still aims at
//  the inline run the count is the plain one, and when it has been moved
//  away a second count is added. Both names are ours.
// *********************************************************************** //

struct WorldPolyVertexView
{
	LTVector* m_pPos;                        // 0x00
	uint8     m_pad_04[0x18 - 0x04];
};

class CWorldPolyOwnerView
{
public:
	uint32 GetVertexCount();                 // 0x10007d8d

	uint8  m_pad_000[0x4c];
	// Normally &m_Vertices[0]; a polygon whose vertex run has been re-homed
	// points it elsewhere and then carries the extra count below.
	WorldPolyVertexView* m_pVertexRun;       // 0x4c
	uint16 m_nVertices;                      // 0x50
	uint16 m_nExtraVertices;                 // 0x52
	WorldPolyVertexView  m_Vertices[1];      // 0x54
};

// The arm order is the whole content of this function, and it is not the
// obvious spelling. The shipped `je` jumps FORWARD to the one-term return
// and the sum falls through, which is the negated-condition layout of a
// guard on `!=` (arm order tells you which branch the source
// guarded). Written the natural way round —
//     if (m_pVertexRun == m_Vertices) return m_nVertices;
//     return m_nVertices + m_nExtraVertices;
// — VC6 emits `jne` to the sum instead, which is one byte in the same place
// and a different function.
//
// COIN FLIP: the two `movzwl` come out 0x52 first, then 0x50, then `add`.
// `m_nVertices + m_nExtraVertices` is written because it is the reading that
// matches the names; if the operands come out transposed the fix is
// `m_nExtraVertices + m_nVertices` and nothing else moves.
uint32 CWorldPolyOwnerView::GetVertexCount()
{
	if (m_pVertexRun != m_Vertices)
		return m_nVertices + m_nExtraVertices;

	return m_nVertices;
}



// *********************************************************************** //
//  MOVE-ME: the three global banks — one per file named below.
//
//  Each is a file-scope bank object, and each ships as FOUR functions:
//
//      _$E   `call <ctor body>; jmp <atexit thunk>`      10 bytes
//      ctor  the constructor, fully expanded             25..30 bytes
//      _$F   `push <dtor body>; call atexit; pop; ret`   12 bytes
//      dtor  the destructor, fully expanded              35 bytes
//
//  Only the two middle rows are the work here; the thunks come with them.
//  `__forceinline` on BOTH is what expands them: VC6
//  at /O1 never expands a constructor into the generated initializer without
//  it, and the destructors here are expanded in exactly the same way (the
//  atexit-registered function IS the destructor body, with the global's
//  address folded into every store and no `this` register), so the same
//  spelling is used for both. fsstream.cpp's two stream banks are the
//  established precedent for the constructor half.
//
//  The three vtable addresses are objbank.cpp's own table of banks found by
//  sweeping .rdata:
//      0x1004d638  0x18-byte records   Init 0x1000dea0  dtor 0x1000debb
//      0x1004d8dc  0x20-byte records   Init 0x10015b4a  dtor 0x10015b9d
//      0x1004e8b4  0x0c-byte records   Init 0x10034e7b  dtor 0x10034e96
//  so all three are the same bank template objbank.cpp already instantiates.
//  What is new is that each has a CONSTRUCTOR of its own, and the three
//  constructors differ: the 0x18 bank grows sixty-four records at a time
//  through sb_Init2 while the other two take sb_Init's default. Both are
//  objbank.h's template's constructors (with and without a grow size); the
//  0x18 bank below is now that template, and the other two are still
//  classes of their own here.
//
//  Placement, from where the four-function groups land in .text:
//      0x1000d95d  between dsi.cpp's con_Printf and dhash.cpp's dhash_Create,
//                  and the 0x18 bank's own Init/dtor (0x1000dea0/0x1000debb)
//                  sit just past dhash_HashWord — so this is dhash.cpp's
//                  element bank, DEFINED AT THE TOP OF dhash.cpp (COMDAT
//                  order is source order, and its group precedes
//                  dhash_Create).
//      0x10015508  between localdriver.cpp's CLocalDriver::_p74 and
//                  lthread.cpp's LCriticalSection constructor, and the 0x20
//                  bank's Init/dtor are in lthread's range — servermgr.h
//                  already names the class LThreadMessageBank. Top of
//                  lthread.cpp.
//      0x10031da5  between smgr.cpp's CQueuedMsg::Release and
//                  spritecontrol.cpp's sm_ClaimObjectID. The 0x0c bank's own
//                  Init/dtor are at 0x10034e7b, far from both, so the file is
//                  NOT settled — smgr.cpp is the better guess only because
//                  the function before it is smgr's.
//
//  Vtable slot counts: only the destructor slot is exercised, so the base is
//  modelled the way fsstream.cpp models the identical 0x1004d260 base — one
//  defined, empty virtual destructor. It has to be DEFINED, not declared, or
//  the derived destructor calls it instead of inlining its vptr store.
// *********************************************************************** //

class CGlobalBankBase
{
public:
	virtual ~CGlobalBankBase() {}
};

// ---- dhash.cpp: the hash-element bank (0x1004d638) ---------------------- //

// 0x100583e8 — objbank.h's bank over servermgr.h's hash elements, so its
// vtable (0x1004d638) is the template's: 0x1000d967 (30 bytes) is the constructor expanded, 0x18-byte
// elements, a cache of thirty-two and a grow of sixty-four, four arguments
// so sb_Init2 and one batched `add $0x10,%esp`; 0x1000d991 (35 bytes) the
// destructor expanded, the chunks going back between the two vptr stores.
// Slots 1..3 are the shared five-byte forward 0x1000de9b (`jmp 0x1000ed6f`,
// see servermessage.cpp's CXferFileBank: the element's LTLink has a
// constructor, so the typed allocation is not expanded into the slot), the
// shared Free 0x10015ac1 and Init 0x1000dea0.
static CObjectBank<CHashElement> g_HashElemBank(0x40);

// ---- lthread.cpp: the thread-message bank (0x1004d8dc) ------------------ //

class CThreadMsgBank : public CGlobalBankBase
{
public:
	// 0x10015512 (28 bytes) — 0x20-byte elements, cache of thirty-two, and
	// the grow left at sb_Init's default: three arguments, `add $0xc,%esp`.
	__forceinline CThreadMsgBank() { sb_Init(&m_Bank, 0x20, 0x20); }
	// 0x1001553a (35 bytes).
	__forceinline ~CThreadMsgBank() { sb_FreeChunks(&m_Bank, 1); }

	StructBank m_Bank;                       // +0x04
	uint32     m_Unknown20;                  // +0x20
};

// 0x10058700. Not static: this IS lthread.h's g_ThreadMessageBank (the
// same shipped address -- 0x10015674 and 0x100156e7 in lthread.cpp's range
// push/load it), reached under that name through an alias in
// link/aliases.rsp until the definition moves to lthread.cpp.
CThreadMsgBank g_ThreadMsgBank;

// ---- smgr.cpp (unsettled): the 0x0c-record bank (0x1004e8b4) ------------ //

// The element is 0x0c bytes, an LTLink: this is the bank packet.cpp's
// sm_CreateInterObjectLink draws its list nodes from (`mov $0x10058bc0,%edi`
// at 0x10013b8f, then `call 0x1000ed6f`) and sm_BreakInterObjectLink frees them back to (its free
// list 0x10058bdc is this object's +0x1c), so it carries that file's name.
// smgr.cpp's CObjQueueBank / g_ObjQueueBank and smgrnew.cpp's are the same
// object. Defined once, here, with its initialiser group.
class CLinkNodeBank : public CGlobalBankBase
{
public:
	// 0x10031daf (28 bytes).
	__forceinline CLinkNodeBank() { sb_Init(&m_Bank, 0x0c, 0x20); }
	// 0x10031dd7 (35 bytes).
	__forceinline ~CLinkNodeBank() { sb_FreeChunks(&m_Bank, 1); }

	StructBank m_Bank;                       // +0x04
	uint32     m_Unknown20;                  // +0x20
};

// 0x10058bc0. Not static: packet.cpp reaches it by name.
CLinkNodeBank g_LinkNodeBank;



// *********************************************************************** //
//  MOVED: the initializer group at 0x1002bfe7 / 0x1002bff1 / 0x1002c00a /
//  0x1002c016 is g_ServerInterface's (0x10058bb0, MyServerInterface: its
//  vtable 0x1004e24c holds 0x1002c021 RunConsoleString, 0x1002c06f
//  GetConsoleVar, ...). The object is now defined once, in servermgr.cpp, and
//  the two stores and the vptr restore are MyServerInterface's inline
//  constructor and destructor (servermgr.h). The stand-in CServerGlobalObj
//  that stood here was a second definition of the same shipped global.
// *********************************************************************** //



// *********************************************************************** //
//  MOVE-ME: servermessage.cpp, beside the rest of the xfer_* family.
//
//  Four functions of the client file-transfer state. servermessage.cpp
//  already carries CClientFiles, CXferFile and CXferFileBank with every
//  offset used below; the class views are repeated here rather than moved so
//  that this file changes nothing.
// *********************************************************************** //

// servermgr.h's CUnknownA14 is the same 0x18-byte stdlith object: its
// constructor is 0x1003d500 and its destructor 0x1003d580, both above the
// app-region cut, so both are declared and neither is defined.
class CXferBaseView
{
public:
	CXferBaseView();                         // 0x1003d500
	~CXferBaseView();                        // 0x1003d580

	uint8 m_pad[0x18];
};

// The record bank at CClientFiles+0x18. objbank.cpp's sweep lists its vtable
// (0x1004d648) as the 0x1c-byte instantiation, and 0x1c is sizeof(CXferFile)
// rounded up — which is the independent check on this constructor.
//
// The constructor is defined in the class body so VC6 expands it into
// CClientFiles' own: a vptr store plus one call is inside /Ob1's gate
// (a constructor inlines at vptr + one store + call).
class CXferFileBankView : public CGlobalBankBase
{
public:
	CXferFileBankView() { sb_Init(&m_Bank, 0x1c, 0x20); }
	~CXferFileBankView() { sb_FreeChunks(&m_Bank, 1); }

	StructBank m_Bank;                       // +0x04
	uint32     m_Unknown20;                  // +0x20
};

class CXferFileView
{
public:
	LTLink m_Link;                           // 0x00
	uint32 m_nFileID;                        // 0x0c
};

// Only the head of the class matters to the four functions here; the rest is
// servermessage.cpp's.
class CClientFilesView : public CXferBaseView
{
public:
	CClientFilesView();                      // 0x1000e5a2
	~CClientFilesView();                      // 0x1000e5ec

	CXferFileBankView m_FileBank;            // 0x18
	LTLink            m_ListHead;            // 0x3c
};

void xfer_FreeAllFiles(CClientFilesView* pState);      // 0x1000e857
void xfer_FinishTransfer(CClientFilesView* pState);    // 0x1000e8ba

// 0x1000e5a2 (35 bytes). The body is EMPTY: everything the shipped function
// does is the base constructor's call and the bank member's expansion, in
// declaration order, and `mov %esi,%eax` on the way out is the constructor's
// own return of `this`. Note there is no vptr store for CClientFiles itself
// — the class is not polymorphic, which is what the missing store says.
CClientFilesView::CClientFilesView()
{
}

// 0x1000e5ec (39 bytes). Equally empty, and the order is the destructor's:
// the member first (its own vptr, sb_FreeChunks, the base bank vptr) and the
// base class last, as a call because CXferBaseView's destructor is out of
// line.
CClientFilesView::~CClientFilesView()
{
}

// 0x1000e5c5 (39 bytes) — tear the whole transfer state down and release it.
//
// The two `pop ecx` of the two fixed-prototype cdecl calls are batched and
// the null test is scheduled between them, which is cl 8447 doing its
// usual pre-8804 cleanup merging — not evidence about the
// prototypes. The tail is `delete`, not a written-out free: the guarded pair
// of destructor-then-release with the guard around BOTH is exactly what
// `delete p` emits, and an explicit `if (p) { ... }` around the same two
// statements is the same bytes only by accident of there being nothing else
// in the block.
void xfer_Term(CClientFilesView* pState)
{
	xfer_FreeAllFiles(pState);

	xfer_FinishTransfer(pState);

	delete pState;
}

// 0x1000e9c7 (39 bytes) — the record for one file id, or null.
//
// This is xfer_FindPendingFile (0x1000eaa2, already byte-identical in
// servermessage.cpp) with the flag test replaced by an id compare, and the
// two are worth reading side by side because the loop comes out DIFFERENTLY
// and the source shape is the same. FindPendingFile's back edge is an
// unconditional `jmp` to a shared head test; this one is rotated — the head
// test is peeled to the top, the back edge is `jne`, and the preheader that
// creates is where the `movzwl` of the parameter is hoisted to.
//
// The reason: widening a compare buys the rotation. The
// parameter is a uint16 and the field is a dword, so the comparison is a
// dword compare with a zero-extended operand — a loop invariant with
// somewhere to live — where FindPendingFile's `m_Flags & 2` is recomputed
// each trip and needs no preheader. Nothing about the loop was spelled
// differently.
//
// Operand order: `cmp %edx,0xc(%eax)` puts the member first, so the source
// is `pFile->m_nFileID == nFileID` and not the other way round.
CXferFileView* xfer_FindFileByID(CClientFilesView* pState, uint16 nFileID)
{
	LTLink* pCur = pState->m_ListHead.m_pNext;

	while (pCur != &pState->m_ListHead)
	{
		CXferFileView* pFile = (CXferFileView*)pCur->m_pData;

		if (pFile->m_nFileID == nFileID)
			return pFile;

		pCur = pCur->m_pNext;
	}

	return LTNULL;
}



// *********************************************************************** //
//  MOVED: 0x10016b3e, CCollElem24Rec's owner-taking constructor, now lives in
//  collection.cpp beside the default constructor, on the real class (the
//  view here named array constructors and a setter nothing defined).
// *********************************************************************** //



// *********************************************************************** //
//  MOVE-ME: commonmodel.cpp, beside Model::GetTotalAnimCount (0x1001767a),
//  which is the function immediately above it in the image.
//
//  0x100176b7 (37 bytes) — how many of this model's own animation entries
//  belong to it rather than to a child. commonmodel.h already carries the
//  collection (`CMoArray<AnimInfo, NoCache> m_AnimArray` at 0x164, so array
//  at 0x168 and count at 0x16c) and AnimInfo's first member (`ModelAnim*
//  m_pAnim`); what it does not name is ModelAnim's +0x20, which is the only
//  thing this function reads through it and is a back-pointer to the Model
//  that owns the animation (name ours).
//
//  Written against a view so the header does not move. The subscripted form
//  is deliberate: loops want the array indexed, not walked —
//  VC6 strength-reduces `m_pAnims[i]` to the pointer walk the shipped bytes
//  have, and a hand-written pointer walk comes out a byte or two different.
//  The zero-trip guard is `jbe`, which is the unsigned compare of
//  `i < m_nAnims`, and the counter never reaches memory so it takes no slot.
// *********************************************************************** //

//  Three `flds 0x74(%ecx)` per statement in GetDims above, and the reason is
//  worth recording next to this one: ltvector.h's _CVector(T,T,T) takes its
//  three components BY VALUE, so each is loaded separately — where the
//  scalar `operator-(T v)` overload that also exists in that header would
//  load the radius once. The triple load is what picks the vector spelling.

// One view stands in for two classes here — ModelAnim (which contributes the
// back-pointer at +0x20) and Model (the collection at +0x164). Their offsets
// do not overlap, and merging them keeps the comparison against `this`
// well-typed without dragging commonmodel.h in.
class CModelAnimOwnerView;

class CModelAnimInfoView
{
public:
	CModelAnimOwnerView* m_pAnim;            // 0x00
	uint8                m_pad_04[0x20 - 0x04];
};

class CModelAnimOwnerView
{
public:
	uint32 CountOwnAnims();                  // 0x100176b7

	uint8  m_pad_000[0x20];
	// ModelAnim+0x20: the model the animation record belongs to.
	CModelAnimOwnerView* m_pOwnerModel;      // 0x20
	uint8  m_pad_024[0x168 - 0x24];
	CModelAnimInfoView*  m_pAnims;           // 0x168  (m_AnimArray.m_pArray)
	uint32               m_nAnims;           // 0x16c  (m_AnimArray.m_nElements)
};

uint32 CModelAnimOwnerView::CountOwnAnims()
{
	uint32 nCount = 0;
	uint32 i;

	for (i = 0; i < m_nAnims; i++)
	{
		if (m_pAnims[i].m_pAnim->m_pOwnerModel == this)
			nCount++;
	}

	return nCount;
}



// *********************************************************************** //
//  MOVE-ME: objtypes.cpp, immediately after the vftable slot at 0x10024c21
//  that constructs it (which is NOT reconstructed).
//
//  0x10024c87 (37 bytes) — the constructor of the twenty-eight-byte query
//  record CObjWorldModel's world walk fills in. Its three function-pointer
//  defaults are the SAME three smgr.cpp's CVisQuery constructor installs
//  (0x10024cac, a one-byte `ret`, and 0x10024cad, `push $1; pop eax; ret`),
//  which the linker folded onto one pair of addresses.
//
//  The two stores of 0x10024cac are the point of the function, and
//  this is why: two stores of the same IMMEDIATE mean two
//  relocations and therefore TWO symbols in the source, where one function
//  assigned to both slots makes VC6 hold the address in a register and store
//  from it. So there are two do-nothing defaults here, not one used twice —
//  exactly as smgr.cpp had to write vq_Ignore and vq_IgnoreToo.
//
//  If this record turns out to be the same class as smgr.cpp's CVisQuery
//  seen through a different vftable, the four statics below fold into that
//  file's and this constructor becomes a second CVisQuery constructor. The
//  layouts do NOT agree as they stand (CVisQuery's position is a vector at
//  +0x04 and this record's +0x04 is an integer zero), so they are kept
//  apart.
// *********************************************************************** //

// Not static: netobj.cpp's CObjWorldQueryQ458 installs the same three.
void ow_Ignore(void* pUser, void* pObject)
{
}

// The second symbol. See above: one function assigned twice is a different
// twenty-nine bytes.
void ow_IgnoreToo(void* pUser, void* pObject)
{
}

int ow_Accept(void* pUser, void* pObject)
{
	return 1;
}

class CObjWorldQuery
{
public:
	CObjWorldQuery();                        // 0x10024c87

	uint32 m_Unknown00;                      // +0x00
	uint32 m_Unknown04;                      // +0x04
	void (*m_pFn08)(void*, void*);           // +0x08
	void (*m_pFn0c)(void*, void*);           // +0x0c
	uint32 m_Unknown10;                      // +0x10
	int  (*m_pFn14)(void*, void*);           // +0x14
	uint32 m_Unknown18;                      // +0x18
	// Never written by the constructor, and the caller's frame is 0x20 —
	// so the record is seven initialised words and one that is not.
	uint32 m_Unknown1c;                      // +0x1c
};

// The stores come out in declaration order with one register carrying the
// zero, and `this` is moved to %eax first so %ecx is free to be that zero
// register. Neither is a source choice.
CObjWorldQuery::CObjWorldQuery()
{
	m_Unknown00 = 0;
	m_Unknown04 = 0;
	m_pFn08 = ow_Ignore;
	m_pFn0c = ow_IgnoreToo;
	m_Unknown10 = 0;
	m_pFn14 = ow_Accept;
	m_Unknown18 = 0;
}



// *********************************************************************** //
//  MOVE-ME: smgr.cpp, beside CSoundListenerView (0x100273bc) — the function
//  immediately above 0x100274bc in the image, and the only other thing that
//  touches this run of records.
//
//  0x100274bc (32 bytes) — release each of four records' object, back to
//  front. The stride is twelve, the run ends at this+0x164 (so it starts at
//  this+0x134), the trip count is the immediate four (`push $4; pop edi`,
//  which is the three-byte `mov reg,imm`), and each record's first
//  dword is a pointer whose vftable slot 2 is called when it is not null.
//
//  COIN FLIP, and it is the whole function. The shipped pointer starts ONE
//  PAST the end and each trip reads `-0xc(%esi)` before `sub $0xc,%esi`,
//  which is the canonical form of a descending index. Written as
//      for (i = 3; i >= 0; i--)
//  (below) VC6 should reduce to exactly that; the alternative spelling, if
//  the pointer comes out starting AT the last element instead, is
//      for (i = 4; i > 0; i--)  ... m_Slots[i - 1] ...
//  Both run the same four times; only the induction variable's bias differs,
//  and that bias is the two bytes this function is worth.
// *********************************************************************** //

class CListenerSlotObj
{
public:
	virtual void _v00() = 0;
	virtual void _v04() = 0;
	// Slot 2 — the only one reached here.
	virtual void Release() = 0;
};

struct CListenerSlot
{
	CListenerSlotObj* m_pObj;                // +0x00
	uint8             m_pad_04[0x0c - 0x04];
};

class CListenerSlotOwner
{
public:
	void ReleaseSlots();                     // 0x100274bc

	uint8         m_pad_000[0x134];
	CListenerSlot m_Slots[4];                // 0x134 .. 0x163
};

void CListenerSlotOwner::ReleaseSlots()
{
	int i;

	for (i = 3; i >= 0; i--)
	{
		if (m_Slots[i].m_pObj)
			m_Slots[i].m_pObj->Release();
	}
}



// *********************************************************************** //
//  MOVE-ME: objbank.cpp — slot 2 of the bank vftable at 0x1004e2f0, which
//  that file already identifies as the 0x54-byte (sizeof(ServerMessage))
//  instantiation.
//
//  0x1002ccb5 (30 bytes) — and it CONTRADICTS the model objbank.cpp carries
//  for that instantiation, which is worth stating plainly because it is the
//  most useful thing in this block.
//
//  objbank.cpp declares `virtual ~CBankElem54();` and writes Free as
//  `((T*)pObj)->~T(); sb_Free(...)`. A virtual destructor makes the call
//  INDIRECT (`mov (%ecx),%eax; push $0; call *(%eax)`) and puts the deleting
//  destructor in slot 0 of the element's vftable. The shipped Free calls
//  0x1002ceac DIRECTLY, and a sweep of every four-byte value in every
//  non-.text section finds 0x1002ceac in NO vftable at all. So the element's
//  teardown is not virtual and 0x1002ceac is not a `??_G`: it is an ordinary
//  member taking a flags word (`testb $1,0x4(%esp)`), releasing a
//  sub-object at +0x2c through that object's own slot 2, freeing itself only
//  when bit 0 is set, and returning `this`.
//
//  That is the same signature the bank hands it — zero, meaning "destruct,
//  do not release" — so the record's own Release is what the source calls.
// *********************************************************************** //

class CBankElem54View
{
public:
	// 0x1002ceac. Returns `this`, which nothing reads.
	void* Release(uint32 nFlags);

	uint8 m_pad[0x54];
};

class CMsgRecordBank : public CGlobalBankBase
{
public:
	virtual void* Allocate() = 0;
	virtual void  Free(void* pObj);          // 0x1002ccb5
	virtual void  Init() = 0;

	StructBank m_Bank;                       // +0x04
	uint32     m_Unknown20;                  // +0x20
};

// sb_Free is servermgr.h's inline: the freed record's first dword becomes
// the next-free link. m_Bank.m_pFreeList is at this+0x1c, which is what the
// shipped `mov 0x1c(%esi),%eax` / `mov %edi,0x1c(%esi)` pair says.
void CMsgRecordBank::Free(void* pObj)
{
	((CBankElem54View*)pObj)->Release(0);

	sb_Free(&m_Bank, pObj);
}



// *********************************************************************** //
//  MOVE-ME: tcpdriver.cpp.
//
//  0x10036bc5 (40 bytes) — clear a freshly allocated session record. Its one
//  caller allocates 0x10b0 bytes, stores the vftable 0x1004e9f0 into the
//  block ITSELF and only then calls this, which is the one-level
//  /Ob1 constructor expansion: `new TCPSession` inlined the constructor (its
//  vptr store) and left the body as a call. tcpdriver.cpp already computes
//  sizeof(TCPSession) as 0x10b0 independently (NetSession is 0x109c, one
//  spare word, then a sockaddr_in), which is the check on the identification.
//
//  The fields, read off the stores: an id-like word set to 0xffff, four
//  dwords zeroed, three char arrays emptied by writing their first byte
//  (0x34, 0x58, 0x1058 — `mov %al,...`, so a byte store and not a dword),
//  and one more dword at 0x1098. The byte stores are the tell that those
//  three are strings: an integer field would be cleared with the dword.
//
//  Only the STORE ORDER is a source fact here, and it is written out in the
//  emitted order below. The 0xffff is first, which is where a
//  non-zero initialiser goes when everything after it shares a zero
//  register.
// *********************************************************************** //

class CNetSessionView
{
public:
	void Clear();                            // 0x10036bc5

	uint8  m_pad_000[0x04];
	uint32 m_Unknown04;                      // 0x04
	uint8  m_pad_008[0x28 - 0x08];
	uint32 m_Unknown28;                      // 0x28
	uint32 m_Unknown2c;                      // 0x2c
	uint32 m_Unknown30;                      // 0x30
	char   m_Str34[0x54 - 0x34];             // 0x34
	uint32 m_Unknown54;                      // 0x54
	char   m_Str58[0x1058 - 0x58];           // 0x58
	char   m_Str1058[0x1098 - 0x1058];       // 0x1058
	uint32 m_Unknown1098;                    // 0x1098
};

void CNetSessionView::Clear()
{
	m_Unknown54 = 0xffff;

	m_Unknown04 = 0;
	m_Unknown28 = 0;
	m_Unknown2c = 0;
	m_Unknown30 = 0;

	m_Str58[0] = 0;
	m_Str1058[0] = 0;
	m_Str34[0] = 0;

	m_Unknown1098 = 0;
}



// *********************************************************************** //
//  MOVE-ME: sharedcommon.cpp, first function of the file.
//
//  0x1001214c (13 bytes) — SharedCommonLT's constructor: `mov %ecx,%eax ;
//  andl $0,0xc(%eax) ; movl $0x1004d700,(%eax) ; ret`. 0x1004d700 is
//  SharedCommonLT's vtable (slot 0 is the ??_G 0x10012159 it shares with
//  ILTCommon's 0x1004d748; slots 2/3 are 0x10012180/0x100121df), +0x0c is
//  sharedcommon.h's m_pMath, and ILTCommon's own inline constructor leaves
//  nothing behind but its vptr store, which is dead. The only caller is the
//  CLTServer constructor (0x1002d3ba, on its +0x234 member), which then
//  overwrites the vptr with ServerCommonLT's 0x1004e5ac. Lithtech's twin is
//  0x004347a0 (cltapi.cpp declares it as SharedCommonLTQ404()).
//
//  The right home is `SharedCommonLT();` declared in sharedcommon.h and
//  defined at the top of sharedcommon.cpp (it precedes 0x10012159 and
//  SetMath 0x10012176). That is a header edit, so it is modelled here with
//  a view of the same layout, as this file's other MOVE-ME classes are.
//  Too small for a masked byte match to credit (thirteen bytes, four of them masked).
// *********************************************************************** //

class CSharedCommonBaseView
{
public:
	virtual ~CSharedCommonBaseView() {}

	void* m_pTransformLT;                    // 0x04
	void* m_pModelLT;                        // 0x08
};

class CSharedCommonView : public CSharedCommonBaseView
{
public:
	CSharedCommonView();                     // 0x1001214c

	void* m_pMath;                           // 0x0c
};

CSharedCommonView::CSharedCommonView()
{
	m_pMath = 0;
}



// *********************************************************************** //
//  NOT WRITTEN — the rows of this range that have no body here, with
//  what the bytes say.
// *********************************************************************** //
//
//  0x10016c69 (44 bytes) — a destructor tail: one CMoArray-derived member at
//    +0x10 is Term'd twice, first with `SetSize2(0, m_pOwner->m_pAlloc)`
//    (the owner is at +0x24) and then, after its vptr is re-stored to
//    0x1004daf4, with `SetSize2(0, g_pDefaultAlloc)` — the derived array's
//    destructor followed by CMoArray's own. So the class has a collection at
//    +0x10 and an owner back-pointer at +0x24, which is CCollElem24Rec's
//    shape shifted four bytes (that class's owner is at +0x20 and it Terms
//    BOTH its arrays). It is the destructor of a 0x28-byte sibling, and
//    which one is not settled. commonmodel.cpp / collection.cpp.
//
//  0x10024c21 (102 bytes) — objtypes.cpp, and the slot that fills the query
//    record CObjWorldQuery above constructs. Two vftables carry it
//    (0x1004dfc4 and 0x1004e1c8, so two classes share the body). It
//    constructs the record on the frame, calls the point-to-poly walk
//    (0x10024cb1, which objtypes.cpp already documents) with
//    &pArg->m_vPos, and then copies six fields out of its argument into the
//    record — +0x14, +0x1c, +0x20, +0x18, +0x24 — with the record's +0x1c
//    coming from a call on pArg->m_p44 (0x10039ba2). Every one of the
//    constructor's seven stores is overwritten, which is why the record's
//    field TYPES cannot be read off either function alone.
//
//  0x10024d6e (165 bytes) — objtypes.cpp, vftables 0x1004dfac / 0x1004e1b0.
//    CObjWorldModel's terrain hookup: it asks the object at +0x1a8 for its
//    flags (slot 6), takes bit 4 then bit 5 as two literal cases (0 and 2),
//    otherwise calls IsWorldBusy (0x10024e7f, already reconstructed) and
//    then slot 0, and on the fall-through reports
//    "ERROR!!  Terrain section '%s' has invalid m_NodePath." with
//    "---NONAME---" for a null section. Three call sites share one tail at
//    0x10024d95 through two `jmp`s, which is a cross-jump the source shape
//    has to reproduce; that is the whole difficulty.
//
//  0x1002d385 (208 bytes) — commonphysics.cpp. The constructor of the
//    physics object CreateServer builds: its own vftable at 0x1004e368, a
//    second at +0x228 (0x1004e660), a third at +0x22c (0x1004e5f4), a
//    fourth at +0x234 (0x1004e5ac, after a call to 0x1001214c) and two more
//    at +0x248 (0x1004e560) and +0x258 (0x1004e550), with the owner
//    (argument 1) stored into +0x224, +0x04 of three of them, +0x244, and
//    +0x1c as `pOwner ? pOwner + 4 : NULL`. It needs the whole class graph.
//
//  0x1002f83a (39 bytes) — the `??_G` of that same class (vftable
//    0x1004e484): restore the member vptr at +0x234 (0x1004d748) and the
//    object's own base vptr (0x1004e684), then free only if bit 0 of the
//    flag is set. It falls out for free once 0x1002d385's class exists, and
//    not before.
