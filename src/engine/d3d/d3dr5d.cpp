// *********************************************************************** //
//
//  MODULE:   D3DR5D (.CPP)  --  d3d.ren "batch4"
//
//  PURPOSE:  the writable rows of batch4.txt (284..513 bytes each) --
//            ten of the twenty-five, plus the two REAL functions that
//            own the two rows which are not functions at all,
//            decoded instruction by instruction from the retail d3d.ren.
//            Intel C/C++ 4.0.  THE FILE IS NOT FLAG-HOMOGENEOUS -- see the
//            table below; it has to be cut.
//
//  ------------------------------------------------------------------ //
//  THE FLAG VERDICTS (read off the bytes, per function)
//  ------------------------------------------------------------------ //
//
//      addr        flag   the idiom that says so
//      0x10021ad0  /Os    every later exit CROSS-JUMPS (`eb a4` at
//                         0x10021b42, `e9` at 0x10021be9) into ONE
//                         `pop %ebx/%esi/%edi; mov %ebp,%esp; pop %ebp;
//                         ret $0xc`, and `return TRUE` is `xor %eax,%eax;
//                         inc %eax`.
//      0x10009d60  /O2    THE SAME SOURCE as 0x10021ad0, and it duplicates
//                         that epilogue THREE times (0x10009d7d,
//                         0x10009ddf, 0x10009ea7) and spells `return TRUE`
//                         `mov $0x1,%eax`.  This pair is the cleanest
//                         flag-split evidence in the batch: one source,
//                         two objects, two flags.
//      0x1007c290  /O2    five duplicated epilogues, `mov $0x1,%eax`.
//      0x10009f10  /O2    five duplicated epilogues, `mov $0x1,%eax`.
//                         (Now the template itself, w64_10009f10_qip.cpp.)
//      0x10032f30  /O2    SIX duplicated `mov (%esp),%ebx; mov %ebp,%esp;
//                         pop %ebp; ret` epilogues, and `mov $0x1,%eax`
//                         at 0x10033048.
//      0x100395f0  /O2    two duplicated epilogues; and `&g_1008e640.m_Bank`
//                         is `mov $0x1008e640,%eax; add $0x4,%eax` TWICE
//                         -- the /O2 `&p->m` form, not `lea`.
//      0x10064b70  /O2    outgoing arguments are `add $0xfffffff0,%esp`
//                         then `mov ...,(%esp)`; no `push` anywhere.
//      0x10035c30  /O2    same; single exit, so the epilogue test is mute.
//      0x10068a50  /O2    same; two duplicated epilogues (0x10068aa8 and
//                         0x10068c37).
//      0x1001d810  /Os    all four `return TRUE` sites JUMP into one
//                         block at 0x1001d834 and the FALSE exit
//                         cross-jumps into its tail.  (It covers
//                         listed row 0x1001d834.)
//      0x1007f420  /Os    identical, around 0x1007f444.  (It
//                         covers listed row 0x1007f444.)
//      0x1001e770  /Os    the second exit cross-jumps (`eb ed` at
//                         0x1001e8ce) into the first's `pop %ebx; pop %ebp;
//                         ret`, and `return TRUE` is `xor %eax,%eax;
//                         inc %eax`.  It is also the only frame-pointerless
//                         function here.
//
//  So: 0x10021ad0, 0x1001d810, 0x1007f420 and 0x1001e770 want /Os;
//  the other eight want /O2.  The file has to be cut on that line.
//
//  ------------------------------------------------------------------ //
//  THE BIG FIND: SIX OF THE ROWS ARE stdlith's CMoArray<T,C>
//  ------------------------------------------------------------------ //
//
//  src/LT2/lithshared/stdlith/dynarray.h is in the tree, so for these the
//  source is not a reconstruction at all -- it is the header, transcribed,
//  with the accessors defined out of line (/Ob1).  The identification is
//  not a shape argument; it is the callee set, which is the same three
//  one-line accessors in every instantiation:
//
//      0x1000a670  `push %esi; mov 0x4(%ecx),%eax; add $0x4,%esp; ret`
//                  == DefaultCache::GetWantedCache  (m_WantedCache, +0x04)
//      0x1000a680  `push %esi; mov 0x8(%esp),%eax; mov %eax,(%ecx);
//                   add $0x4,%esp; ret $0x4`
//                  == DefaultCache::SetCacheSize    (m_CacheSize,   +0x00)
//      0x1000a690  `push %esi; mov (%ecx),%eax; add $0x4,%esp; ret`
//                  == DefaultCache::GetCacheSize
//
//  and the two private helpers, one pair per instantiation:
//
//      _AllocateTArray            _DeleteAndDestroy   GetNumAllocElements
//      0x10021aa0 (n*4 bytes)     0x1001cf00          0x10021a80
//      0x10009cc0 (n*4 bytes)     0x10009cf0          0x10009c90
//      0x1000a0e0 (n*16, `shl $0x4`) 0x1002e6c0       0x10009c90
//
//  0x10021a80 and 0x10009c90 are literally `GetNumAllocatedElements()`:
//  `mov $0xc,%ecx; add %ebx,%ecx; call GetCacheSize; add 0x8(%ebx),%eax`
//  == `m_nElements + m_Cache.GetCacheSize()`, and that `mov $0xc,%ecx;
//  add %ebx,%ecx` is the /O2 `&this->m_Cache`.
//
//  THE LAYOUT falls straight out: +0x00 vptr (CMoArray derives GenList<T>),
//  +0x04 m_pArray, +0x08 m_nElements, +0x0c m_Cache (two dwords).
//
//  ELEMENT SIZE is read off the copy loops:
//      0x10021ad0 / 0x10009d60 / 0x1007c290   `(%edx,%eax,4)`      -> 4
//      0x10009f10                             four dword movs per
//                                             element, `shl $0x4` in
//                                             its _AllocateTArray -> 16
//
//  The `Insert2` pair (0x10021ad0, 0x10009d60) is byte-for-byte the same
//  instruction sequence apart from the epilogue idiom, which is what makes
//  the /Os-vs-/O2 reading above safe rather than a guess.
//
//  ------------------------------------------------------------------ //
//  TWO ROWS ARE NOT FUNCTIONS
//  ------------------------------------------------------------------ //
//
//   0x1001d834 is the shared `return TRUE` exit INSIDE
//              CMoArray<WORD,DefaultCache>::InternalNiceSetSize, whose real
//              prologue is at 0x1001d810 (`push %ebp; mov %esp,%ebp;
//              sub $0x18,%esp; push %edi; push %edi; push %esi; push %ebx;
//              mov %ecx,%ebx`).  0x1001d832's `jne 0x1001d840` falls into
//              it and 0x1001d8e5's `jmp 0x1001d834` re-enters it; it has no
//              prologue of its own.  Its 284 "bytes" are the distance to
//              0x1001d950, which is that function's `_AllocateTArray`.
//              (d3dr4c.cpp already records 0x1001d810 as a real, unwritten,
//              200-400 byte function -- this is what it is.)
//
//   0x1007f444 is the identical block of the identical function at
//              0x1007f420: the two are the same source emitted into two
//              objects (compare 0x1001d820..0x1001d83d with
//              0x1007f430..0x1007f44d -- same bytes, different call
//              displacements).  Same verdict.
//
//              SO BOTH HOSTS ARE WRITTEN BELOW -- 0x1001d810 (T = WORD:
//              `movzwl (%edx,%eax,2)` / `mov %dx,(%esi,%eax,2)`, one
//              `add %ecx,%ecx` for the byte count) and 0x1007f420 (T = 4
//              bytes: `mov (%ecx,%eax,4),%ecx`, two doublings).  They are
//              the same InternalNiceSetSize source as the two /O2 copies,
//              in /Os objects.  Getting those two right retires these two
//              rows; scoring them as separate functions is the
//              enumeration's business, not the source's.
//
//  ------------------------------------------------------------------ //
//  THIRTEEN ROWS ARE DECODED BUT NOT WRITTEN.  The
//  short version, so they need not be re-decoded:
//
//   0x1006c6e0 (311) two-entry.  `malloc(0x28)` (0x10081222), vptr
//                  0x10089800, nine zero stores + `movl $1,0x20`, then
//                  d3d_Alloc(0x80) into +0x0c, +0x14 = 0x2000, a
//                  DDSURFACEDESC2 built on the stack (dwSize 0x7c,
//                  dwFlags 0x40014, ddsCaps 0x1000, and a 0x20-byte
//                  DDPIXELFORMAT memcpy'd out of 0x1008e6c4+0x0c), then
//                  IDirectDraw7::CreateSurface (vtbl +0x18) into +0x1c.
//                  On success it links itself onto the caller's list
//                  (+0x24 = *pList, ++pList->m_nCount, *pList = this),
//                  adds 0x2000 to g_10091948 and its +0x14 to
//                  g_1008cbcc->+0x50.  A vertex-buffer allocator.
//   0x1007e8b0 (314) `Init(handle, w, h, a, b)` on a class with a vtable:
//                  slot +0x0c is Term, slot +0x1c is a virtual "create"
//                  taking one dword.  Contains TWO `jne` off ONE `test`
//                  (0x1007e91b/0x1007e91d) -- the deleted-arm
//                  pattern -- and reaches g_DefAlloc (0x10091df0) Alloc/
//                  Free directly.  Its buffer grows by `(m_24 + m_14) * 4`.
//   0x10064b70 -- WRITTEN below.
//   0x1007da80 (334) model animation blend: two node lists indexed by
//                  0x208(%ebx,%iAnim,4) and 0x228(...), 0x1c-byte
//                  per-animation records, an interpolation weight at
//                  0x14(%ebx,%iAnim*0x1c), a call to 0x10073500 and a
//                  three-component lerp `a + (b - a) * t` written into the
//                  caller's LTVector, then a second lerp over 0x20-byte
//                  records hanging off 0x168 of 0x1a4(%ebx) ADDED to that
//                  vector.  Two `rep movsl` ecx=3 confirm LTVector.
//   0x1001e770 -- WRITTEN below.
//   0x10069ca0 (360) two-entry.  Opens a file through 0x1007fa80(name,
//                  &pStream, 0), memsets two 0x17c-byte structures and
//                  stamps 0x17c into each (a dwSize-carrying descriptor
//                  pair), calls the stream's vtable +0x2c with them,
//                  d3d_Alloc(0xec) (0x10021c20), copies 0x10 bytes from
//                  the caller's argument into +0x10, sets +0x20 =
//                  this+0x10 (or 0 when that argument is NULL), strncpy's
//                  two 0x64-byte names into +0x24 and +0x88, then links
//                  the record onto the doubly-linked list whose head is
//                  0x1008bf78/0x1008bf7c.  Always closes the stream
//                  (vtable +0x08) and returns TRUE.
//   0x10066d10 (384) two-entry.  Walks a `count`-long array of object
//                  pointers; stamps a global frame stamp (0x1008bea8,
//                  a WORD) into each object's +0x42 and skips the ones
//                  already stamped; then `Dot(pDir, pObj->+0x28->+0x34) -
//                  dist > 0.0001f` (the constant at
//                  0x100aaf44), a `+0x38 & 4` reject, and either records the
//                  object in a 0x20-entry table at 0x140/0x40 of the
//                  manager (0x1007b8d0) or dispatches it through one of
//                  four function pointers at 0x1008ce94/98/9c/a0 chosen by
//                  bits 0x80 and 0x8000 of that +0x38.
//   0x10056ab4 (413) an intrusive-list "find or create" over a
//                  0x8c-stride and a 0x1dc-stride table, with a free list
//                  at +0x7f0 and a bank refill call at 0x1007fbf0; the
//                  found/created record is tied onto two circular lists
//                  (+0x10/+0x14 and +0x1c/+0x20) and onto the caller's
//                  link.  Four `dl_Insert`-shaped tie-offs, no arithmetic.
//   0x1005f580 (428) a 0x94-byte frame that stages ~20 globals into an
//                  argument block, calls 0x10020d80 with a 0x3c23d70a
//                  (0.01f) constant, then 0x1001ef30 and 0x10030a60.  It
//                  is a "draw the debug/wireframe pass" driver; almost
//                  every byte is a global-to-slot move, so it is
//                  reconstructible but needs the two callees' prototypes
//                  first.
//   0x10009ae0 (432) five identical `pObj->vtbl->+0x10(pObj, w, h, 0|1,
//                  g_1008cb14)` calls on five global objects
//                  (0x1008ad4c, 0x1008ad10, 0x1008acd4, 0x1008ac98,
//                  0x10091d80) with w/h clamped to >= 1 and w rounded to
//                  a multiple of 3 (`mov $3,%esi; cltd; idiv; lea
//                  (%eax,%eax,2)`), then g_10091de0 = g_1008abb0.  A
//                  render-target reallocation.
//   0x10019874 (432) the same inline "ensure the array is at least
//                  vtbl->m_64 (resp. m_78) elements" helper written TWICE,
//                  over (0x62c,0x630,0x634,0x638) with a `shl $0x5`
//                  element and (0x640,0x644,0x648,0x64c) with `shl $0x6`.
//                  Each copy has the TWO-`jne`-off-one-`test` deleted-arm
//                  shape.  It wants an `inline BOOL Ensure(...)` in the
//                  source; the two copies must be spelled once.
//   0x10008a44 (462) the renderer's per-frame setup: chooses one of two
//                  (stride, cb, cb, size, bank) quintuples by
//                  m_428 == 1, then walks m_pWorld->+0x38[0..m_pWorld->
//                  +0x3c) doing per-"sky/portal" work.  Long, and its
//                  callee 0x10009ae0 is in this same list.
//   0x10058e64 (477) `DrawPolyMgr::DrawPolyAdditionalPass` -- the string
//                  at 0x100aa7b0 names it.  0x1448-byte frame (`_chkstk`
//                  at 0x100805a0), a 0x1428-byte vertex scratch, and four
//                  dispatch tables at 0x10089738/44/4c/5c indexed by the
//                  pass descriptor's +0x10/+0x14/+0x1c.
//   0x10035c30 -- WRITTEN below.
//   0x10068a50 -- WRITTEN below.
//   0x1004fea4 (503) a shell sort of 12-byte {ptr, ?, float} records:
//                  ten passes over the gap tables 0x10089560
//                  {9,8,7,6,5,4,3,2,1,0} (shift) and 0x10089588
//                  {512,...,1} (gap), each pass comparing
//                  `a[j<<s].key < a[(j<<s)+gap].key` with the flag bit
//                  0x40 of each element's +0x88 taken first (an integer
//                  compare when the two differ, the float compare when
//                  they agree), swapping the three dwords and backing up
//                  by one.  Ends with a `d3d_Warning(0, "Sort: %d ticks")`
//                  gated on 0x1008ef60.
//   0x1007f150 (513) not decoded past its prologue in the time available.
//
// *********************************************************************** //

