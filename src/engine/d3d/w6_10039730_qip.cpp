// *********************************************************************** //
//
//  MODULE:   W6_10039730_QIP (.CPP)  --  the RTexture free/bind cluster
//
//  0x10039730..0x10039f82 of the retail d3d.ren, written for the d3d-link.
//  Built /O2 /Qip (the _qip suffix): the shipped bytes are one icl /Qip
//  translation unit --
//
//    * 0x10039850 is a REGISTER-ONLY entry (%eax = texture, %edx = force
//      flag, no stack shim in front of it: the byte before is padding), so
//      it had internal linkage.  It is recursive, and /Qip inlined ONE
//      level of it into each of its callers here (0x10039730, 0x10039930,
//      0x10039bdc) while the recursion stays a call -- exactly what the
//      shipped callers show: the whole body inline, then
//      `mov $0x1,%edx ; call 0x10039850` for the children.
//    * 0x10039bd0 is /Qip's EXTERNAL two-entry pair: the twelve-byte
//      `mov 0x4/0x8/0xc(%esp),%eax/%edx/%ecx` stack shim falling into the
//      register entry 0x10039bdc, which 0x10039f50 (and 0x10039f90 and
//      0x10039fe0, d3dmid.cpp's) call with %eax/%edx/%ecx loaded.
//    * every function here pads to 16 with lea-nops: /O2.
//
//  WHAT IS NOT REACHABLE HERE, and why (near, not identical):
//    0x10039bdc calls 0x10038a90 (r_CreateRTexture) and 0x1003a080
//    (r_TransferTexture) through their REGISTER entries.  Both are
//    statics of the shipped TU (no shim in front of either).
//    Both are now written at the end of this file as statics
//    (r_TransferTexture was big3a080.cpp, r_CreateRTexture
//    big38a90.cpp), which made 0x10039fe0 and 0x10039bdc identical.
//    0x10038a90's own callees 0x100385e0 and 0x10038630 are register-only
//    statics of the same shipped TU too, still in w6_100385e0.cpp as
//    ordinary functions (so 0x10038a90 remains a near).
//
//  THE RECORD.  big38a90.cpp's RTexture (0x4c bytes) -- every offset this
//  file touches was named there first; see that file's EVIDENCE note.
// *********************************************************************** //

#include "ltbasedefs.h"

#include <string.h>


// ----------------------------------------------------------------------- //
//  COM surface: slot 0x08 Release, 0x64 Lock, 0x80 Unlock
//  (IDirectDrawSurface7's vtable).  All __stdcall.
// ----------------------------------------------------------------------- //
struct DDSD2;

struct DDSurface7C;
struct DDSurface7Vtbl
{
	void*	m_pad_00[2];                                    // +0x00..+0x04
	long	(__stdcall* Release)(DDSurface7C*);             // +0x08
};
struct DDSurface7C { DDSurface7Vtbl* lpVtbl; };

// Written as a C++ interface: the shipped Release reloads the pointer once
// after its null test and uses it for BOTH the vtable and `this`
// (`mov 0xc(%esi),%eax ; mov (%eax),%edx ; push %eax ; call *0x8(%edx)`),
// which is `p->Release()`; the C spelling `p->lpVtbl->Release(p)` reads it
// twice.
struct DDSurface7
{
	virtual long __stdcall QueryInterface(void* riid, void** ppObj) = 0;
	virtual unsigned long __stdcall AddRef() = 0;
	virtual unsigned long __stdcall Release() = 0;           // +0x08
	virtual void __stdcall v03() = 0;
	virtual void __stdcall v04() = 0;
	virtual void __stdcall v05() = 0;
	virtual void __stdcall v06() = 0;
	virtual void __stdcall v07() = 0;
	virtual void __stdcall v08() = 0;
	virtual void __stdcall v09() = 0;
	virtual void __stdcall v10() = 0;
	virtual void __stdcall v11() = 0;
	virtual void __stdcall v12() = 0;
	virtual void __stdcall v13() = 0;
	virtual void __stdcall v14() = 0;
	virtual void __stdcall v15() = 0;
	virtual void __stdcall v16() = 0;
	virtual void __stdcall v17() = 0;
	virtual void __stdcall v18() = 0;
	virtual void __stdcall v19() = 0;
	virtual void __stdcall v20() = 0;
	virtual void __stdcall v21() = 0;
	virtual void __stdcall v22() = 0;
	virtual void __stdcall v23() = 0;
	virtual void __stdcall v24() = 0;
	virtual long __stdcall Lock(void* pRect, DDSD2* pDesc, uint32 nFlags,
	                            void* hEvent) = 0;          // +0x64
	virtual void __stdcall v26() = 0;
	virtual void __stdcall v27() = 0;
	virtual void __stdcall v28() = 0;
	virtual void __stdcall v29() = 0;
	virtual void __stdcall v30() = 0;
	virtual void __stdcall v31() = 0;
	virtual long __stdcall Unlock(void* pRect) = 0;         // +0x80
};

// DDSURFACEDESC2, 0x7c bytes; only the fields read here are named.
struct DDSD2
{
	uint32	dwSize;                                         // +0x00
	uint32	dwFlags;                                        // +0x04
	uint32	dwHeight;                                       // +0x08
	uint32	dwWidth;                                        // +0x0c
	int32	lPitch;                                         // +0x10
	uint32	m_pad_14[4];                                    // +0x14..+0x23
	void*	lpSurface;                                      // +0x24
	uint8	m_pad_28[0x7c - 0x28];                          // +0x28..+0x7b
};

#define W6_DDLOCK_WAIT          0x00000001
#define W6_DDLOCK_NOSYSLOCK     0x00000800


// ----------------------------------------------------------------------- //
//  The engine's shared texture: +0x0c is the renderer's RTexture for it.
// ----------------------------------------------------------------------- //
struct RTexture;

struct SharedTexture
{
	uint8		m_Reserved00[0x0c];                         // +0x00..+0x0b
	RTexture*	m_pRTexture0c;                              // +0x0c
};

class TextureData;

// big38a90.cpp's three-dword request, built on 0x10039bdc's stack.
struct TexRequest
{
	SharedTexture*	m_pShared00;                            // +0x00
	TextureData*	m_pData04;                              // +0x04
	uint32			m_nStage08;                             // +0x08
};


// ----------------------------------------------------------------------- //
//  RTexture, big38a90.cpp's layout.  The link at +0x1c is ltlink.h's
//  LTLink (m_pPrev, m_pNext, m_pData).
// ----------------------------------------------------------------------- //
struct RTexture
{
	union
	{
		void*		m_pVTable;                              // +0x00
		RTexture*	m_pNextFree;                            // +0x00
	};
	float			m_fInvW04;                              // +0x04
	float			m_fInvH08;                              // +0x08
	DDSurface7*		m_pSurface0c;                           // +0x0c
	uint32			m_nBytes10;                             // +0x10
	uint16			m_nStamp14;                             // +0x14
	uint16			m_nAlphaRef16;                          // +0x16
	RTexture*		m_pSelf18;                              // +0x18
	LTLink			m_Link1c;                               // +0x1c..+0x27
	uint16			m_nWidth28;                             // +0x28
	uint16			m_nHeight2a;                            // +0x2a
	SharedTexture*	m_pShared2c;                            // +0x2c
	RTexture*		m_pNext30;                              // +0x30
	float			m_fScale34;                             // +0x34
	float			m_fCos38;                               // +0x38
	float			m_fSin3c;                               // +0x3c
	uint16			m_n40;                                  // +0x40
	uint16			m_nStage42;                             // +0x42
	uint16			m_nDirty44;                             // +0x44
	uint8			m_nFirstMip46;                          // +0x46
	uint8			m_nMipCount47;                          // +0x47
	uint8			m_nFormatIndex48;                       // +0x48
	uint8			m_nFlags49;                             // +0x49
	uint16			m_pad4a;                                // +0x4a

	// 0x1003abe4 -- d3dbatchd.cpp's C1003abe4::C1003abe4, reached from
	// 0x10038a90 as `mov %esi,%ecx ; call 0x1003abe4` with nothing pushed.
	void			sub_1003abe4();                         // 0x1003abe4
};                                                          // 0x4c

// Bit 0 of +0x49: the record is not freed by a plain (unforced) free,
// and its +0x30 chain is left alone.
#define RTEX_FLAG_KEEP          0x01


// ----------------------------------------------------------------------- //
//  The record bank at 0x1008e644 (big38a90.cpp's RTexBank): its free head
//  is +0x18, 0x1008e65c.  The free below is inlined in the shipped code as
//  `mov 0x1008e65c,%eax ; mov %eax,(%esi) ; mov %esi,0x1008e65c`.
// ----------------------------------------------------------------------- //
struct RTexBank
{
	uint32		m_n00;                                      // +0x00
	uint32		m_nElemSize04;                              // +0x04
	uint32		m_nGrowCount08;                             // +0x08
	uint8		m_Reserved0c[0x0c];                         // +0x0c..+0x17
	RTexture*	m_pFree18;                                  // +0x18

	void Free(RTexture* pNode)
	{
		pNode->m_pNextFree = m_pFree18;
		m_pFree18 = pNode;
	}
	RTexture* Alloc();                                      // big38a90.cpp's, below
};

extern RTexBank g_1008e644;                                 // 0x1008e644


// ----------------------------------------------------------------------- //
//  The engine's RenderStruct at 0x1008cbcc (d3d.cpp's map names the
//  slots): +0x08 GetTextureData, +0x0c FreeTextureData, +0x50 the texture
//  byte counter big38a90.cpp adds each record's +0x10 into.
// ----------------------------------------------------------------------- //
struct RenderStats
{
	void*			m_pad_00[2];                            // +0x00..+0x07
	TextureData*	(*GetTextureData)(SharedTexture* pShared,
	                                  uint32* pOut);        // +0x08
	void			(*FreeTextureData)(SharedTexture* pShared); // +0x0c
	uint8			m_pad_10[0x14 - 0x10];                  // +0x10..+0x13
	void			(*ConsolePrintf)(const char* pszFmt, ...);  // +0x14
	uint8			m_pad_18[0x50 - 0x18];                  // +0x18..+0x4f
	uint32			m_nTextureBytes50;                      // +0x50
};
extern RenderStats* g_p1008cbcc;                            // 0x1008cbcc


// ----------------------------------------------------------------------- //
//  The LRU list head (an LTLink object, big38a90.cpp), and the two
//  per-stage "current texture" slots at 0x1008e684/0x1008e688
//  (big0ae20.cpp's g_a1008e684[nStage]).
// ----------------------------------------------------------------------- //
struct RTexLink : public LTLink
{
	// ltlink.h's CheapLTLink::AddAfter, verbatim (big38a90.cpp's note:
	// prev, then next, then the double assignment evaluated right to left).
	void AddAfter(LTLink* pLink)
	{
		pLink->m_pPrev = this;
		pLink->m_pNext = m_pNext;
		pLink->m_pPrev->m_pNext = pLink->m_pNext->m_pPrev = pLink;
	}
};
extern RTexLink g_1008e634;                                 // 0x1008e634
extern void*    g_1008e684[];                               // 0x1008e684

// The base vtable the record's (empty, inlined) destructor stores.
extern void*    g_vtbl_100894d0[];                          // 0x100894d0

extern uint32   g_1008d7cc;                                 // 0x1008d7cc, stage

// 0x10091950 -- 0x10039a28 calls 0x1006d3e0 with `mov $0x10091950,%ecx`
// and nothing pushed: a __thiscall no-argument member on the object at
// 0x10091950 (whose +0x04 and +0x18 are the two arrays d3dr3a.cpp's
// sub_1006d3e0 walks).
struct C10091950
{
	void	sub_1006d3e0();                                 // 0x1006d3e0
};
extern C10091950 g_10091950;                                // 0x10091950


// ----------------------------------------------------------------------- //
//  Callees in other files.
// ----------------------------------------------------------------------- //
static RTexture* sub_10038a90(TexRequest* pReq, uint32 nFlags);  // r_CreateRTexture, below
static long r_TransferTexture(RTexture* pTex, TextureData* pData); // 0x1003a080, below
void      sub_10021d60(uint32 nLevel, const char* pszMsg, ...);  // dlog


// ======================================================================= //
//  0x10039850 -- 224 bytes.  r_FreeTexture(pTex, bForce), REGISTER ENTRY.
//
//      mov %eax,%esi                     pTex
//      testb $0x1,0x49(%esi) ; je kids   flag clear: free the +0x30 chain
//      test %edx,%edx ; jne tail         flag set: only when forced
//      <return>
//   kids: for (p = m_pNext30; p; ) { next = p->m_pNext30;
//                                    0x10039850(p, 1); p = next; }
//   tail: m_pShared2c->m_pRTexture0c = 0, m_pShared2c = 0
//         g_pStruct->+0x50 -= m_nBytes10
//         clear either per-stage slot that holds it
//         Release m_pSurface0c
//         if the LRU link is in a list (m_pData) unlink and tie it off
//         (ltlink.h's Remove, stores +0x20 before +0x1c)
//         the destructor's vptr store (0x100894d0), then the bank free.
// ======================================================================= //
static void sub_10039850(RTexture* pTex, uint32 bForce)
{
	RTexture* pChild;
	RTexture* pNext;

	if (pTex->m_nFlags49 & RTEX_FLAG_KEEP)
	{
		if (!bForce)
			return;
	}
	else
	{
		pChild = pTex->m_pNext30;
		while (pChild)
		{
			pNext = pChild->m_pNext30;
			sub_10039850(pChild, 1);
			pChild = pNext;
		}
	}

	if (pTex->m_pShared2c)
	{
		pTex->m_pShared2c->m_pRTexture0c = LTNULL;
		pTex->m_pShared2c = LTNULL;
	}

	g_p1008cbcc->m_nTextureBytes50 -= pTex->m_nBytes10;

	if (pTex == g_1008e684[0])
		g_1008e684[0] = LTNULL;

	if (pTex == g_1008e684[1])
		g_1008e684[1] = LTNULL;

	if (pTex->m_pSurface0c)
	{
		// d3dbatcha.cpp's measured spelling of the shipped RELOAD after
		// the null test (see its sub_100210d0 banner): one volatile read.
		DDSurface7C*    pSurf = *(DDSurface7C* volatile*)&pTex->m_pSurface0c;
		DDSurface7Vtbl* pVtbl = pSurf->lpVtbl;
		pVtbl->Release(pSurf);
	}

	if (pTex->m_Link1c.m_pData)
		dl_Remove(&pTex->m_Link1c);

	pTex->m_pVTable = g_vtbl_100894d0;
	g_1008e644.Free(pTex);
}


// ======================================================================= //
//  0x10039730 -- 287 bytes.  Free every texture on one list, then tie the
//  head off.  __cdecl (LTLink*), d3dr3a.cpp's sub_1006d3e0 calls it for
//  each of its five lists.  0x10039850's first level inlined.
//  The tie-off stores +0x04 then +0x00: `m_pPrev = m_pNext = head`.
// ======================================================================= //
void sub_10039730(LTLink* pList)
{
	LTLink*   pCur;
	LTLink*   pNext;
	RTexture* pTex;

	pCur = pList->m_pNext;
	while (pCur != pList)
	{
		pTex  = (RTexture*)pCur->m_pData;
		pNext = pCur->m_pNext;
		sub_10039850(pTex, 0);
		pCur = pNext;
	}

	pList->m_pPrev = pList->m_pNext = pList;
}


// ======================================================================= //
//  0x10039930 -- 303 bytes (the enumerator's 143 stops mid-instruction at
//  0x100399bf; d3dmid.cpp records it).  Free every texture on the LRU list
//  at 0x1008e634, tie the head off (+0x00 stored first here), then have
//  0x1006d3e0 flush the five deferred lists.
// ======================================================================= //
void sub_10039930()
{
	LTLink*   pCur;
	LTLink*   pNext;
	RTexture* pTex;

	pCur = g_1008e634.m_pNext;
	while (pCur != &g_1008e634)
	{
		pTex  = (RTexture*)pCur->m_pData;
		pNext = pCur->m_pNext;
		sub_10039850(pTex, 0);
		pCur = pNext;
	}

	g_1008e634.m_pPrev = g_1008e634.m_pNext = &g_1008e634;

	g_10091950.sub_1006d3e0();
}


// ======================================================================= //
//  0x10039bd0 (+ register entry 0x10039bdc, 404 bytes).  Bind a shared
//  texture: fetch its TextureData from the engine, build the RTexture
//  (0x10038a90), upload it (0x1003a080) -- on an upload failure log it and
//  free the record -- then hand the TextureData back.  Returns the record
//  or NULL.  (pShared, nStage, nFlags): nFlags is big38a90.cpp's
//  RTEX_NOLINK word.
// ======================================================================= //
extern "C" RTexture* sub_10039bd0(SharedTexture* pShared, uint32 nStage,
                                  uint32 nFlags)
{
	RTexture*    pRet;
	RTexture*    pTex;
	TextureData* pData;
	TexRequest   req;
	uint32       nOut;

	pRet = LTNULL;

	pData = g_p1008cbcc->GetTextureData(pShared, &nOut);
	if (pData)
	{
		req.m_pShared00 = pShared;
		req.m_pData04   = pData;
		req.m_nStage08  = nStage;

		pTex = sub_10038a90(&req, nFlags);
		pRet = pTex;

		if (pTex)
		{
			if (!r_TransferTexture(pTex, pData))
			{
				sub_10021d60(4, "Unable to transfer texture data to video memory.");
				sub_10039850(pTex, 0);
				pRet = LTNULL;
			}
		}

		g_p1008cbcc->FreeTextureData(pShared);
	}

	return pRet;
}


// ======================================================================= //
//  0x10039d70 -- 479 bytes.  Build a poly's lightmap.
//
//  bToSurface != 0: lock the surface of the texture current on stage
//  g_1008d7cc (DDLOCK_WAIT | DDLOCK_NOSYSLOCK = 0x801) and have 0x1006ad20
//  render straight into it (bits/pitch/width/height, then bits/pitch
//  again), unlock, answer 1 -- also 1 when there is nothing to lock.
//  bToSurface == 0: fill a lightmap descriptor (0x1006d550, the same
//  __thiscall pair big0ae20.cpp decodes), count the upload
//  (g_nLitPoliesUploaded), render into the descriptor's buffer at the
//  poly's +0x48/+0x49 lightmap size, count a zero result as a save, and
//  answer whether 0x1006d210 accepted it and it was non-zero.
//
//  0x1006ad20's result is an INTEGER here, measured: typed as a pointer,
//  icl sinks the `g_nTextureUploadSaves++` arm below the function, which
//  the shipped code keeps inline (the same int/pointer difference in
//  icl's branch prediction that decides the Lock test below).  It is
//  still handed to 0x1006d210, whose referenced spelling takes a void*.
// ======================================================================= //
struct S1001e520Root;
extern S1001e520Root* g_1008beac;                           // 0x1008beac
extern long g_nLitPoliesUploaded;                           // 0x1008be28
extern long g_nTextureUploadSaves;                          // 0x1008be2c

struct Surf0ae20
{
	uint8	m_pad_00[0x48];
	uint8	m_b48;                                          // +0x48
	uint8	m_b49;                                          // +0x49
};

class CLightDesc0ae20
{
public:
	int32	sub_1006d550(Surf0ae20* pSurf, uint32 nA, uint32 nB,
	                     uint32 nC);                        // 0x1006d550
	int32	sub_1006d210(void* pMap);                       // 0x1006d210

	void*	m_pBits00;                                      // +0x00
	int32	m_nPitch04;                                     // +0x04
	uint8	m_pad_08[0x1c - 0x08];
};

extern "C" uint32 sub_1006ad20(S1001e520Root* pGrid, void* pPoly,
                              void* pBits, int32 nPitch,
                              uint32 nWidth, uint32 nHeight,
                              void* pBits2, int32 nPitch2);  // 0x1006ad20

// Lock the surface for writing: LTFALSE when DirectDraw refuses.  See the
// note in 0x10039d70 for why this is a helper.
static inline LTBOOL LockSurf10039d70(DDSurface7* pDDS, DDSD2* pDesc)
{
	if (pDDS->Lock(LTNULL, pDesc, W6_DDLOCK_WAIT | W6_DDLOCK_NOSYSLOCK, LTNULL) < 0)
		return LTFALSE;
	return LTTRUE;
}

