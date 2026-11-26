// Split out of d3dr2d.cpp: these functions need the
// OTHER optimisation flag from the rest of that file.  d3d.ren is a
// mixed build and our file boundaries are invented.
// Everything that is not a function body is
// carried over verbatim so no declaration is lost.
// ********************************************************* //
// *********************************************************************** //
//
//  MODULE:   BATCH_D (.CPP)
//
//  PURPOSE:  Reconstructed d3d.ren functions, listing
//            batch D (25 rows).
//
//  Compiler: Intel C/C++ 4.0, the same as the rest of src/engine/d3d.
//
//  WHAT THIS BATCH IS.  Twenty-one of the twenty-five rows are container
//  and list plumbing around ONE global object -- 0x10091df0, the
//  IAllocator that src/engine/d3d/d3dbatchb.cpp already pinned -- plus
//  four functions that are the SDK's own headers emitted out of line.
//
//  THREE FINDS WORTH CARRYING FORWARD:
//
//  1.  `src/LT2/sdk/inc/ltmatrix.h` gives up TWO MORE functions verbatim, on
//      top of the six ltvector/ltmatrix bodies found earlier:
//
//        0x1001e450  Mat_GetBasisVectors   ltmatrix.h:818
//        0x1005f790  MatVMul_InPlace_3x3   ltmatrix.h:274
//
//      and `ltvector.h` gives up a third:
//
//        0x100154b0  _CVector<T>::Dist     ltvector.h:36
//
//      Dist is `(*this - vec).Mag()`, and BOTH halves are visible in the
//      bytes: 0x10015640 is `operator-` (it is `ret $0x10`, i.e. the
//      return slot plus a twelve-byte vector BY VALUE, and it subtracts
//      the argument FROM `this`), and the tail is Mag inlined --
//      x*x + (y*y + z*z), associated from the last term backwards, then
//      `fstpl` into the CRT's double sqrt at 0x1007fc98.  That is the
//      same non-intrinsic sqrt Norm (0x10015530) uses eight functions
//      earlier, so 0x100154b0 is in Norm's translation unit, NOT Mag's,
//      and it is written here WITHOUT `#pragma intrinsic(sqrt)`.
//
//      NOTE, because it decides two of these three: ltmatrix.h's
//      MatVMul_3x3 and ltlink.h's dl_Remove are BOTH `inline` in the SDK
//      and BOTH are called, not inlined, in the shipped image (they live
//      at 0x10014a50 and 0x1000a6e0 as ordinary out-of-line bodies with
//      stack arguments).  icl's inliner declined them on size.  Rather
//      than bet on our build declining them too, they are declared here
//      as plain extern functions so the call is FORCED.  The call
//      displacement is a relocation, so which symbol it names costs
//      nothing.
//
//  2.  THE FORWARDER FAMILY IS ALREADY PROVEN.  d3dbatchb.cpp's
//      Obj1000a630::sub_1000a630 is the same 38-byte shape as four rows
//      here (0x1000a1f0, 0x1001d540, 0x1002e7f0, 0x1007f120) -- frame,
//      `sub $0x10,%esp`, `add $0xfffffff8,%esp`, the `mov %ecx,%eax ;
//      mov %eax,%ecx` round trip, `ret $0x4`.  Those four differ from it
//      only in WHICH default they supply: 0x1000a630 supplies a leading
//      zero, these four supply a trailing `&g_10091df0`.  0x1000a1f0
//      even shares 0x1000a630's callee (0x1000a110), so it is the same
//      class -- a container `SetSize(n)` next to a `SetAllocator(a)`.
//
//      Two more rows are the same family with a different arity:
//      0x1001ccb0 (`ret $0x8`, two arguments plus the allocator) and
//      0x1007ee70 (one argument, a literal 0, and the allocator).
//
//  3.  A DEAD ROW AND A REGISTER-ABI ROW.  See the bottom of this file.
//
//  Naming: `sub_<address>` throughout, so the address can be read
//  out of the symbol.  Types spelled `Obj<hex>` /
//  `Node<hex>` are invented here for classes whose real names are not
//  known; a class that exists only to host a constructor is named
//  `sub_<address>` so the constructor's mangling carries the address.
//
// *********************************************************************** //

#include "ltbasedefs.h"

#include <ctype.h>
#include <math.h>
#include <string.h>


// ======================================================================= //
//  SHARED DECLARATIONS
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  The allocator interface, repeated from src/engine/d3d/d3dbatchb.cpp
//  (separate translation units, so it is repeated rather than shared).
//  The evidence for the slot numbering is in that file's header.
//
//  0x10091df0 is a global allocator OBJECT, not a pointer to one, and it
//  is what all six forwarders in this batch supply as the defaulted last
//  argument.  128 sites in the image push its address.
// ---------------------------------------------------------------------- //
struct IAllocator
{
	virtual void* Slot00(uint32 nSize);                 // vtable +0x00
	virtual void* Alloc(uint32 nSize);                  // vtable +0x04
	virtual void  Free(void* pMem);                     // vtable +0x08
};