// Only ltbasedefs.h, as d3dr4b.cpp has it.  memcpy needs no include of
// its own: ltbasedefs.h already pulls in <string.h> (and <stdlib.h>),
// which is what lets icl treat memcpy as the intrinsic it expands
// inline at 0x1007c290 and calls at 0x1007f420.
#include "ltbasedefs.h"


// ======================================================================= //
//  SHARED DECLARATIONS
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  stdlith's LAlloc (l_allocator.h), reached as an OBJECT at 0x10091df0
//  and as a pointer parameter everywhere else.  Declared exactly as
//  d3dr4b.cpp / d3dbatchb.cpp declare it -- never defined, so no vtable
//  for it is emitted here.  Slot +0x04 is Alloc (one pushed dword, %ecx =
//  the object) and slot +0x08 is Free; slot +0x00 is only present to put
//  them at those offsets.
// ---------------------------------------------------------------------- //
class IAllocator
{
public:
	virtual void* Slot00(uint32 nSize) = 0;             // +0x00
	virtual void* Alloc(uint32 nSize) = 0;              // +0x04
	virtual void  Free(void* pMem) = 0;                 // +0x08
};


// ---------------------------------------------------------------------- //
//  dynarray.h's DefaultCache.  m_CacheSize at +0x00 (0x1000a690 reads
//  `(%ecx)`, 0x1000a680 writes it) and m_WantedCache at +0x04 (0x1000a670
//  reads `0x4(%ecx)`).  All three accessors are OUT OF LINE in the shipped
//  build, which is what /Ob1 does to a member function that is not marked
//  `inline` -- so they are declared here and never defined.
// ---------------------------------------------------------------------- //
struct DefaultCache
{
	uint32 m_CacheSize;                                 // +0x00
	uint32 m_WantedCache;                               // +0x04

