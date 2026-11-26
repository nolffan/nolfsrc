// *********************************************************************** //
//
//  MODULE:   CLU405F30 (.CPP)  -- scratch reconstruction, lithtech.exe only
//
//  WHAT THIS CLUSTER IS, AND HOW IT WAS IDENTIFIED
//
//  Nine rows are enumerated over 0x00405f30..0x004064d9.  Seven of
//  them are the target of a dword in .rdata, and those dwords are what names
//  the module:
//
//      0x004a1758  0x00405f30 \
//      0x004a175c  0x00405fd0  |  a SIX-slot vtable, bounded on both sides
//      0x004a1760  0x00406070  |  by unrelated tables (0x004a16ec's 27-slot
//      0x004a1764  0x00406140  |  client ILTModel below it, 0x004a1770's
//      0x004a1768  0x00406210  |  ILTPhysics/ILTCommon run above it).
//      0x004a176c  0x004062e0 /
//
//      0x004a172c  0x00406460     slot 16 of the 27-slot table at 0x004a16ec
//
//  src/LT2/sdk/inc/iltvideomgr.h declares exactly six pure virtuals, in this
//  order: StartOnScreenVideo, StartTextureVideo, UpdateVideo,
//  GetVideoStatus, StopVideo, BindTextureVideoToPoly.  The `ret $N` of the
//  six bodies is 0xc, 0xc, 4, 4, 4, 8 -- 3,3,1,1,1,2 arguments, which is
//  iltvideomgr.h's signature list exactly and in order.  So 0x004a1758 is
//  the concrete ILTVideoMgr the client hands out.
//
//  The .data string pool confirms it, IN SOURCE ORDER:
//
//      0x004a7950  "LVideoMgr::StartOnScreenVideo"
//      0x004a7970  "LVideoMgr::StartTextureVideo"
//      0x004a7990  "LVideoMgr::UpdateVideo"
//      0x004a79a8  "LVideoMgr::BindTextureVideoToPoly"
//
//  FOUR strings for SIX methods.  GetVideoStatus and StopVideo both push
//  0x004a7990 -- Monolith copy-pasted UpdateVideo and did not retype the
//  name.  That is shipped behaviour and it is reproduced verbatim below;
//  it is also why the enumeration's name column carries "UpdateVideo"
//  three times.
//
//  0x00406460 is NOT part of this module.  Its name literal is reached
//  through a `static char*` at 0x004a79e8 -> 0x004a79cc
//  "ClientModelLT::SetCurAnim", and its shape is line-for-line
//  src/engine/server/servermodel.cpp's ServerModelLT::SetCurAnim.  It is
//  the CLIENT's ILTModel override, slot 16 of 0x004a16ec (lt2new4.cpp
//  already pins "call *0x40 is index 16, SetCurAnim").  It is included here
//  because it is enumerated in this range, in its own clearly-marked section at the
//  bottom; if this ever lands in the tree it belongs in a clientmodel.cpp
//  next to commonmodel.cpp, not in the video file.
//
//  STEP ZERO.  Neither src/engine/lithtech nor src/engine/server contains
//  "LVideoMgr", any of the four name literals, or a 0x10xxxxxx annotation
//  for these bodies.  binkvideo.cpp reconstructs the thing BEHIND this
//  interface (BinkVideoMgr / BinkVideoInst, 0x00401770..0x00402300) and
//  cltnew.cpp/ltnew1.cpp reconstruct CALLERS of it, but the forwarding
//  front end itself is unwritten.  ClientModelLT is likewise unwritten
//  (only CommonModelLT and ServerModelLT exist, both server-side).
//
//  THE TWO MISALIGNED ROWS -- 0x00405f77 and 0x00406017 -- ARE NOT
//  FUNCTIONS.  Both are shape (c), the fall-through of an entry test, and
//  both are the SAME defect twice because the two functions above them are
//  the same function twice:
//
//      0x00405f30  mov 0x4(%ecx),%eax        ; m_pClientMgr
//                  mov 0x1610(%eax),%ecx     ; m_pClientMgr->m_pVideoMgr
//                  test %ecx,%ecx
//                  jne  0x00405f77           <-- THE ROW BOUNDARY
//                  ...report 73, ret $0xc... ; the failure arm FALLS THROUGH
//      0x00405f77  mov 0x4(%esp),%eax        ; pFilename -- the SECOND check
//
//  i.e. 0x00405f77 is the continuation of 0x00405f30 after the "engine is
//  up" guard, and 0x00406017 is the identical continuation of 0x00405fd0.
//  The 71-byte row has one branch target outside the range:
//  0x00405f3b -> 0x00405f77.
//  The true extents are
//
//      0x00405f30..0x00405fcd  157 bytes   (71 + 86, contiguous)
//      0x00405fd0..0x0040606e  158 bytes   (71 + 87, contiguous)
//
//  and the joined ranges are clean: no branch target leaves
//  them, the last instruction ends exactly on the boundary, and the three
//  `ret`s are the three arms (two error returns and the forward).
//
//  Note the SECOND defect the enumeration also reports on these bodies and
//  which is equally false: "contains an interior ret then a prologue |
//  start missing at 0x00405fb9 (+66)".  0x00405fb9 is the fall-through of
//  the *second* guard, the same shape one level down.  Every one of the
//  nine rows carries that note; none of them is a real start.
//
//  WHAT THE 204/204/206 FAMILY IS.  One shape, three times:
//  UpdateVideo (0x00406070), GetVideoStatus (0x00406140) and StopVideo
//  (0x00406210) differ only in which VideoInst vtable slot they forward to
//  (0x14 Update, 0x18 GetStatus, 0x04 Release) and, for StopVideo, in
//  swallowing the result (`xor %eax,%eax` -> LT_OK), which is the two extra
//  bytes.  Decoding one decoded all three.
//
// *********************************************************************** //

// (no system headers needed: every type this file touches is declared below)