extern IAllocator g_10091df0;


// ---------------------------------------------------------------------- //
//  0x1008b880 -- BSS, 65 sites reference it, and they use it three
//  different ways (a `this`-less parameter block at +0x31c..+0x330 for
//  the transform at 0x10014580, a plain pass-through argument, and a
//  dead first argument at 0x1003b740).  Nothing in this batch pins its
//  layout, so it is an unbounded byte array and is NOT named.
// ---------------------------------------------------------------------- //
extern uint8 g_1008b880[];


// ---------------------------------------------------------------------- //
//  ltlink.h's CheapLTLink, copied rather than included: `m_pPrev` at
//  +0x00 and `m_pNext` at +0x04, and TieOff written exactly as the
//  header writes it --
//
//      m_pPrev = m_pNext = (LTLink*)this;
//
//  which stores +0x04 FIRST and then +0x00.  That store order is not a
//  guess: 0x1006d3a4 below emits `mov %eax,0x4(%eax)` then
//  `mov %eax,(%eax)`, in that order, and 0x1000a6e0 (the out-of-line
//  dl_Remove this batch calls twice) ends the same way.
// ---------------------------------------------------------------------- //
struct CheapLink
{
	CheapLink*  m_pPrev;                                // +0x00
	CheapLink*  m_pNext;                                // +0x04

	void TieOff() { m_pPrev = m_pNext = this; }
};

struct DLink                                            // ltlink.h's LTLink
{
	DLink*      m_pPrev;                                // +0x00
	DLink*      m_pNext;                                // +0x04
	void*       m_pData;                                // +0x08

	void TieOff() { m_pPrev = m_pNext = this; }
};

// 0x1000a6e0 -- ltlink.h's dl_Remove/CheapLTLink::Remove, emitted out of
// line by the shipped build:
//     mov (%eax),%edx ; mov 0x4(%eax),%ecx ; mov %ecx,0x4(%edx)
//     mov 0x4(%eax),%edx ; mov (%eax),%ecx ; mov %ecx,(%edx)
//     mov %eax,0x4(%eax) ; mov %eax,(%eax) ; ret
// __cdecl, one stack argument, no cleanup at the call site.  Declared
// non-inline on purpose -- see find (1) in the header.
extern void dl_Remove_1000a6e0(CheapLink* pLink);


// ---------------------------------------------------------------------- //
//  The Direct3D 7 device.  Slot +0x8c is SetTexture: d3dbatchb.cpp pins
//  SetRenderState at +0x50 (index 20) and SetTextureStageState at +0x94
//  (index 37), and SetTexture is index 35, which is +0x8c.  The
//  `mov (%eax),%edx ; call *0x8c(%edx)` with the object pushed as the
//  first stack argument is the function-pointer-struct model the rest of
//  src/engine/d3d uses for COM.
// ---------------------------------------------------------------------- //
struct D3DDevice7;

struct D3DDevice7Vtbl
{
	uint8 m_pad_00[0x8c];
	long (__stdcall* SetTexture)(D3DDevice7*, uint32 dwStage,
	                             void* lpTexture);
};

struct D3DDevice7
{
	D3DDevice7Vtbl* lpVtbl;
};

extern D3DDevice7* g_pD3DDevice;                        // 0x1008d988


// ======================================================================= //
//  FAMILY 1 -- THE DEFAULTED-ALLOCATOR FORWARDERS
//
//  Six functions, one shape.  Each is a thiscall member that calls
//  another member of the same class and supplies `&g_10091df0` as the
//  argument the caller left out.  They are OVERLOADS, not default
//  arguments: a C++ default argument is materialised in the CALLER and
//  never produces a body of its own.
//
//  The `mov %ecx,%eax ; mov %eax,%ecx` round trip in every one of them
//  is icl, not source -- d3dbatchb.cpp records the same round trip in
//  the same position on three functions that already reproduce.
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  0x1000a1f0 -- 38 bytes, 1 caller.  `ret $0x4`.
//
//  The callee, 0x1000a110, is the SAME function d3dbatchb.cpp's
//  Obj1000a630::sub_1000a630 forwards to, so this is that class: a
//  container whose +0x08 is its element count and whose +0x0c is a
//  Node1000a670 subobject.  0x1000a110 compares its first argument with
//  +0x08 and hands its second straight to the allocator at 0x1000a0e0,
//  so first argument = size, second = allocator.
// ---------------------------------------------------------------------- //
struct Obj1000a1f0
{
	void sub_1000a110(uint32 nSize, IAllocator* pAlloc);
	void sub_1000a1f0(uint32 nSize);
};