	uint32 GetCacheSize() const;                        // 0x1000a690
	void   SetCacheSize(uint32 val);                    // 0x1000a680
	uint32 GetWantedCache() const;                      // 0x1000a670
};


// ======================================================================= //
//  FAMILY A -- CMoArray<T,DefaultCache>::Insert2(DWORD, const T&, LAlloc*)
//
//      BOOL Insert2( DWORD index, const T &toInsert, LAlloc *pAlloc )
//      {
//          T *pNewArray;  DWORD newSize, i;
//
//          if(index > m_nElements) return FALSE;
//          newSize = m_nElements + 1;
//          if( m_Cache.GetCacheSize() == 0 ) { ...grow... }
//          else                              { ...slide... }
//          ++m_nElements;
//          return TRUE;
//      }
//
//  WHAT PINS IT, instruction by instruction (0x10021ad0):
//   * `mov 0x8(%ebx),%edx; cmp %edx,%esi; jbe` is `index > m_nElements`
//     with the SOURCE's operand order preserved -- index is the cmp's
//     destination because the source wrote `index > m_nElements`.
//   * `lea 0xc(%ebx),%ecx; call 0x1000a690` then `test %eax,%eax; je` is
//     `m_Cache.GetCacheSize() == 0`, and the je goes to the GROW arm --
//     so the slide arm is the fall-through, i.e. the source's `if` tests
//     the cache being zero and the grow arm is the `if` body.  (The `je`
//     jumping FORWARD past the slide code is icl sinking the larger arm.)
//   * `lea 0x1(%eax,%edx,1),%eax` after GetWantedCache is
//     `newSize + m_Cache.GetWantedCache()` with newSize = m_nElements + 1
//     folded into the lea's displacement -- which is why `newSize` is a
//     variable in the source and yet nothing ever adds one.
//   * the two copy loops are `pNewArray[i] = m_pArray[i]` for i<index and
//     `pNewArray[i+1] = m_pArray[i]` for index<=i<m_nElements: the second
//     stores at `0x4(%edi,%eax,4)` and RELOADS `0x8(%ebx)` every trip,
//     which is the header's un-cached `i < m_nElements` bound.
//   * `mov -0x8(%ebp),%eax; mov (%eax),%eax; mov %eax,(%edx,%esi,4)` is
//     `pNewArray[index] = toInsert` through the reference.
//   * `call 0x10021a80` NESTED inside the `_DeleteAndDestroyArray` call's
//     argument block is `_DeleteAndDestroyArray(pAlloc,
//     GetNumAllocatedElements())` -- pAlloc at (%esp), the count at
//     0x4(%esp), matching that function's declared parameter order.
//   * `incl 0x8(%ebx)` reached by BOTH arms is the single `++m_nElements`
//     after the if/else, and the `xor %eax,%eax; inc %eax` next to it is
//     the single `return TRUE`.
// ======================================================================= //

// ---- 0x10021ad0 -- T is 4 bytes; _AllocateTArray 0x10021aa0 doubles its
//      count twice (`add %eax,%eax` twice) for the byte size.   /Os.
struct Arr10021ad0
{
	uint32*      m_pVTable;                             // +0x00
	uint32*      m_pArray;                              // +0x04
	uint32       m_nElements;                           // +0x08
	DefaultCache m_Cache;                               // +0x0c

	uint32* _AllocateTArray(uint32 nElements, IAllocator* pAlloc);   // 0x10021aa0
	void    _DeleteAndDestroyArray(IAllocator* pAlloc, uint32 n);    // 0x1001cf00
	uint32  GetNumAllocatedElements();                               // 0x10021a80

	int32   Insert2(uint32 index, const uint32& toInsert,
	                IAllocator* pAlloc);                             // 0x10021ad0
};



// ---- 0x10009d60 -- the SAME source, T also 4 bytes, in an /O2 object.
//      Its helpers are 0x10009cc0 / 0x10009cf0 / 0x10009c90.
struct Arr10009d60
{
	uint32*      m_pVTable;                             // +0x00
	uint32*      m_pArray;                              // +0x04
	uint32       m_nElements;                           // +0x08
	DefaultCache m_Cache;                               // +0x0c

	uint32* _AllocateTArray(uint32 nElements, IAllocator* pAlloc);   // 0x10009cc0
	void    _DeleteAndDestroyArray(IAllocator* pAlloc, uint32 n);    // 0x10009cf0
	uint32  GetNumAllocatedElements();                               // 0x10009c90

	int32   Insert2(uint32 index, const uint32& toInsert,
	                IAllocator* pAlloc);                             // 0x10009d60
};

int32 Arr10009d60::Insert2(uint32 index, const uint32& toInsert,
                           IAllocator* pAlloc)
{
	uint32* pNewArray;
	uint32  newSize, i;

	if (index > m_nElements)
		return 0;

	newSize = m_nElements + 1;

	if (m_Cache.GetCacheSize() == 0)
	{
		pNewArray = _AllocateTArray(newSize + m_Cache.GetWantedCache(), pAlloc);
		if (!pNewArray)
			return 0;

		for (i = 0; i < index; i++)
			pNewArray[i] = m_pArray[i];

		for (i = index; i < m_nElements; i++)
			pNewArray[i + 1] = m_pArray[i];

		pNewArray[index] = toInsert;

		if (m_pArray)
		{
			_DeleteAndDestroyArray(pAlloc, GetNumAllocatedElements());
		}

		m_Cache.SetCacheSize(m_Cache.GetWantedCache());
		m_pArray = pNewArray;
	}
	else
	{
		for (i = m_nElements; i > index; i--)
			m_pArray[i] = m_pArray[i - 1];

		m_Cache.SetCacheSize(m_Cache.GetCacheSize() - 1);

		m_pArray[index] = toInsert;
	}

	++m_nElements;

	return 1;
}


// ======================================================================= //
//  FAMILY B -- CMoArray<T,DefaultCache>::InternalNiceSetSize
//
//      BOOL InternalNiceSetSize( DWORD newSize, BOOL bFast, LAlloc *pAlloc )
//
//  WHAT PINS IT: the header's else-if chain compares newSize against
//  m_nElements THREE TIMES and icl emits THREE `cmp %eax,%edx` off the
//  same two registers --
//
//      3b d0  cmp %eax,%edx ; 0f 82  jb  shrink      newSize <  m_nElements
//      3b d0  cmp %eax,%edx ; 0f 87  ja  grow        newSize >  m_nElements
//      3b d0  cmp %eax,%edx ; 75     jne alloc       newSize == m_nElements
//
//  which is the "several branches off one compare" signature and
//  is only produced by three separate source comparisons, not by a
//  switch or by a cached result.  The `grow` arm's own
//  `(m_nElements + GetCacheSize()) >= newSize` failure jumps BACK to the
//  third compare (`jb 0x1007c2bb`), which is icl routing the else-if
//  chain -- more proof that the chain is written exactly as the header
//  writes it.
//
//  Other pins:
//   * TWO GetCacheSize calls in the grow arm == two in the source
//     (the guard `(m_nElements + GetCacheSize()) >= newSize` and then
//     `SetCacheSize(GetCacheSize() - (newSize - m_nElements))`).
//   * the shrink arm computes `m_nElements - newSize` FIRST and adds the
//     cache size to it (`sub %ecx,%edx; add %eax,%edx`) -- the source's
//     own parenthesisation, `GetCacheSize() + (m_nElements - newSize)`.
//   * `mov 0x8(%ebx),%edx; mov 0x8(%ebp),%eax; cmp %eax,%edx; jbe; mov
//     %eax,%edx` is `nToCopy = m_nElements; if(nToCopy > newSize) nToCopy
//     = newSize;` -- two statements, not a ?: (a ?: puts both arms
//     through one register and loses the `jbe` over a single `mov`).
//   * `mov 0xc(%ebp),%eax; test %eax,%eax; jne` is `if(bFast)`, with the
//     memcpy arm SUNK past the element loop -- the fall-through is the
//     `else`.
// ======================================================================= //

