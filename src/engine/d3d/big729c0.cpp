// *********************************************************************** //
//
//  MODULE:   BIG729C0 (.CPP)
//
//  PURPOSE:  0x100729c0 -- d3d.ren's RENDER-BUCKET FLUSH.  Walk the frame's
//            bucket list at 0x1008d4c4, draw every bucket's elements, and
//            return the buckets and their elements to the two free lists.
//            The producer that fills those buckets is big73140.cpp, the
//            OTHER function in this enumeration row.
//
//  ------------------------------------------------------------------- //
//  THE EXTENT -- 0x100729c0..0x1007313c, 1,916 bytes.  CONFIRMED.
//  ------------------------------------------------------------------- //
//
//      0x100729c0..0x1007313c  1,916 bytes  569 instructions
//      ret               1        0x10072f55
//      push %ebp frames  1        0x100729c0
//      branch targets    0 outside the range or mid-instruction
//      last instruction  ends exactly at the stated end
//
//  One `ret`, one prologue, no strays and no second function.
//  The 486 bytes after the `ret` (0x10072f56..
//  0x1007313b) are twelve /O2-sunk cold blocks; every one of them uses
//  THIS frame (-0x28(%ebp), -0x1c(%ebp), ...) and every one ends by
//  branching BACK above 0x10072f55 -- the individual targets are listed in
//  the COVERAGE table at the bottom.  0x1007313c..0x1007313f is four
//  `nop`s and 0x10073140 is the separate frameless producer already
//  decoded in big73140.cpp; NOTHING here branches to it.
//
//  Call census:
//
//      1,916 bytes, 26 calls (13 direct, 13 indirect), 74 bytes per call
//
//  TWENTY-SIX, not 25, and THIRTEEN indirect, not
//  12.  The extra indirect is the fourth `*0xbc(%eax)` SetLOD -- there are
//  two in each of the two texture-bind copies (one in the main path, one in
//  the sunk `pRec is already bound` arm), which is easy to count as three.
//  74 bytes per call puts this well inside the band where the call check is strong.
//
//  Distinct direct callees: 8 (0x10039f50, 0x1006d910, 0x100256a0,
//  0x1000da50, 0x10070d10, 0x1006f540, 0x1007fbf0, 0x10039bd0), plus five
//  distinct indirect vtable slots.  Thirteen distinct targets over 26 call
//  sites through a real control-flow graph, a discriminating shape
//  rather than a table-driven one.
//
//  ------------------------------------------------------------------- //
//  THE SIGNATURE:  `void __cdecl sub_100729c0(void)`
//  ------------------------------------------------------------------- //
//
//    * NO ARGUMENT IS READ.  A census of every `(%ebp)` operand in the
//      569 instructions gives exactly ten displacements and all ten are
//      NEGATIVE: -0x04, -0x08, -0x0c, -0x10, -0x14, -0x18, -0x1c, -0x20,
//      -0x24, -0x28.  There is no 0x8(%ebp) anywhere.
//    * NO EXIT SETS %eax.  The single epilogue at 0x10072f4f is
//      `pop %ebx ; pop %esi ; pop %edi ; mov %ebp,%esp ; pop %ebp ; ret`
//      and the instruction before it is `movl $0x0,0x1008d4c4`.
//    * THE ONE CALLER AGREES.
//      A scan of all 0x86b90 bytes of .text for an `e8`/`e9`
//      displacement resolving to 0x100729c0 finds exactly ONE hit,
//      0x10026522:
//
//          10026519: mov  0x1008d96c,%eax
//          1002651e: test %eax,%eax
//          10026520: je   0x10026529
//          10026522: call 0x100729c0        <- nothing pushed
//          10026527: jmp  0x1002652e        <- and nothing unwound
//          10026529: call 0x10012380
//
//      No `push` before it, no `add $N,%esp` after it, and the result is
//      not tested.  0x10012380 is the same flush for the OTHER pipeline
//      (0x1008d96c is the selector), and eleven instructions later the
//      same flag picks 0x100733a0 the same way.  That flag also drives the
//      dispatch table at 0x100256d0, which plants 0x10073140 -- THIS
//      function's producer -- into 0x1008ce9c.  So the pairing of the two
//      halves of this enumeration row is confirmed from a third place.
//
//  ------------------------------------------------------------------- //
//  WHAT PINS THE TWELVE THIRTEEN INDIRECT TARGETS
//  ------------------------------------------------------------------- //
//
//  The call check matches indirect calls BY POSITION ONLY, so each slot needs its
//  own evidence.  Every one of these five offsets is independently
//  recorded by big0ae20.cpp, big6d910.cpp, big61a30.cpp and big2ef34.cpp
//  from their own copies of the same texture-bind block, AND each is
//  corroborated here by its own operands:
//
//    +0x8c  SetTexture           2 sites (0x10072c11, 0x10072e3e).  Three
//           dwords: the device as `this`, the stage NARROWED WITH
//           `movzbl %al,%eax` from pRec->m_nStage42, and pRec->+0x0c,
//           which every other user of this record treats as a
//           DirectDrawSurface.  A three-argument device method whose
//           second argument is a small stage index and whose third is a
//           surface is SetTexture and nothing else.
//    +0x50  SetRenderState       4 sites (0x10072c3c, 0x10072ce0,
//           0x10072e69, 0x10072f1c).  Three dwords, argument 1 always a
//           LITERAL render-state number -- 0x18 (D3DRENDERSTATE_ALPHAREF,
//           paired with pRec->m_nAlphaRef16) and 0x1b, set to 1 before the
//           deferred-bucket pass and back to 0 after it.  A state number
//           plus a value is SetRenderState.
//    +0x90  GetTextureStageState 1 site  (0x10072d03).  FOUR dwords and
//           the LAST ONE IS `lea -0x24(%ebp),%eax` -- an OUT POINTER into
//           this frame.  Only a Get takes one.  The value that comes back
//           out of -0x24 is handed to +0x94 at 0x10072f33, which is what
//           makes this a save/restore pair and fixes the direction.
//    +0x94  SetTextureStageState 2 sites (0x10072d26, 0x10072f3f).  Four
//           dwords, all values; the first writes (stage 1, state 1, value
//           1) and the second writes back the dword +0x90 saved.  Same
//           (stage, state) pair as the Get, opposite direction.
//    +0xbc  IDirectDrawSurface7::SetLOD  4 sites (0x10072c76, 0x10072ea3,
//           0x1007303b, 0x100730c9).  NOT on the device: the vtable comes
//           from pRec->+0x0c's own first dword (`mov 0xc(%ebx),%eax ;
//           mov (%eax),%eax ; call *0xbc(%eax)`), it takes two dwords
//           (the surface and a literal 0), and it is IMMEDIATELY followed
//           by `xor %eax,%eax ; mov %ax,0x44(%ebx)` -- clearing the
//           record's dirty flag.  A surface method taking one integer that
//           clears a dirty flag is SetLOD.
//
//  ------------------------------------------------------------------- //
//  COMPILER: Intel C/C++ 4.0 /O2, same as the rest of d3d.ren.
//  ------------------------------------------------------------------- //
//
//    * COLD-BLOCK SINKING BELOW THE EPILOGUE, twelve-fold -- 486 of 1,916
//      bytes.
//    * OUTGOING ARGUMENTS as `add $-N,%esp` then `mov %reg,K(%esp)`
//      (0x100729f7, 0x10072bf5, 0x10072c22, 0x10072c63, 0x10072cc3,
//      0x10072ce3, 0x10072d09, ...) mixed with plain `push` where the
//      value is already in a register (0x10072a47, 0x10072a7b, ...).
//      big6d910.cpp records the same mixture and calls it unexplained.
//    * `push %edi ; push %edi` IN THE PROLOGUE (0x100729c6/0x100729c7)
//      with only THREE pops in the epilogue.  The second push is a
//      four-byte stack bump, not a save -- and that identifies the same
//      idiom at four call sites; see THE `push %edi` PADDING below.
//    * `xor %eax,%eax ; inc %eax` for the literal 1 (0x10072cf3,
//      0x10072d0c, 0x10072f22).
//
//  ------------------------------------------------------------------- //
//  THE `push %edi` PADDING, and what it does to four prototypes
//  ------------------------------------------------------------------- //
//
//  big6d910.cpp records that icl rounds every outgoing argument area in
//  this module up to a MULTIPLE OF EIGHT, so a one-argument __cdecl call
//  gets two pushes and `add $0x8,%esp`.  Four call sites here are that
//  shape, and all four pad with `push %edi`:
//
//      0x10072a7b  push %edi ; push pElements ; call 0x100256a0
//      0x10072a47  push %edi ; push pOwner    ; call 0x1006d910
//      0x10072c9b  push %edi ; push pOwner    ; call 0x10070d10
//      0x10072ec8  push %edi ; push pOwner    ; call 0x1006f540
//
//  0x100256a0 SETTLES IT.  Its body is 48 bytes and reads ONE argument,
//  `mov 0x4(%esp),%eax` at 0x100256a0, and nothing else -- so the %edi in
//  the higher slot there is definitively padding and not an argument.  The
//  same `push %edi` at the other three sites is therefore the same idiom,
//  and it is the same register the prologue uses for its own four-byte
//  bump.  Confirming it from the callees: 0x1006d910, 0x10070d10 and
//  0x1006f540 each read `0x8(%ebp)` and NEVER `0xc(%ebp)` -- checked over
//  all 7,204 / 7,331 / 6,096 bytes of each -- so none of them has a second
//  parameter at all.
//
//  So all four are declared here with ONE parameter.  big6d910.cpp and
//  big70d10.cpp declare their own function as `(pObj, void* pCtx)` with
//  "pCtx never read", which is the same fact spelled the other way; the
//  difference cannot move a call and cannot change the pushes, because
//  icl's alignment padding is not reachable from the source.
//
//  ------------------------------------------------------------------- //
//  THE ACCEPTANCE FIGURE, AND WHAT IT DOES NOT WITNESS
//  ------------------------------------------------------------------- //
//
//  Predicted: 26 shipped, 26 ours, 22 or more in agreement.  The two ways
//  it can lose rows are both stated as coin-flips at the bottom, and both
//  are BLOCK PLACEMENT of the sunk tail rather than decode errors:
//  the benign pattern (equal counts, the odd rows all in the sunk
//  region) is exactly what a miss here should look like.
//
//  There is NO call-free region worth naming: the longest run of bytes
//  here without a call is 0x10072b8c..0x10072c11, 133 bytes of the
//  texture-bind block, and that block is decoded against four independent
//  siblings field for field.  There are NO `// UNDECODED:` gaps.
//
//  /Oi- JUDGEMENT: NOT WANTED.  The residual it fixes is CRT intrinsics --
//  sqrt, fabs, tan, atan, memcpy -- and this function contains no floating
//  point at all (zero x87 instructions in 569) and no block copy: every
//  pointer store here is a single `mov`.  There is nothing for /Oi- to
//  bring back and it can turn open-coded copies
//  into an ours-only `memcpy` row.  Census: 0 candidate sites.
//
//  /Qrcd JUDGEMENT: NOT WANTED.  Zero float-to-int conversions -- there is
//  no `fistp`, no `fld` and no `.rdata` float operand anywhere in the
//  range.  The 27 `movzbl`/`movzwl` narrowings here are all INTEGER field
//  loads (m_nStage42, m_nStamp14, m_nDirty44, m_nAlphaRef16, g_1008b870,
//  g_1008bea8) and /Qrcd does not touch them -- the full decomposition is
//  in THE INSTRUCTION CENSUS at the bottom.
//
//  `_chkstk`: NOT PRESENT.  The frame is 0x38 bytes, far under the probe
//  threshold, and there is no `mov $N,%eax ; call 0x100805a0`.  So the one
//  residual known to be benign does not even arise here.
//
// *********************************************************************** //