// ---------------------------------------------------------------------- //
//  0x1001d540 -- 40 bytes, 1 caller.  `ret $0x4`.  Callee 0x1001d390.
//
//  Its own caller, 0x1001d180, is the next wrapper out: it takes TWO
//  stack arguments (`ret $0x8`), forwards only the first, and rounds
//  `this` through %eax exactly the same way.  A three-deep stack of
//  convenience overloads.
// ---------------------------------------------------------------------- //
struct Obj1001d540
{
	void sub_1001d390(uint32 nSize, IAllocator* pAlloc);
	void sub_1001d540(uint32 nSize);
};



// ---------------------------------------------------------------------- //
//  0x1002e7f0 -- 38 bytes, 1 caller.  `ret $0x4`.  Callee 0x1002e710,
//  which is 0x1000a110's twin instruction for instruction (same
//  `cmp 0x8(%ebx),%esi ; jne ; test ; je` opening), i.e. the same
//  container template at another instantiation.
// ---------------------------------------------------------------------- //
struct Obj1002e7f0
{
	void sub_1002e710(uint32 nSize, IAllocator* pAlloc);
	void sub_1002e7f0(uint32 nSize);
};



// ---------------------------------------------------------------------- //
//  0x1007f120 -- 40 bytes, 1 caller.  `ret $0x4`.  Callee 0x1007ef70,
//  which opens `mov $0xc,%ecx ; add %ebx,%ecx ; call 0x1000a690` -- the
//  identical opening to 0x1001d540's callee 0x1001d390, so those two are
//  one template at two instantiations as well.
// ---------------------------------------------------------------------- //
struct Obj1007f120
{
	void sub_1007ef70(uint32 nSize, IAllocator* pAlloc);
	void sub_1007f120(uint32 nSize);
};

// Moved from d3dr2d.cpp: shipped pads 38 bytes with `8b f6` to 40, the /Os layout (/O2 pads to 48).
void Obj1007f120::sub_1007f120(uint32 nSize)
{
	sub_1007ef70(nSize, &g_10091df0);
}



// ---------------------------------------------------------------------- //
//  0x1001ccb0 -- 48 bytes, 1 caller.  The two-argument member of the
//  family: `ret $0x8`, `sub $0x18,%esp` + `add $0xfffffff4,%esp` for a
//  twelve-byte outgoing area, and the allocator lands in the THIRD slot.
//
//  The callee 0x1001cab0 opens `mov 0x8(%ecx),%edx ; mov 0x8(%ebp),%eax
//  ; cmp %edx,%eax ; jbe`, so its first argument is a count measured
//  against the same +0x08 the rest of the family uses.
// ---------------------------------------------------------------------- //
struct Obj1001ccb0
{
	void sub_1001cab0(uint32 nA, uint32 nB, IAllocator* pAlloc);
	void sub_1001ccb0(uint32 nA, uint32 nB);
};



// ---------------------------------------------------------------------- //
//  0x1007ee70 -- 44 bytes, 1 caller.  `ret $0x4`, and it supplies TWO
//  defaults: a literal zero in the second slot and the allocator in the
//  third.  The store order in the shipped bytes is argument, then the
//  zero, then the allocator -- the zero's `movl $0x0,0x4(%esp)` sits
//  BETWEEN the argument's load and its store, which is scheduling.
// ---------------------------------------------------------------------- //
struct Obj1007ee70
{
	void sub_1007e9f0(uint32 nA, uint32 nB, IAllocator* pAlloc);
	void sub_1007ee70(uint32 nA);
};



// ======================================================================= //
//  FAMILY 2 -- __cdecl WRAPPERS ROUND A MEMBER
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  0x1003b740 -- 27 bytes, 1 caller (0x1003b8c0).  __cdecl, TWO stack
//  arguments, and the FIRST IS DEAD: the body reads only 0xc(%ebp).
//  The caller pushes `g_1008b880` into that dead slot, which is what a
//  parameter kept for signature compatibility looks like.
//
//  `this` is a constant: `mov $0x1008eda0,%ecx`, a global object of the
//  class that owns 0x1003b964 (only two sites in the image name that
//  address).  0x1003b964 is `ret $0x4`, i.e. one stack argument.
// ---------------------------------------------------------------------- //
struct Obj1008eda0
{
	void sub_1003b964(void* pArg);
};

extern Obj1008eda0 g_1008eda0;

// Moved from d3dr2d.cpp: shipped is 27 bytes then 0x90 fill, the /Os layout (/O2 pads with a lea to 32).
void sub_1003b740(void* pUnused, void* pArg)
{
	g_1008eda0.sub_1003b964(pArg);
}