// ---- 0x1007c290 -- T is 4 bytes.  memcpy is expanded inline here
//      (`push %ecx; shr $0x2,%ecx; rep movsl; pop %ecx; and $0x3,%ecx;
//      rep movsb`) with the byte count built by two `add %edx,%edx`, so
//      the source really does say memcpy(dst, src, sizeof(T) * nToCopy).
//      /O2.
struct Arr1007c290
{
	uint32*      m_pVTable;                             // +0x00
	uint32*      m_pArray;                              // +0x04
	uint32       m_nElements;                           // +0x08
	DefaultCache m_Cache;                               // +0x0c

	uint32* _AllocateTArray(uint32 nElements, IAllocator* pAlloc);   // 0x10021aa0
	void    _DeleteAndDestroyArray(IAllocator* pAlloc, uint32 n);    // 0x1001cf00
	uint32  GetNumAllocatedElements();                               // 0x10021a80

	int32   InternalNiceSetSize(uint32 newSize, int32 bFast,
	                            IAllocator* pAlloc);                 // 0x1007c290
};

int32 Arr1007c290::InternalNiceSetSize(uint32 newSize, int32 bFast,
                                       IAllocator* pAlloc)
{
	uint32* pNewArray;
	uint32  i, nToCopy;

	if (newSize < m_nElements)
	{
		m_Cache.SetCacheSize(m_Cache.GetCacheSize() + (m_nElements - newSize));
		m_nElements = newSize;
		return 1;
	}
	else if (newSize > m_nElements && (m_nElements + m_Cache.GetCacheSize()) >= newSize)
	{
		m_Cache.SetCacheSize(m_Cache.GetCacheSize() - (newSize - m_nElements));
		m_nElements = newSize;
		return 1;
	}
	else if (newSize == m_nElements)
	{
		return 1;
	}

	pNewArray = _AllocateTArray(newSize + m_Cache.GetWantedCache(), pAlloc);
	if (!pNewArray)
		return 0;

	nToCopy = m_nElements;
	if (nToCopy > newSize)
		nToCopy = newSize;

	if (bFast)
	{
		memcpy(pNewArray, m_pArray, sizeof(uint32) * nToCopy);
	}
	else
	{
		for (i = 0; i < nToCopy; i++)
		{
			pNewArray[i] = m_pArray[i];
		}
	}

	_DeleteAndDestroyArray(pAlloc, GetNumAllocatedElements());
	m_pArray = pNewArray;
	m_nElements = newSize;
	m_Cache.SetCacheSize(m_Cache.GetWantedCache());

	return 1;
}


// ---- 0x10009f10 -- the SAME source with a SIXTEEN-byte T (four dword
//      movs per element, `shl $0x4` in its _AllocateTArray 0x1000a0e0).
//      It is CMoArray<SavedVert589c4>'s, the array of DrawPolyMgr's saved
//      vertices, and is now the template itself: w64_10009f10_qip.cpp
//      instantiates the stock dynarray.h body, byte-identical at /O2 /Qip.
//      It was written here as a hand transcription over a stand-in
//      Arr10009f10 (identical too), while d3dr5b.cpp's copy of the real
//      template -- the one its NiceSetSize calls -- was a second, unplaced
//      definition of the same function.


// ---- 0x1001d810 (316 bytes, /Os) -- the SAME source with T = WORD, and
//      it is what accounts for listed rows 0x1001d834 and 0x1007f444:
//      0x1001d834 is this function's shared `return TRUE` block.
//
//      T = WORD is read off the element loop --
//          mov 0x4(%ebx),%edx ; movzwl (%edx,%eax,2),%edx
//          mov %dx,(%esi,%eax,2)
//      -- a genuine 16-bit load and 16-bit store, and off the byte count
//      the fast arm builds, `add %ecx,%ecx` (ONE doubling).  Its
//      _AllocateTArray is 0x1001d950; its _DeleteAndDestroyArray
//      (0x1001cf00) and GetNumAllocatedElements (0x10021a80) are the
//      T-INDEPENDENT ones the T=4 instantiation also uses, which is
//      COMDAT folding, not a different function.
//
//      /Os: every one of the four `return TRUE` sites is a jump into the
//      block at 0x1001d834 (`e9 4a ff ff ff`, `e9 0e ff ff ff`,
//      `e9 eb fe ff ff` and the `jne 0x1001d840` fall-in), and the FALSE
//      exit at 0x1001d949 cross-jumps into its tail.
struct Arr1001d810
{
	uint32*      m_pVTable;                             // +0x00
	uint16*      m_pArray;                              // +0x04
	uint32       m_nElements;                           // +0x08
	DefaultCache m_Cache;                               // +0x0c

	uint16* _AllocateTArray(uint32 nElements, IAllocator* pAlloc);   // 0x1001d950
	void    _DeleteAndDestroyArray(IAllocator* pAlloc, uint32 n);    // 0x1001cf00
	uint32  GetNumAllocatedElements();                               // 0x10021a80

	int32   InternalNiceSetSize(uint32 newSize, int32 bFast,
	                            IAllocator* pAlloc);                 // 0x1001d810
};



// ---- 0x1007f420 (316 bytes, /Os) -- T = 4 bytes again
//      (`mov (%ecx,%eax,4),%ecx`, `add %edx,%edx` TWICE), helpers
//      0x10009cc0 / 0x10009cf0 / 0x10009c90, i.e. the same instantiation
//      as 0x10009d60's Insert2 but in an /Os object.  0x1007f444 is its
//      shared `return TRUE` block.
//
//      WORTH RECORDING: this copy CALLS memcpy (0x100805d0) where
//      0x1007c290 -- the /O2 object, same T, same source line -- expands
//      it inline.  So the inline-vs-call decision on memcpy is per
//      OBJECT, not per element type, and a call turning up where you
//      expected an expansion is a flag symptom, not a source difference.
struct Arr1007f420
{
	uint32*      m_pVTable;                             // +0x00
	uint32*      m_pArray;                              // +0x04
	uint32       m_nElements;                           // +0x08
	DefaultCache m_Cache;                               // +0x0c

	uint32* _AllocateTArray(uint32 nElements, IAllocator* pAlloc);   // 0x10009cc0
	void    _DeleteAndDestroyArray(IAllocator* pAlloc, uint32 n);    // 0x10009cf0
	uint32  GetNumAllocatedElements();                               // 0x10009c90

	int32   InternalNiceSetSize(uint32 newSize, int32 bFast,
	                            IAllocator* pAlloc);                 // 0x1007f420
};



// 0x10032f30 / 0x10032f34 moved to w32_10032d00_qip.cpp: both callers
// enter the /Qip register entry, and one of them, 0x10032d00, is there.