long sub_10039d70(void* pPoly, long bToSurface)
{
	Surf0ae20*      pSurf;
	RTexture*       pTex;
	DDSurface7*     pDDS;
	DDSD2           ddsd;
	CLightDesc0ae20 desc;
	uint32          nMap;

	pSurf = (Surf0ae20*)pPoly;

	if (bToSurface)
	{
		pTex = (RTexture*)g_1008e684[g_1008d7cc];
		if (!pTex)
			return 1;

		pDDS = pTex->m_pSurface0c;
		if (!pDDS)
			return 1;

		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);

		// NEAR, 3 bytes (was 297).  The shipped code sinks
		// the Lock failure's `return 1` below the function (`jl` to a cold
		// copy, the success path falling through).  A direct `< 0` test
		// keeps the return inline in every spelling (the earlier
		// notes: `>= 0` with the work inside, if/else both ways,
		// `!(... >= 0)`, a local HRESULT, nesting under `if (pDDS)`); the
		// Lock wrapped in an inline LTBOOL helper that tests `< 0` and
		// answers LTFALSE/LTTRUE gives the shipped layout -- icl predicts
		// the helper's FALSE unlikely -- and its own cold return copy.
		// (The helper as `return Lock(...) >= 0`, its result in a local,
		// or a result variable inside it all lose the layout again.)
		// What is left is the ORDER of the three cold `return 1` copies:
		// shipped !pTex, Lock failure, !pDDS; ours !pDDS, !pTex, Lock
		// failure (3 jump displacements).  Unchanged by: `== LTNULL`
		// tests, `if (pDDS) {...} else return 1`, the statement orders
		// T/M/D/L that keep the bytes, the lightmap path first or in an
		// else, the Lock test as `!= 0` / `>= 0 -> TRUE`, `== LTFALSE`, and
		// dummy functions ahead of it in the TU (so not TU history).
		if (!LockSurf10039d70(pDDS, &ddsd))
			return 1;

		sub_1006ad20(g_1008beac, pPoly, ddsd.lpSurface, ddsd.lPitch,
		             ddsd.dwWidth, ddsd.dwHeight,
		             ddsd.lpSurface, ddsd.lPitch);

		pDDS->Unlock(LTNULL);
		return 1;
	}

	if (!desc.sub_1006d550(pSurf, 1, 0, 0))
		return 0;

	g_nLitPoliesUploaded++;

	nMap = sub_1006ad20(g_1008beac, pPoly, desc.m_pBits00, desc.m_nPitch04,
	                    pSurf->m_b48, pSurf->m_b49,
	                    desc.m_pBits00, desc.m_nPitch04);
	if (!nMap)
		g_nTextureUploadSaves++;

	return desc.sub_1006d210((void*)nMap) && nMap;
}


// ======================================================================= //
//  0x10039f50 -- 51 bytes + pad.  The shared texture's RTexture +0x40,
//  binding it first (register call into 0x10039bdc, flags 0) when it has
//  none; 0 when that fails.
// ======================================================================= //
extern "C" uint32 sub_10039f50(SharedTexture* pShared, uint32 nStage)
{
	if (!pShared->m_pRTexture0c)
	{
		if (!sub_10039bd0(pShared, nStage, 0))
			return 0;
	}

	return pShared->m_pRTexture0c->m_n40;
}


// ----------------------------------------------------------------------- //
//  0x10039f90 -- 78 bytes of body (80 with the two-byte pad).
//
//  WHAT IT IS: fetch two dwords out of a texture's renderer-side data,
//  creating that data first if it is not there yet.  Answers 1 on success
//  and 0 if the creation failed.
//
//  WHAT PINS IT:
//    * `push %ebx` then `mov 0x8(%esp),%ebx` -- that is 0x4(%esp) before
//      the push, so %ebx is ARGUMENT 1.  After `sub $0x18,%esp` the other
//      three arrive at 0x24, 0x28 and 0x2c(%esp): four arguments, __cdecl.
//    * `mov 0xc(%ebx),%eax; test %eax,%eax; je` -- everything hangs on
//      pTexture->+0x0c being null.
//    * the null arm calls 0x10039bdc with %eax = arg1, %edx = arg2,
//      %ecx = 0, tests the answer, and on success jumps BACK INTO the
//      success arm at 0x10039f9f, which RELOADS 0xc(%ebx).  The reload is
//      the proof that the call is what fills +0x0c.
//    * the success arm stores `pData->+0x04` through arg3 and
//      `pData->+0x08` through arg4, in that order, then `mov $0x1,%eax`.
//      The failure tail is `xor %eax,%eax`.
//    * the two outputs are copied with INTEGER moves (`mov 0x4(%eax),%ecx;
//      mov %ecx,(%edx)`), so they are written here as uint32 even though
//      the caller at 0x1005a010 reads the destination with `flds` and
//      multiplies it.  Writing `float` produces fld/fstp, which is a
//      different function.  (This is the trap d3dmisc.cpp records against
//      d3d_TransposeRotation.)
//    * arg2 is a small index at both call sites (0x1005a007 and 0x1005a098
//      scale it by 4 and by 8 to address a caller-side array), so it is a
//      count/stage, not a pointer.
//
//  CONTEXT, not needed to pin it: 0x10039bd0/0x10039bdc opens by calling
//  g_pStruct->+0x08, which d3d.cpp names GetTextureData, so arg1 is an
//  engine SharedTexture and +0x0c is the renderer's cached form of it.
//  0x10039930's teardown Releases that same +0x0c and subtracts the
//  object's +0x10 from g_pStruct->+0x50, a byte counter.
//
//  IDENTICAL HERE (was a d3dmid.cpp near, 65 bytes
//  differ).  The call goes to 0x10039bdc, twelve bytes past the
//  `mov N(%esp),{eax,edx,ecx}` shim at 0x10039bd0, with the arguments
//  already in registers: /Qip, in the TU that holds 0x10039bd0 -- this
//  one.  The `mov %edi,0x10(%esp)` frame-slot save is /O2.  The data
//  pointer is a named local read once after the bind, so both stores go
//  through one load of +0x0c (read twice straight off pTexture, the second
//  store's load is repeated).  The two types keep d3dmid.cpp's tags, so the
//  symbol is spelled as before; the bind is w6's extern "C" 0x10039bd0.
// ----------------------------------------------------------------------- //

struct D3DTextureData
{
	uint32  m_nUnknown00;                               // +0x00
	uint32  m_nUnknown04;                               // +0x04
	uint32  m_nUnknown08;                               // +0x08
};

struct D3DSharedTexture
{
	uint8            m_Reserved00[0x0c];                // +0x00..+0x0b
	D3DTextureData*  m_pData;                           // +0x0c
};

uint32 sub_10039f90(D3DSharedTexture* pTexture, uint32 nIndex,
                    uint32* pOut0, uint32* pOut1)
{
	D3DTextureData* pData;

	if (!pTexture->m_pData)
	{
		if (!sub_10039bd0((SharedTexture*)pTexture, nIndex, 0))
			return 0;
	}

	pData = pTexture->m_pData;
	*pOut0 = pData->m_nUnknown04;
	*pOut1 = pData->m_nUnknown08;

	return 1;
}


// ======================================================================= //
//  0x10039fe0 -- 146 bytes + pad.  RenderStruct slot +0x74 (w26's
//  RenderDLLSetup): bind a shared texture.  With no RTexture yet it is
//  created through 0x10039bd0 (register entry 0x10039bdc: %eax the shared
//  texture, %edx the current stage 0x1008d7d0, %ecx 0) and that is all;
//  otherwise, when the second argument asks for it, the engine's texture
//  data is fetched (+0x08) and every RTexture on the +0x30 chain is
//  re-uploaded (r_TransferTexture, logging each failure at level 4), and
//  the data handed back (+0x0c).  Added here: it belongs to this
//  TU -- the register call into 0x10039bdc -- and is /O2 (lea-nop pad).
//
//  IDENTICAL: the shipped loop calls r_TransferTexture
//  through its register entry (`mov %ebx,%eax ; mov %ebp,%edx ; call
//  0x1003a080`), and r_TransferTexture is now a static at the end of this
//  file.
// ======================================================================= //
extern uint32 g_1008d7d0;                                   // 0x1008d7d0, current stage

void sub_10039fe0(SharedTexture* pShared, LTBOOL bUpload)
{
	RTexture*    pTex;
	RTexture*    pCur;
	TextureData* pData;
	uint32       nOut;

	pTex = pShared->m_pRTexture0c;
	if (!pTex)
	{
		sub_10039bd0(pShared, g_1008d7d0, 0);
		return;
	}

	if (bUpload)
	{
		pData = g_p1008cbcc->GetTextureData(pShared, &nOut);
		if (pData)
		{
			for (pCur = pTex; pCur; pCur = pCur->m_pNext30)
			{
				if (!r_TransferTexture(pCur, pData))
					sub_10021d60(4, "Unable to transfer texture data to video memory.");
			}

			g_p1008cbcc->FreeTextureData(pShared);
		}
	}
}


// ======================================================================= //
//  0x1003aac0 -- 222 bytes.  RenderStruct slot +0x78: unbind a shared
//  texture -- r_FreeTexture(its RTexture, not forced), 0x10039850's whole
//  body inlined (the children through the register entry with %edx = 1),
//  as /Qip does for the callers above.  Frame and
//  padding are this file's /O2.
// ======================================================================= //
void sub_1003aac0(SharedTexture* pShared)
{
	RTexture* pTex;

	pTex = pShared->m_pRTexture0c;
	if (pTex)
		sub_10039850(pTex, 0);
}



// ======================================================================= //
//  THE TEXTURE-FORMAT LISTING -- the LISTTEXTUREFORMATS console command
//  (RenderCommand, 0x10032d00, w32_10032d00_qip.cpp).
//
//  0x100375f0 and 0x10037b10 are REGISTER-ONLY entries (no stack shim:
//  the bytes before each are the previous function's padding), called
//  with %eax/%edx loaded from 0x10037b10 and 0x10039a60 -- statics of this
//  /O2 /Qip TU, which in the shipped image runs from 0x100375f0 to
//  0x1003aba0.  0x100375f0 pads with `lea 0x0(%esi),%esi` and 0x10037b10
//  with a lea-nop: /O2, like the rest of this file; its strcats are the
//  inline `repnz scas` copy (intrinsics on).
//
//  The format record is big3a080.cpp's D3DFormatRec (0x1008e6a4 is the
//  list of every format the device enumerated, 0x1008e6b0 the table of
//  the ones chosen per texture class); the four dwords at +0x34..+0x40
//  are printed as the channel bit counts.
// ======================================================================= //
struct D3DFormatRec
{
	uint32  m_nUnknown00[3];                                // +0x00..+0x0b
	uint32  m_dwSize;                                       // +0x0c DDPIXELFORMAT
	uint32  m_dwFlags;                                      // +0x10
	uint32  m_dwFourCC;                                     // +0x14
	uint32  m_dwRGBBitCount;                                // +0x18
	uint32  m_dwRBitMask;                                   // +0x1c
	uint32  m_dwGBitMask;                                   // +0x20
	uint32  m_dwBBitMask;                                   // +0x24
	uint32  m_dwRGBAlphaBitMask;                            // +0x28
	uint32  m_nPixelShift;                                  // +0x2c
	uint32  m_nPixelSize;                                   // +0x30
	uint32  m_nBits[4];                                     // +0x34..+0x43
};

extern LTLink        g_1008e6a4;                            // 0x1008e6a4
extern D3DFormatRec* g_1008e6b0[7];                         // 0x1008e6b0

extern "C" int __cdecl sprintf(char* pBuf, const char* pszFmt, ...);   // 0x10080cbc


// ======================================================================= //
//  0x100375f0 -- 1309 bytes.  The first DDPF_* flag set in dwFlags, by
//  name and followed by a space, into pszOut ("UNKNOWN " when none is).
//  One flag only: every arm returns.
// ======================================================================= //
static void sub_100375f0(uint32 dwFlags, char* pszOut)
{
	pszOut[0] = 0;

	if (dwFlags & 0x00000002)       { strcat(pszOut, "DDPF_ALPHA");             strcat(pszOut, " "); }
	else if (dwFlags & 0x00000001)  { strcat(pszOut, "DDPF_ALPHAPIXELS");       strcat(pszOut, " "); }
	else if (dwFlags & 0x00008000)  { strcat(pszOut, "DDPF_ALPHAPREMULT");      strcat(pszOut, " "); }
	else if (dwFlags & 0x00040000)  { strcat(pszOut, "DDPF_BUMPLUMINANCE");     strcat(pszOut, " "); }
	else if (dwFlags & 0x00080000)  { strcat(pszOut, "DDPF_BUMPDUDV");          strcat(pszOut, " "); }
	else if (dwFlags & 0x00000080)  { strcat(pszOut, "DDPF_COMPRESSED");        strcat(pszOut, " "); }
	else if (dwFlags & 0x00000004)  { strcat(pszOut, "DDPF_FOURCC");            strcat(pszOut, " "); }
	else if (dwFlags & 0x00020000)  { strcat(pszOut, "DDPF_LUMINANCE");         strcat(pszOut, " "); }
	else if (dwFlags & 0x00000800)  { strcat(pszOut, "DDPF_PALETTEINDEXED1");   strcat(pszOut, " "); }
	else if (dwFlags & 0x00001000)  { strcat(pszOut, "DDPF_PALETTEINDEXED2");   strcat(pszOut, " "); }
	else if (dwFlags & 0x00000008)  { strcat(pszOut, "DDPF_PALETTEINDEXED4");   strcat(pszOut, " "); }
	else if (dwFlags & 0x00000020)  { strcat(pszOut, "DDPF_PALETTEINDEXED8");   strcat(pszOut, " "); }
	else if (dwFlags & 0x00000010)  { strcat(pszOut, "DDPF_PALETTEINDEXEDTO8"); strcat(pszOut, " "); }
	else if (dwFlags & 0x00000040)  { strcat(pszOut, "DDPF_RGB");               strcat(pszOut, " "); }
	else if (dwFlags & 0x00000100)  { strcat(pszOut, "DDPF_RGBTOYUV");          strcat(pszOut, " "); }
	else if (dwFlags & 0x00004000)  { strcat(pszOut, "DDPF_STENCILBUFFER");     strcat(pszOut, " "); }
	else if (dwFlags & 0x00000400)  { strcat(pszOut, "DDPF_ZBUFFER");           strcat(pszOut, " "); }
	else if (dwFlags & 0x00002000)  { strcat(pszOut, "DDPF_ZPIXELS");           strcat(pszOut, " "); }
	else                            { strcat(pszOut, "UNKNOWN");                strcat(pszOut, " "); }
}


// ======================================================================= //
//  0x10037b10 -- 217 bytes.  One console line for a format: the prefix,
//  its flags (or "FOURCC( xxxx )" for a FourCC format), its bit count,
//  channel bit counts and masks; "%sNONE" for a missing one.
// ======================================================================= //
static void sub_10037b10(const char* pszPrefix, D3DFormatRec* pRec)
{
	char szFlags[256];
	char szFourCC[256];

	if (!pRec)
	{
		g_p1008cbcc->ConsolePrintf("%sNONE", pszPrefix);
		return;
	}

	if (pRec->m_dwFlags & 0x00000004)                       // DDPF_FOURCC
	{
		memcpy(szFourCC, &pRec->m_dwFourCC, 4);
		szFourCC[4] = 0;
		sprintf(szFlags, "FOURCC( %s )", szFourCC);
	}
	else
	{
		sub_100375f0(pRec->m_dwFlags, szFlags);
	}

	g_p1008cbcc->ConsolePrintf("%s%s - %d bits (%d %d %d %d) (%x %x %x %x)",
		pszPrefix, szFlags, pRec->m_dwRGBBitCount,
		pRec->m_nBits[0], pRec->m_nBits[1], pRec->m_nBits[2], pRec->m_nBits[3],
		pRec->m_dwRBitMask, pRec->m_dwGBitMask, pRec->m_dwBBitMask,
		pRec->m_dwRGBAlphaBitMask);
}


// ======================================================================= //
//  0x10039a60 -- 363 bytes.  LISTTEXTUREFORMATS: every enumerated format
//  (0x10037b10 inlined into the loop by /Qip), then the five chosen ones.
// ======================================================================= //
// NEAR, 5 bytes: the inlined 0x10037b10's ConsolePrintf
// stores its format (hoisted into %edi) before szFlags; shipped stores
// szFlags (`lea 0x30(%esp)`) first.  Unchanged: a pRec local, a while
// loop, the list head through a pointer, the 0x10037b10 body with an
// else arm, a format local, swapped buffer declarations, `(*pfn)()`,
// const record, char* prefix/format, int fields, a prefix local in the
// loop, a szFlags pointer, the whole listing block first in the file or
// this function last, and dummy functions ahead (so not TU history).
// A RenderStats local, szFourCC[5] and a dword FourCC store lose
// 0x10037b10.
void sub_10039a60()
{
	LTLink* pCur;

	for (pCur = g_1008e6a4.m_pNext; pCur != &g_1008e6a4; pCur = pCur->m_pNext)
		sub_10037b10("", (D3DFormatRec*)pCur->m_pData);

	sub_10037b10("[FULLBRITE] - ", g_1008e6b0[1]);
	sub_10037b10("[4444] - ", g_1008e6b0[2]);
	sub_10037b10("[NORMAL] - ", g_1008e6b0[3]);
	sub_10037b10("[INTERFACE] - ", g_1008e6b0[4]);
	sub_10037b10("[LIGHTMAP] - ", g_1008e6b0[5]);
}