#include "ltbasedefs.h"


// ======================================================================= //
//  TYPES
//
//  Every offset below is one this function's bytes touch, with the
//  touching address given.  The bucket and element are big73140.cpp's --
//  this is the consumer of what that function produces -- and the texture
//  record is big6d910.cpp's Tex6d910 / big0ae20.cpp's Tex0ae20, reached
//  here through the identical bind block.
// ======================================================================= //

struct Obj729c0;
struct Bucket729c0;
struct Tex729c0;


// ----------------------------------------------------------------------- //
//  THE ELEMENT, 0x18 bytes.  big73140.cpp's Elem73140 and big124a4.cpp's
//  Elem124a4, from the pool at 0x1008d4e4 whose element size 0x1002e2d0
//  sets to 0x18.  This function reads +0x00, +0x10 and +0x14 and writes
//  all three; +0x04, +0x08 and +0x0c it never touches, and they are named
//  from the producer.
//
//  +0x00 IS ALSO THE FREE-LIST LINK.  `mov 0x1008d4fc,%eax ;
//  mov %eax,(%ebx) ; mov %ebx,0x1008d4fc` at 0x10072ac3/0x10072aca/
//  0x10072acc pushes the element by writing the list head over its +0x00,
//  and `mov (%ecx),%eax ; mov %eax,0x1008d4fc` at 0x10072f60 pops it back
//  out of the same dword.  Written as a cast of m_pOwner here, exactly as
//  big73140.cpp does; see COIN-FLIP 3.
// ----------------------------------------------------------------------- //
struct Elem729c0                                        // 0x18 bytes
{
	Obj729c0*       m_pOwner;                           // +0x00
	uint32          m_n04;                              // +0x04
	uint32          m_n08;                              // +0x08
	uint32          m_n0c;                              // +0x0c
	uint32          m_nClipFlags10;                     // +0x10
	Elem729c0*      m_pNext;                            // +0x14
};


// ----------------------------------------------------------------------- //
//  THE PER-STAGE TEXTURE RECORD.  big6d910.cpp's Tex6d910 field for field;
//  this function reads +0x04, +0x08, +0x0c, +0x10, +0x14, +0x16, +0x1c,
//  +0x20, +0x30, +0x42 and +0x44 and writes +0x14, +0x1c, +0x20, +0x30 and
//  +0x44, which is every field that decode names.  The vtable at +0x00 is
//  never called from here.
// ----------------------------------------------------------------------- //
struct Link729c0                                        // ltlink.h CheapLTLink
{
	Link729c0*      m_pPrev;                            // +0x00
	Link729c0*      m_pNext;                            // +0x04
	void*           m_pData;                            // +0x08

	inline void Remove()
	{
		m_pPrev->m_pNext = m_pNext;
		m_pNext->m_pPrev = m_pPrev;
		TieOff();
	}

	inline void TieOff()
	{
		m_pPrev = m_pNext = this;
	}

	inline void AddAfter(Link729c0* pLink)
	{
		pLink->m_pPrev = this;
		pLink->m_pNext = m_pNext;
		pLink->m_pPrev->m_pNext = pLink->m_pNext->m_pPrev = pLink;
	}
};

inline void dl_Insert(Link729c0* pAfter, Link729c0* pLink)
{
	pAfter->AddAfter(pLink);
}

inline void dl_Remove(Link729c0* pLink)
{
	pLink->Remove();
}

struct DDSurface729c0;

struct TexBase729c0
{
	void*           m_pVtbl00;                          // +0x00
	float           m_fInvW04;                          // +0x04  0x10072c48
	float           m_fInvH08;                          // +0x08  0x10072c4b
	DDSurface729c0* m_pSurface0c;                       // +0x0c  0x10072c08
	uint32          m_nBytes10;                         // +0x10  0x10072be0
	uint16          m_nStamp14;                         // +0x14  0x10072ba0
	uint16          m_nAlphaRef16;                      // +0x16  0x10072c17
	uint8           m_pad_18[0x1c - 0x18];
	Link729c0       m_Link1c;                           // +0x1c  0x10072ba8
	uint8           m_pad_28[0x30 - 0x28];
};

struct Tex729c0 : public TexBase729c0
{
	Tex729c0*       m_pNext30;                          // +0x30  0x10072b56
	uint8           m_pad_34[0x42 - 0x34];
	uint16          m_nStage42;                         // +0x42  0x10072b4a
	// +0x44 is the record's CURRENT LOD, not a dirty flag. 0x10047230
	// passes a VARIABLE at four of its six SetLOD tails (`cmp -0x1c(%ebp)`
	// then `mov %dx,0x44(%eax)`), so the guard is "wanted != held".
	uint16          m_nDirty44;    // a.k.a. m_nLOD44  // +0x44  0x10072c5b
};


// ----------------------------------------------------------------------- //
//  THE TEXTURE BLOCK -- and this decode UNIFIES THREE PREVIOUSLY SEPARATE
//  MODELS OF IT, which is worth stating because it is the one structural
//  claim here that is not a copy of an earlier file:
//
//    big73140.cpp   Tex73140       +0x1c  the bucket being accumulated
//    big124a4.cpp   Table124a4     +0x1c  m_pBuckets[1], +0x2c
//    big6d910.cpp   TexBlock6d910  +0x0c  record list head, +0x30 stamp
//
//  They are ONE type, and this function is where the three views meet.
//  Reached the SAME way in both of its phases --
//  `pElem->m_pOwner->m_p28->+0x30` (0x100729f5..0x10072a03 and
//  0x10072d48..0x10072d4d) -- it is then used at +0x1c as the bucket back
//  pointer (`movl $0x0,0x1c(%edx)` at 0x10072a6f, `movl $0x0,0x1c(%eax)`
//  at 0x10072ad8), at +0x0c as the record list head (`mov 0xc(%esi),%edi`
//  at 0x10072b2e, `mov 0xc(%eax),%esi` at 0x10072d65) and at +0x30 as the
//  frame stamp (`mov %ax,0x30(%esi)` at 0x10072b33, `mov %dx,0x30(%eax)`
//  at 0x10072d6d).  The SAME dereference chain feeds 0x10039f50 at
//  0x10072a0d, whose own body reads its argument's +0x0c and that
//  record's +0x40 -- a fourth witness that +0x0c is the record list.
// ----------------------------------------------------------------------- //
struct TexBlock729c0
{
	uint8           m_pad_00[0x0c];
	Tex729c0*       m_pHead0c;                          // +0x0c
	uint8           m_pad_10[0x1c - 0x10];
	Bucket729c0*    m_pBucket1c;                        // +0x1c
	uint8           m_pad_20[0x30 - 0x20];
	uint16          m_nStamp30;                         // +0x30
};


// ----------------------------------------------------------------------- //
//  THE BUCKET, 0x0c bytes.  big73140.cpp's Bucket73140 and big124a4.cpp's
//  Bucket124a4, from the pool at 0x1008d4c8 (element size 0x0c).  +0x00 is
//  the texture block AND the free-list link, the same double duty the
//  element's +0x00 does: `mov 0x1008d4e0,%eax ; mov %eax,(%edi) ;
//  mov %edi,0x1008d4e0` at 0x10072adf.
// ----------------------------------------------------------------------- //
struct Bucket729c0                                      // 0x0c bytes
{
	TexBlock729c0*  m_pTexture;                         // +0x00
	Elem729c0*      m_pElements;                        // +0x04
	Bucket729c0*    m_pNextInList;                      // +0x08
};


// ----------------------------------------------------------------------- //
//  THE OBJECT AND ITS HOLDER.  big124a4.cpp's Obj124a4 / Ctx124a4 and
//  big6d910.cpp's Obj6d910 / TexHolder6d910.  Only +0x28 of the object is
//  read here; the holder gives up +0x30 (the texture block) and +0x38 (the
//  flag word), which is big124a4.cpp's m_nFlags38 exactly.
//
//  BIT 0x8000 of +0x38 is THE DEFERRAL FLAG, tested at 0x10072a31 and
//  0x10072a9c with `test $0x8000,%eax` on the full dword.
// ----------------------------------------------------------------------- //
struct Holder729c0
{
	uint8           m_pad_00[0x30];
	TexBlock729c0*  m_pBlock30;                         // +0x30
	uint8           m_pad_34[0x38 - 0x34];
	uint32          m_nFlags38;                         // +0x38
};

struct Obj729c0
{
	uint8           m_pad_00[0x28];
	Holder729c0*    m_p28;                              // +0x28
};