// ======================================================================= //
//  0x100395f0   317 bytes.  TEAR DOWN THE TEXTURE/SURFACE SUBSYSTEM.
//
//  WHAT PINS IT:
//   * the whole body is guarded by `mov 0x1008da70,%eax; test; jne` with
//     the jump going INTO the work -- so the source's guard is
//     `if (!g_bInitialised) return;` and the last statement of the body is
//     the store that clears the same global.  (The epilogue is duplicated
//     at 0x1003960a and 0x1003971e: /O2.)
//   * 0x1007fba0 walks a singly-linked chain out of its argument's +0x14
//     calling d3d_Free (0x10021cb0) on every node and then zeroes +0x14
//     and +0x18 -- a memory bank's Term.  0x1007fb20 writes an element
//     size at +0x00, rounds it up to a multiple of four into +0x04 and
//     takes a third argument -- the matching Init(size, count).  The
//     arguments here are (0x4c, 0x20).
//   * BOTH bank calls address their object as `mov $0x1008e640,%eax;
//     add $0x4,%eax` -- recomputed, not hoisted, which is exactly what
//     a repeated `&g_X.m_Member` expression does, and what
//     tells you 0x1008e640 is a STRUCT whose bank member sits at +0x04
//     rather than a bank at 0x1008e644.
//   * the three `if (p) { p->lpVtbl->Release(p); p = NULL; }` blocks are
//     three separate statements: each reloads its global AFTER the test
//     (`mov g,%eax; test %eax,%eax; je; mov g,%eax; mov (%eax),%edx`).
//   * the list walk is an LTLink circular list whose head node IS the two
//     dwords at 0x1008e6a4/0x1008e6a8: the loop starts from the head's
//     m_pNext, frees each node's m_pData (+0x08), follows m_pNext (+0x04)
//     and stops when it reaches the head's own address, and the tie-off
//     afterwards stores that address into both dwords.
//   * the tail zeroes SEVEN consecutive dwords off ONE base register
//     (`mov $0x1008e6b0,%edi` then (%edi) .. 0x18(%edi)) -- seven member
//     assignments in one aggregate, not seven separate globals, which
//     would each have been an absolute `movl $0x0,imm32`.
//
//  WHAT CLOSED IT (was 304 bytes, 179 differ), each measured:
//   * the bank: `&g_1008e640.m_Bank` folds to one `movl $g+4`.  Only a
//     pointer LOCAL initialised to &g_1008e640 at its declaration gives
//     `mov $g,%eax; add $0x4,%eax`, rematerialised at both calls.  (The
//     same local assigned after the guard is held in %ebx instead; inline
//     member wrappers fold like the plain expression.)
//   * the tail is `memset(&g_1008e6b0, 0, sizeof)`: the intrinsic is what
//     puts the base in %edi (and so saves %edi, the 0x20 frame).  Seven
//     member stores go to absolute addresses.
//   * the walk reads m_pNext into a local BEFORE the d3d_Free call.
//   * the reload after each test: no spelling found that makes icl reload
//     -- C++ virtual Release, an inline SafeRelease by reference or by
//     pointer, `!= LTNULL`, casts, a local copy, an array with an unrolled
//     loop, and the flags /Oi- /Qip /Qrcd /Op /Ow /Oa /Og- all keep one
//     load.  A volatile read AT THE TEST (the `*(volatile T*)&x`
//     lever) is the one construct that reproduces it; it is written that
//     way below as a measured lever, not as a claim about the original.
// ======================================================================= //

struct MemBank
{
	uint32 m_nElementSize;                              // +0x00
	uint32 m_nRoundedSize;                              // +0x04
	uint32 m_pad_08[3];                                 // +0x08
	void*  m_pBlocks;                                   // +0x14
	uint32 m_nUnknown18;                                // +0x18
};

extern void sub_1007fba0(MemBank* pBank, uint32 bFlag);      // 0x1007fba0
extern void sub_1007fb20(MemBank* pBank, uint32 nElemSize,
                         uint32 nCount);                     // 0x1007fb20
extern void sub_10039930();                                  // 0x10039930
extern void d3d_Free(void* pMem);                            // 0x10021cb0

// The record 0x1008e640 is the head of; only its bank member is touched
// here, and the bank is at +0x04.
struct Unknown1008e640
{
	uint32  m_nUnknown00;                               // +0x00
	MemBank m_Bank;                                     // +0x04
};

extern Unknown1008e640 g_1008e640;                      // 0x1008e640

// A COM object with nothing but IUnknown reached here.
struct Releasable;
struct ReleasableVtbl
{
	uint32 m_pad_00[2];
	void (__stdcall* Release)(Releasable*);             // +0x08
};
struct Releasable
{
	ReleasableVtbl* lpVtbl;
};

extern Releasable* g_p1008da74;                         // 0x1008da74
extern Releasable* g_p1008da78;                         // 0x1008da78
extern Releasable* g_p1008da7c;                         // 0x1008da7c
extern LTBOOL      g_b1008da70;                         // 0x1008da70

// The seven dwords at 0x1008e6b0, cleared as one aggregate.
struct Unknown1008e6b0
{
	uint32 m_n00, m_n04, m_n08, m_n0c, m_n10, m_n14, m_n18;
};

extern Unknown1008e6b0 g_1008e6b0;                      // 0x1008e6b0

// stdlith's LTLink, the circular list node this module uses.
struct D3DLink
{
	D3DLink* m_pPrev;                                   // +0x00
	D3DLink* m_pNext;                                   // +0x04
	void*    m_pData;                                   // +0x08
};

extern D3DLink g_List1008e6a4;                          // 0x1008e6a4


void sub_100395f0()
{
	Unknown1008e640* pRec = &g_1008e640;
	D3DLink* pCur;
	D3DLink* pNext;

	if (!g_b1008da70)
		return;

	sub_10039930();

	sub_1007fba0(&pRec->m_Bank, 1);
	sub_1007fb20(&pRec->m_Bank, 0x4c, 0x20);

	if (*(Releasable* volatile*)&g_p1008da7c)
	{
		g_p1008da7c->lpVtbl->Release(g_p1008da7c);
		g_p1008da7c = LTNULL;
	}

	if (*(Releasable* volatile*)&g_p1008da78)
	{
		g_p1008da78->lpVtbl->Release(g_p1008da78);
		g_p1008da78 = LTNULL;
	}

	if (*(Releasable* volatile*)&g_p1008da74)
	{
		g_p1008da74->lpVtbl->Release(g_p1008da74);
		g_p1008da74 = LTNULL;
	}

	pCur = g_List1008e6a4.m_pNext;
	while (pCur != &g_List1008e6a4)
	{
		pNext = pCur->m_pNext;
		d3d_Free(pCur->m_pData);
		pCur = pNext;
	}

	g_List1008e6a4.m_pNext = &g_List1008e6a4;
	g_List1008e6a4.m_pPrev = &g_List1008e6a4;

	memset(&g_1008e6b0, 0, sizeof(g_1008e6b0));

	g_b1008da70 = LTFALSE;
}


