// ======================================================================= //
//  w7_10040cc0.cpp -- d3d.ren functions written for the whole-DLL link
//.  /O2: every function here is followed by multi-byte lea
//  padding to a 16-byte boundary.
// ======================================================================= //
#include "ltbasedefs.h"


// ======================================================================= //
//  0x10040cc0 -- 272 bytes.  The per-light callback that 0x10040dd0
//  (d3dr4a_o2.cpp) registers with sub_1007b8e0: collect one dynamic light
//  into the frame's light array, then query the world for everything its
//  sphere touches.
//
//      55 8b ec / 83 ec 38                 13 dwords of locals
//      33 c0 / 89 45 d4 .. 89 45 cc        the 0x34-byte query block,
//                                          constructed inline (0x10015830's
//                                          members, d3dr5b.cpp)
//      8b 45 0c                            the SECOND argument is the light
//      test $0x20,0x88(light) ; jne        not flagged 0x20 and
//      cmp $0x28,g_1008bd84 ; jae          fewer than 40 collected:
//        g_1008bd88[g_1008bd84++] = light
//      test $0x80,0x88(light) ; jne ret
//      test g_1008beac ; je ret
//      min = pos(0x18c) - radius(0x1a8); max = pos + radius
//      query.+0x30 = light ; query.+0x2c = 0x10040ca0
//      g_1008beac->+0x6c ->vtbl[2](&query)
//
//  The first argument is never read.  Plain `ret`: __cdecl.  The
//  reference in d3dr4a_o2.cpp is spelled `void sub_10040cc0()` (only the
//  address is taken there); link/aliases.rsp maps it onto this one.
//
//  IDENTICAL (was 39 bytes differing).  Three things:
//    * the query block is built by its (inline) constructor -- +0x00,
//      +0x04, the three vectors' Init(0,0,0), +0x2c, +0x30 -- which gives
//      the shipped store order (vectors first, then +0x30, +0x2c, +0x04,
//      +0x00); the nine member stores written out come out in another;
//    * the callback is stored before the user pointer (icl schedules them
//      the other way round from source);
//    * the shipped layout parks the g_1008beac == NULL epilogue after the
//      body and falls into the query.  Every `if` spelling (`!g`, `== 0`,
//      an integer read, volatile, a local, a wrapping if, `&&`, `||`,
//      nested ifs, an inline helper, goto) puts the query out of line
//      instead; a `switch` on the comparison whose non-zero arm returns is
//      what icl sinks (the lever of 0x1007ebe0 in d3dbatcha.cpp).
// ======================================================================= //
struct C1001e210
{
	uint8		m_pad_00[0x88];                         // +0x00
	uint32		m_dwFlags88;                            // +0x88
	uint8		m_pad_8c[0x18c - 0x8c];                 // +0x8c
	float		m_vPos18c[3];                           // +0x18c
	uint8		m_pad_198[0x1a8 - 0x198];               // +0x198
	float		m_fRadius1a8;                           // +0x1a8
};

struct W7Vec40cc0
{
	float x, y, z;

	void Init(float a, float b, float c) { x = a; y = b; z = c; }
};

// The 0x10015830 query block (big19d64.cpp's Query10015830, d3dr5b.cpp's
// C10015830), with its constructor written inline (the store order needs it).
struct W7Query40cc0
{
	uint32		m_n00;                                  // +0x00
	uint32		m_n04;                                  // +0x04
	W7Vec40cc0	m_vMin08;                               // +0x08
	W7Vec40cc0	m_vMax14;                               // +0x14
	W7Vec40cc0	m_v20;                                  // +0x20
	void*		m_pfnCallback2c;                        // +0x2c
	void*		m_pUser30;                              // +0x30

	W7Query40cc0()
	{
		m_n00 = 0;
		m_n04 = 0;
		m_vMin08.Init(0, 0, 0);
		m_vMax14.Init(0, 0, 0);
		m_v20.Init(0, 0, 0);
		m_pfnCallback2c = 0;
		m_pUser30 = 0;
	}
};

class W7GridQuery40cc0
{
public:
	virtual void	Slot0();
	virtual void	Slot1();
	virtual void	Slot2(W7Query40cc0* pQuery);
};