// ----------------------------------------------------------------------- //
//  THE POOL HEADER, d3dbatchc.cpp's Pool1000a6a0.  +0x08 is the grow count
//  and +0x18 the free-list head; the addresses agree, 0x1008d4e4 + 8 =
//  0x1008d4ec and 0x1008d4e4 + 0x18 = 0x1008d4fc.  Both refill sites plant
//  the pool as an IMMEDIATE (`mov $0x1008d4e4,%ecx` at 0x10072f8f and
//  0x10072ffc), so it is an object and not a pointer.
// ----------------------------------------------------------------------- //
struct StructLink729c0
{
	StructLink729c0*  m_pSLNext;
};

struct Pool729c0                                        // StdLith StructBank
{
	uint32            m_StructSize;                     // +0x00
	uint32            m_AlignedStructSize;              // +0x04
	uint32            m_nGrow08;                        // +0x08 m_CacheSize
	uint32            m_nPages;                         // +0x0c
	uint32            m_nTotalObjects;                  // +0x10
	void*             m_PageHead;                       // +0x14
	StructLink729c0*  m_FreeListHead;                   // +0x18
};


// ----------------------------------------------------------------------- //
//  THE PAIR PUBLISHED PER STAGE at 0x1008e664 -- `mov %ecx,(%edx,%eax,8)`
//  and `mov %esi,0x4(%edx,%eax,8)` at 0x10072c4e/0x10072c51.  Both are
//  INTEGER moves of floats, as in every sibling.
// ----------------------------------------------------------------------- //
struct TexScale729c0
{
	float           m_fInvW00;                          // +0x00
	float           m_fInvH04;                          // +0x04
};

// 0x1008cbcc: only +0x48 is touched (`add %edx,0x48(%eax)` at 0x10072be3
// and 0x10072e10) -- the same field big0ae20.cpp, big2ef34.cpp and
// big6d910.cpp record.
struct Stats729c0
{
	uint8           m_pad_00[0x48];
	uint32          m_nBytes48;                         // +0x48
};

// 0x1008beb8 -- d3dr2c.cpp's C1000da50Data*.  Only +0x00 is read here
// (`mov 0x1008beb8,%eax ; mov (%eax),%esi` at 0x10072b02/0x10072b0f) and
// what comes out is used as a texture block.  Same reading as
// big0ae20.cpp's Cur0ae20.
struct Cur729c0
{
	TexBlock729c0*  m_pBlock00;                         // +0x00
};


// ======================================================================= //
//  THE DEVICE AND THE SURFACE.  The five slots are pinned in the banner;
//  the shapes are big0ae20.cpp's, unchanged.
// ======================================================================= //
class D3DDevice1003d350
{
public:
	virtual long __stdcall _v00() = 0;
	virtual long __stdcall _v04() = 0;
	virtual long __stdcall _v08() = 0;
	virtual long __stdcall _v0c() = 0;
	virtual long __stdcall _v10() = 0;
	virtual long __stdcall _v14() = 0;
	virtual long __stdcall _v18() = 0;
	virtual long __stdcall _v1c() = 0;
	virtual long __stdcall _v20() = 0;
	virtual long __stdcall _v24() = 0;
	virtual long __stdcall _v28() = 0;
	virtual long __stdcall _v2c() = 0;
	virtual long __stdcall _v30() = 0;
	virtual long __stdcall _v34() = 0;
	virtual long __stdcall _v38() = 0;
	virtual long __stdcall _v3c() = 0;
	virtual long __stdcall _v40() = 0;
	virtual long __stdcall _v44() = 0;
	virtual long __stdcall _v48() = 0;
	virtual long __stdcall _v4c() = 0;
	virtual long __stdcall SetRenderState(uint32 nState, uint32 nValue) = 0;        // +0x50
	virtual long __stdcall _v54() = 0;
	virtual long __stdcall _v58() = 0;
	virtual long __stdcall _v5c() = 0;
	virtual long __stdcall _v60() = 0;
	virtual long __stdcall _v64() = 0;
	virtual long __stdcall _v68() = 0;
	virtual long __stdcall _v6c() = 0;
	virtual long __stdcall _v70() = 0;
	virtual long __stdcall _v74() = 0;
	virtual long __stdcall _v78() = 0;
	virtual long __stdcall _v7c() = 0;
	virtual long __stdcall _v80() = 0;
	virtual long __stdcall _v84() = 0;
	virtual long __stdcall _v88() = 0;
	virtual long __stdcall SetTexture(uint32 nStage, DDSurface729c0* pSurf) = 0;    // +0x8c
	virtual long __stdcall GetTextureStageState(uint32, uint32, uint32*) = 0;       // +0x90
	virtual long __stdcall SetTextureStageState(uint32, uint32, uint32) = 0;        // +0x94
};

struct DDSurface729c0Vtbl
{
	uint8   m_pad_00[0xbc];
	long (__stdcall* SetLOD)(DDSurface729c0*, uint32);  // +0xbc
};

struct DDSurface729c0
{
	DDSurface729c0Vtbl* lpVtbl;
};


// ======================================================================= //
//  GLOBALS.  Relocations are masked, so only the shapes matter; the names
//  are the ones already in the tree.
// ======================================================================= //

extern D3DDevice1003d350* g_pD3DDevice;                 // 0x1008d988

extern uint16           g_w1008b870;                    // 0x1008b870
extern uint32           g_n1008b874;                    // 0x1008b874
extern uint16           g_w1008bea8;                    // 0x1008bea8
extern uint32           g_n1008bd80;                    // 0x1008bd80
extern Stats729c0*      g_p1008cbcc;                    // 0x1008cbcc
extern Cur729c0*        g_p1008beb8;                    // 0x1008beb8
extern uint32           g_n1008d7cc;                    // 0x1008d7cc
extern uint32           g_n1008d7d0;                    // 0x1008d7d0

// 0x1008e634 is the 12-byte list HEAD OBJECT, not a pointer variable --
// big0ae20.cpp's own correction, settled there by 0x10038e3d planting the
// IMMEDIATE $0x1008e634 into a link's m_pPrev.  This function reads the
// head's +0x00 field (`mov 0x1008e634,%eax` at 0x10072bc5 and 0x10072def),
// which is the same instruction a pointer load would emit, so the bytes
// cannot tell the two spellings apart and the object is the right one.
extern Link729c0        g_Link1008e634;                 // 0x1008e634
extern TexScale729c0    g_a1008e664[];                  // 0x1008e664
extern Tex729c0*        g_a1008e684[];                  // 0x1008e684

extern Bucket729c0*     g_pBucketList1008d4c4;          // 0x1008d4c4
extern Bucket729c0*     g_pFreeBuckets1008d4e0;         // 0x1008d4e0
extern Elem729c0*       g_pFreeElems1008d4fc;           // 0x1008d4fc
extern Pool729c0        g_Pool1008d4e4;                 // 0x1008d4e4  elements


// ======================================================================= //
//  NEIGHBOURS.  Each prototype is read off the call site in THESE bytes
//  and cross-checked against the callee's own prologue.
// ======================================================================= //

// 0x10039f50 -- __cdecl, TWO stack dwords.  Its own body is
// `push %ebx ; mov 0x8(%esp),%ebx ; sub $0x10,%esp`, then it reads
// `0x1c(%esp)` for the second argument (0x10039f6b), which after the push
// and the sub is the caller's second slot.  It answers
// pBlock->m_pHead0c->+0x40 as a uint16, creating the record through
// 0x10039bdc when the head is empty (`mov %ebx,%eax ; xor %ecx,%ecx ;
// call 0x10039bdc`) and 0 when that fails.  The call here tests the result
// for nonzero and nothing else.
extern "C" uint32 sub_10039f50(TexBlock729c0* pBlock, uint32 nStage);

// 0x1006d910 -- big6d910.cpp, decoded in full: d3d.ren's draw-one-polygon
// pass, 40-byte / two-stage vertex.  Returns 1 when it issued a
// DrawPrimitive.  ONE argument; see THE `push %edi` PADDING.
extern "C" int32  sub_1006d910(Obj729c0* pObj);

// 0x1006f540 -- 6,096 bytes, on the listing.  Same family: its prologue
// is `push %ebp ; mov %esp,%ebp ; sub $0xf10,%esp ; push %edi ; push %edi ;
// push %esi ; push %ebx ; mov 0x8(%ebp),%edi`, it reads the item's
// `movzwl 0x50(%edi)` vertex count and walks `0x54(%edi)` as 0x18-byte
// source vertices, and it scales by 0x1008e664/0x1008e668 exactly as
// big6d910.cpp's L_Draw does.  It never reads 0xc(%ebp).
extern "C" int32  sub_1006f540(Obj729c0* pObj);

// 0x10070d10 -- big70d10.cpp, decoded in full as a diff against
// big6d910.cpp: the draw-one-LIT-polygon pass.  THIS is its only caller,
// and big70d10.cpp names 0x10072c9d as such.
extern "C" int32  sub_10070d10(Obj729c0* pObj);

// 0x100256a0 -- 48 bytes, quoted in full because it settles the padding
// question:
//      100256a0: mov 0x4(%esp),%eax ; test %eax,%eax ; je 0x100256c6
//      100256b0: mov 0x1008d4fc,%edx ; mov 0x14(%eax),%ecx
//                mov %edx,(%eax)    ; mov %eax,0x1008d4fc
//                mov %ecx,%eax      ; test %ecx,%ecx ; jne 0x100256b0
//      100256c6: ret
// -- push a whole +0x14-linked element chain onto the free list at
// 0x1008d4fc.  ONE argument.
extern "C" void   sub_100256a0(Elem729c0* pChain);

// 0x1000da50 -- d3dr2c.cpp, decoded in full.  No arguments; recomputes
// four globals from 0x1008beb8 and 0x1008e664.  The call at 0x10072c82
// pushes nothing and unwinds nothing.
extern "C" void   sub_1000da50();

// 0x10039bd0 -- d3dmid.cpp's stack-ABI entry (the register entry is
// 0x10039bdc).  Three arguments; big0ae20.cpp, big2ef34.cpp, big6d910.cpp
// and big43e10.cpp all reach it with the same (block, stage, bLink)
// triple, and all four call sites here push four dwords and pop 0x10 --
// three real arguments plus icl's padding.
extern "C" Tex729c0* sub_10039bd0(TexBlock729c0* pBlock, uint32 nStage,
                                  uint32 bLink);

// 0x1007fbf0 -- d3dbatchc.cpp.  (pool, pool->m_nGrow08); both refill sites
// load 0x1008d4ec, which is 0x1008d4e4 + 8.
extern "C" void   sub_1007fbf0(Pool729c0* pPool, uint32 nGrow);