// ---------------------------------------------------------------------- //
//  0x1001e450 -- 46 bytes, 1 caller.  ltmatrix.h:818, VERBATIM:
//
//      inline void Mat_GetBasisVectors(LTMatrix *pMat, LTVector *pRight,
//                                      LTVector *pUp, LTVector *pForward)
//      { pMat->GetBasisVectors(pRight, pUp, pForward); }
//
//  WHAT PINS IT: plain `ret` (four __cdecl arguments, caller cleans),
//  the first argument goes into %ecx as `this`, and the other three go
//  to the stack in order.  The callee 0x1001e4d0 then copies (%ecx)+0,
//  +0x10 and +0x20 into the first output's +0, +4, +8 -- the first
//  COLUMN of a 4x4 row-major matrix, which is exactly the right basis
//  vector.  Nothing else in ltmatrix.h has that argument list and that
//  callee shape.
// ---------------------------------------------------------------------- //
struct Vec3D
{
	float x;                                            // +0x00
	float y;                                            // +0x04
	float z;                                            // +0x08
};

struct Mat4D
{
	float m[4][4];

	// 0x1001e4d0.  Declared only.
	void sub_1001e4d0(Vec3D* pRight, Vec3D* pUp, Vec3D* pForward);
};

void sub_1001e450(Mat4D* pMat, Vec3D* pRight, Vec3D* pUp, Vec3D* pForward)
{
	pMat->sub_1001e4d0(pRight, pUp, pForward);
}


// ---------------------------------------------------------------------- //
//  0x1005f790 -- 57 bytes, 1 caller.  ltmatrix.h:274, VERBATIM:
//
//      inline void MatVMul_InPlace_3x3(LTMatrix *pMat, LTVector *pSrc)
//      { LTVector temp; MatVMul_3x3(&temp, pMat, pSrc); *pSrc = temp; }
//
//  WHAT PINS IT: the twelve-byte local at 0x10(%esp) goes into the call
//  as the FIRST argument (the explicit destination, not a hidden return
//  slot), the two incoming pointers follow in source order, and then the
//  local is copied back over the SECOND argument three dwords at a time
//  -- integer moves, because that is a struct assignment, not float
//  arithmetic.  The callee 0x10014a50 is MatVMul_3x3 itself: three dot
//  products of rows 0/1/2 against the vector, no +0x0c translation term
//  and no homogeneous divide.
// ---------------------------------------------------------------------- //
extern void MatVMul_3x3_10014a50(Vec3D* pDest, Mat4D* pMat, Vec3D* pSrc);



// ---------------------------------------------------------------------- //
//  0x10043620 -- 27 bytes, 2 callers.  __cdecl, one argument, forwards
//  it to 0x10014580 with `g_1008b880` as the second.
//
//  0x10014580 is a projection: it divides a constant by the first
//  argument's +0x08, stores the reciprocal at +0x0c, and combines +0x00,
//  +0x04 and +0x08 with the second argument's +0x31c..+0x330.  So the
//  second argument is a parameter block and this wrapper is the
//  "transform with the current one" convenience.  Only the forwarding is
//  reconstructed here; 0x10014580 is declared.
// ---------------------------------------------------------------------- //
extern void sub_10014580(void* pPoint, uint8* pParams);



// ---------------------------------------------------------------------- //
//  0x1006d8e0 -- 43 bytes, 1 caller.  __cdecl, two arguments, forwards
//  them to 0x1000a8a0 with `g_1008b880` third and a literal zero fourth.
//  Same "supply the defaults" idea as family 1, written as a free
//  function because there is no %ecx anywhere in it.
// ---------------------------------------------------------------------- //
extern void sub_1000a8a0(void* pA, void* pB, uint8* pParams, uint32 nFlags);

// Moved from d3dr2d.cpp (/O2): same bytes at either flag, but /O2 pads the
// 43 bytes to 48 with a lea-nop and the shipped function is followed by
// 0x90 fill, which is /Os (a 1-byte pad to 4, then the linker's).
void sub_1006d8e0(void* pA, void* pB)
{
	sub_1000a8a0(pA, pB, g_1008b880, 0);
}



// ======================================================================= //
//  ACCESSORS AND SMALL MEMBERS
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  0x100647d0 -- 8 bytes plus padding, 1 caller.
//
//      56              push %esi          ; icl's dead four-byte slot
//      d9 41 1c        flds 0x1c(%ecx)    ; `this` really is in %ecx
//      83 c4 04        add  $0x4,%esp
//      c3              ret                ; no stack arguments
//
//  The FLOAT twin of d3dbatchb.cpp's three integer accessors at
//  0x1000a670 / 0x1004fb70 / 0x1007ee60.  Defined OUT OF LINE, because
//  the shipped code still calls it: /Ob1 would have folded it away had
//  it been written inside the class body.
// ---------------------------------------------------------------------- //
struct Obj100647d0
{
	uint8   m_pad_00[0x1c];                             // +0x00
	float   m_f1c;                                      // +0x1c

	float sub_100647d0() const;
};