struct S1001e520Root
{
	uint8				m_pad_00[0x6c];                 // +0x00
	W7GridQuery40cc0	m_Query6c;                      // +0x6c
};

// 0x10040ca0 -- d3dqip.cpp; only its address is taken here.
struct Obj10040ca0;
struct Ctx10040938;
extern void sub_10040ca0(Obj10040ca0* pObj, Ctx10040938* pCtx);
extern S1001e520Root*		g_1008beac;                     // 0x1008beac
extern unsigned long	g_1008bd84;                     // 0x1008bd84
extern C1001e210*	g_1008bd88[40];                 // 0x1008bd88

void sub_10040cc0(void* pUnused, C1001e210* pLight)
{
	W7Query40cc0 query;

	if (!(pLight->m_dwFlags88 & 0x20) && g_1008bd84 < 40)
		g_1008bd88[g_1008bd84++] = pLight;

	if (pLight->m_dwFlags88 & 0x80)
		return;
	switch (g_1008beac == LTNULL)
	{
		case 0:  break;
		default: return;
	}

	query.m_vMin08.Init(pLight->m_vPos18c[0] - pLight->m_fRadius1a8,
	                    pLight->m_vPos18c[1] - pLight->m_fRadius1a8,
	                    pLight->m_vPos18c[2] - pLight->m_fRadius1a8);
	query.m_vMax14.Init(pLight->m_vPos18c[0] + pLight->m_fRadius1a8,
	                    pLight->m_vPos18c[1] + pLight->m_fRadius1a8,
	                    pLight->m_vPos18c[2] + pLight->m_fRadius1a8);
	query.m_pfnCallback2c = (void*)sub_10040ca0;
	query.m_pUser30 = pLight;

	g_1008beac->m_Query6c.Slot2(&query);
}


// ======================================================================= //
//  The Direct3D device, as the C COM interface d3dr5b.cpp uses (slot +0x50
//  SetRenderState, +0x54 GetRenderState, +0x64 DrawPrimitive, +0x8c SetTexture, +0x94
//  SetTextureStageState of
//  IDirect3DDevice7).  The tag is the one d3ddata.cpp defines 0x1008d988
//  under, so the reference needs no alias.
// ======================================================================= //
struct D3DDevTSSR5C;
struct W7DevVtbl
{
	void*	m_Slots00[20];                                              // +0x00
	long (__stdcall* SetRenderState)(D3DDevTSSR5C*, uint32 nState,
	                                 uint32 nValue);                    // +0x50
	long (__stdcall* GetRenderState)(D3DDevTSSR5C*, uint32 nState,
	                                 uint32* pValue);                   // +0x54
	void*	m_Slots58[3];                                               // +0x58
	long (__stdcall* DrawPrimitive)(D3DDevTSSR5C*, uint32 nType, uint32 nFVF,
	                                void* pVerts, uint32 nVerts,
	                                uint32 nFlags);                     // +0x64
	void*	m_Slots68[9];                                               // +0x68
	long (__stdcall* SetTexture)(D3DDevTSSR5C*, uint32 nStage,
	                             void* pTexture);                       // +0x8c
	void*	m_Slot90;                                                   // +0x90
	long (__stdcall* SetTextureStageState)(D3DDevTSSR5C*, uint32 nStage,
	                                       uint32 nType, uint32 nValue); // +0x94
};
struct D3DDevTSSR5C { W7DevVtbl* lpVtbl; };

extern D3DDevTSSR5C* g_1008d988;                                    // 0x1008d988

// A render state set for the lifetime of the object and put back by its
// destructor: the constructor reads the old value then writes the new one,
// the destructor writes the old one back.  In 0x10043a80 four of these
// restore in REVERSE order at the end, which is C++ destruction order.
struct W7RenderStateSave
{
	uint32	m_nState;                                   // +0x00
	uint32	m_nOld;                                     // +0x04

	W7RenderStateSave(uint32 nState, uint32 nValue)
	{
		m_nState = nState;
		g_1008d988->lpVtbl->GetRenderState(g_1008d988, m_nState, &m_nOld);
		g_1008d988->lpVtbl->SetRenderState(g_1008d988, m_nState, nValue);
	}
	~W7RenderStateSave()
	{
		g_1008d988->lpVtbl->SetRenderState(g_1008d988, m_nState, m_nOld);
	}
};