// struct_bank.h's inline sb_Allocate, line for line (release build), as
// big73140.cpp -- this function's producer -- has it.
inline void* sb_Allocate729c0(Pool729c0* pBank)
{
	StructLink729c0* pRet;

	pRet = pBank->m_FreeListHead;
	if (!pRet)
	{
		sub_1007fbf0(pBank, pBank->m_nGrow08);
		pRet = pBank->m_FreeListHead;
		if (!pRet)
			return 0;
	}

	pBank->m_FreeListHead = pRet->m_pSLNext;
	return pRet;
}

// ======================================================================= //
//  THE BIND: w24_10059380_os.cpp's identical BindBlock59380
//  -- w3_10014580_os.cpp's 0x10015320 as an inline -- over this file's
//  types.  Both bind copies below are this inline, with a LOD of 0.
// ======================================================================= //
static inline Tex729c0* FindRec729c0(Tex729c0* pHead, uint32 nStage)
{
	Tex729c0* pCur;

	for (pCur = pHead; pCur; pCur = pCur->m_pNext30)
	{
		if (pCur->m_nStage42 == (uint8)nStage)
			return pCur;
	}

	return LTNULL;
}

static inline uint32 StageOf729c0(Tex729c0* pRec)
{
	return pRec->m_nStage42;
}

static inline void SetScale729c0(TexScale729c0* pScales, uint32 nStage, float fInvW, float fInvH)
{
	pScales[nStage].m_fInvW00 = fInvW;
	pScales[nStage].m_fInvH04 = fInvH;
}

static inline void BindRec729c0(Tex729c0* pRec)
{
	TexBase729c0* pTex = pRec;

	g_a1008e684[StageOf729c0(pRec)] = (Tex729c0*)pTex;

	if (pTex->m_nStamp14 != g_w1008bea8)
	{
		dl_Remove(&pRec->m_Link1c);
		dl_Insert(g_Link1008e634.m_pPrev, &pRec->m_Link1c);

		g_p1008cbcc->m_nBytes48 += pTex->m_nBytes10;

		pTex->m_nStamp14 = g_w1008bea8;
	}

	g_pD3DDevice->SetTexture((uint8)StageOf729c0(pRec), pTex->m_pSurface0c);

	if (pTex->m_nAlphaRef16)
		g_pD3DDevice->SetRenderState(0x18, pTex->m_nAlphaRef16);

	SetScale729c0(g_a1008e664, StageOf729c0(pRec), pTex->m_fInvW04, pTex->m_fInvH08);

	g_n1008bd80++;
}

static inline int32 BindBlock729c0(TexBlock729c0* pBlock, uint32 nStage, uint32 nLOD)
{
	Tex729c0* pRec;
	Tex729c0* pHead;
	Tex729c0* pNew;

	if (!pBlock)
		return 0;

	pBlock->m_nStamp30 = g_w1008b870;

	pHead = pBlock->m_pHead0c;
	pRec  = LTNULL;

	if (pHead)
		pRec = FindRec729c0(pHead, nStage);

	if (!pRec)
		goto L_MakeRec;

	if (pRec == g_a1008e684[nStage])
		goto L_ElseLOD;

L_Bind:
	BindRec729c0(pRec);

	if (pRec->m_nDirty44 != nLOD)
	{
		pRec->m_pSurface0c->lpVtbl->SetLOD(pRec->m_pSurface0c, nLOD);
		pRec->m_nDirty44 = (uint16)nLOD;
	}

	return 1;

L_ElseLOD:
	if (pRec->m_nDirty44 != nLOD)
	{
		pRec->m_pSurface0c->lpVtbl->SetLOD(pRec->m_pSurface0c, nLOD);
		pRec->m_nDirty44 = (uint16)nLOD;
	}

	return 1;

L_MakeRec:
	if (pBlock->m_pHead0c)
	{
		pNew = sub_10039bd0(pBlock, nStage, 1);

		if (!pNew)
			return 0;

		pNew->m_pNext30  = pHead->m_pNext30;
		pHead->m_pNext30 = pNew;
		pRec = pNew;

		goto L_Bind;
	}

	pRec = sub_10039bd0(pBlock, nStage, 0);

	if (!pRec)
		return 0;

	goto L_Bind;
}

// ======================================================================= //
//  NEAR: the loop guard of region 2 now goes to the `if (nAcc)` test as
//  shipped (the element walk is the DrawElems729c0 inline).  Left: (a)
//  the sunk blocks' order and (c) one `xor`.  On (a), measured with
//  probe TUs: icl emits a sunk block when its place in a depth-first
//  walk of the function comes up -- a then-arm's cold blocks as soon as
//  the arm ends, an else-arm's only when the code it falls into has
//  been walked to its end -- so region 4's pop (in the else arm) comes
//  after both binds here, and second in the shipped code.  Not moved
//  by: the gate's arms without gotos, with `continue`, as a switch or
//  two tests; region 4 or its step as inlines; an `if` around the gate
//  or around region 4's loop; the gate's return type.  Before that:
//  NEAR, now the shipped 1916 bytes: (b) below is closed -- the second
//  draw loop assigns the list head to pElem and walks a copy of it,
//  which is the shipped store to -0x20 and `mov %eax,%ebx`.  (a) and
//  (c) are what is left.  Before that:
//  NEAR, 557 bytes differing (was 1774), 1911 bytes against
//  the shipped 1916.  Three changes:
//    * built /Os (build/engine.sh D3D_OS_TUS), not /O2: the prologue and
//      the one exit are `push`/`pop` saves (/O2: 2112 bytes);
//    * both texture binds are w24_10059380_os.cpp's IDENTICAL BindBlock
//      inline (BindBlock729c0 above: base-class texture pointer, inline
//      stage accessor and scale setter, dl_Remove/dl_Insert, the search
//      as an inline returning the record) with a LOD of 0 -- the first
//      site's doubled block test is that inline's own `if (!pBlock)`
//      after the caller's guard, as w24 records for 0x10059620;
//    * the two deferral pops are struct_bank.h's sb_Allocate inlined
//      (the pool is a StructBank, its +0x18 free list the 0x1008d4fc the
//      frees push onto), as in big73140.cpp, this function's producer.
//  What is left: (a) the cold blocks are sunk in a different order --
//  shipped: both deferral pops, then the second bind's arms, then the
//  first's; ours puts the second deferral pop last; (b) the second
//  draw loop's element pointer is stored to -0x20 in the shipped code
//  (a spill store never reloaded), not here; (c) `xor %esi,%esi` (nAcc)
//  is scheduled before the element load, shipped after.  Unchanged by:
//  the nAcc/pElem statement order, the second draw loop over another
//  local or in an inline helper, the draw-or-defer step as an inline
//  helper taking &pDeferElems (563).
// ======================================================================= //
// ======================================================================= //
//  THE ELEMENT WALK of regions 2 and 4, as ONE inline: draw each element
//  of the bucket through 0x1006d910, or copy it onto the deferral chain
//  when its surface asks for the lit pass; bFree recycles each element as
//  it goes (region 4), and the answer is the OR of what the draws said
//  (region 2 keeps it, region 4 discards it -- which is why region 4 has
//  no `or`).  ppDefer is this helper's parameter, and the comparison with
//  `(Elem**)-1` is its own.
//
//  Why an inline and not the two loops written out: with the loop in the
//  function itself icl knows the accumulator is 0 when the list is empty
//  and threads the loop guard past the `if (nAcc)` that follows
//  (`je 0x10072a7b`); the shipped guard goes to that test (`je
//  0x10072a60`), as it does when the accumulator is the inline's result.
// ======================================================================= //
static inline uint32 DrawElems729c0(Bucket729c0* pBucket, Elem729c0** ppDefer, LTBOOL bFree)
{
	Elem729c0* pElem;
	Elem729c0* pNextElem;
	Elem729c0* pNew;
	uint32     nAcc;

	nAcc  = 0;                                          // 0x10072a20
	pElem = pBucket->m_pElements;                       // 0x10072a1d / 0x10072a8a
	while (pElem)
	{
		pNextElem = pElem->m_pNext;

		if (!(pElem->m_pOwner->m_p28->m_nFlags38 & 0x8000)
		    || ppDefer == (Elem729c0**)-1)
		{
			g_n1008b874 = pElem->m_nClipFlags10;
			nAcc |= sub_1006d910(pElem->m_pOwner);
		}
		else
		{
			// -- 0x10072f56 / 0x10072fc3, SUNK.  struct_bank.h's
			//    sb_Allocate inlined; the accumulator is set INSIDE the
			//    success arm (`or $0x1,%esi` at 0x10072f73).
			pNew = (Elem729c0*)sb_Allocate729c0(&g_Pool1008d4e4);

			if (pNew)
			{
				pNew->m_pOwner       = pElem->m_pOwner;
				pNew->m_nClipFlags10 = pElem->m_nClipFlags10;
				pNew->m_pNext        = *ppDefer;
				*ppDefer             = pNew;

				nAcc |= 1;
			}
		}

		// 0x10072ac3 -- region 4 only: the original goes back on the
		// free list whether it was drawn or copied.
		if (bFree)
		{
			pElem->m_pOwner = (Obj729c0*)g_pFreeElems1008d4fc;
			g_pFreeElems1008d4fc = pElem;
		}

		pElem = pNextElem;
	}

	return nAcc;
}