// ======================================================================= //
//  r_TransferTexture (0x1003a080) -- MOVED HERE from
//  big3a080.cpp, whose banner follows unchanged.  It is a /Qip REGISTER-
//  ONLY entry (no stack shim; the bytes before it are 0x10039fe0's
//  padding) and its only two callers, 0x10039bdc and 0x10039fe0, are in
//  this file: so it is a static of this TU.  Measured: 0x10039fe0 became
//  byte-identical (was 63 bytes differing, the register call being its
//  only residual); nothing here lost; r_TransferTexture itself went from
//  2480 bytes / 719 instructions (as an ordinary /O2 function) to 2496 /
//  723 against the shipped 2613 / 723.  Its types are big3a080.cpp's,
//  except that RTexture and D3DFormatRec are this file's (the fields it
//  uses are the same offsets under this file's names) and its C-style
//  DirectDraw surface is DDSurf7T, this file's DDSurface7 being the C++
//  interface spelling.
// ======================================================================= //
// *********************************************************************** //
//
//  (was) MODULE:   BIG3A080 (.CPP)  --  one large function from d3d.ren
//
//  TARGET:   0x1003a080 .. 0x1003aab4 inclusive (2,613 bytes), followed by
//            eleven bytes of lea-nop padding at 0x1003aab5..0x1003aabf and
//            the next function at 0x1003aac0.
//
//  STANDARD: STRUCTURAL, not byte-exact.
//            Every offset, constant, string, branch and vtable slot below
//            was read off the retail d3d.ren.  Nothing is guessed; the coin-flips
//            that could not be settled are called out in comments.
//
// ----------------------------------------------------------------------- //
//  EXTENT CHECK -- THE STATED EXTENT IS WRONG.
//
//  The row said "1,580 bytes, stated end 0x1003a6ac".  It is not.
//
//  * 0x1003a6ac IS NOT AN INSTRUCTION BOUNDARY.  The byte at 0x1003a6ab is
//    `74`, the opcode of `je 0x1003a700`, whose displacement byte is
//    0x1003a6ac.  A range ending there cuts a two-byte jcc in half.
//  * FIVE BRANCHES INSIDE THE STATED RANGE TARGET PAST IT:
//        0x1003a0e6  je 0x1003aa86      (nFormat == 0 -> 3)
//        0x1003a18e  je 0x1003aaa4      (null surface -> return 0)
//        0x1003a4d9  je 0x1003a9c0      (nFormat == 3)
//        0x1003a4e2  je 0x1003a9b0      (nFormat == 4)
//        0x1003a4eb  je 0x1003a999      (nFormat == 5)
//    and 0x1003aa86 / 0x1003a9c0 / 0x1003a9b0 / 0x1003a999 all jump BACK
//    inside (to 0x1003a0ef, 0x1003a527, 0x1003a678, 0x1003a678).  Those are
//    /O2 cold blocks sunk below the last hot exit, not separate functions.
//  * ONE PROLOGUE in 0x1003a080..0x1003aab4: `55 8b ec` at 0x1003a080.
//    The next `push %ebp; mov %esp,%ebp` is at 0x1003aac0, past the end.
//  * THIRTEEN `ret`s, at 0x1003a0df, 0x1003a1ee, 0x1003a333, 0x1003a4d2,
//    0x1003a526, 0x1003a7d8, 0x1003a8c1, 0x1003a92f, 0x1003a95c,
//    0x1003a998, 0x1003aa85, 0x1003aaa3 and 0x1003aab4.  All thirteen are
//    the same epilogue byte for byte:
//        8b 1c 24 / 8b 74 24 04 / 8b 7c 24 08 / 8b e5 / 5d / c3
//    matching the prologue's `mov %edi,0x8(%esp) / %esi,0x4(%esp) /
//    %ebx,(%esp)` at 0x1003a089.  Thirteen duplicated epilogues is /O2, not
//    thirteen functions (big331e0.cpp records the same count).
//  * NO branch anywhere in the range targets past 0x1003aab4; the furthest
//    forward target is 0x1003aaa4, the last block.
//  * TWO CALLERS, both of the true start 0x1003a080: 0x10039c4a and
//    0x1003a024.  Nothing calls or jumps into 0x1003a6ac.
//  * After 0x1003aab4: `8d 74 26 00` + `8d bc 27 00 00 00 00`, the
//    compiler's lea-nop alignment padding, then the prologue at 0x1003aac0.
//
//  VERDICT: ONE function, 0x1003a080..0x1003aab4 inclusive, 2,613 bytes --
//  1,033 bytes more than stated.
//
//  Call sequence over 0x1003a080..0x1003aab5:
//      2,613 bytes, 32 calls (12 direct, 20 indirect), 82 bytes per call
//
//  8 DISTINCT direct callees (0x10079fd0, 0x10021d60, 0x10081222,
//  0x1007a130, 0x10080479, 0x10079f30, 0x1007a3b0, 0x10021010) over 12
//  direct calls, plus 20 indirect COM slots whose POSITION the check can
//  still compare.  82 B/call is inside the strong band.
//
// ----------------------------------------------------------------------- //
//  WHAT IT IS: r_TransferTexture -- upload one RTexture's mip chain into
//  its DirectDraw/Direct3D texture surface chain.
//
//  Named outright by its own string, 0x100a9770:
//      "r_TransferTexture: mipmap count doesn't match!"
//  and by 0x100a97a0, "Uploading a (%dx%d) texture".  Both callers report
//  its failure as "Unable to transfer texture data to video memory."
//  (0x100a9870) and then tear the texture down.
//
//  ARGUMENTS ARRIVE IN REGISTERS (icl /Qip): 0x1003a094 is
//  `mov %eax,%ebx ; mov %edx,%edi`, and both call sites load them that way
//  (0x10039c46 and 0x1003a020-0x1003a022 `mov %ebx,%eax ; mov %ebp,%edx`).
//  That is decoded as an ordinary two-argument call; the
//  tier-Y/Z classification does not apply.  arg1 = the renderer's RTexture
//  (walked by the caller through +0x30), arg2 = the engine's TextureData
//  (the RETURN value of g_pStruct->+0x08, which d3d.cpp names
//  GetTextureData; 0x10039fe0 does exactly that and then loops
//  0x1003a020..0x1003a048 calling us once per RTexture in the chain).
//
//  ONE LOOP over mip levels m_nFirstMip .. m_nFirstMip+m_nMipCount-1.  Per
//  level: Lock the surface; if the SURFACE is DDPF_BUMPDUDV convert the mip
//  to 8 bits into a scratch buffer and differentiate it into V8U8 in place;
//  otherwise switch on the texture's format byte -- 3 = convert through the
//  format table's DDPIXELFORMAT, 4/5/6 = 'DXT1'/'DXT3'/'DXT5' copied or
//  blitted in; then for format 3 tile the mip across a larger surface;
//  Unlock, GetAttachedSurface(DDSCAPS_MIPMAP|DDSCAPS_TEXTURE), halve the
//  size and go round.
//
//  THE DXT NUMBERING IS CROSS-CHECKED, NOT ASSUMED.  0x1003a9b0 /
//  0x1003a999 / 0x1003a670 plant 0x31545844 / 0x33545844 / 0x35545844 --
//  'DXT1' / 'DXT3' / 'DXT5' -- for formats 4 / 5 / 6, and d3dhigh.cpp's
//  independent decode of 0x10079f30 (the size-of-a-surface helper this
//  function calls with the SAME format number) found 4, 5 and 6 to be
//  exactly the three whose bytes-per-pixel table entry is zero: "DXT1 /
//  DXT3 / DXT5 in that order".  Two unrelated readings, same three numbers.
//
// ----------------------------------------------------------------------- //
//  FRAME ARITHMETIC, AND WHY IT IS THE STRONGEST EVIDENCE HERE
//
//  `sub $0x3a8,%esp` = 936 bytes.  The bottom twelve are the register save
//  area (`mov %edi,0x8(%esp) / %esi,0x4(%esp) / %ebx,(%esp)`, restored by
//  all thirteen epilogues).  The 896 bytes from ebp-0x380 to ebp-0x01
//  decompose EXACTLY, with no slack, into the ten objects below -- and each
//  object's size is fixed independently of this function:
//
//    -0x380  ConvertRequest  req        0x98   ctor 0x10079fd0
//    -0x2e8  DDSurfaceDesc2  ddsd       0x7c   dwSize = 0x7c planted
//    -0x26c  DDSurface7*     pSurf      0x04
//    -0x268  DDSurfaceDesc2  lockDesc   0x7c   dwSize = 0x7c planted
//    -0x1ec  ConvertRequest  req2       0x98   ctor 0x10079fd0
//    -0x154  DDSurfaceDesc2  tempDesc   0x7c   rep stos 0x1f dwords = 124
//    -0x0d8  DDSurface7*     pTempSurf  0x04
//    -0x0d4  DDSurfaceDesc2  tempLock   0x7c   rep stos 0x1f dwords = 124
//    -0x058  DDSCAPS2        caps       0x10
//    -0x048  eighteen dwords            0x48
//                                      -----
//                                      0x380 = 896
//
//  The 0x98 is not a guess either.  0x10079fd0 sets `(%ecx) = %ecx+0x28`
//  and `0x10(%ecx) = %ecx+0x60` and stamps the vtable 0x10089390 into both
//  +0x28 and +0x60, so the object carries two embedded pixel-format
//  sub-objects 0x38 apart; d3dr4c.cpp's independent decode of 0x1007a0d0
//  found the SAME 0x38 stride (`m_Fmt38` at +0x38 of a two-format object)
//  and the SAME vtable address.  0x28 + 0x38 + 0x38 = 0x98, and 0x98
//  lands req's end exactly on ddsd's start and req2's end exactly on
//  tempDesc's start.  A different size for either would break the sum.
//
//  The format-table record decomposes the same way: 0x10021010 is handed
//  `pFmtRec + 0x0c` and reads +0x0c/+0x10/+0x14/+0x18/+0x1c off it (a
//  DDPIXELFORMAT's dwRGBBitCount/R/G/B/Alpha masks), so the DDPIXELFORMAT
//  occupies +0x0c..+0x2b of the record -- and the two scalars this function
//  reads out of the record are +0x2c and +0x30, the first two dwords after
//  it.  Nothing overlaps.
//
// ----------------------------------------------------------------------- //
//  FLAG VERDICT: /O2, for the whole file.
//    * thirteen duplicated epilogues, no cross-jumped exits;
//    * every outgoing argument block is `add $-N,%esp` then `mov ..,K(%esp)`
//      -- 0x1003a0b5, 0x1003a0f6, 0x1003a13b, 0x1003a197, 0x1003a26b,
//      0x1003a458, 0x1003a67b, 0x1003a7e8, 0x1003a86e, 0x1003a8c2 ... --
//      and the only `push`es in the function are the five one-dword
//      argument areas (0x1003a258, 0x1003a2db, 0x1003a916, 0x1003a97f,
//      0x1003a8a7) and the `push %ecx` halves of icl's inline rep movs/stos;
//    * `&pFmtRec->m_ddpf` at 0x1003a9f6 is `mov -0x44(%ebp),%edx ;
//      add $0xc,%edx`, not `lea 0xc(%edx),%edx`;
//    * `return 1` at 0x1003a4bf is `mov $0x1,%eax`, not xor/inc;
//    * the four cold arms (0x1003a999, 0x1003a9b0, 0x1003a9c0, 0x1003aa86)
//      are SUNK below the last hot exit, which is what makes the stated
//      extent wrong in the first place.
//
// *********************************************************************** //




// ======================================================================= //
//  TYPES
//
//  Nothing in LT2\sdk\inc declares the DirectDraw shapes and d3dr4b.cpp
//  includes only "ltbasedefs.h", so they are spelled out here at the
//  offsets the bytes give.  The DDSurfaceDesc2 / DDSCaps2 / DDraw7 /
//  DDSurf7T declarations are big331e0.cpp's, reused verbatim except that
//  DDPixelFormat is broken out into fields (this function reads three of
//  them) and the vtable slots this function uses are filled in.
// ======================================================================= //

struct DDPixelFormat
{
	uint32 dwSize;                                      // +0x00
	uint32 dwFlags;                                     // +0x04
	uint32 dwFourCC;                                    // +0x08
	uint32 dwRGBBitCount;                               // +0x0c
	uint32 dwRBitMask;                                  // +0x10
	uint32 dwGBitMask;                                  // +0x14
	uint32 dwBBitMask;                                  // +0x18
	uint32 dwRGBAlphaBitMask;                           // +0x1c
};                                                      // 0x20

struct DDSCaps2 { uint32 dwCaps, dwCaps2, dwCaps3, dwCaps4; };

struct DDSurfaceDesc2
{
	uint32        dwSize;                               // +0x00
	uint32        dwFlags;                              // +0x04
	uint32        dwHeight;                             // +0x08
	uint32        dwWidth;                              // +0x0c
	int32         lPitch;                               // +0x10
	uint32        dwBackBufferCount;                    // +0x14
	uint32        dwMipMapCount;                        // +0x18
	uint32        dwAlphaBitDepth;                      // +0x1c
	uint32        dwReserved;                           // +0x20
	void*         lpSurface;                            // +0x24
	uint32        ddckCKDestOverlay[2];                 // +0x28
	uint32        ddckCKDestBlt[2];                     // +0x30
	uint32        ddckCKSrcOverlay[2];                  // +0x38
	uint32        ddckCKSrcBlt[2];                      // +0x40
	DDPixelFormat ddpfPixelFormat;                      // +0x48
	DDSCaps2      ddsCaps;                              // +0x68
	uint32        dwTextureStage;                       // +0x78
};                                                      // 0x7c

// The DirectDraw constants this function plants, all read off the bytes.
#define DDSD_CAPS                       0x00000001
#define DDSD_HEIGHT                     0x00000002
#define DDSD_WIDTH                      0x00000004
#define DDSD_PIXELFORMAT                0x00001000
#define DDPF_FOURCC                     0x00000004
#define DDPF_BUMPDUDV                   0x00080000
#define DDSCAPS_SYSTEMMEMORY            0x00000800
#define DDSCAPS_TEXTURE                 0x00001000
#define DDSCAPS_MIPMAP                  0x00400000
#define DDBLTFAST_WAIT                  0x00000010

// The two Lock() flag words are left as hex on purpose.  0x20 is the flag
// this module's OTHER Lock sites use (d3dbatchb.cpp reads 0x801 as
// DDLOCK_WAIT|DDLOCK_SURFACEMEMORYPTR and d3dblit.cpp defines DDLOCK_WAIT
// as 1), and neither 0x20 nor 0x21 decomposes into that spelling, so
// naming them would be guessing.  0x21 is 0x20 with bit 0 added.
#define R_LOCK_FLAGS                    0x00000020
#define R_LOCK_FLAGS_TEMP               0x00000021


// ----------------------------------------------------------------------- //
//  COM INTERFACES.  Every slot below lands on the documented index for its
//  interface, which is the cross-check that the pointers are typed right:
//    IDirectDrawSurface7:  +0x08 Release, +0x1c BltFast,
//                          +0x30 GetAttachedSurface, +0x58 GetSurfaceDesc,
//                          +0x64 Lock, +0x80 Unlock
//    IDirectDraw7:         +0x18 CreateSurface
//  d3dblit.cpp already records +0x64 as Lock(pRect, pDesc, dwFlags, hEvent)
//  and big331e0.cpp already records IDirectDraw7 +0x18 as CreateSurface.
// ----------------------------------------------------------------------- //

struct DDSurf7T;

struct DDSurf7TVtbl
{
	void* m_pad_00[2];                                  // +0x00..+0x04
	long (__stdcall* Release)(DDSurf7T*);             // +0x08
	void* m_pad_0c[4];                                  // +0x0c..+0x18
	long (__stdcall* BltFast)(DDSurf7T*, uint32 dwX, uint32 dwY,
	                          DDSurf7T* lpDDSrcSurface, void* lpSrcRect,
	                          uint32 dwTrans);          // +0x1c
	void* m_pad_20[4];                                  // +0x20..+0x2c
	long (__stdcall* GetAttachedSurface)(DDSurf7T*, DDSCaps2* lpDDSCaps,
	                                     DDSurf7T** lplpDDAttached);
	                                                    // +0x30
	void* m_pad_34[9];                                  // +0x34..+0x54
	long (__stdcall* GetSurfaceDesc)(DDSurf7T*,
	                                 DDSurfaceDesc2* lpDDSurfaceDesc);
	                                                    // +0x58
	void* m_pad_5c[2];                                  // +0x5c..+0x60
	long (__stdcall* Lock)(DDSurf7T*, void* lpDestRect,
	                       DDSurfaceDesc2* lpDDSurfaceDesc, uint32 dwFlags,
	                       void* hEvent);               // +0x64
	void* m_pad_68[6];                                  // +0x68..+0x7c
	long (__stdcall* Unlock)(DDSurf7T*, void* lpRect);// +0x80
};
struct DDSurf7T { DDSurf7TVtbl* lpVtbl; };

struct DDraw7;
struct DDraw7Vtbl
{
	void* m_pad_00[6];                                  // +0x00..+0x14
	long (__stdcall* CreateSurface)(DDraw7*,
	                                DDSurfaceDesc2* lpDDSurfaceDesc,
	                                DDSurf7T** lplpDDSurface,
	                                void* pUnkOuter);   // +0x18
};
struct DDraw7 { DDraw7Vtbl* lpVtbl; };

// 0x1008bf74.  d3dbatcha.cpp already names this one g_pDirectDraw.
extern DDraw7* g_pDirectDraw;                           // 0x1008bf74


// ----------------------------------------------------------------------- //
//  THE MODULE'S PIXEL-FORMAT CLASSES
//
//  PFormat -- 0x38 bytes, vtable 0x10089390, slot 0 is a five-argument
//  Init.  Established twice over:
//    * d3dr4c.cpp's decode of 0x1007a0d0 declares the identical thing
//      (`Fmt1007a0d0::Slot00(nA,nB,nC,nD,nE)`, vtable g_10089390) and finds
//      it embedded at +0x38 of a two-format object -- 0x38 stride;
//    * the three call sites agree on the argument ORDER:
//        0x1007a3b0  Slot00(3, 0xff000000, 0x00ff0000, 0x0000ff00, 0xff)
//        0x1007a0d0  Slot00(2, 0,          0xf800,     0x07e0, 0x001f)
//        0x10021010  Slot00(t, pPF->dwRGBAlphaBitMask, pPF->dwRBitMask,
//                              pPF->dwGBitMask,        pPF->dwBBitMask)
//      = (type, alpha, red, green, blue).  32-bit ARGB, RGB565, and a
//      DDPIXELFORMAT forwarded field by field.  Three readings, one order.
// ----------------------------------------------------------------------- //
class PFormat
{
public:
	// 0x1007a3b0 -- Slot00(3, 0xff000000, 0xff0000, 0xff00, 0xff).
	// d3dbatchd.cpp already records this address as a thiscall member.
	// Named with "Default" for icl's cold-call layout (0x10038630 hosts
	// its one conditional call past the epilogue, as shipped; aliased to
	// the definition in link/aliases.rsp).
	void            InitDefault1007a3b0();              // 0x1007a3b0

	virtual uint32  Slot00(uint32 nType, uint32 nAlphaMask, uint32 nRedMask,
	                       uint32 nGreenMask, uint32 nBlueMask);
};                                                      // 0x38


// The argument block 0x1007a130 consumes.  0x1007a130 reads +0x00 and
// +0x10 out of it, takes their +0x04 (the format type) and indexes
// 0x100898b0[srcType*8 + dstType] for a converter to call with
// (theManager, theRequest) -- so +0x00 and +0x10 are the two PFormat*s and
// the ctor's `(%ecx) = %ecx+0x28` / `0x10(%ecx) = %ecx+0x60` points them at
// the two embedded formats.  Every other field below is written by this
// function and read off its stores.
struct ConvertRequest
{
	// 0x10079fd0.  Called as a thiscall on a stack local, `this` in %ecx.
	// Written as an explicit member rather than a constructor because
	// there is no body to give it and because a real ctor would make icl
	// generate vptr stores for the two embedded formats.
	ConvertRequest* Construct();                        // 0x10079fd0

	PFormat* m_pSrcFormat;                              // +0x00
	void*    m_pSrcData;                                // +0x04
	uint32   m_nSrcPitch;                               // +0x08
	uint32   m_nUnknown0c;                              // +0x0c  ctor: 0
	PFormat* m_pDstFormat;                              // +0x10
	void*    m_pDstData;                                // +0x14
	uint32   m_nDstPitch;                               // +0x18
	uint32   m_nWidth;                                  // +0x1c
	uint32   m_nHeight;                                 // +0x20
	uint32   m_nUnknown24;                              // +0x24  ctor: 0
	uint8    m_SrcFormat[0x38];                         // +0x28
	uint8    m_DstFormat[0x38];                         // +0x60
};                                                      // 0x98

// 0x1008daa0.  d3dr5c.cpp already names this global and records
// 0x1007a0d0 being reached as `mov $0x1008daa0,%ecx` with nothing pushed.
// 0x1007a130 ends `ret $0x4`: thiscall, one stack argument.
class FormatMgr
{
public:
	uint32 ConvertPixels(ConvertRequest* pRequest);     // 0x1007a130

	// 0x1007a180 -- convert a colour into a format's pixel, 0 on success
	// (d3dr2c_os.cpp's C1007a180Disp); 0x10038630 uses it.  LTRESULT,
	// unsigned: as int32 icl predicts it non-zero and sinks 0x10038630's
	// colour-key stores, which the shipped code keeps inline.
	LTRESULT sub_1007a180(PFormat* pFmt, uint32 nColor, uint32* pOut);
};
extern FormatMgr g_1008daa0;                            // 0x1008daa0


// ----------------------------------------------------------------------- //
//  THE ENGINE AND RENDERER TEXTURE SHAPES
// ----------------------------------------------------------------------- //

// The per-level record.  Stride 0x18 (`imul $0x18,%eax,%eax` at 0x1003a17b
// and `add $0x18,%eax` at 0x1003a4a1), base +0xcc in TextureData.  Four of
// its six dwords are used here and each is pinned by what it feeds:
//   +0x00  width   -- the ConvertRequest's m_nWidth AND its m_nDstPitch in
//                     the 8-bit case, and the divisor for the X tiling
//   +0x04  height  -- m_nHeight, and the divisor for the Y tiling
//   +0x08  data    -- m_pSrcData, and the memcpy source
//   +0x0c  pitch   -- m_nSrcPitch
struct TextureMipData
{
	uint32 m_nWidth;                                    // +0x00
	uint32 m_nHeight;                                   // +0x04
	uint8* m_pData;                                     // +0x08
	uint32 m_nPitch;                                    // +0x0c
	uint32 m_nUnknown10;                                // +0x10
	uint32 m_nUnknown14;                                // +0x14
};                                                      // 0x18

// The engine-side texture.  It is POLYMORPHIC: 0x1003a255 is
// `mov (%ecx),%eax ; mov (%eax),%eax ; call *%eax` with %ecx holding the
// object -- a thiscall virtual through slot 0 -- and its one argument is
// the ConvertRequest's source PFormat, so slot 0 fills in the format that
// describes the texture's own pixels.  (Same instruction shape as the
// PFormat call at 0x1003a2a9 and as 0x1007a3b0's own, which d3dr4c.cpp
// already reads as slot 0.)
//
// PACKED (found at runtime): m_fScaleBias26 sits at +0x26, which is not a float
// boundary.  Without the pack icl aligned it to +0x28 and shifted every
// later field by two -- m_Mips landed at +0xd0 instead of +0xcc, so
// r_TransferTexture read each mip's width/height/data/pitch from the wrong
// dwords and every texture uploaded as garbage (the dark-blue tiled world).
// Found by splicing r_TransferTexture alone into the shipped image (a
// since-retired test tool): the converter then read its source pointer
// from a mip's pitch.
#pragma pack(push, 1)
class TextureData
{
public:
	virtual void   Slot00(PFormat* pFormat);               // vtable +0x00

	uint8          m_Reserved04[0x10];                  // +0x04..+0x13
	uint16         m_nMipCount;                         // +0x14
	uint8          m_Reserved16[0x02];                  // +0x16..+0x17
	uint32         m_nFlags18;                          // +0x18
	uint8          m_Reserved1c[0x04];                  // +0x1c..+0x1f
	uint8          m_nMipOffset20;                      // +0x20
	uint8          m_nWantedMips21;                     // +0x21
	uint8          m_nFormat;                           // +0x22
	uint8          m_nExtraBias23;                      // +0x23
	uint8          m_Reserved24[0x02];                  // +0x24..+0x25
	float          m_fScaleBias26;                      // +0x26 (UNALIGNED)
	uint8          m_nAngleLo2a;                        // +0x2a
	uint8          m_nAngleHi2b;                        // +0x2b
	uint8          m_Reserved2c[0xcc - 0x2c];           // +0x2c..+0xcb
	TextureMipData m_Mips[1];                           // +0xcc
};
#pragma pack(pop)


// One entry of the module's surface-format table.  See the frame-arithmetic
// note above for why the DDPIXELFORMAT is at +0x0c and the two scalars at
// +0x2c/+0x30 -- the DDPIXELFORMAT ends at +0x2b and they are the next two
// dwords.  +0x2c is used as a SHIFT (`shl %cl,%edx` on a pixel count) and
// +0x30 as a MULTIPLIER (`imul 0x30(%eax),%edi` on a pixel count): the
// log2 and the value of the same bytes-per-pixel, and the function really
// does use both, one in each of the two tiling loops.

// 0x1008e6b0, indexed by RTexture::m_nFormatIndex with a dword scale.
// d3dr5d.cpp's decode of the module terminator clears SEVEN consecutive
// dwords here off one base register and reads them as one aggregate; this
// is that aggregate, and the indexing here says the seven dwords are
// pointers.


// ----------------------------------------------------------------------- //
//  PLAIN CALLEES
// ----------------------------------------------------------------------- //

// 0x10021d60 -- the renderer's level-plus-format warning.  Declared the
// same way d3dr5a_o2.cpp declares it.
extern void sub_10021d60(uint32 nLevel, const char* pszMsg, ...);

// 0x10081222 -- `push $1 ; push size ; call _nh_malloc`, operator new
// (d3dr3a.cpp, d3dr5c.cpp).  Called through its address rather than
// written as `new` so that the relocation lands on 0x10081222 and not on
// the CRT's vector-new thunk.
extern void* sub_10081222(uint32 nSize);

// 0x10080479 -- the CRT free reached as operator delete (d3dbatchc.cpp,
// d3dr5a_o2.cpp).
extern void sub_10080479(void* pMem);

// 0x10079f30 -- SIZE OF A SURFACE IN BYTES, (format, width, height).
// d3dhigh.cpp decodes it in full: formats 4/5/6 are the compressed ones,
// 4 at half a byte a pixel and 5/6 at one.
extern uint32 sub_10079f30(uint32 nFormat, uint32 nWidth, uint32 nHeight);