// ======================================================================= //
//  THE OBJECT-SET FAMILY -- 0x10064b70 and 0x10068a50
//
//  BOTH functions add render objects to a bounded "set", and the set is a
//  0x1c-byte record whose field offsets are established TWICE over,
//  independently:
//
//   * 0x10064b70 uses three of them inside the manager that 0x1007b8d0
//     returns (0x10091a10), at +0x1e4, +0x204 and +0x224.  Within each,
//     the element array is at +0x00, the count at +0x04, the limit at
//     +0x14 and the name at +0x18:
//         0x1e4/0x1e8/0x1f8/0x1fc
//         0x204/0x208/0x218/0x21c
//         0x224/0x228/0x238/0x23c
//   * 0x10068a50 builds one on its own STACK at -0x1020 and uses exactly
//     the same four deltas (-0x1020/-0x101c/-0x100c/-0x1008), and the
//     0x400-entry array it points the set at starts at -0x1004 -- which
//     is -0x1020 + 0x1c, so the record is 0x1c bytes and nothing follows
//     it.
//
//  The add itself is expanded INLINE at all four sites -- three in
//  0x10064b70 and one in 0x10068a50 -- with the count's increment folded
//  onto the owning object (`incl 0x208(%eax)`, `incl -0x101c(%ebp)`), so
//  the source marks the helper `inline` (this build is /Ob1: nothing else
//  gets inlined).  The overflow arm is AddDebugMessage(1, "Set '%s'
//  overflowed", pSet->m_pszName) -- the format string is at 0x100a9a10
//  and 0x10021d60 is LithTech's AddDebugMessage (d3dblit.cpp calls it
//  d3d_Warning; declared below as sub_10021d60, and why the name matters).
//
//  The "which set" test is a flags dword fetched by a SECOND inline
//  helper, and its five-instruction pointer chain is byte-identical in
//  the two functions:
//
//      mov 0x1a8(%ebx),%eax ; test %eax,%eax ; je  zero
//      mov (%ebx),%eax ; mov %ebx,%ecx ; mov 0x38(%eax),%eax ; call *%eax
//      test %eax,%eax ; je zero
//      mov 0x1a8(%ebx),%eax          <- the member RELOADED after the call
//      mov 0xa4(%eax),%edx ; test %edx,%edx ; jbe zero
//      mov 0xa0(%eax),%eax ; mov (%eax),%eax
//      mov 0x28(%eax),%eax ; mov 0x38(%eax),%eax
//
//  Two `xor %eax,%eax` blocks fall together into the join, which is the
//  helper's single `return 0` after tail merging.  0x10064b70 tests bit
//  0x8 of the result and 0x10068a50 tests bit 0x80000, so the helper is
//  shared and the bit is the caller's.
// ======================================================================= //

// 0x10021d60, LithTech's AddDebugMessage.  Declared as sub_10021d60 (the
// name d3dr5a.cpp and big331e0.cpp already link against) and not as
// `d3d_Warning`: icl 4.0 sinks a block that calls a function whose name
// contains "Warn" or "Error" below the epilogue, and the shipped overflow
// arms of 0x10064b70 / 0x10068a50 are inline.  Measured: with the old name
// 0x10064b70 was 196 bytes off (arms sunk), with this one it is identical;
// see d3dr5b_o2.cpp for the probe.
extern void sub_10021d60(uint32 nLevel, const char* pszMsg, ...);   // 0x10021d60

struct D3DRenderStyle
{
	uint8  m_pad_00[0x38];
	uint32 m_nFlags;                                    // +0x38
};

struct D3DModelPiece
{
	uint8           m_pad_00[0x28];
	D3DRenderStyle* m_pRenderStyle;                     // +0x28
};

struct D3DModelInstance
{
	uint8           m_pad_00[0xa0];
	D3DModelPiece** m_pPieces;                          // +0xa0
	uint32          m_nPieces;                          // +0xa4
};

class D3DObject
{
public:
	// Fourteen slots ahead of the one this family calls; the call is
	// `mov %ebx,%ecx; call *0x38(%eax)` with nothing pushed and a
	// dword answer, i.e. an ordinary virtual __thiscall taking no
	// arguments.
	virtual void  _Slot00();  virtual void  _Slot04();
	virtual void  _Slot08();  virtual void  _Slot0c();
	virtual void  _Slot10();  virtual void  _Slot14();
	virtual void  _Slot18();  virtual void  _Slot1c();
	virtual void  _Slot20();  virtual void  _Slot24();
	virtual void  _Slot28();  virtual void  _Slot2c();
	virtual void  _Slot30();  virtual void  _Slot34();
	virtual int32 Slot38();                             // vtable +0x38

public:
	uint8             m_pad_04[0x84];                   // +0x04
	uint32            m_nFlags;                         // +0x88
	uint32            m_nFlags2;                        // +0x8c
	uint8             m_pad_90[0x118];                  // +0x90
	D3DModelInstance* m_pInstance;                       // +0x1a8
};

// The bounded set.  0x1c bytes; the three dwords at +0x08..+0x13 are
// never touched by either function and are named by offset.
struct D3DObjectSet
{
	D3DObject** m_pElements;                            // +0x00
	uint32      m_nElements;                            // +0x04
	uint32      m_nUnknown08;                           // +0x08
	uint32      m_nUnknown0c;                           // +0x0c
	void*       m_pUnknown10;                           // +0x10
	uint32      m_nMaxElements;                         // +0x14
	const char* m_pszName;                              // +0x18
};

// 0x1007b8d0 is `mov $0x10091a10,%eax; ret` -- it hands back the address
// of the manager, so it is an accessor, not a lookup.
struct D3DObjectMgr
{
	uint8        m_pad_000[0x1e4];                      // +0x000
	D3DObjectSet m_Set1e4;                              // +0x1e4 (0x1c)
	uint32       m_nUnknown200;                         // +0x200
	D3DObjectSet m_Set204;                              // +0x204 (0x1c)
	uint32       m_nUnknown220;                         // +0x220
	D3DObjectSet m_Set224;                              // +0x224 (0x1c)
};

extern D3DObjectMgr* sub_1007b8d0();                    // 0x1007b8d0


inline void d3d_AddToObjectSet(D3DObjectSet* pSet, D3DObject* pObject)
{
	if (pSet->m_nElements < pSet->m_nMaxElements)
	{
		pSet->m_pElements[pSet->m_nElements] = pObject;
		pSet->m_nElements++;
	}
	else
	{
		sub_10021d60(1, "Set '%s' overflowed", pSet->m_pszName);
	}
}

inline uint32 d3d_GetObjectRenderFlags(D3DObject* pObject)
{
	if (pObject->m_pInstance && pObject->Slot38())
	{
		if (pObject->m_pInstance->m_nPieces > 0)
			return pObject->m_pInstance->m_pPieces[0]->m_pRenderStyle->m_nFlags;

		return 0;
	}

	return 0;
}


// ---------------------------------------------------------------------- //
//  0x10064b70   330 bytes.  FILE THE OBJECT INTO ONE OF THREE SETS.
//
//  __cdecl, one argument (`mov 0x8(%ebp),%ebx`, plain `ret`); its address
//  is in two dispatch tables (.data 0x100895e8 and 0x10089674), which is
//  what makes it __cdecl rather than the register entry its neighbours
//  use.
//
//  ARM ORDER: `test $0x8,%al; jne` sinks the first arm to the very end of
//  the function and `test $0x2,%al; je` sinks the third past the second,
//  so the second arm is the fall-through.  That is the plain
//  if / else-if / else order written below -- icl lays the arms out
//  backwards.  All three arms end `jmp 0x10064cb3`, one shared epilogue.
// ---------------------------------------------------------------------- //
void sub_10064b70(D3DObject* pObject)
{
	D3DObjectMgr* pMgr;

	if (d3d_GetObjectRenderFlags(pObject) & 0x8)
	{
		pMgr = sub_1007b8d0();
		d3d_AddToObjectSet(&pMgr->m_Set204, pObject);
	}
	else if (pObject->m_nFlags2 & 0x2)
	{
		pMgr = sub_1007b8d0();
		d3d_AddToObjectSet(&pMgr->m_Set224, pObject);
	}
	else
	{
		pMgr = sub_1007b8d0();
		d3d_AddToObjectSet(&pMgr->m_Set1e4, pObject);
	}
}