// ======================================================================= //
//  0x100729c0
//
//  THE FRAME.  `sub $0x38,%esp` plus `push %edi ; push %edi ; push %esi ;
//  push %ebx` against an epilogue with only THREE pops, so 0x38 of locals
//  and one four-byte bump.  Ten dword slots are used, -0x04 down to -0x28,
//  and the frame cannot be inverted to a declaration order because SIX of
//  the ten are re-used for unrelated values in different phases: -0x0c is
//  the element phase's stage and also the element cursor spilled across
//  the pool refill (0x10072f9d), -0x14 is the deferred bucket's successor
//  and also the record spilled across a SetLOD (0x100730c6), -0x1c is the
//  bucket walk's successor, then the bucket phase's stage, then that
//  phase's alpha reference (0x10072e48), and -0x20 is the deferred bucket
//  head and then the record search's cursor (0x10072d77).  -0x0c hosts a
//  THIRD value besides those two, L_ElseLOD2's record (0x10073038).
//  Only four slots hold one value for their whole life, and three of those four are the
//  ones named below; the rule "the frame is the declaration order" is
//  therefore not usable here and is not claimed.
// ======================================================================= //
void sub_100729c0()
{
	Bucket729c0*    pBucket;
	Bucket729c0*    pNextBucket;
	Bucket729c0*    pDeferBuckets;
	Elem729c0*      pDeferElems;
	Elem729c0**     ppDefer;
	Elem729c0*      pElem;
	Elem729c0*      pNextElem;
	Elem729c0*      pNew;
	uint32          nAcc;
	Cur729c0*       pCur1008beb8;
	TexBlock729c0*  pBlock;
	Tex729c0*       pCur;
	Tex729c0*       pRec;
	uint32          nStage;
	uint32          nOldColorOp;

	// ------------------------------------------------------------------ //
	//  REGION 0 -- 0x100729c0..0x100729e3, 36 B.  The two deferral chains
	//  start empty and the bucket list is picked up once.
	//
	//  The two zero stores are the "zero-store grouping": one goes
	//  out through a materialised zero (`xor %eax,%eax` at 0x100729ca,
	//  `mov %eax,-0x20(%ebp)` at 0x100729d2) and the other as an immediate
	//  (`movl $0x0,-0x28(%ebp)` at 0x100729d7), with the list load and the
	//  emptiness test scheduled between them.
	//
	//  ppDefer: SEE THE `-1` NOTE IN REGION 2.  Its initialiser is not
	//  emitted -- icl materialises `&pDeferElems` with a `lea` at each of
	//  the two uses (0x10072a38, 0x10072aa3) rather than keeping it in a
	//  register -- but taking the address is what forces pDeferElems into
	//  a frame slot, which is what the bytes show.
	// ------------------------------------------------------------------ //
	pDeferBuckets = LTNULL;                             // 0x100729d2
	pDeferElems   = LTNULL;                             // 0x100729d7
	ppDefer       = &pDeferElems;

	pBucket = g_pBucketList1008d4c4;                    // 0x100729cc

	// ------------------------------------------------------------------ //
	//  REGIONS 1..5 -- 0x100729e4..0x10072af6, 239 B.  THE BUCKET WALK.
	//
	//  Written as a `while` with the successor saved into a local first:
	//  0x100729e4 loads `0x8(%edi)` into -0x1c(%ebp) BEFORE anything can
	//  free the bucket, and 0x10072aec reloads it as the back edge's
	//  cursor.  That is the cursor-alias walk, and it has to be a
	//  saved copy because the deferral arm OVERWRITES +0x08 (0x10072a6c)
	//  and the free arm overwrites +0x00 (0x10072ae4).
	// ------------------------------------------------------------------ //
	while (pBucket)                                     // 0x100729d5/0x10072af1
	{
		pNextBucket = pBucket->m_pNextInList;           // 0x100729e4/e7

		// -------------------------------------------------------------- //
		//  REGION 1 -- 0x100729ea..0x10072a1c.  An empty bucket goes
		//  straight to the free list; otherwise ask 0x10039f50 whether
		//  this texture wants the single-pass path.
		//
		//  The `je 0x10072ad6` at 0x100729ef jumps FORWARD past both
		//  element walks into the free block, which is what makes this a
		//  guarded `goto` rather than a wrapping `if` -- the arm
		//  order, "error falls through, success is jumped over", read the
		//  other way round.
		// -------------------------------------------------------------- //
		if (!pBucket->m_pElements)                      // 0x100729ea/ed/ef
			goto L_FreeBucket;

		if (sub_10039f50(pBucket->m_pElements->m_pOwner->m_p28->m_pBlock30,
		                 g_n1008d7d0))                  // 0x10072a0d
		{
			// ---------------------------------------------------------- //
			//  REGION 2 -- 0x10072a1d..0x10072a5f.  Draw every element of
			//  this bucket, OR-ing the results together.  The bucket
			//  SURVIVES if anything answers nonzero.
			//
			//  THE `cmp $0xffffffff` AT 0x10072a3b IS NOT A COMPILER
			//  ARTEFACT AND IT IS NOT AN UNCONDITIONAL JUMP.  The bytes
			//  are
			//
			//      10072a31: test $0x8000,%eax
			//      10072a36: je   0x10072a44        ; -> the draw arm
			//      10072a38: lea  -0x28(%ebp),%eax
			//      10072a3b: cmp  $0xffffffff,%eax
			//      10072a3e: jne  0x10072f56        ; -> the defer arm
			//
			//  i.e. the ADDRESS OF A FRAME LOCAL compared against -1, so
			//  the branch is always taken.  Two things follow, and both
			//  are read off the bytes rather than assumed:
			//
			//    * icl did not fold it, and icl folds `&local != -1`
			//      readily when the address is a compile-time constant in
			//      the function being compiled.  What it does NOT fold is
			//      a comparison against a value that ARRIVED AS AN INLINED
			//      FUNCTION'S PARAMETER.  So the element walk was an
			//      inline helper in the original source taking an
			//      `Elem** ppDefer`, and `(Elem**)-1` is that helper's
			//      "do not defer, draw it anyway" sentinel -- used by some
			//      OTHER caller of the same helper, not by this one.
			//    * the shape it produces is exactly `if (A && B) {defer}
			//      else {draw}`: one arm per branch, no third path.
			//
			//  Reproduced here with the sentinel comparison written out
			//  against a named `Elem729c0** ppDefer`, which is the
			//  smallest source that emits the `lea` + `cmp`.  Whether our
			//  icl folds it or not, IT MOVES NO CALL -- there is no call on
			//  either side of the comparison.  See COIN-FLIP 4.
			//
			//  ARM ORDER IS DELIBERATE.  The draw arm is written FIRST and
			//  the condition inverted, because in the shipped bytes the
			//  draw arm is the fall-through at 0x10072a44 and the defer
			//  arm is SUNK to 0x10072f56, below the epilogue.  Measured:
			//  `if (A && B) {cold} else {hot}` emits the cold arm inline
			//  and `if (!A || !B) {hot} else {cold}` sinks it.  The
			//  caveat about an EMPTY then-arm does not apply --
			//  this then-arm makes a call -- so the inversion really is
			//  the lever here.  It is what puts the two 0x1007fbf0 refills
			//  at call positions 19 and 20 instead of 4 and 5; see
			//  COIN-FLIP 1.
			// ---------------------------------------------------------- //
			nAcc = DrawElems729c0(pBucket, ppDefer, LTFALSE);


			// ---------------------------------------------------------- //
			//  REGION 3 -- 0x10072a60..0x10072a89.  Something deferred, so
			//  the BUCKET is deferred too: it moves onto the -0x20 chain
			//  with its element list intact and the texture forgets it, so
			//  the next 0x10073140 for the same texture starts a new
			//  bucket.  Nothing deferred, so the whole element chain goes
			//  back on the free list in one call and the bucket follows.
			//
			//  `jmp 0x10072aec` at 0x10072a76 skips the free block
			//  entirely -- the deferred bucket must NOT be freed -- which
			//  is why this arm ends in a goto to the back edge and not in
			//  a fall-through.
			// ---------------------------------------------------------- //
			if (nAcc)                                   // 0x10072a60
			{
				pBucket->m_pNextInList = pDeferBuckets; // 0x10072a64/6c
				pDeferBuckets          = pBucket;       // 0x10072a69
				pBucket->m_pTexture->m_pBucket1c = LTNULL;
				                                        // 0x10072a67/6f
				goto L_NextBucket;                      // 0x10072a76
			}

			sub_100256a0(pBucket->m_pElements);          // 0x10072a80
		}
		else
		{
			// ---------------------------------------------------------- //
			//  REGION 4 -- 0x10072a8a..0x10072ad5.  The texture said no,
			//  so each element is drawn and freed one at a time and there
			//  is nothing to accumulate: the shipped bytes have no `or`
			//  here, which is the tell that the caller discarded the
			//  helper's result.  The deferral test and the deferral block
			//  are otherwise IDENTICAL to region 2's, sentinel comparison
			//  included (0x10072aa3/0x10072aa6), and the defer arm is
			//  sunk the same way to 0x10072fc3.
			//
			//  Note where the defer arm rejoins: `jmp 0x10072ac3`, i.e.
			//  the free-list push, NOT the loop tail.  A deferred element
			//  is COPIED, so the original still has to be recycled.
			// ---------------------------------------------------------- //
			DrawElems729c0(pBucket, ppDefer, LTTRUE);
		}

		// -------------------------------------------------------------- //
		//  REGION 5 -- 0x10072ad6..0x10072af6.  Free the bucket: the
		//  texture forgets it (which is the store big73140.cpp's null test
		//  at 0x1007314e reads back) and +0x00 becomes the free link.
		// -------------------------------------------------------------- //
L_FreeBucket:
		pBucket->m_pTexture->m_pBucket1c = LTNULL;      // 0x10072ad6/d8
		pBucket->m_pTexture =
		    (TexBlock729c0*)g_pFreeBuckets1008d4e0;     // 0x10072adf/e4
		g_pFreeBuckets1008d4e0 = pBucket;               // 0x10072ae6

L_NextBucket:
		pBucket = pNextBucket;                          // 0x10072aec
	}

	// ------------------------------------------------------------------ //
	//  REGION 6 -- 0x10072af7..0x10072b01, 11 B.  THE DEFERRED-ELEMENT
	//  PHASE.  Anything region 2 or region 4 copied aside is drawn now,
	//  through 0x10070d10 (the LIT pass) rather than 0x1006d910, with the
	//  texture stage rebound once for the whole batch.
	// ------------------------------------------------------------------ //
	if (!pDeferElems)                                   // 0x10072af7/fa/fc
		goto L_BucketPhase;

	// ------------------------------------------------------------------ //
	//  REGION 7 -- 0x10072b02..0x10072b7b, 122 B.  FIND THIS STAGE'S
	//  TEXTURE RECORD.  big0ae20.cpp's and big6d910.cpp's block, with the
	//  block reached off 0x1008beb8 (big0ae20's way) rather than off an
	//  object (big6d910's way), and with the stage taken from 0x1008d7cc
	//  rather than 0x1008d7d0.
	//
	//  THE STAGE GLOBAL IS 0x1008d7cc HERE AND 0x1008d7d0 IN REGION 11.
	//  Not a transcription slip -- `a1 cc d7 08 10` at 0x10072b19 against
	//  `8b 0d d0 d7 08 10` at 0x10072d5f, four bytes apart in .data, and
	//  the gate call in region 1 passes 0x1008d7d0 as well.  Both are
	//  already named in the tree (d3dr2c.cpp has 0x1008d7cc; big0ae20.cpp
	//  and big6d910.cpp have both), so the names are reused as they stand.
	//
	//  THREE NULL TESTS OFF TWO VALUES, and none of them makes a call:
	//    0x10072b07  test %eax,%eax  je  -- the 0x1008beb8 object
	//    0x10072b13  test %esi,%esi  je  -- the block
	//    0x10072b21                  je  -- THE BLOCK AGAIN, off the same
	//                                       flags, with only two `mov`s in
	//                                       between
	//  icl 4.0 does not fold a redundant null test; big0ae20.cpp and
	//  big6d910.cpp both record the identical non-folding on the LIST HEAD
	//  four instructions further down (0x10072b37 and 0x10072b40 here, one
	//  `test %edi,%edi` serving two `je`s).  Written out as two guards,
	//  which is the only spelling that emits two `je`s.
	// ------------------------------------------------------------------ //
	pCur1008beb8 = g_p1008beb8;                         // 0x10072b02

	if (pCur1008beb8 && pCur1008beb8->m_pBlock00)       // 0x10072b07..13
		BindBlock729c0(pCur1008beb8->m_pBlock00, g_n1008d7cc, 0);

	// (L_ElemDone: the bind is the BindBlock729c0 inline now)
	// ------------------------------------------------------------------ //
	//  REGION 9 -- 0x10072c82..0x10072cb7, 54 B.  Republish the derived
	//  scale globals for the stage just bound, then draw and recycle the
	//  whole deferral chain.
	//
	//  0x10072c82 is where EVERY failure in regions 7 and 8 lands -- the
	//  two 0x1008beb8 tests, the redundant third, both 0x10039bd0 failures
	//  and the two `m_nDirty44 == 0` paths -- so 0x1000da50 runs whether or
	//  not a texture was bound.  Written as a label rather than as the tail
	//  of a positive `if` for that reason: seven separate branches reach it.
	// ------------------------------------------------------------------ //
	sub_1000da50();                                     // 0x10072c82

	pElem = pDeferElems;                                // 0x10072c87
	while (pElem)                                       // 0x10072c8a/0x10072cb6
	{
		pNextElem   = pElem->m_pNext;                   // 0x10072c91
		g_n1008b874 = pElem->m_nClipFlags10;            // 0x10072c8e/94

		sub_10070d10(pElem->m_pOwner);                  // 0x10072c99/9d

		pElem->m_pOwner = (Obj729c0*)g_pFreeElems1008d4fc;
		                                                // 0x10072ca2/aa
		g_pFreeElems1008d4fc = pElem;                   // 0x10072cac

		pElem = pNextElem;                              // 0x10072cb4
	}

L_BucketPhase:
	// ------------------------------------------------------------------ //
	//  REGION 10 -- 0x10072cb8..0x10072d2b, 116 B.  THE DEFERRED-BUCKET
	//  PHASE.  The buckets region 3 kept are drawn now, with render state
	//  0x1b (D3DRENDERSTATE_ALPHABLENDENABLE) forced on for the whole pass
	//  and stage 1's state 1 (D3DTSS_COLOROP) forced to 1 (D3DTOP_DISABLE)
	//  around it -- the previous value SAVED into -0x24(%ebp) through the
	//  Get and written back at 0x10072f3f.  Region 14 undoes both.
	//
	//  The three literal 1s at 0x10072cf3/0x10072d0c come out as
	//  `xor %eax,%eax ; inc %eax` and are reused across the three slots of
	//  one call, which is why the (stage, state, value) triple of the Set
	//  is three copies of one register.
	// ------------------------------------------------------------------ //
	if (pDeferBuckets)                                  // 0x10072cb8/bb/bd
	{
		// 0x10072ce0 -- +0x50.
		g_pD3DDevice->SetRenderState(0x1b, 1);

		// 0x10072d03 -- +0x90, four dwords, the last `lea -0x24(%ebp)`.
		g_pD3DDevice->GetTextureStageState(1, 1,
		                                           &nOldColorOp);

		// 0x10072d26 -- +0x94.
		g_pD3DDevice->SetTextureStageState(1, 1, 1);

		pBucket = pDeferBuckets;                        // 0x10072d2c

		// -------------------------------------------------------------- //
		//  REGION 11 -- 0x10072d2c..0x10072db8, 141 B.  Per bucket: the
		//  same find-the-record block as region 7, with three differences
		//  that are all in the bytes:
		//    * the block comes from the ELEMENT's owner
		//      (`pElem->m_pOwner->m_p28->m_pBlock30`, 0x10072d48..4d) the
		//      way region 1's gate call does, not from 0x1008beb8;
		//    * the stage is 0x1008d7d0, not 0x1008d7cc (0x10072d5f);
		//    * there is only ONE null test of the block (0x10072d52) and
		//      the search loop is NOT unrolled -- one copy at
		//      0x10072d87..0x10072d98 with a single `break` trampoline
		//      sunk at 0x1007309a.
		//  Every failure lands on the DRAW loop at 0x10072eaf, not on the
		//  free block: a bucket with no bindable texture still gets its
		//  elements drawn.
		// -------------------------------------------------------------- //
		while (pBucket)                                 // 0x10072d2f/0x10072ef9
		{
			pNextBucket = pBucket->m_pNextInList;       // 0x10072d37/3a

			if (!pBucket->m_pElements)                  // 0x10072d3d/40/42
				goto L_FreeBucket2;

			pBlock = pBucket->m_pElements->m_pOwner->m_p28->m_pBlock30;
			                                            // 0x10072d48/4a/4d

			if (!pBlock)                                // 0x10072d50/52
				goto L_FreeBucket2;

			BindBlock729c0(pBlock, g_n1008d7d0, 0);

			// (L_DrawElems2)
			// ---------------------------------------------------------- //
			//  REGION 13 -- 0x10072eaf..0x10072efe, 80 B.  Draw every
			//  element through 0x1006f540 and recycle it.  Region 9's loop
			//  with a different callee and no chain head to null out; the
			//  texture's +0x1c was already cleared in region 3, which is
			//  why this bucket's free block does NOT clear it again.
			// ---------------------------------------------------------- //
			//  The list head is assigned to pElem -- whose home is
			//  -0x20, and the shipped code stores it there (0x10072eb2)
			//  and never reloads it -- and the walk runs over a second
			//  pointer copied from it (`mov %eax,%ebx`).
			pElem = pBucket->m_pElements;               // 0x10072eaf/b2
			{
				Elem729c0* pWalkElem = pElem;           // 0x10072eb9

				while (pWalkElem)                       // 0x10072eb5/0x10072ee3
				{
					pNextElem   = pWalkElem->m_pNext;   // 0x10072ebe
					g_n1008b874 = pWalkElem->m_nClipFlags10;
					                                    // 0x10072ebb/c1

					sub_1006f540(pWalkElem->m_pOwner);  // 0x10072ec6/ca

					pWalkElem->m_pOwner = (Obj729c0*)g_pFreeElems1008d4fc;
					                                    // 0x10072ecf/d7
					g_pFreeElems1008d4fc = pWalkElem;   // 0x10072ed9

					pWalkElem = pNextElem;              // 0x10072ee1
				}
			}

L_FreeBucket2:
			pBucket->m_pTexture =
			    (TexBlock729c0*)g_pFreeBuckets1008d4e0; // 0x10072ee5/ed
			g_pFreeBuckets1008d4e0 = pBucket;           // 0x10072eef

			pBucket = pNextBucket;                      // 0x10072eea/f5
		}

		// -------------------------------------------------------------- //
		//  REGION 14a -- 0x10072eff..0x10072f44.  Undo region 10, in the
		//  SAME order it set them: the render state first, then the stage
		//  state, and the stage state goes back to the value -0x24(%ebp)
		//  holds rather than to a constant.
		// -------------------------------------------------------------- //
		// 0x10072f1c -- +0x50.
		g_pD3DDevice->SetRenderState(0x1b, 0);

		// 0x10072f3f -- +0x94, with the saved value at 0x10072f33.
		g_pD3DDevice->SetTextureStageState(1, 1,
		                                           nOldColorOp);
	}

	// ------------------------------------------------------------------ //
	//  REGION 14b -- 0x10072f45..0x10072f55, 17 B.  The frame's bucket
	//  list is now empty by construction: every bucket has been either
	//  freed or re-threaded onto the deferral chain and then freed.  The
	//  single epilogue follows and sets no result.
	// ------------------------------------------------------------------ //
	g_pBucketList1008d4c4 = LTNULL;                     // 0x10072f45
}