// ======================================================================= //
//  0x10043a80 -- 738 bytes (0x10043a80..0x10043d61, then lea padding).
//  The enumerator gives it size 0: it merged it into 0x10043a30, which
//  d3dr4c.cpp shows is 73 bytes and ends in `ret` at 0x10043a78.  This is
//  the callback 0x10043d70 (d3dr3c.cpp) hands the ring buffer at
//  0x1008ef3c with each entry: draw one object's list through 0x10015c70
//  under the blend mode its flags ask for.
//
//      SetTSS(1,1,1) ; SetTSS(1,2,2) ; SetTSS(1,3,1)
//      GetRenderState(0x22 FOGCOLOR, &fog) ; GetRenderState(0x1c FOGENABLE, &fogon)
//      obj = second argument (0xc(%ebp))
//      +0x88 & 0x80        -> fogon = 0
//      +0x8c & 4           -> src 2, dst 2, fog colour 0      (additive)
//      +0x8c & 8           -> src 1, dst 3, fog colour ~0
//      else                -> src 5, dst 6                     (alpha)
//      four save/set pairs: 0x13 SRCBLEND, 0x14 DESTBLEND, 0x1c, 0x22
//      sub_10015c70(g_1008b880, obj)
//      SetTSS(1,1,1)
//      the four pairs restored 0x22, 0x1c, 0x14, 0x13
//
//  Plain `ret`, two dword arguments, the first never read: __cdecl.
// ======================================================================= //
struct Ctx1007b8e0;
extern void sub_10015c70(Ctx1007b8e0* pContext, void* pElem);      // 0x10015c70
extern unsigned char g_1008b880[28];                                // 0x1008b880

struct W7Obj43a80
{
	uint8		m_pad_00[0x88];                         // +0x00
	uint32		m_dwFlags88;                            // +0x88
	uint32		m_dwFlags8c;                            // +0x8c
};

void sub_10043a80(void* pUnused, W7Obj43a80* pObj)
{
	uint32 nFogColor;
	uint32 nFogEnable;
	uint32 nSrcBlend;
	uint32 nDestBlend;

	g_1008d988->lpVtbl->SetTextureStageState(g_1008d988, 1, 1, 1);
	g_1008d988->lpVtbl->SetTextureStageState(g_1008d988, 1, 2, 2);
	g_1008d988->lpVtbl->SetTextureStageState(g_1008d988, 1, 3, 1);

	g_1008d988->lpVtbl->GetRenderState(g_1008d988, 0x22, &nFogColor);
	g_1008d988->lpVtbl->GetRenderState(g_1008d988, 0x1c, &nFogEnable);

	if (pObj->m_dwFlags88 & 0x80)
		nFogEnable = 0;

	if (pObj->m_dwFlags8c & 4)
	{
		nSrcBlend  = 2;
		nDestBlend = 2;
		nFogColor  = 0;
	}
	else if (pObj->m_dwFlags8c & 8)
	{
		nSrcBlend  = 1;
		nDestBlend = 3;
		nFogColor  = 0xffffffff;
	}
	else
	{
		nSrcBlend  = 5;
		nDestBlend = 6;
	}

	W7RenderStateSave saveSrc(0x13, nSrcBlend);
	W7RenderStateSave saveDest(0x14, nDestBlend);
	W7RenderStateSave saveFogOn(0x1c, nFogEnable);
	W7RenderStateSave saveFogColor(0x22, nFogColor);

	sub_10015c70((Ctx1007b8e0*)g_1008b880, pObj);

	g_1008d988->lpVtbl->SetTextureStageState(g_1008d988, 1, 1, 1);
}


// ======================================================================= //
//  0x10047150 (the unclipped two-point line draw) now lives in
//  w31_10044ff0_qipni.cpp as a static beside its only caller: it is a /Qip
//  register-only entry (identical there).
// ======================================================================= //