// ---------------------------------------------------------------------- //
//  0x10068a50   502 bytes.  SWEEP THE OBJECT LIST INTO A LOCAL SET.
//
//  __cdecl, NO arguments (plain `ret`, nothing read above %ebp).  The
//  0x1040-byte frame goes through _chkstk (0x100805a0), which is what
//  `mov $0x1040,%eax; call` is.
//
//  WHAT PINS IT:
//   * the local set's five initialising stores are emitted in the order
//     +0x10, +0x00, +0x04, +0x18, +0x14, and the +0x10 one is
//     `lea -0x1020(%ebp),%eax` -- the set's OWN address.  That store is
//     written first below because a statement's position relative to the
//     initialised declarations is source-visible ("a statement
//     written before the initialised declarations stays before them").
//     THIS IS THE FILE'S MAIN COIN-FLIP.
//   * `mov 0x1009110c,%ecx; test %ecx,%ecx; jne` with the whole body
//     jumped to is the leading `if (!g) return;`.
//   * `movl $0x0,-0x101c(%ebp)` a SECOND time at 0x10068ac0, after
//     0x1007b8d0, is a second source statement -- the set is cleared
//     again once the manager is in hand.
//   * the two-way flag test is
//         if (pObj->m_nFlags & 1) { if (g && (pObj->m_nFlags2 & 1)) continue; }
//         else                    { if (g && !(pObj->m_nFlags & 0x400)) continue; }
//     read off `test $0x1,%al / je 0x10068b10`, then in the taken arm
//     `test $0x1,%al` on +0x8c with `jne skip`, and in the fall-through
//     arm `test $0x4,%ah` (bit 0x400 of the SAME +0x88 dword, still in
//     %eax) with `je skip`.  Both arms then reach 0x10068b23.
//   * `mov 0x208(%edi),%edx` is reloaded on EVERY path out of the body,
//     which is the loop's un-cached `i < pMgr->m_Set204.m_nElements`
//     bound.
//   * the second pass hoists `mov $0x10068910,%esi` out of the loop and
//     reloads `mov 0x1008ef3c,%ecx` inside it: a function address is
//     loop-invariant, a global pointer member expression is re-read.
//
//  NEAR, size-exact (was 16 bytes long, the overflow arm sunk): after
//  each of the two calls in the loop body shipped pops the outgoing area
//  (`add $0x10` / `add $0x8,%esp`) and THEN reloads the bound
//  `mov 0x208(%edi),%edx`; ours reloads first.  Not moved by a `continue`
//  after the add, a while loop with the increment at a label, or /Oi-,
//  /Qip, /Qrcd.
// ---------------------------------------------------------------------- //
extern LTBOOL g_b1009110c;                              // 0x1009110c
extern LTBOOL g_b1008bd54;                              // 0x1008bd54

extern void sub_100688e0(void* pUnknown, D3DObject* pObject);   // 0x100688e0
extern uint32 g_1008b880;                               // 0x1008b880

// 0x100500a0 -- a __thiscall member (this stays in %ecx, two pushed
// dwords) of whatever 0x1008ef3c points at; the second argument is the
// address of the function at 0x10068910.
class C1008ef3c
{
public:
	void sub_100500a0(D3DObject* pObject, void (*pfn)(void*));
};

extern C1008ef3c* g_p1008ef3c;                          // 0x1008ef3c
extern void       sub_10068910(void* pUnknown);          // 0x10068910


void sub_10068a50()
{
	D3DObject*    apObjects[0x400];
	D3DObjectSet  objectSet;
	D3DObjectMgr* pMgr;
	D3DObjectSet* pSet;
	uint32        i;

	objectSet.m_pUnknown10 = &objectSet;

	objectSet.m_pElements    = apObjects;
	objectSet.m_nElements    = 0;
	objectSet.m_pszName      = "";
	objectSet.m_nMaxElements = 0x400;

	if (!g_b1009110c)
		return;

	pMgr = sub_1007b8d0();

	objectSet.m_nElements = 0;

	// The walk goes through a pointer to the set: read straight off pMgr,
	// the count reload after each call is scheduled ABOVE the call's
	// `add %esp` (17 bytes differ); through pSet it lands below, as
	// shipped (measured).
	pSet = &pMgr->m_Set204;

	for (i = 0; i < pSet->m_nElements; i++)
	{
		D3DObject* pObject = pSet->m_pElements[i];

		if (pObject->m_nFlags & 0x1)
		{
			if (g_b1008bd54 && (pObject->m_nFlags2 & 0x1))
				continue;
		}
		else
		{
			if (g_b1008bd54 && !(pObject->m_nFlags & 0x400))
				continue;
		}

		if (d3d_GetObjectRenderFlags(pObject) & 0x80000)
		{
			d3d_AddToObjectSet(&objectSet, pObject);
		}
		else
		{
			sub_100688e0(&g_1008b880, pObject);
		}
	}

	// `test ; jbe ; mov $0x0,%ebx ; jbe` -- the count tested, the counter
	// zeroed with a flag-preserving mov, and the SAME flags tested again by
	// the for's own guard: an `if (n > 0)` around the loop.
	if (objectSet.m_nElements > 0)
	{
		for (i = 0; i < objectSet.m_nElements; i++)
		{
			g_p1008ef3c->sub_100500a0(apObjects[i], sub_10068910);
		}
	}
}


// ======================================================================= //
//  0x10035c30 -- MOVED to d3dr3d.cpp.  Its bytes are /Os, not /O2:
//  `push %edi/%esi/%ebx` after `sub $0x18` (not frame-slot saves),
//  `xor %eax,%eax; inc %eax` for the literal 1 at three sites, and no
//  alignment `nop` ahead of the loop.  This file compiled /Os makes it
//  byte-identical unchanged; d3dr3d.cpp is the /Os file it went to.
// ======================================================================= //


// ======================================================================= //
//  0x1001e770   356 bytes.  IS THE OBJECT'S BOX INSIDE THE SIX PLANES?
//
//  __cdecl, ONE argument (`push %ebp; push %ebx; mov 0xc(%esp),%eax`) and
//  NO frame pointer -- the only function in this batch without one.  Its
//  address is planted into a local structure at 0x1007b69c
//  (`movl $0x1001e770,-0x2c(%ebp)`), so it is a callback slot, which is
//  why the object it takes is anonymous.
//
//  WHAT IT DOES: builds the eight corners of the box (centre at +0x0c,
//  half-dimensions at +0x18) and tests them against the six planes at
//  0x1008bc50 (stride 0x10 -- `add $0x10,%ebx; cmp $0x60,%ebx; jl`, i.e.
//  six LTPlanes).  If EVERY corner of the box is on the negative side of
//  ANY one plane the answer is FALSE; otherwise TRUE.
//
//  WHAT PINS IT:
//   * the frame is 0x64 and the twenty-four `fstps` walk 0x00, 0x04 ...
//     0x5c in ASCENDING order in eight groups of three -- the rule
//     "contiguous ascending frame slots are an array", so this is
//     LTVector v[8] and not eight named locals.  The remaining dword of
//     the 0x64 is icl's.
//   * the inner loop walks a POINTER (`mov %esp,%edx` ... `add $0xc,%edx`)
//     against `lea 0x60(%esp),%ebp`, which is the strength reduction of
//     an indexed `v[j]` -- write the indexed form.
//   * the constant at 0x100a50a4 reads as the four bytes 00 00 00 00, so
//     the comparison is against 0.0f, and `fcomps` + `fnstsw`/`sahf` +
//     `jbe` (taken when st0 <= 0) sends the loop on to the NEXT CORNER,
//     so the source's test is `> 0.0f` and its body is the break.
//   * the dot product is emitted z-term first, y, faddp, x, faddp -- the
//     "last-term-backwards" association of a three-term float sum, i.e.
//     `n.x*v.x + n.y*v.y + n.z*v.z` written left to right.
//   * `fsub %st(1),%st` against the plane's +0x0c, which is LTPlane's
//     m_Dist, and %st(1) is loaded ONCE per plane before the corner loop:
//     the plane is the loop invariant, the corner is not.
//   * the plane's SIGNS: corner 0 is (+,+,+) and corner 7 is (-,-,-),
//     with the index's bit 0 selecting y, bit 1 x and bit 2 z -- read off
//     the eight `fadds`/`fsubs` groups in order.  Every `-` is
//     `flds centre; fsubs dims`, i.e. centre - dims, and never the other
//     way round.
//   * the two exits: `add $0x64,%esp; xor %eax,%eax; inc %eax` (TRUE,
//     reached when the plane loop runs out) and `add $0x64,%esp;
//     xor %eax,%eax; eb ed` (FALSE, reached when a corner loop completes
//     without breaking) -- the second CROSS-JUMPS, so this TU is /Os.
//
//  COIN-FLIP: `if (j == 8)`.  icl threads the inner loop's two exit edges
//  and emits no compare at all, so the shipped bytes do not say whether
//  the source wrote the `j == 8` test or a `goto` past a `return LTFALSE`
//  placed after the inner loop.
// ======================================================================= //