// ---------------------------------------------------------------------- //
//  0x1007dec4 -- 33 bytes, 2 callers.  A ring-buffer advance.
//
//      mov 0x18(%ecx),%eax ; mov 0x24(%ecx),%edx
//      inc %eax ; cmp %edx,%eax ; mov %eax,0x18(%ecx)
//      jb  L                       ; unsigned, so the fields are unsigned
//      movl $0x0,0x18(%ecx)
//  L:  movl $0x0,0x1c(%ecx)
//
//  WHAT PINS IT: the `mov` of the incremented value sits BETWEEN the
//  `cmp` and the `jb` and does not disturb the flags, so the store
//  happens before the test in source too -- `++m_n18;` is its own
//  statement and the `if` re-reads it.  `jb` (below) taken means the
//  wrap is NOT done, so the written condition is `>=`.  +0x1c is reset
//  unconditionally, outside the if.
// ---------------------------------------------------------------------- //
struct Obj1007dec4
{
	uint8   m_pad_00[0x18];                             // +0x00
	uint32  m_n18;                                      // +0x18
	uint32  m_n1c;                                      // +0x1c
	uint8   m_pad_20[0x04];                             // +0x20
	uint32  m_n24;                                      // +0x24

	void sub_1007dec4();
};

// Moved from d3dr2d.cpp: shipped pads 33 bytes with `8d 76 00` to 36, the /Os layout (/O2 pads to 48).
void Obj1007dec4::sub_1007dec4()
{
	++m_n18;

	if (m_n18 >= m_n24)
		m_n18 = 0;

	m_n1c = 0;
}



// ---------------------------------------------------------------------- //
//  0x10050d60 -- 47 bytes, 1 caller.  Returning a node to a free list.
//
//      lea 0x10(%ebx),%eax ; call 0x1000a6e0
//      lea 0x1c(%ebx),%eax ; call 0x1000a6e0
//      mov 0x1c(%ebp),%eax ; mov %eax,(%ebx)     ; p->next = m_pFree
//      mov %ebx,0x1c(%ebp)                       ; m_pFree = p
//
//  WHAT PINS IT: 0x1000a6e0 is ltlink.h's dl_Remove (it does the two
//  neighbour fixups and then ties the link off), and it is reached twice
//  on the SAME argument at two different offsets -- so the node carries
//  two independent list links, at +0x10 and +0x1c.  The push onto the
//  free list is written through the node's +0x00, which is therefore the
//  free-list next pointer and not one of the links.
//
//  Both calls share one twelve-byte outgoing area with no cleanup
//  between them, which is __cdecl.
// ---------------------------------------------------------------------- //
struct Node10050d60
{
	Node10050d60*   m_pFreeNext;                        // +0x00
	uint8           m_pad_04[0x0c];                     // +0x04
	CheapLink       m_Link10;                           // +0x10
	uint8           m_pad_18[0x04];                     // +0x18
	CheapLink       m_Link1c;                           // +0x1c
};

struct Obj10050d60
{
	uint8           m_pad_00[0x1c];                     // +0x00
	Node10050d60*   m_pFree;                            // +0x1c

	void sub_10050d60(Node10050d60* pNode);
};

void Obj10050d60::sub_10050d60(Node10050d60* pNode)
{
	dl_Remove_1000a6e0(&pNode->m_Link10);
	dl_Remove_1000a6e0(&pNode->m_Link1c);

	pNode->m_pFreeNext = m_pFree;
	m_pFree = pNode;
}


// ======================================================================= //
//  CONSTRUCTORS
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  0x1003b924 -- 56 bytes, 1 caller.
//
//      mov  $0x2c,%eax ; add %ebx,%eax        ; icl's &this->m_a2c
//      movl $0x10089530,(%ebx)                ; the vptr
//      movl $0x0,0x4(%esp) ; mov %eax,(%esp) ; movl $0x20,0x8(%esp)
//      call 0x100809a0                        ; memset
//      movl $0x0,0x28(%ebx)
//      add  $0x18,%esp ; mov %ebx,%eax ; ret  ; answers `this`
//
//  WHAT PINS IT AS A CONSTRUCTOR: `mov %ebx,%eax` after the stack
//  adjustment with a plain `ret`, plus a vptr store ahead of everything
//  the body does.  0x10089530 is a run of code pointers in .data
//  (0x1003bb10, 0x1003d350, 0x1003d5c0, 0x1003d5f0, ...), i.e. a vtable,
//  so the class is polymorphic; four slots are declared to give it one.
//  0x100809a0 is the CRT memset (it splices the fill byte to a dword and
//  aligns before the rep).
//
//  SOURCE ORDER: memset first, then +0x28 = 0.  The vptr store is the
//  compiler's and comes first regardless.
//
//  The vptr is written as an ORDINARY MEMBER assignment rather than
//  by declaring four virtual functions, the same way d3dbatchb.cpp's
//  Obj1007dcc4 handles its two vptrs: an explicit store at the top of
//  the body lands in exactly the position the compiler's own store
//  would, and costs no undefined vtable.
//
//  RISK, stated plainly: the shipped build CALLS memset for a constant
//  32-byte clear.  If our flags have memset as an intrinsic it will come
//  out as `rep stos` and this function will miss.  Nothing in the source
//  can force the call; the fill length and fill value are what they are.
// ---------------------------------------------------------------------- //
extern uint8 g_10089530[];                              // the vtable