// 0x10021010 -- copy a DDPIXELFORMAT into a PFormat.  Stack ABI (its first
// two instructions are `mov 0x4(%esp),%eax ; mov 0x8(%esp),%edx`), cdecl
// (0x1003a9ce reserves eight bytes and 0x1003aa1f releases them).  Body:
// pFmt->Slot00(pPF->dwRGBBitCount == 16 ? 2 : 3, dwRGBAlphaBitMask,
//              dwRBitMask, dwGBitMask, dwBBitMask).
extern void sub_10021010(const DDPixelFormat* pPF, PFormat* pFormat);


// ======================================================================= //
//  0x1003a080  r_TransferTexture(RTexture*, TextureData*)   2,613 bytes
//
//  Register-argument entry (icl /Qip): %eax = pRTex, %edx = pData.
//  Returns 1 on success, 0 on every failure.
// ======================================================================= //
static long r_TransferTexture(RTexture* pRTex, TextureData* pData)
{
	// The ten frame objects, in the order the frame lays them out; see the
	// FRAME ARITHMETIC banner at the top -- their sizes sum to 0x380 with
	// no slack, which is what fixes sizeof(ConvertRequest) at 0x98.
	ConvertRequest  req;                                // ebp-0x380
	DDSurfaceDesc2  ddsd;                               // ebp-0x2e8
	DDSurf7T*     pSurf;                              // ebp-0x26c
	DDSurfaceDesc2  lockDesc;                           // ebp-0x268
	ConvertRequest  req2;                               // ebp-0x1ec
	DDSurfaceDesc2  tempDesc;                           // ebp-0x154
	DDSurf7T*     pTempSurf;                          // ebp-0x0d8
	DDSurfaceDesc2  tempLock;                           // ebp-0x0d4
	DDSCaps2        caps;                               // ebp-0x058

	D3DFormatRec*   pFmtRec;                            // ebp-0x44
	TextureMipData* pMip;                               // ebp-0x24
	uint8*          pTemp;
	uint8*          pDest;
	uint8*          pDstRow;
	uint8*          pSrc;
	uint8*          pDst;
	uint8*          pOut;
	int32           lPitch;                             // ebp-0x40
	uint32          nFormat;                            // ebp-0x0c
	uint32          nLevel;                             // ebp-0x10
	uint32          nWidth;                             // ebp-0x14
	uint32          nHeight;                            // ebp-0x18
	uint32          nMipWidth;                          // ebp-0x2c
	uint32          nMipHeight;                         // ebp-0x28
	uint32          nFourCC;
	uint32          nSize;
	uint32          x, y, j;
	int32           nTiles, k;
	int32           nC;
	long            hr;

	// ------------------------------------------------------------------ //
	//  REGION 1 -- 0x1003a080..0x1003a0ee.  Prologue, the request object,
	//  the mip-count sanity check and the format default.
	//
	//  WHAT PINS IT: the ctor call is `lea -0x380(%ebp),%ecx ;
	//  call 0x10079fd0` with nothing pushed -- a no-argument thiscall on a
	//  stack local, the first instruction of the function body.  The test
	//  is `movzbl 0x46 ; movzbl 0x47 ; movzwl 0x14 ; add ; cmp ; jle`, so
	//  the two RTexture fields are BYTES, the TextureData field is a WORD,
	//  and the comparison is SIGNED -- all three promote to int.  The
	//  format default is the sunk cold block at 0x1003aa86
	//  (`mov $3,%eax ; mov %eax,-0xc(%ebp) ; jmp 0x1003a0ef`), the /O2
	//  shape of `if (!x) x = 3;`.
	// ------------------------------------------------------------------ //
	req.Construct();

	if (pRTex->m_nFirstMip46 + pRTex->m_nMipCount47 > pData->m_nMipCount)
	{
		sub_10021d60(1, "r_TransferTexture: mipmap count doesn't match!");
		return 0;
	}

	nFormat = pData->m_nFormat;

	if (nFormat == 0)
		nFormat = 3;

	// ------------------------------------------------------------------ //
	//  REGION 2 -- 0x1003a0ef..0x1003a185.  Describe the top surface, log
	//  its size, and set up the mip walk.
	//
	//  WHAT PINS IT: `movl $0x7c,-0x2e8` is sizeof(DDSURFACEDESC2) and
	//  `movl $0x6,-0x2e4` is dwFlags = DDSD_HEIGHT|DDSD_WIDTH -- exactly
	//  the two fields the code then reads back (+0x08 -> nHeight,
	//  +0x0c -> nWidth).  The call is vtable +0x58, GetSurfaceDesc.  The
	//  two printf arguments go into 0x8(%esp) and 0xc(%esp) as
	//  (dwWidth, dwHeight), matching "(%dx%d)".  The loop bound is
	//  `cmp %edx,%eax ; jae` -- UNSIGNED, so nLevel is a uint32 -- and the
	//  mip base is `imul $0x18,%eax,%eax ; add %eax,%edi`, the 0x18-byte
	//  stride at TextureData+0xcc.
	// ------------------------------------------------------------------ //
	pFmtRec = g_1008e6b0[pRTex->m_nFormatIndex48];
	pSurf   = ((DDSurf7T*)pRTex->m_pSurface0c);

	ddsd.dwSize  = sizeof(DDSurfaceDesc2);
	ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
	pSurf->lpVtbl->GetSurfaceDesc(pSurf, &ddsd);

	nWidth  = ddsd.dwWidth;
	nHeight = ddsd.dwHeight;

	sub_10021d60(4, "Uploading a (%dx%d) texture", nWidth, nHeight);

	nLevel = pRTex->m_nFirstMip46;

	if (nLevel >= (uint32)(pRTex->m_nFirstMip46 + pRTex->m_nMipCount47))
		return 1;

	pMip = &pData->m_Mips[pRTex->m_nFirstMip46];

	for (;;)
	{
		// -------------------------------------------------------------- //
		//  REGION 3 -- 0x1003a186..0x1003a209.  Lock this level.
		//
		//  WHAT PINS IT: vtable +0x64 with five arguments
		//  (this, NULL, &desc, 0x20, NULL) is
		//  IDirectDrawSurface7::Lock, which d3dblit.cpp already records at
		//  that slot with that signature.  The desc's dwSize is planted
		//  and nothing else is, so it is not memset here (unlike the two
		//  in the compressed path, which ARE, with `rep stos` of 0x1f
		//  dwords = 124 bytes).
		// -------------------------------------------------------------- //
		if (!pSurf)
			return 0;

		lockDesc.dwSize = sizeof(DDSurfaceDesc2);

		nMipHeight = pMip->m_nHeight;
		nMipWidth  = pMip->m_nWidth;

		if (pSurf->lpVtbl->Lock(pSurf, LTNULL, &lockDesc,
		                        R_LOCK_FLAGS, LTNULL) != 0)
			return 0;

		lPitch = lockDesc.lPitch;

		// -------------------------------------------------------------- //
		//  REGION 4 -- 0x1003a20a..0x1003a333 and 0x1003a334..0x1003a427.
		//  DDPF_BUMPDUDV: convert the mip to eight bits into a scratch
		//  buffer and differentiate it into the surface as V8U8.
		//
		//  WHAT PINS IT:
		//   * 0x80000 tested on lockDesc.ddpfPixelFormat.dwFlags is
		//     DDPF_BUMPDUDV, and the very next test is dwRGBBitCount
		//     (+0x54 of the desc = +0x0c of the pixel format) against 16 --
		//     a two-bytes-per-texel bump surface;
		//   * the scratch buffer is height*width bytes and the request's
		//     destination pitch is the WIDTH, so the intermediate is one
		//     byte per texel; its destination format is set up by
		//     Slot00(1, 0xff, 0, 0, 0) -- type 1, a single 0xff channel;
		//   * `movzbl (%edx)` / `movzbl -0x1(%edx)` / `movzbl (%edx-w)`
		//     with two `sar $1` and two BYTE stores two bytes apart is a
		//     central-difference into a V8U8 pair, and the row and column
		//     zeroed first are the x=0 and y=0 edges the difference cannot
		//     reach;
		//   * the zero-fill of the first row is `width*2` bytes
		//     (`add %ecx,%ecx`) and the column store is 16-bit
		//     (`mov %cx,(%edx)`) -- two bytes a texel again.
		// -------------------------------------------------------------- //
		if (lockDesc.ddpfPixelFormat.dwFlags & DDPF_BUMPDUDV)
		{
			if (lockDesc.ddpfPixelFormat.dwRGBBitCount != 16)
				goto bumpfail;

			pDest = (uint8*)lockDesc.lpSurface;

			req2.Construct();

			pTemp = (uint8*)sub_10081222(pMip->m_nHeight * pMip->m_nWidth);

			if (!pTemp)
				goto bumpfail;

			// slot 0 of the engine texture fills in the SOURCE format.
			pData->Slot00(req2.m_pSrcFormat);

			req2.m_pSrcData  = pMip->m_pData;
			req2.m_nSrcPitch = pMip->m_nPitch;

			req2.m_pDstFormat->Slot00(1, 0xff, 0, 0, 0);

			req2.m_pDstData  = pTemp;
			req2.m_nDstPitch = pMip->m_nWidth;
			req2.m_nWidth    = pMip->m_nWidth;
			req2.m_nHeight   = pMip->m_nHeight;

			if (g_1008daa0.ConvertPixels(&req2) != 0)
			{
				sub_10080479(pTemp);
				goto bumpfail;
			}

			memset(pDest, 0, pMip->m_nWidth * 2);

			pDst = pDest;

			for (j = 0; j < pMip->m_nHeight; j++)
			{
				*(uint16*)pDst = 0;
				pDst += lPitch;
			}

			pDstRow = pDest;

			for (y = 1; y < pMip->m_nHeight; y++)
			{
				pSrc = pTemp + pMip->m_nWidth * y + 1;
				pOut = pDstRow;

				for (x = 1; x < pMip->m_nWidth; x++)
				{
					nC = *pSrc;

					// COIN-FLIP: the two byte stores are emitted [1] then
					// [0] (`mov %dl,0x1(%ebx)` at 0x1003a3e0 before
					// `mov %al,(%ebx)` at 0x1003a3e3) and the two
					// subtractions in the same order, so they are written
					// that way here.  The alternative spelling is the
					// natural du-then-dv order:
					//     pOut[0] = (uint8)((nC - pSrc[-1]) >> 1);
					//     pOut[1] = (uint8)((nC - pSrc[-(int32)pMip->m_nWidth]) >> 1);
					// One build settles which one icl emits in this order.
					pOut[1] = (uint8)((nC - pSrc[-(int32)pMip->m_nWidth]) >> 1);
					pOut[0] = (uint8)((nC - pSrc[-1]) >> 1);

					pOut += 2;
					pSrc++;
				}

				pDstRow += lPitch;
			}

			sub_10080479(pTemp);

			if (nFormat == 3)
				goto tile;

			goto next;

		bumpfail:
			// 0x1003a307.  ONE Unlock, reached from the bit-count test,
			// the allocation test and the conversion test alike -- three
			// predecessors of one block, which is why this is a label and
			// not three copies.
			pSurf->lpVtbl->Unlock(pSurf, LTNULL);
			return 0;
		}

		// -------------------------------------------------------------- //
		//  REGION 5 -- 0x1003a4d3..0x1003a526.  The format dispatch.
		//
		//  WHAT PINS IT: four `cmp $N,%eax ; je` in the order 3, 4, 5, 6
		//  with a fall-through failure tail, and the three FourCCs planted
		//  by the 4/5/6 arms are 0x31545844, 0x33545844 and 0x35545844 --
		//  'DXT1', 'DXT3', 'DXT5' little-endian.
		//
		//  WHY GOTOs AND NOT A SWITCH.  The comparison order in the bytes
		//  is 3,4,5,6 (source order) but the BLOCK order is 6 (inline at
		//  0x1003a670), then 5, 4 and 3 sunk to 0x1003a999, 0x1003a9b0 and
		//  0x1003a9c0, and the 0x10079f30 call in the compressed arm is
		//  reached BEFORE the three calls in the format-3 arm.  Written as
		//  `switch (nFormat) { case 3: ... case 4: ... }` the call sequence
		//  comes out with 0x10079f30 last instead of before 0x1007a3b0 --
		//  one shipped-only and one ours-only row in the call sequence, nothing else.
		//  The goto chain here reproduces both the comparison order and
		//  the call order, and is what the bytes actually are.
		// -------------------------------------------------------------- //
		if (nFormat == 3)
			goto convert;

		if (nFormat == 4)
		{
			nFourCC = 0x31545844;                       // 'DXT1'
			goto compressed;
		}

		if (nFormat == 5)
		{
			nFourCC = 0x33545844;                       // 'DXT3'
			goto compressed;
		}

		if (nFormat == 6)
		{
			nFourCC = 0x35545844;                       // 'DXT5'
			goto compressed;
		}

		pSurf->lpVtbl->Unlock(pSurf, LTNULL);
		return 0;

		// -------------------------------------------------------------- //
		//  REGION 6 -- 0x1003a678..0x1003a6f3 and 0x1003a700..0x1003a998.
		//  A compressed level.
		//
		//  WHAT PINS IT:
		//   * the size call is `(nFormat, pMip->m_nWidth, pMip->m_nHeight)`
		//     into 0x10079f30, which d3dhigh.cpp decodes as the
		//     size-of-a-surface helper -- and this function passes it the
		//     SAME format number it just switched on, which is the
		//     cross-check on the DXT numbering;
		//   * the fast path tests ddsd (the TOP surface's) pixel format for
		//     DDPF_FOURCC (0x4) and then compares BOTH the FourCC (+0x50 of
		//     ddsd) and the size against lockDesc.lPitch -- for a
		//     compressed surface lPitch is the linear size -- and both
		//     `jne`s go to ONE block, so the source is a single `||`;
		//   * the slow path builds a SYSTEM-MEMORY (DDSCAPS 0x1800 =
		//     DDSCAPS_TEXTURE|DDSCAPS_SYSTEMMEMORY) surface whose
		//     lpSurface IS the mip's own bytes, over dwFlags 0x1007 =
		//     DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT, then
		//     BltFast(0, 0, temp, NULL, 0x10 = DDBLTFAST_WAIT) it in;
		//   * `movl $0x20,-0x10c` is sizeof(DDPIXELFORMAT) at the desc's
		//     +0x48, which is what fixes the pixel format's position;
		//   * the DDPF_FOURCC store is a READ-MODIFY-WRITE
		//     (`mov -0x108,%edx ; or $0x4,%edx ; mov %edx,-0x108`), so the
		//     source is `|=` and not `=`;
		//   * the slow path Unlocks the real surface before creating the
		//     temporary and RE-LOCKS it after the blit, because the shared
		//     tail below unconditionally Unlocks.  That relock is the only
		//     reason the sequence balances.
		// -------------------------------------------------------------- //
	compressed:
		nSize = sub_10079f30(nFormat, pMip->m_nWidth, pMip->m_nHeight);

		if (ddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
		{
			if (nFourCC != ddsd.ddpfPixelFormat.dwFourCC ||
			    nSize != (uint32)lockDesc.lPitch)
			{
				pSurf->lpVtbl->Unlock(pSurf, LTNULL);
				return 0;
			}

			memcpy(lockDesc.lpSurface, pMip->m_pData, nSize);
			goto next;
		}

		pSurf->lpVtbl->Unlock(pSurf, LTNULL);

		memset(&tempDesc, 0, sizeof(tempDesc));

		tempDesc.dwSize   = sizeof(DDSurfaceDesc2);
		tempDesc.dwFlags  = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
		                    DDSD_PIXELFORMAT;
		tempDesc.dwHeight = pMip->m_nHeight;
		tempDesc.dwWidth  = pMip->m_nWidth;
		tempDesc.lPitch   = nSize;
		tempDesc.lpSurface = pMip->m_pData;

		tempDesc.ddpfPixelFormat.dwSize   = sizeof(DDPixelFormat);
		tempDesc.ddpfPixelFormat.dwFlags |= DDPF_FOURCC;
		tempDesc.ddpfPixelFormat.dwFourCC = nFourCC;

		tempDesc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;

		if (g_pDirectDraw->lpVtbl->CreateSurface(g_pDirectDraw, &tempDesc,
		                                         &pTempSurf, LTNULL) != 0)
			return 0;

		memset(&tempLock, 0, sizeof(tempLock));

		tempLock.dwSize = sizeof(DDSurfaceDesc2);

		if (pTempSurf->lpVtbl->Lock(pTempSurf, LTNULL, &tempLock,
		                            R_LOCK_FLAGS_TEMP, LTNULL) != 0)
		{
			pTempSurf->lpVtbl->Release(pTempSurf);
			return 0;
		}

		if (nSize != (uint32)tempLock.lPitch)
		{
			pTempSurf->lpVtbl->Unlock(pTempSurf, LTNULL);
			pTempSurf->lpVtbl->Release(pTempSurf);
			return 0;
		}

		memcpy(tempLock.lpSurface, pMip->m_pData, nSize);

		pTempSurf->lpVtbl->Unlock(pTempSurf, LTNULL);

		hr = pSurf->lpVtbl->BltFast(pSurf, 0, 0, pTempSurf, LTNULL,
		                            DDBLTFAST_WAIT);

		pTempSurf->lpVtbl->Release(pTempSurf);

		if (hr != 0)
			return 0;

		lockDesc.dwSize = sizeof(DDSurfaceDesc2);

		if (pSurf->lpVtbl->Lock(pSurf, LTNULL, &lockDesc,
		                        R_LOCK_FLAGS, LTNULL) != 0)
			return 0;

		goto next;

		// -------------------------------------------------------------- //
		//  REGION 7 -- 0x1003a9c0..0x1003aa85.  Format 3: convert the
		//  32-bit source into the surface's own pixel format.
		//
		//  WHAT PINS IT: 0x1007a3b0 is called on the request's SOURCE
		//  format (`mov -0x380(%ebp),%ecx` -- the value at +0x00, not the
		//  address of the object) and d3dr4c.cpp reads that function as
		//  Slot00(3, 0xff000000, 0xff0000, 0xff00, 0xff), 32-bit ARGB.
		//  0x10021010 is then handed `pFmtRec + 0x0c` and the request's
		//  DESTINATION format, and its body copies a DDPIXELFORMAT's five
		//  fields into a Slot00 call -- so the destination format is the
		//  one the surface-format table names.  m_nUnknown24 is the only
		//  request field written here that the 8-bit path leaves alone.
		// -------------------------------------------------------------- //
	convert:
		req.m_pSrcFormat->InitDefault1007a3b0();

		req.m_pSrcData  = pMip->m_pData;
		req.m_nSrcPitch = pMip->m_nPitch;

		sub_10021010((const DDPixelFormat*)&pFmtRec->m_dwSize, req.m_pDstFormat);

		req.m_pDstData   = lockDesc.lpSurface;
		req.m_nDstPitch  = lPitch;
		req.m_nWidth     = pMip->m_nWidth;
		req.m_nHeight    = pMip->m_nHeight;
		req.m_nUnknown24 = 0;

		if (g_1008daa0.ConvertPixels(&req) != 0)
		{
			pSurf->lpVtbl->Unlock(pSurf, LTNULL);
			return 0;
		}

		// -------------------------------------------------------------- //
		//  REGION 8 -- 0x1003a527..0x1003a66a.  Tile a small mip across a
		//  larger surface.  Reached from the format-3 arm (unconditionally)
		//  and from the bump arm (only when nFormat == 3).
		//
		//  WHAT PINS IT:
		//   * two `div`s, `nWidth / nMipWidth` and `nHeight / nMipHeight`
		//     -- UNSIGNED division whose quotient is then compared
		//     `cmp $1,%eax ; jle` and stepped with `jl`, so the counts are
		//     ints and the operands uint32;
		//   * the X copy walks `nMipWidth << pFmtRec->m_nPixelShift` bytes
		//     a row and the Y copy walks `nWidth << pFmtRec->m_nPixelShift`
		//     -- one tile wide against the whole row, which is the giveaway
		//     that X runs first and Y replicates the finished rows;
		//   * the X destination is
		//     `lpSurface + nMipWidth * pFmtRec->m_nPixelSize * k`
		//     (`imul 0x30(%eax),%edi ; imul %edx,%edi ; add %esi,%edi`) and
		//     the Y destination is `lpSurface + lPitch * k * nMipHeight`.
		//     The Y one is emitted through an induction variable -- a slot
		//     initialised to lPitch before the loop and incremented by
		//     lPitch at 0x1003a637 -- which is exactly a strength-reduced
		//     `lPitch * k` with k starting at 1.  The X one is NOT reduced
		//     because pFmtRec->m_nPixelSize is reloaded inside it, and that
		//     asymmetry is what says the two really are the two different
		//     expressions written below and not one shared helper.
		//   * both use pFmtRec->m_nPixelShift for the byte count and the X
		//     one uses pFmtRec->m_nPixelSize for the offset: the log2 and
		//     the value of the same bytes-per-pixel, both present in the
		//     record and both used.
		// -------------------------------------------------------------- //
	tile:
		nTiles = nWidth / nMipWidth;

		for (k = 1; k < nTiles; k++)
		{
			pSrc = (uint8*)lockDesc.lpSurface;
			pDst = (uint8*)lockDesc.lpSurface +
			       nMipWidth * pFmtRec->m_nPixelSize * k;

			for (j = 0; j < nMipHeight; j++)
			{
				memcpy(pDst, pSrc, nMipWidth << pFmtRec->m_nPixelShift);
				pSrc += lPitch;
				pDst += lPitch;
			}
		}

		nTiles = nHeight / nMipHeight;

		for (k = 1; k < nTiles; k++)
		{
			pSrc = (uint8*)lockDesc.lpSurface;
			pDst = (uint8*)lockDesc.lpSurface + lPitch * k * nMipHeight;

			for (j = 0; j < nMipHeight; j++)
			{
				memcpy(pDst, pSrc, nWidth << pFmtRec->m_nPixelShift);
				pSrc += lPitch;
				pDst += lPitch;
			}
		}

		// -------------------------------------------------------------- //
		//  REGION 9 -- 0x1003a434..0x1003a4c2.  Unlock, step to the next
		//  attached mip surface, halve the size, and go round.
		//
		//  WHAT PINS IT: the DDSCAPS2 is (0x401000, 0, 0, 0) =
		//  DDSCAPS_MIPMAP|DDSCAPS_TEXTURE, and vtable +0x30 with
		//  (this, &caps, &pSurf) is GetAttachedSurface -- the out-pointer
		//  is the SAME local the loop reads at its top, which is what makes
		//  this a chain walk rather than a fresh lookup.  The two sizes are
		//  stepped with `shr $1` (UNSIGNED) and the level with `inc`, and
		//  the bound is recomputed from the RTexture bytes every iteration
		//  (`movzbl 0x46 ; movzbl 0x47 ; add`), not cached.
		// -------------------------------------------------------------- //
	next:
		pSurf->lpVtbl->Unlock(pSurf, LTNULL);

		caps.dwCaps  = DDSCAPS_MIPMAP | DDSCAPS_TEXTURE;
		caps.dwCaps2 = 0;
		caps.dwCaps3 = 0;
		caps.dwCaps4 = 0;

		pSurf->lpVtbl->GetAttachedSurface(pSurf, &caps, &pSurf);

		nLevel++;
		pMip++;

		nHeight >>= 1;
		nWidth  >>= 1;

		if (nLevel >= (uint32)(pRTex->m_nFirstMip46 + pRTex->m_nMipCount47))
			return 1;
	}
}


// *********************************************************************** //
//  WHAT THE CALL SEQUENCE DOES *NOT* WITNESS  (quoted here so the figure is
//  never quoted without it)
//
//  The 12 direct calls are expected to line up 12 of 12, in order: the
//  source above is written so that the goto chain in REGION 5 puts
//  0x10079f30 before 0x1007a3b0, which is where the shipped block layout
//  puts it.  The 20 indirect calls are expected to come back 20 against 20
//  with several rows displaced, ALL indirect -- the /O2 cold-block sinking
//  also seen at 0x10061a30.  The predicted displacements, in
//  advance: 0x1003a450/0x1003a487 (the shared tail, sunk ABOVE the failure
//  arms in the shipped layout but written last here) and the four failure
//  Unlock/Release blocks at 0x1003a91c, 0x1003a946, 0x1003a973 and
//  0x1003a985, which the shipped code parks after the last hot exit.
//
//  CALL-FREE REGIONS -- 1,215 of 2,613 bytes (47%) sit in stretches of 100
//  bytes or more with no call in them.  The two that matter:
//
//    0x1003a510..0x1003a698   392 bytes.  The whole tiling block
//        (0x1003a527..0x1003a66a) plus the three FourCC arms.  The call
//        sequence cannot see any of it.  What pins it instead: the two `div`s name
//        their operands (surface size over mip size, one per axis); the
//        two copy lengths are DIFFERENT expressions (mip width vs surface
//        width, both shifted by pFmtRec->m_nPixelShift), which is what
//        orders the two loops; the Y destination is a strength-reduced
//        induction variable initialised to lPitch and stepped by lPitch,
//        which is `lPitch * k` with k from 1 and nothing else; and the
//        three FourCCs are literal ASCII cross-checked against
//        d3dhigh.cpp's independent reading of 0x10079f30.
//
//    0x1003a31d..0x1003a420   259 bytes.  The V8U8 differentiation.  What
//        pins it: an instruction census with no residue.  THREE byte loads
//        per inner iteration (`movzbl (%edx)`, `movzbl -0x1(%edx)`,
//        `movzbl (%edi)` with `%edi = %edx - width`) -- centre, left, up;
//        TWO `sub`s and TWO `sar $1`s -- two signed halved differences;
//        TWO byte stores one byte apart (`mov %dl,0x1(%ebx)`,
//        `mov %al,(%ebx)`) and `add $0x2,%ebx` -- a two-byte texel;
//        ONE 16-bit store in the column loop (`mov %cx,(%edx)`) and ONE
//        `add %ecx,%ecx` on the row memset -- the same two bytes again;
//        both loops start their counter at 1 and both are guarded by
//        `cmp $1 ; jbe`.  A third difference, a one-byte destination, a
//        forward difference instead of a backward one, or a loop starting
//        at 0 would each move at least two of those rows.
//
//  Everything else in the function is within 171 bytes of a call.
//
//  NOTHING IS LEFT UNDECODED.  There is no `// UNDECODED:` banner in this
//  file: every instruction in 0x1003a080..0x1003aab4 is accounted for by
//  one of the nine regions above.
// *********************************************************************** //


// ======================================================================= //
//  0x10038a90 (r_CreateRTexture) -- MOVED HERE from
//  big38a90.cpp, whose banner follows.  A /Qip REGISTER-ONLY entry (the
//  request in %eax, the flags in %edx, no shim), and its only caller is
//  0x10039bdc in this file: a static of this TU.
// ======================================================================= //
// *********************************************************************** //
//
//  MODULE:   BIG38A90 (.CPP)  --  one large function from d3d.ren
//
//  TARGET:   0x10038a90 .. 0x10038f42 inclusive (1,203 bytes), followed by
//            thirteen bytes of lea-nop padding at 0x10038f43..0x10038f4f
//            and the NEXT function at 0x10038f50.
//
//  STANDARD: STRUCTURAL, not byte-exact.
//            Every offset, constant, branch and vtable slot below was read
//            off the retail d3d.ren.  Nothing is guessed; the coin-flips that
//            could not be settled are called out in comments.
//
// ----------------------------------------------------------------------- //
//  EXTENT CHECK -- THE STATED EXTENT IS WRONG.  IT SPANS TWO FUNCTIONS.
//
//  The row said "1,664 bytes, stated end 0x10039110".  The start is
//  right; the end is 462 bytes into the FOLLOWING function.
//
//  * TWO PROLOGUES in the stated range.  `55 8b ec` + `81 ec <imm32>` at
//    0x10038a90 (`sub $0x80,%esp`) and again at 0x10038f50
//    (`sub $0xb8,%esp`).  Two different frame sizes: two functions.
//  * THE PADDING BETWEEN THEM IS THE COMPILER'S ALIGNMENT PADDING, not
//    code: 0x10038f43 `8d b6 00 00 00 00` (lea 0x0(%esi),%esi) and
//    0x10038f49 `8d bc 27 00 00 00 00` (lea 0x0(%edi,%eiz,1),%edi), six
//    plus seven bytes taking 0x10038f43 to the 16-byte boundary 0x10038f50.
//    The same two-lea pad appears at 0x10038623, 0x100374c5 and elsewhere.
//  * NO BRANCH inside 0x10038a90..0x10038f42 targets 0x10038f43 or beyond.
//    The furthest forward target is 0x10038f2b, and every one of the six
//    sunk cold blocks (0x10038cde, 0x10038e81, 0x10038ea5, 0x10038ed5,
//    0x10038edf, 0x10038eed, 0x10038ef4, 0x10038efb, 0x10038f05,
//    0x10038f2b) either jumps BACK inside or ends in the function's own
//    epilogue.  Nothing in 0x10038f50.. is reachable from here.
//  * THE STARTS ARE BOTH REAL AND SEPARATELY REFERENCED.  A full scan of
//    .text for `e8`/`e9` rel32 displacements landing on either address
//    finds exactly one caller of 0x10038a90 -- 0x10039c2f -- and NONE of
//    0x10038f50; 0x10038f50 is taken by ADDRESS instead (the immediate at
//    raw 0x3ae61, which d3dr3d.cpp already records: 0x1003ad30 hands
//    0x10038f50 to sub_1006d460 as its allocator callback).  A function
//    whose address is taken as a callback is not an interior block of the
//    one above it.
//  * THE TWO HAVE DIFFERENT CALLING CONVENTIONS.  0x10038a90 reads %eax
//    and %edx before touching the stack (register entry, icl /Qip) and its
//    caller reserves only 8 bytes; 0x10038f50 reads 0x8(%ebp), 0xc(%ebp)
//    and 0x10(%ebp) -- three ordinary stack arguments, which is what
//    d3dr3d.cpp's decode of sub_1006d460 says its callback takes.
//  * 0x10038a8e, immediately before our start, is `8b f6` (mov %esi,%esi),
//    the 2-byte alignment pad after the previous function's `ret` at
//    0x10038a8d.  0x10038a90 is a genuine function start, not an interior
//    /Qip register entry: the byte before it is padding, not a fall-through
//    stack entry.
//  * SIX `ret`s in 0x10038a90..0x10038f42, at 0x10038cad, 0x10038cc5,
//    0x10038cdd, 0x10038e80, 0x10038ea4 and 0x10038f42.  All six are the
//    SAME epilogue byte for byte:
//        8b 1c 24 / 8b 74 24 04 / 8b 7c 24 08 / 8b e5 / 5d / c3
//    matching the prologue's register saves at 0x10038a99, and all six are
//    preceded by `movl $0x100894d0,-0x58(%ebp)` -- five of them with
//    `xor %eax,%eax` (return 0) and one, 0x10038e69, with `mov %esi,%eax`
//    (return the record).  Duplicated epilogues are /O2, not six functions.
//
//  VERDICT: ONE function, 0x10038a90..0x10038f42 inclusive, 1,203 bytes --
//  461 bytes FEWER than stated (0x10039110 - 0x10038f43 = 0x1cd), because
//  the stated end reached 462 bytes into its neighbour.
//  0x10038f50..0x10039107 is a DIFFERENT function and is NOT written here.
//
//  Call sequence over 0x10038a90..0x10038f43:
//      1,203 bytes, 5 calls (4 direct, 1 indirect), 241 bytes per call
//
//  4 DISTINCT direct callees over 4 direct calls -- 0x100385e0, 0x10038630,
//  0x1003abe4, 0x1007fbf0 -- plus one indirect COM Release whose POSITION
//  the check can still compare.  241 B/call is in the WEAK band that
//  makes the call sequence a sanity check here and
//  NOT the evidence.  What pins this function is listed under EVIDENCE.
//
// ----------------------------------------------------------------------- //
//  WHAT IT IS: r_CreateRTexture -- build the renderer-side RTexture record
//  for one engine TextureData, and hang it off the shared texture.
//
//  It is the second half of the module's texture-binding path.  Its one
//  caller, 0x10039bdc (the body of d3dmid.cpp's 0x10039bd0, which
//  big6d910.cpp declares as
//      Tex6d910* sub_10039bd0(TexBlock6d910*, uint32 nStage, uint32 bLink)
//  ), does exactly this:
//
//      g_pStruct->Slot08(pBlock, &nOutData)   -- GetTextureData
//      if (!pData) return NULL
//      info = { pBlock, pData, nStage }       -- a 3-dword stack record
//      pRec = <THIS FUNCTION>(&info, bLink)   -- %eax = &info, %edx = bLink
//      if (!pRec) return NULL
//      if (r_TransferTexture(pRec, pData))    -- 0x1003a080, big3a080.cpp
//          ... on failure tear it down ...
//
//  So this function CHOOSES the surface format, CHOOSES which mip level to
//  start at and how many to take, asks 0x10038630 to create the DirectDraw
//  surface chain, allocates the RTexture out of the module's 0x4c-byte
//  bank at 0x1008e644, fills it in and links it into the LRU list at
//  0x1008e634.  0x1003a080 then uploads the pixels into the surfaces.
//
// ----------------------------------------------------------------------- //
//  EVIDENCE -- what pins the decode, given the call sequence cannot.
//
//  1. THE RECORD IS ALREADY DECODED, TWICE, ELSEWHERE.  Every field
//     this function stores was independently named from the CONSUMER side:
//
//       +0x04/+0x08  big6d910.cpp  m_fInvW04 / m_fInvH08 -> g_a1008e664[]
//       +0x0c        big3a080.cpp  m_pSurface  (RTexture::m_pSurface)
//       +0x10        big6d910.cpp  m_nBytes10, added to g_p1008cbcc->+0x48
//       +0x14        big6d910.cpp  m_nStamp14, compared with g_w1008bea8
//       +0x16        big6d910.cpp  m_nAlphaRef16 -> D3DRENDERSTATE 0x18
//       +0x1c..+0x24 big6d910.cpp  m_Link1c, an LTLink threaded on
//                                  g_1008e634 with m_pData at +0x24
//       +0x28/+0x2a  big6d910.cpp  m_n28 / m_n2a, returned by vtable slots
//       +0x30        big6d910.cpp  m_pNext30, the per-block chain
//       +0x42        big6d910.cpp  m_nStage42, the texture stage
//       +0x46/+0x47  big3a080.cpp  m_nFirstMip / m_nMipCount
//       +0x48        big3a080.cpp  m_nFormatIndex, indexes g_1008e6b0[]
//       +0x34/+0x38/+0x3c  big6d910.cpp m_f34/m_f38/m_f3c, the UV transform
//
//     None of those offsets was chosen here; they were fixed before this
//     function was opened, and every single store here lands on one of
//     them.  A wrong decode would have to hit fourteen pre-existing
//     offsets by accident.
//
//  2. THE VTABLE SLOTS CONFIRM THE SELF-POINTER AT +0x18.  0x100894f0 is
//     the record's vtable (d3dbatchd.cpp: 0x1003abe4 installs it).  Its
//     slots are 0x100374a0 (dtor), 0x1003aba0 (`return 1`), 0x1003abb0
//     (`return this->p18->w40`), 0x1003abc0 (`... ->w28`), 0x1003abd0
//     (`... ->w2a`).  This function stores `mov %esi,0x18(%esi)` -- the
//     record points +0x18 at ITSELF -- and stores w28, w2a and w40.  The
//     three accessors and the three stores match one for one.  That is not
//     a coincidence available to a wrong decode.
//
//  3. THE DESTRUCTOR IS ON EVERY EXIT.  All six return paths execute
//     `movl $0x100894d0,-0x58(%ebp)` before the epilogue -- including the
//     three that return 0 without ever having filled the local.  0x100894d0
//     is the BASE vtable (its slots 1..4 are 0x10080905, the purecall
//     thunk) and 0x100374a0, the destructor, stores 0x100894d0 into
//     `(%ecx)`.  A store of a base vtable into a stack slot on every single
//     exit is an inlined destructor and nothing else; d3dr2c_os.cpp reads
//     0x10037550's identical store the same way.  That fixes ebp-0x58 as a
//     local OBJECT with a lifetime spanning the whole function, which in
//     turn fixes the seven-dword clear at 0x10038b14 as a memset of that
//     object and the 0x100894f0 store at 0x10038aa4 as its constructor.
//
//  4. THE FRAME DECOMPOSES.  `sub $0x80,%esp` = 128 bytes.  The bottom
//     twelve are the register save area (`mov %edi,0x8(%esp) ;
//     %esi,0x4(%esp) ; %ebx,(%esp)`, restored by all six epilogues), so
//     ebp-0x80..ebp-0x75 is not local storage.  Every ebp-relative access
//     in the function -- all enumerated -- is one of exactly nine
//     slots:
//        -0x58  RTexInfo info      0x1c   (7 dwords cleared, +0x00..+0x18)
//        -0x38  int32   nAngle     0x04   (fildl source)
//        -0x24  uint32  nFlags     0x04   (argument 2)
//        -0x20  uint32  nFormat    0x04
//        -0x1c  int32   nMipCount  0x04   (shares the slot with nBias)
//        -0x18  TexRef* pRef       0x04   (argument 1)
//        -0x14  int32   nFirstMip  0x04
//        -0x10  double  fTheta     0x08   (fstpl/fldl around fcos/fsin)
//        -0x08  uint8*  spill      0x04   (pData across the byte-count loop)
//     Nothing touches -0x74..-0x59, -0x34..-0x25, -0x0c or -0x04.  The
//     0x1c at -0x58 is what fixes sizeof(RTexInfo): the clear writes
//     offsets 0x00,0x04,0x08,0x0c,0x10,0x14,0x18 and stops, and 0x1c is
//     also exactly the span of record fields the copy at 0x10038d2f
//     reproduces (+0x04,+0x08,+0x0c,+0x10, then the two WORDS +0x14 and
//     +0x16).  Two independent readings of the same number.
//
//  5. THE MIP ARITHMETIC IS CLOSED.  Three clamps, each read off its own
//     branch pair, and every one of the six sunk cold blocks is one arm of
//     one of them:
//        0x10038ef4 / 0x10038eed   LTCLAMP(nMipBase, 0, 3)
//        0x10038edf                LTCLAMP(nBias, 0, nMipCount14 - 1)
//        0x10038c58 / 0x10038c66   LTCLAMP(nCount, 1, nAvail)
//     plus 0x10038ed5 (`nCount = 4` when the byte is zero), 0x10038efb
//     (`nFmt = 3` when the byte is zero) and 0x10038f05 (the format-0
//     precondition).  There is no residue: every cold block below the last
//     hot exit is accounted for, and every one of them jumps back to a
//     named join point.
//
//  6. THE STRIDE IS 0x18 IN BOTH LOOPS AND MATCHES big3a080.  The search
//     loop steps `add $0x18,%eax` over `0xcc(%ebx,%eax,1)` /
//     `0xd0(%ebx,%eax,1)`; the byte-count loop does `imul $0x18` twice and
//     `add $0x18,%eax` over `0xcc(%eax)` / `0xd0(%eax)`.  big3a080.cpp
//     fixed TextureMipData at 0x18 bytes based at TextureData+0xcc with
//     width at +0x00 and height at +0x04, from a different function.
//
// ----------------------------------------------------------------------- //
//  FLAG VERDICT: /O2.
//    * six duplicated epilogues, byte for byte, no cross-jumped exits
//      (0x10038cad, 0x10038cc5, 0x10038cdd, 0x10038e80, 0x10038ea4 and
//      0x10038f42) -- /Os would have cross-jumped them;
//    * every outgoing argument block is `add $-N,%esp` then `mov`:
//      0x10038b54 (`add $0xfffffff8`), 0x10038c6e (`add $0xffffffe8` then
//      four `mov`s into 0xc(%esp)/0x10(%esp)), 0x10038ea5 (`add
//      $0xfffffff8` then `mov %edx,0x4(%esp) ; movl $..,(%esp)`).  The one
//      `push` in the function, 0x10038e84, is the single-dword outgoing
//      area for the COM Release and is written through `mov %edx,(%esp)`
//      immediately after, which is the same /O2 shape;
//    * the cold blocks are SUNK below the last hot exit -- ten of them,
//      0x10038cde..0x10038f2b;
//    * `return 0` is `xor %eax,%eax` at all five failure exits, and there
//      is no `xor/inc` anywhere.
//  ONE SHAPE ARGUES THE OTHER WAY and is recorded as such: `&pRec->m_Link1c`
//  at 0x10038e53 is `lea 0x1c(%esi),%edx`, where the /O2 pattern has
//  `mov $0x1c,%reg ; add %esi,%reg`.  Against six duplicated
//  epilogues that is not enough to overturn /O2; big6d910.cpp's copy of
//  the same AddAfter block is in a file this project already builds.
//
// *********************************************************************** //

#include <math.h>



// ======================================================================= //
//  TYPES
//
//  Nothing in LT2\sdk\inc declares these and d3dr4b.cpp includes only
//  "ltbasedefs.h", so they are spelled out here at the offsets the bytes
//  give.  Where a shape already exists in src/engine/d3d it is REUSED, not
//  re-derived, and the file it came from is named.
// ======================================================================= //

// ----------------------------------------------------------------------- //
//  IDirectDrawSurface7.  Only Release is reached from here: 0x10038e8a is
//  `mov (%edx),%ecx ; mov %edx,(%esp) ; call *0x8(%ecx)` -- slot +0x08 with
//  the interface itself as the only argument, which big3a080.cpp and
//  big331e0.cpp both already record as Release.
// ----------------------------------------------------------------------- //


// ----------------------------------------------------------------------- //
//  TextureMipData / TextureData -- big3a080.cpp's shapes, extended with
//  the six fields THIS function reads and nothing else.
//
//  The extension offsets, each read off an instruction here:
//    +0x18  dword flags   `mov 0x18(%ebx),%eax` then test $0x2 / $0x100 /
//                         $0x80 / $0x1 (0x10038ad3..0x10038b00) and
//                         test $0x1 again at 0x10038e22
//    +0x20  byte          `movzbl 0x20(%ebx),%eax`, clamped to <= 9 and
//                         used to index a ten-dword table
//    +0x21  byte          `movzbl 0x21(%ebx),%eax`, the requested mip count
//    +0x22  byte          `movzbl 0x22(%ebx),%eax` -- big3a080.cpp's
//                         m_nFormat, and it applies the SAME `if (!x) x = 3`
//                         default to it that this function does
//    +0x23  byte          `movzbl 0x23(%ebx),%eax`, an extra mip bias added
//                         only when the compressed format is unavailable
//    +0x26  float         `fadds 0x26(%ebx)`
//    +0x2a  byte          )  assembled by hand into a signed 16-bit angle:
//    +0x2b  byte          )  `movzbl 0x2b ; movzbl 0x2a ; shl $8 ; add ;
//                            movswl` -- TWO byte loads and a shift, not a
//                            `movswl 0x2a(%ebx)`, so the source really does
//                            combine two bytes rather than read a short.
// ----------------------------------------------------------------------- //
                                                      // 0x18


// TextureData::m_nFlags18, from the four tests at 0x10038ad6..0x10038b00
// and the fifth at 0x10038e22.  Only the bit VALUES are read off the
// bytes; the names are mine and are marked as such.
#define TD_FLAG_01                      0x00000001      // -> record +0x40
#define TD_FLAG_NOTCOMPRESSED_02        0x00000002
#define TD_FLAG_80                      0x00000080
#define TD_FLAG_100                     0x00000100


// ----------------------------------------------------------------------- //
//  The 0x1c-byte local the surface builder fills in.
//
//  It carries a vtable pointer at +0x00 -- 0x100894f0 on construction
//  (0x10038aa4) and 0x100894d0 on destruction (all six exits) -- and
//  0x100894f0 is the SAME vtable 0x1003abe4 stamps into the heap record.
//  The two are therefore the same class or a base/derived pair sharing it;
//  these bytes do not tell which and it does not matter here,
//  because everything this function does with the local is (a) construct
//  it, (b) memset it, (c) hand its address to 0x10038630, (d) copy six
//  fields out of it, (e) Release its +0x0c on one failure path and
//  (f) destroy it.
//
//  It is spelled with the vtable as a PLAIN DATA MEMBER and with the two
//  ends written by hand, which is d3dr2c_os.cpp's house solution for
//  exactly this: a real `virtual` would make icl lay out a vtable of its
//  own and the store would not be 0x100894f0.
//
//  THE MEMSET CLOBBERS THE VTABLE POINTER AND THAT IS WHAT THE BYTES SAY.
//  0x10038aa4 stores 0x100894f0 at ebp-0x58; 0x10038b19, after the format
//  cascade, stores 0 to the same dword.  Under icl there is no dead-store
//  elimination through memory, so the shipped code keeping
//  both is not evidence of anything clever -- but the two being separated
//  by a five-way branch IS evidence that they are two source statements,
//  because a constructor body would sit against its own vptr store.
// ----------------------------------------------------------------------- //
extern void* g_vtbl_100894d0[];                         // 0x100894d0
extern void* g_vtbl_100894f0[];                         // 0x100894f0

struct RTexInfo
{
	void*       m_pVTable;                              // +0x00
	float       m_fInvW04;                              // +0x04
	float       m_fInvH08;                              // +0x08
	DDSurf7T*   m_pSurface0c;                           // +0x0c
	uint32      m_nBytes10;                             // +0x10
	uint16      m_nStamp14;                             // +0x14
	uint16      m_nAlphaRef16;                          // +0x16
	uint32      m_n18;                                  // +0x18

	// Defined in the class body so /Ob1 inlines them, which is how the
	// shipped code has them.
	RTexInfo()  { m_pVTable = g_vtbl_100894f0; }
	~RTexInfo() { m_pVTable = g_vtbl_100894d0; }
};                                                      // 0x1c


// ----------------------------------------------------------------------- //
//  The renderer-side texture record.  0x4c bytes -- d3dr5c.cpp's decode of
//  the module init reads `sub_1007fb20(g_1008e644, 0x4c, 0x20)`, an
//  element size of 0x4c and a grow-by of 0x20 -- and every field named
//  below was named by big6d910.cpp, big43e10.cpp, big3a080.cpp or
//  d3dbatchd.cpp before this function was opened.  See EVIDENCE note 1.
//
//  The free list threads through the FIRST dword: 0x10038cf2 is
//  `mov (%eax),%edx ; mov %edx,0x1008e65c`, popping through the same slot
//  the vtable lives in.  d3dr2c_os.cpp spells that as a union and this
//  does the same.
// ----------------------------------------------------------------------- //

// (the record itself is this file's RTexture, above)


// ----------------------------------------------------------------------- //
//  The argument this function is handed.  Three dwords, built on its one
//  caller's stack at 0x10039c20..0x10039c26 and read here as:
//     +0x00  `mov (%eax),%eax` at 0x10038e0b and 0x10038e18  -- the shared
//            texture; its +0x0c is where the finished record is hung
//     +0x04  `mov 0x4(%eax),%ebx` at 0x10038ab7             -- the
//            TextureData, %ebx for the whole function
//     +0x08  `mov 0x8(%eax),%ecx ; test $0x1,%ch` at 0x10038ab1 and
//            `movzbl 0x8(%edx),%edx` at 0x10038d1b -- a DWORD whose bit 8
//            selects format 6 and whose LOW BYTE is the texture stage
// ----------------------------------------------------------------------- //


// Bit 8 of TexRequest::m_nStage08, tested as `test $0x1,%ch` on the dword.
#define TEXREQ_WANT_FORMAT6             0x00000100

// Bit 0 of argument 2, tested as `test $0x1,%dl` at 0x10038e0d.  When it
// is CLEAR the record is published into the shared texture's +0x0c; when
// it is set it is not.  big6d910.cpp calls the caller's third argument
// bLink, which is the same dword.
#define RTEX_NOLINK                     0x00000001


// ----------------------------------------------------------------------- //
//  The surface-format table.  big3a080.cpp's D3DFormatRec and its
//  g_1008e6b0[7]; only +0x2c, the log2 of the bytes per pixel, is read
//  here (`mov 0x2c(%eax),%edi` then `shl %cl,%edx` on a pixel count),
//  which is exactly how big3a080.cpp uses it.
// ----------------------------------------------------------------------- //



// ----------------------------------------------------------------------- //
//  The record bank.  d3dr5c.cpp's decode of 0x1003ac30 gives
//  sub_1007fb20(g_1008e644, 0x4c, 0x20) -- so 0x1008e644 is a bank object
//  with the element size at +0x04 and the grow-by at +0x08, which is
//  d3dbatchc.cpp's Pool1000a6a0 shape.  Here the pop is INLINED, and it is
//  the same source d3dbatchc.cpp writes out of line for 0x1000a6a0: the
//  empty test leads, the grow sits in a sunk cold block that jumps BACK to
//  the pop, and the pop is the common tail.  The free head is at +0x18
//  (0x1008e65c = 0x1008e644 + 0x18), which lands on Pool1000a6a0's
//  m_pFree18 exactly.
// ----------------------------------------------------------------------- //


// 0x1007fbf0 -- d3dbatchc.cpp: __cdecl (pool, count), grows the pool.
extern "C" void sub_1007fbf0(RTexBank* pBank, uint32 nCount);

// The body of 0x1000a6a0, INLINED.  d3dbatchc.cpp reproduces the same
// source out of line for the OTHER pool in this module and its banner
// applies here word for word: "the pop is the FALL-THROUGH of the empty
// test and the refill sits out of line after the pop, jumping back into
// it.  That is a leading `if (list is empty) { grow; re-read; if (still
// empty) return NULL; }` with the pop as the function's common tail --
// not an if/else, and not a loop (the retry happens exactly once)."
// Here it is at 0x10038ce5 with the refill sunk to 0x10038ea5 and
// `jne 0x10038cf2` landing on the pop, and the caller's OWN null test
// follows at 0x10038cfc -- three zero tests off one pointer, which is
// what this shape produces and what icl will not fold.
// Marked `inline` so that /Ob1 expands it, which is how the shipped code
// has it; it is defined here rather than in the class body only because it
// needs RTexture to be complete.
inline RTexture* RTexBank::Alloc()
{
	RTexture* pNode;

	pNode = m_pFree18;
	if (pNode == LTNULL)
	{
		sub_1007fbf0(this, m_nGrowCount08);

		pNode = m_pFree18;
		if (pNode == LTNULL)
			return LTNULL;
	}

	m_pFree18 = pNode->m_pNextFree;
	return pNode;
}


// ----------------------------------------------------------------------- //
//  0x100385e0 -- 67 bytes of body, 0x100385e0..0x10038622 (d3dbatcha.cpp
//  says 80, counting the two lea-nop pads).  DECODED IN FULL from its own
//  bytes:
//
//      cmp $3,%eax ; je -> 0
//      if (!g_1008da80) return 0            <- d3dr5c.cpp: "S3TCEnable"
//      if (eax == 4) return g_1008da68 != 0
//      if (eax == 5) return g_1008da64 != 0
//      if (eax == 6) return g_1008da60 != 0
//      return 0
//
//  i.e. "is this compressed texture format available", with format 3 (the
//  uncompressed default) answering no, and 4/5/6 -- which big3a080.cpp
//  pins as DXT1/DXT3/DXT5 by their FourCC immediates -- each gated on the
//  S3TC console variable and on its own capability flag.  It is a REGISTER
//  ENTRY (the argument arrives in %eax and it opens no frame), which
//  d3dbatcha.cpp already records: "0x100385e0 (80 bytes, 3 callers) --
//  REGISTER-ABI ENTRY, cannot be spelled".  It is decoded here as
//  an ordinary call whose argument arrives in %eax.
// ----------------------------------------------------------------------- //
// (moved here from w6_100385e0.cpp, a /Qip register-only
// static of this TU, as measured.)
struct CVarR5C
{
	uint32	m_n00;                                          // +0x00, tested
};
extern CVarR5C g_1008da80;                                  // 0x1008da80  S3TCEnable

extern uint32 g_1008da60;                                   // 0x1008da60  DXT5 supported
extern uint32 g_1008da64;                                   // 0x1008da64  DXT3 supported
extern uint32 g_1008da68;                                   // 0x1008da68  DXT1 supported

// LTBOOL (unsigned), measured: as int32 icl predicts the answer non-zero
// and sinks 0x10038a90's `if (!sub_100385e0()) nMipBase += bias`, which
// the shipped code keeps inline (0x10038a90 1232 -> 1216 bytes, the
// shipped length).
static LTBOOL sub_100385e0(uint32 nFormat)
{
	uint32 bSupported;

	if (nFormat == 3 || !g_1008da80.m_n00)
		return 0;

	switch (nFormat)
	{
		case 4:  bSupported = g_1008da68; break;
		case 5:  bSupported = g_1008da64; break;
		case 6:  bSupported = g_1008da60; break;
		default: bSupported = 0; break;
	}

	return bSupported ? 1 : 0;
}

// MEASURED, NOT KEPT: a dead `case 3:` label (the guard
// above already excludes 3) is not shipped source we can evidence, so it
// stays out. With it: attempted -> NEAR (agreement 0.375 -> 0.531, 30 of 64
// unmasked bytes off, the shipped 80 bytes).  The switch carries its own
// `case 3: return 0;` and the default answers with an early `return 0`:
// that is the fourth case label icl was found to need for the shipped `je`
// chain (cmp 4/5/6 each `je` to a body, 3 dropped because the guard above
// already excludes it, which is why the shipped chain has no compare for
// it).  Still different: the three case bodies are not sunk past the ret
// (shipped order 6, 5, 4 after the shared exit), and the default and the
// early out share one `xor ; ret` where the shipped code has two.
// Measured with this switch: `case 3:` / `default:` as `bSupported = 0;
// break;` in the four combinations (0.438 .. 0.516), and the tail as
// `!= 0`, `if (b) return 1; return 0;`, `if (!b) return 0; return 1;`
// and `(b != 0) ? 1 : 0` (0.344 .. 0.531; the two `if` forms drop the
// separate not-supported `xor` and come out 64 bytes).

// The unsupported answer through the result (`default:
// bSupported = 0` and `return bSupported ? 1 : 0`) instead of an early
// `return 0` and an `if`: the body is the shipped 67 bytes with the early
// out and the not-supported answer as two separate `xor %eax,%eax` blocks,
// as shipped (80 bytes padded, so the lengths match; 12 ->
// 24 of the 64 unmasked bytes agree).  Still `cmp; jne` with the bodies
// inline rather than the shipped `je` chain; an enum-typed format, a
// result variable around the whole switch, and `case 3:` merged into
// `default:` measured no closer.

// MEASURED FOR 0x100385e0 (still w6_100385e0.cpp's): as a
// static here or with its callers in any /Qip file its register entry is
// right (`cmp $0x3,%eax` first, no shim) and it drops from 50 to 26-34
// bytes differing, but the body does not close.  The shipped 4/5/6 test is
// a `je` chain with the three case bodies sunk (6, 5, 4) and one shared
// ret -- the icl switch lowering, which in this compiler needs FOUR case
// labels: a 3-label switch (and every if-chain) comes out as `cmp; jne`
// with the bodies inline.  A dead `case 3:` label brings the je chain
// (icl then drops its compare, 3 being excluded already), but no spelling
// tried also gives the single ret with separate `xor` blocks for the early
// out and for the not-supported answer -- early returns merge them, a
// result variable keeps the case-3 compare.  0x10038630's switch on the
// same three formats is sunk the same way in the shipped image.  Moving
// both here was not done: the case-3 label would be invented, and the
// pair is still near either way.


// ----------------------------------------------------------------------- //
//  0x10038630 -- 1,118 bytes (0x10038630..0x10038a8d), THIS is its only
//  caller.  It builds the
//  DirectDraw surface chain and fills the RTexInfo.
//
//  ITS SIGNATURE IS READ OFF BOTH ENDS.  Its prologue is
//  `push %ebp ; mov %esp,%ebp ; push %ecx ; push %edx ; push %eax ;
//   mov $0x1614,%eax ; call 0x100805a0` and it then reads those three
//  saves back at 0x161c/0x1618/0x1614(%esp) -- so %eax, %edx and %ecx are
//  arguments -- and reads 0x14(%ebp) as a fourth.  The call site here
//  opens 0x18 of outgoing area and writes only 0xc(%esp) and 0x10(%esp),
//  which land on 0x14(%ebp) and 0x18(%ebp) in the callee: three register
//  arguments with their stack home slots RESERVED, then two more on the
//  stack.  Five arguments.
//
//  WHICH IS WHICH: %eax is the TexRequest (0x10038663 `mov %eax,%esi`,
//  then 0x4(%esi) -> %ebx = the TextureData and 0x8(%esi) -> a local, the
//  same two fields this function reads); %edx is the RTexInfo (the only
//  thing the caller has an address of); %ecx is nFirstMip (0x1003866f
//  stores it and 0x10038694 does `imul $0x18,%eax,%eax` on it to index the
//  mip array at +0xcc); 0x14(%ebp) is nMipCount; 0x18(%ebp) is nFormat.
//  It also calls 0x100385e0 with pData->m_nFormat defaulted to 3, exactly
//  as this function does, which is the cross-check on the last one.
// ----------------------------------------------------------------------- //
// (moved here from w6_100385e0.cpp with 0x100385e0.)
// DDSURFACEDESC2, the fields used (DDPIXELFORMAT is this file's).                                                          // 0x20

struct DDSD2Full
{
	uint32			dwSize;                                 // +0x00
	uint32			dwFlags;                                // +0x04
	uint32			dwHeight;                               // +0x08
	uint32			dwWidth;                                // +0x0c
	int32			lPitch;                                 // +0x10
	uint32			dwBackBufferCount;                      // +0x14
	uint32			dwMipMapCount;                          // +0x18
	uint32			dwAlphaBitDepth;                        // +0x1c
	uint32			dwReserved;                             // +0x20
	void*			lpSurface;                              // +0x24
	uint32			ddckCKDestOverlay[2];                   // +0x28
	uint32			ddckCKDestBlt[2];                       // +0x30
	uint32			ddckCKSrcOverlay[2];                    // +0x38
	uint32			ddckCKSrcBltLow;                        // +0x40
	uint32			ddckCKSrcBltHigh;                       // +0x44
	DDPixelFormat	ddpfPixelFormat;                        // +0x48
	uint32			dwCaps;                                 // +0x68
	uint32			dwCaps2;                                // +0x6c
	uint32			dwCaps3;                                // +0x70
	uint32			dwCaps4;                                // +0x74
	uint32			dwTextureStage;                         // +0x78
};                                                          // 0x7c

#define W6_DDSD_CAPS            0x00000001
#define W6_DDSD_HEIGHT          0x00000002
#define W6_DDSD_WIDTH           0x00000004
#define W6_DDSD_PIXELFORMAT     0x00001000
#define W6_DDSD_CKSRCBLT        0x00010000
#define W6_DDSD_MIPMAPCOUNT     0x00020000
#define W6_DDSD_TEXTURESTAGE    0x00100000
#define W6_DDPF_FOURCC          0x00000004
#define W6_DDSCAPS_COMPLEX      0x00000008
#define W6_DDSCAPS_TEXTURE      0x00001000
#define W6_DDSCAPS_MIPMAP       0x00400000
#define W6_DDSCAPS2_40090       0x00040090      // as read (0x10038697)

#define W6_FOURCC_DXT1          0x31545844
#define W6_FOURCC_DXT3          0x33545844
#define W6_FOURCC_DXT5          0x35545844

// IDirectDrawSurface7 / IDirectDraw7, C-style; +0xb4 is SetLOD, +0x18
// CreateSurface.
struct Surf8630;
struct Surf8630Vtbl
{
	void*	m_pad_00[45];                                   // +0x00..+0xb0
	long	(__stdcall* SetLOD)(Surf8630*, uint32 nLOD);  // +0xb4
};
struct Surf8630 { Surf8630Vtbl* lpVtbl; };

struct DDraw8630;
struct DDraw8630Vtbl
{
	void*	m_pad_00[6];                                    // +0x00..+0x14
	long	(__stdcall* CreateSurface)(DDraw8630*, DDSD2Full* pDesc,
	                                   Surf8630** ppSurf,
	                                   void* pOuter);        // +0x18
};
struct DDraw8630 { DDraw8630Vtbl* lpVtbl; };

#define g_pDirectDraw8630  ((DDraw8630*)g_pDirectDraw)            // 0x1008bf74

// big38a90.cpp's TexData8630, and the fields THIS function reads:
//   +0x22 the format (0 -> 3), +0x24 a mip-scale shift, +0x25 the LOD,
//   +0x2c the texture's command string (ConParse input), +0xcc the mips.
struct Mip8630
{
	uint32	m_nWidth;                                       // +0x00
	uint32	m_nHeight;                                      // +0x04
	uint8	m_Reserved08[0x10];                             // +0x08..+0x17
};                                                          // 0x18

class TexData8630
{
public:
	void*			m_pVTable;                              // +0x00
	uint8			m_Reserved04[0x1e];                     // +0x04..+0x21
	uint8			m_nFormat;                              // +0x22
	uint8			m_nExtraBias23;                         // +0x23
	uint8			m_nMipScale24;                          // +0x24
	uint8			m_nLOD25;                               // +0x25
	uint8			m_Reserved26[0x06];                     // +0x26..+0x2b
	char			m_szCommand2c[0xcc - 0x2c];             // +0x2c..+0xcb
	Mip8630	m_Mips[1];                              // +0xcc
};

// The surface-format table (big38a90.cpp / big3a080.cpp): +0x0c is the
// DDPIXELFORMAT.
struct FmtRec8630
{
	uint32			m_nUnknown00[3];                        // +0x00..+0x0b
	DDPixelFormat	m_ddpf;                                 // +0x0c..+0x2b
};
#define g_1008e6b0_8630  ((FmtRec8630**)g_1008e6b0)              // 0x1008e6b0

// PFormat8630, 0x38 bytes, vtable 0x10089390 (big3a080.cpp).  Spelled with
// the vptr as a data member and the constructor's one store by hand, as
// big38a90.cpp does for RTexInfo, so no vtable of ours is emitted.
extern uint8 g_10089390[];                                  // 0x10089390

class PFormat8630
{
public:
	void*	m_pVTable;                                      // +0x00
	uint32	m_nType04;                                      // +0x04
	uint8	m_Reserved08[0x30];                             // +0x08..+0x37

	PFormat8630()	{ m_pVTable = g_10089390; }

};



// LithTech's ConParse (d3dqip3.cpp's C10023130): 64 argument pointers,
// the count at +0x100, the command position at +0x104.  Its inline
// constructor clears the position (`movl $0x0,-0x14fc(%ebp)`).
class C10023130
{
public:
	char*	m_Args[0x40];                                   // +0x000
	int32	m_n100;                                         // +0x100
	char*	m_p104;                                         // +0x104
	void*	m_p108;                                         // +0x108
	uint8	m_Buffer[0x1508 - 0x10c];                       // +0x10c

	C10023130()				{ m_p104 = LTNULL; }
	void	Init(char* p)	{ m_p104 = p; }

	LTBOOL  sub_10023130(const char* pName, uint32 bCaseSensitive,
	                     int32 nIndex);                     // 0x10023130
};

extern uint32 g_1008d5e4;                                   // 0x1008d5e4, colour keys off

// 0x10038560 -- (w, h, &w, &h): the dimensions the device will accept.
extern void sub_10038560(uint32 nW, uint32 nH, uint32* pW, uint32* pH);



// NEAR (1120 = 1120 bytes, was 1104).  What moved it: the desc stores
// written size/flags/caps/caps2 and width/height/mipcount (icl emits a
// run of constant stores in reverse); the format default through this
// inline helper (icl sinks the `return 3` arm, as shipped: `je` to a
// `mov $3,%esi ; jmp` past the epilogue); the colour as
// `r<<16 | (g<<8 | b)`; the table format copied by memcpy (through %edi,
// as shipped); the LOD in a local before SetLOD; 0x1007a180 unsigned and
// 0x1007a3b0 cold-named (see the declarations).  Left: the FourCC switch.
// The shipped code is a `je` chain (4, 5, 6) with the three stores sunk
// past the epilogue; icl 4.0 lowers a switch that way only with FOUR
// labels (a separate-bodied `case 3:` or `case 0:` gives it, plus that
// label's own compare and return, 1136 bytes); with three it is
// `cmp; jne` with the bodies inline.  Shared-body extra labels (`case
// 3: default:`), default first, cases 6/5/4, an if-chain, a FourCC helper
// returning the code or a BOOL: no.
static inline uint32 GetFmt8630(TexData8630* p)
{
	if (!p->m_nFormat)
		return 3;
	return p->m_nFormat;
}

static int32 sub_10038630(TexRequest* pReq, RTexInfo* pInfo,
                          uint32 nFirstMip, uint32 nMipCount,
                          uint32 nFormat)
{
	C10023130      parse;
	PFormat8630        fmt;
	DDSD2Full      ddsd;
	TexData8630*   pData;
	Surf8630*    pSurf;
	uint32         nFmt;
	uint32         nColor;
	uint32         nKey;
	uint32         nW, nH;
	uint32         nLOD;

	pData = (TexData8630*)pReq->m_pData04;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize         = sizeof(ddsd);
	ddsd.dwFlags        = W6_DDSD_CAPS | W6_DDSD_HEIGHT | W6_DDSD_WIDTH |
	                      W6_DDSD_PIXELFORMAT | W6_DDSD_MIPMAPCOUNT |
	                      W6_DDSD_TEXTURESTAGE;
	ddsd.dwCaps         = W6_DDSCAPS_TEXTURE | W6_DDSCAPS_MIPMAP | W6_DDSCAPS_COMPLEX;
	ddsd.dwCaps2        = W6_DDSCAPS2_40090;
	ddsd.dwWidth        = pData->m_Mips[nFirstMip].m_nWidth;
	ddsd.dwHeight       = pData->m_Mips[nFirstMip].m_nHeight;
	ddsd.dwMipMapCount  = nMipCount;
	ddsd.dwTextureStage = pReq->m_nStage08;

	nFmt = GetFmt8630(pData);

	if (sub_100385e0(nFmt))
	{
		memset(&ddsd.ddpfPixelFormat, 0, sizeof(ddsd.ddpfPixelFormat));
		ddsd.ddpfPixelFormat.dwSize   = sizeof(ddsd.ddpfPixelFormat);
		ddsd.ddpfPixelFormat.dwFlags |= W6_DDPF_FOURCC;

		switch (nFmt)
		{
			case 4:  ddsd.ddpfPixelFormat.dwFourCC = W6_FOURCC_DXT1; break;
			case 5:  ddsd.ddpfPixelFormat.dwFourCC = W6_FOURCC_DXT3; break;
			case 6:  ddsd.ddpfPixelFormat.dwFourCC = W6_FOURCC_DXT5; break;
			default: return 0;
		}
	}
	else
	{
		memcpy(&ddsd.ddpfPixelFormat, &g_1008e6b0_8630[nFormat]->m_ddpf, sizeof(ddsd.ddpfPixelFormat));
		sub_10038560(ddsd.dwWidth, ddsd.dwHeight,
		             &ddsd.dwWidth, &ddsd.dwHeight);
	}

	// "ColorKey r g b": a source colour key, unless 0x1008d5e4 is set.
	parse.Init(pData->m_szCommand2c);
	if (parse.sub_10023130("ColorKey", 0, 3) && !g_1008d5e4)
	{
		nColor = (atoi(parse.m_Args[1]) << 16) |
		         ((atoi(parse.m_Args[2]) << 8) |
		         atoi(parse.m_Args[3]));

		if (sub_100385e0(nFmt))
			((PFormat*)&fmt)->InitDefault1007a3b0();
		else
			sub_10021010(&ddsd.ddpfPixelFormat, (PFormat*)&fmt);

		if (!g_1008daa0.sub_1007a180((PFormat*)&fmt, nColor, &nKey))
		{
			ddsd.dwFlags |= W6_DDSD_CKSRCBLT;
			ddsd.ddckCKSrcBltHigh = nKey;
			ddsd.ddckCKSrcBltLow  = nKey;
		}
	}

	// "AlphaRef n".
	parse.Init(pData->m_szCommand2c);
	if (!parse.sub_10023130("AlphaRef", 0, 1))
		pInfo->m_nAlphaRef16 = 0;
	else
		pInfo->m_nAlphaRef16 = (uint16)atoi(parse.m_Args[1]);

	if (g_pDirectDraw8630->lpVtbl->CreateSurface(g_pDirectDraw8630, &ddsd, &pSurf,
	                                         LTNULL))
	{
		sub_10021d60(4, "Unable to create (%dx%d) texture surface.",
		             ddsd.dwWidth, ddsd.dwHeight);
		return 0;
	}

	nLOD = pData->m_nLOD25;
	pSurf->lpVtbl->SetLOD(pSurf, nLOD);

	sub_10038560(pData->m_Mips[0].m_nWidth, pData->m_Mips[0].m_nHeight,
	             &nW, &nH);

	pInfo->m_fInvW04 = 1.0f / (float)nW;
	pInfo->m_fInvH08 = 1.0f / (float)nH;
	pInfo->m_fInvW04 *= (float)(1 << pData->m_nMipScale24);
	pInfo->m_fInvH08 *= (float)(1 << pData->m_nMipScale24);

	pInfo->m_pSurface0c = (DDSurf7T*)pSurf;
	return 1;
}


// ----------------------------------------------------------------------- //
//  GLOBALS.  Every one is read straight off an instruction; the names are
//  by address except where an earlier file already named them.
// ----------------------------------------------------------------------- //

// The LRU list this function links every new record into.  It is an LTLink
// OBJECT, not a pointer: 0x10038e3d plants the IMMEDIATE 0x1008e634 as the
// new link's m_pPrev and 0x10038e4b LOADS 0x1008e638 as its m_pNext, and
// 0x1008e638 is 0x1008e634 + 4 -- the head's own m_pNext.  d3dr5c.cpp's
// decode of the module init 0x1003ac30 records a bare LTLink tied off at
// 0x1008e6a4 in the same file, the same shape.
//
// AND ITS SIZE IS FIXED FROM OUTSIDE.  d3dr5c.cpp also reads
// `g_1008e640 = g_10089510` -- the bank's own `this` slot, which it places
// immediately ahead of the bank at 0x1008e644.  0x1008e640 - 0x1008e634 =
// 0x0c, so the head occupies exactly twelve bytes and there is no room for
// a fourth dword.  Twelve is ltlink.h's LTLink (m_pPrev, m_pNext,
// m_pData), which is also the size the record's own +0x1c..+0x27 link must
// be for big6d910.cpp's +0x28 to land where it found it.  Two independent
// readings of the same twelve.

// NOTE ON THE FREE HEAD.  The pop reads and writes the ABSOLUTE address
// 0x1008e65c rather than an offset off a base register, and the grow call
// passes the IMMEDIATE 0x1008e644 and reads the IMMEDIATE 0x1008e64c.
// 0x1008e65c - 0x1008e644 = 0x18 and 0x1008e64c - 0x1008e644 = 0x08, which
// is Pool1000a6a0's m_pFree18 and m_nGrowCount08 exactly; a member call on
// a global object folds the base into the displacement, so absolute
// addressing is what a global bank produces and is not evidence against
// the bank shape.

// d3dmid.cpp / d3dbatchc.cpp's g_pStruct, the engine's RenderStruct.  Only
// its +0x50 counter is touched here (`add %edx,0x50(%eax)`).  big6d910.cpp
// and big43e10.cpp add the SAME record field, m_nBytes10, into the
// neighbouring +0x48; two different counters off the same object.

// Read but never written here.  All six are plain dwords tested against
// zero or used as bounds; nothing in this function names them further.
extern uint32 g_n1008cad4;                              // 0x1008cad4
extern uint32 g_a1008cb34[10];                          // 0x1008cb34
extern uint32 g_n1008cb70;                              // 0x1008cb70
extern uint32 g_n1008cbb0;                              // 0x1008cbb0
extern uint32 g_n1008cbb4;                              // 0x1008cbb4
extern uint32 g_n1008cbd0;                              // 0x1008cbd0
extern uint32 g_n1008d828;                              // 0x1008d828
// 0x1008d82c -- the one D3DDEVICEDESC7 copy 0x100322c0 fills (w5_100322c0.cpp);
// the fields this file reads are addressed inside it, not as separate
// globals, so a linked d3d.ren reads what that copy wrote.
struct D3DDevDesc100322c0;
extern D3DDevDesc100322c0 g_DevDesc1008d82c;
#define g_n1008d8b0 (*(uint32*)((char*)&g_DevDesc1008d82c + 0x84))   // 0x1008d8b0 dwMaxTextureWidth
#define g_n1008d8b4 (*(uint32*)((char*)&g_DevDesc1008d82c + 0x88))   // 0x1008d8b4 dwMaxTextureHeight
extern uint32 g_n1008e6c8;                              // 0x1008e6c8


// The two float constants, read out of .data1 rather than assumed:
//   0x100a93f8 = 0x3f800000 = 1.0f          (fld, then fadd the texture's
//                                            own bias)
//   0x100a93fc = 0x3c8efa35 = 0.017453292f  (pi/180 -- degrees to radians,
//                                            which is what fixes +0x2a/+0x2b
//                                            as an angle in DEGREES)
#define R_DEG2RAD                       0.017453292f


// icl expands cos and sin to `fcos` and `fsin` where the shipped code has
// them inline (0x10038d8f and 0x10038d99 are the bare instructions, with
// no call in between).  d3dbatchd.cpp uses the same pragma for sqrt.  If
// icl emits _CIcos/_CIsin calls anyway they will show up as two ours-only
// rows in the call sequence and nothing else.
#pragma intrinsic(cos, sin)


// ======================================================================= //
//  0x10038a90   r_CreateRTexture(TexRequest*, uint32)   1,203 bytes
//
//  REGISTER-ARGUMENT ENTRY (icl /Qip): %eax = pReq, %edx = nFlags.  The
//  first three instructions of the body -- `mov 0x8(%eax),%ecx ;
//  mov %eax,-0x18(%ebp) ; mov %edx,-0x24(%ebp)` -- read both before
//  anything is loaded off the stack, and the one call site at 0x10039c29
//  reserves eight bytes and writes NEITHER of them (`lea -0x14(%ebp),%eax`
//  and `mov -0x8(%ebp),%edx`).  Decoded as an ordinary
//  two-argument call.
//
//  Returns the record, or LTNULL on any of five failures.
// ======================================================================= //
static RTexture* sub_10038a90(TexRequest* pReq, uint32 nFlags)
{
	// The nine frame slots, in the order the frame lays them out; see the
	// FRAME ARITHMETIC banner above.
	RTexInfo      info;                                 // ebp-0x58
	int32         nAngle;                               // ebp-0x38
	uint32        nFormat;                              // ebp-0x20
	int32         nBias;                                // ebp-0x1c
	int32         nMipCount;                            // ebp-0x1c (shared)
	int32         nFirstMip;                            // ebp-0x14
	double        fTheta;                               // ebp-0x10

	TextureData*  pData;
	D3DFormatRec* pFmtRec;
	RTexture*     pRec;
	int32         nMipBase, nLevel, nAvail, nCount, nFmt, nTableIdx;
	uint32        i, nMaxW, nMaxH;

	pData = pReq->m_pData04;

	// ------------------------------------------------------------------ //
	//  REGION 1 -- 0x10038a90..0x10038b13, plus the sunk arms 0x10038f05
	//  and 0x10038f2b.  Choose the surface format index.
	//
	//  WHAT PINS IT: five distinct immediates land in the SAME slot,
	//  ebp-0x20 -- 6, 1, 2, 1, 3 and 0 -- and that slot is later used two
	//  ways that agree with each other: as the record's m_nFormatIndex48
	//  (`mov %al,0x48(%esi)`) and as the index into g_1008e6b0
	//  (`mov 0x1008e6b0(,%eax,4),%eax`).  big3a080.cpp independently found
	//  RTexture+0x48 to be the index into that same seven-entry table.
	//  The four flag tests are `test $0x2,%al`, `test $0x1,%ah`,
	//  `test $0x80,%eax` and `test $0x1,%al` on ONE dword load of
	//  pData->m_nFlags18, so the bit values are 0x02, 0x100, 0x80 and
	//  0x01 -- read, not guessed.
	//
	//  THE 0x100 BIT OF THE REQUEST comes off a DWORD load and a `%ch`
	//  test (`mov 0x8(%eax),%ecx ; test $0x1,%ch`), which is icl's shape
	//  for `(dword & 0x100)`; the low byte of the same dword is re-read
	//  from memory later for m_nStage42, so the field is one dword used
	//  two ways and not two byte members.
	//
	//  THE `!(flags & 2)` ARM IS THE SUNK ONE (0x10038f05, below the last
	//  hot exit, jumping back to 0x10038ade and 0x10038b14).  That is /O2
	//  predicting the bit set; it does not change the condition.
	// ------------------------------------------------------------------ //
	if (pReq->m_nStage08 & TEXREQ_WANT_FORMAT6)
	{
		// 0x10038abc.  No format-6 support -> refuse outright.  This exit
		// runs the RTexInfo destructor and nothing else, which is why the
		// sunk block at 0x10038f2b is a bare
		// `movl $0x100894d0,-0x58 ; xor %eax,%eax ; <epilogue>`.
		if (!g_n1008e6c8)
			return LTNULL;

		nFormat = 6;
	}
	else if (!(pData->m_nFlags18 & TD_FLAG_NOTCOMPRESSED_02) &&
	         g_n1008cbd0 && g_1008e6b0[0])
	{
		nFormat = 0;
	}
	else if (pData->m_nFlags18 & TD_FLAG_100)
	{
		nFormat = 1;
	}
	else if (pData->m_nFlags18 & TD_FLAG_80)
	{
		nFormat = 2;
	}
	else if (pData->m_nFlags18 & TD_FLAG_01)
	{
		// The SAME immediate 1 as the 0x100 arm, planted separately at
		// 0x10038b02; icl did not cross-jump the two, so they really are
		// two source arms.
		nFormat = 1;
	}
	else
	{
		nFormat = 3;
	}

	// ------------------------------------------------------------------ //
	//  REGION 2 -- 0x10038b14..0x10038b68, plus the sunk arm 0x10038efb.
	//  Clear the info block, then work out the base mip level.
	//
	//  WHAT PINS IT: the clear is seven `mov %eax,K(%edi)` off one
	//  `xor %eax,%eax` at 0x00,0x04,0x08,0x0c,0x10,0x14,0x18 -- 0x1c
	//  bytes, sizeof(RTexInfo), and icl's expansion of a small memset
	//  (0x10038630 has the same unrolled shape for 0x20 bytes at
	//  0x100386fe and a `rep stos` for the 0x7c one at 0x1003868c, so the
	//  cut-off is between 0x20 and 0x7c).
	//
	//  The table read is `movzbl 0x20(%ebx),%eax ; cmp $0x9,%eax ;
	//  jle ; mov $0x9,%eax` then `mov 0x1008cad4,%esi ;
	//  add 0x1008cb34(,%eax,4),%esi` -- a byte clamped to at most 9 and
	//  used to index a DWORD table, so the table has ten entries and the
	//  clamp is `> 9`, not `>= 9`.  Both globals are in .bss so their
	//  values cannot be read; only the shape is claimed.
	//
	//  The format default is `movzbl 0x22(%ebx) ; test ; je <sunk
	//  0x10038efb: mov $3,%eax ; jmp back>` -- the /O2 shape of
	//  `if (!x) x = 3;`, and big3a080.cpp applies the identical default to
	//  the identical byte.
	// ------------------------------------------------------------------ //
	memset(&info, 0, sizeof(info));

	// `cmp $0x9,%eax ; jle` is a SIGNED compare on the byte promoted to
	// int, so the index is an int and the test is `> 9`.
	nTableIdx = pData->m_nMipOffset20;
	if (nTableIdx > 9)
		nTableIdx = 9;

	nMipBase = g_n1008cad4 + g_a1008cb34[nTableIdx];

	nFmt = pData->m_nFormat;
	if (nFmt == 0)
		nFmt = 3;

	// 0x10038b54.  When the compressed format this texture wants is not
	// available the texture is dropped one or more extra levels.
	if (!sub_100385e0(nFmt))
		nMipBase += pData->m_nExtraBias23;

	// ------------------------------------------------------------------ //
	//  REGION 3 -- 0x10038b69..0x10038b99, plus the sunk arms 0x10038edf,
	//  0x10038eed and 0x10038ef4.  Two nested clamps on the base level.
	//
	//  WHAT PINS IT: `test %esi,%esi ; jl` then `cmp $0x3,%esi ; jle` then
	//  a fall-through `mov $0x3,%eax` is LTCLAMP(x, 0, 3) exactly -- low
	//  arm, high arm, hi-value fall-through -- and the second pair,
	//  `test %eax,%eax ; jl` then `cmp %edx,%eax ; jle` with
	//  `%edx = pData->m_nMipCount - 1`, is LTCLAMP(x, 0, n - 1).  BOTH
	//  arms of the second clamp load `movzwl 0x14(%ebx),%esi` (0x10038b87
	//  and 0x10038edf), which is the macro evaluating its `hi` twice; the
	//  count then stays in %esi as the search loop's bound.
	//
	//  The second clamp's low test is NOT foldable away even though the
	//  first already forced x >= 0.  icl does not fold
	//  redundant tests, so two tests means the source really tests twice
	//  -- two nested clamp expressions, not one.
	// ------------------------------------------------------------------ //
	nBias = LTCLAMP(nMipBase, 0, 3);
	nBias = LTCLAMP(nBias, 0, (int32)pData->m_nMipCount - 1);

	// ------------------------------------------------------------------ //
	//  REGION 4 -- 0x10038b9a..0x10038bf0.  The two size caps.
	//
	//  WHAT PINS IT: six global dwords, four min-selects and two
	//  zero-defaults, all in one straight-line block.  Each select is a
	//  `cmp` plus a conditional `mov` of the OTHER operand, and the
	//  direction is read off the condition code, not assumed:
	//
	//    0x10038ba5  cmp %edx,%eax ; jae skip ; mov %eax,%edx
	//                -> %edx = LTMIN(g_n1008cb70, g_n1008d8b0)
	//    0x10038bb3  cmp %edi,%eax ; jb  skip ; mov %edi,%eax
	//                -> %eax = LTMIN(g_n1008cb70, g_n1008d8b4)
	//    0x10038bbb  test %edx,%edx ; jne skip ; mov $0x100,%ecx
	//    0x10038bc4  test %eax,%eax ; jne skip ; mov $0x100,%edi
	//                -> zero means "no cap", spelled as 256
	//    0x10038bdb  cmp %eax,%ecx ; jae skip ; mov %ecx,%eax
	//                -> LTMIN(nMaxW, g_n1008cbb4)
	//    0x10038be9  cmp %edx,%eax ; jae skip ; mov %eax,%edx
	//                -> LTMIN(<that same result>, g_n1008cbb0)
	//
	//  All six are UNSIGNED (jae / jb / jae), so the caps are uint32.
	//
	//  A DELIBERATE ODDITY, AND IT IS WHAT THE BYTES SAY.  The last
	//  select's left operand is %eax, and %eax at 0x10038be9 is the value
	//  the PREVIOUS select just produced -- the clamped WIDTH -- not the
	//  height in %edi.  `mov %eax,%ecx` at 0x10038be7 publishes it as the
	//  new width and `mov %edx,%edi` at 0x10038bef publishes
	//  min(width, g_n1008cbb0) as the new height, so the incoming height
	//  is discarded entirely inside this `if`.  That is not smoothed
	//  over: %eax is set at 0x10038bd6/0x10038bdf, used at 0x10038be7 and
	//  0x10038be9 with nothing in between, one basic block, no ambiguity.
	//  If the original said `nMaxH = LTMIN(nMaxH, g_n1008cbb0)` the
	//  compare would have to read %edi and it does not.  Written as the
	//  bytes have it; flagged as the one place where the
	//  reconstruction reproduces what looks like a source-level bug.
	// ------------------------------------------------------------------ //
	nMaxW = LTMIN(g_n1008cb70, g_n1008d8b0);
	nMaxH = LTMIN(g_n1008cb70, g_n1008d8b4);

	if (nMaxW == 0)
		nMaxW = 0x100;

	if (nMaxH == 0)
		nMaxH = 0x100;

	if (!g_n1008d828)
	{
		nMaxW = LTMIN(nMaxW, g_n1008cbb4);
		nMaxH = LTMIN(nMaxW, g_n1008cbb0);
	}

	// ------------------------------------------------------------------ //
	//  REGION 5 -- 0x10038bf1..0x10038c3c, plus the sunk arm 0x10038cde.
	//  Find the first mip level that fits inside the caps.
	//
	//  WHAT PINS IT: the loop is `xor %edx,%edx` (the index) plus
	//  `xor %eax,%eax` (a BYTE offset stepped by `add $0x18,%eax`), the
	//  bound is `cmp %esi,%edx ; jb` against the mip count and the entry
	//  test is `test %esi,%esi ; jbe` -- both UNSIGNED, so the counter is
	//  a uint32 and the loop is a plain `for`.  The two comparisons are
	//  `cmp 0xcc(%ebx,%eax,1),%ecx` and `cmp 0xd0(%ebx,%eax,1),%edi`:
	//  the CAP is the cmp destination in both, so it is the source's left
	//  operand, and the exits are `jb` and `jae` -- `nMaxW >= w &&
	//  nMaxH >= h`, unsigned.  Stride 0x18 at base +0xcc matches
	//  big3a080.cpp's TextureMipData exactly.
	//
	//  The sentinel is explicit: the loop's fall-through is
	//  `mov $0xffffffff,%eax` and the hit sinks to 0x10038cde
	//  (`mov %edx,%eax ; jmp back`), both landing on the SAME
	//  `cmp $0xffffffff,%eax ; je`.  That is `nLevel = -1` with a `break`,
	//  not a post-loop `i == n` test -- a post-loop test would compare
	//  against %esi.
	//
	//  The join is `cmp %eax,%edx ; jle` with nBias in %edx -- SIGNED, and
	//  nBias is the cmp destination, so `LTMAX(nBias, nLevel)`.
	// ------------------------------------------------------------------ //
	nLevel = -1;

	for (i = 0; i < pData->m_nMipCount; i++)
	{
		if (nMaxW >= pData->m_Mips[i].m_nWidth &&
		    nMaxH >= pData->m_Mips[i].m_nHeight)
		{
			nLevel = i;
			break;
		}
	}

	// 0x10038cc6.  Nothing fits: destroy the (still empty) info block and
	// refuse.  No surface has been made yet, so nothing is released.
	if (nLevel == -1)
		return LTNULL;

	nFirstMip = LTMAX(nBias, nLevel);

	// ------------------------------------------------------------------ //
	//  REGION 6 -- 0x10038c3d..0x10038c6d, plus the sunk arms 0x10038ed5,
	//  0x10038c58, 0x10038c66 and 0x10038cae.  How many levels to take.
	//
	//  WHAT PINS IT: `movzbl 0x21(%ebx) ; test ; je <sunk 0x10038ed5:
	//  mov $0x4,%eax ; jmp back>` is `if (!x) x = 4;` in the same /O2
	//  shape as the format default in region 2.  Then `mov %esi,%edx ;
	//  sub %ecx,%edx ; je` -- the SUBTRACTION is tested, not the two
	//  operands, so the source computes the difference and tests it.  Then
	//  `test %eax,%eax ; jg` / `cmp %edx,%eax ; jle`, both SIGNED, which
	//  is LTCLAMP(x, 1, nAvail): the low arm is `x < 1`, spelled by icl as
	//  `x <= 0`.
	//
	//  The `nAvail == 0` exit at 0x10038cae is the third of the three
	//  early refusals and, like the other two, releases nothing.
	// ------------------------------------------------------------------ //
	nCount = pData->m_nWantedMips21;
	if (nCount == 0)
		nCount = 4;

	nAvail = (int32)pData->m_nMipCount - nFirstMip;

	if (nAvail == 0)
		return LTNULL;

	nMipCount = LTCLAMP(nCount, 1, nAvail);

	// ------------------------------------------------------------------ //
	//  REGION 7 -- 0x10038c6e..0x10038c95.  Build the surfaces.
	//
	//  WHAT PINS IT: the argument marshalling is spelled out in the
	//  0x10038630 banner above.  The failure test is `test %eax,%eax ;
	//  jne <the allocate path>` with the zero arm falling straight into
	//  the destructor and `xor %eax,%eax`, so a zero answer is failure and
	//  a non-zero one is success -- and, again, nothing is released here,
	//  because a helper that answers 0 has not left a surface behind.
	// ------------------------------------------------------------------ //
	if (!sub_10038630(pReq, &info, nFirstMip, nMipCount, nFormat))
		return LTNULL;

	// ------------------------------------------------------------------ //
	//  REGION 8 -- 0x10038ce5..0x10038d0a, with the refill sunk to
	//  0x10038ea5 and the failure arm sunk to 0x10038e81.  Take a record
	//  off the bank and run its constructor.
	//
	//  WHAT PINS IT: see the RTexBank::Alloc banner above -- the pop, the
	//  refill and the retry are 0x1000a6a0's body instruction for
	//  instruction, with 0x1007fbf0 called as (0x1008e644, [0x1008e64c])
	//  which d3dbatchc.cpp reads as (pool, pool->m_nGrowCount08).  The
	//  free list threads through the record's FIRST dword
	//  (`mov (%eax),%edx ; mov %edx,0x1008e65c`), the same dword
	//  0x1003abe4 then overwrites with the vtable -- which is why RTexture
	//  spells +0x00 as a union, and which is d3dr2c_os.cpp's reading of
	//  the mirror-image free at 0x10037550.
	//
	//  THE FAILURE ARM IS THE ONLY ONE THAT RELEASES ANYTHING.  0x10038e81
	//  is `mov -0x4c(%ebp),%edx ; mov (%edx),%ecx ; mov %edx,(%esp) ;
	//  call *0x8(%ecx)` -- ebp-0x4c is info.m_pSurface0c, the surface
	//  0x10038630 just created, and slot +0x08 is Release.  That is the
	//  cross-check on the whole failure structure: exactly the one exit
	//  taken AFTER the surface exists releases it, and the four taken
	//  before it do not.
	// ------------------------------------------------------------------ //
	pRec = g_1008e644.Alloc();

	if (pRec == LTNULL)
	{
		info.m_pSurface0c->lpVtbl->Release(info.m_pSurface0c);
		return LTNULL;
	}

	pRec->sub_1003abe4();

	// ------------------------------------------------------------------ //
	//  REGION 9 -- 0x10038d0b..0x10038d6b.  Copy the info block into the
	//  record and stamp the scalars.
	//
	//  WHAT PINS IT: six consecutive frame slots go to six consecutive
	//  record offsets -- ebp-0x54,-0x50,-0x4c,-0x48 as DWORDS to +0x04,
	//  +0x08, +0x0c, +0x10 and ebp-0x44,-0x42 as WORDS (`movzwl` then
	//  `mov %dx,`) to +0x14, +0x16.  That is field-by-field, not
	//  `rep movsl`, and it is what fixes the two 16-bit members: a struct
	//  copy would have moved seven dwords.  It also fixes ebp-0x58 as the
	//  base of the object, because the copy starts at ebp-0x54 = base+4.
	//
	//  The floats are moved with INTEGER `mov`s, which is
	//  icl's behaviour for float lvalue copies -- so +0x04 and
	//  +0x08 being floats is big6d910.cpp's finding (it divides by them),
	//  not something this function can show on its own.
	//
	//  Three casts are read off their instructions, not assumed:
	//  `mov %al,0x49` / `mov %al,0x48` / `mov %cl,0x47` / `mov %al,0x46`
	//  are BYTE stores out of dword values, and `movzbl 0x8(%edx),%edx ;
	//  mov %dx,0x42` is a byte widened into a WORD field.
	//
	//  +0x18 IS THE SELF-POINTER, not a copy of info's +0x18:
	//  `mov %esi,0x18(%esi)`.  See EVIDENCE note 2 -- the three vtable
	//  accessors at 0x1003abb0/c0/d0 all read `this->p18->w40/w28/w2a`,
	//  and w40, w28 and w2a are three of the stores below.
	// ------------------------------------------------------------------ //
	pRec->m_pNext30    = LTNULL;
	pRec->m_nFlags49   = (uint8)nFlags;
	pRec->m_nStage42   = (uint8)pReq->m_nStage08;

	pRec->m_fInvW04    = info.m_fInvW04;
	pRec->m_fInvH08    = info.m_fInvH08;
	pRec->m_pSurface0c = (DDSurface7*)info.m_pSurface0c;
	pRec->m_nBytes10   = info.m_nBytes10;
	pRec->m_nStamp14   = info.m_nStamp14;
	pRec->m_nAlphaRef16 = info.m_nAlphaRef16;

	pRec->m_nFormatIndex48 = (uint8)nFormat;
	pRec->m_nMipCount47    = (uint8)nMipCount;
	pRec->m_pSelf18        = pRec;
	pRec->m_nFirstMip46    = (uint8)nFirstMip;

	// 0x10038d25 loads 1.0f early and 0x10038d66 adds pData+0x26 to it.
	// COIN-FLIP, and one build settles it: `1.0f + pData->m_fScaleBias26`
	// (written) against `pData->m_fScaleBias26 + 1.0f`.  icl emits
	// `flds <const> ; fadds <mem>`, which is load-left-add-right for a
	// two-term sum; the last-term-backwards rule is for
	// three terms and up, so the written form is the one expected.
	pRec->m_fScale34 = 1.0f + pData->m_fScaleBias26;

	// ------------------------------------------------------------------ //
	//  REGION 10 -- 0x10038d6c..0x10038da3.  The UV rotation.
	//
	//  WHAT PINS IT: the angle is assembled BY HAND out of two bytes --
	//  `movzbl 0x2b ; movzbl 0x2a ; shl $0x8,%eax ; add %eax,%edx ;
	//  movswl %dx,%edx` -- and icl would have emitted a single
	//  `movswl 0x2a(%ebx)` for a short member, so the source really does
	//  combine two byte fields and cast the sum to a signed 16-bit value.
	//  `add %eax,%edx` has the LOW byte as its destination, so the low
	//  byte is the source's left operand.
	//
	//  0x100a93fc is 0x3c8efa35 = 0.017453292f, pi/180 -- which is what
	//  makes the field an angle in DEGREES.  The constant was read out of
	//  .data1 rather than recognised by eye.
	//
	//  fcos AND fsin ON THE SAME VALUE: `fld %st(0) ; fxch ; fstpl -0x10 ;
	//  fcos ; fldl -0x10 ; fxch ; fstps 0x38 ; fsin ; fstps 0x3c`.  The
	//  spill slot is eight bytes (`fstpl`/`fldl`), which is why fTheta is
	//  declared double; the two results are stored as floats (`fstps`).
	//  COS goes to +0x38 and SIN to +0x3c -- the order is unambiguous
	//  because fcos runs first and its result is the first thing stored.
	// ------------------------------------------------------------------ //
	nAngle = (int16)(pData->m_nAngleLo2a + (pData->m_nAngleHi2b << 8));

	fTheta = nAngle * R_DEG2RAD;

	pRec->m_fCos38 = (float)cos(fTheta);
	pRec->m_fSin3c = (float)sin(fTheta);

	// ------------------------------------------------------------------ //
	//  REGION 11 -- 0x10038da4..0x10038e04.  Total the bytes the mip
	//  chain occupies.
	//
	//  WHAT PINS IT: `mov 0x1008e6b0(,%eax,4),%eax` with %eax = nFormat
	//  indexes the SAME table by the SAME value big3a080.cpp indexes with
	//  RTexture::m_nFormatIndex, and `mov 0x2c(%eax),%edi` then
	//  `shl %cl,%edx` uses the SAME +0x2c field big3a080.cpp calls
	//  m_nPixelShift and also uses as a shift on a pixel count.
	//
	//  The loop is pointer-walked, not indexed: `imul $0x18,%edx,%edx ;
	//  imul $0x18,%eax,%eax` turn both bounds into byte offsets, %esi
	//  holds the end pointer and %eax the cursor, and the exit is
	//  `cmp %esi,%eax ; jb` -- UNSIGNED.  %ebx is spilled to ebp-0x08 for
	//  the duration because the loop needs it for the record.
	//
	//  `movl $0x0,0x10(%esi)` at 0x10038db3 RE-ZEROES m_nBytes10, which
	//  region 9 has just copied out of the info block seven instructions
	//  earlier.  icl does no dead-store elimination through memory, so
	//  the shipped code keeping both is expected -- but the two being
	//  separated by the copy means they are two source statements, and the
	//  second is the accumulator's initialiser.
	// ------------------------------------------------------------------ //
	pFmtRec = g_1008e6b0[nFormat];

	pRec->m_nBytes10 = 0;

	for (i = nFirstMip; i < (uint32)(nFirstMip + nMipCount); i++)
	{
		pRec->m_nBytes10 += (pData->m_Mips[i].m_nWidth *
		                     pData->m_Mips[i].m_nHeight)
		                    << pFmtRec->m_nPixelShift;
	}

	// ------------------------------------------------------------------ //
	//  REGION 12 -- 0x10038e05..0x10038e42.  Publish the record.
	//
	//  WHAT PINS IT: `mov (%eax),%eax ; mov %eax,0x2c(%esi)` stores the
	//  shared texture into +0x2c (big6d910.cpp's m_p2c), and the guarded
	//  `mov %esi,0xc(%eax)` stores the record back into the shared
	//  texture's +0x0c -- the back-pointer, which is what makes the whole
	//  function a "create and hang".  The guard is `test $0x1,%dl` on
	//  argument 2 and it SKIPS the store when the bit is set, so the bit
	//  means "do not publish".  The pointer is RELOADED for the second
	//  store (`mov -0x18(%ebp),%eax ; mov (%eax),%eax` at 0x10038e15,
	//  after it was already in %eax), which is icl declining to keep it
	//  across the branch.
	//
	//  `xor %edx,%edx ; test $0x1,%al ; setne %dl ; mov %dx,0x40(%esi)`
	//  is a boolean, not a copy of the bit: `setne` normalises to 0 or 1.
	//
	//  +0x28 and +0x2a come from mip level ZERO -- `movzwl 0xcc(%ebx)` and
	//  `movzwl 0xd0(%ebx)` with NO index term -- and they are the full
	//  texture's dimensions, not the chosen level's.  big6d910.cpp's
	//  vtable slots 0x1003abc0 and 0x1003abd0 return exactly these two.
	// ------------------------------------------------------------------ //
	pRec->m_pShared2c = pReq->m_pShared00;

	if (!(nFlags & RTEX_NOLINK))
		pReq->m_pShared00->m_pRTexture0c = pRec;

	pRec->m_n40 = (uint16)((pData->m_nFlags18 & TD_FLAG_01) != 0);

	pRec->m_nWidth28  = (uint16)pData->m_Mips[0].m_nWidth;
	pRec->m_nHeight2a = (uint16)pData->m_Mips[0].m_nHeight;

	// ------------------------------------------------------------------ //
	//  REGION 13 -- 0x10038e43..0x10038e80.  Link into the LRU list,
	//  charge the byte count and return.
	//
	//  WHAT PINS IT: `movl $0x1008e634,0x1c(%esi)` plants the head's
	//  ADDRESS as m_pPrev and `mov 0x1008e638,%eax` LOADS the dword four
	//  bytes past it as m_pNext, so 0x1008e634 is an LTLink object whose
	//  m_pNext is at 0x1008e638 -- and the two stores that follow
	//  (`mov %edx,(%eax)` then a RELOAD of +0x1c and `mov %edx,0x4(%eax)`)
	//  are ltlink.h's CheapLTLink::AddAfter with its double assignment
	//  evaluated right to left.  d3dr3d.cpp decodes the identical block at
	//  0x1006d460 and big6d910.cpp the identical block at 0x1006f21f,
	//  including the reload; this is the third copy and it is not
	//  re-derived here.
	//
	//  `mov %esi,0x24(%esi)` is the LTLink's m_pData, which is what makes
	//  the link 12 bytes rather than 8 and puts +0x28 where big6d910.cpp
	//  found it.  It is emitted BETWEEN the two link stores; d3dr3d.cpp's
	//  copy has it ahead of the insert, which is dl_AddAfter's order, so
	//  the interleave here is scheduling and the source order is the one
	//  written below.
	//
	//  `mov 0x1008cbcc,%eax ; mov 0x10(%esi),%edx ; add %edx,0x50(%eax)`
	//  charges the record's byte count to the RenderStruct's +0x50
	//  counter.  big6d910.cpp and big43e10.cpp add the SAME field into
	//  +0x48 of the same object, so the two counters are distinct and this
	//  one is read off the displacement, not carried over.
	//
	//  The return value is `mov %esi,%eax` at 0x10038e69, then the
	//  destructor store and the epilogue.
	// ------------------------------------------------------------------ //
	pRec->m_Link1c.m_pData = pRec;

	g_1008e634.AddAfter(&pRec->m_Link1c);

	g_p1008cbcc->m_nTextureBytes50 += pRec->m_nBytes10;

	return pRec;
}


// *********************************************************************** //
//  COVERAGE -- every byte of 0x10038a90..0x10038f42 accounted for.
//
//  THERE ARE NO `// UNDECODED:` GAPS IN THIS FILE.
//
//    0x10038a90..0x10038b13   region 1   format cascade
//    0x10038b14..0x10038b68   region 2   memset, mip base, format probe
//    0x10038b69..0x10038b99   region 3   the two base-level clamps
//    0x10038b9a..0x10038bf0   region 4   the size caps
//    0x10038bf1..0x10038c3c   region 5   the level search + LTMAX
//    0x10038c3d..0x10038c6d   region 6   the level-count clamp
//    0x10038c6e..0x10038c95   region 7   sub_10038630
//    0x10038c96..0x10038cad   epilogue   `return LTNULL` after region 7
//    0x10038cae..0x10038cc5   epilogue   `return LTNULL`, nAvail == 0
//    0x10038cc6..0x10038cdd   epilogue   `return LTNULL`, nLevel == -1
//    0x10038cde..0x10038ce4   region 5   the loop's `break` trampoline
//    0x10038ce5..0x10038d0a   region 8   bank pop + constructor
//    0x10038d0b..0x10038d6b   region 9   the info copy and the scalars
//    0x10038d6c..0x10038da3   region 10  the UV rotation
//    0x10038da4..0x10038e04   region 11  the byte-count loop
//    0x10038e05..0x10038e42   region 12  publish
//    0x10038e43..0x10038e80   region 13  link, charge, `return pRec`
//    0x10038e81..0x10038ea4   region 8   Release + `return LTNULL` (sunk)
//    0x10038ea5..0x10038ed4   region 8   RTexBank::Alloc's refill (sunk)
//    0x10038ed5..0x10038ede   region 6   `nCount = 4` (sunk)
//    0x10038edf..0x10038eec   region 3   clamp 2's low arm (sunk)
//    0x10038eed..0x10038ef3   region 3   clamp 1's high arm (sunk)
//    0x10038ef4..0x10038efa   region 3   clamp 1's low arm (sunk)
//    0x10038efb..0x10038f04   region 2   `nFmt = 3` (sunk)
//    0x10038f05..0x10038f2a   region 1   the format-0 precondition (sunk)
//    0x10038f2b..0x10038f42   region 1   `return LTNULL`, no format 6
//
// ----------------------------------------------------------------------- //
//  CALL-FREE COVERAGE -- WHAT THE CALL SEQUENCE CANNOT SEE, STATED EXPLICITLY.
//
//  This belongs beside any call-sequence figure, and here
//  it dominates: the function makes FIVE calls in 1,203 bytes, 241 bytes
//  per call, which is inside the weak band.  The five sit
//  at 0x10038b57, 0x10038c8a, 0x10038d06, 0x10038e8a and 0x10038eb9.  The
//  call-free stretches between and around them are:
//
//    0x10038a90..0x10038b56    199 bytes   regions 1-2 to the first call
//    0x10038b5c..0x10038c89    302 bytes   regions 3-6, ALL THE ARITHMETIC
//    0x10038c8f..0x10038d05    119 bytes   the three failure exits + pop
//    0x10038d0b..0x10038e89    383 bytes   regions 9-13, THE WHOLE FILL
//    0x10038e8f..0x10038eb8     42 bytes   an epilogue and the refill head
//    0x10038ebe..0x10038f42    133 bytes   the six sunk cold blocks
//
//  So 1,178 of the 1,203 bytes -- 98% -- the clamps, the caps, the two
//  loops, the whole record fill and the list insert -- are invisible to the
//  call check.  THE CALL SEQUENCE IS NOT THE EVIDENCE FOR THIS FUNCTION.
//  What carries it is the EVIDENCE block at the top of the file, and in
//  particular:
//
//    * fourteen record offsets that four OTHER files already fixed
//      from the consumer side, every one of which this function stores
//      (note 1);
//    * four vtable slots whose bodies read exactly the three fields this
//      function writes, through exactly the self-pointer it plants
//      (note 2);
//    * a base-vtable store on all six exits, which is what makes ebp-0x58
//      an object with a destructor (note 3);
//    * a 128-byte frame that decomposes into nine slots with nothing left
//      over, and 0x1c for the info block read twice over -- once from the
//      clear and once from the copy (note 4);
//    * ten sunk cold blocks, each one an arm of one of the six named
//      conditionals, with no residue (note 5);
//    * a 0x18 mip stride at base +0xcc in both loops, matching
//      big3a080.cpp's independent decode (note 6).
//
// ----------------------------------------------------------------------- //
//  PREDICTED CALL-SEQUENCE RESULT, so that a mismatch can be read rather than
//  investigated.
//
//  Shipped order (disassembly order):
//      0x100385e0, 0x10038630, 0x1003abe4, <indirect>, 0x1007fbf0
//
//  The last two are in COLD BLOCKS sunk below the epilogue.  If icl sinks
//  them the same way, ours comes out identical and the result is 5 of 5.
//  If it does not, ours comes out
//      0x100385e0, 0x10038630, 0x1007fbf0, <indirect>, 0x1003abe4
//  which is the benign block-placement pattern -- equal
//  counts on both sides, the odd rows being the two calls that live in cold
//  blocks -- and scores 3 of 5 with one shipped-only and one ours-only row
//  for 0x1003abe4 and 0x1007fbf0.  Either outcome is consistent with this
//  decode; a MISSING or EXTRA callee, or a target that is not one of these
//  four, is not.
//
//  One further known shape: 0x100385e0 is a REGISTER-ABI entry (its
//  argument arrives in %eax and it opens no frame), which d3dbatcha.cpp
//  records as "cannot be spelled".  It is decoded as an
//  ordinary call whose argument arrives in %eax, which is what is done
//  here; the emitted call will therefore pass its argument on the stack
//  and the BYTES around it will differ even though the call itself lines
//  up.  The same is true of the entry to this function.
// *********************************************************************** //