struct Obj1001e770
{
	uint8    m_pad_00[0x0c];                            // +0x00
	LTVector m_vPos;                                    // +0x0c
	LTVector m_vDims;                                   // +0x18
};

extern LTPlane g_Planes1008bc50[6];                     // 0x1008bc50




// *********************************************************************** //
//  COIN-FLIPS -- each is ONE build to settle, and the alternative spelling
//  is written out so it can be pasted in.
// *********************************************************************** //
//
//  1. 0x1001e770, the inner loop's two exits.  Written above as
//
//         for (j = 0; j < 8; j++) { if (... > 0.0f) break; }
//         if (j == 8) return LTFALSE;
//
//     which needs icl to thread the two exit edges and drop the compare
//     (the shipped code has no `cmp $0x8` anywhere).  If a `cmp $0x8,%edx`
//     turns up, the source was the goto form instead:
//
//         for (i = 0; i < 6; i++)
//         {
//             for (j = 0; j < 8; j++)
//             {
//                 if (... > 0.0f)
//                     goto NextPlane;
//             }
//             return LTFALSE;
//         NextPlane:;
//         }
//         return LTTRUE;
//
//  2. 0x10068a50, the `objectSet.m_pUnknown10 = &objectSet;` store.  The
//     shipped order of the five initialising stores is +0x10, +0x00,
//     +0x04, +0x18, +0x14, and +0x10 gets `lea -0x1020(%ebp),%eax`, the
//     record's own address.  Written above as a first statement.  The
//     alternative, if the store lands in the wrong place, is that the
//     record is CONSTRUCTED and the +0x10 store is the constructor's:
//
//         D3DObjectSet objectSet(apObjects, 0x400, "");
//
//     with an inline constructor whose body is
//     `m_pUnknown10 = this; m_pElements = p; m_nElements = 0;
//      m_pszName = s; m_nMaxElements = n;`.
//
//  3. 0x10068a50, the second loop's guard.  `test %eax,%eax; jbe done;
//     mov $0x0,%ebx; jbe done` is TWO jbe off one test (the `mov $0x0`
//     is the five-byte form precisely so it does not disturb the flags).
//     Written above as a plain `for`.  If the duplicate guard does not
//     come back, the source wrapped it:
//
//         if (objectSet.m_nElements > 0)
//             for (i = 0; i < objectSet.m_nElements; i++) ...
//
//  4. 0x10009f10's element type.  Sixteen bytes copied as four dword
//     movs.  Written as four named uint32s.  If the copy comes back as
//     `rep movsl` the type is a single 16-byte aggregate
//     (`struct { LTVector v; float f; };` is the obvious candidate for a
//     renderer) and the assignment has to be spelled as four member
//     assignments instead of one struct assignment.
//     (It is: SavedVert589c4, {LTVector; uint32}, and the copy is the
//     template's own -- w64_10009f10_qip.cpp.)
//
//  5. 0x10021ad0 / 0x10009d60's `newSize`.  The header keeps
//     `newSize = m_nElements + 1;` above the branch and icl folds the +1
//     into `lea 0x1(%eax,%edx,1)`, so what is live across the two
//     accessor calls is m_nElements, not newSize.  If the spill pattern
//     at -0x4(%ebp) comes back wrong, drop the variable and write
//     `_AllocateTArray(m_nElements + 1 + m_Cache.GetWantedCache(), pAlloc)`
//     directly.
//
//  6. 0x1001e770's corner adds.  EVERY `+` is emitted `flds <dims>;
//     fadds <pos>` (the half-dimension loaded first) while every `-` is
//     `flds <pos>; fsubs <dims>` (which it has no choice about).  Written
//     above as `m_vPos.c + m_vDims.c` on the rule that FPU add
//     order is the scheduler's.  If the twenty-four groups come back with
//     the two loads transposed, the source wrote the adds the other way
//     round -- `pObject->m_vDims.x + pObject->m_vPos.x` -- and only the
//     twelve `+` lines change.
//
//  7. 0x10032f30's entry point.  Named after the four-byte stack adapter
//     at 0x10032f30; both callers actually call the register entry at
//     0x10032f34.  If the result is short by four bytes at the front,
//     rename it sub_10032f34 -- the body does not change.
//
// *********************************************************************** //


// ======================================================================= //
//  0x1005a410   116 bytes of body + padding   1 caller (0x1005aaf5)
//
//  Sphere against a six-plane frustum, with an outcode mask.  Answers 0 the
//  moment the sphere is wholly outside any plane; otherwise answers 1 and
//  leaves a six-bit mask in *pClipMask with a bit CLEARED for every plane
//  the sphere is wholly inside (i.e. the planes a later per-vertex clip can
//  skip).
//
//  WHAT PINS IT.  Three independent things:
//
//   1. The loop is the frustum walk and nothing else: `add $0x10,%edx` with
//      `cmp $0x6,%ebp / jl` is six 16-byte planes, and 16 bytes with three
//      floats multiplied against the three floats of another pointer and a
//      fourth subtracted is LTPlane {m_Normal, m_Dist} exactly.
//   2. The dot product is emitted z, y, x, which is ltvector.h's
//      `(x * v.x) + (y * v.y) + (z * v.z)` under the measured rule that
//      n-ary float expressions associate from the LAST term backwards.  The
//      plane component is the one LOADED (`flds 0x8(%edx)`) and the vector
//      component is the memory operand (`fmuls 0x8(%ebx)`), which is the
//      order `m_Normal.z * vec.z` and not the reverse.
//   3. The SAME code appears inlined at 0x10067ae0-0x10067b2f, tail bytes
//      and all (`shl %cl,%eax ; xor $0xffffffff,%eax ; and %eax,%edi ;
//      inc %esi ; add $0x10,%edx ; cmp $0x6,%esi ; jl`).  Two copies of one
//      loop is a second reading of the same source.
//
//  `+fRadius` and `-fRadius` are both computed BEFORE the loop (`fld %st(0)
//  ; fchs`) and live on the x87 stack across all six iterations; that is
//  icl hoisting `-fRadius`, not a source temp.
//
//  The caller (0x1005aaf5) settles the argument list: __cdecl, four dwords,
//  `add $0x10,%esp` after the call, arg0 a vector pointer from 0x100158b0,
//  arg1 a float, arg2 `this+0x3d0`, arg3 a stack local, result tested with
//  `test %eax,%eax`.
//
//  MOVED HERE FROM d3dr3d.cpp (/Os), and the two changes that closed it:
//  the flag -- /Os spells both `return 1`s `xor %eax,%eax; inc %eax` and
//  cross-jumps the second into the first exit; /O2 gives `mov $0x1,%eax`
//  and its own epilogue, as shipped -- and the plane walk is a POINTER
//  bump (`pPlanes++`), not `pPlanes[i]`: indexing keeps the parameter in
//  %eax and copies it to %edx, which also costs the one-byte `nop` that
//  aligns the loop head at +0x20.
//
//  Two `xor %eax,%eax`-free returns: the refusal is `xor %eax,%eax` in its
//  own block and the acceptance is `mov $0x1,%eax`, because the literal 1
//  is used once (the reused-constant rule d3dbatchb.cpp records).
// ======================================================================= //

int32 sub_1005a410(LTVector* pPos, float fRadius, LTPlane* pPlanes,
                   uint32* pClipMask)
{
	int32 i;
	float fDot;

	*pClipMask = 0x3f;

	for (i = 0; i < 6; i++)
	{
		fDot = pPlanes->m_Normal.x * pPos->x +
		       pPlanes->m_Normal.y * pPos->y +
		       pPlanes->m_Normal.z * pPos->z - pPlanes->m_Dist;

		if (fDot < -fRadius)
			return 0;

		if (fDot > fRadius)
			*pClipMask &= ~(1 << i);

		pPlanes++;
	}

	return 1;
}