struct sub_1003b924
{
	uint8*  m_pVtbl;                                    // +0x00
	uint32  m_pad_04[9];                                // +0x04..+0x27
	uint32  m_n28;                                      // +0x28
	uint8   m_a2c[0x20];                                // +0x2c

	sub_1003b924();
};



// ---------------------------------------------------------------------- //
//  0x1006d3a4 -- 48 bytes, 1 caller.  A constructor whose body touches
//  nothing but two static arrays.
//
//      mov $0x10091968,%eax ; add %ecx,%eax   ; &g_a10091968[i]
//      add $0xc,%ecx                          ; stride 12 -> LTLink
//      mov %eax,0x4(%eax)                     ; m_pNext = this
//      mov %eax,(%eax)                        ; m_pPrev = this
//      mov %ebx,0x10091954(,%edx,1)           ; the other array = 0
//      add $0x4,%edx ; cmp $0x14,%edx ; jb    ; five iterations
//      add $0x4,%esp ; mov %ebp,%eax          ; dead slot, answers `this`
//
//  WHAT PINS IT: two induction variables with strides 12 and 4 running
//  to the SAME trip count, over two arrays that are adjacent in memory
//  (0x10091954 + 5*4 == 0x10091968).  Twelve bytes is LTLink, and the
//  two stores are +0x04 then +0x00, which is precisely what ltlink.h's
//  `m_pPrev = m_pNext = (LTLink*)this` emits -- the inner assignment
//  first.  So the first array is five LTLinks being tied off and the
//  second is five parallel counters being zeroed.  Not an LTList array:
//  an LTList is one 16-byte object per entry and its head and count
//  would interleave.
// ---------------------------------------------------------------------- //
extern DLink  g_a10091968[5];
extern uint32 g_a10091954[5];

struct sub_1006d3a4
{
	sub_1006d3a4();
};

sub_1006d3a4::sub_1006d3a4()
{
	uint32 i;

	for (i = 0; i < 5; ++i)
	{
		g_a10091968[i].TieOff();
		g_a10091954[i] = 0;
	}
}


// ======================================================================= //
//  LOOPS OVER GLOBAL TABLES
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  0x10020f30 -- 65 bytes, 2 callers.  Emptying a doubly linked list.
//
//      mov 0x1008bf7c,%eax ; mov $0x1008bf78,%edx ; cmp ; je  END
//      mov $0x1008bf78,%ebp
//   L: mov 0x8(%eax),%edx      ; pCur->m_pData
//      mov 0x4(%eax),%ebx      ; pNext = pCur->m_pNext
//      mov %edx,(%esp) ; call 0x10021cb0
//      mov %ebx,%eax ; cmp %ebp,%ebx ; jne L
//  END: mov $0x1008bf78,%eax
//      mov %eax,0x1008bf78 ; mov %eax,0x1008bf7c
//
//  WHAT PINS IT: the head is walked from +0x04 and the payload comes off
//  +0x08, which is ltlink.h's LTLink (m_pPrev, m_pNext, m_pData) and
//  fixes 0x1008bf78 as a bare list head -- there is no element count
//  anywhere, so it is an LTLink and not an LTList.  The successor is
//  taken BEFORE the release call, which is CPP_DELETELIST's shape.  The
//  test-then-loop (`je` forward, `jne` back) is a `while`, not a
//  `do`/`while`.  0x10021cb0 is the module's free -- it routes through
//  slot +0x38 of the record at 0x1008cbcc and falls back to the CRT's
//  free -- so it is declared, not reconstructed.
//
//  THE ONE UNCERTAIN POINT: the
//  tie-off at the end stores +0x00 BEFORE +0x04, which is the reverse of
//  the order ltlink.h's `m_pPrev = m_pNext = this` produces and the
//  reverse of what 0x1006d3a4 above emits.  So it is written here as two
//  separate statements in the order the bytes show.  IF THIS FUNCTION
//  MISSES BY EXACTLY THE TWO TIE-OFF STORES, replace those two lines
//  with `g_1008bf78.TieOff();` and it will land -- and if that is what
//  happens, 0x1006d3a4 is the one that is already right and must not be
//  touched.
// ---------------------------------------------------------------------- //
extern DLink g_1008bf78;
extern void  sub_10021cb0(void* pMem);

void sub_10020f30()
{
	DLink* pCur;
	DLink* pNext;

	pCur = g_1008bf78.m_pNext;
	while (pCur != &g_1008bf78)
	{
		pNext = pCur->m_pNext;
		sub_10021cb0(pCur->m_pData);
		pCur = pNext;
	}

	g_1008bf78.m_pNext = &g_1008bf78;
	g_1008bf78.m_pPrev = &g_1008bf78;
}