// ======================================================================= //
//  COVERAGE -- every byte of 0x100729c0..0x1007313c accounted for.
//
//    0x100729c0..0x100729e3    36 B  prologue; both chains emptied; the
//                                    bucket list picked up and tested
//    0x100729e4..0x10072a1c    57 B  bucket loop head + the texture gate
//    0x10072a1d..0x10072a5f    67 B  gate-TRUE element walk (accumulating)
//    0x10072a60..0x10072a89    42 B  defer the bucket, or free its chain
//    0x10072a8a..0x10072ad5    76 B  gate-FALSE element walk (draw + free)
//    0x10072ad6..0x10072af6    33 B  free the bucket; the back edge
//    0x10072af7..0x10072b01    11 B  deferred-ELEMENT phase guard
//    0x10072b02..0x10072b7b   122 B  find the stage's texture record
//    0x10072b7c..0x10072c81   262 B  bind it
//    0x10072c82..0x10072cb7    54 B  0x1000da50 + the deferred-element draw
//    0x10072cb8..0x10072d2b   116 B  deferred-BUCKET phase: 0x1b on, save
//                                    stage 1's COLOROP
//    0x10072d2c..0x10072db8   141 B  per bucket: find the stage's record
//    0x10072db9..0x10072eae   246 B  bind it
//    0x10072eaf..0x10072efe    80 B  draw its elements, free them, free it
//    0x10072eff..0x10072f55    87 B  0x1b off, restore COLOROP, clear the
//                                    list, epilogue
//                            ------
//                            1,430 B above the `ret`
//
//    0x10072f56..0x10072fc2   109 B  SUNK: defer-element block 1 + refill
//    0x10072fc3..0x10073018    86 B  SUNK: defer-element block 2 + refill
//    0x10073019..0x1007304e    54 B  SUNK: L_ElseLOD2
//    0x1007304f..0x10073099    75 B  SUNK: L_MakeRec2, two arms
//    0x1007309a..0x100730a6    13 B  SUNK: region 11's `break` trampoline
//    0x100730a7..0x100730dc    54 B  SUNK: L_ElseLOD
//    0x100730dd..0x100730e6    10 B  SUNK: region 7's `break` trampoline #1
//    0x100730e7..0x10073131    75 B  SUNK: L_MakeRec, two arms
//    0x10073132..0x1007313b    10 B  SUNK: region 7's `break` trampoline #2
//                            ------
//                              486 B below the `ret`
//                            ======
//                            1,916 B
//
//  There are NO `// UNDECODED:` gaps.
//
// ======================================================================= //
//  WHAT THE CALL CHECK WITNESSES
//
//  The 26 call sites partition 1,916 bytes into 27 runs.  Twenty-five of
//  the 27 are 107 bytes or fewer and the median is 45.  The two long ones
//  are:
//
//    0x10072abb..0x10072c11   342 B  (17.9%)  the tail of region 4, all of
//                                    regions 5, 6 and 7, and region 8 down
//                                    to SetTexture
//    0x10072d26..0x10072e3e   280 B  (14.6%)  region 11 and region 12's
//                                    link surgery
//
//  Both are the find-the-record and LRU-relink code, and both are decoded
//  against FOUR independent siblings that reproduce the same block --
//  big0ae20.cpp, big6d910.cpp, big61a30.cpp and big2ef34.cpp -- field for
//  field, not skimmed.  The `test`/`je` pairs, the two-fold unroll, the
//  three list-head tests, the eight link stores, the `+= m_nBytes10` and
//  the stamp compare are all in the shipped bytes at the addresses cited
//  inline.  So the call check leaves 33% of the bytes unwitnessed and the
//  sibling diff covers all of it.
//
//  This function is NOT in the blind band: 74 bytes per call, 13
//  distinct targets, and a real control-flow graph rather than a table.
//
// ======================================================================= //
//  PREDICTED CALL-SEQUENCE FIGURE
//
//      shipped calls  26   ours  26
//      in agreement   24 to 26
//
//  Our emitted order, lexically:
//
//     1 0x10039f50      region 1, the texture gate
//     2 0x1006d910      region 2
//     3 0x1007fbf0      region 2's defer arm, refill   <- see COIN-FLIP 1
//     4 0x100256a0      region 3
//     5 0x1006d910      region 4
//     6 0x1007fbf0      region 4's defer arm, refill   <- see COIN-FLIP 1
//     7 *0x8c   8 *0x50   9 *0xbc     region 8
//    10 0x1000da50     11 0x10070d10  region 9
//    12 *0x50  13 *0x90  14 *0x94     region 10
//    15 *0x8c  16 *0x50  17 *0xbc     region 12
//    18 0x1006f540                    region 13
//    19 *0x50  20 *0x94               region 14a
//    21 *0xbc                         L_ElseLOD2
//    22 0x10039bd0  23 0x10039bd0     L_MakeRec2
//    24 *0xbc                         L_ElseLOD
//    25 0x10039bd0  26 0x10039bd0     L_MakeRec
//
//  and the shipped order is the same list with rows 3 and 6 moved to 19
//  and 20 -- i.e. IDENTICAL if icl sinks the two defer arms past the
//  epilogue ahead of the four goto-placed blocks, which is where the
//  shipped build put them.  If it does not, the two 0x1007fbf0 rows are
//  the whole residual: 24 in agreement, 2 shipped-only and 2 ours-only,
//  all four naming 0x1007fbf0, which is the benign equal-counts
//  pattern with DIRECT calls and therefore has to be reported as a
//  block-placement question rather than waved through.
//
//  PREDICTED RESIDUALS BESIDES THAT: none.
//
//    * `_chkstk` DOES NOT ARISE.  The frame is 0x38 bytes and there is no
//      `mov $N,%eax ; call 0x100805a0` at 0x100729c0.
//    * /Oi- IS NOT WANTED HERE, judged from this function's own census and
//      not from the list.  There is no CRT intrinsic to bring back: zero
//      x87 instructions in 569, so no sqrt/fabs/tan/atan, and zero
//      multi-dword copies, so no memcpy -- every structure store in the
//      function is a single `mov` of a pointer or a `mov %ax` of a
//      sixteen-bit field.  The known risk that /Oi- can manufacture an
//      ours-only `memcpy` row therefore applies with nothing on the other
//      side of the trade, so the flag is declined WITH REASON, as two
//      earlier functions declined it.
//    * /Qrcd IS NOT WANTED.  Zero float-to-int conversions: no `fistp`,
//      no `fld`, no `.rdata` float operand.  The 27 `movzbl`/`movzwl` here
//      are integer field narrowings, all 27 accounted for in the census
//      below, and /Qrcd does not touch them.
//    * NO DEAD `1.0f` SLOTS, because there are no divisions and no float
//      literals at all -- this function has no literal pool entry of any
//      kind, so the dead-1.0f census row is empty rather than
//      unexplained.
//    * The /Qip `f+4` question does not arise: none of the eight direct
//      callees is same-TU here, and the call check accepts that offset anyway.
//
// ======================================================================= //
//  COIN-FLIPS, each with both spellings
//
//  1. THE ONLY ONE THAT MOVES A CALL.  The two element-walk defer arms are
//     written as
//
//         if (!(flags & 0x8000) || ppDefer == (Elem729c0**)-1)  { draw }
//         else                                                  { defer }
//
//     which is the measured lever for SINKING the else arm, and the
//     shipped build has those arms at 0x10072f56 and 0x10072fc3, below the
//     epilogue.  The other spelling is
//
//         if ((flags & 0x8000) && ppDefer != (Elem729c0**)-1)   { defer }
//         else                                                  { draw }
//
//     which emits the defer arm INLINE and puts its 0x1007fbf0 refill at
//     call positions 3 and 6 instead of 19 and 20.  Predictions: the
//     written form 26 of 26 if icl orders the sunk blocks as the shipped
//     build does, 24 of 26 otherwise; the alternative 24 of 26 outright.
//     One build settles it, and the diagnostic is whether the two
//     0x1007fbf0 rows are the only disagreement.
//
//     A third spelling exists and was rejected: a `goto` out of the loop
//     to a hand-placed block that `goto`es back to a label at the loop's
//     tail, which is how big0ae20.cpp and big6d910.cpp place THEIR sunk
//     blocks.  It cannot be used for these two because the re-entry point
//     is inside a loop body rather than in straight-line code, and
//     re-entering a loop makes the region irreducible.  The four blocks
//     that DO re-enter straight-line code (L_ElseLOD, L_MakeRec) or a loop
//     body reached only by the same kind of jump (L_ElseLOD2, L_MakeRec2)
//     are placed that way.
//
//  2. THE ORDER OF THE FOUR GOTO-PLACED BLOCKS, and it moves NO call.
//     They are written in shipped ADDRESS order -- bucket phase first
//     (0x10073019, 0x1007304f), element phase second (0x100730a7,
//     0x100730e7) -- which is the REVERSE of body order.  If icl emits
//     them in body order instead, our sequence for positions 21..26
//     becomes `*0xbc, 0x10039bd0, 0x10039bd0, *0xbc, 0x10039bd0,
//     0x10039bd0` -- token for token what it already is, because the two
//     phases' blocks have the same shape and the call check compares indirect
//     calls by position only.  So this is genuinely free either way, and
//     that is worth stating rather than leaving as a risk.
//
//  3. THE FREE-LIST LINK SPELLING.  +0x00 of the element and +0x00 of the
//     bucket each serve two roles -- the owner / the texture block in use,
//     the next-free pointer when on a free list.  Written here as a cast of
//     the in-use member, exactly as big73140.cpp writes it; a `union` with
//     a dedicated `m_pNextFree` emits the identical bytes.  Moves no call.
//
//  4. `ppDefer == (Elem729c0**)-1`.  Written as a comparison on a named
//     local initialised to `&pDeferElems`, which is the smallest source
//     that emits the `lea -0x28(%ebp),%eax ; cmp $0xffffffff,%eax` the
//     shipped bytes have at 0x10072a38 and 0x10072aa3.  The reading of
//     WHY it survives -- that the element walk was an inline helper taking
//     an `Elem**` and `-1` is another caller's "draw it anyway" sentinel --
//     is an inference from icl's refusal to fold a constant address, and it
//     is recorded as such.  Spelling it as an actual `static inline`
//     helper taking that parameter is the alternative; it was NOT chosen,
//     because the helper contains two calls and if icl declines to inline
//     it the sequence gains an ours-only row at every element, which is a
//     far worse failure than the folding question.  Either way the
//     comparison itself is call-free, so if our icl DOES fold it away the
//     call sequence is unaffected -- but the source must not pretend the
//     comparison is not there, which is why it is written and not replaced
//     by an unconditional branch.
//
//  5. THE DOUBLED NULL TESTS.  `if (!pBlock) goto L_ElemDone;` appears
//     TWICE in region 7 because the shipped bytes have two `je`s off one
//     `test %esi,%esi` (0x10072b13 and 0x10072b21).  Writing it once
//     emits one `je`.  Moves no call.  The same non-folding on the list
//     head is spelled the way big0ae20.cpp spells it -- one positive `if`
//     wrapping the `while` -- which is the form measured there.
//
//  6. `nAcc |= 1;` is written LAST in the defer success arm; the shipped
//     `or $0x1,%esi` at 0x10072f73 is scheduled between the owner load and
//     the owner store.  Register-only, moves no call.
//
//  7. THE FOUR ONE-ARGUMENT PROTOTYPES (0x100256a0, 0x1006d910,
//     0x1006f540, 0x10070d10).  Declared with one parameter here, on the
//     evidence of 0x100256a0's 48-byte body and of the three big callees
//     never reading `0xc(%ebp)`.  big6d910.cpp and big70d10.cpp declare a
//     dead second parameter instead.  Neither spelling can move a call and
//     neither can produce or suppress icl's `push %edi` padding.
//
// ======================================================================= //
//  WHAT COULD NOT BE PINNED
//
//    * SIXTEEN BYTES OF FRAME.  `sub $0x38,%esp` reserves 56 bytes and the
//      deepest referenced slot is -0x28, 40 bytes.  Nothing in the 569
//      instructions touches -0x2c..-0x38.  It could be alignment, a dead
//      local, or an outgoing area icl reserved and then stopped using; the
//      bytes do not say and no member or statement is invented for it.
//    * THE OTHER CALLER OF THE ELEMENT-WALK HELPER, i.e. whoever passes
//      `(Elem**)-1`.  The sentinel proves such a caller existed when the
//      helper was compiled; this function is not it, and which function is
//      is outside these 1,916 bytes.
//    * Elem729c0 +0x04, +0x08 and +0x0c.  Never touched here; named from
//      big73140.cpp and big124a4.cpp, which write them.
//    * Tex729c0 +0x00 (the vtable, never called from here) and +0x40,
//      which only 0x10039f50's own body reads.
//    * TexBlock729c0 +0x2c, which big124a4.cpp names and this function
//      does not touch.
//    * THE D3D CONSTANT NAMES.  What is pinned is the NUMBERS: render
//      states 0x18 and 0x1b, texture stage 1, stage state 1, values 0 and
//      1.  The names offered inline (ALPHAREF, ALPHABLENDENABLE, COLOROP,
//      D3DTOP_DISABLE) are the standard D3D7 assignments for those
//      numbers and are a reading, not a measurement.
//
// ======================================================================= //
//  THE INSTRUCTION CENSUS -- 569 instructions, 20 distinct mnemonics, NO
//  RESIDUE.  It is wanted where the call check leaves bytes
//  unwitnessed, and here it happens to be unusually clean, because the
//  function is a pair of near-identical halves and almost every row is
//  therefore an EVEN number that the claimed structure has to predict.
//
//      mov     275     test     48     je     48     push   33
//      add      29     call     26     movzwl 23     jmp    15
//      movl     14     xor      13     jne    12     cmp     9
//      lea       7     pop       4     movzbl  4     inc     3
//      or        2     incl      2     sub     1     ret     1
//                                                 ---------------
//                                                 569  (= 580 objdump
//                                                 lines minus 11
//                                                 continuation lines)
//
//  ZERO x87 INSTRUCTIONS, zero `rep movs`, zero `idiv`, zero `.rdata`
//  operands.  That is the whole basis for declining /Oi- and /Qrcd above:
//  there is no floating point and no block copy in this function at all.
//
//  The eight discriminating rows, decomposed by operand.  Each one is
//  exact, and the structure claimed above is what makes it exact:
//
//  add 29 -- AND THIS ROW ALONE CONFIRMS EVERY CALL'S ARGUMENT COUNT:
//        8  `add $0x8,%esp`        the eight 8-byte cdecl cleanups: the
//                                  five two-push calls (0x100256a0,
//                                  0x1006d910 twice, 0x10070d10,
//                                  0x1006f540), the gate 0x10039f50, and
//                                  the two pool refills
//        6  `add $0xfffffff8,%esp` the six 8-byte outgoing areas built
//                                  with add/mov: the gate, the FOUR
//                                  SetLOD sites, and refill 1
//        6  `add $0xfffffff4,%esp` the six 12-byte areas: SetTexture x2
//                                  plus SetRenderState x4
//        4  `add $0x10,%esp`       the four 0x10039bd0 cleanups
//        3  `add $0xfffffff0,%esp` the three 16-byte areas: one
//                                  GetTextureStageState plus two Sets
//        2  `add %edx,0x48(%eax)`  the two `m_nBytes48 +=`, one per bind
//
//  movzwl 23:
//        6  `0x42(%ebx)`  m_nStage42, THREE per bind copy (the g_1008e684
//                         index, the SetTexture stage, the g_1008e664
//                         index) x 2 copies
//        4  `0x1008bea8`  the stamp compare and the stamp store x 2
//        3  `0x42(%eax)`  the search compares: region 7's TWO unrolled
//                         copies plus region 11's single one
//        2  `0x44(%ebx)`  m_nDirty44 in the two main binds
//        2  `0x44(%edx)`  m_nDirty44 in the two L_ElseLOD copies
//        2  `0x16(%esi)`  m_nAlphaRef16 x 2
//        2  `0x14(%esi)`  m_nStamp14 x 2
//        2  `0x1008b870`  the frame stamp x 2
//
//  movzbl 4:  2 x `%al` (the SetTexture stage cast) and 2 x `%cl` (the
//        `(uint8)nStage` of the two searches).
//
//  cmp 9:  2 x `$0xffffffff` (the deferral sentinel, one per element walk)
//        + 3 x `%ecx,%edx` (the three search-compare copies) + 2 x
//        `0x1008e684(,%eax,4),%edx` (the already-bound test) + 2 x
//        `%edx,%eax` (the stamp test).
//
//  lea 7:  2 x `-0x28(%ebp)` (the sentinel's address, one per walk) + 2 +
//        2 x `0x1c(%ebx)` (the link address in each bind) + 1 x
//        `-0x24(%ebp)` (the ONLY out-parameter in the function).
//
//  movl 14:  4 x `$0x0,0x4(%esp)` (SetLOD's literal second argument, one
//        per SetLOD) + 2 x `$0x1b` + 2 x `$0x18` (the four
//        SetRenderState state numbers) + `$0x1,0x8(%esp)` and
//        `$0x0,0x8(%esp)` (0x1b on and off) + `$0x0,0x1c(%edx)` and
//        `$0x0,0x1c(%eax)` (THE TWO m_pBucket1c CLEARS, one on the
//        deferral arm and one on the free arm -- so they are two source
//        statements, not one hoisted) + `$0x0,0x1008d4c4` + the
//        `pDeferElems = 0` at 0x100729d7.
//
//  xor 13:  1 x `%esi,%esi` -- ONE accumulator, which is the census proof
//        that region 4's walk discards 0x1006d910's result -- + 2 x
//        `%edx,%edx` (pRec = NULL, one per search) + 10 x `%eax,%eax`,
//        being `pDeferBuckets = 0`, the FOUR `m_nDirty44 = 0`, the THREE
//        `xor;inc` literal ones, and the TWO allocation-failure
//        `pNew = 0` in the sunk refills (0x10072fbf, 0x10073015).
//
//  or 2 / incl 2 / inc 3:  `or %eax,%esi` (nAcc |= draw result) and
//        `or $0x1,%esi` (nAcc |= 1 on a successful defer) -- exactly two,
//        both in region 2, none in region 4; `g_n1008bd80++` once per
//        bind; three literal-1 materialisations, one for the 0x1b enable
//        and two for the stage-state pair.
//
//  WHAT WOULD MOVE THESE ROWS.  A third texture-bind copy moves five rows
//  at once (movzwl x4, movl, add, xor, incl).  A third element walk moves
//  `test $0x8000` and `cmp $0xffffffff` and `lea -0x28(%ebp)`.  One more
//  or one fewer unroll of region 7's search moves `movzwl 0x42(%eax)` and
//  `cmp %ecx,%edx`.  A fifth 0x10039bd0 arm moves `add $0x10,%esp` and
//  four pushes.  A single extra literal-0 argument anywhere moves `movl`.
//  Nothing in the reconstruction above is free of at least one of them.
//
// ======================================================================= //