// ======================================================================= //
//  0x10047b60 -- 387 bytes (`ret $4`, then lea padding).  The model
//  renderer's per-model state setup, called from 0x10044f74 (d3dbatcha_o2
//  .cpp) with the address of a local that it then hands to 0x10008da0:
//  the out-parameter is "draw this model fullbright".
//
//      g_f1008ac90 = g_f1008ac94 = 0 ; m_308 = 0
//      if (TextureModels && (m_694 & 1))   m_308 = 1
//      else                                unbind the current stage
//      GetRenderState(8 FILLMODE, &g_n1008ee00)
//      if (m_698 & 0x10)                   SetRenderState(8, 2 WIREFRAME)
//      GetRenderState(0x1b ALPHABLENDENABLE, &blend)
//      fullbright = g_1008d964 && ModelFullbrite && the bound texture
//                   exists && its slot-2 virtual says yes && !blend,
//      and ShowFullbriteModels turns it back off and unbinds.
//
//  The "unbind" block is the same eleven instructions both times (stage
//  re-read from 0x1008d7d0 into %esi, the cached pointer tested, SetTexture
//  (stage, 0), the cache cleared): one inline helper.
// ======================================================================= //
struct Tex10059590
{
	virtual void	Slot0();
	virtual void	Slot1();
	virtual int		Slot2();
};

extern float			g_f1008ac90;                    // 0x1008ac90
extern float			g_f1008ac94;                    // 0x1008ac94
extern long				g_CV_TextureModels;             // 0x100893d0
extern long				g_CV_ModelFullbrite;            // 0x100893e4
extern long				g_CV_ShowFullbriteModels;       // 0x1008cad8
extern unsigned long	g_1008d7d0;                     // 0x1008d7d0  the current stage
extern unsigned long	g_1008d964;                     // 0x1008d964
extern Tex10059590*		g_1008e684[1];                  // 0x1008e684  bound texture per stage
extern unsigned long	g_n1008ee00;                    // 0x1008ee00  saved FILLMODE

static inline void W7UnbindStageTexture()
{
	uint32 nStage = g_1008d7d0;

	if (g_1008e684[nStage])
	{
		g_1008d988->lpVtbl->SetTexture(g_1008d988, nStage, 0);
		g_1008e684[nStage] = 0;
	}
}

class C10044f74
{
public:
	void sub_10047b60(uint32* pbFullbright);

	uint8	m_pad_000[0x308];                           // +0x000
	uint32	m_n308;                                     // +0x308
	uint8	m_pad_30c[0x694 - 0x30c];                   // +0x30c
	uint32	m_dwFlags694;                               // +0x694
	uint32	m_dwFlags698;                               // +0x698
};

void C10044f74::sub_10047b60(uint32* pbFullbright)
{
	uint32 nBlendState;
	uint32 bBlend;
	uint32 bFullbright;

	// In the opposite order to the shipped stores: VC6 writes the second
	// of two stores of one register first (the data-reference check).
	g_f1008ac94 = 0.0f;
	g_f1008ac90 = 0.0f;
	m_n308 = 0;

	if (g_CV_TextureModels && (m_dwFlags694 & 1))
		m_n308 = 1;
	else
		W7UnbindStageTexture();

	g_1008d988->lpVtbl->GetRenderState(g_1008d988, 8, (uint32*)&g_n1008ee00);
	if (m_dwFlags698 & 0x10)
		g_1008d988->lpVtbl->SetRenderState(g_1008d988, 8, 2);

	g_1008d988->lpVtbl->GetRenderState(g_1008d988, 0x1b, &nBlendState);

	bFullbright = 0;
	bBlend = nBlendState;
	if (g_1008d964 && g_CV_ModelFullbrite && g_1008e684[g_1008d7d0] &&
	    g_1008e684[g_1008d7d0]->Slot2() && !bBlend)
	{
		bFullbright = 1;
		if (g_CV_ShowFullbriteModels)
		{
			bFullbright = 0;
			W7UnbindStageTexture();
		}
	}

	*pbFullbright = bFullbright;
}

// 0x1005d170, 0x1005d3e0 and 0x1005a630 were here; they are /Qip
// register-only statics of 0x1005a960 and moved into its TU,
// w40_1005a960_qipni.cpp, where the two fills are identical.