// ---------------------------------------------------------------------- //
//  0x10050390 -- 35 bytes, 1 caller.  Calling one hook out of every
//  record in a static table.
//
//      mov $0xffffff24,%ebx                   ; -220 == -(11 * 20)
//   L: mov 0x10089698(%ebx),%eax              ; 0x10089698 is one PAST
//      test %eax,%eax ; je S ; call *%eax     ;   the end of the table
//   S: add $0x14,%ebx ; jne L
//
//  WHAT PINS THE BOUNDS: the biased index runs from -0xdc to 0 in steps
//  of 0x14, so the table is ELEVEN records of TWENTY bytes ending at
//  0x10089698 -- i.e. based at 0x100895bc.  Reading .data there confirms
//  it: eleven groups of five dwords, and the first dword of a group is
//  a .text pointer or zero (0x100895bc has none, 0x100895d0 has
//  0x10015d30, 0x100895e4 and 0x100895f8 both have 0x1007b8c0, and so
//  on).  The null test is on that first dword only.
//
//  The call takes no arguments and cleans no stack, so the field is a
//  plain `void (*)()`.  The ten bytes of lea-nop between the setup and
//  the loop head are icl aligning the loop to 16.
// ---------------------------------------------------------------------- //
struct Rec100895bc
{
	void  (*m_pfnSlot00)();                             // +0x00
	void*   m_p04;                                      // +0x04
	void*   m_p08;                                      // +0x08
	void*   m_p0c;                                      // +0x0c
	void*   m_p10;                                      // +0x10
};

extern Rec100895bc g_a100895bc[11];



// ---------------------------------------------------------------------- //
//  0x10021ed0 -- 59 bytes, 2 callers.  Upper-casing a string in place.
//
//      call 0x10080d10 ; test %eax,%eax ; jbe END      ; strlen first
//   L: movsbl 0x0(%ebp,%ebx,1),%eax
//      call 0x10080d8b ; mov %al,0x0(%ebp,%ebx,1)
//      inc %ebx ; call 0x10080d10 ; cmp %eax,%ebx ; jb L
//
//  WHAT PINS IT: 0x10080d10 is the CRT strlen (the 0x7efefeff word-at-a-
//  time scan) and 0x10080d8b is the CRT toupper (`if 'a' <= c && c <=
//  'z' then c - 0x20`, behind the locale check at 0x10091edc).  strlen
//  is called AGAIN at the bottom of every iteration, so it really is in
//  the loop condition and not hoisted into a variable -- the shape below
//  is forced.  The load is `movsbl`, so the element is a signed char
//  widening into toupper's int; the store is `mov %al`, so the result is
//  narrowed straight back.  `jb`/`jbe` throughout: the index is
//  unsigned.
// ---------------------------------------------------------------------- //


// ======================================================================= //
//  GUARDED CALLS
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  0x100257d0 -- 75 bytes, 1 caller.  Dropping the texture bound to the
//  current stage.
//
//      mov 0x1008d7d0,%ebx                    ; the stage index
//      mov 0x1008e684(,%ebx,4),%eax ; test ; je END
//      mov 0x1008d988,%eax                    ; the device
//      movl $0x0,0x8(%esp) ; mov %ebx,0x4(%esp)
//      mov (%eax),%edx ; mov %eax,(%esp) ; call *0x8c(%edx)
//      movl $0x0,0x1008e684(,%ebx,4)
//
//  WHAT PINS IT: +0x8c of IDirect3DDevice7 is SetTexture (index 35;
//  d3dbatchb.cpp pins index 20 at +0x50 and index 37 at +0x94), the
//  arguments are (stage, NULL), and the shadow array indexed by the same
//  stage is cleared to match.  The index is held in %ebx ACROSS the COM
//  call and never reloaded, which a global cannot be -- so the source
//  copies it into a local first.
// ---------------------------------------------------------------------- //
extern uint32 g_1008d7d0;                               // current stage
extern void*  g_a1008e684[];                            // bound textures