// ======================================================================= //
//  TYPES.  Everything is suffixed V so nothing here can collide with
//  src/engine/lithtech or src/engine/server.
// ======================================================================= //

typedef int             LTRESULT;
typedef int             LTBOOL;
typedef unsigned long   uint32;
typedef unsigned long   HPOLY;

#define LTNULL  0
#define LTTRUE  1
#define LTFALSE 0

#define LT_OK                 0
#define LT_INVALIDPARAMS     60
#define LT_NOTINITIALIZED    73


// ======================================================================= //
//  THE ERROR MACROS -- src/engine/server/de_error.h's, renamed.
//
//  Fixed by the bytes at every site, e.g. 0x00405f3d:
//
//      push $0x49            ; dsi_OnReturnError(73)
//      call 0x0042d760
//      mov  0x004b0ab0,%eax  ; g_DebugLevel
//      add  $0x4,%esp
//      cmp  $0x2,%eax        ; >= level   (level is 2 here, 1 at 0x004063ef)
//      jl   <skip>
//      mov  0x004a7110,%ecx  ; g_pErrorFormat
//      push $0x004a7164      ; " "
//      push $0x004a7168      ; "73"
//      push $0x004a7950      ; "LVideoMgr::StartOnScreenVideo"
//      push %ecx
//      call 0x0042e6f0
//      add  $0x10,%esp
//      mov  $0x49,%eax       ; return 73
//
//  The third argument is the NUMBER as a string ("73", "60"), which is the
//  two-level stringize; 0x00406460 uses the other form and pushes
//  "LT_INVALIDPARAMS" (0x004a77c8) plus a `static char*` name, i.e.
//  de_error.h's RETURN_ERROR_NAMED.
// ======================================================================= //

extern "C" void dsi_OnReturnErrorV(int code);            // 0x0042d760
extern int      g_DebugLevelV;                           // 0x004b0ab0
extern char*    g_pErrorFormatV;                         // 0x004a7110
void            dsi_ServerErrorV(char* pFormat, ...);    // 0x0042e6f0

#define V_ERRSTR_(x) #x
#define V_ERRSTR(x)  V_ERRSTR_(x)
#define V_ERRDESC    " "

#define V_REPORT_ERROR(level, fnName, code) \
	dsi_OnReturnErrorV(code); \
	if (g_DebugLevelV >= level) \
		dsi_ServerErrorV(g_pErrorFormatV, fnName, V_ERRSTR(code), V_ERRDESC);

#define V_RETURN_ERROR(level, fnName, code) \
	{ \
		V_REPORT_ERROR(level, fnName, code) \
		return code; \
	}