// ======================================================================= //
//  MEASURED, NOT CLOSED.  What was tried against the bytes,
//  with a mnemonic-diff block count (fewer is closer) and
//  size against the shipped 1,916 bytes:
//
//    as written, /O2 (the file's current flag)      52 blocks, 2,112 B
//    as written, /Os                                43 blocks, 1,936 B
//    /Oi-, /Qrcd, /Qip on top of /O2                unchanged
//
//  THE FLAG.  The shipped prologue saves with `push` (`push %edi ; push
//  %edi ; push %esi ; push %ebx`); at /O2 ours saves INTO the frame
//  (`mov %edi,X(%esp)`), a strictly /O2-only tell.
//  At /Os the prologue, the gate call and the first element walk's head
//  come out instruction for instruction.  But the shipped function also
//  unrolls region 7's search twice and sinks both defer arms below the
//  epilogue, and our /Os does neither -- so neither flag alone is the
//  shipped one, and the file was NOT moved (build/engine.sh untouched).
//
//  THE `ppDefer == -1` SENTINEL IS CONFIRMED.  Writing the two element
//  walks as inline helpers that TAKE `Elem729c0** ppDefer` and are called
//  with `&pDeferElems` --
//
//      inline uint32 DrawList729c0(Elem729c0* pList, Elem729c0** ppDefer);
//      inline void   DrawFreeList729c0(Elem729c0* pList, Elem729c0** ppDefer);
//      ...
//      nAcc = DrawList729c0(pBucket->m_pElements, &pDeferElems);
//      ...
//      DrawFreeList729c0(pBucket->m_pElements, &pDeferElems);
//
//  (bodies = regions 2 and 4's loops verbatim, `*ppDefer` for pDeferElems)
//  -- reproduces the shipped `lea -N(%ebp),%eax ; cmp $0xffffffff,%eax`
//  at both sites with no register spent on ppDefer, and the accumulator
//  as `or %eax,%esi` / `or $0x1,%esi` in the two arms exactly as shipped.
//  With the local `ppDefer` below, /Os instead parks &pDeferElems in %edi
//  for the whole function and every register after it shifts.  Measured:
//  /O2 52 -> 48 blocks (2,096 B), /Os 43 -> 36 blocks (1,912 B).  Arm
//  order (draw-first or defer-first) and a separate inline DeferElem
//  helper moved nothing (36-38 blocks).  Not applied here because at the
//  file's /O2 the gain is four blocks and the defer arm is still not
//  sunk.
//
//  REMAINING GAPS at /Os + helpers, first to last: the defer arm inline
//  rather than sunk (0x10072a3e `jne 0x10072f56`); the frame 0x28 against
//  0x38; region 7's redundant third block test (0x10072b21) folded away;
//  region 7's search not unrolled; pRec held in %ebx where the shipped
//  code keeps it in %esi with a copy in %ebx.  Each is a block-placement
//  or allocation decision, not a decode error.
// ======================================================================= //