// ---------------------------------------------------------------------- //
//  0x10068840 -- 73 bytes, 1 caller.  Two guards and a call.
//
//      mov 0x1009110c,%eax ; test ; jne ...   ; else return
//      call 0x1007b8d0                        ; `mov $0x10091a10,%eax`
//      mov %eax,%ecx
//      mov 0x1e8(%ecx),%eax                   ; the count
//      add $0x1e4,%ecx                        ; the subobject
//      test %eax,%eax ; jne ...               ; else return
//      movl $0x10067aa0,0x4(%esp)             ; a callback
//      movl $0x1008b880,(%esp)
//      call 0x1007b8e0                        ; thiscall on %ecx
//
//  WHAT PINS THE SUBOBJECT: the guard reads +0x1e8 but the call's `this`
//  is +0x1e4, four bytes earlier, and 0x1007b8e0's own prologue reads
//  `0x4(%ebp)` as a loop bound and `0x0(%ebp)` as an array base.  So
//  +0x1e4 is a {pointer, count} pair and the guard is that pair's count.
//  Taking the address before the test is why %ecx is formed above the
//  branch, so the source names the subobject first.
//
//  THREE separate `mov %ebp,%esp ; pop %ebp ; ret` epilogues, which is
//  the shared-exit lever reading "a run of guarded returns":
//  written as two early returns rather than as nesting.
//
//  0x1007b8d0 is a one-instruction accessor (`mov $0x10091a10,%eax ;
//  ret`), i.e. the address of a global singleton.
// ---------------------------------------------------------------------- //
struct Arr1007b8e0
{
	void*   m_pArray;                                   // +0x00
	uint32  m_nCount;                                   // +0x04

	// 0x1007b8e0.  Declared only.
	void sub_1007b8e0(uint8* pParams, void (*pfnCallback)(void*, void*));
};

struct Obj10091a10
{
	uint8       m_pad_000[0x1e4];                       // +0x000
	Arr1007b8e0 m_Arr1e4;                               // +0x1e4
};

extern uint32        g_1009110c;
extern Obj10091a10*  sub_1007b8d0();
extern void          sub_10067aa0(void* pA, void* pB);



// ---------------------------------------------------------------------- //
//  0x10066cf0 -- 24 bytes, 1 caller.  SPECULATIVE.
//
//      mov 0x8(%ebp),%ecx
//      mov 0x1008ce8c,%eax
//      mov $0x14,%edx ; add %ecx,%edx
//      call *%eax
//
//  0x1008ce8c is a global function POINTER (0x1002b13b assigns it
//  0x100234d0, an empty body) and every one of its fourteen call sites
//  loads %ecx and %edx and nothing else -- 0x1000f060 even does
//  `mov %ecx,%edx ; mov %ebx,%ecx` to get two live values into exactly
//  those two registers, which is a two-argument register convention and
//  not a stray `this`.  %ecx and %edx, in that order, is __fastcall.
//
//  The second argument is the first plus 0x14, formed with icl's
//  `mov $imm ; add` address idiom rather than a lea.
// ---------------------------------------------------------------------- //
typedef void (__fastcall* PFN_1008ce8c)(void* pA, void* pB);

extern PFN_1008ce8c g_pfn_1008ce8c;



// ======================================================================= //
//  THE SDK VECTOR MATHS -- LAST, so that nothing after it inherits the
//  sqrt situation.
// ======================================================================= //

// 0x100154b0 (_CVector::Dist) is in d3dbatchd.cpp, the /Os /Oi- file,
// beside Norm: it CALLS the CRT sqrt, so it needs intrinsics off.



// ======================================================================= //
//  NOT WRITTEN -- and neither for want of a decode.
//
//  0x1001df50   NOT A FUNCTION.  The row is an enumeration artefact: the
//               address is 0x70 bytes INSIDE the body that starts at
//               0x1001dee0 (`push %ebp ; mov %esp,%ebp ; sub $0x18,%esp
//               ; push %edi ; push %edi ; push %esi ; push %ebx`, ending
//               `ret $0xc`), and disassembling from 0x1001df50 lands
//               mid-instruction -- the first "instruction" it produces is
//               `jb 0x1001df51`, a jump into its own second byte.  The
//               containing function is an insert-at-index on a uint16
//               array: it bounds-checks the index against +0x08, grows
//               through 0x1000a690/0x1000a680, shifts the tail up one
//               element with a backwards `movzwl`/`mov %cx` loop, writes
//               the new element, `incl 0x8(%ebx)` and returns 1 via
//               `xor %eax,%eax ; inc %eax`.  It is a REAL function and a
//               tractable one, but it is not this row and it is not on
//               this listing.
//
//  0x1002d05c   REGISTER-ABI BODY -- the filter missed one.  It opens
//               `push %edi ; push %esi ; push %ebp ; push %ebx ;
//               sub $0x14,%esp ; mov %edx,%ebp ; mov %ecx,%esi ;
//               mov %eax,%ebx` and never reads a stack argument, so its
//               three parameters arrive in %eax, %edx and %ecx.  Its own
//               recursive call at 0x1002d098 sets up the same three
//               registers, and its call to 0x1002dd6c does too.  Decoded
//               anyway, for whoever gets the far side of it: it is a
//               recursive tree walk -- `if (nDepth > nLimit) return;`
//               then 0x1002dd6c(&this[0x18], &this[0x24], -1), then
//               `if (m_p4c)` walk four child pointers at +0x00..+0x0c
//               calling itself with (m_p4c, nDepth + 1, nLimit).  No C
//               declaration produces that entry.
// ======================================================================= //