#define V_RETURN_ERROR_SYM(level, fnName, code) \
	{ \
		dsi_OnReturnErrorV(code); \
		if (g_DebugLevelV >= level) \
			dsi_ServerErrorV(g_pErrorFormatV, fnName, #code, V_ERRDESC); \
		return code; \
	}

// Not written through V_RETURN_ERROR_SYM: the preprocessor expands `code`
// before V_RETURN_ERROR_SYM can apply `#` to it, and then the code string is
// "60" (see de_error.h's RETURN_ERROR_NAMED).
#define V_RETURN_ERROR_NAMED(level, fnName, code) \
	{ \
		static char* pErrFnName = fnName; \
		dsi_OnReturnErrorV(code); \
		if (g_DebugLevelV >= level) \
			dsi_ServerErrorV(g_pErrorFormatV, pErrFnName, #code, V_ERRDESC); \
		return code; \
	}


// ======================================================================= //
//  THE LIST
//
//  NOT ltlink.h's LTList (whose head is an embedded LTLink and whose count
//  comes first at the same offsets by accident).  This is the hand-rolled
//  one binkvideo.cpp already recovered from the manager's own insert at
//  0x00401af0: a bare head POINTER, nullable, with the count in front of
//  it.  The four search sites below read the head at manager+0x08 and test
//  it against zero, which an embedded head could never be.
//
//      mgr +0x04  m_nElements
//      mgr +0x08  m_pHead
//
//  The node is ltlink.h's LTLink: prev, next, data.  The walk reads +0x04
//  (next) and compares +0x08 (data) against the HVIDEO.
// ======================================================================= //

class LTLinkV
{
public:
	LTLinkV* m_pPrev;                        // 0x00
	LTLinkV* m_pNext;                        // 0x04
	void*    m_pData;                        // 0x08
};

class LTListV
{
public:
	uint32   m_nElements;                    // 0x00
	LTLinkV* m_pHead;                        // 0x04
};


// ======================================================================= //
//  THE LIST SEARCH -- inlined at all four sites, 0x004060c1, 0x00406191,
//  0x00406261 and 0x00406312.
//
//  THIS IS THE ONE PIECE OF THE CLUSTER THAT IS NOT OBVIOUS, so the whole
//  derivation is here.  The shipped shape (UpdateVideo's copy) is:
//
//      004060c1  mov  0x8(%eax),%edx        ; pList->m_pHead
//      004060c4  xor  %esi,%esi             ; i = 0
//      004060c6  test %edx,%edx
//      004060c8  je   004060e8              ; empty
//      004060ca  mov  %edx,%eax
//      004060cc  cmp  %ecx,0x8(%eax)        ; pCur->m_pData == pObj ?
//      004060cf  je   004060de              ; found
//      004060d1  mov  0x4(%eax),%eax        ; pCur = pCur->m_pNext
//      004060d4  inc  %esi                  ; i++
//      004060d5  cmp  %edx,%eax
//      004060d7  je   004060e8              ; wrapped -> not found
//      004060d9  cmp  %ecx,0x8(%eax)
//      004060dc  jne  004060d1
//  found:
//      004060de  lea  0x8(%esp),%edx        ; &pIndex's target
//      004060e2  test %edx,%edx
//      004060e4  je   004060f0
//      004060e6  jmp  004060f4
//  empty:
//      004060e8  lea  0x8(%esp),%eax
//      004060ec  test %eax,%eax
//      004060ee  jne  00406102              ; -> the LT_INVALIDPARAMS report
//      004060f0  mov  0x8(%esp),%esi
//      004060f4  cmp  $0xffffffff,%esi
//      004060f7  je   00406102
//
//  `lea <stackslot>; test` is `if (p)` on the address of a local -- MSVC6
//  does not fold that away when the callee is inlined -- so the callee
//  takes a `uint32 *pIndex` and null-checks it TWICE.  Both arms of both
//  tests are then explained by exactly one source shape, and no other:
//
//    * found, pIndex != 0:  `*pIndex = i` is dead (the caller's last use of
//      the variable is the compare that follows), so the store is dropped
//      and the value stays in %esi -- `jmp` to the compare.
//    * found, pIndex == 0:  no store happened, so the compare must RELOAD
//      the variable from memory -- `je` to `mov 0x8(%esp),%esi`.
//    * empty, pIndex != 0:  `*pIndex = (uint32)-1` is likewise dropped, but
//      MSVC propagates the constant into the compare, which is then always
//      true -- `jne` STRAIGHT to the error block, with no store and no
//      compare.  That is why the "not found" arm looks like it is testing
//      a pointer to decide whether to fail.
//    * empty, pIndex == 0:  no store -- `je` to the same reload.
//
//  The caller therefore IGNORES the return value and tests the index; if it
//  had tested the return value both arms would be constant-folded branches
//  and neither `lea` would exist.  The stack slot the local gets is the
//  dead HVIDEO parameter slot, which is MSVC's live-range allocation and
//  nothing the source chooses.
// ======================================================================= //

inline LTBOOL v_FindElementIndex(LTListV* pList, void* pObj, uint32* pIndex)
{
	LTLinkV* pCur;
	uint32   i = 0;

	if (pList->m_pHead)
	{
		pCur = pList->m_pHead;

		do
		{
			if (pCur->m_pData == pObj)
			{
				if (pIndex)
					*pIndex = i;

				return LTTRUE;
			}

			pCur = pCur->m_pNext;
			++i;
		} while (pCur != pList->m_pHead);
	}

	if (pIndex)
		*pIndex = (uint32)-1;

	return LTFALSE;
}


// ======================================================================= //
//  THE CLASSES BEHIND THE INTERFACE
// ======================================================================= //

// What BindTextureVideoToPoly binds a video to.  Only +0x28 of the poly
// record is reached from here; ci_SetPolyHideStatus (0x00407f90, the same
// hPoly walk verbatim) reaches +0x38 of the same object as a flags word,
// and binkvideo.cpp's BindToSurface reaches +0x30 as a SharedTexture*.
// Only the vptr-free size matters here, so nothing is named.
class CVideoPolyV;

// The poly record the world model hands back for an HPOLY.
class WorldPolyV
{
public:
	unsigned char m_pad_00[0x28];
	CVideoPolyV*  m_pSurface;                // 0x28
};

// The thing hanging off world-model +0x04 that turns an HPOLY into a poly
// record.  Slot 3 (`call *0xc(%eax)`), one argument, __thiscall.  The three
// slots before it are never reached from this cluster, so they are spacers
// rather than guesses.
class CWorldBspV
{
public:
	virtual void* _v00() = 0;
	virtual void* _v04() = 0;
	virtual void* _v08() = 0;
	virtual WorldPolyV* GetPolyFromHPoly(HPOLY hPoly) = 0;   // 0x0c
};

class CWorldModelV
{
public:
	unsigned char m_pad_00[0x04];
	CWorldBspV*   m_pBsp;                    // 0x04
};

// One video in flight.  binkvideo.cpp already pins the whole table from the
// Bink side (vtable 0x004a1454); the three slots this cluster calls are
//   0x04 Release, 0x14 Update, 0x18 GetStatus, 0x1c BindToSurface.
// Slot 0 is the virtual destructor there.  Declaring it as a destructor
// here would force a body into this object, so it is a spacer.
class VideoInstV
{
public:
	virtual void     _v00() = 0;                            // 0x00  (~VideoInst)
	virtual void     Release() = 0;                         // 0x04
	virtual LTRESULT Restore() = 0;                         // 0x08
	virtual void     FreeSurfaces() = 0;                    // 0x0c
	virtual LTRESULT UpdateScreen() = 0;                    // 0x10
	virtual LTRESULT Update() = 0;                          // 0x14
	virtual LTRESULT GetStatus() = 0;                       // 0x18
	virtual LTRESULT BindToSurface(CVideoPolyV* pPoly) = 0; // 0x1c
};

typedef VideoInstV* HVIDEOV;

// The driver behind the interface -- BinkVideoMgr or SmackVideoMgr.  Only
// its first two slots and its video list are reached: UpdateVideo,
// GetVideoStatus and StopVideo go STRAIGHT to the instance rather than
// through the manager's own 0x08/0x0c/0x10 slots.
class VideoMgrBaseV
{
public:
	virtual LTRESULT StartOnScreenVideo(char* pFilename, uint32 flags,
	                                    HVIDEOV& hVideo) = 0;      // 0x00
	virtual LTRESULT StartTextureVideo(char* pFilename, uint32 flags,
	                                   HVIDEOV& hVideo) = 0;       // 0x04

public:
	LTListV m_Videos;                        // 0x04 count, 0x08 head
};

// CClientMgr, as far as this cluster reaches it.  0x150c and the 0x18c /
// 0x190 pair are already named by clientmgr.h and clienttexmgr.cpp; 0x1610
// is new and is fixed by the six bodies below all reading it and by
// 0x004a1698's cursor front end reading 0x1614 one dword above.
class CClientMgrVideoViewV
{
public:
	unsigned char  m_pad_000[0x18c];
	CWorldModelV** m_pWorldModels;           // 0x018c
	uint32         m_nWorldModels;           // 0x0190
	unsigned char  m_pad_194[0x150c - 0x194];
	void*          m_pConnection;            // 0x150c
	unsigned char  m_pad_1510[0x1610 - 0x1510];
	VideoMgrBaseV* m_pVideoMgr;              // 0x1610
};


// ======================================================================= //
//  THE INTERFACE -- vtable 0x004a1758, six slots, iltvideomgr.h order.
//
//  WHAT PINS THE SHAPE (one decode, six bodies)
//    * `mov 0x4(%ecx),%eax` opens all six: %ecx is `this` and +0x04 is the
//      one member, the client manager -- the same layout lt2new2.cpp gives
//      the cursor front end at 0x004a1698 and the same one 0x00406460 has.
//    * the guard is on m_pClientMgr->m_pVideoMgr, NOT on m_pClientMgr: the
//      cursor family tests 0x4(%ecx) itself, these dereference it first and
//      test what comes out.  So the driver pointer is the nullable one.
//    * the forward is `mov (%ecx),%edx; call *0xNN(%edx)` on a POINTER, so
//      the member's declared type is the abstract manager, not a named
//      concrete class (a named global object devirtualises).
// ======================================================================= //

class LVideoMgrV
{
public:
	virtual LTRESULT StartOnScreenVideo(char* pFilename, uint32 flags,
	                                    HVIDEOV& hVideo);          // 0x00405f30
	virtual LTRESULT StartTextureVideo(char* pFilename, uint32 flags,
	                                   HVIDEOV& hVideo);           // 0x00405fd0
	virtual LTRESULT UpdateVideo(HVIDEOV hVideo);                  // 0x00406070
	virtual LTRESULT GetVideoStatus(HVIDEOV hVideo);               // 0x00406140
	virtual LTRESULT StopVideo(HVIDEOV hVideo);                    // 0x00406210
	virtual LTRESULT BindTextureVideoToPoly(HVIDEOV hVideo,
	                                        HPOLY hPoly);          // 0x004062e0

public:
	CClientMgrVideoViewV* m_pClientMgr;      // 0x04 (0x00 is the vptr)
};


// ======================================================================= //
//  1.  0x00405f30 .. 0x00405fcd, 157 bytes -- LVideoMgr::StartOnScreenVideo
//
//  THE ENUMERATION'S DEFECT, instance one.  Carried as 0x00405f30 (71) plus
//  0x00405f77 (86).  0x00405f77 is the target of the `jne` at 0x00405f3b:
//  the "video system is up" guard's FAILURE arm is the fall-through and
//  ends `mov $0x49,%eax; ret $0xc` at 0x00405f6f, so the byte after it
//  looks like a fresh entry.  It is the second guard, not a function.
//
//  WHAT PINS IT
//    * `ret $0xc` -> three stack arguments, __thiscall.
//    * two guards in sequence, both with the report FALLING THROUGH and the
//      body hosted after them: `if (!p) RETURN_ERROR(...)` twice, not an
//      if/else.
//    * 0x00405f44 / 0x00405f86 read g_DebugLevel and compare against 2 in
//      BOTH reports, so both sites are level 2.
//    * the forward at 0x00405fc7 is `call *(%edx)` -- slot 0, which is what
//      iltvideomgr.h's own slot 0 is, and binkvideo.cpp's BinkVideoMgr slot
//      0 is StartOnScreenVideo.  %ecx already holds the driver, so no `this`
//      is materialised.
//    * arguments three and two are each loaded into %esi and pushed as they
//      come (0x00405fbc, 0x00405fc1), which is plain right-to-left
//      evaluation of a three-argument call and needs no local.
//
//  PREDICTED CALLSEQ: 0x0042d760, 0x0042e6f0, 0x0042d760, 0x0042e6f0,
//                     *(slot 0 of m_pVideoMgr)
// ======================================================================= //

LTRESULT LVideoMgrV::StartOnScreenVideo(char* pFilename, uint32 flags, HVIDEOV& hVideo)
{
	VideoMgrBaseV* pMgr = m_pClientMgr->m_pVideoMgr;

	if (!pMgr)
		V_RETURN_ERROR(2, "LVideoMgr::StartOnScreenVideo", LT_NOTINITIALIZED);

	if (!pFilename)
		V_RETURN_ERROR(2, "LVideoMgr::StartOnScreenVideo", LT_INVALIDPARAMS);

	return pMgr->StartOnScreenVideo(pFilename, flags, hVideo);
}


// ======================================================================= //
//  2.  0x00405fd0 .. 0x0040606e, 158 bytes -- LVideoMgr::StartTextureVideo
//
//  THE ENUMERATION'S DEFECT, instance two, and it is the SAME defect: row
//  0x00405fd0 (71) plus row 0x00406017 (87), with 0x00406017 the target of
//  the `jne` at 0x00405fdb.  Identical to 1 byte-for-byte except the name
//  literal (0x004a7970) and the slot (`call *0x4(%edx)`, slot 1); the one
//  extra byte is the longer `jne` displacement the wider body needs.
//
//  PREDICTED CALLSEQ: 0x0042d760, 0x0042e6f0, 0x0042d760, 0x0042e6f0,
//                     *(slot 1 of m_pVideoMgr)
// ======================================================================= //

LTRESULT LVideoMgrV::StartTextureVideo(char* pFilename, uint32 flags, HVIDEOV& hVideo)
{
	VideoMgrBaseV* pMgr = m_pClientMgr->m_pVideoMgr;

	if (!pMgr)
		V_RETURN_ERROR(2, "LVideoMgr::StartTextureVideo", LT_NOTINITIALIZED);

	if (!pFilename)
		V_RETURN_ERROR(2, "LVideoMgr::StartTextureVideo", LT_INVALIDPARAMS);

	return pMgr->StartTextureVideo(pFilename, flags, hVideo);
}


// ======================================================================= //
//  3.  0x00406070 .. 0x0040613c, 204 bytes -- LVideoMgr::UpdateVideo
//
//  WHAT PINS IT
//    * `ret $0x4` -> one stack argument.
//    * the LT_NOTINITIALIZED report is the fall-through (0x0040607e) and
//      the LT_INVALIDPARAMS report is hosted at the end (0x00406102),
//      reached from BOTH the `!hVideo` test (0x004060bf) and the index
//      test (0x004060f7).  That is ONE expansion after nested positive
//      guards (`if (hVideo) { find; if (nIndex != -1) return ...; }`):
//      two early refusals came out 274 bytes with an unmerged second copy.
//    * the search re-reads pList->m_pHead for the first test and the loop
//      bound; caching it in a `pHead` local put the first m_pData compare
//      in %edi (push/pop %edi, 210 bytes).
//    * the list walk is the inlined search derived above; it establishes
//      that the handle has to BE in the manager's list, which is the only
//      validation this interface can do on an opaque HVIDEO.
//    * the forward is `mov (%ecx),%edx; call *0x14(%edx)` on the HANDLE,
//      not on the manager -- slot 5, binkvideo.cpp's VideoInst::Update.
//      The manager's own UpdateVideo slot (0x0048a6b0) is never reached
//      from here.
//    * the result is returned unchanged: no `xor %eax,%eax` before the
//      `pop %esi`.
//
//  PREDICTED CALLSEQ: 0x0042d760, 0x0042e6f0, *(slot 5 of hVideo),
//                     0x0042d760, 0x0042e6f0
//  (the shared LT_INVALIDPARAMS block sits after the forward's `ret`)
// ======================================================================= //

LTRESULT LVideoMgrV::UpdateVideo(HVIDEOV hVideo)
{
	uint32 nIndex;

	VideoMgrBaseV* pMgr = m_pClientMgr->m_pVideoMgr;

	if (!pMgr)
		V_RETURN_ERROR(2, "LVideoMgr::UpdateVideo", LT_NOTINITIALIZED);

	if (hVideo)
	{
		v_FindElementIndex(&pMgr->m_Videos, hVideo, &nIndex);

		if (nIndex != (uint32)-1)
			return hVideo->Update();
	}

	V_RETURN_ERROR(2, "LVideoMgr::UpdateVideo", LT_INVALIDPARAMS);
}


// ======================================================================= //
//  4.  0x00406140 .. 0x0040620c, 204 bytes -- LVideoMgr::GetVideoStatus
//
//  Byte-for-byte 3 with `call *0x18(%edx)` (slot 6, VideoInst::GetStatus)
//  in place of `call *0x14(%edx)`.  It is slot 3 of 0x004a1758, which
//  iltvideomgr.h declares as GetVideoStatus ("LT_OK = still playing,
//  LT_FINISHED = done playing"), and ltnew1.cpp's caller note already
//  reads the pair that way.
//
//  THE NAME LITERAL IS "LVideoMgr::UpdateVideo" (0x004a7990), not
//  "GetVideoStatus".  That is not a mis-read: there is no GetVideoStatus
//  string anywhere in the image, the pool holds four strings for six
//  methods, and 0x00406172 / 0x00406125 push 0x004a7990.  Shipped
//  copy-paste, reproduced.
//
//  PREDICTED CALLSEQ: 0x0042d760, 0x0042e6f0, *(slot 6 of hVideo),
//                     0x0042d760, 0x0042e6f0
// ======================================================================= //

LTRESULT LVideoMgrV::GetVideoStatus(HVIDEOV hVideo)
{
	uint32 nIndex;

	VideoMgrBaseV* pMgr = m_pClientMgr->m_pVideoMgr;

	if (!pMgr)
		V_RETURN_ERROR(2, "LVideoMgr::UpdateVideo", LT_NOTINITIALIZED);

	if (hVideo)
	{
		v_FindElementIndex(&pMgr->m_Videos, hVideo, &nIndex);

		if (nIndex != (uint32)-1)
			return hVideo->GetStatus();
	}

	V_RETURN_ERROR(2, "LVideoMgr::UpdateVideo", LT_INVALIDPARAMS);
}


// ======================================================================= //
//  5.  0x00406210 .. 0x004062de, 206 bytes -- LVideoMgr::StopVideo
//
//  The same shape again, with two differences that account for the two
//  extra bytes: the forward is `call *0x4(%edx)` (slot 1, VideoInst::
//  Release, which returns void) and 0x0040629e is `xor %eax,%eax` --
//  `return LT_OK;`.  Everything upstream of the forward is identical
//  except the branch displacements the two extra bytes push out.
//
//  Same copy-pasted name literal as 4.
//
//  PREDICTED CALLSEQ: 0x0042d760, 0x0042e6f0, *(slot 1 of hVideo),
//                     0x0042d760, 0x0042e6f0
// ======================================================================= //

LTRESULT LVideoMgrV::StopVideo(HVIDEOV hVideo)
{
	uint32 nIndex;

	VideoMgrBaseV* pMgr = m_pClientMgr->m_pVideoMgr;

	if (!pMgr)
		V_RETURN_ERROR(2, "LVideoMgr::UpdateVideo", LT_NOTINITIALIZED);

	if (hVideo)
	{
		v_FindElementIndex(&pMgr->m_Videos, hVideo, &nIndex);

		if (nIndex != (uint32)-1)
		{
			hVideo->Release();
			return LT_OK;
		}
	}

	V_RETURN_ERROR(2, "LVideoMgr::UpdateVideo", LT_INVALIDPARAMS);
}


// ======================================================================= //
//  THE HPOLY WALK -- inlined at 0x00406399.
//
//  This is engine-wide boilerplate, not something this module invented.
//  The identical chain appears at 0x00407fcf (ci_SetPolyHideStatus, which
//  clientinterface.h already declares), at 0x00408095, and -- without the
//  connection guard, over a different base -- at 0x004055f9 and 0x0046c6e2:
//
//      mov  0x150c(%esi),%eax      ; m_pConnection
//      test %eax,%eax
//      je   <fail>
//      mov  0x190(%esi),%ecx       ; m_nWorldModels
//      mov  %edx,%eax
//      shr  $0x10,%eax             ; hPoly >> 16  == the world-model index
//      cmp  %ecx,%eax
//      jae  <fail>                 ; UNSIGNED, so the count is a uint32
//      mov  0x18c(%esi),%ecx       ; m_pWorldModels
//      mov  (%ecx,%eax,4),%eax
//      test %eax,%eax
//      je   <fail>
//      mov  0x4(%eax),%ecx
//      push %edx                   ; hPoly, whole
//      mov  (%ecx),%eax
//      call *0xc(%eax)
//
//  Every arm leaves by branching to ONE trailing report, which is what an
//  inlined helper with three early `return LTNULL`s plus a returned call
//  result gives.  Five `je`/`jae` land on 0x004063e0 and the helper
//  accounts for three of them; the caller's two null tests account for the
//  other two.
// ======================================================================= //

inline WorldPolyV* v_GetPolyFromHPoly(CClientMgrVideoViewV* pClientMgr, HPOLY hPoly)
{
	if (!pClientMgr->m_pConnection)
		return (WorldPolyV*)LTNULL;

	uint32 nWorldModel = hPoly >> 16;

	if (nWorldModel >= pClientMgr->m_nWorldModels)
		return (WorldPolyV*)LTNULL;

	CWorldModelV* pWorldModel = pClientMgr->m_pWorldModels[nWorldModel];

	if (!pWorldModel)
		return (WorldPolyV*)LTNULL;

	return pWorldModel->m_pBsp->GetPolyFromHPoly(hPoly);
}


// ======================================================================= //
//  6.  0x004062e0 .. 0x00406457, 375 bytes
//      -- LVideoMgr::BindTextureVideoToPoly
//
//  WHAT PINS IT
//    * `ret $0x8` -> two stack arguments; iltvideomgr.h's slot 5 is the
//      only two-argument slot in the interface.
//    * m_pClientMgr is cached in %esi at 0x004062e1 because it is wanted
//      twice (the driver guard and the hPoly walk); the other five read it
//      once and do not.
//    * THREE REPORT BLOCKS, not two, and they are at TWO DIFFERENT LEVELS:
//        - 0x004062ef  code 73, `cmp $0x2,%eax`  then `jmp 0x004063f2`
//        - 0x004063e0  code 73, `cmp $0x1,%eax`  then `jl  0x00406411`
//        - 0x0040635a / 0x0040641b  code 60, `cmp $0x2,%eax`
//      The first two share everything from the `jl` onward -- MSVC tail
//      merged them -- which is the only reason a single `cmp` constant
//      appears to serve both.  It does not: the driver guard is level 2 and
//      the world/poly chain is level 1.  Reading 0x004062fe's `cmp $0x2`
//      as the chain's threshold would put the wrong number in three places.
//    * the 60 block at 0x0040635a is the fall-through of `!hPoly` and is
//      NOT merged with the one at 0x0040641b, which serves the `!hVideo`
//      and index tests: `!hPoly` is an early refusal INSIDE the nested
//      positive guards of 3, the level-1 73 report closes the inner block,
//      and the 60 report closes the function.
//    * the forward is `mov (%edi),%edx; mov %edi,%ecx; call *0x1c(%edx)` --
//      slot 7 on the HANDLE, binkvideo.cpp's VideoInst::BindToSurface, and
//      its argument is the poly record's +0x28, which ci_SetPolyHideStatus
//      reaches the same way.
//
//  PREDICTED CALLSEQ, read off the image in address order (seven direct,
//  two indirect):
//      0x0042d760            the driver guard's dsi_OnReturnError (73)
//      0x0042d760            the !hPoly guard's  dsi_OnReturnError (60)
//      0x0042e6f0            its dsi_ServerError
//      *(slot 3 of the world bsp)
//      *(slot 7 of hVideo)
//      0x0042d760, 0x0042e6f0   the level-1 73 block -- whose printer call
//                               is ALSO the driver guard's, tail merged,
//                               which is why the first report contributes
//                               a dsi_OnReturnError but no dsi_ServerError
//      0x0042d760, 0x0042e6f0   the merged 60 block (!hVideo / index)
// ======================================================================= //

LTRESULT LVideoMgrV::BindTextureVideoToPoly(HVIDEOV hVideo, HPOLY hPoly)
{
	uint32 nIndex;

	CClientMgrVideoViewV* pClientMgr = m_pClientMgr;
	VideoMgrBaseV*        pMgr       = pClientMgr->m_pVideoMgr;

	if (!pMgr)
		V_RETURN_ERROR(2, "LVideoMgr::BindTextureVideoToPoly", LT_NOTINITIALIZED);

	if (hVideo)
	{
		v_FindElementIndex(&pMgr->m_Videos, hVideo, &nIndex);

		if (nIndex != (uint32)-1)
		{
			if (!hPoly)
				V_RETURN_ERROR(2, "LVideoMgr::BindTextureVideoToPoly", LT_INVALIDPARAMS);

			WorldPolyV* pPoly = v_GetPolyFromHPoly(pClientMgr, hPoly);

			if (pPoly)
			{
				CVideoPolyV* pSurface = pPoly->m_pSurface;

				if (pSurface)
					return hVideo->BindToSurface(pSurface);
			}

			V_RETURN_ERROR(1, "LVideoMgr::BindTextureVideoToPoly", LT_NOTINITIALIZED);
		}
	}

	V_RETURN_ERROR(2, "LVideoMgr::BindTextureVideoToPoly", LT_INVALIDPARAMS);
}


// ======================================================================= //
//  NOT PART OF THE VIDEO MODULE EITHER.
//
//  0x00407f90 .. 0x00408061, 210 bytes -- ci_SetPolyHideStatus
//  ("CLTClient::SetPolyHideStatus", 0x004a7c60; clientinterface.h slot
//  0x324).  Its real home is clientinterface.cpp, the ci_ API TU; it is
//  written here only because it shares v_GetPolyFromHPoly verbatim (the walk
//  at 0x00407fcf) and clientinterface.cpp is owned elsewhere.  Move it next
//  to ci_GetPolyTextureFlags (0x00408070, now written just below) when
//  that file is next touched.
//
//  WHAT PINS IT
//    * plain `ret` -> cdecl free function, hPoly at 4(%esp), bHide at 8.
//    * `!hPoly` report falls through (level 1, code 60); the tail report at
//      0x0040802a is level 1 with code 1 -- the literal "1" at 0x004a7494,
//      i.e. LT_ERROR.
//    * the client manager is the global at 0x004b17e0, not a member.
//    * the surface pointer (+0x28) is NOT null-tested before its +0x38
//      flags word is read; bit 2 is set / cleared on bHide.
// ======================================================================= //

#define LT_ERROR 1

struct CPolySurfaceV
{
	unsigned char m_pad_00[0x38];
	uint32        m_Flags;                   // 0x38
	unsigned short m_TextureFlags;           // 0x3c (ci_GetPolyTextureFlags)
};

extern CClientMgrVideoViewV* g_pClientMgrV;  // 0x004b17e0 (clientmgr.h's g_pClientMgr)

LTRESULT ci_SetPolyHideStatus(HPOLY hPoly, LTBOOL bHide)
{
	if (!hPoly)
		V_RETURN_ERROR(1, "CLTClient::SetPolyHideStatus", LT_INVALIDPARAMS);

	WorldPolyV* pPoly = v_GetPolyFromHPoly(g_pClientMgrV, hPoly);

	if (pPoly)
	{
		if (bHide)
			((CPolySurfaceV*)pPoly->m_pSurface)->m_Flags |= 4;
		else
			((CPolySurfaceV*)pPoly->m_pSurface)->m_Flags &= ~4;

		return LT_OK;
	}

	V_RETURN_ERROR(1, "CLTClient::SetPolyHideStatus", LT_ERROR);
}


// ======================================================================= //
//  0x00408070 .. 0x00408149, 218 bytes -- ci_GetPolyTextureFlags
//  ("CLTClient::GetPolyTextureFlags", 0x004a7c80; clientinterface.h slot
//  0x328).  Written here beside its shipped neighbour ci_SetPolyHideStatus
//  for the same reason: it shares v_GetPolyFromHPoly (the walk at 0x00408095).
//
//  WHAT PINS IT
//    * both argument tests `je` to ONE report (code 60, level 1) placed
//      LAST -> `if (hPoly && pFlags) { ... }` with the refusal after it.
//    * *pFlags = 0 is stored before the walk; the failure report is level 2
//      with code 1 (LT_ERROR).
//    * the flags are the 16-bit word at +0x3c of the surface (+0x28),
//      zero-extended (`xor %edx,%edx; mov 0x3c(%ecx),%dx`).
// ======================================================================= //

LTRESULT ci_GetPolyTextureFlags(HPOLY hPoly, uint32 *pFlags)
{
	if (hPoly && pFlags)
	{
		*pFlags = 0;

		WorldPolyV* pPoly = v_GetPolyFromHPoly(g_pClientMgrV, hPoly);

		if (pPoly)
		{
			*pFlags = ((CPolySurfaceV*)pPoly->m_pSurface)->m_TextureFlags;
			return LT_OK;
		}

		V_RETURN_ERROR(2, "CLTClient::GetPolyTextureFlags", LT_ERROR);
	}

	V_RETURN_ERROR(1, "CLTClient::GetPolyTextureFlags", LT_INVALIDPARAMS);
}


// ======================================================================= //
// ======================================================================= //
//
//  NOT PART OF THE VIDEO MODULE.
//
//  7.  0x00406460 .. 0x004064d9, 121 bytes -- ClientModelLT::SetCurAnim
//
//  It sits in this address range and it is enumerated here, but its data
//  anchor says it belongs elsewhere: it is slot 16 of vtable 0x004a16ec
//  (0x004a172c holds it), whose other twenty-six slots are the shared
//  CommonModelLT bodies at 0x0044e8f0..0x00450090.  lt2new4.cpp already
//  pins "call *0x40 is index 16, SetCurAnim".  Its name literal is reached
//  through a `static char*` at 0x004a79e8 -> 0x004a79cc, and the pool entry
//  above it is "ClientModelLT::SetCurAnim" -- which is the OTHER TU's
//  string block, immediately after the video one.  If this is ever landed
//  it belongs in a clientmodel.cpp beside src/engine/server/commonmodel.cpp.
//
//  IT IS THE CLIENT TWIN OF src/engine/server/servermodel.cpp's
//  ServerModelLT::SetCurAnim (0x1002dc22 in server.dll), line for line,
//  including the detail that file already flags: the model instance is read
//  from the tracker BEFORE the null checks, not after.
//
//      00406460  mov  0x4(%esp),%eax        ; pTracker
//      00406464  push %ebx
//      00406465  push %esi
//      00406466  mov  %ecx,%esi             ; this
//      00406468  mov  0x8(%eax),%ebx        ; pTracker->m_Link.m_pData
//                                           ;   == GetModelInstance(),
//                                           ;   BEFORE the test below
//      0040646b  test %eax,%eax
//      0040646d  je   0040649c
//      0040646f  test %ebx,%ebx
//      00406471  je   0040649c
//      ...
//      0040647c  call 0044fee0              ; CommonModelLT::SetCurAnim,
//                                           ;   qualified -> devirtualised
//      00406483  test %edi,%edi
//      00406485  jne  00406494              ; `== LT_OK` with the call
//                                           ;   FALLING THROUGH
//      00406487  mov  0x4(%esi),%edx        ; m_pClientMgr
//      0040648c  call 00420690              ; cdecl, (pClientMgr, pObj)
//
//  0x00420690 is the client's "this model changed, re-derive it" call --
//  the 261-byte body clienttexmgr.cpp parks as one group with 0x00420580
//  and 0x00420670 (both already declared by clientmgr.h).  Its first
//  argument is the manager (it reads +0x150c) and its second the object (it
//  reads +0x180, +0x1dc, +0x1e0, +0x1e8), which is the order the two pushes
//  at 0x0040648a/0x0040648b give.  It is the client's counterpart of the
//  server's sm_SetObjectChangeFlag(m_pServerMgr, pObj, CF_MODELINFO), but
//  it takes no flag, so it is NOT that function and is not named as one.
//
//  The report is de_error.h's RETURN_ERROR_NAMED, not RETURN_ERROR: the
//  code goes out SYMBOLICALLY ("LT_INVALIDPARAMS" at 0x004a77c8) and the
//  name through a `static char*` (`mov 0x004a79e8,%eax`, a load, not a
//  `push imm32`).  Level 2.
//
//  PREDICTED CALLSEQ: 0x0044fee0, 0x00420690, 0x0042d760, 0x0042e6f0
// ======================================================================= //

class LTAnimTrackerV
{
public:
	void* GetModelInstance()  { return m_Link.m_pData; }

public:
	LTLinkV m_Link;                          // 0x00 (m_pData at 0x08)
};

typedef unsigned long HMODELANIMV;

// Declared, never defined here: the bodies are the shared ones at
// 0x0044e8f0.., which src/engine/server/commonmodel.cpp already covers.
class CommonModelLTV
{
public:
	virtual LTRESULT SetCurAnim(LTAnimTrackerV* pTracker, HMODELANIMV hAnim);
};

// 0x00420690.
void sub_420690V(CClientMgrVideoViewV* pClientMgr, void* pObj);

class ClientModelLTV : public CommonModelLTV
{
public:
	virtual LTRESULT SetCurAnim(LTAnimTrackerV* pTracker, HMODELANIMV hAnim);

public:
	CClientMgrVideoViewV* m_pClientMgr;      // 0x04 (0x00 is the vptr)
};

LTRESULT ClientModelLTV::SetCurAnim(LTAnimTrackerV* pTracker, HMODELANIMV hAnim)
{
	// Read before the checks, not after: 0x00406468 loads it into a
	// callee-saved register ahead of the `test` at 0x0040646b.
	void* pObj = pTracker->GetModelInstance();

	if (!pTracker || !pObj)
		V_RETURN_ERROR_NAMED(2, "ClientModelLT::SetCurAnim", LT_INVALIDPARAMS);

	LTRESULT dResult = CommonModelLTV::SetCurAnim(pTracker, hAnim);

	if (dResult == LT_OK)
		sub_420690V(m_pClientMgr, pObj);

	return dResult;
}


// ======================================================================= //
//  WHERE THE OBJECT LIVES -- confirmation, not inference
//
//  CLTClient's constructor plants all of these front ends as SUB-OBJECTS,
//  each two dwords wide, at 0x00404bcf..0x00404c46:
//
//      [ebx+0x000] = 0x004a17c8   [ebx+0x010] = %edi
//      [esi+0x384] = 0x004a1770   [esi+0x390] = %edi   (ILTPhysics)
//      [esi+0x394] = 0x004a1758   [esi+0x398] = %edi   <-- THIS ONE
//      [esi+0x39c] = 0x004a16ec   [esi+0x3a0] = %edi   (client ILTModel,
//                                                       i.e. 7 above)
//      [esi+0x3a4] = 0x004a16c8   [esi+0x3ac] = %edi
//      [esi+0x3a8] = 0x004a16b8
//      [esi+0x3b8] = 0x004a1698   [esi+0x3bc] = %edi   (ILTCursor, which
//                                                       lt2new2.cpp models
//                                                       with exactly this
//                                                       two-field layout)
//
//  %edi is the CClientMgr the constructor was handed.  That is what fixes
//  m_pClientMgr at +0x04 of every one of them, including 0x00406460's --
//  the client ILTModel sits one sub-object above the video one, which is
//  also why the two TUs' string pools are adjacent.
//
//
//  SCOREBOARD -- seven functions, not nine.
//
//    0x00405f30  157  LVideoMgr::StartOnScreenVideo      (3 args, ret $0xc)
//    0x00405fd0  158  LVideoMgr::StartTextureVideo       (3 args, ret $0xc)
//    0x00406070  204  LVideoMgr::UpdateVideo             (1 arg,  ret $4)
//    0x00406140  204  LVideoMgr::GetVideoStatus          (1 arg,  ret $4)
//    0x00406210  206  LVideoMgr::StopVideo               (1 arg,  ret $4)
//    0x004062e0  375  LVideoMgr::BindTextureVideoToPoly  (2 args, ret $8)
//    0x00406460  121  ClientModelLT::SetCurAnim          (2 args, ret $8)
//
//    0x00405f77   --  interior of 0x00405f30 (target of its `jne`)
//    0x00406017   --  interior of 0x00405fd0 (target of its `jne`)
//
//  INTERNAL CALL GRAPH.  There is none: no function here calls another.
//  Every edge leaves the module -- two into the error pair (0x0042d760 /
//  0x0042e6f0), six into the driver or the video instance through a vtable,
//  one into the world model's bsp, and 7's two into 0x0044fee0 /
//  0x00420690.  The only sharing is the two inlined helpers above.
//
//  WHAT IS NOT PINNED
//    * the three vtable slots before CWorldBspV::GetPolyFromHPoly, and the
//      class itself.  Nothing in range reaches them; they are spacers.
//    * CVideoPolyV's layout.  Only its ADDRESS crosses this module.
//    * whether 0x00420690's real name is a client SetObjectChangeFlag
//      analogue.  It takes no flag argument, so it is not that function,
//      and clienttexmgr.cpp already declines to name the group.
//    * whether v_FindElementIndex is a free function, a member of the list,
//      or a member of the driver.  All three inline identically; the source
//      shape is fixed by the derivation above, its spelling is not.
// ======================================================================= //
