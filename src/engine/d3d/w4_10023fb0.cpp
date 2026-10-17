// *********************************************************************** //
//
//  MODULE:   W4_10023FB0 (.CPP)  -- /O2
//
//  PURPOSE:  Functions the d3d.ren link referenced by address and nobody
//            had written, 0x10023fb0..0x1002e350: the vertex pool's grow, the element free-list
//            returns, and three /Qip callers whose callees live in files
//            built without /Qip (near by construction).
//
//  Flag set: /O2 -- each body is followed by multi-byte lea-nops to the
//  next 16-byte boundary and its loop heads are aligned the same way.
//
// *********************************************************************** //

#include "ltbasedefs.h"

void* sub_10021c20(uint32 nSize);                       // 0x10021c20 alloc
void  sub_10021cb0(void* p);                            // 0x10021cb0 free


// ======================================================================= //
//  0x10023fb0 -- grow the 32-byte-vertex pool at 0x1008d524 to nWanted
//  entries: allocate, copy the g_1008d51c entries it held (an inline
//  dword copy), free the old block, and keep the new one.  Answers FALSE
//  if the allocation fails.  C linkage (bigb910.cpp calls `_sub_10023fb0`
//  with a second argument this body never reads).
// ======================================================================= //
struct PoolVert { uint32 m_d[8]; };                     // 0x20

extern uint32    g_1008d51c;                            // 0x1008d51c  capacity
extern PoolVert* g_1008d524;                            // 0x1008d524  the pool

extern "C" int32 sub_10023fb0(uint32 nWanted)
{
	PoolVert* pNew;

	pNew = (PoolVert*)sub_10021c20(nWanted * sizeof(PoolVert));
	if (!pNew)
		return 0;

	memcpy(pNew, g_1008d524, g_1008d51c * sizeof(PoolVert));
	if (g_1008d524)
		sub_10021cb0(g_1008d524);

	g_1008d524 = pNew;
	g_1008d51c = nWanted;
	return 1;
}


// ======================================================================= //
//  The element free list at 0x1008d4fc: +0x00 is the element's object
//  while it is in use and the free-list link once it is returned; in-use
//  elements are chained through +0x14.
// ======================================================================= //
struct Obj28030;

struct Elem24024
{
	union
	{
		Obj28030*   m_pObj;                             // +0x00 in use
		Elem24024*  m_pNextFree;                        // +0x00 free
	};
	uint32      m_n04;                                  // +0x04
	uint32      m_n08;                                  // +0x08
	uint32      m_n0c;                                  // +0x0c
	uint32      m_nClipFlags;                           // +0x10
	Elem24024*  m_pNext;                                // +0x14
};

extern Elem24024* g_1008d4fc;                           // 0x1008d4fc  free list
extern Elem24024* g_1008d4bc;                           // 0x1008d4bc  pending


// ======================================================================= //
//  0x100256a0 -- return a whole in-use chain to the free list.
// ======================================================================= //
extern "C" void sub_100256a0(Elem24024* pChain)
{
	Elem24024* pNext;

	while (pChain)
	{
		pNext = pChain->m_pNext;
		pChain->m_pNextFree = g_1008d4fc;
		g_1008d4fc = pChain;
		pChain = pNext;
	}
}


// ======================================================================= //
//  0x10025820 -- MOVED to w38_10028030_qip.cpp: its call enters
//  0x1002803c's /Qip register entry, which only 0x10028030's own /Qip TU
//  emits.  Identical there.
// ======================================================================= //


// ======================================================================= //
//  0x1002e350 now lives in w28_100256d0_qipni.cpp, with the box drawer
//  whose /Qip register entry it calls.
// ======================================================================= //


// ======================================================================= //
//  0x1002b0c0 now lives in w28_100256d0_qipni.cpp (was big26728.cpp),
//  with the function it calls at its /Qip register entry (0x10026728).
// ======================================================================= //


// ======================================================================= //
//  0x10023230 -- transform a point by a 4x4 and divide through by w,
//  answering 1/w: ltmatrix.h's inline MatVMul_H, the out-of-line copy
//  (`?MatVMul_H@@YAMPAV?$_CVector@M@@PAVLTMatrix@@0@Z`) that
//  w30_1000f020_osqipni.cpp emits.  It was ALSO
//  written here, as `extern "C" sub_10023230`, a hand spelling for the two
//  callers that name the address (big2b1b0.cpp, big40e10.cpp; the shipped
//  calls are 0x1002b1d4, 0x1002b206, 0x10040eda, 0x10040f63): one shipped
//  function, two of ours, and this one was left unplaced.  It is gone; the
//  callers' spelling is aliased onto the header's function in
//  link/aliases.rsp.  Measured at the shipped address: the header's own
//  body as w30 builds it is 12 bytes off, this spelling was 16 (its notes:
//  every one of 144 spellings of the z row stays last-first, the order
//  moving with the functions compiled ahead of it -- icl's sum order
//  follows TU history, as at 0x1005a550, w7_1005a550.cpp).
// ======================================================================= //


// ======================================================================= //
//  0x10029890 / 0x1002cf40 (the /Qip pairs whose register entries are
//  0x10029894 / 0x1002cf48) were ALSO written here, as a second body of
//  each.  They now live only in d3dqip2.cpp, the translation unit the
//  shipped build put them in (0x10029890 reaches 0x10026480 through its
//  register entry 0x10026484, so it shares that file), whose register-ABI
//  callees are no longer stand-ins but the real declarations.  One body
//  per shipped function: the old spellings here -- sub_10029894(SceneDesc*)
//  and sub_1002cf48(Obj1002cf40*, uint32), still referenced by
//  big2d1c0.cpp / big26728.cpp -- are aliased onto d3dqip2.cpp's in
//  link/aliases.rsp.
// ======================================================================= //
