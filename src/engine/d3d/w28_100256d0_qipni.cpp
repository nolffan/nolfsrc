// *********************************************************************** //
//
//  MODULE:   W28_100256D0_QIPNI (.CPP)  -- /O2 /Qip /Qip_no_inlining
//
//  PURPOSE:  The frame-draw translation unit, 0x100256d0..0x1002d1c0 --
//            the functions of one shipped TU that reach each other through
//            icl's /Qip register entries:
//
//              0x100256d0  the draw-slot selector (static)
//              0x10025890  the light-add pass (static)
//              0x10025db0  the screen-tint pass (static)
//              0x10026480/84  the mode switch (pair; calls 0x100256d0)
//              0x10026500/04  the renderer shutdown sequence (pair)
//              0x10026720/28  the mirror pass (pair; calls 0x10029894)
//              0x10029890/94  the frame teardown (pair; calls 0x10026484,
//                             0x10026504, 0x10025db0, 0x10025890)
//              0x1002b0c0  the mirror hook (calls 0x10026728)
//              0x1002cf40/48  the world-tree view (pair)
//              0x1002d1c0  RenderScene (calls 0x10029894, 0x1002cf48)
//
//  Made from d3dqip2.cpp's draw functions,
//  w15_100256d0.cpp, w15_10025890.cpp, big26728.cpp and big2d1c0.cpp,
//  whose section banners follow unchanged.  Each move is the register ABI
//  in the shipped bytes: a static callee with no stack entry (0x100256d0,
//  0x10025890, 0x10025db0) or a caller entering a pair at +4/+8
//  (0x10029894 from 0x10026728 and 0x1002d1c0; 0x1002cf48 from
//  0x1002d1c0) needs caller and callee in one TU.
//
//  WHY /Qip_no_inlining.  Plain /Qip (d3dqip2.cpp's flags) inlines
//  0x10029890 into both 0x10026728 and 0x1002d1c0 and 0x1002cf40 into
//  0x1002d1c0, where the shipped callers CALL them; declaring them after
//  their callers (the shipped order) does not stop it.  With
//  /Qip_no_inlining (explicit `inline` still expands): 0x10029890 /
//  0x10029894 and the 0x1002cf40 shim byte-identical, 0x10026728 4058 ->
//  3944 bytes differing, nothing else in the group changed.  The texture
//  functions (0x10038f50..0x1003ad30) stay in d3dqip2.cpp under plain
//  /Qip: shipped 0x1003ad30 has 0x10039420 inlined.
//
// *********************************************************************** //

#include "ltbasedefs.h"

#include <math.h>
#include <string.h>


// ======================================================================= //
//  THE PLACEHOLDER MACHINERY
// ======================================================================= //

// (LL) The placeholder machinery (g_qip_nSink, QIP_MIX, QIP_BODY) is gone:
// every qip_Callee stand-in in this file now calls the real function (see
// each declaration), so nothing here runs a wrong body any more.


// ======================================================================= //
//  SHARED DECLARATIONS
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  The Direct3D 7 device at 0x1008d988, repeated from
//  src/engine/d3d/d3dr2d.cpp (separate translation units, so it is
//  repeated rather than shared).  Slot numbering, all already pinned in
//  this repo: SetRenderState is index 20 (+0x50), GetRenderState is index
//  21 (+0x54), SetTexture is index 35 (+0x8c).  The
//  `mov (%eax),%edx ; call *0xNN(%edx)` with the object written as the
//  first stack argument is the function-pointer-struct model the rest of
//  src/engine/d3d uses for COM.
// ---------------------------------------------------------------------- //
struct D3DDevice7;

struct DDSurface;

struct D3DDevice7Vtbl
{
	uint8 m_pad_00[0x20];
	long (__stdcall* SetRenderTarget)(D3DDevice7*, DDSurface* pTarget,
	                                  uint32 dwFlags);                // +0x20
	uint8 m_pad_24[0x50 - 0x24];
	long (__stdcall* SetRenderState)(D3DDevice7*, uint32 dwState,
	                                 uint32 dwValue);                 // +0x50
	long (__stdcall* GetRenderState)(D3DDevice7*, uint32 dwState,
	                                 uint32* lpdwValue);              // +0x54
	uint8 m_pad_58[0x0c];
	long (__stdcall* DrawPrimitive)(D3DDevice7*, uint32 dwPrimType,
	                                uint32 dwVertexType, void* pVerts,
	                                uint32 dwCount, uint32 dwFlags);  // +0x64
	uint8 m_pad_68[0x24];
	long (__stdcall* SetTexture)(D3DDevice7*, uint32 dwStage,
	                             void* lpTexture);                    // +0x8c
};

struct D3DDevice7
{
	D3DDevice7Vtbl* lpVtbl;
};

extern D3DDevice7* g_pD3DDevice;                        // 0x1008d988

extern uint32 g_1008d7d0;                               // current stage
extern void*  g_a1008e684[];                            // bound textures


// ======================================================================= //
//  0x10026480   128 bytes (0x10026480..0x100264ff)      2 callers
//
//  DECODED, every instruction.  A mode switch: it retargets four function
//  pointers through its callee, sets three flags from the mode, and then
//  runs one of two fixups.
//
//      10026480  mov 0x4(%esp),%eax             <- the STACK entry
//      10026484  push %ebx / sub $0x8,%esp      <- the REGISTER entry
//                mov %eax,%ebx / mov %ebx,%eax
//                call 0x100256d0                ; one argument, in %eax
//                cmp $0x1,%ebx / je 0x100264ef
//                movl $0x0,0x1008ce7c / movl $0x1,0x1008ce80
//      100264aa  mov 0x1008d970,%eax / test / je 0x100264c4
//                cmp $0x1,%ebx / je 0x100264eb
//                mov $0x1,%eax
//      100264bd  mov %eax,0x1008ceac / jmp 0x100264ce
//      100264c4  movl $0x0,0x1008ceac
//      100264ce  mov 0x1008d96c,%eax / test / je 0x100264e1
//                call 0x10073210 / add $0x8,%esp / pop %ebx / ret
//      100264e1  call 0x100145f0 / add $0x8,%esp / pop %ebx / ret
//      100264eb  xor %eax,%eax / jmp 0x100264bd
//      100264ef  mov $0x1,%eax / mov %eax,0x1008ce7c
//                mov %eax,0x1008ce80 / jmp 0x100264aa
//
//  WHAT PINS IT:
//    * the two-entry pair: 0x10026480 is four bytes of `mov 0x4(%esp),%eax`
//      in front of 0x10026484, and 0x10029890 below calls 0x10026484
//      DIRECTLY.  So this is an external-linkage /Qip function of ONE
//      argument, and 0x10029890 is in its translation unit.
//    * 0x100256d0 has no shim of its own (0x100256cc is zero padding), so
//      it is static; it reads its argument at 0x1002579d (`cmp $0x1,%eax`)
//      and returns nothing.  The 8 bytes `sub $0x8,%esp` reserves in the
//      prologue ARE that call's outgoing area -- 4 bytes for one argument,
//      padded to 8 -- which is why the epilogue frees 8 after two calls
//      that push nothing.
//    * 0x100264bd is ONE store reached from both arms through %eax, so the
//      inner if/else is a value-producing expression -- a `?:`, not two
//      separate `movl $K,0x1008ceac`.  The outer arm at 0x100264c4 IS a
//      separate `movl $0x0`, so that one is a statement.
//    * `mov $0x1,%eax` (b8 01 00 00 00) not `xor;inc`, `add $-N,%esp` for
//      outgoing arguments and the DUPLICATED epilogue at 0x100264dc and
//      0x100264e6: /O2.
//    * 0x100145f0 is a bare `c3`, an empty function; 0x10073210 is a real
//      body.  Neither takes an argument.
//
//  THE SUNK `nMode == 1` ARMS (both of them, the if/else and the `?:`)
//  are icl predicting a SIGNED `== K` false.  IDENTICAL
//  (was 73 bytes differ) with nMode int32 and 0x100256d0 moved in as
//  a static: with uint32 the arms stay inline, and the negated spelling
//  (`!= 1` with the arms swapped) keeps them inline too.
// ======================================================================= //

extern uint32 g_1008ce7c;                               // 0x1008ce7c
extern uint32 g_1008ce80;                               // 0x1008ce80
extern uint32 g_1008ceac;                               // 0x1008ceac
extern uint32 g_1008d970;                               // 0x1008d970
extern uint32 g_1008d96c;                               // 0x1008d96c

extern void sub_10073210();                             // 0x10073210
extern void sub_100145f0();                             // 0x100145f0

// ======================================================================= //
//  0x100256d0 -- 250 bytes.  The draw-slot selector 0x10026480 calls with
//  the mode: fills the four polygon draw slots 0x1008ce94/98/9c/a0 from
//  the device capability and the debug console variables.
//
//  IDENTICAL HERE (was w15_100256d0.cpp, /O2, 82 bytes
//  differ).  In the shipped image it is a /Qip STATIC of 0x10026480's TU:
//  no stack entry, the mode arrives in %eax (0x1002579d `cmp $0x1,%eax`).
//  Moved here as a static it gets the register entry and is not inlined
//  (15 bytes then differed).  The last two were block placement: icl sinks
//  the two final `if` arms below the `ret` exactly when it predicts their
//  test FALSE, and it predicts `x == K` false for a SIGNED int only (an
//  uint32 `nMode == 1` was laid out inline) and `!p` false for a POINTER
//  only (a uint32 `!m_00` was laid out inline).  So nMode is int32 and
//  +0x00 of 0x1008beb8's record is a pointer (big0ae20.cpp reads it as the
//  current texture block, which agrees).
// ======================================================================= //
struct Poly10066d1c;
typedef void (*DrawSlotFn)(Poly10066d1c* pPoly);

extern DrawSlotFn g_1008ce94;                           // 0x1008ce94  textured
extern DrawSlotFn g_1008ce98;                           // 0x1008ce98  untextured
extern DrawSlotFn g_1008ce9c;                           // 0x1008ce9c  flag 0x80
extern DrawSlotFn g_1008cea0;                           // 0x1008cea0

struct C1000da50Data
{
	void*   m_p00;
	uint32  m_04;
	uint32  m_08;
	float   m_0c;
	float   m_10;
};

extern uint32          g_1008d978;                      // 0x1008d978
extern int32           g_CV_ShowFillInfo;               // 0x1008cacc   ShowFillInfo
extern int32           g_CV_DrawFlat;                   // 0x1008caa4   DrawFlat
extern int32           g_CV_LightMap;                   // 0x10089400   LightMap
struct Ctx1008beac;
extern Ctx1008beac*    g_p1008beac;                     // 0x1008beac  the world
extern C1000da50Data*  g_1008beb8;                      // 0x1008beb8

// The draw routines the slots hold -- every one `void (*)(Obj*)`, one
// stack argument (big24020.cpp / big24090.cpp).
struct Obj24090;
struct Obj73140;
extern "C" void sub_10024020(void* pObj);               // 0x10024020  defer
extern void     sub_10024090(Obj24090* pObj);           // 0x10024090
extern void     sub_10073140(Obj73140* pObj);           // 0x10073140
extern void     sub_1003dc80(void* pPoly);              // 0x1003dc80  w15_1000d9f0.cpp
extern void     sub_1000d9f0(void* pPoly);              // 0x1000d9f0  w15_1000d9f0.cpp
extern void     sub_1000f650(void* pPoly);              // 0x1000f650  w15_1000f650_os.cpp


// ======================================================================= //
//  0x100256d0 -- 250 bytes.
// ======================================================================= //
static void sub_100256d0(int32 nMode)
{
	DrawSlotFn pfn;

	g_1008ce94 = (DrawSlotFn)sub_1003dc80;
	g_1008ce98 = (DrawSlotFn)sub_10024020;
	g_1008cea0 = (DrawSlotFn)sub_1000d9f0;

	if (g_1008d96c)
	{
		pfn = (DrawSlotFn)sub_10073140;
		g_1008ce9c = pfn;
		g_1008cea0 = pfn;
	}
	else
	{
		g_1008ce9c = (DrawSlotFn)sub_1000f650;
	}

	if (g_CV_ShowFillInfo)
	{
		pfn = (DrawSlotFn)sub_10024090;
		g_1008ce98 = pfn;
		g_1008ce94 = pfn;
		g_1008ce9c = pfn;
		g_1008cea0 = pfn;
	}
	else if (g_CV_DrawFlat)
	{
		pfn = (DrawSlotFn)sub_10024020;
		g_1008ce98 = pfn;
		g_1008ce94 = pfn;
		g_1008ce9c = pfn;
		g_1008cea0 = pfn;
	}
	else if (!g_CV_LightMap || !g_1008d978 ||
	         !(*(uint32*)((uint8*)g_p1008beac + 0x180) & 4))
	{
		pfn = (DrawSlotFn)sub_1003dc80;
		g_1008cea0 = pfn;
		g_1008ce9c = pfn;
	}

	if (nMode == 1)
	{
		pfn = (DrawSlotFn)sub_1003dc80;
		g_1008cea0 = pfn;
		g_1008ce9c = pfn;
	}

	if (!g_1008beb8->m_p00)
		g_1008cea0 = (DrawSlotFn)sub_1003dc80;
}


void sub_10026480(int32 nMode)
{
	sub_100256d0(nMode);

	if (nMode == 1)
	{
		g_1008ce7c = 1;
		g_1008ce80 = 1;
	}
	else
	{
		g_1008ce7c = 0;
		g_1008ce80 = 1;
	}

	if (g_1008d970)
		g_1008ceac = (nMode == 1) ? 0 : 1;
	else
		g_1008ceac = 0;

	if (g_1008d96c)
		sub_10073210();
	else
		sub_100145f0();
}


// ======================================================================= //
//  0x10029890   280 bytes (0x10029890..0x1002999a)      2 callers
//
//  DECODED, every instruction.  A frame teardown: one render state, three
//  fixed calls, then either a per-object sweep or a pair of mode calls.
//
//      10029890  mov 0x4(%esp),%eax                       <- STACK entry
//      10029894  push %ebp / mov %esp,%ebp / sub $0x20,%esp <- REGISTER
//                mov %eax,%edi
//                mov 0x1008d988,%eax                      ; loaded FIRST
//                mov 0x1008d7a0,%edx / test / je 0x100298bd
//                mov $0x1,%edx / jmp 0x100298bf
//      100298bd  xor %edx,%edx
//      100298bf  add $0xfffffff4,%esp
//                movl $0x88,0x4(%esp) / mov %edx,0x8(%esp)
//                call *0x50(%edx)                         ; SetRenderState
//                call 0x10026670 / call 0x10050390 / call 0x1001eb20
//                cmpl $0x2,(%edi) / je 0x1002993a
//                add $0xfffffff8,%esp / xor %eax,%eax
//                call 0x10026484 / add $0x8,%esp
//                call 0x1007b880
//                add $0xfffffff8,%esp / xor %eax,%eax
//                call 0x10026504 / add $0x8,%esp
//      10029909  call 0x1004fbc0 / call 0x1001eb20
//                lea 0x50(%edi),%eax / add $0xfffffff8,%esp
//                call 0x10025db0
//                add $0x5c,%edi / mov %edi,%eax
//                call 0x10025890 / add $0x8,%esp / <epilogue>
//      1002993a  call 0x1007b8d0 / mov %eax,%ecx / call 0x1007bae0
//                mov 0xe0(%edi),%eax / mov 0xdc(%edi),%edx
//                test %eax,%eax / jle 0x10029909
//                mov %edx,%ebx / lea (%edx,%eax,4),%esi
//      10029960  mov (%ebx),%eax / test / je 0x10029993
//                movsbl 0xbc(%eax),%edx
//                test %edx,%edx / jl 0x10029993
//                cmp $0xb,%edx / jge 0x10029993
//                lea (%edx,%edx,4),%edx / mov %edx,%ecx / add %ecx,%ecx
//                mov 0x100895c0(%ecx,%edx,2),%edx
//                test %edx,%edx / je 0x10029993
//                add $0xfffffff8,%esp / mov %eax,(%esp) / call *%edx
//      10029993  add $0x4,%ebx / cmp %esi,%ebx / jl 0x10029960
//                jmp 0x10029909
//
//  WHAT PINS IT:
//    * the shim at +0 is one `mov 0x4(%esp),%eax`: ONE argument.
//    * 0x10026484 and 0x10026504 are the REGISTER entries of 0x10026480
//      (above) and 0x10026500, both entered four bytes past their shims, so
//      this function shares their translation unit.  Both get 8 bytes of
//      outgoing area and only %eax: one argument each.  0x10025db0 and
//      0x10025890 have NO shim (0x10025dac and 0x1002588c are zero), so
//      they are static one-argument functions, and 0x10025890's argument is
//      formed by `add $0x5c,%edi` -- %edi is dead afterwards, so icl
//      clobbered it rather than lea.
//    * the addressing `0x100895c0(%ecx,%edx,2)` with %edx = n*5 and
//      %ecx = n*10 is base + n*20, i.e. element n of the ELEVEN twenty-byte
//      records at 0x100895bc that d3dr2d.cpp's sub_10050390 already pinned,
//      and the +4 in the base picks the SECOND field of the record.  The
//      bound `cmp $0xb` is that array's 11.
//    * `movsbl 0xbc(%eax)` -- the discriminator is a SIGNED char, and it is
//      range-checked `>= 0` (`jl`) then `< 11` (`jge`).
//    * the sweep's bound is `test %eax,%eax ; jle` and the back edge is
//      `cmp %esi,%ebx ; jl`: SIGNED, so the count at +0xe0 is int32 and the
//      index is int32.  The indirect call reserves 8 and cleans 8, so the
//      slot is a __cdecl `void (*)(T*)`.
//    * `add $-N,%esp` + `mov` throughout and no cross-jumped exit: /O2.
//
//  IDENTICAL (was 209 bytes differ), both entries,
//  from four things: (1) `cmpl $0x2,(%edi) ; je` sinking the sweep is icl
//  predicting a SIGNED `== K` false, so m_n00 is int32 (as uint32 the arm
//  stays inline); (2) the device read into a local first (shipped loads
//  0x1008d988 before the flag); (3) the count and the array read into
//  locals before the sweep (shipped walks a pointer to a fixed end; the
//  field spelling reloads m_ne0 every pass, as the indirect call could
//  change it); (4) the TU: 0x10025db0 / 0x10025890 are statics here and
//  0x10026728 / 0x1002d1c0 call the register entry, which /Qip emits only
//  with an in-TU caller -- and under /Qip_no_inlining, see the banner.
// ======================================================================= //

struct Elem10029890
{
	uint8   m_pad_00[0xbc];                             // +0x00
	char    m_cbc;                                      // +0xbc
};

struct Rec100895bc
{
	void  (*m_pfnSlot00)();                             // +0x00
	void  (*m_pfnSlot04)(Elem10029890*);                // +0x04
	void*   m_p08;                                      // +0x08
	void*   m_p0c;                                      // +0x0c
	void*   m_p10;                                      // +0x10
};

extern Rec100895bc g_a100895bc[11];                     // 0x100895bc

struct Obj10029890
{
	int32           m_n00;                              // +0x00
	uint8           m_pad_04[0x4c];                     // +0x04
	uint8           m_a50[0xc];                         // +0x50
	uint8           m_a5c[0x80];                        // +0x5c
	Elem10029890**  m_pdc;                              // +0xdc
	int32           m_ne0;                              // +0xe0
};

struct Mgr1007b8d0
{
	void sub_1007bae0();                                // 0x1007bae0

	uint8  m_pad_00[0x140];
	uint32 m_nPortals140;                               // +0x140 (RenderScene)
};

extern Mgr1007b8d0* sub_1007b8d0();                     // 0x1007b8d0

extern uint32 g_1008d7a0;                               // 0x1008d7a0

extern void sub_10026670();                             // 0x10026670
extern void sub_10050390();                             // 0x10050390
extern void sub_1001eb20();                             // 0x1001eb20
extern void sub_1007b880();                             // 0x1007b880
extern void sub_1004fbc0();                             // 0x1004fbc0

// (LL) The four callees below were qip_Callee placeholders with wrong bodies.
// 0x10026484 is the register entry of 0x10026480 -- sub_10026480, written
// above in this file, so /Qip reaches its register entry itself.
// 0x10026504 is the register entry of 0x10026500 -- sub_10026500, moved
// here from d3dr5b.cpp so that /Qip reaches its
// register entry itself; it is defined just below.
// 0x10025db0 and 0x10025890 are register-only (no stack entry), one
// argument in %eax, void; declared as the one-argument functions they are.
// ======================================================================= //
//  0x10025890 and 0x10025db0 -- the two full-viewport colour passes
//  0x10029894 runs last (moved here from w15_10025890.cpp:
//  both are /Qip STATICS of 0x10029894's TU -- no stack entry, the
//  colour pointer arrives in %eax -- so they belong beside their caller).
//
//              0x10025db0  the screen TINT  -- a modulate-2x quad
//                          (SRCBLEND DESTCOLOR, DESTBLEND SRCCOLOR) in the
//                          colour at scene+0x50, skipped when the colour
//                          is within 0.01 of white;
//              0x10025890  the LIGHT ADD   -- an additive quad (ONE, ONE)
//                          in the colour at scene+0x5c, gated on the
//                          LightAddPoly console variable and skipped when
//                          the colour is black.
//
//  Each has its own function-static D3DTLVERTEX[4] (0x1008ccf4 / 0x1008cdb8)
//  behind a one-bit guard (0x1008cdb4 / 0x1008ce78) with nothing to
//  construct -- the D3D_OVERLOADS empty constructor.  The render states
//  are saved and restored by six stack objects whose destructors run in
//  reverse order after the DrawPrimitive.  The colour bytes are C casts
//  (the twelve-instruction truncation, not /Qrcd).
// ======================================================================= //
struct LTVec { float x, y, z; };

extern uint32      g_1008d968;                          // 0x1008d968
extern int32       g_CV_LightAddPoly;                   // 0x100893b8   LightAddPoly
extern int32       g_nViewLeft;                         // 0x1008b89c
extern int32       g_nViewTop;                          // 0x1008b8a0
extern int32       g_nViewRight;                        // 0x1008b8a4
extern int32       g_nViewBottom;                       // 0x1008b8a8

#define D3DRENDERSTATE_ZENABLE           7
#define D3DRENDERSTATE_ZWRITEENABLE      14
#define D3DRENDERSTATE_SRCBLEND          19
#define D3DRENDERSTATE_DESTBLEND         20
#define D3DRENDERSTATE_ALPHABLENDENABLE  27
#define D3DRENDERSTATE_FOGENABLE         28
#define D3DBLEND_ONE                     2
#define D3DBLEND_SRCCOLOR                3
#define D3DBLEND_DESTCOLOR               9
#define D3DPT_TRIANGLEFAN                6
#define D3DFVF_TLVERTEX                  0x1c4

// D3DTLVERTEX as d3dtypes.h declares it under D3D_OVERLOADS: the empty
// constructor is what gives the function statics their guard bit.
struct TLVert25890
{
	float   sx, sy, sz, rhw;                            // +0x00
	uint32  color;                                      // +0x10
	uint32  specular;                                   // +0x14
	float   tu, tv;                                     // +0x18

	TLVert25890() { }
};

// Save a render state, set it, and put it back when the scope closes.
class CRenderStateSet25890
{
public:
	CRenderStateSet25890(uint32 nState, uint32 nValue)
	{
		m_nState = nState;
		g_pD3DDevice->lpVtbl->GetRenderState(g_pD3DDevice, nState, &m_nOld);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, m_nState, nValue);
	}

	~CRenderStateSet25890()
	{
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, m_nState, m_nOld);
	}

	uint32  m_nState;                                   // +0x00
	uint32  m_nOld;                                     // +0x04
};

// 0x100257d0 (d3dr2d.cpp), inlined: drop the current stage's texture.
inline void DropStageTexture25890()
{
	uint32 nStage;

	nStage = g_1008d7d0;
	if (g_a1008e684[nStage])
	{
		g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, nStage, LTNULL);
		g_a1008e684[nStage] = LTNULL;
	}
}

union Color25890
{
	struct { uint8 b, g, r, a; } c;
	uint32 dw;
};

// The viewport quad, one colour, z 0.
inline void FillQuad25890(TLVert25890* pVerts, uint32 dwColor)
{
	pVerts[0].sx = (float)g_nViewLeft;
	pVerts[0].sy = (float)g_nViewTop;
	pVerts[1].sx = (float)g_nViewRight;
	pVerts[1].sy = (float)g_nViewTop;
	pVerts[2].sx = (float)g_nViewRight;
	pVerts[2].sy = (float)g_nViewBottom;
	pVerts[3].sx = (float)g_nViewLeft;
	pVerts[3].sy = (float)g_nViewBottom;

	pVerts[0].color = dwColor;
	pVerts[1].color = dwColor;
	pVerts[2].color = dwColor;
	pVerts[3].color = dwColor;

	pVerts[0].sz = 0.0f;
	pVerts[1].sz = 0.0f;
	pVerts[2].sz = 0.0f;
	pVerts[3].sz = 0.0f;
}


// ======================================================================= //
//  0x10025890 -- 1300 bytes.  The additive light pass.
// ======================================================================= //
static void sub_10025890(void* pvColor)
{
	const LTVec* pColor = (const LTVec*)pvColor;
	static TLVert25890 s_Verts[4];
	Color25890 col;

	if (!g_CV_LightAddPoly || !g_1008d968)
		return;

	if (pColor->x < 0.001f && pColor->y < 0.001f && pColor->z < 0.001f)
		return;

	col.c.r = (uint8)(int32)(pColor->x * 255.0f);
	col.c.g = (uint8)(int32)(pColor->y * 255.0f);
	col.c.b = (uint8)(int32)(pColor->z * 255.0f);
	col.c.a = 0xff;

	FillQuad25890(s_Verts, col.dw);
	DropStageTexture25890();

	{
		CRenderStateSet25890 ssFog(D3DRENDERSTATE_FOGENABLE, 0);
		CRenderStateSet25890 ssZ(D3DRENDERSTATE_ZENABLE, 0);
		CRenderStateSet25890 ssZWrite(D3DRENDERSTATE_ZWRITEENABLE, 0);
		CRenderStateSet25890 ssBlend(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
		CRenderStateSet25890 ssSrc(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
		CRenderStateSet25890 ssDest(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);

		g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, D3DPT_TRIANGLEFAN,
			D3DFVF_TLVERTEX, s_Verts, 4, 0);
	}
}


// ======================================================================= //
//  0x10025db0 -- 1338 bytes.  The modulate-2x tint pass.
// ======================================================================= //
static void sub_10025db0(void* pvColor)
{
	const LTVec* pColor = (const LTVec*)pvColor;
	static TLVert25890 s_Verts[4];
	Color25890 col;

	if (pColor->x > 0.99f && pColor->y > 0.99f && pColor->z > 0.99f &&
	    pColor->x < 1.01f && pColor->y < 1.01f && pColor->z < 1.01f)
		return;

	col.c.r = (uint8)(int32)(pColor->x * 127.5f);
	col.c.g = (uint8)(int32)(pColor->y * 127.5f);
	col.c.b = (uint8)(int32)(pColor->z * 127.5f);
	col.c.a = 0xff;

	FillQuad25890(s_Verts, col.dw);
	DropStageTexture25890();

	{
		CRenderStateSet25890 ssFog(D3DRENDERSTATE_FOGENABLE, 0);
		CRenderStateSet25890 ssZ(D3DRENDERSTATE_ZENABLE, 0);
		CRenderStateSet25890 ssZWrite(D3DRENDERSTATE_ZWRITEENABLE, 0);
		CRenderStateSet25890 ssBlend(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
		CRenderStateSet25890 ssSrc(D3DRENDERSTATE_SRCBLEND, D3DBLEND_DESTCOLOR);
		CRenderStateSet25890 ssDest(D3DRENDERSTATE_DESTBLEND, D3DBLEND_SRCCOLOR);

		g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, D3DPT_TRIANGLEFAN,
			D3DFVF_TLVERTEX, s_Verts, 4, 0);
	}
}


// ======================================================================= //
//  0x1002d050/5c and 0x1002dd60/6c -- the debug box drawing 0x1002cf48
//  (below) and 0x1002e350 use.  Moved here from w15_1002d050_qip.cpp
//  : 0x1002cf48 calls both at their /Qip register
//  entries.  That file's notes, unchanged:
//
//  Both are /Qip EXTERNAL-linkage two-entry pairs: a three-mov stack entry
//  (%eax/%edx/%ecx from 4/8/0xc(%esp)) falling into the register entry the
//  shipped callers call.  The recursion in 0x1002d05c and its call to
//  0x1002dd6c are the same-file register calls that make icl emit that
//  shape.  /O2: the padding after each is a lea-nop.
//
//  0x1002dd6c was a NEAR of one kind only: instruction for instruction the
//  same, but the shipped function gives each of its 24 edge temporaries a
//  16-byte frame slot (frame 0x1a0) where icl packed them at 12 (0x140).
//  Tried without effect: the real _CVector<float>, a reference-taking
//  inline line helper, a by-value factory function, /Os, /O1, /Qsfalign16,
//  /Qip_no_inlining.  Closed by the 16-byte temporary type
//  LTVec4_1002dd60 below.
// ======================================================================= //
// big2b1b0.cpp's vector type, the 12-byte by-value parameter.
struct LTVec2b1b0
{
	float x, y, z;
};

// 0x1002dd6c's edge temporaries: a 16-byte vector (the three floats and a
// fourth slot), handed to 0x1002b1b0 sliced to its 12-byte parameter.
// This is what gives each of the 24 temporaries the 16-byte
// frame slot the shipped code has (frame 0x1a0; a 12-byte temporary packs
// them at 12, frame 0x140, every displacement after the first off).  A
// destructor, a copy constructor, an assignment operator, a default
// constructor, a wrapper with a conversion operator and a by-value factory
// all keep the 12-byte packing (measured); with this type 0x1002dd6c is
// byte-identical.  (The constructor is this type's: one on the base as
// well is emitted out of line, unreferenced.)
struct LTVec4_1002dd60 : public LTVec2b1b0
{
	float w;

	LTVec4_1002dd60(float fX, float fY, float fZ) { x = fX; y = fY; z = fZ; }
};

void sub_1002b1b0(LTVec2b1b0 vStart, LTVec2b1b0 vEnd,
                  uint32 nColor0, uint32 nColor1);      // 0x1002b1b0

// One world-tree node: its box at +0x18/+0x24 and four children at +0x4c.
struct TreeNode1002d050
{
	uint8               m_pad_00[0x18];                 // +0x00
	float               m_vMin[3];                      // +0x18
	float               m_vMax[3];                      // +0x24
	uint8               m_pad_30[0x4c - 0x30];          // +0x30
	TreeNode1002d050*   m_pChildren[4];                 // +0x4c
};


// ======================================================================= //
//  0x1002d050 (stack entry, 12 bytes) / 0x1002d05c -- 92 bytes.
// ======================================================================= //
void sub_1002dd60(float* pMin, float* pMax, uint32 nColor);

void sub_1002d050(void* pvNode, uint32 nDepth, uint32 nMaxDepth)
{
	TreeNode1002d050* pNode = (TreeNode1002d050*)pvNode;
	uint32 i;

	if (nDepth > nMaxDepth)
		return;

	sub_1002dd60(pNode->m_vMin, pNode->m_vMax, 0xffffffff);

	if (pNode->m_pChildren[0])
	{
		for (i = 0; i < 4; i++)
			sub_1002d050(pNode->m_pChildren[i], nDepth + 1, nMaxDepth);
	}
}


// ======================================================================= //
//  0x1002dd60 (stack entry, 12 bytes) / 0x1002dd6c -- 1377 bytes.
//  The four edges of the bottom (min y) face, the four of the top face,
//  then the four uprights.
// ======================================================================= //
void sub_1002dd60(float* pMin, float* pMax, uint32 nColor)
{
	sub_1002b1b0(LTVec4_1002dd60(pMin[0], pMin[1], pMin[2]), LTVec4_1002dd60(pMax[0], pMin[1], pMin[2]), nColor, nColor);
	sub_1002b1b0(LTVec4_1002dd60(pMin[0], pMin[1], pMin[2]), LTVec4_1002dd60(pMin[0], pMin[1], pMax[2]), nColor, nColor);
	sub_1002b1b0(LTVec4_1002dd60(pMax[0], pMin[1], pMin[2]), LTVec4_1002dd60(pMax[0], pMin[1], pMax[2]), nColor, nColor);
	sub_1002b1b0(LTVec4_1002dd60(pMin[0], pMin[1], pMax[2]), LTVec4_1002dd60(pMax[0], pMin[1], pMax[2]), nColor, nColor);

	sub_1002b1b0(LTVec4_1002dd60(pMin[0], pMax[1], pMin[2]), LTVec4_1002dd60(pMax[0], pMax[1], pMin[2]), nColor, nColor);
	sub_1002b1b0(LTVec4_1002dd60(pMin[0], pMax[1], pMin[2]), LTVec4_1002dd60(pMin[0], pMax[1], pMax[2]), nColor, nColor);
	sub_1002b1b0(LTVec4_1002dd60(pMax[0], pMax[1], pMin[2]), LTVec4_1002dd60(pMax[0], pMax[1], pMax[2]), nColor, nColor);
	sub_1002b1b0(LTVec4_1002dd60(pMin[0], pMax[1], pMax[2]), LTVec4_1002dd60(pMax[0], pMax[1], pMax[2]), nColor, nColor);

	sub_1002b1b0(LTVec4_1002dd60(pMin[0], pMin[1], pMin[2]), LTVec4_1002dd60(pMin[0], pMax[1], pMin[2]), nColor, nColor);
	sub_1002b1b0(LTVec4_1002dd60(pMax[0], pMin[1], pMin[2]), LTVec4_1002dd60(pMax[0], pMax[1], pMin[2]), nColor, nColor);
	sub_1002b1b0(LTVec4_1002dd60(pMax[0], pMin[1], pMax[2]), LTVec4_1002dd60(pMax[0], pMax[1], pMax[2]), nColor, nColor);
	sub_1002b1b0(LTVec4_1002dd60(pMin[0], pMin[1], pMax[2]), LTVec4_1002dd60(pMin[0], pMax[1], pMax[2]), nColor, nColor);
}


// ======================================================================= //
//  0x1002cf40   268 bytes (0x1002cf40..0x1002d04b)      1 caller
//
//  DECODED, every instruction.  Save one render state, force it to 1, drop
//  the bound texture, do the work, put the state back.
//
//      1002cf40  mov 0x4(%esp),%eax / mov 0x8(%esp),%edx   <- STACK entry
//      1002cf48  push %ebp / mov %esp,%ebp / sub $0x28,%esp <- REGISTER
//                mov %edi,0x8(%esp) / mov %esi,0x4(%esp) / mov %ebx,(%esp)
//                mov $0x7,%ecx / add $0xfffffff4,%esp
//                lea -0x4(%ebp),%ebx / mov 0x1008d988,%esi
//                mov %ecx,-0x8(%ebp)            ; the state id, in a LOCAL
//                mov %ebx,0x8(%esp)             ; &nSaved
//                mov %ecx,0x4(%esp)
//                mov %eax,%edi / mov %edx,%esi
//                call *0x54(%ecx)               ; GetRenderState
//                ... mov -0x8(%ebp),%eax / movl $0x1,0x8(%esp)
//                call *0x50(%eax)               ; SetRenderState(state, 1)
//      1002cfa1  mov 0x1008d7d0,%ebx
//                mov 0x1008e684(,%ebx,4),%eax / test / je 0x1002cfdc
//                ... call *0x8c(%edx)           ; SetTexture(stage, NULL)
//                movl $0x0,0x1008e684(,%ebx,4)
//      1002cfdc  test %esi,%esi / jb 0x1002d01e
//      1002cfe0  lea 0x24(%edi),%eax / lea 0x30(%edi),%edx
//                add $0xfffffff0,%esp / mov $0xffffffff,%ecx
//                call 0x1002dd6c / add $0x10,%esp
//                mov 0x58(%edi),%eax / test / je 0x1002d01e
//                mov %edi,%ebx / add $0x10,%edi
//      1002d002  mov 0x58(%ebx),%eax / add $0xfffffff0,%esp
//                mov $0x1,%edx / mov %esi,%ecx
//                call 0x1002d05c
//                add $0x4,%ebx / add $0x10,%esp
//                cmp %ebx,%edi / ja 0x1002d002
//      1002d01e  ... call *0x50(%edx)           ; SetRenderState(state, nSaved)
//
//  WHAT PINS IT:
//    * the two-entry pair at +0/+8 says TWO arguments, %eax then %edx.
//    * 0x1002dd6c has a three-mov shim at 0x1002dd60 and 0x1002d05c has one
//      at 0x1002d050, so both take THREE arguments; the caller reserves 16
//      bytes for each and writes none of them, which is the register holes.
//    * 0x1002cfa1..0x1002cfdc is byte-for-byte the body of 0x100257d0,
//      which d3dr2d.cpp already reconstructed as
//      `nStage = g_1008d7d0; if (g_a1008e684[nStage]) { SetTexture(...); }`.
//      The stage is held in %ebx ACROSS the COM call and never reloaded,
//      which a global cannot be, so it is a local here too.
//    * the state id is materialised ONCE into %ecx and ALSO stored to
//      -0x8(%ebp), then reloaded from there twice more.  A literal would
//      have been re-materialised as `movl $0x7,0x4(%esp)`; the stack home
//      plus two reloads is a source VARIABLE that icl declined to
//      enregister across the COM calls.
//    * the loop walks %ebx from %edi to %edi+0x10 in steps of 4 reading
//      0x58(%ebx), so +0x58 is an array of FOUR pointers, and
//      `cmp %ebx,%edi ; ja` is an UNSIGNED compare -- an unsigned index.
//      The `mov 0x58(%edi),%eax ; test ; je` in front of it tests element
//      ZERO and is a separate source `if`, not the loop guard.
//    * `add $-N,%esp` + `mov` for every outgoing argument and no
//      cross-jumped exit: /O2.
//
//  IDENTICAL (was 244 bytes differ), both entries:
//  the saved state is a CRenderStateSet25890 stack object (the state id
//  homed at -0x8 and reloaded, the old value at -0x4 -- the constructor
//  and destructor 0x10025890 uses), the texture drop is
//  DropStageTexture25890(), and 0x1002d050 / 0x1002dd60 and the caller
//  0x1002d1c0 are in this TU for their register entries.
//
//  THE ALWAYS-TRUE GUARD at 0x1002cfdc is in the shipped bytes and is not
//  a decoding error: `test %esi,%esi` clears CF, so `jb` is never taken and
//  the block always runs.  The inverse of `jb` is `jae`, so the source
//  condition is an UNSIGNED `>= 0`.  icl neither folded it nor warned it
//  away.  COIN-FLIP: written below as `if (nValue >= 0)` on a uint32; the
//  other spelling that gives the same `test`/`jb` is `if (0 <= nValue)`.
// ======================================================================= //

struct Obj1002cf40
{
	uint8   m_pad_00[0x24];                             // +0x00
	uint8   m_a24[0xc];                                 // +0x24
	uint8   m_a30[0x28];                                // +0x30
	void*   m_ap58[4];                                  // +0x58
};

// 0x1002dd6c / 0x1002d05c -- the REGISTER entries (%eax/%edx/%ecx) of two
// three-argument /Qip functions whose STACK entries are 0x1002dd60 and
// 0x1002d050 (`mov 4(%esp),%eax; mov 8(%esp),%edx; mov 0xc(%esp),%ecx`
// falling into the body). Neither is in this file, so the calls go to the
// stack entries. (LL: were qip_Callee placeholders with wrong bodies.)
// sub_1002dd60 and sub_1002d050 are defined above.

void sub_1002cf40(Obj1002cf40* pObj, uint32 nValue)
{
	uint32 i;

	CRenderStateSet25890 ssZ(D3DRENDERSTATE_ZENABLE, 1);

	DropStageTexture25890();

	if (nValue >= 0)
	{
		sub_1002dd60((float*)pObj->m_a24, (float*)pObj->m_a30, 0xffffffff);

		if (pObj->m_ap58[0])
		{
			for (i = 0; i < 4; ++i)
				sub_1002d050(pObj->m_ap58[i], 1, nValue);
		}
	}
}


// ======================================================================= //
//  0x10026500 (body at 0x10026504) -- 145 bytes.  The renderer's shutdown
//  sequence: five guarded groups off five separate globals.
//
//      8b 44 24 04              <- the stack shim; see the file banner
//      55 8b ec / 83 ec 10 / 89 1c 24 / 8b d8
//      e8 <0x100123a0> / e8 <0x1000b6c0>
//      a1 <0x1008d96c> / 85 c0 / 74 07 / e8 <0x100729c0> / eb 05
//                                        e8 <0x10012380>
//      a1 <0x1008d96c> / 85 c0 / 74 07 / e8 <0x100733a0> / eb 05
//                                        e8 <0x100145f0>
//      a1 <0x1008d4c0> / 85 c0 / 74 18
//        83 c4 f8 / 89 1c 24 / e8 <0x10040780> / 83 c4 08
//        e8 <0x10040850> / e8 <0x10040810>
//      a1 <0x1008d4bc> / 85 c0 / 74 0a / e8 <0x100257d0> / e8 <0x10025820>
//      a1 <0x1008fe18> / 85 c0 / 74 0a
//        b9 <0x1008f620> / e8 <0x10056c60>
//      8b 1c 24 / 8b e5 / 5d / c3
//
//  WHAT PINS IT:
//    * 0x1008d96c is read TWICE and drives two independent if/else pairs,
//      each with its own `eb 05` over the else arm -- two statements, not
//      one block with two calls each.
//    * the ONE argument reaches the body in %ebx and is spent on a single
//      cdecl call (`add $0xfffffff8,%esp ; mov %ebx,(%esp)`), which is the
//      /O2 outgoing shape and also why %ebx has to be callee-saved across
//      everything above it.
//    * the last group is __thiscall on a fixed object (`mov $K,%ecx` and
//      nothing pushed), so 0x1008f620 is a global instance, not a pointer.
//    * every callee here is already known: 0x10012380 and 0x100145f0 from
//      d3dr2a.cpp, 0x100733a0 from d3dr2b.cpp, 0x10040780 from d3dr3d.cpp,
//      0x10040850 from d3dr3c.cpp, 0x10040810 from d3dr2a.cpp, 0x100257d0
//      from d3dr2d.cpp and 0x10025820 from d3dr3b.cpp.
//
//  IDENTICAL HERE.  In d3dr5b.cpp (/Os) it was a near
//  (59 bytes differ): the shipped bytes are /O2 (`add $-8,%esp; mov %ebx,
//  (%esp)` for the one outgoing argument, callee-saved %ebx in a frame
//  slot) and an external /Qip pair (`mov 0x4(%esp),%eax` falling into the
//  register entry 0x10026504, which 0x10029890 below calls).  This file is
//  /O2 /Qip and holds that caller; moved unchanged, both entries match.
// ======================================================================= //
extern void sub_100123a0();                             // 0x100123a0
extern void sub_1000b6c0();                             // 0x1000b6c0
extern void sub_100729c0();                             // 0x100729c0
extern void sub_10012380();                             // 0x10012380
extern void sub_100733a0();                             // 0x100733a0
extern void sub_100145f0();                             // 0x100145f0
extern void sub_10040780(void* pArg);                   // 0x10040780
extern void sub_10040850();                             // 0x10040850
extern void sub_10040810();                             // 0x10040810
extern void sub_100257d0();                             // 0x100257d0
extern void sub_10025820();                             // 0x10025820

struct C1008f620
{
	void sub_10056c60();                                // 0x10056c60
};

extern C1008f620 g_1008f620;                            // 0x1008f620

extern uint32 g_1008d96c;                               // 0x1008d96c
extern uint32 g_1008d4c0;                               // 0x1008d4c0
extern uint32 g_1008d4bc;                               // 0x1008d4bc
extern uint32 g_1008fe18;                               // 0x1008fe18

void sub_10026500(void* pArg)
{
	sub_100123a0();
	sub_1000b6c0();

	if (g_1008d96c)
		sub_100729c0();
	else
		sub_10012380();

	if (g_1008d96c)
		sub_100733a0();
	else
		sub_100145f0();

	if (g_1008d4c0)
	{
		sub_10040780(pArg);
		sub_10040850();
		sub_10040810();
	}

	if (g_1008d4bc)
	{
		sub_100257d0();
		sub_10025820();
	}

	if (g_1008fe18)
		g_1008f620.sub_10056c60();
}


// 0x10029890 is defined below 0x10026728 (their shipped order): icl /Qip
// expands a callee defined ABOVE its caller, and the shipped 0x10026728
// CALLS 0x10029894.
void sub_10029890(Obj10029890* pObj);


// *********************************************************************** //
//
//  (was MODULE BIG_26728 (.CPP); merged into d3dqip2.cpp)
//
//  PURPOSE:  One function -- 0x10026728, 6,024 bytes, ONE caller
//            (0x1002b0cf).  This is d3d.ren's MIRROR / REFLECTION pass:
//            for every entry the visibility walk parked in the render
//            manager's 32-slot table it stencils the mirror polygon into
//            the frame buffer, builds the reflected view transform, and
//            re-runs the world draw through it.
//
//  Compiler: Intel C/C++ 4.0, /O2.  All three tells of the
//            /O2 pattern are present:
//              * TWO epilogues (0x10027e16 and 0x10027e25) for the two
//                exits, duplicated rather than cross-jumped;
//              * every outgoing argument block is `add $-N,%esp` followed
//                by `mov %reg,K(%esp)` -- not one `push` in 6 KB;
//              * `&local` is `lea`, and the only pointer arithmetic on an
//                object (`add $0x8,%eax` at 0x10027e05) is an induction
//                variable, not a member address.
//            The function uses the REGISTER-PASSING convention: the caller
//            at 0x1002b0c0 is
//                sub $0xc,%esp ; call 0x1007b8d0 ; mov %eax,%edx
//                mov 0x1008beb0,%eax ; call 0x10026728 ; add $0xc,%esp
//            so arg0 arrives in %eax (`*g_1008beb0`, the scene sink) and
//            arg1 in %edx (`sub_1007b8d0()`, the 0x10091a10 singleton).
//            That is decoded as an ordinary call whose
//            arguments arrive in registers, which is what is done here.
//
//  /O2 /Qip, AND THE CALLER IN THIS FILE.  That register
//  entry is icl's /Qip two-entry pair: 0x10026720 is the stack entry
//  (`mov 0x4(%esp),%eax ; mov 0x8(%esp),%edx`, no caller in the image)
//  and 0x10026728 the register entry, reached only from 0x1002b0c0.
//  icl emits the pair, and a caller that uses the register entry, only
//  inside one /Qip translation unit -- so 0x1002b0c0 lives here now
//  (it was w4_10023fb0.cpp's, calling through the stack), this file is
//  in build/engine.sh's D3D_QIP_TUS, and the function is NAMED for its
//  stack entry, sub_10026720 (the register entry is icl's
//  `sub_10026720$`).  Measured: 0x10026720 and 0x1002b0c0 byte-
//  identical (were: no shim, and 13 bytes off), 0x10026728 4088 -> 4058
//  bytes differing.  No other file names sub_10026728 in code.  /Qip
//  also emits an unreferenced out-of-line copy of MatMul26728 (every
//  call to it is inlined), which the link discards.
//
// ----------------------------------------------------------------------- //
//  EXTENT -- VERIFIED BEFORE DECODING
// ----------------------------------------------------------------------- //
//  The stated extent 0x10026728..0x10027eb0 is CORRECT.  Four checks:
//    * ONE prologue.  `push %ebp ; mov %esp,%ebp` occurs exactly once, at
//      0x10026728; there is no second one anywhere in the range.
//    * TWO `ret`s (0x10027e24, 0x10027e33) and two `pop %ebp`s, and they
//      are the same epilogue duplicated -- both are
//      `mov (%esp),%ebx ; mov 0x4(%esp),%esi ; mov 0x8(%esp),%edi ;
//       mov %ebp,%esp ; pop %ebp ; ret`, restoring the three registers the
//      prologue saved at 0x10026731..0x10026739.  One function, two exits.
//    * NO branch leaves the range.  All 28 distinct branch targets in the
//      body lie inside [0x10026728, 0x10027eb0).
//    * the next function starts EXACTLY at 0x10027eb0
//      (`push %ebp ; mov %esp,%ebp ; sub $0x38,%esp`), and it is
//      LTMatrix::TransformPlane, which THIS function calls at 0x10026808.
//  The 124 bytes after the second `ret` (0x10027e34..0x10027ea9) are NOT a
//  separate function: they are seven cold blocks that all jump BACKWARDS
//  into the body (0x10027d48, 0x10027ad1, 0x10027ab4, 0x10027a97,
//  0x10027a7b, 0x10027a5e, 0x10027a41).  Six of them are the
//  `fstp %st(0) ; jmp` other-arm of a LTMIN/LTMAX ternary and the seventh
//  is the `g_CV_LightMap = 0` arm.  This is exactly the cold-block sinking
//  pattern; see the call-sequence note at the foot of this banner.
//
// ----------------------------------------------------------------------- //
//  WHAT THE FUNCTION IS -- five independent tells
// ----------------------------------------------------------------------- //
//
//   1. THE REFLECTION MATRIX.  0x100268f7..0x10026b6c builds three 4x4s at
//      -0x464, -0x424 and -0x3e4 and then multiplies them.  The one at
//      -0x464 is, element for element,
//          [-2Nx*Nx+1  -2Nx*Ny    -2Nx*Nz    0]
//          [-2Ny*Nx    -2Ny*Ny+1  -2Ny*Nz    0]
//          [-2Nz*Nx    -2Nz*Ny    -2Nz*Nz+1  0]
//          [ 0          0          0         1]
//      -- the -2.0f constants are at 0x100a5e50/58/5c and the +1.0f at
//      0x100a5e54/60/64 -- and the other two are Identity() with
//      SetTranslation(-vOrigin) and SetTranslation(+vOrigin) written as
//      direct stores.  That is src/LT2/sdk/inc/ltmatrix.h:539
//      `LTMatrix::SetupReflectionMatrix(LTVector N, LTVector vOrigin)`
//      VERBATIM, including its `*this = mForward * mReflect * mBack`.
//
//   2. THE D3D7 STENCIL SEQUENCE.  Seventeen calls through
//      g_pD3DDevice->lpVtbl->+0x50 (SetRenderState, index 20) with the
//      literal D3DRENDERSTATE numbers
//          0x34 STENCILENABLE  0x37 STENCILPASS  0x38 STENCILFUNC
//          0x39 STENCILREF     0x07 ZENABLE      0x0e ZWRITEENABLE
//          0x17 ZFUNC
//      in the order enable / REPLACE / ALWAYS / ref=1 -- draw the polygon --
//      KEEP / EQUAL -- draw the world -- disable.  That is a stencilled
//      mirror, not a shadow: a shadow pass does not need REPLACE+ref.
//
//   3. THE TABLE IT WALKS.  `0x140(%edx)` is the count and `0x40(%edx)` the
//      base of an 8-byte-record array in the 0x1007b8d0 singleton.
//      d3dr5d.cpp already records 0x10066d10 as the producer: "records the
//      object in a 0x20-entry table at 0x140/0x40 of the manager
//      (0x1007b8d0)".  0x20 entries x 8 bytes = 0x100, and the local copy
//      at -0x5d0 is exactly 0x100 bytes wide (it ends where the
//      TransformPlane return slot at -0x4d0 begins).
//
//   4. THE +0x42 WORD.  The same d3dr5d.cpp note says 0x10066d10 "stamps a
//      global frame stamp (0x1008bea8, a WORD) into each object's +0x42"
//      and tests `Dot(pDir, pObj->+0x28->+0x34) - dist`.  Both appear here:
//      the record's object is dereferenced at +0x28, that sub-object's
//      +0x34 -- a POINTER to an LTPlane, `mov` and not `lea` -- is handed
//      to TransformPlane, and its +0x42 is masked with 0x7fff and 0x8000.  0x1008bea8 is written from
//      `pThing->m_w0c` at 0x10027859.
//
//   5. THE VERTEX FORMAT.  DrawPrimitive (vtable +0x64, index 25) is called
//      with type 6 (D3DPT_TRIANGLEFAN) and FVF 0x1c4
//      (XYZRHW|DIFFUSE|SPECULAR|TEX1); the loop that fills the buffer
//      strides 0x20 and touches +0x00/+0x04/+0x08/+0x0c as floats and
//      +0x10/+0x11/+0x12 as bytes -- big16324.cpp's RVert exactly.
//
// ----------------------------------------------------------------------- //
//  THE FRAME.  `sub $0xb00,%esp`, and every byte of it is accounted for:
//
//    -0xb00 .. -0xaf5   %ebx/%esi/%edi spill (they are addressed off %esp)
//    -0xac8 .. -0x5d1   `saved`, 0x4f8 bytes.  Two `rep movsl` with
//                       %ecx = 0x13e (= 0x4f8/4) copy g_1008b880 into it
//                       at 0x10027bf5 and back out at 0x10027d6f, and its
//                       constructor 0x100234b0 runs at 0x10026746.
//                       0x4f8 lands the array below exactly at -0x5d0,
//                       and -0x5fc -- read once, at 0x10027cf3, and never
//                       written -- is `saved.m_4cc`, INSIDE it.
//    -0x5d0 .. -0x4d1   aRecs[32], the local copy of the manager table
//    -0x4d0 .. -0x4c1   TransformPlane's return slot
//    -0x4c0 .. -0x4b1   `plane`
//    -0x4b0 .. -0x471   `mCam`
//    -0x470 .. -0x465   `vOrigin`
//    -0x464 .. -0x425   SetupReflectionMatrix's mReflect
//    -0x424 .. -0x3e5   SetupReflectionMatrix's mBack
//    -0x3e4 .. -0x3a5   SetupReflectionMatrix's mForward
//    -0x3a4 .. -0x365   MatMul temp 1  (mForward * mReflect)
//    -0x364 .. -0x325   MatMul temp 2  (temp1 * mBack)
//    -0x324 .. -0x2e5   `mReflect` -- temp2 is copied here dword by dword
//                       at 0x10027076..0x1002729c: the `*this =` of
//                       SetupReflectionMatrix
//    -0x2e4 .. -0x2a5   MatMul temp 3  (mReflect * mCam)
//    -0x2a4 .. -0x265   `mFull` -- temp3 copied here at 0x1002776f..
//                       0x10027838
//    -0x264/-0x260      pVerts / nVerts, filled by 0x1002803c
//    -0x25c .. -0x248   the six bounding-box floats
//    -0x244 .. -0x229   the 28-byte block 0x10020d80 fills
//    -0x228 .. -0x21d   the LTVector(1,1,1) 0x1001ef30 takes by value
//    -0x218/-0x210      x87 spills of the two loop-invariant z/rhw values
//    -0x208 .. -0x1f0   pA, pThing, the induction pointer and the three
//                       per-record values
//    -0x1e8 .. -0x80    45 eight-byte x87 spill slots.  They are the
//                       compiler's, not the source's: every one is written
//                       with `fstpl` and read with `fldl`, i.e. in DOUBLE
//                       precision, which icl only does for unnamed
//                       temporaries.  A `float` local rounds on store.
//
//  Which multiplicand is which is read off the sums, not assumed:
//    * -0x3a4[0][0] (0x10026aa6) = sum of mForward[0][k]*mReflect[k][0]
//      -> MatMul(temp1, &mForward, &mReflect);
//    * -0x2e4[0][0] (0x1002733a) = -0x324[0][0]*mCam[0][0] +
//      -0x324[0][1]*mCam[1][0] + ... -> MatMul(temp3, &mReflect, &mCam),
//      i.e. `mReflect * mCam` and not the other order.
//
// ----------------------------------------------------------------------- //
//  CALLEES -- thirteen direct, all but two already named by earlier work
// ----------------------------------------------------------------------- //
//    0x100234b0  the 21-byte ctor d3dr4b.cpp reconstructed (zeroes four
//                dwords at +0x1c..+0x28 and returns `this`)
//    0x10027eb0  LTMatrix::TransformPlane      (d3dr3b.cpp, verbatim)
//    0x10027f50  LTVector::Dot(LTVector)       (d3dbatchb_os.cpp)
//    0x100156e0  LTMatrix::Identity            (d3dr2b.cpp / d3dlow.cpp)
//    0x10027f70  LTMatrix::SetBasisVectors     (d3dr5c.cpp)
//    0x10028010  LTMatrix::SetTranslation      (d3dr2a.cpp)
//    0x10015690  LTVector::operator*(float)    (d3dbatchd.cpp)
//    0x1001e520  sub_1001e520(pThing)          (d3dr3a.cpp -- and d3dr3a
//                already says its ONE caller is 0x10027841, this site)
//    0x1002803c  the register-ABI body behind the 0x10028030 shim
//                (d3dmid.cpp).  Three register arguments plus one stack
//                argument at 0xc(%esp): the caller opens 0x10 and writes
//                only that slot, which is icl reserving 0/4/8 for %eax,
//                %edx, %ecx.
//    0x10020d80  248 bytes (d3dr3a.cpp's size table).  __cdecl, 8 dwords.
//    0x1001ef30  __cdecl, 10 dwords, the last three being an LTVector by
//                value.  d3dr3a.cpp saw the identical shape from
//                0x10020ce0: "0x1001ef30(a0, &L28, a5, a6, a7, a8, a9,
//                <12-byte struct by value>) where the struct is a local set
//                to three 0x3f800000 dwords -- an LTVector(1,1,1)".  Same
//                28-byte local, same 7.0f, same LTVector(1,1,1) here.
//    0x10029894  register ABI, one argument in %eax (its body touches only
//                %eax); the world-draw entry point.
//    0x100299a8  register ABI, two arguments (%eax, %edx); it opens with
//                `push %edx ; push %eax` and a 0x1720-byte _chkstk.
//
//  Plus eighteen INDIRECT calls, all on g_pD3DDevice: seventeen
//  SetRenderState and one DrawPrimitive.
//
// ----------------------------------------------------------------------- //
//  WHAT THE CALL SEQUENCE CAN AND CANNOT SEE HERE
// ----------------------------------------------------------------------- //
//  13 direct calls / 13 distinct callees over 6,024 bytes is 463 bytes per
//  call: the WEAK band.  What else pins the decode is stated above -- the
//  ltmatrix.h header seam (SetupReflectionMatrix element for element), the
//  D3DRENDERSTATE literals, the frame arithmetic (0x4f8 + 0x100 landing
//  exactly on -0x5d0), and d3dr5d.cpp's independent description of the
//  producer of the table this function consumes.
//
//  CALL-FREE REGIONS, which the call sequence cannot witness at all:
//    * 0x100268f7 .. 0x10027840, 3,913 bytes -- the reflection matrix and
//      the three inlined MatMuls.  Zero calls.  This is 65% of the
//      function.  It is pinned by the header seam, not by the call sequence.
//    * 0x100279e0 .. 0x10027ae6, 263 bytes -- the vertex loop.  Zero calls.
//
//  PREDICTED CALL-SEQUENCE RESIDUAL: none of the block-placement kind.  The seven
//  sunk cold blocks below the epilogue carry NO calls (they are
//  `fstp %st(0) ; jmp` and one `movl $0x0,g ; jmp`), so unlike 0x10061a30
//  this function should come back with the indirect calls in position.  The
//  eighteen indirect calls sit in source order in one straight run.
//
// ----------------------------------------------------------------------- //
//  COIN-FLIPS -- stated with both spellings
// ----------------------------------------------------------------------- //
//   (A) 0x10028010 is written here as `SetTranslation(Vec vec)`, not as
//       ltmatrix.h's `SetTranslation(float x, float y, float z)`.  The two
//       are byte-identical in the CALLEE (`ret $0xc`, three dwords read at
//       0x8/0xc/0x10(%esp)), but not in the CALLER: this call site is
//       `mov %esp,%eax` followed by three `mov g,%edx ; mov %edx,K(%eax)`,
//       which is icl's 12-byte STRUCT copy -- the same shape it uses two
//       instructions earlier for the by-value Dot argument.  Three separate
//       float arguments store straight to `K(%esp)` with no base register.
//       ALTERNATIVE: `mCam.SetTranslation(g_v1008bd34.x, g_v1008bd34.y,
//       g_v1008bd34.z);` with the three-float overload.
//
//   (B) the per-record copy at 0x10026770 is TWO `mov`s, not `rep movsl`.
//       d3dr4b.cpp measured that icl uses `rep movsl` for struct assignment
//       even at %ecx = 2 (the 8-byte GenListPos), so this is written as two
//       MEMBER assignments.  ALTERNATIVE: `aRecs[i] = pMgr->m_aRecs[i];`
//       as one struct assignment.
//
//   (C) the two loop-invariant vertex values.  They are spilled as DOUBLES
//       (`fstpl -0x210` / `fstpl -0x218`), which is an unnamed temporary,
//       so the source computes them INSIDE the loop and icl hoisted them.
//       Written here as two `float` locals for legibility, which would
//       instead give `fstps`/`flds` on 4-byte slots.
//       ALTERNATIVE, matching the double spills exactly:
//           for (j = 0; j < nVerts; j++) {
//               pVerts[j].m_fRHW = 1.0f / (g_f1008b90c - 1.0f);
//               pVerts[j].m_vPos.z =
//                   ((g_f1008b90c - 1.0f) * g_f1008bbac + g_f1008bbb0) *
//                   (1.0f / (g_f1008b90c - 1.0f));
//               ...
//           }
//
//   (D) the six bounding-box floats are written as six named floats.  They
//       occupy -0x25c..-0x245, six CONTIGUOUS slots in min.x/min.y/min.z/
//       max.x/max.y/max.z order, so they could equally be two LTVectors --
//       but only four of the six ever leave the function (minX, minY, maxX,
//       maxY go to both 0x10020d80 and 0x1001ef30; minZ and maxZ are
//       computed and dropped), which is why they are written separately.
//       ALTERNATIVE: `Vec26728 vMin(100000.0f,100000.0f,100000.0f), vMax(
//       -100000.0f,-100000.0f,-100000.0f);` and `vMin.x` etc at the sites.
//
//   (E) the three colour bytes go out in the order +0x11, +0x10, +0x12.
//       Written below ascending (b, g, r) to match big16324.cpp's RVert
//       declaration order; the emitted order is a scheduler choice inside
//       one basic block and does not pin the source order.
//
//   (F) the LTVector(1,1,1) that 0x1001ef30 takes by value is written as a
//       NAMED LOCAL declared at the top of the `if` body.  The three
//       0x3f800000 stores land at -0x220, -0x224, -0x228 -- DESCENDING --
//       and are then copied field by field into 0x1c(%esp), which is a
//       materialised object, not three separate arguments.  d3dr3a.cpp
//       recorded the identical descending triple at 0x10020ce0 and flagged
//       it: "if a written `LTVector(1,1,1)` misses on those three
//       displacements, the answer is three separate float locals".
//       ALTERNATIVE: `sub_1001ef30(..., Vec26728(1.0f, 1.0f, 1.0f));` with
//       the temporary built in the argument list and no named local.
//
//   (G) `saved.sub_100234b0();` is spelled as an explicit call to the named
//       member d3dr4b.cpp reconstructed, because a compiler-generated
//       constructor name cannot be resolved by the call-sequence comparison.  The shipped source
//       almost certainly declares it as a CONSTRUCTOR -- the call is the
//       first instruction after the prologue, before even
//       `g_1008ef80 = 0`, which is where icl runs a local's constructor and
//       not where a hand-written first statement would necessarily go.
//       ALTERNATIVE: give XForm26728 a real `XForm26728();` and drop the
//       explicit call; the byte sequence is the same and only the symbol
//       name changes.
//
//  NO `// UNDECODED:` REGIONS.  Every instruction in 0x10026728..0x10027eb0
//  is accounted for by the source below.
//
// *********************************************************************** //




// ======================================================================= //
//  VECTOR / PLANE / MATRIX
//
//  Declared LOCALLY rather than by including ltmatrix.h, for the reason
//  d3dr3b.cpp gives: under /Ob1 icl inlines exactly what the source marks
//  inline, and the shipped code CALLS Identity, SetBasisVectors,
//  SetTranslation, TransformPlane, Dot and operator*(float) out of line
//  while INLINING SetupReflectionMatrix, operator*(Mat&) and MatMul.
//  ltmatrix.h marks all nine `inline`, so it cannot produce these bytes.
// ======================================================================= //

class Vec26728
{
public:
	                Vec26728() {}
	                Vec26728(float mx, float my, float mz)
	                    { x = mx; y = my; z = mz; }

	// 0x10027f50 -- `ret $0xc`, the argument read at 0x8/0xc/0x10(%esp):
	// BY VALUE.  Reconstructed in d3dbatchb_os.cpp.
	float           Dot(Vec26728 v) const;

	// 0x10015690 -- _CVector<T>::operator*(T), d3dbatchd.cpp.
	Vec26728        sub_10015690(float v) const;

	float           x;                                  // +0x00
	float           y;                                  // +0x04
	float           z;                                  // +0x08
};


class Plane26728
{
public:
	Vec26728        m_Normal;                           // +0x00
	float           m_Dist;                             // +0x0c
};


class Mat26728;

// ltmatrix.h:172 MatMul, inlined here because the shipped code inlines it.
inline void MatMul26728(Mat26728 *pDest, Mat26728 *pMat1, Mat26728 *pMat2);


class Mat26728
{
public:
	// --- the six the shipped build calls OUT OF LINE ---
	void            Identity();                         // 0x100156e0
	void            SetBasisVectors(Vec26728 *pRight, Vec26728 *pUp,
	                                Vec26728 *pForward);// 0x10027f70
	void            SetTranslation(Vec26728 vec);       // 0x10028010
	Plane26728      TransformPlane(Plane26728 &thePlane);
	                                                    // 0x10027eb0

	// --- the three the shipped build INLINES ---
	inline void     SetupReflectionMatrix(Vec26728 N, Vec26728 vOrigin);
	inline Mat26728 operator*(Mat26728 &other);

	float           m[4][4];
};


inline void MatMul26728(Mat26728 *pDest, Mat26728 *pMat1, Mat26728 *pMat2)
{
	#define MM1 pMat1->m
	#define MM2 pMat2->m

	pDest->m[0][0] = MM1[0][0]*MM2[0][0] + MM1[0][1]*MM2[1][0] + MM1[0][2]*MM2[2][0] + MM1[0][3]*MM2[3][0];
	pDest->m[1][0] = MM1[1][0]*MM2[0][0] + MM1[1][1]*MM2[1][0] + MM1[1][2]*MM2[2][0] + MM1[1][3]*MM2[3][0];
	pDest->m[2][0] = MM1[2][0]*MM2[0][0] + MM1[2][1]*MM2[1][0] + MM1[2][2]*MM2[2][0] + MM1[2][3]*MM2[3][0];
	pDest->m[3][0] = MM1[3][0]*MM2[0][0] + MM1[3][1]*MM2[1][0] + MM1[3][2]*MM2[2][0] + MM1[3][3]*MM2[3][0];

	pDest->m[0][1] = MM1[0][0]*MM2[0][1] + MM1[0][1]*MM2[1][1] + MM1[0][2]*MM2[2][1] + MM1[0][3]*MM2[3][1];
	pDest->m[1][1] = MM1[1][0]*MM2[0][1] + MM1[1][1]*MM2[1][1] + MM1[1][2]*MM2[2][1] + MM1[1][3]*MM2[3][1];
	pDest->m[2][1] = MM1[2][0]*MM2[0][1] + MM1[2][1]*MM2[1][1] + MM1[2][2]*MM2[2][1] + MM1[2][3]*MM2[3][1];
	pDest->m[3][1] = MM1[3][0]*MM2[0][1] + MM1[3][1]*MM2[1][1] + MM1[3][2]*MM2[2][1] + MM1[3][3]*MM2[3][1];

	pDest->m[0][2] = MM1[0][0]*MM2[0][2] + MM1[0][1]*MM2[1][2] + MM1[0][2]*MM2[2][2] + MM1[0][3]*MM2[3][2];
	pDest->m[1][2] = MM1[1][0]*MM2[0][2] + MM1[1][1]*MM2[1][2] + MM1[1][2]*MM2[2][2] + MM1[1][3]*MM2[3][2];
	pDest->m[2][2] = MM1[2][0]*MM2[0][2] + MM1[2][1]*MM2[1][2] + MM1[2][2]*MM2[2][2] + MM1[2][3]*MM2[3][2];
	pDest->m[3][2] = MM1[3][0]*MM2[0][2] + MM1[3][1]*MM2[1][2] + MM1[3][2]*MM2[2][2] + MM1[3][3]*MM2[3][2];

	pDest->m[0][3] = MM1[0][0]*MM2[0][3] + MM1[0][1]*MM2[1][3] + MM1[0][2]*MM2[2][3] + MM1[0][3]*MM2[3][3];
	pDest->m[1][3] = MM1[1][0]*MM2[0][3] + MM1[1][1]*MM2[1][3] + MM1[1][2]*MM2[2][3] + MM1[1][3]*MM2[3][3];
	pDest->m[2][3] = MM1[2][0]*MM2[0][3] + MM1[2][1]*MM2[1][3] + MM1[2][2]*MM2[2][3] + MM1[2][3]*MM2[3][3];
	pDest->m[3][3] = MM1[3][0]*MM2[0][3] + MM1[3][1]*MM2[1][3] + MM1[3][2]*MM2[2][3] + MM1[3][3]*MM2[3][3];

	#undef MM1
	#undef MM2
}


// ltmatrix.h:801.
inline Mat26728 Mat26728::operator*(Mat26728 &other)
{
	Mat26728 mRet;

	MatMul26728(&mRet, this, &other);
	return mRet;
}


// ltmatrix.h:539, verbatim.  Read off 0x100268f7..0x10027331: the -2.0f
// constants at 0x100a5e50/58/5c, the +1.0f at 0x100a5e54/60/64, mBack's
// three `fchs`es at 0x10026af0/0x10026b00/0x10026b22, and the two identity
// matrices materialised by `mov $0x3f800000,%eax ; xor %edx,%edx` and
// twenty-six integer stores.
inline void Mat26728::SetupReflectionMatrix(Vec26728 N, Vec26728 vOrigin)
{
	Mat26728 mReflect, mBack, mForward;

	mReflect.m[0][0] = -2.0f*N.x*N.x + 1.0f;
	mReflect.m[0][1] = -2.0f*N.x*N.y;
	mReflect.m[0][2] = -2.0f*N.x*N.z;
	mReflect.m[0][3] = 0.0f;

	mReflect.m[1][0] = -2.0f*N.y*N.x;
	mReflect.m[1][1] = -2.0f*N.y*N.y + 1.0f;
	mReflect.m[1][2] = -2.0f*N.y*N.z;
	mReflect.m[1][3] = 0.0f;

	mReflect.m[2][0] = -2.0f*N.z*N.x;
	mReflect.m[2][1] = -2.0f*N.z*N.y;
	mReflect.m[2][2] = -2.0f*N.z*N.z + 1.0f;
	mReflect.m[2][3] = 0.0f;

	mReflect.m[3][0] = 0.0f;
	mReflect.m[3][1] = 0.0f;
	mReflect.m[3][2] = 0.0f;
	mReflect.m[3][3] = 1.0f;

	// mBack.Identity(); mBack.SetTranslation(-vOrigin);  Written out
	// because the shipped code does NOT call 0x100156e0 or 0x10028010
	// here -- the sixteen stores at -0x424..-0x3e8 are direct, and the
	// negation is three `fchs`es feeding +0x0c/+0x1c/+0x2c of the block.
	mBack.m[0][0] = 1.0f;   mBack.m[0][1] = 0.0f;
	mBack.m[0][2] = 0.0f;   mBack.m[0][3] = -vOrigin.x;
	mBack.m[1][0] = 0.0f;   mBack.m[1][1] = 1.0f;
	mBack.m[1][2] = 0.0f;   mBack.m[1][3] = -vOrigin.y;
	mBack.m[2][0] = 0.0f;   mBack.m[2][1] = 0.0f;
	mBack.m[2][2] = 1.0f;   mBack.m[2][3] = -vOrigin.z;
	mBack.m[3][0] = 0.0f;   mBack.m[3][1] = 0.0f;
	mBack.m[3][2] = 0.0f;   mBack.m[3][3] = 1.0f;

	mForward.m[0][0] = 1.0f;   mForward.m[0][1] = 0.0f;
	mForward.m[0][2] = 0.0f;   mForward.m[0][3] = vOrigin.x;
	mForward.m[1][0] = 0.0f;   mForward.m[1][1] = 1.0f;
	mForward.m[1][2] = 0.0f;   mForward.m[1][3] = vOrigin.y;
	mForward.m[2][0] = 0.0f;   mForward.m[2][1] = 0.0f;
	mForward.m[2][2] = 1.0f;   mForward.m[2][3] = vOrigin.z;
	mForward.m[3][0] = 0.0f;   mForward.m[3][1] = 0.0f;
	mForward.m[3][2] = 0.0f;   mForward.m[3][3] = 1.0f;

	*this = mForward * mReflect * mBack;
}


// ======================================================================= //
//  THE DIRECT3D 7 DEVICE AT 0x1008d988 -- the slice big16324.cpp and
//  big4b720.cpp model, cut down to the two slots this function uses.
//  Index = offset/4 against IDirect3DDevice7: +0x50 is 20 SetRenderState,
//  +0x64 is 25 DrawPrimitive.
// ======================================================================= //
// (D3DDevice7 is the one defined at the top of this file.)

#define D3DPT_TRIANGLEFAN                        6
#define D3DFVF_TLVERTEX                      0x1c4

#define D3DRENDERSTATE_ZFUNC                  0x17
#define D3DRENDERSTATE_STENCILENABLE          0x34
#define D3DRENDERSTATE_STENCILPASS            0x37
#define D3DRENDERSTATE_STENCILFUNC            0x38
#define D3DRENDERSTATE_STENCILREF             0x39

#define D3DCMP_EQUAL                             3
#define D3DCMP_LESSEQUAL                         4
#define D3DCMP_ALWAYS                            8

#define D3DSTENCILOP_KEEP                        1
#define D3DSTENCILOP_REPLACE                     3


// ======================================================================= //
//  THE VERTEX -- big16324.cpp's RVert, byte for byte.  The loop at
//  0x100279f0 strides 0x20 and touches +0x00/+0x04/+0x08 as floats,
//  +0x0c as a float and +0x10/+0x11/+0x12 as bytes.
// ======================================================================= //
struct RVert26728
{
	Vec26728  m_vPos;                                   // +0x00
	float     m_fRHW;                                   // +0x0c
	uint8     m_b;                                      // +0x10
	uint8     m_g;                                      // +0x11
	uint8     m_r;                                      // +0x12
	uint8     m_a;                                      // +0x13
	uint8     m_sb;                                     // +0x14
	uint8     m_sg;                                     // +0x15
	uint8     m_sr;                                     // +0x16
	uint8     m_sa;                                     // +0x17
	float     m_fU;                                     // +0x18
	float     m_fV;                                     // +0x1c
};


// ======================================================================= //
//  THE OBJECTS
// ======================================================================= //

// The thing hanging off each record's object at +0x28.  Only two fields are
// touched.  +0x34 is a POINTER to an LTPlane, not an embedded one: the
// argument setup at 0x100267fb is `mov 0x34(%edx),%edx`, a LOAD, where an
// embedded plane handed to TransformPlane's `LTPlane&` would be a `lea`.
// +0x42 is the 16-bit word that d3dr5d.cpp's note on 0x10066d10 identifies
// as the visibility stamp; it is read twice with `movzwl 0x42(%ecx),%eax`,
// once masked with 0x7fff and once tested with 0x8000.
struct Sub26728
{
	uint8       m_pad_00[0x34];                         // +0x00 .. +0x33
	Plane26728* m_pPlane34;                             // +0x34
	uint8       m_pad_38[0x0a];                         // +0x38 .. +0x41
	uint16      m_w42;                                  // +0x42
};

// The record's object.  +0x18/+0x1c/+0x20 are copied wholesale into
// 0x1008bd58/5c/60 at 0x10027cea..0x10027d04.
struct Obj26728
{
	uint8       m_pad_00[0x18];                         // +0x00 .. +0x17
	uint32      m_18;                                   // +0x18
	uint32      m_1c;                                   // +0x1c
	uint32      m_20;                                   // +0x20
	uint8       m_pad_24[0x04];                         // +0x24 .. +0x27
	Sub26728*   m_p28;                                  // +0x28
};

// 0x40(%edx) / 0x140(%edx) of the 0x1007b8d0 singleton: d3dr5d.cpp's
// "0x20-entry table at 0x140/0x40 of the manager".  Eight bytes a record;
// 0x140 - 0x40 = 0x100 = 0x20 * 8, so the table is exactly full-width.
struct SRec26728
{
	Obj26728*   m_pObj;                                 // +0x00
	Mat26728*   m_pXForm;                               // +0x04
};

struct Mgr26728
{
	uint8       m_pad_000[0x40];                        // +0x000 .. +0x03f
	SRec26728   m_aRecs[0x20];                          // +0x040 .. +0x13f
	uint32      m_nRecs;                                // +0x140
};

// The scene sink at *0x1008beb0 (big19d64.cpp's Src19d64Sink; big61a30.cpp
// reaches its +0x9c/+0xa0 array).  Two fields are read here.
struct Thing26728;

struct Sink26728
{
	uint8        m_pad_00[0x34];                        // +0x00 .. +0x33
	Thing26728*  m_p34;                                 // +0x34
	uint8        m_pad_38[0x84];                        // +0x38 .. +0xbb
	uint32       m_bc;                                  // +0xbc
};

// 0x34(%eax) of the sink.  Only its 16-bit +0x0c is read, and it goes
// straight into the frame stamp 0x1008bea8 that big17c14.cpp already names.
struct Thing26728
{
	uint8        m_pad_00[0x0c];                        // +0x00 .. +0x0b
	uint16       m_w0c;                                 // +0x0c
};

// The 0x4f8-byte transform object at 0x1008b880 that d3dlow.cpp, d3dr2a.cpp,
// big16324.cpp and big124a4.cpp all see pushed as a context argument.  Its
// size is fixed by the two `rep movsl`s with %ecx = 0x13e, and +0x4cc is
// fixed by the read at 0x10027cf3 landing at -0x5fc = -0xac8 + 0x4cc.
struct XForm26728
{
	XForm26728* sub_100234b0();                         // 0x100234b0

	uint8       m_pad_000[0x1c];                        // +0x000 .. +0x01b
	uint32      m_01c;                                  // +0x01c
	uint32      m_020;                                  // +0x020
	uint32      m_024;                                  // +0x024
	uint32      m_028;                                  // +0x028
	uint8       m_pad_02c[0x4a0];                       // +0x02c .. +0x4cb
	uint32      m_4cc;                                  // +0x4cc
	uint8       m_pad_4d0[0x28];                        // +0x4d0 .. +0x4f7
};

// The struct-of-function-pointers at *0x1008cbcc (big19d64.cpp's
// g_p1008cbcc).  `mov 0x1008cbcc,%edx ; call *0x28(%edx)` -- ONE
// dereference, so +0x28 is a function pointer IN the object, not a vtable
// slot; and no argument block is opened around it, so it takes none.
struct Sys26728
{
	uint8    m_pad_00[0x28];                            // +0x00 .. +0x27
	uint32 (*m_pfn28)();                                // +0x28
};


// ======================================================================= //
//  GLOBALS -- every one is an address this function touches
// ======================================================================= //

extern Sink26728*   g_p1008beb0;                        // 0x1008beb0
extern Sys26728*    g_p1008cbcc;                        // 0x1008cbcc
extern XForm26728   g_1008b880;                         // 0x1008b880

// The camera basis and eye point.  d3dr5c.cpp assigns the three
// SetBasisVectors arguments in this order; big16324.cpp already names
// 0x1008bd34 as the eye point.
extern Vec26728     g_v1008bd10;                        // 0x1008bd10  up
extern Vec26728     g_v1008bd1c;                        // 0x1008bd1c  right
extern Vec26728     g_v1008bd28;                        // 0x1008bd28  forward
extern Vec26728     g_v1008bd34;                        // 0x1008bd34  eye

// The six the world-draw reads back out.
extern int32        g_1008bd4c;                         // 0x1008bd4c
extern float        g_f1008bd50;                        // 0x1008bd50
extern int32        g_1008bd54;                         // 0x1008bd54
extern uint32       g_1008bd58;                         // 0x1008bd58
extern uint32       g_1008bd5c;                         // 0x1008bd5c
extern uint32       g_1008bd60;                         // 0x1008bd60

extern uint32       g_1008b874;                         // clip flags
extern uint32       g_1008bea4;                         // 0x1008bea4
extern uint16       g_w1008bea8;                        // 0x1008bea8
extern uint32       g_1008ef80;                         // 0x1008ef80

// The screen-space z encode pair big5a960.cpp names, plus the far value.
extern float        g_f1008b90c;                        // 0x1008b90c
extern float        g_f1008bbac;                        // 0x1008bbac
extern float        g_f1008bbb0;                        // 0x1008bbb0

// Console variables / gates.
extern int32        g_CV_LightMap;                      // 0x10089400
extern int32        g_10089420;                         // 0x10089420
extern uint32       g_1008d574;                         // 0x1008d574
extern uint32       g_1008d594;                         // 0x1008d594


// ======================================================================= //
//  THE OUT-OF-LINE CALLEES
// ======================================================================= //

// sub_1007b8d0 (0x1007b8d0) is declared above, returning Mgr1007b8d0*.
extern void      sub_1001e520(Thing26728 *pThing);      // 0x1001e520
extern int32     sub_1002803c(Obj26728 *pObj, RVert26728 **ppVerts,
                              int32 *pnVerts, Mat26728 *pXForm);
                                                        // 0x1002803c
extern void      sub_10020d80(void *pOut, float fA, uint32 nB,
                              XForm26728 *pXForm, float fMinX, float fMinY,
                              float fMaxX, float fMaxY);// 0x10020d80
extern void      sub_1001ef30(XForm26728 *pXForm, void *pIn, float fMinX,
                              float fMinY, float fMaxX, float fMaxY,
                              Mat26728 *pMat, Vec26728 vScale);
                                                        // 0x1001ef30
// 0x10029894 is sub_10029890's register entry, defined above.
struct Obj299a8;
struct Mat299a8;
void             sub_100299a0(Obj299a8 *pObj, Mat299a8 *pXForm);   // below
                                                        // 0x100299a8


// ======================================================================= //
//  0x10026728 -- 6,024 bytes (0x10026728..0x10027eb0), one caller.
//
//  THE MIRROR PASS.  Register-ABI: %eax = the scene sink, %edx = the
//  0x1007b8d0 manager.  Nothing is returned (the caller discards %eax).
//
//  The structure, block by block, with the address each block starts at:
//
//    0x10026728  prologue, `saved`'s constructor, g_1008ef80 = 0
//    0x10026755  read the record count; the `je 0x10027e25` is this
//                function's early exit and the two `jbe`s at 0x10026768
//                and 0x10026796 are the two loops' own zero guards --
//                three branches, TWO `test`s, one per `for`
//    0x10026770  loop 1: copy the manager's table into `aRecs`
//    0x1002678b  pThing = pSink->m_p34
//    0x100267b2  loop 2 head, walked as a pointer from %ebp to %ebp+n*8
//    0x100267cd  the 0x7fff/0x7ffe stamp test -- `jne` to the loop tail
//    0x100267ed  TransformPlane and the 16-byte copy of its return slot
//    0x10026840  the Dot/dist reject -- `jb` to the loop tail
//    0x1002687c  mCam: Identity, SetBasisVectors, SetTranslation
//    0x100268d9  vOrigin = plane.m_Normal * plane.m_Dist
//    0x100268f7  SetupReflectionMatrix, inlined (3,913 bytes, no calls)
//    0x10027841  the per-mirror setup and the four stencil states
//    0x100278e7  g_1008b874 = 0x3f and the 0x1002803c gate
//    0x10027920  three more stencil states and the z/rhw constants
//    0x100279f0  the vertex loop: stamp z/rhw/colour, accumulate the box
//    0x10027ae7  z states, DrawPrimitive, and the `!a || b` stencil reset
//    0x10027bf5  save the transform object, build the reflected view,
//                publish the six globals, draw the world, restore
//    0x10027d71  the ZFUNC bracket around the optional 0x100299a8
//    0x10027dd9  STENCILENABLE off -- the loop tail
//    0x10027df9  advance the record pointer and loop
// ======================================================================= //

void sub_10026720(Sink26728 *pSink, Mgr26728 *pMgr)
{
	XForm26728   saved;
	SRec26728    aRecs[0x20];
	Plane26728   plane;
	Mat26728     mCam;
	Vec26728     vOrigin;
	Mat26728     mReflect;
	Mat26728     mFull;
	RVert26728*  pVerts;
	int32        nVerts;
	float        fMinX, fMinY, fMinZ;
	float        fMaxX, fMaxY, fMaxZ;
	uint8        aBlock[0x1c];
	Thing26728*  pThing;
	Obj26728*    pObj;
	Sub26728*    pSub;
	Mat26728*    pXForm;
	uint32       nRecs, i;
	int32        j;
	int32        nSaveLightMap;
	float        fZ, fRHW;

	// -- 0x10026740: `lea -0xac8(%ebp),%ecx ; call 0x100234b0`.  This is
	//    the constructor of `saved`; it is spelled as the named member
	//    d3dr4b.cpp reconstructed so that the call site resolves.
	saved.sub_100234b0();

	// -- 0x1002674b
	g_1008ef80 = 0;

	// -- 0x10026755 .. 0x10026789
	nRecs = pMgr->m_nRecs;

	for (i = 0; i < nRecs; i++)
	{
		aRecs[i].m_pObj   = pMgr->m_aRecs[i].m_pObj;
		aRecs[i].m_pXForm = pMgr->m_aRecs[i].m_pXForm;
	}

	// -- 0x1002678b
	pThing = pSink->m_p34;

	for (i = 0; i < nRecs; i++)
	{
		// -- 0x100267b2 .. 0x100267e7
		pObj   = aRecs[i].m_pObj;
		pXForm = aRecs[i].m_pXForm;
		pSub   = pObj->m_p28;

		if ((pSub->m_w42 & 0x7fff) != 0x7ffe)
			continue;

		// -- 0x100267ed: `ret $8` thiscall, the return slot at -0x4d0 and
		//    the plane by reference; the 16-byte copy into `plane` at
		//    0x1002680d is the assignment.
		plane = pXForm->TransformPlane(*pSub->m_pPlane34);

		// -- 0x10026837 .. 0x10026876.  `fcomps 0x100a5e4c` (0.0f) then
		//    `jb`, so the reject is `< 0.0f` and the body runs on `>=`.
		if (plane.m_Normal.Dot(g_v1008bd34) - plane.m_Dist < 0.0f)
			continue;

		// -- 0x1002687c .. 0x100268d1
		mCam.Identity();
		mCam.SetBasisVectors(&g_v1008bd1c, &g_v1008bd10, &g_v1008bd28);
		mCam.SetTranslation(g_v1008bd34);

		// -- 0x100268d6: operator*(float) with the return slot at -0x470
		vOrigin = plane.m_Normal.sub_10015690(plane.m_Dist);

		// -- 0x100268f7 .. 0x10027331
		mReflect.SetupReflectionMatrix(plane.m_Normal, vOrigin);

		// -- 0x10027332 .. 0x10027838
		mFull = mReflect * mCam;

		// -- 0x10027829
		sub_1001e520(pThing);

		// -- 0x10027846 .. 0x10027862
		g_w1008bea8 = pThing->m_w0c;
		g_1008bea4  = g_p1008cbcc->m_pfn28();

		// -- 0x10027867 .. 0x100278e4
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_STENCILENABLE, 1);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_STENCILPASS, D3DSTENCILOP_REPLACE);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_STENCILFUNC, D3DCMP_ALWAYS);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_STENCILREF, 1);

		// -- 0x100278f0
		g_1008b874 = 0x3f;

		// -- 0x10027904: three register arguments and one stack argument
		//    at 0xc(%esp); `test %eax,%eax ; je 0x10027dd9` skips the
		//    whole body but NOT the STENCILENABLE reset at the tail, so
		//    this is an `if`, not a `continue`.
		if (sub_1002803c(pObj, &pVerts, &nVerts, pXForm))
		{
			Vec26728 vScale(1.0f, 1.0f, 1.0f);

			// -- 0x10027920 .. 0x1002797d
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_STENCILREF, 1);
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_STENCILPASS, D3DSTENCILOP_KEEP);
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_STENCILFUNC, D3DCMP_EQUAL);

			// -- 0x10027980 .. 0x100279e2.  See coin-flip (C).
			fZ   = g_f1008b90c - 1.0f;
			fRHW = 1.0f / fZ;

			// -- 0x10027996 .. 0x100279d0: 0x47c35000 / 0xc7c35000
			fMinX = 100000.0f;
			fMinY = 100000.0f;
			fMinZ = 100000.0f;
			fMaxX = -100000.0f;
			fMaxY = -100000.0f;
			fMaxZ = -100000.0f;

			// -- 0x100279e8 .. 0x10027ae1.  %ebx strides 0x20 and the
			//    base is reloaded from -0x264 every iteration because
			//    &pVerts was handed to 0x1002803c.
			for (j = 0; j < nVerts; j++)
			{
				pVerts[j].m_b = 0;
				pVerts[j].m_g = 0;
				pVerts[j].m_r = 255;

				pVerts[j].m_fRHW    = fRHW;
				pVerts[j].m_vPos.z  = (fZ * g_f1008bbac + g_f1008bbb0)
				                      * fRHW;

				fMinX = LTMIN(fMinX, pVerts[j].m_vPos.x);
				fMinY = LTMIN(fMinY, pVerts[j].m_vPos.y);
				fMinZ = LTMIN(fMinZ, pVerts[j].m_vPos.z);

				fMaxX = LTMAX(fMaxX, pVerts[j].m_vPos.x);
				fMaxY = LTMAX(fMaxY, pVerts[j].m_vPos.y);
				fMaxZ = LTMAX(fMaxZ, pVerts[j].m_vPos.z);
			}

			// -- 0x10027ae7 .. 0x10027b44
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_ZENABLE, 1);
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_ZWRITEENABLE, 1);
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);

			// -- 0x10027b47
			g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice,
				D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, pVerts, nVerts, 0);

			// -- 0x10027b83
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);

			// -- 0x10027ba3: `je 0x10027bb5` on the first global ENTERS
			//    the block and `je 0x10027bf5` on the second SKIPS it,
			//    which is `!a || b` short-circuited.
			if (!g_10089420 || g_1008d594)
			{
				g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
					D3DRENDERSTATE_STENCILENABLE, 0);
				g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
					D3DRENDERSTATE_STENCILFUNC, D3DCMP_EQUAL);
			}

			// -- 0x10027bf5: `rep movsl` %ecx = 0x13e
			saved = g_1008b880;

			// -- 0x10027c07: eight __cdecl dwords, the 0x40e00000 (7.0f)
			//    literal among them -- the same shape d3dr3b.cpp records
			//    for 0x10020c00's fourth argument.
			sub_10020d80(aBlock, 7.0f, pSink->m_bc, &g_1008b880,
			             fMinX, fMinY, fMaxX, fMaxY);

			// -- 0x10027c60: ten __cdecl dwords, the last three a 12-byte
			//    struct copied field by field into 0x1c(%esp).
			sub_1001ef30(&g_1008b880, aBlock, fMinX, fMinY, fMaxX, fMaxY,
			             &mFull, vScale);

			// -- 0x10027cdc .. 0x10027d3f
			g_1008bd58 = pObj->m_18;
			g_1008bd5c = pObj->m_1c;
			g_1008bd60 = pObj->m_20;

			g_1008bd4c  = (saved.m_4cc == 0);
			g_f1008bd50 = g_1008bd4c ? -1.0f : 1.0f;
			g_1008bd54  = 1;

			// -- 0x10027d32 / 0x10027e34 / 0x10027d48.  The read of
			//    g_CV_LightMap is hoisted ABOVE the branch (into %ebx,
			//    callee-saved) and the `= 0` arm is the cold block sunk
			//    to 0x10027e34.
			nSaveLightMap = g_CV_LightMap;

			if (!g_1008d574)
				g_CV_LightMap = 0;

			sub_10029890((Obj10029890*)pSink);

			g_CV_LightMap = nSaveLightMap;

			// -- 0x10027d56: `rep movsl` %ecx = 0x13e, the other way
			g_1008b880 = saved;

			// -- 0x10027d71
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);

			// -- 0x10027d91: the SAME +0x42 word, this time the top bit
			if (pSub->m_w42 & 0x8000)
				sub_100299a0((Obj299a8*)pObj, (Mat299a8*)pXForm);

			// -- 0x10027db9
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
		}

		// -- 0x10027dd9
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_STENCILENABLE, 0);
	}
}


// ======================================================================= //
//  0x1002b0c0 -- run 0x10026728 over the current scene (0x1008beb0) with
//  the manager 0x1007b8d0 answers.  Installed as a callback
//  (`movl $0x1002b0c0,0x1008ef80` at 0x1002d670) and never called
//  directly, which is why the enumeration lists it at size 0.  Moved here
//  from w4_10023fb0.cpp: it enters 0x10026728 at the /Qip register
//  entry (%eax = the scene, %edx = the manager), which needs this TU.
// ======================================================================= //
struct C1008beb0Thing;

extern C1008beb0Thing* g_1008beb0;                      // 0x1008beb0

void sub_1002b0c0()
{
	Mgr26728* pMgr = (Mgr26728*)sub_1007b8d0();
	sub_10026720((Sink26728*)g_1008beb0, pMgr);
}



void sub_10029890(Obj10029890* pObj)
{
	Elem10029890*   pElem;
	Elem10029890**  ppElems;
	D3DDevice7*     pDev;
	int32           nType;
	int32           nCount;
	int32           i;

	pDev = g_pD3DDevice;
	pDev->lpVtbl->SetRenderState(pDev, 0x88, g_1008d7a0 ? 1 : 0);

	sub_10026670();
	sub_10050390();
	sub_1001eb20();

	if (pObj->m_n00 == 2)
	{
		sub_1007b8d0()->sub_1007bae0();

		nCount = pObj->m_ne0;
		ppElems = pObj->m_pdc;
		for (i = 0; i < nCount; ++i)
		{
			pElem = ppElems[i];
			if (pElem)
			{
				nType = pElem->m_cbc;
				if (nType >= 0 && nType < 11 &&
				    g_a100895bc[nType].m_pfnSlot04)
				{
					g_a100895bc[nType].m_pfnSlot04(pElem);
				}
			}
		}
	}
	else
	{
		sub_10026480(0);
		sub_1007b880();
		sub_10026500(0);
	}

	sub_1004fbc0();
	sub_1001eb20();

	sub_10025db0(pObj->m_a50);
	sub_10025890(pObj->m_a5c);
}


// ======================================================================= //
// *********************************************************************** //
//
//  0x100299a0 / 0x100299a8 -- formerly big299a8.cpp (merged here:
//  its one caller 0x10026728, above, calls it at the /Qip register
//  entry 0x100299a8, and icl gives a function that shape only when the
//  caller is in its TU).  That file's notes follow, unchanged except
//  where marked.
//
//  PURPOSE:  One function.  The enumeration row is
//                0x100299a8, 5944
//            and BOTH of its numbers are wrong.  See EXTENT below.
//
//            CORRECTED START:  0x100299a0   (the /Qip STACK entry)
//            CORRECTED END:    0x1002b0c0   (exclusive)
//            CORRECTED SIZE:   5,920 bytes  (0x1720)
//
//            What it is: d3d.ren's MIRROR-SURFACE DRAW -- the pass that
//            paints the mirror polygon ITSELF, alpha-blended, on top of
//            the reflected world that 0x10026728 has just drawn through
//            it.  Transform the polygon's vertices by the mirror's own
//            4x4 (with a perspective divide), white diffuse, texture
//            coordinates scaled by the bound texture's 1/w,1/h, fog into
//            specular; then the standard six-plane frustum clip, the
//            screen projection, and one DrawPrimitive triangle fan.
//
//  The thirty colour bytes go through RoundF, the __asm
//  `fld ; fistp` helper (w1_10001000.cpp), which is the shipped
//  `fstps t ; flds t ; fistpl n` -- 8,191 -> 5,759 bytes (shipped 5,944).
//  The function is a /Qip register-entry pair (0x100299a0 / 0x100299a8,
//  `push %edx ; push %eax` saving the register arguments); its one caller
//  0x10026728 is in w28_100256d0_qipni.cpp -- the shipped TU, which is
//  where this body belongs for the pair and the register call.
//  MOVED there.  In 0x10026728's TU the body opens as the
//  shipped one does -- `push %ebp ; mov %esp,%ebp ; push %edx ; push %eax ;
//  mov $0x1720,%eax ; call _chkstk`, the register arguments saved and the
//  frame exactly 0x1720 (it was a plain stack function with a 0x1728
//  frame) -- and the row pairs: attempted -> near.
//
//  Compiler: Intel C/C++ 4.0, /O2.  All the /O2 tells:
//              * every outgoing argument block is `add $-N,%esp` followed
//                by `mov %reg,K(%esp)` -- not one `push` in 5.9 KB
//                (0x100299d2, 0x10029a0d, 0x10029a2d, 0x10029ba0,
//                 0x1002af20, 0x1002af43, 0x1002af83);
//              * TWO epilogues (0x10029a09 and 0x1002afa3), duplicated
//                rather than cross-jumped;
//              * the six clip-epsilon `else fT = 0.0f;` arms are all sunk
//                below the last hot block, to 0x1002afb2..0x1002b0bf,
//                and every one of them jumps BACKWARDS into the body.
//            Register-passing convention (/Qip): arg0 in %eax, arg1 in
//            %edx.  That is decoded as an ordinary call whose
//            arguments arrive in registers, which is what is done here.
//
// ----------------------------------------------------------------------- //
//  EXTENT -- THE ROW IS WRONG AT BOTH ENDS
// ----------------------------------------------------------------------- //
//
//      extent check of 0x100299a8, 5944 bytes:
//      -> "/Qip  !! THIS ROW STARTS AT A REGISTER ENTRY"
//      -> "ret 3", "the final 8 bytes are alignment padding"
//
//  (a) THE START IS EIGHT BYTES LATE.  Walking back from 0x100299a8:
//
//        1002999a  jmp 0x10029909        <- last insn of the PREVIOUS fn
//        1002999f  nop                   <- 1 byte of alignment padding
//        100299a0  mov 0x4(%esp),%eax    <- /Qip STACK entry, arg0
//        100299a4  mov 0x8(%esp),%edx    <- arg1
//        100299a8  push %ebp             <- /Qip REGISTER entry (the row)
//        100299a9  mov %esp,%ebp
//        100299ab  push %edx ; push %eax
//        100299ad  mov $0x1720,%eax ; call 0x100805a0     <- _chkstk
//
//      The two `mov`s fall straight through into the `push %ebp`; they
//      are the /Qip two-entry pattern.  The previous function is
//      0x10029890 (d3dqip3.cpp has it decoded in full, 280 bytes); its
//      `ret` is at 0x10029939 and the 0x1002993a..0x1002999a tail is its
//      /O2-sunk per-object sweep, whose last instruction is the
//      `jmp 0x10029909` above.  So the byte before our function is
//      padding and the byte before that is the previous function's last
//      instruction: 0x100299a0 is a function start.
//
//      NOTHING in .text references 0x100299a0.  The one caller is
//      0x10027db1 `call 0x100299a8` -- the register entry -- from inside
//      0x10026728, which is exactly the `/Qip` two-entry
//      residual (shipped calls f+8 where we call f).
//
//  (b) THE ROW SPANS TWO FUNCTIONS.  0x1002b0c0..0x1002b0d7 is a
//      SEPARATE 24-byte frameless function:
//
//        1002b0c0  sub $0xc,%esp
//        1002b0c3  call 0x1007b8d0
//        1002b0c8  mov %eax,%edx
//        1002b0ca  mov 0x1008beb0,%eax
//        1002b0cf  call 0x10026728
//        1002b0d4  add $0xc,%esp
//        1002b0d7  ret
//
//      It is not a cold block of ours: it has its own stack adjustment
//      and its own `ret`, our two epilogues restore %ebx/%esi/%edi and do
//      `mov %ebp,%esp ; pop %ebp`, and NO branch anywhere in
//      0x100299a0..0x1002b0bf targets it.  It is reached only as a
//      FUNCTION POINTER: 0x1002d670 is
//      `movl $0x1002b0c0,0x1008ef80`, and big26728.cpp records
//      `g_1008ef80 = 0` at the head of 0x10026728, i.e. the slot is the
//      "draw the mirrors" callback.  Its own body is the register-ABI
//      call big26728.cpp already quotes as 0x10026728's single caller.
//      Reported as a candidate at the foot of this file.
//
//  (c) AFTER THE SPLIT EVERY CHECK PASSES.
//
//      extent check of 0x100299a0, 5920 bytes:
//        ret 2            0x10029a0c, 0x1002afb1   (both full epilogues)
//        push %ebp        1                        0x100299a8
//        branch targets   0 outside the range or mid-instruction
//        last instruction ends exactly at the stated end
//
//      Two `ret`s, ONE prologue: the /O2 duplicated epilogue.  Both are
//      `mov (%esp),%ebx ; mov 0x4(%esp),%esi ; mov 0x8(%esp),%edi ;
//       mov %ebp,%esp ; pop %ebp ; ret` restoring the three registers the
//      prologue saved at 0x100299c5..0x100299cf.
//
//      extent check of 0x1002b0c0..0x1002b0d8:
//        ret 1, no prologue (frameless: normal for icl), nothing outside.
//      0x1002b0d8..0x1002b0df is 8 bytes of alignment padding, and
//      0x1002b0e0 is the next enumerated function (208 bytes,
//      d3dr3d.cpp's fog-mode installer -- it is what WRITES
//      g_pfn1008ce8c, see INDIRECT (2)).
//
//      The two sizes sum to the row: 5,920 + 24 + 8 pad = 5,952
//      = 5,944 + the 8 bytes the row's start was late by.
//
// ----------------------------------------------------------------------- //
//  THE ACCEPTANCE TEST, AND HOW LITTLE OF THE FUNCTION IT SEES
// ----------------------------------------------------------------------- //
//
//      call sequence over 0x100299a0..0x1002b0c0:
//      5,920 bytes, 9 calls (5 direct, 4 indirect), 658 bytes per call
//
//  FIVE distinct direct callees:
//      0x100805a0 _chkstk   0x10015320   0x1000a700
//      0x1000d9b0           0x10014580
//
//  EXPECTED: 9 shipped / 8 ours, 8 in agreement (4 by resolved target,
//  4 by position only), 1 shipped-only -- the `_chkstk` at 0x100299b2,
//  the benign large-frame residual.  Do NOT add /Ge.
//  It does NOT need /Oi-: there is no sqrt/fabs/tan/atan/memcpy shape
//  anywhere in the function.  /Oi- would be actively harmful here --
//  the six `*pOut = *pPrev` 32-byte struct copies are open-coded as
//  eight dword `mov` pairs in the shipped bytes (0x10029cb1..0x10029cea
//  and five siblings), and turning intrinsic expansion off risks six
//  ours-only `memcpy` calls.
//
//  THE CALL SITES ARE ALL IN THE FIRST 540 AND THE LAST 200 BYTES:
//      0x100299b2 _chkstk        0x100299f2 0x10015320
//      0x10029a2a *0x50(%eax)    0x10029a47 0x1000a700
//      0x10029b41 *%eax          0x10029baa 0x1000d9b0
//      0x1002af2a 0x10014580     0x1002af80 *0x64(%ecx)
//      0x1002afa0 *0x50(%eax)
//
//  **0x10029baf .. 0x1002af29 -- 4,987 bytes, 84.2% of the function --
//  contains not one call of any kind**, and 0x1002afb2..0x1002b0bf (the
//  six sunk epsilon arms, 270 bytes, 4.6%) is a second call-free stretch
//  after the last call.  5,257 bytes, 88.8%, hold no call at all.  The
//  two call-bearing clusters -- 0x100299a0..0x10029bae (527 bytes) and
//  0x1002af2a..0x1002afb1 (136 bytes) -- are 663 bytes, so **the call sequence
//  witnesses 11.2% of the function.**  Of the blind part,
//  0x10029bdc..0x1002aeee (4,883 bytes, 82.5%) is the six inlined clip
//  planes.  Everything that pins that region is below, and none of it is
//  the call sequence.
//
// ----------------------------------------------------------------------- //
//  INSTRUMENT 1 -- THE FRAME DECOMPOSES EXACTLY, SLOTS DUMPED NOT ASSUMED
// ----------------------------------------------------------------------- //
//  `mov $0x1720,%eax ; call 0x100805a0` is _chkstk for 5,920 bytes, and
//  every byte of it is named.  The offsets below are the ones that
//  actually appear in the disassembly (`grep -oE '\-0x[0-9a-f]+\(%ebp\)'`,
//  75 distinct, all accounted for):
//
//   ebp-0x1728 .. -0x171d    12   %ebx/%esi/%edi spill.  They are stored
//                                 at (%esp)/0x4(%esp)/0x8(%esp) AFTER
//                                 _chkstk, i.e. at the very bottom.
//   ebp-0x171c .. -0x1701    28   alignment slack, never referenced
//   ebp-0x1700 .. -0x1615   236   THIRTY float temps, 8 apart:
//                                 -0x1700,-0x16f8,...,-0x1618.  Six
//                                 planes x { fRed fGreen fBlue fAlpha
//                                 fSpec }.  Each is `fstps` then `flds`
//                                 then `fistpl`, 4 live bytes in an
//                                 8-byte slot -- and the census below has
//                                 fstps 30 and flds 30 on exactly these
//                                 thirty addresses, no more, no fewer.
//   ebp-0x1614                4   pData   (out-param of 0x1000a700;
//                                 `lea -0x1614(%ebp),%edx` at 0x10029a36)
//   ebp-0x1610                4   nVerts  (out-param; `lea` at 0x10029a30)
//   ebp-0x160c .. -0x060d  4096   RVert aVerts[128] -- 128 * 0x20 EXACTLY.
//                                 -0x160c is `lea`d three times
//                                 (0x10029a5d, 0x10029b6e, 0x10029b91)
//                                 and -0x160c + 0x1000 = -0x060c, the
//                                 next named slot.  128 is the bound
//                                 0x1000a740 enforces for this same
//                                 geometry call (big572b4.cpp).
//   ebp-0x060c                4   pVerts  (`mov %eax,-0x60c(%ebp)` at
//                                 0x10029b7a with %eax = &aVerts[0])
//   ebp-0x0608 .. -0x00e1  1320   SIX clip blocks of 0xdc:
//                                 0xc8 = int32 abInside[50], then
//                                 0x14 = five int slots for the five
//                                 (uint8)float conversions.
//                                 The six flag-array bases are
//                                 -0x608, -0x52c, -0x450, -0x374,
//                                 -0x298, -0x1bc -- 0xdc apart, each
//                                 `lea`d exactly once (0x10029c27,
//                                 0x10029f49, 0x1002a281, 0x1002a5a4,
//                                 0x1002a8cd, 0x1002ac00) and each the
//                                 base of exactly one `cmp
//                                 -0xXXX(%ebp,%edi,4),%eax`.
//                                 The six five-int groups start at
//                                 -0x540, -0x464, -0x388, -0x2ac,
//                                 -0x1d0, -0xf4 -- each exactly 0xc8
//                                 into its block.  -0x608 + 6*0xdc
//                                 = -0xe0.
//   ebp-0x00e0                4   THE shared int every `fildl` reads.
//                                 120 references, and all 60 movzbl->
//                                 fildl pairs use this one slot.
//   ebp-0x00dc .. -0x0009   212   the scalars and icl's slack:
//                                 -0xcc pOut       (74 refs)
//                                 -0xc8 nIn        (29 refs)
//                                 -0xc4 pOutStart  (12 refs = 6 x 2)
//                                 -0xc0 nClipFlags ( 8 refs)
//                                 -0x90 pIn spill  (12 refs = 6 x 2)
//   ebp-0x0008                4   `push %eax` -- arg0's spill slot, read
//                                 once at 0x100299be and DEAD after.
//                                 icl rehosts `i` in it: the 12
//                                 references are the six edge loops'
//                                 `mov %edi,-0x8(%ebp)` /
//                                 `mov -0x8(%ebp),%edi` around the
//                                 32-byte copy.  The known "a dead
//                                 parameter slot also hosts ordinary
//                                 temporaries".
//   ebp-0x0004                4   `push %edx` -- arg1's spill slot, read
//                                 once at 0x100299b7, dead after.
//
//     12 + 28 + 236 + 4 + 4 + 4096 + 4 + 1320 + 4 + 212 = 5,920 = 0x1720
//
//  TWO ARRAY BOUNDS ARE READ OFF THE FRAME, not assumed: aVerts[128]
//  from 0x1000/0x20, and abInside[50] from 0xc8/4.
//
// ----------------------------------------------------------------------- //
//  INSTRUMENT 2 -- AN INSTRUCTION CENSUS WITH NO RESIDUE
// ----------------------------------------------------------------------- //
//  1,542 instructions in 0x100299a0..0x1002b0bf.  Every mnemonic in the
//  function is in this table and every row falls out of the structure
//  claimed below.  The rows over five are decomposed BY OPERAND.
//
//   | mnemonic | n   | what the claimed structure requires |
//   |----------|-----|-------------------------------------|
//   | flds     | 108 | decomposed below -- the load-bearing row |
//   | fstps    |  61 | 30 byte-lerp temps + 12 texture (6x2) + 14 position + 5 transform |
//   | movzbl   |  60 | 6 planes x 5 byte lerps x 2 widenings.  NOTHING else widens a byte |
//   | fildl    |  60 | the same 30 byte lerps x 2 (difference, base) |
//   | fistpl   |  30 | the same 30 byte lerps x 1 |
//   | fmul     |  50 | 6 x 8 uses of fT (2 position + 2 texture + 4 of the 5 byte lerps) + 2 transform (`* fOOW` for x and y) |
//   | fmulp    |   7 | the 5th byte lerp of each plane (operand order forces the pop form) + the transform's z |
//   | fmuls    |  14 | 12 = 4 matrix rows x 3 components + 2 = the u,v texture scales |
//   | faddp    |  40 | 30 byte-lerp adds + 2 numerators (planes 2,5) + 8 = 4 matrix rows x 2 |
//   | fadds    |  32 | 24 = 6 x 4 lerp bases + 4 fDen tails (planes 2,3,4,5) + 4 matrix row translations |
//   | fsubs    |  28 | 24 = 6 x 4 lerp differences + 4 fDen tails |
//   | fsub     |   6 | the six `pCur->? - pPrev->?` that OPEN each fDen |
//   | fsubrs   |   2 | `g_1008b908 - pPrev->z` (plane 1) and `g_1008b910 - pPrev->z` (plane 6) |
//   | fsubrp   |   2 | planes 3 and 4's `pPrev->y - pPrev->z` / `pPrev->x - pPrev->z` (DE E8+i is Intel FSUBP -- see the objdump warning below) |
//   | fchs     |   8 | 2 classify negations (2,5) + 4 numerator negations (2,3,4,5) + 2 result negations (2,5) |
//   | fld      |   2 | planes 2 and 5 duplicating fZ before negating it |
//   | fsts     |   2 | plane 3's `y = fZ` and plane 4's `x = fZ` |
//   | fxch     |  15 | 6 (the 5th byte lerp) + 8 (1+2+1+1+2+1 in the numerators/results of planes 2..5) + 1 (the u/v pair) |
//   | fdivp    |   6 | one divide per plane |
//   | fdivrs   |   1 | the transform's `1.0f / fW` -- the ONLY divide outside the clipper |
//   | fcomps   |   6 | the six classify tests, one per plane |
//   | fcomp    |  12 | 6 x `fDen < -1e-5` + 6 x `fDen > 1e-5` |
//   | fnstsw   |  18 | those 6 + 12, one each |
//   | sahf     |  18 | ditto |
//   | fstp     |  16 | the six sunk arms' x87 cleanup: 2+3+3+3+3+2 |
//   | shl      |  17 | 6 (pEnd = pIn + nIn) + 6 (pPrev = pIn + nIn-1) + FIVE (pOut += nIn) -- see (*) |
//   | sar      |   6 | six `(pOut - pOutStart) / 32` |
//   | sub      |  36 | 30 byte-lerp integer differences (24 `%edx,%eax` + 6 `%ebx,%eax`) + 6 pointer differences (1 + 5) |
//   | add      |  68 | decomposed below |
//   | lea      |  37 | 23 lea-nop pads + 14 real -- decomposed below |
//   | inc      |  12 | 6 x `g_1008be44++` + 6 x `i++` |
//   | dec      |   9 | 6 x `j = nIn - 1` + the THREE `while (n--)` loops |
//   | xor      |  12 | 6 x `nInside = 0` + 6 x `i = 0` |
//   | test     |  30 | 24 = 6 x (bit mask, nInside==0, nIn<=0, abInside[j]) + 6 scalar tests |
//   | cmp      |  30 | 6 x 5 (pIn==pEnd, nInside!=nIn, pWalk!=pEnd, abInside[j]!=abInside[i], i<nIn) |
//   | je       |  41 | 6 x 6 + 5 |
//   | jne      |  10 | 6 classify back-edges + 1 (0x10015320's result) + 3 loop back-edges |
//   | jae      |   1 | plane 1's classify (`z >= near`) |
//   | jbe      |   7 | 6 in-line epsilon low tests + plane 6's classify (`z <= far`) |
//   | jb       |  10 | 4 classify (planes 2,3,4,5) + 6 sunk-arm high tests |
//   | jl       |   6 | the six edge loops' `i < nIn` |
//   | jle      |   6 | the six `nIn <= 0` guards |
//   | jmp      |   6 | the six sunk epsilon arms, and nothing else |
//   | and      |   1 | `nClipFlags &= 1` under g_1008d7a0 |
//   | nop      |   5 | alignment |
//   | mov      | 563 | bulk |
//   | push     |   3 | `push %ebp ; push %edx ; push %eax` |
//   | pop      |   2 | the two epilogues' `pop %ebp` |
//   | ret      |   2 | the two epilogues |
//   | call     |   9 | the nine call sites listed above |
//
//  `flds` 108 decomposes BY OPERAND with nothing over:
//      0x8(%edx) x10 = 4 classify (planes 1,2,5,6) + 2 fDen opens (1,6)
//                      + 4 numerator pPrev->z loads (2,3,4,5)
//      0x4(%edx) x3  = 1 classify (3) + 2 fDen opens (3,5)
//      (%edx)    x3  = 1 classify (4) + 2 fDen opens (2,4)
//      0x8(%esi) x6  = 2 fDen opens (1,6) + 4 z-lerps (2,3,4,5)
//      (%esi)    x6  = 4 x-lerps (1,3,5,6) + 2 fDen opens (2,4)
//      0x4(%esi) x6  = 4 y-lerps (1,2,4,6) + 2 fDen opens (3,5)
//      0x18(%esi) x6 + 0x1c(%esi) x6 = the TWELVE texture lerps
//                      -- ONE uv pair per vertex, not two.  A second pair
//                      would make this 24 and move fsubs/fadds/fmul too.
//      0x100a60d8+8k x6 (-1e-5) + 0x100a6108..0x100a6134 x12 (+1e-5, 0.0)
//                    = 18 constants
//      -0x1700(%ebp) .. -0x1618(%ebp) x30 = the byte-lerp float temps
//      (%eax) x2, 0x4(%eax) x3, 0x8(%eax) x2 = the source position, twice
//                      for the w and x rows plus one 0x4(%eax) for the u
//      0x10/0x14/0x18(%ebx) x1 each + 0x20/0x24/0x28(%ebx) x1 each = 6
//                      matrix elements, the y and z rows
//      0x1008e668 x1 = the v scale
//                                                        ---- 108 exactly
//
//  `add` 68, by operand:
//      %eax,%ecx  11 = 6 `pOut++` after the lerp (`mov $0x20,%ecx ; add`)
//                      + FIVE `pOut += nIn`         (see (*) )
//      %eax,%edi   6 = `pEnd = pIn + nIn`
//      %eax,%ebx   6 = `pPrev = pIn + (nIn - 1)`
//      %esi,%ebx   6 = `nInside += nFlag`
//      $0x4,%ecx   6 = `pFlag++`
//      $0x20,%edx  6 = the six classify loops' `pWalk++`
//      $0x20,%esi  7 = the six edge loops' `pWalk++` + the transform's
//                      `pDst++`
//      $0x20,%eax  7 = the six `pOut++` after the 32-byte copy + the
//                      transform's biased second induction pointer
//      $0x20,%ebx  2 = the 0x1000d9b0 loop and the 0x10014580 loop
//      $-N,%esp / $N,%esp 11 = the eleven outgoing-argument-area
//                      adjustments (five opened+closed pairs plus the
//                      DrawPrimitive open, which the epilogue closes)
//                                                        ---- 68 exactly
//
//  `lea` 37, by operand:
//      23 pads: `lea 0x0(%esi,%eiz,1),%esi` x12,
//               `lea 0x0(%edi,%eiz,1),%edi` x7,
//               `lea 0x0(%esi),%esi` x3, `lea 0x0(%edi),%edi` x1
//      14 real: SIX flag-array bases (one per plane -- a seventh plane
//               would make it seven), THREE `-0x160c(%ebp)` (the
//               transform's %esi, `pVerts = aVerts`, and the 0x1000d9b0
//               loop's %ebx), ONE `-0x15f8(%ebp)` (= &aVerts[0] + 0x14,
//               the fog out-parameter), `-0x1610` and `-0x1614` (the two
//               out-parameters of 0x1000a700), ONE `0x18(%edx)` -- THE
//               SOURCE RECORD STRIDE, 24 bytes -- and ONE
//               `-0x2(%eax,%edx,1)` = `g_1008be34 + nVerts - 2`.
//                                                        ---- 37 exactly
//
//  (*) `shl` 17 and `add %eax,%ecx` 11 both say FIVE, not six,
//  `pOut += nIn`.  Plane 6's copy is absent because pOut is dead after
//  it -- the same dead-`pOut +=` big051a0.cpp found in ITS pass 6 by
//  counting `lea`.  No call, no constant and no frame slot shows that.
//
//  A SEVENTH PLANE, A MISSING PLANE, A SECOND TEXTURE PAIR, A SIXTH
//  COLOUR BYTE or a THIRD position lerp would each move at least three
//  of movzbl/fildl/fistpl/flds/fsubs/fadds/cmp/test, and none of them
//  moves.
//
//  ONE PREDICTED CENSUS DIVERGENCE, and it is a known toolchain gap
//  rather than a decode error.  The thirty `(uint8)float` conversions
//  ship as `fstps <temp> ; flds <temp> ; fistpl <int>` with no
//  fnstcw/fldcw and no helper call.  Measured: plain
//  /O2 icl instead emits a shld/neg/or/shr idiom at every such site and
//  that `/Qrcd` cuts it back (icl 4.0 has NO /QIfist -- it parses as
//  /QIf and is silently ignored).  So expect our fistpl 30 -> 0 with
//  shld/neg/or/shr appearing at exactly 30, and the fstps/flds rows to
//  move by 30 with them.  It costs no CALL either way, so the call-sequence
//  figure is unaffected.
//
// ----------------------------------------------------------------------- //
//  INSTRUMENT 3 -- THE LITERAL POOL, READ IN ORDER
// ----------------------------------------------------------------------- //
//  Our pool run is 0x100a60d4..0x100a6138 and it is a transcript.  Every
//  word dumped from the image, with the code address that loads it (a
//  blank means icl emitted the literal and never loads it):
//
//      0x100a60d0   1.0        --            <- see (b)
//      0x100a60d4   1.0        0x10029a9f    <- `fdivrs`: 1.0f / fW
//      0x100a60d8  -1e-5       0x10029d14    plane 1 low epsilon
//      0x100a60dc   1.0        --
//      0x100a60e0  -1e-5       0x1002a048    plane 2
//      0x100a60e4   1.0        --
//      0x100a60e8  -1e-5       0x1002a37a    plane 3
//      0x100a60ec   1.0        --
//      0x100a60f0  -1e-5       0x1002a69a    plane 4
//      0x100a60f4   1.0        --
//      0x100a60f8  -1e-5       0x1002a9ca    plane 5
//      0x100a60fc   1.0        --
//      0x100a6100  -1e-5       0x1002acf4    plane 6
//      0x100a6104   1.0        --
//      0x100a6108  +1e-5       0x1002b0a0  } plane 6's sunk arm
//      0x100a610c   0.0        0x1002b0b5  }
//      0x100a6110  +1e-5       0x1002b070  } plane 5
//      0x100a6114   0.0        0x1002b087  }
//      0x100a6118  +1e-5       0x1002b040  } plane 4
//      0x100a611c   0.0        0x1002b057  }
//      0x100a6120  +1e-5       0x1002b010  } plane 3
//      0x100a6124   0.0        0x1002b027  }
//      0x100a6128  +1e-5       0x1002afe0  } plane 2
//      0x100a612c   0.0        0x1002aff7  }
//      0x100a6130  +1e-5       0x1002afb2  } plane 1
//      0x100a6134   0.0        0x1002afc7  }
//      0x100a6138   1.0        --            <- see (b)
//      0x100a613c 255.0        0x1002b0f6    <- the NEXT function's
//
//  Three things it settles:
//
//  (a) THE TRANSFORM IS WRITTEN BEFORE THE CLIPPER.  0x100a60d4 is the
//      function's FIRST literal and it is the `1.0f` of `1.0f / fW`.
//      Nothing else in 5.9 KB emits a float literal outside the clipper,
//      so there is no colour scale, no fog constant and no z bias
//      anywhere -- and the transform loop cannot have been written after
//      the six planes.
//
//  (b) THE CLIP RUN IS BIT-FOR-BIT big051a0.cpp's.  That function is
//      d3d.ren's SAME six-plane clipper standing alone, and its pool is
//
//        0x100a412c..0x100a4188  6 x (-1e-5, 1.0-unref) then
//                                6 x (+1e-5, 0.0) in REVERSE plane order
//        0x100a418c              a trailing unreferenced 1.0
//
//      which is our 0x100a60d8..0x100a6134 plus our trailing 0x100a6138,
//      shape for shape, including the seven unreferenced 1.0f and
//      including the reversal of the sunk arms.  0x100a60d0 is the SAME
//      trailing artefact belonging to the function before ours (whose own
//      six (+1e-5, 0.0) pairs sit at 0x100a60a0..0x100a60cf -- it is
//      0x1002803c, our near twin, see INSTRUMENT 4).  Two independently
//      compiled copies of one clip source produce one pool signature;
//      that is what this is.
//      The seven unreferenced 1.0f are NOT explained here and are not
//      explained in big051a0.cpp either -- see NOT PINNED (1).  What
//      matters is that they are the SAME artefact, so they are not an
//      extra statement our source is missing.
//
//  (c) SIX PLANES, in the order near, left, top, right, bottom, far --
//      the six -1e-5 loads are in ASCENDING pool order at ascending code
//      addresses, one per plane, and the six sunk arms consume the other
//      twelve.  There is no room in the run for a seventh.
//
// ----------------------------------------------------------------------- //
//  INSTRUMENT 4 -- SIBLING CORRESPONDENCE WITH 0x1002803c
// ----------------------------------------------------------------------- //
//  0x1002803c (stack entry 0x10028030; big26728.cpp already declares it
//  `sub_1002803c(Obj26728*, RVert**, int32*, Mat26728*)` and calls it at
//  0x100267ff) is this function's NEAR TWIN: the same geometry fetch, the
//  same transform loop, the same six-plane clipper.  Diffed statement by
//  statement, with the constants dumped:
//
//   * THE TRANSFORM LOOP IS THE SAME TWENTY-SIX INSTRUCTIONS.
//       ours   0x10029a80  mov (%eax),%eax
//              0x10029a87  flds 0x8(%eax) ; fmuls 0x38(%ebx)
//              ...         fadds 0x3c(%ebx) ; fdivrs 0x100a60d4
//       twin   0x100280d6  mov ...
//              0x100280e?  flds 0x8(%eax) ; fmuls 0x38(%edx)
//              0x100280f4  fadds 0x3c(%edx) ; fdivrs 0x100a6064
//     and 0x100a6064 dumps as 1.0f, the same constant as our 0x100a60d4.
//     The four rows are read in the same order (w, then x, then y with
//     the MATRIX loaded and the vertex multiplied, then z ending in
//     `fmulp`), the destination stride is 0x20 in both, and the SOURCE
//     stride is `add $0x18,%eax` in both (0x10029b57 `lea 0x18(%edx),%eax`
//     here, 0x10028161 `add $0x18,%eax` there).  That is what pins the
//     24-byte source record and the 4x4 row-major matrix independently of
//     anything in this function.
//   * WHERE THEY DIFFER, AND WHY.  The twin writes BOTH 0x10 and 0x14 of
//     the destination vertex as 0xffffffff (0x1002814f/0x10028152) and
//     touches neither 0x18 nor 0x1c.  Ours writes 0x10 as 0xffffffff
//     (0x10029b0e), fills 0x18/0x1c from the source record scaled by
//     g_a1008e664[0], and hands 0x14 to the fog function pointer.  Both
//     then do `g_1008be34 += nVerts - 2` with the identical
//     `lea -0x2(%eax,%edx,1)` (0x1002af46 / 0x10028186).
//   * THE CLIPPER IS THE SAME SIX 0xdc BLOCKS.  The twin's six flag-array
//     bases are -0x618, -0x53c, -0x460, -0x384, -0x2a8, -0x1cc -- 0xdc
//     apart, one `lea ...,%ecx` each, exactly our -0x608/-0x52c/-0x450/
//     -0x374/-0x298/-0x1bc shifted by 0x10.  Its six sunk epsilon arms
//     are at 0x10029790..0x10029885 and consume 0x100a60a0..0x100a60cf in
//     the same reversed order.  It is a third independent copy of
//     big051a0.cpp's clipper in this image and ours agrees with both.
//   * WHERE THE FRAMES DIFFER, THE DIFFERENCE IS READ OFF THEM, NOT
//     ASSUMED.  The twin's frame is `sub $0xc30,%esp` and its vertex
//     block runs -0xb18 .. -0x619, which is 0x500 = FORTY vertices of
//     0x20; ours runs -0x160c .. -0x60d, 0x1000 = ONE HUNDRED AND
//     TWENTY-EIGHT.  Two copies of one routine with different array
//     bounds, each bound falling out of its own `sub`.
//   * THE TWIN'S ROW IS ALSO MIS-ADDRESSED, twelve bytes this time:
//     an extent check on 0x1002803c finds the stack entry at 0x10028030
//     (three argument loads) and the row's end landing mid-instruction.
//     Its real extent is 0x10028030..0x10029890, 6,236 bytes, ONE `ret`
//     (0x1002978f), ONE prologue, nothing branching out.  Recorded here
//     as a bonus finding for whoever takes it next -- 5 calls in 6,236
//     bytes, 1,247 B/call, so it will need these same four instruments.
//
// ----------------------------------------------------------------------- //
//  WHAT THE FUNCTION IS -- five tells, none of them a guess
// ----------------------------------------------------------------------- //
//   1. THE CALL SITE.  0x10027db1, and nothing else in the image.
//      big26728.cpp has that block decoded:
//          g_pD3DDevice->SetRenderState(ZFUNC, D3DCMP_ALWAYS);
//          if (pSub->m_w42 & 0x8000) sub_100299a8(pObj, pXForm);
//          g_pD3DDevice->SetRenderState(ZFUNC, D3DCMP_LESSEQUAL);
//      i.e. we are called, depth-test defeated, immediately after the
//      reflected world has been drawn through the mirror -- and %eax is
//      the mirror record's object, %edx its 4x4 (`SRec26728::m_pXForm`).
//      Drawing the mirror's own surface on top is the only thing that
//      block can be.
//   2. ALPHA BLEND ON, THEN OFF, WITH NOTHING ELSE TOUCHED.  The two
//      indirect SetRenderState calls at 0x10029a2a and 0x1002afa0 are
//      D3DRENDERSTATE_ALPHABLENDENABLE (0x1b) = 1 and = 0, and they
//      bracket the whole body.  A glass/mirror overlay, not a shadow
//      (no stencil state) and not a lightmap pass (no ZFUNC/blend
//      factors, which is what big124a4.cpp's pass sets).
//   3. THE TEXTURE GATE.  `if (!sub_10015320(pObj->m_p28->m_p30,
//      g_1008d7d0, 0)) return;` -- 0x1008d7d0 is the current texture
//      stage (d3dqip3.cpp, big43e10.cpp, big6d910.cpp all name it) and
//      0x10015320 is d3dbatcha.cpp's texture bind, which returns the
//      bound object or LTNULL.  No texture, no mirror surface.
//   4. THE TEXTURE COORDINATES.  0x1008e664 and 0x1008e668 are element
//      ZERO of big6d910.cpp's per-stage `{ 1/width, 1/height }` pair, and
//      they are applied here exactly as big6d910.cpp records them,
//      including the operand order:
//          u = pWalk->m_fU * g_a1008e664[0].m_fInvW00;
//          v = g_a1008e664[0].m_fInvH04 * pWalk->m_fV;
//   5. THE VERTEX AND THE PRIMITIVE.  DrawPrimitive (vtable +0x64,
//      index 25) with type 6 = D3DPT_TRIANGLEFAN and FVF 0x1c4 =
//      XYZRHW|DIFFUSE|SPECULAR|TEX1, over a 0x20 stride whose
//      +0x00/04/08 are floats, +0x10..0x13 four bytes, +0x14 a dword and
//      +0x18/+0x1c floats -- big16324.cpp's RVert, and the +0x0c the
//      clipper copies but nobody here writes is the RHW that 0x10014580
//      fills in (big572b4.cpp: "+0x0c the reciprocal w").
//
// ----------------------------------------------------------------------- //
//  A WARNING ABOUT THE DUMPS
// ----------------------------------------------------------------------- //
//  objdump prints binutils' SWAPPED mnemonics for the x87 DE E0/E8/F0/F8
//  register forms.  Verified with `as` before believing either:
//      de f1  objdump "fdivp  %st,%st(1)"  = Intel FDIVRP: st1 = st0/st1
//      de ea  objdump "fsubrp %st,%st(2)"  = Intel FSUBP:  st2 = st2-st0
//  All six `de f1` divides in this function are therefore
//  numerator/denominator with the NUMERATOR in st0, and planes 3 and 4's
//  `de ea` is `pPrev->y - pPrev->z` / `pPrev->x - pPrev->z` and not the
//  reverse.  big051a0.cpp reaches the same reading of the same bytes.
//
// *********************************************************************** //



// ======================================================================= //
//  TYPES.  Every offset below is read off a shipped instruction and the
//  address is quoted.  Nothing is padded to a size that is not measured.
// ======================================================================= //

struct Vec299a8
{
	float           x;                                  // +0x00
	float           y;                                  // +0x04
	float           z;                                  // +0x08
};

union VColor299a8
{
	uint32          m_n;
	uint8           m_b[4];
};

// ----------------------------------------------------------------------- //
//  THE DRAW VERTEX.  0x20 bytes: the stride is `add $0x20,%esi` /
//  `add $0x20,%edx` / `shl $0x5` throughout, and the FVF handed to
//  DrawPrimitive at 0x1002af55 is 0x1c4 = XYZRHW|DIFFUSE|SPECULAR|TEX1,
//  which lays out exactly this way.  Fields, with a writer for each:
//    +0x00/04/08  fstps (%esi) / 0x4(%esi) / 0x8(%esi)   0x10029ad1..
//    +0x0c        NEVER written here -- only copied by `*pOut = *pPrev`
//                 (0x10029fff).  0x10014580 fills it (big572b4.cpp).
//    +0x10        mov %ecx,0x10(%esi) with %ecx = -1     0x10029b0e
//    +0x14        the fog function pointer's out-parameter (see the
//                 `lea -0x15f8(%ebp)` at 0x10029a69 -- &aVerts[0]+0x14)
//    +0x17        the FIFTH byte lerp of every clip plane 0x10029e8a
//    +0x18/+0x1c  fstps 0x18(%esi) / 0x1c(%esi)          0x10029b2e/31
//  The four diffuse bytes are lerped in the order 0x12, 0x11, 0x10,
//  0x13 (0x10029d6e, 0x10029db5, 0x10029dfc, 0x10029e43) -- b,g,r,a in
//  memory, r,g,b,a in source order, the same as big051a0.cpp's macro.
// ----------------------------------------------------------------------- //
struct RVert299a8                                       // 0x20 bytes
{
	Vec299a8        m_vPos;                             // +0x00
	float           m_fRHW;                             // +0x0c
	VColor299a8     m_Diffuse10;                        // +0x10
	VColor299a8     m_Specular14;                       // +0x14
	float           m_fU;                               // +0x18
	float           m_fV;                               // +0x1c
};

// ----------------------------------------------------------------------- //
//  THE SOURCE RECORD.  Stride 0x18, read off `lea 0x18(%edx),%eax` at
//  0x10029b57 -- and independently off `add $0x18,%eax` at 0x10028161 in
//  the twin 0x1002803c, which walks the same array from the same
//  0x1000a700 call.  +0x00 is a POINTER, not an embedded vector:
//  0x10029a80 is `mov (%eax),%eax` and the float loads that follow are
//  off the LOADED value.  +0x04 and +0x08 are the untransformed u and v
//  (0x10029b20/0x10029b23).  The remaining 12 bytes are touched by
//  nothing in this function; big572b4.cpp's copy of the same record is
//  equally silent about them.
// ----------------------------------------------------------------------- //
struct SrcRef299a8                                      // 0x18 bytes
{
	Vec299a8*       m_pPos;                             // +0x00
	float           m_fU;                               // +0x04
	float           m_fV;                               // +0x08
	uint8           m_pad_0c[0x0c];                     // +0x0c .. +0x17
};

// ----------------------------------------------------------------------- //
//  THE MATRIX.  arg1.  Row-major 4x4, and every one of the sixteen
//  elements is either loaded or would be: the w row is 0x30/0x34/0x38 +
//  0x3c (0x10029a8a..0x10029a9c), x is 0x00/0x04/0x08 + 0x0c
//  (0x10029ab5..0x10029ab9), y is 0x10/0x14/0x18 + 0x1c
//  (0x10029ad3..0x10029ae8) and z is 0x20/0x24/0x28 + 0x2c
//  (0x10029af4..0x10029b09).  big26728.cpp already calls it Mat26728 and
//  identifies it as src/LT2/sdk/inc/ltmatrix.h's LTMatrix.
// ----------------------------------------------------------------------- //
struct Mat299a8
{
	float           m[4][4];                            // +0x00 .. +0x3f
};

// ----------------------------------------------------------------------- //
//  THE OBJECT.  arg0.  Only two things are read out of it directly --
//  +0x28 and, through it, +0x30 -- and 0x1000a700 reads +0x4c/+0x50/
//  +0x52/+0x54 out of the same pointer.  Same shape big26728.cpp calls
//  Obj26728 / Sub26728; the +0x30 texture slot is new here and is read
//  off `mov 0x28(%esi),%eax ; mov 0x30(%eax),%eax` at 0x100299d5.
// ----------------------------------------------------------------------- //
struct Sub299a8
{
	uint8           m_pad_00[0x30];                     // +0x00 .. +0x2f
	void*           m_p30;                              // +0x30
};

struct Obj299a8
{
	uint8           m_pad_00[0x28];                     // +0x00 .. +0x27
	Sub299a8*       m_p28;                              // +0x28
};


// ======================================================================= //
//  THE DIRECT3D 7 DEVICE AT 0x1008d988 -- big124a4.cpp's / big572b4.cpp's
//  declaration.  Two slots are used: +0x50 SetRenderState (index 20) and
//  +0x64 DrawPrimitive (index 25).
// ======================================================================= //
// (The device, D3DDevice7 / g_pD3DDevice, and the render-state and
// primitive constants are this file's, declared at its head.)

#define D3DFVF_CLIPVERTEX                         0x1c4  // XYZRHW|DIFFUSE
                                                         // |SPECULAR|TEX1


// ======================================================================= //
//  GLOBALS.  Relocations are masked, so only the shapes matter.  Each is
//  named by the address these bytes touch and each name is one an earlier
//  decode already fixed.
// ======================================================================= //

// Round to nearest in the current FPU mode (w1_10001000.cpp's helper, whose
// out-of-line copy is 0x10015190): the shipped colour conversions are
// `fstps tmp ; flds tmp ; fistpl n`, the inlined __asm body.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}


// 0x1008d7d0 -- the current texture stage.  d3dqip3.cpp ("current
// stage"), big43e10.cpp and big6d910.cpp all name it uint32.  Read once,
// at 0x100299d8, and handed straight to 0x10015320.
extern uint32       g_n1008d7d0;

// 0x1008b874 -- the clip-plane mask.  `movl $0x3f,0x1008b874` at
// 0x10029b87 with the dead `mov $0x3f,%eax` at 0x10029b82 (the same pair
// big572b4.cpp records at 0x1005738a/91), then re-read at 0x10029bb8.
extern uint32       g_n1008b874;

// 0x1008b87c -- the clipper's output bank.  Read ONCE at 0x10029bdc and
// stored to BOTH pOut and pOutStart (0x10029be7 / 0x10029bed), exactly as
// big051a0.cpp records at 0x100051de.
extern RVert299a8*  g_p1008b87c;

// 0x1008b880 -- the transform object.  Taken by ADDRESS
// (`mov $0x1008b880,%edi` at 0x10029b9b and 0x1002af13), so it is an
// object, not a pointer.  big572b4.cpp / big124a4.cpp same.
// (declared above as this file's XForm26728; the calls take its address)

// 0x1008b908 / 0x1008b910 -- the near and far plane constants.  Each is
// used twice: as plane 1's / plane 6's classify comparand (0x10029c43,
// 0x1002ac15) and as the `fsubrs` numerator plus the integer store into
// pOut->m_vPos.z (0x10029d27/0x10029d49, 0x1002ad07/0x1002ad29).
extern float        g_f1008b908;
extern float        g_f1008b910;

// 0x1008be34 -- the frame's triangle counter.  `lea -0x2(%eax,%edx,1)`
// at 0x1002af46 is `+= nVerts - 2`, the triangle count of a fan.  The
// twin 0x1002803c does the identical thing at 0x10028186.
extern uint32       g_n1008be34;

// 0x1008be44 -- the clipped-polygon counter, `++` once per LIVE plane
// (six `mov ; inc ; mov` groups; big572b4.cpp records the same idiom
// where big124a4.cpp has `incl`).
extern uint32       g_n1008be44;

// 0x1008d7a0 -- UseD3DClip.  `mov 0x1008d7a0,%edx ; test %edx,%edx ; je`
// at 0x10029bc3, the only site.
extern uint32       g_n1008d7a0;

// 0x1008e664 -- element ZERO of big6d910.cpp's per-stage
// { 1/width, 1/height } pair; the two absolute addresses 0x1008e664 and
// 0x1008e668 are its two members.  Loaded at 0x10029b1a / 0x10029b26.
struct TexScale299a8
{
	float           m_fInvW00;                          // +0x00
	float           m_fInvH04;                          // +0x04
};

extern TexScale299a8 g_a1008e664[];

// 0x1008ce8c -- THE FOG FUNCTION POINTER.  big17c14.cpp, bige0e0.cpp and
// bigb910.cpp all declare it with this exact prototype: both arguments in
// registers, %ecx the position and %edx the output dword.  Our site is
//     10029b34  mov -0x1614(%ebp),%eax
//     10029b3a  mov (%eax),%ecx          <- pWalk->m_pPos
//     10029b3c  mov 0x1008ce8c,%eax
//     10029b41  call *%eax               <- %edx already = &pDst->m_14
// with no stack adjustment at all, which is what makes it __fastcall
// rather than a register-ABI /Qip callee.
typedef void (__fastcall* PFN1008ce8c)(Vec299a8* pPos, uint32* pOut);

extern PFN1008ce8c  g_pfn1008ce8c;


// ======================================================================= //
//  NEIGHBOURS.  Each signature is read off the call site in these bytes
//  and cross-checked against the decode named.
// ======================================================================= //

// 0x10015320 -- d3dbatcha.cpp's texture bind, "DECODED but the source
// shape is not".  Three dwords in the outgoing area at 0x100299e5/e1/ea;
// its own body reads 0x8(%ebp)/0xc(%ebp)/0x10(%ebp), so __cdecl with the
// arguments in that order.  big5a960.cpp declares it identically and
// tests the result against LTNULL, which is what 0x100299fa does.
// (int32, as w3_10014580_os.cpp defines it -- see the clipper
// note in the body below.)
extern "C" int32  sub_10015320(void* pTex, uint32 nStage, uint32 nFlag);

// 0x1000a700 -- d3dbatchd.cpp.  Dumped in full (36 bytes):
//     if (g_1008cb84) { *ppData = pObj->m_p4c; *pnCount = pObj->m_w52; }
//     else            { *ppData = pObj + 0x54; *pnCount = pObj->m_w50; }
// The count is a `movzwl` of a WORD, which is why the loop guards that
// read it here are `test ; je` (!= 0) rather than the `test ; jle` the
// twin 0x1002803c has for its int32* parameter.
extern "C" void   sub_1000a700(void* pGeom, void** ppOutData,
                               uint32* pnOutCount);

// 0x1000d9b0 -- d3dlow.cpp, __cdecl (vertex, transform object).  Its body
// is four instructions of argument shuffling and a tail call to
// 0x1000a770, a pure 3x4 matrix-vector multiply that writes x,y,z back in
// place and touches no global -- so it does NOT change g_n1008b874, and
// the re-read at 0x10029bb8 is icl declining to keep a global live across
// a call (big572b4.cpp records the same at 0x100573b4).
extern "C" void   sub_1000d9b0(RVert299a8* pVert, uint8* pXForm);

// 0x10014580 -- the screen projection through the transform object's
// +0x31c..+0x330, decoded in full at that address; d3dr2a.cpp,
// big124a4.cpp and big572b4.cpp all call it __cdecl (vertex, xform).
extern "C" void   sub_10014580(RVert299a8* pVert, uint8* pXForm);


// The clip flag array is fifty entries: 0xc8 = 200 bytes at each of the
// six 0xdc-apart bases (-0x608, -0x52c, -0x450, -0x374, -0x298, -0x1bc),
// four bytes an entry.  Same constant as big051a0.cpp, big124a4.cpp,
// big572b4.cpp and big6d910.cpp.
#define CLIP299A8_MAX   50


// ======================================================================= //
//  THE CLIPPER, as d3dr3b.cpp's driver 0x1000aa90 over the
//  six plane passes -- the shape big3c40.cpp's 0x10003c40 (the 32-byte
//  vertex clipper, near) is written in, and the shipped bytes of the two
//  agree instruction for instruction in the classify loops (`mov $1 ;
//  mov $0 ; jae ; mov` selects into the flag array, the six 0xdc-apart
//  flag arrays, the shared exit).  Here the driver is inlined into
//  0x100299a8's body.  Named by plane, not by address.
// ======================================================================= //
#define CLIP299A8_EPSILON   0.00001f

static inline void LerpClipVert299a8(RVert299a8* a, RVert299a8* b,
                                     RVert299a8* dst, float t)
{
	dst->m_fU = (b->m_fU - a->m_fU) * t + a->m_fU;
	dst->m_fV = (b->m_fV - a->m_fV) * t + a->m_fV;
	dst->m_Diffuse10.m_b[2]  = (uint8)RoundF((b->m_Diffuse10.m_b[2]  - a->m_Diffuse10.m_b[2])  * t + a->m_Diffuse10.m_b[2]);
	dst->m_Diffuse10.m_b[1]  = (uint8)RoundF((b->m_Diffuse10.m_b[1]  - a->m_Diffuse10.m_b[1])  * t + a->m_Diffuse10.m_b[1]);
	dst->m_Diffuse10.m_b[0]  = (uint8)RoundF((b->m_Diffuse10.m_b[0]  - a->m_Diffuse10.m_b[0])  * t + a->m_Diffuse10.m_b[0]);
	dst->m_Diffuse10.m_b[3]  = (uint8)RoundF((b->m_Diffuse10.m_b[3]  - a->m_Diffuse10.m_b[3])  * t + a->m_Diffuse10.m_b[3]);
	dst->m_Specular14.m_b[3] = (uint8)RoundF((b->m_Specular14.m_b[3] - a->m_Specular14.m_b[3]) * t + a->m_Specular14.m_b[3]);
}

static inline float ClipEdgeNear299a8(Vec299a8* a, Vec299a8* b, Vec299a8* dst)
{
	float fDenom = b->z - a->z;
	float t;

	if (fDenom < -CLIP299A8_EPSILON || fDenom > CLIP299A8_EPSILON)
		t = (g_f1008b908 - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	dst->y = (b->y - a->y) * t + a->y;
	dst->z = g_f1008b908;

	return t;
}

static inline float ClipEdgeLeft299a8(Vec299a8* a, Vec299a8* b, Vec299a8* dst)
{
	float fDenom = (b->x - a->x) + b->z - a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP299A8_EPSILON || fDenom > CLIP299A8_EPSILON)
		t = -(a->x + a->z) / fDenom;
	else
		t = 0.0f;

	dst->y = (b->y - a->y) * t + a->y;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->x = -fZ;

	return t;
}

static inline float ClipEdgeTop299a8(Vec299a8* a, Vec299a8* b, Vec299a8* dst)
{
	float fDenom = (b->y - a->y) - b->z + a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP299A8_EPSILON || fDenom > CLIP299A8_EPSILON)
		t = -(a->y - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->y = fZ;

	return t;
}

static inline float ClipEdgeRight299a8(Vec299a8* a, Vec299a8* b, Vec299a8* dst)
{
	float fDenom = (b->x - a->x) - b->z + a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP299A8_EPSILON || fDenom > CLIP299A8_EPSILON)
		t = -(a->x - a->z) / fDenom;
	else
		t = 0.0f;

	dst->y = (b->y - a->y) * t + a->y;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->x = fZ;

	return t;
}

static inline float ClipEdgeBottom299a8(Vec299a8* a, Vec299a8* b, Vec299a8* dst)
{
	float fDenom = (b->y - a->y) + b->z - a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP299A8_EPSILON || fDenom > CLIP299A8_EPSILON)
		t = -(a->y + a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->y = -fZ;

	return t;
}

static inline float ClipEdgeFar299a8(Vec299a8* a, Vec299a8* b, Vec299a8* dst)
{
	float fDenom = b->z - a->z;
	float t;

	if (fDenom < -CLIP299A8_EPSILON || fDenom > CLIP299A8_EPSILON)
		t = (g_f1008b910 - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	dst->y = (b->y - a->y) * t + a->y;
	dst->z = g_f1008b910;

	return t;
}

#define CLIP299A8_PASS(NAME, INSIDE, EDGE)                                   \
static inline LTBOOL NAME(RVert299a8** ppVerts, int32* pnVerts,              \
                          RVert299a8** ppOut)                                \
{                                                                            \
	uint32       anInside[CLIP299A8_MAX];                                    \
	RVert299a8*  pOutStart;                                                  \
	RVert299a8*  pV;                                                         \
	RVert299a8*  pEnd;                                                       \
	RVert299a8*  pPrev;                                                      \
	RVert299a8*  pCur;                                                       \
	uint32*      pFlag;                                                      \
	int32        nInside;                                                    \
	int32        i;                                                          \
	int32        iPrev;                                                      \
                                                                             \
	++g_n1008be44;                                                           \
                                                                             \
	nInside = 0;                                                             \
	pFlag   = anInside;                                                      \
	pV      = *ppVerts;                                                      \
	pEnd    = pV + *pnVerts;                                                 \
                                                                             \
	for (; pV != pEnd; ++pV)                                                 \
	{                                                                        \
		uint32 bIn = (INSIDE);                                               \
                                                                             \
		nInside += bIn;                                                      \
		*pFlag++ = bIn;                                                      \
	}                                                                        \
                                                                             \
	if (nInside == 0)                                                        \
		return LTFALSE;                                                      \
                                                                             \
	if (nInside != *pnVerts)                                                 \
	{                                                                        \
		pOutStart = *ppOut;                                                  \
		iPrev     = *pnVerts - 1;                                            \
		pPrev     = *ppVerts + iPrev;                                        \
                                                                             \
		for (i = 0; i < *pnVerts; ++i)                                       \
		{                                                                    \
			pCur = *ppVerts + i;                                             \
                                                                             \
			if (anInside[iPrev])                                             \
				*(*ppOut)++ = *pPrev;                                        \
                                                                             \
			if (anInside[iPrev] != anInside[i])                              \
			{                                                                \
				float t = EDGE(&pPrev->m_vPos, &pCur->m_vPos,                \
				               &(*ppOut)->m_vPos);                           \
                                                                             \
				LerpClipVert299a8(pPrev, pCur, *ppOut, t);                   \
                                                                             \
				++(*ppOut);                                                  \
			}                                                                \
                                                                             \
			iPrev = i;                                                       \
			pPrev = pCur;                                                    \
		}                                                                    \
                                                                             \
		*pnVerts = *ppOut - pOutStart;                                       \
		*ppVerts = pOutStart;                                                \
		*ppOut  += *pnVerts;                                                 \
	}                                                                        \
                                                                             \
	return LTTRUE;                                                           \
}

CLIP299A8_PASS(ClipNear299a8,   pV->m_vPos.z >= g_f1008b908,        ClipEdgeNear299a8)
CLIP299A8_PASS(ClipLeft299a8,   -pV->m_vPos.z < pV->m_vPos.x,       ClipEdgeLeft299a8)
CLIP299A8_PASS(ClipTop299a8,    pV->m_vPos.y < pV->m_vPos.z,        ClipEdgeTop299a8)
CLIP299A8_PASS(ClipRight299a8,  pV->m_vPos.x < pV->m_vPos.z,        ClipEdgeRight299a8)
CLIP299A8_PASS(ClipBottom299a8, -pV->m_vPos.z < pV->m_vPos.y,       ClipEdgeBottom299a8)
CLIP299A8_PASS(ClipFar299a8,    pV->m_vPos.z <= g_f1008b910,        ClipEdgeFar299a8)

static inline LTBOOL ClipAll299a8(uint32 nClip, RVert299a8** ppVerts,
                                  uint32* pnVerts)
{
	RVert299a8*  pIn;
	RVert299a8*  pOut;
	RVert299a8*  pOutStart;
	int32        nIn;

	if (g_n1008d7a0)
	{
		nClip &= 1;

		if (!nClip)
			return LTTRUE;
	}

	nIn  = *pnVerts;
	pIn  = *ppVerts;
	pOut = pOutStart = g_p1008b87c;

	if (nClip & 0x01)
		if (!ClipNear299a8(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x04)
		if (!ClipLeft299a8(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x08)
		if (!ClipTop299a8(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x10)
		if (!ClipRight299a8(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x20)
		if (!ClipBottom299a8(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x02)
		if (!ClipFar299a8(&pIn, &nIn, &pOut))
			return LTFALSE;

	*ppVerts = pIn;
	*pnVerts = nIn;
	return LTTRUE;
}


// ======================================================================= //
//  0x100299a0 .. 0x1002b0bf   (5,920 bytes, 9 calls)
//
//  Register ABI: %eax = pObj, %edx = pXForm.  The stack entry at
//  0x100299a0 loads them from 0x4(%esp)/0x8(%esp) and falls through into
//  the register entry at 0x100299a8, which spills them straight back with
//  `push %edx ; push %eax` and reloads them after _chkstk.  Written
//  __cdecl, and big26728.cpp declares it the same
//  way from the other side of the call.
//
//  Returns nothing: neither epilogue touches %eax and the caller at
//  0x10027db6 discards it.
// ======================================================================= //
// Named for its STACK entry, as this file's other /Qip pairs
// are (sub_1002dd60, sub_10026720): the 8-byte stub `sub_100299a0` places
// at 0x100299a0 (byte-identical) and the body `sub_100299a0$` is the
// 0x100299a8 row.  Under the old name the stub was credited to the first
// unmatched start with the same eight bytes (0x10069ed0, a THREE-argument
// stub), splitting that row.
void sub_100299a0(Obj299a8* pObj, Mat299a8* pXForm)
{
	// -- the frame, in the order the decomposition above names it ------ //
	RVert299a8    aVerts[128];
	SrcRef299a8*  pData;
	uint32        nVerts;
	RVert299a8*   pVerts;

	RVert299a8*   pWalk;
	SrcRef299a8*  pSrc;
	RVert299a8*   pDst;
	Vec299a8*     pPos;
	uint32        nLeft;
	float         fOOW;

	// ------------------------------------------------------------------ //
	//  REGION 1 -- 0x100299a0..0x100299fc.  PROLOGUE AND THE TEXTURE GATE.
	//
	//    100299d0  mov %eax,%esi                     ; pObj  -> %esi
	//    100299d2  add $0xfffffff0,%esp
	//    100299d5  mov 0x28(%esi),%eax
	//    100299d8  mov 0x1008d7d0,%ecx
	//    100299de  mov 0x30(%eax),%eax
	//    100299e1  mov %ecx,0x4(%esp)
	//    100299e5  mov %eax,(%esp)
	//    100299e8  mov %edx,%ebx                     ; pXForm -> %ebx
	//    100299ea  movl $0x0,0x8(%esp)
	//    100299f2  call 0x10015320
	//    100299f7  add $0x10,%esp
	//    100299fa  test %eax,%eax
	//    100299fc  jne 0x10029a0d
	//              <epilogue at 0x100299fe, ret at 0x10029a0c>
	//
	//  ARM ORDER.  The FAILURE arm (the epilogue) falls through and the
	//  body is jumped to, which is the known "error falls through,
	//  success is jumped over" row: the source is the guard
	//      if (!sub_10015320(...)) return;
	//  and NOT `if (sub_10015320(...)) { the whole body }`, which would
	//  emit the body inline and sink the `return`.  This particular
	//  coin-flip costs nothing in the call sequence, because the arm that
	//  moves (`return`) makes no calls -- the other spelling predicts the
	//  same nine calls in the same order.  It is recorded because the
	//  block placement differs and the reason should be stated.
	//
	//  The three dwords go into a SIXTEEN-byte area (`add $-0x10`), which
	//  is icl's 4-dword rounding, not a fourth argument: 0xc(%esp) is
	//  never written.
	// ------------------------------------------------------------------ //
	if (!sub_10015320(pObj->m_p28->m_p30, g_n1008d7d0, 0))
		return;

	// ------------------------------------------------------------------ //
	//  REGION 2 -- 0x10029a0d..0x10029a4b.  BLEND ON, THEN THE GEOMETRY.
	//
	//    10029a0d  add $0xfffffff4,%esp
	//    10029a10  mov 0x1008d988,%eax
	//    10029a15  movl $0x1,0x8(%esp)
	//    10029a1d  mov %eax,(%esp)
	//    10029a20  movl $0x1b,0x4(%esp)
	//    10029a28  mov (%eax),%eax
	//    10029a2a  call *0x50(%eax)             ; INDIRECT (1)
	//    10029a2d  add $0xfffffff0,%esp
	//    10029a30  lea -0x1610(%ebp),%eax       ; &nVerts
	//    10029a36  lea -0x1614(%ebp),%edx       ; &pData
	//    10029a3c  mov %eax,0x8(%esp)
	//    10029a40  mov %edx,0x4(%esp)
	//    10029a44  mov %esi,(%esp)              ; pObj
	//    10029a47  call 0x1000a700
	//
	//  0x1b is D3DRENDERSTATE_ALPHABLENDENABLE and the matching `= 0` is
	//  at 0x1002af83, the very last thing the function does.  The two
	//  bracket everything, including both `return` paths through the
	//  second epilogue -- but NOT the `return` at 0x100299fe, which is
	//  before the blend is enabled.  That asymmetry is what makes the
	//  texture gate an early return rather than a wrapped `if`.
	// ------------------------------------------------------------------ //
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
	                                     D3DRENDERSTATE_ALPHABLENDENABLE, 1);

	sub_1000a700(pObj, (void**)&pData, &nVerts);

	// ------------------------------------------------------------------ //
	//  REGION 3 -- 0x10029a4c..0x10029b6d.  THE TRANSFORM LOOP.
	//  0x122 bytes, 74 instructions, one indirect call.
	//
	//    10029a4c  mov -0x1610(%ebp),%eax       ; nVerts
	//    10029a55  test %eax,%eax
	//    10029a57  mov %eax,-0xc8(%ebp)         ; nLeft
	//    10029a5d  lea -0x160c(%ebp),%esi       ; pDst = aVerts
	//    10029a63  je 0x10029b6e                ; the `while (n--)` guard
	//    10029a69  lea -0x15f8(%ebp),%eax       ; &aVerts[0].m_Specular14
	//    10029a6f  mov %eax,-0xcc(%ebp)
	//    10029a75  mov -0x1614(%ebp),%eax       ; pSrc = pData
	//    10029a80  mov (%eax),%eax              ; pPos = pSrc->m_pPos
	//    10029a82  mov $0xffffffff,%edx
	//    10029a87  flds 0x8(%eax) ; fmuls 0x38(%ebx)     \
	//    10029a8d  flds 0x4(%eax) ; fmuls 0x34(%ebx)      |  the w row
	//    10029a93  faddp                                  |
	//    10029a95  flds (%eax)    ; fmuls 0x30(%ebx)      |
	//    10029a9a  faddp ; fadds 0x3c(%ebx)              /
	//    10029a9f  fdivrs 0x100a60d4                    ; fOOW = 1.0f / fW
	//    10029aa5  <the x row> ; fmul %st(1),%st ; fstps (%esi)
	//    10029ad3  <the y row> ; fmul %st(1),%st ; fstps 0x4(%esi)
	//    10029af4  <the z row> ; fmulp %st,%st(1) ; fstps 0x8(%esi)
	//    10029ac8  dec %ecx                             ; --nLeft
	//    10029b0e  mov %ecx,0x10(%esi)                  ; diffuse = -1
	//    10029b1a  flds 0x1008e668 ; fmuls 0x8(%eax)     \  u and v
	//    10029b23  flds 0x4(%eax)  ; fmuls 0x1008e664     |
	//    10029b2c  fxch ; fstps 0x1c(%esi) ; fstps 0x18(%esi)
	//    10029b34  mov -0x1614(%ebp),%eax
	//    10029b3a  mov (%eax),%ecx              ; pSrc->m_pPos AGAIN
	//    10029b3c  mov 0x1008ce8c,%eax
	//    10029b41  call *%eax                   ; INDIRECT (2)
	//    10029b4b  add $0x20,%eax ; add $0x20,%esi      ; the two pDst
	//    10029b57  lea 0x18(%edx),%eax                  ; pSrc++
	//    10029b66  test %edx,%edx ; jne 0x10029a80
	//
	//  WHAT PINS IT:
	//    * the divide is `fdivrs 0x100a60d4` and 0x100a60d4 dumps as
	//      1.0f, so `1.0f / fW` and not `fW / K`.  D8 /7 is FDIVR with a
	//      MEMORY operand -- the swapped-mnemonic hazard is only the
	//      DE register forms -- so st0 = mem / st0 = 1.0f / fW.
	//    * fOOW never touches memory: it lives in %st(1) across all three
	//      products (`fmul %st(1),%st` twice and `fmulp` the third time,
	//      which is what pops it).  A value kept in st across a
	//      whole loop is a register-allocated local.
	//    * THE W ROW IS COMPUTED FIRST.  It has to be: the reciprocal is
	//      the operand the other three multiply by.  So the source cannot
	//      have the divide after the x/y/z statements.
	//    * 0x10(%esi) takes %ecx, and %ecx is `mov %edx,%ecx` at
	//      0x10029aed with %edx still the `mov $0xffffffff,%edx` of
	//      0x10029a82 -- one materialised constant, one store, four
	//      bytes.  It is the whole DIFFUSE dword, not one byte.
	//    * pSrc->m_pPos is loaded TWICE (0x10029a80 and 0x10029b3a).  The
	//      second load is not a scheduling artefact -- everything between
	//      them stores through pDst, so icl must assume the aliasing.
	//      That is why the fog call's argument is written as
	//      `pSrc->m_pPos` and the transform's as a local `pPos`: one
	//      local for both would emit ONE load, and three locals none.
	//    * the SECOND destination induction pointer, -0xcc, starts at
	//      `lea -0x15f8(%ebp)` = &aVerts[0] + 0x14 and steps 0x20.  It is
	//      never stored through in this loop; it exists only to be the
	//      fog call's %edx.  Known pattern: "a pointer walk biased to the
	//      middle of the element is still an indexed source loop" -- one
	//      `pDst` in source, two induction registers in codegen.
	//    * `test ; je` over `dec ; test ; jne`, on a count that came from
	//      a `movzwl`: `while (nLeft--)`, the same shape big572b4.cpp
	//      records for the same three loops.  A `for (i = 0; i < nVerts;
	//      i++)` on a SIGNED count would guard with `jle`; the twin
	//      0x1002803c, whose count arrives through an `int32*`, does
	//      exactly that at 0x100280bb.
	//
	//  COIN-FLIP: the u and v statements.  The bytes evaluate v's product
	//  first and store 0x1c before 0x18 (the `fxch` at 0x10029b2c), so
	//  the store order is v,u while the operand order inside each product
	//  is `src * scale` for u and `scale * src` for v.  The operand
	//  orders are source-visible and are written as they ship (they are
	//  also exactly what big6d910.cpp records for the same two globals);
	//  the STATEMENT order is not -- icl reassociates freely without /Op
	//  and there are no literals in the pool to order them.  Written u
	//  then v, as big6d910.cpp has it.  The other spelling predicts the
	//  identical call sequence.
	// ------------------------------------------------------------------ //
	pSrc  = pData;
	pDst  = aVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		pPos = pSrc->m_pPos;

		fOOW = 1.0f / (pXForm->m[3][0] * pPos->x
		             + pXForm->m[3][1] * pPos->y
		             + pXForm->m[3][2] * pPos->z
		             + pXForm->m[3][3]);

		pDst->m_vPos.x = (pXForm->m[0][0] * pPos->x
		                + pXForm->m[0][1] * pPos->y
		                + pXForm->m[0][2] * pPos->z
		                + pXForm->m[0][3]) * fOOW;
		pDst->m_vPos.y = (pXForm->m[1][0] * pPos->x
		                + pXForm->m[1][1] * pPos->y
		                + pXForm->m[1][2] * pPos->z
		                + pXForm->m[1][3]) * fOOW;
		pDst->m_vPos.z = (pXForm->m[2][0] * pPos->x
		                + pXForm->m[2][1] * pPos->y
		                + pXForm->m[2][2] * pPos->z
		                + pXForm->m[2][3]) * fOOW;

		pDst->m_Diffuse10.m_n = 0xffffffff;

		pDst->m_fU = pSrc->m_fU * g_a1008e664[0].m_fInvW00;
		pDst->m_fV = g_a1008e664[0].m_fInvH04 * pSrc->m_fV;

		(*g_pfn1008ce8c)(pSrc->m_pPos, &pDst->m_Specular14.m_n);

		pSrc++;
		pDst++;
	}

	// ------------------------------------------------------------------ //
	//  REGION 4 -- 0x10029b6e..0x10029bdb.  THE CAMERA TRANSFORM, THE
	//  CLIP MASK, AND THE UseD3DClip SHORTCUT.
	//
	//    10029b6e  lea -0x160c(%ebp),%eax
	//    10029b74  mov -0x1610(%ebp),%edx            ; nVerts
	//    10029b7a  mov %eax,-0x60c(%ebp)             ; pVerts = aVerts
	//    10029b80  test %edx,%edx
	//    10029b82  mov $0x3f,%eax                    ; the DEAD copy
	//    10029b87  movl $0x3f,0x1008b874
	//    10029b91  lea -0x160c(%ebp),%ebx            ; pWalk
	//    10029b97  je 0x10029bbd
	//    10029ba0  add $-8,%esp ; mov %edi,0x4(%esp) ; mov %ebx,(%esp)
	//    10029baa  call 0x1000d9b0
	//    10029baf  add $0x20,%ebx ; add $8,%esp ; dec %esi ; jne
	//    10029bb8  mov 0x1008b874,%eax               ; re-read
	//    10029bbd  mov %eax,-0xc0(%ebp)              ; nClipFlags
	//    10029bc3  mov 0x1008d7a0,%edx
	//    10029bc9  test %edx,%edx ; je 0x10029bdc
	//    10029bcd  and $0x1,%eax
	//    10029bd0  mov %eax,-0xc0(%ebp)
	//    10029bd6  je 0x1002af01                     ; -> REGION 11
	//
	//  Three things to read off it:
	//    * `mov $0x3f,%eax` at 0x10029b82 is DEAD unless the loop is
	//      skipped, in which case it supplies the value the re-read at
	//      0x10029bb8 would have produced.  So `nClipFlags =
	//      g_n1008b874;` really is a re-read of the global AFTER the
	//      loop, and icl proved the global unchanged on the empty path
	//      only.  big572b4.cpp records the identical pair.
	//    * TWO stores to -0xc0 (0x10029bbd and 0x10029bd0) -- the
	//      assignment and then the `&= 1`.  big051a0.cpp calls out the
	//      same two stores at 0x100051b4/cc.
	//    * `mov $0x1008b880,%edi` hoisted out of the loop: the transform
	//      object is taken by ADDRESS.
	//
	//  The `je 0x1002af01` is a jump into REGION 11 -- the projection and
	//  the draw -- SKIPPING the whole clipper and skipping the
	//  `pVerts = pIn ; nVerts = nIn` write-back with it, which is why
	//  pIn/nIn are separate variables from pVerts/nVerts here.  In
	//  big051a0.cpp, where the clipper is its own function, that arm is
	//  `return LTTRUE`.
	// ------------------------------------------------------------------ //
	pVerts = aVerts;

	g_n1008b874 = 0x3f;

	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		sub_1000d9b0(pWalk, (uint8*)&g_1008b880);
		pWalk++;
	}

	// ------------------------------------------------------------------ //
	//  REGIONS 4 (from 0x10029bb8) .. 10 -- THE CLIPPER.
	//  The six hand-inlined plane blocks that stood here (big299a8.cpp's
	//  REGIONS 5..10, kept in that file's history) are the driver
	//  ClipAll299a8 over its six passes, defined above and inlined: the
	//  shipped classify loops are the select-shaped ones big3c40.cpp's
	//  0x10003c40 compiles to, and the body comes out 5,896 bytes
	//  (shipped 5,944; the hand-inlined blocks gave 5,672 to 5,767) with
	//  the epsilon arms sunk last, as shipped.
	//  0x10015320's result is the int32 its definition
	//  (w3_10014580_os.cpp) returns: as a void* icl predicted it non-null
	//  and sank the early `return`, which the shipped code keeps in line.
	// ------------------------------------------------------------------ //
	if (!ClipAll299a8(g_n1008b874, &pVerts, &nVerts))
		goto L_Done;

	// ------------------------------------------------------------------ //
	//  REGION 11 -- 0x1002af01..0x1002afb1.  PROJECT, DRAW, BLEND OFF.
	//
	//    1002af01  mov -0x60c(%ebp),%ebx    ; pWalk = pVerts
	//    1002af07  mov -0x1610(%ebp),%eax   ; nLeft = nVerts
	//    1002af0d  test %eax,%eax ; je 0x1002af38
	//    1002af13  mov $0x1008b880,%edi
	//    1002af20  add $-8,%esp ; two movs ; call 0x10014580
	//    1002af2f  add $0x20,%ebx ; add $8,%esp ; dec %esi ; jne
	//    1002af38  mov 0x1008be34,%eax
	//    1002af3d  mov -0x1610(%ebp),%edx
	//    1002af43  add $0xffffffe8,%esp     ; 24 bytes: SIX dwords
	//    1002af46  lea -0x2(%eax,%edx,1),%eax
	//    1002af4a  mov 0x1008d988,%ecx
	//    1002af50  mov %eax,0x1008be34
	//    1002af55  movl $0x1c4,0x8(%esp)    ; FVF
	//    1002af5d  mov %ecx,(%esp)          ; this
	//    1002af60  movl $0x6,0x4(%esp)      ; D3DPT_TRIANGLEFAN
	//    1002af68  mov -0x60c(%ebp),%eax
	//    1002af6e  movl $0x0,0x14(%esp)     ; flags
	//    1002af76  mov %edx,0x10(%esp)      ; count
	//    1002af7a  mov %eax,0xc(%esp)       ; vertices
	//    1002af7e  mov (%ecx),%ecx
	//    1002af80  call *0x64(%ecx)         ; INDIRECT (3)
	//    1002af83  add $0xfffffff4,%esp     <- L_Done; three dwords
	//    1002af86  mov 0x1008d988,%eax
	//    1002af8b  movl $0x0,0x8(%esp)
	//    1002af93  mov %eax,(%esp)
	//    1002af96  movl $0x1b,0x4(%esp)
	//    1002af9e  mov (%eax),%eax
	//    1002afa0  call *0x50(%eax)         ; INDIRECT (4)
	//    1002afa3  <epilogue>  ret at 0x1002afb1
	//
	//  The DrawPrimitive area is SIX dwords (`add $-0x18`), which is the
	//  six arguments of the +0x64 slot: this, type, FVF, vertices, count,
	//  flags -- and all six are written, so the prototype is not a guess.
	//  It is never closed with an `add`: the epilogue's `mov %ebp,%esp`
	//  swallows both it and the following three-dword area.
	// ------------------------------------------------------------------ //
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		sub_10014580(pWalk, (uint8*)&g_1008b880);
		pWalk++;
	}

	g_n1008be34 += nVerts - 2;

	g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, D3DPT_TRIANGLEFAN,
	                                    D3DFVF_CLIPVERTEX, pVerts, nVerts, 0);

	// ------------------------------------------------------------------ //
	//  L_Done -- 0x1002af83.  Every one of the twelve clip rejects (two
	//  per plane: the rotated classify loop's `cmp %eax,%edi ; je` and
	//  the `test %ebx,%ebx ; je` of `nInside == 0`) branches straight
	//  here, PAST the projection and the draw but not past the blend
	//  reset.  In big051a0.cpp, where the clipper is its own function,
	//  the same twelve sites are `return LTFALSE`.
	// ------------------------------------------------------------------ //
L_Done:
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
	                                     D3DRENDERSTATE_ALPHABLENDENABLE, 0);
}


// *********************************************************************** //
//  REGION 12 -- 0x1002afb2..0x1002b0bf.  THE SIX SUNK EPSILON ARMS.
//
//  No source of its own: these are the `else fT = 0.0f;` arms of the six
//  `if (fDen < -1e-5f || fDen > 1e-5f)` above, which /O2 sinks below the
//  last hot block.  Each is
//
//      flds <+1e-5> ; fcomp %st(k) ; fnstsw ; sahf
//      jb   <back into the divide>            ; fDen > +1e-5 after all
//      fstp %st(0) x2 or x3                   ; drop the operands
//      flds <0.0f>
//      jmp  <back into the lerp>
//
//  and every one of them jumps BACKWARDS into the body, which is why the
//  270 bytes are not a separate function:
//
//      0x1002afb2 -> 0x10029d25 / 0x10029d2f    plane 1   2 fstp
//      0x1002afe0 -> 0x1002a05c / 0x1002a064    plane 2   3 fstp
//      0x1002b010 -> 0x1002a38e / 0x1002a396    plane 3   3 fstp
//      0x1002b040 -> 0x1002a6ae / 0x1002a6b6    plane 4   3 fstp
//      0x1002b070 -> 0x1002a9de / 0x1002a9e6    plane 5   3 fstp
//      0x1002b0a0 -> 0x1002ad05 / 0x1002ad0f    plane 6   2 fstp
//
//  2+3+3+3+3+2 = 16 is the `fstp` row of the census, and the six `jmp`
//  are the ONLY six jmps in the function.  Planes 1 and 6 need one fewer
//  because their numerators are `fsubrs` against a global and so leave
//  one fewer value on the stack than planes 2..5's fchs/faddp pairs.
//  The seven-byte `lea 0x0(%esi,%eiz,1),%esi` / `lea 0x0(%edi,%eiz,1),%edi`
//  pairs at 0x1002afd2, 0x1002b002, 0x1002b032, 0x1002b062 and 0x1002b092
//  are icl aligning each arm to 0x10; they are 10 of the 23 lea-nop pads
//  in the census.
//
// *********************************************************************** //


// *********************************************************************** //
//  THE OTHER FUNCTION IN THE ROW -- reported, not decoded
// *********************************************************************** //
//  0x1002b0c0, 24 bytes (0x1002b0c0..0x1002b0d7), 2 calls
//  (2 direct, 0 indirect), 12 bytes per call.  Frameless __cdecl, no
//  arguments, returns nothing.  Not reachable by any branch or call in
//  .text; its address is stored into 0x1008ef80 at 0x1002d670, and
//  big26728.cpp records `g_1008ef80 = 0` at the head of 0x10026728, so
//  the slot is the "draw the mirrors" hook and this is what fills it.
//  Its whole body is the register-ABI call big26728.cpp already quotes as
//  0x10026728's one caller:
//
//      void sub_1002b0c0(void)
//      {
//          sub_10026728(g_p1008beb0, sub_1007b8d0());
//      }
//
//  -- %eax = *0x1008beb0 (the scene sink) and %edx = sub_1007b8d0() (the
//  0x10091a10 singleton).  Left as a candidate rather than written out
//  here, because it belongs with whatever TU owns 0x1002d670.
//
//  0x1002b0d8..0x1002b0df is 8 bytes of alignment padding, and
//  0x1002b0e0 is the next enumerated row (208 bytes) -- d3dr3d.cpp's
//  fog-mode installer, the function that WRITES g_pfn1008ce8c with one of
//  0x100234d0 / 0x100235d0 / 0x100234e0.
//
// *********************************************************************** //


// *********************************************************************** //
//  THE FOUR INDIRECT CALLS, AND WHAT PINS EACH TARGET
// *********************************************************************** //
//  The call-sequence comparison matches indirect calls BY POSITION ONLY,
//  so it confirms that four of them happen and where, and nothing about what they reach.
//  Each is pinned by something else:
//
//  (1) 0x10029a2a  `mov 0x1008d988,%eax ; mov (%eax),%eax ;
//                   call *0x50(%eax)`
//      A VTABLE SLOT AT A KNOWN OFFSET on a named object.  0x1008d988 is
//      g_pD3DDevice; big124a4.cpp, big572b4.cpp and big26728.cpp all fix
//      +0x50 as IDirect3DDevice7::SetRenderState (index 20) and +0x64 as
//      DrawPrimitive (index 25).  The three dwords pushed are (this,
//      0x1b, 1) and 0x1b is D3DRENDERSTATE_ALPHABLENDENABLE, whose only
//      legal values are 0 and 1 -- which is what the pair 0x10029a2a /
//      0x1002afa0 passes.  Seventeen calls through the same +0x50 slot in
//      0x10026728 carry the D3DRENDERSTATE numbers 0x34/0x37/0x38/0x39/
//      0x07/0x0e/0x17, i.e. the slot is definitely SetRenderState.
//
//  (2) 0x10029b41  `mov 0x1008ce8c,%eax ; call *%eax`
//      A FUNCTION POINTER STORED BY A NAMED INSTALLER.  0x1008ce8c is
//      written in exactly three places in the whole image, all inside
//      0x1002b0e0 -- the function 32 bytes after ours --
//          1002b13b  movl $0x100234d0,0x1008ce8c
//          1002b179  movl $0x100235d0,0x1008ce8c
//          1002b198  movl $0x100234e0,0x1008ce8c
//      and d3dr3d.cpp already records that installer ("0x1008ce88 and
//      0x1008ce8c always receive one of 0x10023570 / 0x100234d0 ...").
//      All three candidates take the position in %ecx and the output
//      dword in %edx: 0x100234e0 opens `flds 0x8(%ecx) ;
//      fsubs 0x1008bd6c ; flds 0x4(%ecx) ; fsubs 0x1008bd68 ; ...` --
//      a distance from the camera origin at 0x1008bd64/68/6c, i.e. FOG.
//      big17c14.cpp, bige0e0.cpp and bigb910.cpp independently declare
//      the slot `void (__fastcall*)(LTVector*, uint32*)`, and bige0e0.cpp
//      names its destination "0x1008adb0 filled by g_pfn1008ce8c".  Ours
//      writes into &aVerts[i].m_Specular14, which under FVF 0x1c4 is the
//      D3D7 fog dword.  No stack adjustment at the call site confirms
//      both arguments are in registers.
//
//  (3) 0x1002af80  `mov 0x1008d988,%ecx ; mov (%ecx),%ecx ;
//                   call *0x64(%ecx)`
//      Same vtable, slot +0x64 = DrawPrimitive.  Pinned twice over: the
//      SIX dwords in the outgoing area match the slot's six parameters
//      exactly, and the literals 6 (D3DPT_TRIANGLEFAN) and 0x1c4
//      (XYZRHW|DIFFUSE|SPECULAR|TEX1) are only meaningful as
//      DrawPrimitive's first two.  0x1c4 also has to be the FVF of the
//      0x20-byte vertex the whole function has been building, and it is.
//
//  (4) 0x1002afa0  `call *0x50(%eax)` -- SetRenderState again, the
//      matching ALPHABLENDENABLE = 0.  Same pin as (1).
//
//  BLOCK PLACEMENT.  None of the four sits in a sunk cold block, so
//  the "equal counts on both sides, all indirect" pattern should
//  NOT appear here.  If it does, the arm order to try first is the
//  texture gate (see REGION 1).
//
// *********************************************************************** //
//  THERE ARE NO `// UNDECODED:` GAPS, AND HERE IS WHY THAT IS A CLAIM
//  AND NOT AN OMISSION
// *********************************************************************** //
//  Every one of the 1,542 instructions in 0x100299a0..0x1002b0bf is in
//  the census above and every census row is accounted for by the source
//  below.  The one place where a gap could hide is the interior of clip
//  planes 2..6, which were read at the gate, the classify, the epsilon
//  pair, the denominator, the numerator and the position stores but not
//  instruction by instruction through the thirty byte lerps.  That
//  interior is closed mechanically instead:
//
//    grep movzbl:  0x10(%esi) x6  0x10(%ebx) x6
//                  0x11(%esi) x6  0x11(%ebx) x6
//                  0x12(%esi) x6  0x12(%ebx) x6
//                  0x13(%esi) x6  0x13(%ebx) x6
//                  0x17(%esi) x6  0x17(%ebx) x6      = 60, nothing else
//    grep the stores: `mov %dl,0x10(%eax)` x6, 0x11 x6, 0x12 x6,
//                  0x13 x6, 0x17 x6                  = 30, nothing else
//    and the thirty sites appear at ASCENDING addresses in the SAME
//    order in every plane:
//        plane 1  0x10029d6e 12, 0x10029db5 11, 0x10029dfc 10,
//                 0x10029e43 13, 0x10029e8a 17
//        plane 2  0x1002a0a6 / a0ed / a134 / a17b / a1c2
//        plane 3  0x1002a3cf / a416 / a45d / a4a4 / a4eb
//        plane 4  0x1002a6f2 / a739 / a780 / a7c7 / a80e
//        plane 5  0x1002aa25 / aa6c / aab3 / aafa / ab41
//        plane 6  0x1002ad4e / ad95 / addc / ae23 / ae6a
//    -- 0x12, 0x11, 0x10, 0x13, 0x17, i.e. red, green, blue, alpha,
//    specular-alpha, six times, with the sites 0x47 bytes apart every
//    time.  There is no room in those counts for a sixth byte, a
//    reordering, or a plane that does something different, and the
//    twelve `fstps 0x18(%eax)` / `fstps 0x1c(%eax)` close the texture
//    pair the same way.
//
// *********************************************************************** //
//  WHAT COULD NOT BE PINNED
// *********************************************************************** //
//  (1) THE SEVEN UNREFERENCED 1.0f IN THE LITERAL POOL.  0x100a60dc,
//      0x100a60e4, 0x100a60ec, 0x100a60f4, 0x100a60fc, 0x100a6104 (one
//      per clip plane, each immediately after that plane's -1e-5) and
//      0x100a6138 (one trailing).  icl emitted them and never loads
//      them, so the clip source contains a `1.0f` per plane that folds
//      away entirely.  Which statement it is is unknown.  What CAN be
//      said is that it is not a statement THIS reconstruction is missing:
//      big051a0.cpp's independently compiled copy of the same clipper has
//      the same seven at 0x100a4130/38/40/48/50/58 and 0x100a418c, in the
//      same positions, and its decode does not explain them either.  A
//      build of this file will emit its own pool; comparing that pool
//      against 0x100a60d4..0x100a6138 is the cheapest test of the clip
//      source's shape that exists, and it should be run.
//
//  (2) 0x1000a700's SOURCE RECORD BEYOND +0x08.  The stride is 0x18 and
//      only +0x00, +0x04 and +0x08 are read here.  Declared as 12 bytes
//      of padding rather than invented.
//
//  (3) THE OBJECT BEYOND +0x28 / +0x30.  Our bytes read `pObj->m_p28`
//      and `pObj->m_p28->m_p30` and hand pObj itself to 0x1000a700,
//      which reads +0x4c/+0x50/+0x52/+0x54 out of it.  Those four are
//      d3dbatchd.cpp's, not ours, and are not modelled.
//
//  (4) WHY 0x10015320's SECOND ARGUMENT IS g_n1008d7d0 HERE.  big5a960.cpp
//      calls the same function with a literal 0 in that position; the
//      callee compares it as a BYTE (`movzbl %dl`) against a +0x42 word
//      and uses it to index g_a1008e684[]. "Current texture stage" is
//      d3dqip3.cpp's name for 0x1008d7d0 and it is consistent with both
//      call sites, but 0x10015320 is not decoded far enough to prove
//      the parameter's role, and it is not needed for anything here.
//
// *********************************************************************** //
//  THE REMAINING GENUINE COIN-FLIPS, both spellings given
// *********************************************************************** //
//  (a) THE TEXTURE GATE'S ARM ORDER (REGION 1).  Written
//          if (!sub_10015320(pObj->m_p28->m_p30, g_n1008d7d0, 0))
//              return;
//      which is the spelling that emits the `return` inline and jumps to
//      the body -- what 0x100299fc's `jne` does.  The alternative is
//          if (sub_10015320(pObj->m_p28->m_p30, g_n1008d7d0, 0))
//          {
//              ... the whole 5.9 KB ...
//          }
//      which sinks the `return`.  Both predict the SAME nine calls in
//      the SAME order (the arm that moves makes none), so the call sequence cannot
//      settle it; only the block layout differs.
//
//  (b) THE u/v STATEMENT ORDER (REGION 3).  Written
//          pDst->m_fU = pSrc->m_fU * g_a1008e664[0].m_fInvW00;
//          pDst->m_fV = g_a1008e664[0].m_fInvH04 * pSrc->m_fV;
//      The bytes store 0x1c before 0x18.  Swapping the two statements is
//      the other spelling; the operand order INSIDE each product is
//      source-visible and is not a coin-flip (it is written as it
//      ships, and matches big6d910.cpp).  The pool has no literal for
//      either, so nothing orders them.
//
//  (c) PLANE 3's AND PLANE 4's TWO STORES OF fZ.  Plane 3 emits
//      `fsts 0x4(%ecx) ; fstps 0x8(%ecx)` -- y then z -- and plane 4
//      `fsts (%ecx) ; fstps 0x8(%ecx)` -- x then z.  Written in that
//      order.  The reverse spelling (`pOut->m_vPos.z = fZ;` first) would
//      put the `fsts` on 0x8; one build settles it and no call moves.
//
//  (d) pIn/nIn AS A SECOND VARIABLE PAIR.  Written as four variables
//      with an explicit `pVerts = pIn ; nVerts = nIn;` at 0x1002aeef,
//      because the `goto L_Draw` of REGION 4 reaches the draw without
//      passing through it.  big572b4.cpp's sibling uses ONE pair and
//      calls the same two stores compiler bookkeeping; that spelling is
//      also possible here (icl would have to materialise the same
//      copies) and predicts the same call sequence.
//
// *********************************************************************** //



// *********************************************************************** //
//
//  (was MODULE BIG_2D1C0 (.CPP); merged into d3dqip2.cpp)
//
//  PURPOSE:  One function -- 0x1002d1c0, 2,288 bytes.  d3d.ren's
//            **RenderScene(SceneDesc*)**, RenderStruct slot +0xb0: the
//            per-frame top-level render entry the client calls once per
//            camera.  It validates the device state, optionally swaps the
//            two render-target surfaces, refreshes four cached tables
//            (light-saturation ramp, two colour ramps, the warble wave
//            table), sets up the frame through 0x1001eb90, draws the
//            world, prints eighteen debug statistics lines, and advances
//            the warble clock.
//
//  ------------------------------------------------------------------- //
//  THE EXTENT IS CLEAN AND CONFIRMED.
//  ------------------------------------------------------------------- //
//
//      extent check of 0x1002d1c0, 2288 bytes:
//        0x1002d1c0..0x1002dab0  2,288 bytes  582 instructions
//        ret               3   0x1002da04, 0x1002da2f, 0x1002da40
//        push %ebp frames  1   0x1002d1c0
//        branch targets    0 outside the range or mid-instruction
//        last instruction  ends exactly at the stated end
//
//  ONE prologue, no stray branch target, no /Qip register entry, and the
//  body ends exactly on the row's end.  The THREE `ret`s are the /O2
//  duplicated epilogue: all three are `xor %eax,%eax` followed by the
//  identical `mov (%esp),%ebx ; mov 0x4(%esp),%esi ; mov 0x8(%esp),%edi ;
//  mov %ebp,%esp ; pop %ebp ; ret` restoring the three registers the
//  prologue saved esp-relative at 0x1002d1cd..0x1002d1d5.  They are three
//  separate `return LTFALSE;` statements -- "several return 0
//  epilogues mean several separate if (...) return X; statements" -- and
//  the source below has exactly three.
//
//  Also confirmed: 0x1002dab0 opens the NEXT function (the console-variable
//  initialiser that registers "VFog" / "DrawWorldTree" / ... and whose
//  records this function reads), so the row's end is a real boundary.
//
//  ------------------------------------------------------------------- //
//  THE SIGNATURE, AND WHERE IT IS READ FROM
//  ------------------------------------------------------------------- //
//
//      LTBOOL d3d_RenderScene(SceneDesc* pScene);   /* __cdecl */
//
//  * ONE stack argument: `mov 0x8(%ebp),%edi` at 0x1002d1d8 is the only
//    parameter read, and every `ret` is a bare `c3` (caller cleans).
//  * The identity comes from THREE places already in this repo, none of
//    them a guess:
//      - src/engine/d3d/d3d.cpp's RenderStruct map: "+0xb0  0x1002d1c0
//        RenderScene(SceneDesc*)".
//      - src/engine/lithtech/clientrender.cpp's copy of the same map.
//      - src/engine/lithtech/clientblit.cpp, which decoded the CALLER
//        (lithtech.exe 0x00411700, `RenderCamera`) and lists the
//        ~0x160-byte stack struct it hands to slot +0xb0 offset by
//        offset.  The two fields this function reads off the argument are
//        in that map: +0x0c0 is `LTVector pCamera+0x18c (m_Pos)` -- read
//        here as 0xc0(%edi) and 0xc8(%edi), i.e. m_Pos.x and m_Pos.z --
//        and +0x068 is `pClientMgr+0x1258`, read here as a float.
//  * The return value is LTFALSE on every path: all three epilogues zero
//    %eax explicitly.  clientblit.cpp records "Answer ignored" at the call
//    site, which is why nothing ever returns anything else.
//
//  ------------------------------------------------------------------- //
//  THE ACCEPTANCE FIGURE.  THIS FUNCTION IS THE DENSEST THING LEFT.
//  ------------------------------------------------------------------- //
//
//      call sequence over 0x1002d1c0..0x1002dab0:
//        2,288 bytes, 39 calls (17 direct, 22 indirect), 59 bytes per call
//
//  59 B/call, and under about 100 B/call the test is
//  strong.  14 DISTINCT direct callees over 17 direct calls, so the
//  discrimination is real too -- this is not a table handed to one
//  registrar.
//
//  CALL-FREE COVERAGE, measured rather than estimated (gaps taken from
//  the end of one call instruction to the start of the next):
//
//    * SEVENTEEN windows of 32 bytes or more, 1,671 bytes = **73.0%**.
//      That number is the honest headline and it is not small.
//    * But only FOUR windows reach 100 bytes, totalling 1,111 = 48.6%:
//        0x1002d1cd..0x1002d25b   142  the six entry guards
//        0x1002d2ea..0x1002d3ba   208  the env pan and light direction
//        0x1002d3cf..0x1002d664   661  the epsilon test, the nine ramp
//                                      loops, the cache copy, the
//                                      counter resets and the fill mode
//        0x1002da4c..0x1002dab0   100  the two sunk epsilon terms
//      The other thirteen are 33 to 60 bytes each and every one of them
//      is BRACKETED by two calls whose targets and order the check does
//      verify, so what they can hide is an argument, not a structure.
//    * The 661-byte window is the one that matters, and 488 bytes of it
//      (0x1002d404..0x1002d5ec) are the nine ramp loops.  Those are
//      covered instead by an INSTRUCTION CENSUS WITH ZERO RESIDUE
//      (below): `shr` 10, `dec` 10,
//      `inc` 8, `cmp` 12, `xor` 16, `jbe` 14 and the four `add $imm`
//      rows all fall out of "nine 256-entry ramp loops, the first one
//      unrolled by two", and not one of them has a byte spare.
//
//  ------------------------------------------------------------------- //
//  THE 39 CALLS, IN SHIPPED ORDER, WITH WHAT PINS EACH
//  ------------------------------------------------------------------- //
//
//   #  site        target                what it is
//   -- ----------  --------------------  ----------------------------------
//    1 0x1002d1c8  0x100805a0            _chkstk (`mov $0x5048,%eax`).
//                                        KNOWN RESIDUAL: we do
//                                        not emit it.  Do NOT try /Ge.
//    2 0x1002d25b  *0xc(surface)         IDirectDrawSurface7 slot 3 =
//                                        AddAttachedSurface(pZ)
//    3 0x1002d27d  *0x20(surface)        slot 8 = DeleteAttachedSurface(0,pZ)
//    4 0x1002d29f  *0x20(g_pD3DDevice)   IDirect3DDevice7 slot 8 =
//                                        SetRenderTarget(pSurf, 0)
//    5 0x1002d2b9  0x10032990            d3dneutral.cpp: `return
//                                        g_1008d7e4 > 0;` (14 bytes)
//    6 0x1002d2e5  0x1006c510            d3dr3b.cpp: the 64-entry
//                                        saturation ramp builder, "1
//                                        caller, 0x1002d2e5, which passes
//                                        the float global 0x1008cb94"
//    7 0x1002d3ba  0x1001eb90            d3dr3d.cpp: 925 bytes, 1 caller;
//                                        the frame setup.  Stores its
//                                        arg1 into 0x1008beb0 and its
//                                        arg3 into 0x1008b878, and zeroes
//                                        0x1008be44..0x1008be5c -- the
//                                        very counters printed below.
//    8 0x1002d3ca  0x1002b0e0            d3dr3d.cpp: the fog/pixel-format
//                                        routine installer, "195 bytes of
//                                        body, 1 caller (0x1002d3ca)"
//    9 0x1002d664  *0x50(g_pD3DDevice)   SetRenderState(FILLMODE, n)
//   10 0x1002d67f  0x10029894            big26728.cpp: "register ABI, one
//                                        argument in %eax; the world-draw
//                                        entry point"
//   11 0x1002d6ae  *0x50(g_pD3DDevice)   SetRenderState(FILLMODE, SOLID)
//   12 0x1002d6d5  0x10021d60            d3d_Warning, "Texture changes: %d"
//   13 0x1002d6f2  0x10021d60            d3d_Warning, "Texture uploads: %d"
//   14 0x1002d71f  *0x14(g_pStruct)      RenderStruct +0x14 ConsolePrintf
//   15 0x1002d737  *0x14(g_pStruct)      ditto  ... and so on for
//   16 0x1002d74f  *0x14(g_pStruct)      #14..#23 and #25..#28: FOURTEEN
//   17 0x1002d773  *0x14(g_pStruct)      ConsolePrintf calls in one block
//   18 0x1002d78e  *0x14(g_pStruct)
//   19 0x1002d7a6  *0x14(g_pStruct)
//   20 0x1002d7be  *0x14(g_pStruct)
//   21 0x1002d7d6  *0x14(g_pStruct)
//   22 0x1002d7ee  *0x14(g_pStruct)
//   23 0x1002d806  *0x14(g_pStruct)
//   24 0x1002d80c  0x1007b8d0            `mov $0x10091a10,%eax ; ret` --
//                                        the singleton accessor
//   25 0x1002d82a  *0x14(g_pStruct)
//   26 0x1002d84e  *0x14(g_pStruct)
//   27 0x1002d869  *0x14(g_pStruct)
//   28 0x1002d881  *0x14(g_pStruct)
//   29 0x1002d8a9  *0x14(g_pStruct)      "Tri area drawn: %.3f"
//   30 0x1002d8e8  *0x14(g_pStruct)      "Overdraw: %.3f"
//   31 0x1002d8f7  0x1002d0b0            d3dr3c.cpp: "ShowTexInfo -- ask
//                                        the device for
//                                        D3DDEVINFO_TEXTUREMANAGER";
//                                        DECODED there, every instruction
//   32 0x1002d91a  0x1002cf48            register ABI (%eax, %edx); reads
//                                        the device's viewport through
//                                        +0x54/+0x50 and clears a texture
//                                        stage through +0x8c
//   33 0x1002d938  0x100145f0            d3dqip3.cpp: "a bare `c3`, an
//                                        empty function"
//   34 0x1002d969  0x10021d60            d3d_Warning(9, "Rebuilding
//                                        warble table")
//   35 0x1002d971  0x10015d60            d3dr3a.cpp: "180 bytes including
//                                        the alternate entry, 1 caller
//                                        (0x1002d971).  BUILDS A 32-ENTRY
//                                        WAVE TABLE AND ITS FORWARD
//                                        DIFFERENCES"
//   36 0x1002d9ad  0x1007fd30            floor(double) -- big19d64.cpp
//                                        ("the three `call 0x1007fd30`
//                                        are floor(double)"), and the
//                                        call site pushes 8 bytes with
//                                        `fstl (%esp)`
//   37 0x1002d9e8  *0x14(g_pStruct)      "ModelProfile: %d clipped, ..."
//   38 0x1002da17  0x10021d60            d3d_Warning(0, "Error: tried to
//                                        render 3D while in optimized 2D
//                                        mode.")  -- SUNK cold block
//   39 0x1002da47  0x1002e350            d3dr3d.cpp lists it at 368 bytes
//                                        ("`mov %edx,%eax ; mov
//                                        -0x8(%ebp),%edx ; call
//                                        0x1002dd6c`"); SUNK cold block
//
//  ------------------------------------------------------------------- //
//  HOW EACH OF THE 22 INDIRECT TARGETS IS PINNED
//  (The call sequence matches indirect calls BY POSITION ONLY.)
//  ------------------------------------------------------------------- //
//
//  (A) SIXTEEN of them -- #14..#23, #25..#30, #37 -- are
//      `mov 0x1008cbcc,%eax ; call *0x14(%eax)`.  0x1008cbcc is
//      g_pStruct, the ENGINE's RenderStruct, named identically in
//      d3dmid.cpp, d3dbatchc.cpp and d3dbatchd.cpp, and d3d.cpp's map of
//      lithtech.exe 0x00462110 (which writes every callback from a
//      literal) fixes **+0x14 = ConsolePrintf**.  Independent
//      confirmation from the arguments: every one of the sixteen pushes a
//      printf format string as its first argument and 0, 1 or 2 dwords
//      after it, and two of them (`"Tri area drawn: %.3f"`,
//      `"Overdraw: %.3f"`) push an 8-byte `fstpl` -- a double promotion,
//      i.e. a varargs call.  A FUNCTION-POINTER SLOT AT A KNOWN OFFSET IN
//      A NAMED STRUCT, written by a decoded installer on the other side
//      of the DLL boundary.
//
//  (B) TWO of them -- #9 and #11 -- are
//      `mov 0x1008d988,%eax ; mov (%eax),%eax ; call *0x50(%eax)`.
//      0x1008d988 is g_pD3DDevice and **+0x50 is
//      IDirect3DDevice7::SetRenderState (index 20)**, fixed in d3d.cpp,
//      d3dbatchc.cpp, d3dopt2d.cpp, big124a4.cpp, big572b4.cpp,
//      big26728.cpp and big299a8.cpp.  Both push three dwords
//      (this, 8, v) which is `ret 0xc`, SetRenderState's arity.  The
//      state number 8 is D3DRENDERSTATE_FILLMODE and the two values are
//      2 and 3 -- D3DFILL_WIREFRAME and D3DFILL_SOLID, the only two
//      D3DFILLMODE members a renderer would use -- and the 2 is selected
//      by the console variable at 0x1008cab4, which big21f90.cpp's
//      86-record decode names **Wireframe**.  Three independent
//      agreements on one slot.
//
//  (C) #4 is `mov 0x1008d988,%edx ; mov (%edx),%eax ; call *0x20(%eax)`
//      -- the same named device, **+0x20 = index 8 =
//      IDirect3DDevice7::SetRenderTarget(lpNewRenderTarget, dwFlags)**.
//      Arity agrees (three dwords, `ret 0xc`) and the first argument is
//      the surface this function just made current.
//
//  (D) #2 and #3 are on the two DIRECTDRAW SURFACES, not on the device:
//      0x1008bf6c and 0x1008bf68, which d3dbatcha.cpp, d3ddraw.cpp and
//      d3dr5b.cpp all name (g_pPrimary / g_pOffscreen, a.k.a.
//      g_pPrimarySurface / g_pBackSurface), with 0x1008bf64 g_pZBuffer.
//      **+0x0c = IDirectDrawSurface7 index 3 = AddAttachedSurface(lpSurf)**
//      (two dwords, `ret 8`) and **+0x20 = index 8 =
//      DeleteAttachedSurface(dwFlags, lpSurf)** (three dwords, `ret 0xc`).
//      What pins the pair is that the ONE argument each takes is
//      g_pZBuffer and that they are called in the same breath as
//      SetRenderTarget on the same two surfaces: detach the z-buffer from
//      the surface that stops being the target, attach it to the one that
//      becomes it.  d3ddraw.cpp independently fixes +0x6c on the same
//      vtable as IDirectDrawSurface7::Restore (index 27), so the vtable
//      identity is not in question, only the two slots -- and indices 3
//      and 8 are the only slots of that arity in that range.
//      RESIDUAL HONESTY: the two surface slots are the weakest of the
//      four vtable claims here.  They are consistent with
//      IDirectDrawSurface7 and with the arguments, but no string or
//      decoded installer names them the way +0x14 and +0x50 are named.
//
//  ------------------------------------------------------------------- //
//  THE FRAME.  `mov $0x5048,%eax ; call 0x100805a0` = 20,552 bytes, and
//  it decomposes with 20 bytes of slack.
//  ------------------------------------------------------------------- //
//
//    -0x5028 .. -0x0029   20,480  THE SCENE SETUP BUFFER.  `lea
//                                 -0x5028(%ebp),%eax` at 0x1002d3a5 and
//                                 `movl $0x5000,0x8(%esp)` at 0x1002d3ab
//                                 -- the pointer and its own size are
//                                 handed to 0x1001eb90 together, so the
//                                 0x5000 is not a guess.
//    -0x0028 .. -0x0025        4  the int32 the pixel count goes through
//                                 for `fildl` (`mov %eax,-0x28(%ebp) ;
//                                 fildl -0x28(%ebp)`, 0x1002d8d7).
//    -0x0024 .. -0x0011       20  hole.
//    -0x0010 .. -0x0009        8  the double copy of the warble time,
//                                 saved across the call to floor
//                                 (`fstpl -0x10(%ebp)` / `fsubrl
//                                 -0x10(%ebp)`).
//    -0x0008 .. -0x0005        4  the ONE spill in the whole function:
//                                 the unrolled ramp loop's second output
//                                 pointer (0x1002d439 / 0x1002d44c).
//    -0x0004 .. -0x0001        4  unused.
//    below -0x5028            32  12 bytes of %ebx/%esi/%edi saved
//                                 esp-relative plus 20 of alignment.
//
//  Exactly FOUR `-N(%ebp)` displacements appear in the whole function
//  (-0x8, -0x10, -0x28, -0x5028), which is the whole of the frame's
//  evidence and is why the 20,480-byte buffer has to be one object.
//
//  ------------------------------------------------------------------- //
//  THE LITERAL POOL AS A STATEMENT-ORDER WITNESS, AND THE TWO DEAD 1.0f
//  ------------------------------------------------------------------- //
//
//  This function's run in `.data1` is 0x100a61a0..0x100a61c0, nine slots,
//  every one dumped from the image and every reference in the whole of
//  .text counted:
//
//      0x100a61a0  0x3f000000  0.5     LIVE   fadds  0x1002d32c
//      0x100a61a4  0x3f000000  0.5     LIVE   fadds  0x1002d346
//      0x100a61a8  0x3f800000  1.0     DEAD   0 references in .text
//      0x100a61ac  0x3f000000  0.5     LIVE   flds   0x1002d314
//      0x100a61b0  0x3f800000  1.0     LIVE   fmuls  0x1002d35a
//      0x100a61b4  0x3a83126f  0.001   LIVE   fcomps 0x1002d3e8
//      0x100a61b8  0x3f800000  1.0     DEAD   0 references in .text
//      0x100a61bc  0x3a83126f  0.001   LIVE   fcomps 0x1002da69
//      0x100a61c0  0x3a83126f  0.001   LIVE   fcomps 0x1002da8d
//
//  THE TWO DEAD 1.0f ARE THE TWO DIVISIONS, and the known rule
//  ("Every float division emits a DEAD 1.0f next to the live one") is
//  exact here rather than approximate: the function contains precisely
//  two divides -- `fdivs 0x1008cb6c` at 0x1002d31a (0.5f / EnvScale,
//  whose live numerator 0.5f is the slot IMMEDIATELY AFTER its dead 1.0f)
//  and the Intel FDIVP at 0x1002d8dd (the Overdraw ratio, whose numerator
//  is a variable, so its dead 1.0f stands alone).  Nothing here is
//  unexplained and no statement had to be invented.
//
//  AND THE POOL ORDER IS THE STATEMENT ORDER, including the sunk blocks
//  last -- exactly the reading found for 0x1000e0e0's
//  -100000.0f run.  Source order: the two `+ 0.5f` pan offsets, then the
//  divide, then the `* 1.0f`, then the FIRST of the three light-colour
//  epsilon tests (which stays in the main flow), then the Overdraw
//  divide, then the SECOND and THIRD epsilon tests -- both of which live
//  in cold blocks sunk below the epilogue at 0x1002da54 and 0x1002da78.
//  That is the only ordering consistent with all nine slots, and it is
//  independent confirmation that the three-term epsilon test is one
//  `||` chain with only its first term inline.
//
//  Two more constants, dumped from .data rather than .data1:
//    0x100893a0  0.0005  -- big21f90.cpp's console variable **EnvPanSpeed**,
//                           and its default is readable on disk because it
//                           falls in .data's initialised part.
//    0x1008d528/52c/530 are initialised to 0x40a00000 = **5.0f** each, by
//                           the very next function (0x1002dd40..0x1002dd53).
//                           5.0 is out of range for the light scale they
//                           cache, which is what forces the ramp rebuild on
//                           the first frame -- and what proves the triple is
//                           float and not int.
//
//  ------------------------------------------------------------------- //
//  THE INSTRUCTION CENSUS.  582 instructions, ZERO RESIDUE.
//  ------------------------------------------------------------------- //
//
//  `sed 's/.*\t//' | awk '{print $1}' | sort | uniq -c` over the shipped
//  disassembly of 0x1002d1c0..0x1002dab0, less the 14 continuation lines
//  objdump prints for multi-byte immediates.  The claimed structure is
//  NINE 256-entry ramp loops (three colour channels x {no-saturate,
//  saturate} for the first table, three for the second), the FIRST of
//  which icl unrolled by two, plus EIGHT float compares, TWO divisions,
//  THREE component negations and THREE return statements.
//
//   | mnemonic | shipped | what the structure requires                  |
//   |----------|---------|----------------------------------------------|
//   | shr      |   10    | 9 loops x `nAccum >> 16`, +1 for the unroll  |
//   | dec      |   10    | 8 single loops x 1 + 2 in the unrolled one   |
//   | inc      |    8    | 8 single loops' `pRamp++`; the unrolled one  |
//   |          |         | uses `lea 0x1(%reg)` instead (see lea)       |
//   | cmp      |   12    | 10 ramp clamps (9 loops + 1 unroll) + the    |
//   |          |         | RenderToFront/cache compare + the unrolled   |
//   |          |         | loop's `cmp $-1,%edx`                        |
//   | jbe      |   14    | the same 10 clamps + 4 of the epsilon tests  |
//   | xor      |   16    | 9 ramp `nAccum = 0` + 2 `&&` false arms + 2  |
//   |          |         | zero registers for the counter resets + 3    |
//   |          |         | `return LTFALSE`                             |
//   | add      |   50    | 20 outgoing-area reserves + 15 releases +    |
//   |          |         | 8 `add $0x1x0000` steps + 6 `add $0x100` /   |
//   |          |         | `add $0x200` array bases + `add $0x6c`       |
//   | add $0x100 / $0x200 | 3 / 3 | THE ARRAY EVIDENCE: three groups |
//   |          |         | of three ramps, and channels 1 and 2 are     |
//   |          |         | reached as base + constant index, which      |
//   |          |         | big21f90.cpp records as icl's shape for a    |
//   |          |         | constant-index ARRAY element.  Nine separate |
//   |          |         | scalars would fold to nine immediates.       |
//   | add $0x10000 / $0x20000 | 5 / 3 | the six non-saturate loops  |
//   |          |         | less the unrolled one (which uses `lea`),    |
//   |          |         | and the three saturate loops                 |
//   | test     |   24    | every `if (global)` in the function; counted |
//   |          |         | site by site in the region map below        |
//   | movl     |   35    | 21 format-string / level immediates + the    |
//   |          |         | 0x5000 size + the two hook stores + the two  |
//   |          |         | cache-valid flags + 9 zero/small argument     |
//   |          |         | slots                                        |
//   | flds     |   17    | 1 sat-cache + 1 `0.5f` + 1 pos.x + 1         |
//   |          |         | EnvPanSpeed + 3 light dir + 6 epsilon        |
//   |          |         | operands (3 pairs) + 2 tri-area + 1 warble   |
//   |          |         | scale + 1 pScene->m_68                       |
//   | fstps    |    7    | 2 pan offsets + 1 pan scale + 3 light dir +  |
//   |          |         | the warble fraction                          |
//   | fsts     |    2    | the pan scale stored to TWO globals off one  |
//   |          |         | value, and the warble time kept in st        |
//   | fmuls    |    4    | pos.x*EnvPanSpeed, EnvPanSpeed*pos.z,        |
//   |          |         | scale*1.0f, m_68*WarbleSpeed                 |
//   | fadds    |    3    | two `+ 0.5f` and the warble accumulate       |
//   | fdivs    |    1    | 0.5f / EnvScale                              |
//   | fdivrp*  |    1    | the Overdraw ratio.  *binutils prints Intel  |
//   |          |         | FDIVP (de f9) as `fdivrp`; verified with     |
//   |          |         | `as`, so it is st(1)/st(0) = area/pixels     |
//   | fildl    |    1    | the ONE integer-to-float cast: (float) of    |
//   |          |         | the pixel count                              |
//   | imul     |    1    | width * height                               |
//   | sub      |    2    | right-left and bottom-top.  The ONLY two     |
//   |          |         | integer subtractions in 2,288 bytes -- the   |
//   |          |         | frame is set by _chkstk and the outgoing     |
//   |          |         | areas by `add $-N`.                          |
//   | fcom     |    3    | the three light-colour magnitude compares    |
//   | fcomps   |    5    | three `> 0.001f` + sat-cache + warble-cache  |
//   | fnstsw   |    8    | = 3 + 5.  EIGHT float compares, no more.     |
//   | sahf     |    8    | the same eight                               |
//   | fsubp*   |    3    | *= Intel FSUBRP (de e1): the inline arm of   |
//   | fsubrp*  |    3    | each component; *= Intel FSUBP (de e9): the  |
//   |          |         | sunk arm.  THREE components, TWO arms each.  |
//   | fchs     |    3    | the three light-direction negations          |
//   | fstpl    |    3    | two `%.3f` double promotions + the warble    |
//   |          |         | time saved across floor                      |
//   | fstl     |    1    | the floor() argument                         |
//   | fsubrl   |    1    | x - floor(x)  (FSUBR m64: a MEMORY form, so  |
//   |          |         | binutils does NOT swap this one)             |
//   | fxch     |    2    | scheduling; not load-bearing                 |
//   | incl     |    1    | the frame counter at 0x1008ce90              |
//   | jmp      |   10    | 3 in the `&&`/sat-cache diamonds + 1 out of  |
//   |          |         | the no-saturate group + 1 in the fill-mode   |
//   |          |         | ternary + 5 in the sunk blocks               |
//   | je / jne |  19/16  | the guards; enumerated in the region map     |
//   | ja       |    2    | the second and third epsilon tests' "rebuild"|
//   | jl       |    1    | the SIGNED test of DrawWorldTree, whose      |
//   |          |         | registered default is -1.0f = 0xbf800000     |
//   | lea      |    9    | 1 `lea -0x5028(%ebp)` + 4 in the unrolled    |
//   |          |         | loop + 4 lea-nop pads (0x1002d4c2,           |
//   |          |         | 0x1002d4c9, 0x1002d4dc, 0x1002d569)         |
//   | nop      |    2    | 0x1002d4c1, 0x1002d568                       |
//   | ret      |    3    | the three `return LTFALSE`                   |
//   | push/pop |   1/3   | one `push %ebp`, three `pop %ebp`            |
//   | call     |   39    | the table above                              |
//   | mov      |  218    | includes SIX `mov %esi,%esi` two-byte pads   |
//   |          |         | (0x1002d46e, 0x1002d49e, 0x1002d50e,         |
//   |          |         | 0x1002d53e, 0x1002d59e, 0x1002d5ce) -- one   |
//   |          |         | before each of the six non-unrolled ramp     |
//   |          |         | loops that begins on an odd 16-byte offset   |
//
//  THE LOAD-BEARING ROWS ARE `shr 10`, `dec 10`, `inc 8`, `cmp 12`,
//  `jbe 14`, `xor 16` and the four `add $imm` rows.  A tenth ramp, a
//  missing ramp, a differently-clamped ramp, a fourth colour channel, or
//  a second unrolled loop would move at least four of them at once.
//  `fnstsw 8` / `sahf 8` / `fcom 3` / `fcomps 5` pin the number of float
//  compares to eight with nothing spare, which is what rules out a
//  fourth epsilon test or a `fabs()` call.  `sub 2` and `imul 1` say
//  the Overdraw pixel count is computed exactly once from exactly two
//  differences.  And `fchs 3` with `fstps 7` leaves no room for a
//  fourth negated component.
//
//  Note what is ABSENT and would have to be there under other readings:
//  no `and`, no `or`, no `shl`, no `idiv`, no `movzbl`, no `rep`, no
//  `fabs`, no `fsqrt`.  So there is no bitmask anywhere, no memcpy or
//  memset (the 20,480-byte buffer is never touched by this function --
//  it is passed straight to 0x1001eb90), and no CRT maths beyond the one
//  out-of-line `floor`.
//
//  ------------------------------------------------------------------- //
//  THE REGION MAP.  Every byte accounted for.
//  ------------------------------------------------------------------- //
//
//  0x1002d1c0..0x1002d1d8  PROLOGUE.  `push %ebp ; mov %esp,%ebp ;
//      mov $0x5048,%eax ; call _chkstk`, then %edi/%esi/%ebx saved
//      esp-relative at 0x8/0x4/0x0(%esp).
//
//  0x1002d1d8..0x1002d228  SIX GUARDS, five of them sharing ONE exit.
//      pScene, g_pPrimarySurface (0x1008bf6c), g_pD3DDevice (0x1008d988),
//      g_pDirectDraw (0x1008bf74) and g_bIn3D (0x1008d7c8) all `je
//      0x1002da30` -- one shared `xor %eax,%eax` + epilogue hosted at the
//      END of the function.  "One `||` hosts it last", against
//      "a run of guarded returns hosts its shared exit first", so this is
//      a single five-term `||` and not five statements.  The sixth,
//      g_bInOptimized2D (0x1008d7c4), `jne`s to its OWN block at
//      0x1002da05 -- a different answer (it prints first), hence a
//      separate statement and a second epilogue.
//      Note that the load of g_pPrimarySurface at 0x1002d1e3 is KEPT in
//      %eax across the next four tests and reused at 0x1002d23e, which is
//      why the source reads it into a local.
//
//  0x1002d228..0x1002d2ac  THE RENDER-TARGET SWAP, gated on
//      `g_varRenderToFront.m_nValue00 != g_nRenderToFrontLast`
//      (0x1008d534 vs 0x1008ce84; `je 0x1002d2ac` skips).  Three COM
//      calls: swap the two surface globals, attach the z-buffer to the new
//      one, detach it from the old, point the device at the new one, then
//      remember the setting.  The console-variable NAME is read off the
//      registrar 24 bytes past the end of this function: 0x1002dd27
//      pushes "RenderToFront" with `mov $0x1008d534,%ecx`.
//
//  0x1002d2ac..0x1002d2cb  THE PORTAL GATE.  `g_1008d958 =
//      g_varDrawPortals.m_nValue00 && sub_10032990();` -- three blocks,
//      `mov $0x1,%eax` / `xor %eax,%eax` / `xor %eax,%eax`, merging on one
//      store.  That is `&&` yielding 0 or 1, not a bit test.  "DrawPortals"
//      is 0x1002dc8f's string with `mov $0x1008d5b4,%ecx`.
//
//  0x1002d2cb..0x1002d314  THE SATURATION-TABLE CACHE.
//      `if (!g_bSatTableValid || g_CV_LightSaturate != g_fSatTableLast)
//       { sub_1006c510(g_CV_LightSaturate); g_fSatTableLast =
//         g_CV_LightSaturate; g_bSatTableValid = 1; }`
//      LightSaturate (0x1008cb94) is big21f90.cpp's name.  The rebuild
//      block is hosted at its FIRST reference (0x1002d2da) and the float
//      compare jumps BACKWARD into it from 0x1002d312 -- the
//      "shared-exit host" near class, size-neutral and with no call-sequence
//      consequence (the call stays between 0x10032990 and 0x1001eb90
//      either way).
//
//  0x1002d314..0x1002d3a2  THE ENVIRONMENT-MAP PAN, THE MODEL-PROFILE
//      RESET AND THE NEGATED LIGHT DIRECTION.  Four floats at
//      0x1008b4ec..0x1008b4f8, two counters at 0x1008cea4/a8 zeroed, and
//      0x1008ceb0..b8 = -(g_pStruct->m_vGlobalLightDir), the member
//      d3d.cpp's RenderStruct map puts at +0x130.  The divide is scheduled
//      to the top of the block; the pool (above) gives the source order.
//
//  0x1002d3a2..0x1002d3ca  THE FRAME SETUP.  `if
//      (sub_1001eb90(pScene, buf, 0x5000))` wraps everything down to
//      0x1002d8f7 -- the `je 0x1002d8fc` at 0x1002d3c4 is the only way
//      past it, and 0x1002d8fc is also where the ShowTexInfo test falls
//      through to, which is how the `if`'s extent is fixed.
//
//  0x1002d3ca..0x1002d3cf  `sub_1002b0e0();`  -- d3dr3d.cpp's installer.
//
//  0x1002d3cf..0x1002d3f7  THE LIGHT-COLOUR EPSILON TEST, first term
//      only; terms two and three are the sunk blocks at 0x1002da54 and
//      0x1002da78.  Per component: `fcom` without a pop, then the
//      subtraction in whichever order makes it positive (Intel FSUBRP in
//      the fall-through arm, FSUBP in a sunk arm), then ONE `fcomps
//      0.001f`.  Two arms feeding one comparison is a conditional
//      EXPRESSION, not two `if`s -- two `if`s would need two 0.001f
//      literals per component and the pool has exactly one each.
//
//  0x1002d3f7..0x1002d5ec  THE NINE COLOUR-RAMP LOOPS.
//      `if (!g_CV_Saturate)` (0x1008cb98, big21f90.cpp) picks step
//      0x10000 or 0x20000 for the three channels of g_ColorRamp
//      (0x1008cebc / +0x100 / +0x200 = big3e620.cpp's three 256-entry
//      ramps at 0x1008cebc / 0x1008cfbc / 0x1008d0bc), and the three
//      channels of g_ColorRamp2 (0x1008d1bc / 0x1008d2bc / 0x1008d3bc,
//      d3dhigh.cpp's names) always get 0x10000.  Each loop is
//      `pRamp[i] = (uint8)(nAccum >> 16); nAccum += nStep;
//       if (nAccum > 0xff0000) nAccum = 0xff0000;` -- `shr` and `jbe`
//      make nAccum UNSIGNED, and the store is of the value BEFORE the
//      increment.  The first of the nine is unrolled by two by icl (a
//      255-based counter, a mid-loop `test`/`je` for the odd trip and a
//      `cmp $-1` at the bottom): 128 + 128 = 256 stores, same body.
//
//  0x1002d5ec..0x1002d60e  the cache copy `g_vLightScaleLast =
//      g_vLightScale` (0x1008d528 <- 0x1008be80, three dwords through
//      three integer registers), reached from the rebuild path AND
//      jumped over by the sunk third epsilon test's `jmp 0x1002d60e` --
//      so it is INSIDE the `if`, and the join is at 0x1002d60e.
//
//  0x1002d60e..0x1002d636  FOUR counters and TWO RenderStruct fields
//      zeroed.  g_pStruct is loaded TWICE (0x1002d610 and 0x1002d62d) for
//      the two field stores, in the order +0x4c then +0x48: the source
//      writes the member expressions directly rather than through a local
//      ("paired field stores off one load need a block-local
//      temp" -- read backwards).
//
//  0x1002d636..0x1002d667  `SetRenderState(D3DRENDERSTATE_FILLMODE,
//      g_varWireframe.m_nValue00 ? D3DFILL_WIREFRAME : D3DFILL_SOLID)`.
//      A ternary: both arms materialise into %edx and merge (`mov $2` /
//      `jmp` / `mov $3`), which is the conditional-expression shape.
//
//  0x1002d667..0x1002d691  THE WORLD DRAW, bracketed by the mirror hook:
//      `if (!g_varShowPortalBounds.m_nValue00) { g_pfnMirrorHook =
//       sub_1002b0c0; sub_10029894(pScene); g_pfnMirrorHook = LTNULL; }`.
//      0x1008ef80 is written with the immediate 0x1002b0c0 and then with
//      0, and big299a8.cpp decoded 0x1002b0c0 (24 bytes, reachable only
//      as a function pointer, `sub_10026728(g_p1008beb0, sub_1007b8d0())`)
//      and records "0x1002d670 is `movl $0x1002b0c0,0x1008ef80`" plus
//      big26728.cpp's `g_1008ef80 = 0` at the head of the mirror pass.
//      "ShowPortalBounds" is 0x1002dcb5's string with
//      `mov $0x1008d594,%ecx`.
//
//  0x1002d691..0x1002d6b1  `SetRenderState(FILLMODE, D3DFILL_SOLID)`.
//
//  0x1002d6b1..0x1002d6fa  `if (g_varShowTextureCounts.m_nValue00)` --
//      two d3d_Warning lines.  0x1008cabc is big21f90.cpp's
//      ShowTextureCounts, and the two strings say what the two counters
//      are.
//
//  0x1002d6fa..0x1002d887  `if (g_varShowPolyCounts.m_nValue00)` --
//      0x1008caa8, big21f90.cpp's ShowPolyCounts -- and FOURTEEN
//      ConsolePrintf lines with the direct call to the 0x10091a10
//      singleton accessor between the tenth and the eleventh, for
//      "Portals: %d" (`call 0x1007b8d0 ; mov 0x140(%eax),%eax`).
//
//  0x1002d887..0x1002d8ee  `if (g_varShowFillInfo.m_nValue00)`
//      (0x1008cacc) -- "Tri area drawn" and "Overdraw", the second being
//      the tri area divided by the viewport's pixel count.
//
//  0x1002d8ee..0x1002d8fc  `if (g_varShowTexInfo.m_nValue00)
//      sub_1002d0b0();`  -- 0x1008d554, "ShowTexInfo" at 0x1002dd01, and
//      d3dr3c.cpp's decode of the callee agrees with the name.  This is
//      the END of the `if (sub_1001eb90(...))` body: its `je` target
//      0x1002d8fc is the same as the setup test's.
//
//  0x1002d8fc..0x1002d922  `if (g_varDrawWorldTree.m_nValue00 >= 0 &&
//      g_p1008beac) sub_1002cf48(&g_p1008beac->m_6c,
//      g_varDrawWorldTree.m_nValue00);`  A SIGNED `jl` because the
//      variable's registered default is -1.0f (0x1002dc15 pushes
//      0xbf800000 with `mov $0x1008d624,%ecx` and 0x1002dc1d the name
//      "DrawWorldTree"), i.e. "-1 means no node selected".  g_1008beac is
//      loaded TWICE (0x1002d906, 0x1002d90f) -- two source mentions of
//      the global, no local.
//
//  0x1002d922..0x1002d93d  `if (g_varDrawTerrainSections.m_nValue00 &&
//      g_p1008beac) sub_1002e350(g_p1008beac);` then
//      `sub_100145f0();`.  The call-bearing arm is SUNK to 0x1002da41.
//      "DrawTerrainSections" is 0x1002dc43's string with
//      `mov $0x1008d604,%ecx`.  SEE COIN-FLIP 1.
//
//  0x1002d93d..0x1002d98a  THE WARBLE-TABLE CACHE, the same shape as the
//      saturation cache but hosted the OTHER way round (the block at its
//      LAST reference, both tests jumping forward).  Two statements in the
//      block, in store order: the valid flag then the cached scale --
//      the opposite order from the saturation block, and both are read off
//      the bytes rather than made uniform.
//
//  0x1002d98a..0x1002d9ee  THE WARBLE CLOCK and the ModelProfile line.
//      `g_fWarbleTime` accumulates `g_pSceneDesc->m_f68 *
//      g_CV_WarbleSpeed`; the value stays in %st(0) across the `fsts`, is
//      pushed to floor() as a double and to a local double, and
//      `FSUBR m64` gives the fraction.  Then
//      `if (g_varModelProfile.m_nValue00)` (0x1008ca38, big21f90.cpp's
//      ModelProfile -- and big48180.cpp independently calls it "the timing
//      cvar") prints the two counters this function zeroed at 0x1002d360.
//      Note the m_f68 read goes through the GLOBAL 0x1008beb0, not through
//      %edi: 0x1001eb90 stored the argument there (`mov %ebx,0x1008beb0`
//      at 0x1001ebc9) and the source uses the global name here.
//
//  0x1002d9ee..0x1002da05  `g_nFrameCount++;` (`incl 0x1008ce90`, and
//      0x100263b3 zeroes the same dword) and epilogue 1.
//
//  0x1002da05..0x1002da30  SUNK: the optimized-2D warning and epilogue 2.
//
//  0x1002da30..0x1002da41  epilogue 3, the five guards' shared exit.
//
//  0x1002da41..0x1002da54  SUNK: `sub_1002e350(g_p1008beac)` and
//      `jmp 0x1002d938` back into the main flow.
//
//  0x1002da54..0x1002daad  SUNK: epsilon terms two and three (each a
//      `fcom`/`FSUBRP`/`fcomps` triple), plus the three `FSUBP` arms at
//      0x1002daa1, 0x1002daa5 and 0x1002daa9 that the three `fcom`s
//      branch to.  0x1002da9c's `jmp 0x1002d60e` is the "no rebuild
//      needed" join and 0x1002daab's `jmp 0x1002d3e8` re-enters term
//      one's comparison.  The last instruction ends at 0x1002dab0.
//
//  ------------------------------------------------------------------- //
//  THE EXPECTED CALL-SEQUENCE RESULT, predicted before any build
//  ------------------------------------------------------------------- //
//
//  The source below is written so that its call order IS the shipped
//  order of the 39-row table above, less `_chkstk`.  Two of the source's
//  statements are deliberately placed to achieve that -- the
//  optimized-2D warning last (COIN-FLIP 2) and nothing else moved -- so
//  ours emits 38 calls in shipped sequence.
//
//  **EXPECTED, BEST CASE: 38 of 39 in agreement -- 16 by resolved
//  target, 22 by position (the indirect ones) -- with ONE shipped-only
//  row and ZERO ours-only rows.**  That row is
//
//      0x100805a0 `_chkstk` at 0x1002d1c8.  A 20,552-byte frame; our
//      build does not emit the probe.  This is benign,
//      and /Ge closes it at the cost of nineteen unknown
//      ours-only rows.  NOT proposed.
//
//  **EXPECTED, IF THE SINK DOES NOT REPRODUCE: 37 of 39**, with two
//  shipped-only and one ours-only, all three concerning 0x1002e350 and
//  `_chkstk`.  0x1002da47's call is in a block icl sank below the
//  epilogue; written as an ordinary `if` body (which is how it is written
//  below) it lands between 0x1002cf48 and 0x100145f0 instead of last, and
//  the LCS reports that as one insertion plus one deletion.  COIN-FLIP 1
//  gives the one-line spelling change that should move it, and predicts
//  exactly this pair as the symptom.
//
//  A THIRD, SMALLER RISK, stated so that it is not mistaken for a decode
//  error: if icl declines to host the optimized-2D warning block after
//  the body -- i.e. if COIN-FLIP 2's reading is wrong -- 0x10021d60's
//  first call moves from position 38 to position 2 and costs another
//  shipped-only/ours-only pair, taking the figure to 36 of 39.  The
//  shipped `jne 0x1002da05` into a block that sits after the normal
//  epilogue is the evidence that it should not.
//
//  ANYTHING ELSE IS A DECODE ERROR, with one named exception: a
//  shipped-only row on 0x1007fd30 would be icl expanding `floor` inline,
//  and THAT is the one row /Oi- would fix.
//
//  /Oi- JUDGEMENT: **this file does NOT need /Oi-.**  The only CRT
//  function it calls is `floor`, at 0x1007fd30, and the reason to believe
//  icl leaves it as a call is positive rather than hopeful: there is no
//  x87 instruction for floor, the shipped call site is the ordinary
//  8-byte double push (`fstl (%esp)`), and big19d64.cpp already reports
//  three calls to the same address surviving in a decoded neighbour.
//  There is no sqrt, fabs, tan, atan, memcpy or memset anywhere in the
//  census (no `fsqrt`, no `fabs`, no `rep`), so /Oi- has nothing to buy
//  here -- and it can cost calls by turning open-coded
//  copies into memcpy rows.  IF `floor` does come back as a shipped-only
//  row, that single row is what /Oi- would fix, and only then.
//
//  There is no /Qip two-entry offset to expect: none of the seventeen
//  direct callees is in this translation unit, so every call resolves to
//  the callee's own address.  Three of them (0x10029894, 0x1002cf48,
//  0x1002e350) are REGISTER-ABI bodies -- each is decoded as
//  an ordinary call whose argument arrives in %eax, noted, and left
//  there, which is what the declarations below do.  They will not be
//  byte-identical at the call site (icl will push where the shipped code
//  loads %eax/%edx) but they ARE calls to the right targets in the right
//  order, which is what the acceptance test measures.
//
//  ------------------------------------------------------------------- //
//  COIN-FLIPS.  Both spellings stated, so one build settles each.
//  ------------------------------------------------------------------- //
//
//  1. THE SUNK `sub_1002e350` ARM -- the only coin-flip that costs
//     calls, and the other spelling is worth trying rather
//     than record it as benign block placement (it is a DIRECT call, so
//     the "equal counts, all indirect" excuse does not apply).
//
//       (a) as written below:
//               if (g_varDrawTerrainSections.m_nValue00 && g_p1008beac)
//                   sub_1002e350(g_p1008beac);
//               sub_100145f0();
//           -- predicts the call INLINE, between 0x1002cf48 and
//           0x100145f0: 36 of 39, one shipped-only/ours-only pair.
//
//       (b) the inverted spelling, which is what sinks a
//           call-bearing arm:
//               if (!g_varDrawTerrainSections.m_nValue00 || !g_p1008beac)
//                   ;
//               else
//                   sub_1002e350(g_p1008beac);
//           -- predicts the call SUNK to the end and 37 of 39.
//
//     The shipped branch senses are `je` past (on the first test) and
//     `jne` INTO the block (on the second), which is (b)'s shape: for (a)
//     both tests would `je` past a fall-through body.  (a) is written
//     anyway, because it is the spelling a human would have written
//     and because an empty `then` arm is not credible source; if the
//     build reports the pair, (b) is the one-line change and the
//     prediction above says exactly what it should move.
//
//     *** BOTH WERE BUILT.  (a)'s prediction was exact: 36 of 39 with one
//     shipped-only/ours-only pair.  (b) MEASURED IDENTICALLY -- same 36,
//     same three pairs -- so icl compiles `if (!A || !B) ; else f();` and
//     `if (A && B) f();` to the same thing, and this is not the lever.
//     (a) is kept, being the credible source.
//
//     What the residual actually is: the three unmatched rows are NOT
//     about 0x1002e350 in isolation.  Shipped calls 0x10021d60 twice at
//     0x1002d6d5/0x1002d6f2 and an indirect at 0x1002d9e8 where we do
//     not, and we emit 0x1002e350, an indirect and 0x10015d60 where it
//     does not.  That is one debug block placed elsewhere, carrying its
//     calls with it -- equal counts, and the LCS charges it twice.
//
//  2. THE OPTIMIZED-2D GUARD'S ARM ORDER -- settled, not a coin-flip,
//     and worth stating because it looks like one.  The warning block
//     sits at 0x1002da05, AFTER the normal epilogue, so its call is
//     shipped position 38.  The rule "one guard, one shared answer:
//     test for the case that CONTINUES" says the negated spelling
//         if (g_bInOptimized2D) { warn; return LTFALSE; }
//         ... body ...
//     hosts the block ahead of the body (our position ~2, costing a
//     shipped-only/ours-only pair), and the positive spelling
//         if (!g_bInOptimized2D) { ... body ...; return LTFALSE; }
//         warn; return LTFALSE;
//     hosts it after.  The shipped `jne 0x1002da05` -- a forward jump
//     into a block at the end -- is the positive spelling, and it is
//     what accounts for the SECOND of the three epilogues.  Written that
//     way below.  The other spelling predicts 36 of 39 rather than 37.
//
//  3. THE NINE RAMP LOOPS: nine macro expansions (below) or two nested
//     `for` loops with the outer, three-trip loop unrolled by icl?  The
//     `add $0x100` / `add $0x200` rows prove the ARRAY (a constant index
//     that icl does not fold into the base immediate), but not which
//     construct produced the three copies.  A macro is used because it is
//     guaranteed call-free: a `static` helper with nine call sites is the
//     one shape that could cost nine ours-only rows if icl declined to
//     inline it, and no evidence in the bytes distinguishes the two.  The
//     alternative spelling is
//         for (i = 0; i < 3; i++)
//             for (j = 0; j < 256; j++) { ... g_ColorRamp[i][j] ... }
//     three times over.  Predicted call-sequence difference: none either way.
//
//  4. `g_f1008b4f4 = g_f1008b4f8 = 0.5f / g_CV_EnvScale * 1.0f;` -- the
//     `* 1.0f` is real (a live `fmuls 0x100a61b0` with 0x3f800000 dumped
//     from the pool, and the pool slot is not shared with any other
//     function), but WHERE the source put it is not recoverable: the two
//     stores are `fsts` then `fstps` off ONE %st(0) value, so the
//     expression occurs once and reaches two lvalues.  It is written as a
//     local assigned to both.  The alternative is a chained assignment;
//     both give one literal each of 0.5f and 1.0f, which is what the pool
//     has.  Two copies of the expression would put TWO 0.5f and TWO 1.0f
//     in the pool (icl does not pool duplicate literals -- d3dr3b.cpp and
//     d3dr3a.cpp both record that), so that spelling is ruled out.
//
//  5. THE NEGATED LIGHT DIRECTION.  Shipped emits three `flds`+`fchs`
//     THEN three `fstps` (with one `fxch`), which is the grouped shape of
//     a vector-valued expression -- `LTVector::Init(-x,-y,-z)` or a unary
//     `operator-`.  Three separate component assignments are written, which
//     give three load/negate/store triples instead: instruction-
//     equivalent, differently scheduled.  The reason is the
//     warning that `_CVector`'s members come out as CALLS in this
//     toolchain ("Mag, MagSqr, Norm and Cross come out as calls"), and a
//     spurious ours-only call would cost more than the scheduling does.
//     Predicted call-sequence difference: none for the spelling below; the
//     Init spelling risks one ours-only row.
//
//  6. `g_fWarbleTime = g_fWarbleTime + m_f68 * g_CV_WarbleSpeed;` versus
//     `+=`.  Shipped emits the product first and folds the accumulator
//     into a memory `fadds`, so the accumulator is the term written
//     FIRST in the source (a flat sum emits its terms
//     backwards).  Both spellings are the same size and neither makes a
//     call.  Written as the explicit sum.
//
//  ------------------------------------------------------------------- //
//  WHAT COULD NOT BE PINNED
//  ------------------------------------------------------------------- //
//
//  Nothing is left `// UNDECODED:` -- every one of the 582 instructions
//  is placed.  What is left is NAMING, and each item is named by address
//  in the declarations below:
//
//   * 0x1008ce84 -- the remembered RenderToFront setting.  Written only
//     here (0x1002d2a7) and read only here; no string names it.
//   * 0x1008d958 -- big64cc0.cpp calls it "the lightmap-animation gate.
//     Only ever tested."  Given that this function computes it as
//     `DrawPortals && sub_10032990()`, "portals are live" fits better,
//     but nothing settles it, so it is declared by address with both
//     readings noted.
//   * 0x1008b4ec..0x1008b4f8 -- four floats, an environment-map pan
//     (offset u, offset v, scale u, scale v is the natural reading of
//     `pos.x*speed + 0.5f`, `pos.z*speed + 0.5f`, `0.5f/scale`,
//     `0.5f/scale`).  Referenced nowhere else in .text, so the field
//     roles are inferred from the arithmetic and not from a consumer.
//   * 0x1008b89c/a0/a4/a8 -- the four int32 whose differences give the
//     pixel count.  (a4-9c) x (a8-a0) is all the bytes prove; it is read
//     as {left, top, right, bottom} because that is the only assignment
//     of the four slots that makes both differences positive extents.
//     d3dmisc.cpp calls 0x1008b8a8 `g_nCounterA8`, so the naming is
//     open; the arithmetic is not.
//   * 0x1008d4bc / 0x1008d4c0 / 0x1008d4c4 and 0x1008d520 -- four
//     per-frame counters zeroed together.  bigb910.cpp names 0x1008d520
//     "the shared draw-vertex buffer['s] count" and d3dr5b.cpp declares
//     0x1008d4bc as a `uint32`; the other two are by address.
//   * g_pStruct +0x48 / +0x4c -- two dwords zeroed per frame.  d3d.cpp's
//     RenderStruct map covers +0x3c..+0x44 and +0x58..+0x68 but not
//     these two; big26728.cpp uses +0x50 on the same object.  Declared by
//     offset.
//   * The seventeen statistics counters at 0x1008bd78..0x1008be5c are
//     named by their own format strings, which is as strong as naming
//     gets, but their TYPES are only `%d` (so int32) and their addresses
//     are not a contiguous array (0x1008be28..0x1008be5c has gaps this
//     function does not touch), so they are declared as separate scalars.
//   * pScene +0x68 -- clientblit.cpp's map of the caller records it as
//     `uint32 pClientMgr+0x1258`; this function reads it with `flds`, so
//     it is a float.  What it MEANS (a frame time is the obvious guess,
//     since it scales a wave clock) is not pinned by either side.
//
//  ------------------------------------------------------------------- //
//  ONE MORE THING THE NEXT FUNCTION PROVES: TWO KINDS OF CONSOLE
//  VARIABLE, AND THIS FUNCTION USES BOTH.
//  ------------------------------------------------------------------- //
//
//  big21f90.cpp decoded the 86-record initialiser at 0x10021f90 and
//  showed that a CD3DConVar record's arguments 3 and 4 are an `int32*`
//  and a `float*` "write-through" destination.  Its variables therefore
//  appear in code as plain scalars: `g_CV_Wireframe` at 0x1008cab4,
//  `g_CV_Saturate` at 0x1008cb98, `g_CV_LightSaturate` (float) at
//  0x1008cb94, and so on -- and that is how this function reads six of
//  the seven it consults.
//
//  The SECOND initialiser -- 0x1002db00, the function 80 bytes past the
//  end of ours, whose records run DOWNWARD from 0x1008d704 to 0x1008d534
//  in registration order (mostly by 0x20, with one 0x30 step between
//  0x1008d5e4 and 0x1008d5b4 that a record outside the dumped window
//  presumably fills) -- passes `xor %eax,%eax` for BOTH destination
//  pointers at every
//  one of its registrations (0x1002db1f/0x1002db23 and every pair after).
//  Those variables have no write-through slot at all, so their value can
//  only be read out of the record's own +0x00, which big21f90.cpp names
//  `m_nValue00` ("pVar->m_n00 = (int32)pVar->m_f04" in the updater
//  0x10021e50).  And that is exactly what this function does: six loads
//  at 0x1008d534, 0x1008d554, 0x1008d594, 0x1008d5b4, 0x1008d604 and
//  0x1008d624, every one a record base.
//
//  That is what names them.  The registrar's own strings, read with
//  0x1002dbf0..0x1002dd60 of the retail d3d.ren:
//
//      0x1002dd27  "RenderToFront"        mov $0x1008d534,%ecx   default 0
//      0x1002dd01  "ShowTexInfo"          mov $0x1008d554,%ecx   default 0
//      0x1002dcb5  "ShowPortalBounds"     mov $0x1008d594,%ecx   default 0
//      0x1002dc8f  "DrawPortals"          mov $0x1008d5b4,%ecx   default 1.0
//      0x1002dc43  "DrawTerrainSections"  mov $0x1008d604,%ecx   default 0
//      0x1002dc1d  "DrawWorldTree"        mov $0x1008d624,%ecx   default -1.0
//
//  Six names for six anonymous dwords, and each default corroborates the
//  use: DrawWorldTree's -1.0 is why its test is a SIGNED `jl`,
//  DrawPortals' 1.0 is why the portal gate defaults on, and the four
//  zeroes are why the four debug blocks default off.
//
// *********************************************************************** //




// ======================================================================= //
//  THE ENGINE'S RenderStruct, at 0x1008cbcc
//
//  Named g_pStruct in d3dmid.cpp, d3dbatchc.cpp and d3dbatchd.cpp.  Slot
//  offsets from d3d.cpp's map, which reads them off lithtech.exe
//  0x00462110 (the engine side, which writes every callback from a
//  literal).  Only the three members this function touches are named.
// ======================================================================= //

struct FVec3                                            // a bare float x/y/z
{
	float x;
	float y;
	float z;
};

struct RenderStructCallbacks
{
	uint8    m_Reserved00[0x14];                        // +0x00..+0x13
	void   (*ConsolePrintf)(const char* pszFormat, ...);// +0x14
	uint8    m_Reserved18[0x48 - 0x18];                 // +0x18..+0x47
	uint32   m_nCounter48;                              // +0x48
	uint32   m_nCounter4c;                              // +0x4c
	uint8    m_Reserved50[0x130 - 0x50];                // +0x50..+0x12f
	LTVector m_vGlobalLightDir;                         // +0x130
};

extern RenderStructCallbacks* g_pStruct;                // 0x1008cbcc


// ======================================================================= //
//  THE D3D7 DEVICE AT 0x1008d988 AND THE THREE DIRECTDRAW SURFACES
//
//  +0x20 IDirect3DDevice7::SetRenderTarget (index 8) and +0x50
//  SetRenderState (index 20) -- the latter fixed in seven files.
//  +0x0c IDirectDrawSurface7::AddAttachedSurface (index 3) and +0x20
//  DeleteAttachedSurface (index 8); d3ddraw.cpp fixes +0x6c on the same
//  vtable as Restore (index 27).
// ======================================================================= //

struct DDSurface;

struct DDSurfaceVtbl
{
	uint8 m_pad_00[0x0c];
	// +0x0c -- IDirectDrawSurface7::AddAttachedSurface (index 3), ret 8.
	long (__stdcall* AddAttachedSurface)(DDSurface*, DDSurface* pAttach);
	uint8 m_pad_10[0x20 - 0x10];
	// +0x20 -- IDirectDrawSurface7::DeleteAttachedSurface (index 8),
	//          ret 0xc.
	long (__stdcall* DeleteAttachedSurface)(DDSurface*, uint32 dwFlags,
	                                        DDSurface* pAttach);
};

struct DDSurface { DDSurfaceVtbl* lpVtbl; };

// (D3DDevice7 is the one at the top of this file; SetRenderTarget added there.)

extern DDSurface*  g_pPrimarySurface;                   // 0x1008bf6c
extern DDSurface*  g_pBackSurface;                      // 0x1008bf68
extern DDSurface*  g_pZBuffer;                          // 0x1008bf64

struct DDraw7;
extern DDraw7*     g_pDirectDraw;                       // 0x1008bf74

// D3DRENDERSTATETYPE 8 with values 2 and 3.  The state number and the two
// values are the whole of the evidence for the name pair; 8 is FILLMODE and
// D3DFILLMODE is { POINT = 1, WIREFRAME = 2, SOLID = 3 }.
#define D3DRENDERSTATE_FILLMODE   8
#define D3DFILL_WIREFRAME         2
#define D3DFILL_SOLID             3


// ======================================================================= //
//  THE CONSOLE-VARIABLE RECORD (big21f90.cpp's decode, verbatim) AND THE
//  SIX RECORDS OF THE SECOND INITIALISER THAT THIS FUNCTION READS.
//
//  Only +0x00 is used here.  Declared as objects, not as a class with
//  members, so that `g_varX.m_nValue00` compiles to `mov 0xADDR,%reg` --
//  a folded immediate, which is what shipped has.
// ======================================================================= //

class CD3DConVar
{
public:
	int32       m_nValue00;                             // +0x00 value as int
	float       m_fValue04;                             // +0x04 value as float
	float       m_fDefault08;                           // +0x08 the default
	int32*      m_pnValue0c;                            // +0x0c int32 dest
	float*      m_pfValue10;                            // +0x10 float dest
	const char* m_pszName14;                            // +0x14 the name
	void*       m_pHandle18;                            // +0x18 console handle
	CD3DConVar* m_pNext1c;                              // +0x1c chain
};

extern CD3DConVar g_varRenderToFront;                   // 0x1008d534
extern CD3DConVar g_varShowTexInfo;                     // 0x1008d554
extern CD3DConVar g_varShowPortalBounds;                // 0x1008d594
extern CD3DConVar g_varDrawPortals;                     // 0x1008d5b4
extern CD3DConVar g_varDrawTerrainSections;             // 0x1008d604
extern CD3DConVar g_varDrawWorldTree;                   // 0x1008d624

// The write-through destinations of the FIRST initialiser (big21f90.cpp's
// 86 records), which is why these are plain scalars and the six above are
// not.  Every name here is big21f90.cpp's.
extern int32 g_CV_Saturate;                             // 0x1008cb98
extern int32 g_CV_ShowPolyCounts;                       // 0x1008caa8
extern int32 g_CV_Wireframe;                            // 0x1008cab4
extern int32 g_CV_ShowTextureCounts;                    // 0x1008cabc
extern int32 g_CV_ShowFillInfo;                         // 0x1008cacc
extern int32 g_CV_ModelProfile;                         // 0x1008ca38
extern float g_CV_LightSaturate;                        // 0x1008cb94
extern float g_CV_WarbleScale;                          // 0x1008cb7c
extern float g_CV_WarbleSpeed;                          // 0x1008cb80
extern float g_CV_EnvScale;                             // 0x1008cb6c
extern float g_CV_EnvPanSpeed;                          // 0x100893a0  (0.0005)


// ======================================================================= //
//  THE SCENE DESCRIPTION
//
//  ~0x160 bytes, built on the caller's stack by lithtech.exe's
//  RenderCamera (0x00411700) and mapped offset by offset in
//  src/engine/lithtech/clientblit.cpp.  Only the two fields this function
//  reads are named; everything else is padding so the two offsets cannot
//  drift.
// ======================================================================= //

struct SceneDesc
{
	uint8    m_Reserved00[0x68];                        // +0x00..+0x67
	// +0x68.  clientblit.cpp's map of the caller records this slot as
	// `uint32 pClientMgr+0x1258`; it is read here with `flds`, so it is a
	// float.  It scales the warble clock, so a frame time is the natural
	// reading -- not pinned by either side.
	float    m_f68;
	uint8    m_Reserved6c[0xc0 - 0x6c];                 // +0x6c..+0xbf
	// +0xc0 -- clientblit.cpp: "LTVector pCamera+0x18c (m_Pos)".
	FVec3    m_vPosition;
	uint8    m_Reservedcc[0x160 - 0xcc];                // +0xcc..+0x15f
};

// 0x1008beb0.  0x1001eb90 stores its FIRST argument -- our pScene -- here
// (`mov %ebx,0x1008beb0` at 0x1001ebc9), so from the setup call onwards
// this global and the parameter are the same pointer.  This function reads
// m_f68 through the GLOBAL and m_vPosition through the PARAMETER; both
// spellings are in the shipped bytes and neither is a choice.
// d3dr3b.cpp models the same address as `Ramp1001e660* g_pRamp1008beb0`
// with an m_vLo/m_vHi pair, which is consistent: clientblit.cpp's map puts
// LTVectors at +0x38 and +0x44.
extern SceneDesc* g_pSceneDesc;                         // 0x1008beb0


// ======================================================================= //
//  THE OUT-OF-LINE CALLEES
// ======================================================================= //

// 0x10021d60 -- the renderer's level-plus-format warning.  Declared as
// d3dr3c.cpp / d3dr5b.cpp / d3dr5d.cpp declare it; the three sites here
// push two, three and two dwords, so it must be varargs.
extern void sub_10021d60(uint32 nLevel, const char* pszMsg, ...);  // 0x10021d60 AddDebugMessage (LL: was d3d_Warning*; icl treats a callee named *Warn*/*Error* as cold)

// 0x10032990 -- d3dneutral.cpp: `return g_1008d7e4 > 0;`, 14 bytes.
extern LTBOOL sub_10032990();

// 0x1006c510 -- d3dr3b.cpp: the 64-entry saturation table.  ONE float
// argument, pushed as a dword out of 0x1008cb94.
extern void sub_1006c510(float fSaturate);

// 0x1001eb90 -- d3dr3d.cpp, 925 bytes, one caller.  The per-frame setup:
// three stack arguments (0x8/0xc/0x10(%ebp) into %ebx/%edi/%esi), answers
// non-zero on success.
extern LTBOOL sub_1001eb90(SceneDesc* pScene, void* pMem, uint32 nMemSize);

// 0x1002b0e0 -- d3dr3d.cpp: "Recompute two colour-conversion scales and
// pick the pair of pixel routines the current surface format wants."  No
// arguments, no return value.
extern void sub_1002b0e0();

// 0x10029894 -- big26728.cpp: "register ABI, one argument in %eax; the
// world-draw entry point."  Declared __cdecl here; a register-ABI call
// is decoded as an ordinary call and noted.
// (0x10029894 is sub_10029890's register entry, defined above.)

// 0x1002b0c0 -- big299a8.cpp, 24 bytes, reachable ONLY as a function
// pointer, body `sub_10026728(g_p1008beb0, sub_1007b8d0())`: the mirror
// pass.  This function is what installs it.
// (sub_1002b0c0 is defined above.)

// 0x1007b8d0 -- `mov $0x10091a10,%eax ; ret`.  The renderer's root
// context; only its +0x140 is read here (the portal count).
// (sub_1007b8d0 and its Mgr1007b8d0 -- +0x140 added -- are declared above.)

// 0x1002d0b0 -- d3dr3c.cpp, DECODED there: ShowTexInfo, asks the device
// for D3DDEVINFO_TEXTUREMANAGER and prints ten of its eleven dwords.
extern void sub_1002d0b0();

// 0x1002cf48 -- 276 bytes, register ABI (%eax, %edx).  Reads the device
// viewport through +0x54, sets a render state through +0x50, clears a
// texture stage through +0x8c, then walks a +0x58-counted list.  Called
// only from here.
struct Ctx1008beac;
// (0x1002cf48 is sub_1002cf40's register entry, defined above.)

// 0x1002e350 -- defined below, after the world's layout.  RenderScene
// calls it through this declaration, which nothing in this file defines:
// see the note at the definition.
extern void sub_1002e350(Ctx1008beac* pWorld);

// 0x100145f0 -- d3dqip3.cpp: "a bare `c3`, an empty function".
extern void sub_100145f0();

// 0x10015d60 -- d3dr3a.cpp, DECODED there: "BUILDS A 32-ENTRY WAVE TABLE
// AND ITS FORWARD DIFFERENCES", one caller, 0x1002d971.
extern void sub_10015d60();

// 0x1008beac -- the world (d3dr3a.cpp's `S1001e520Root*`, d3dr3b.cpp's
// `Box1001e660*`).  +0x6c is handed to sub_1002cf48 and the pointer itself
// to sub_1002e350; the layout is 0x1002e350's.
struct TerrainSection
{
	uint8   m_pad_00[0x48];                             // +0x00
	uint8   m_Key48[0xc0 - 0x48];                       // +0x48
};                                                      // 0xc0

struct TerrainBox
{
	uint8   m_pad_00[0x18];                             // +0x00
	float   m_vMin[3];                                  // +0x18
	float   m_vMax[3];                                  // +0x24
};

class TerrainModel
{
public:
	virtual int32   IsHidden();                         // slot 0

	uint8           m_pad_04[0x13c - 4];                // +0x04
	TerrainSection* m_pSections;                        // +0x13c
	uint32          m_nSections;                        // +0x140
};

struct TerrainModelRef
{
	uint32          m_n00;                              // +0x00
	TerrainModel*   m_pModel;                           // +0x04
};

class TerrainLookup
{
public:
	virtual TerrainBox* Find(void* pKey);               // slot 0
};

struct Ctx1008beac
{
	uint8               m_pad_00[0x6c];                 // +0x00
	TerrainLookup       m_Lookup;                       // +0x6c
	uint8               m_pad_70[0x18c - 0x70];         // +0x70
	TerrainModelRef**   m_ppModels;                     // +0x18c
	uint32              m_nModels;                      // +0x190
};

extern Ctx1008beac* g_p1008beac;                        // 0x1008beac

// ======================================================================= //
//  0x1002e350 -- DrawTerrainSections (moved here from w4_10023fb0.cpp):
//  with ZENABLE (render state 7) saved and forced on and the current
//  stage's texture dropped, walk the world's models (+0x18c/+0x190); for
//  each one whose slot-0 virtual answers zero, look every 0xc0-byte
//  section (+0x13c/+0x140) up in the world's +0x6c object (slot-0
//  virtual, keyed by &section->+0x48) and draw the box it answers (its
//  +0x18 and +0x24 vectors) through 0x1002dd60.  Then put state 7 back.
//
//  IDENTICAL here.  The shipped code calls the box drawer at its /Qip
//  REGISTER entry 0x1002dd6c, so it shares this TU with it; but its own
//  entry has no stack shim, and RenderScene (above) calls it on the stack.
//  Defined under the name RenderScene calls, icl gives it a /Qip pair and
//  RenderScene a register call.  So it takes its world as World2e350 (the
//  same layout): its own mangled name, which nothing in this file calls,
//  and the link's alias (link/aliases.rsp) resolves RenderScene's
//  ?sub_1002e350@@YAXPAUCtx1008beac@@@Z to it.
// ======================================================================= //
struct World2e350 : public Ctx1008beac
{
};

class CRenderStateSet2e350
{
public:
	CRenderStateSet2e350(uint32 nState, uint32 nValue)
	{
		m_nState = nState;
		g_pD3DDevice->lpVtbl->GetRenderState(g_pD3DDevice, nState, &m_nOld);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, m_nState, nValue);
	}

	~CRenderStateSet2e350()
	{
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, m_nState, m_nOld);
	}

	uint32  m_nState;                                   // +0x00
	uint32  m_nOld;                                     // +0x04
};

void sub_1002e350(World2e350* pWorld)
{
	uint32          nStage;
	uint32          i, j;
	TerrainModel*   pModel;
	TerrainSection* pSect;
	TerrainBox*     pBox;

	CRenderStateSet2e350 ssZ(7, 1);                     // D3DRENDERSTATE_ZENABLE

	nStage = g_1008d7d0;
	if (g_a1008e684[nStage])
	{
		g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, nStage, LTNULL);
		g_a1008e684[nStage] = LTNULL;
	}

	for (i = 0; i < pWorld->m_nModels; i++)
	{
		pModel = pWorld->m_ppModels[i]->m_pModel;
		if (pModel->IsHidden())
			continue;

		for (j = 0; j < pModel->m_nSections; j++)
		{
			pSect = &pModel->m_pSections[j];
			pBox = pWorld->m_Lookup.Find(pSect->m_Key48);
			if (pBox)
				sub_1002dd60(pBox->m_vMin, pBox->m_vMax,
				             (uint32)pSect + (uint32)pModel);
		}
	}

}




// ======================================================================= //
//  THE MODULE-SCOPE STATE THIS FUNCTION TOUCHES
//
//  Every one carries its shipped address.  Where another decoded file has
//  already named an address, that file's name is used unchanged.
// ======================================================================= //

extern LTBOOL g_bIn3D;                                  // 0x1008d7c8  (d3ddraw)
extern LTBOOL g_bInOptimized2D;                         // 0x1008d7c4  (d3dmisc)

// The remembered RenderToFront setting.  Written only at 0x1002d2a7 and
// read only at 0x1002d22e; no string names it.
extern int32  g_nRenderToFrontLast;                     // 0x1008ce84

// 0x1008d958.  big64cc0.cpp calls it "the lightmap-animation gate.  Only
// ever tested."  This function is its only writer and computes it as
// `DrawPortals && sub_10032990()`, which reads more like "portals are
// live"; both readings are recorded and neither is settled.
extern uint32 g_bPortalsLive;                           // 0x1008d958

// The saturation table's cache pair.  0x1008cb8c sits between
// big21f90.cpp's FilterOptimized destination (0x1008cb88) and its
// LightSaturate destination (0x1008cb94), so neither of the two is a
// console variable -- they are this function's own memo.
extern int32  g_bSatTableValid;                         // 0x1008cb8c
extern float  g_fSatTableLast;                          // 0x1008cb90

// The warble table's cache pair, the same shape (0x1008ca90/94 sit just
// below big21f90.cpp's Bilinear destination at 0x1008ca98).  0x10021810
// also writes the valid flag -- a "everything is stale" reset.
extern int32  g_bWarbleTableValid;                      // 0x1008ca94
extern float  g_fWarbleScaleLast;                       // 0x1008ca90

// The environment-map pan, four adjacent floats referenced NOWHERE else in
// .text.  The field roles are read off the arithmetic: two offsets built
// from the camera's x and z, and one scale stored twice.
extern float  g_fEnvPanU;                               // 0x1008b4ec
extern float  g_fEnvPanV;                               // 0x1008b4f0
extern float  g_fEnvScaleU;                             // 0x1008b4f4
extern float  g_fEnvScaleV;                             // 0x1008b4f8

// The negated global light direction, three adjacent floats.
extern FVec3  g_vNegLightDir;                           // 0x1008ceb0

// The two model-clip counters big17c14.cpp names g_n1008cea4 /
// g_n1008cea8; 0x100193f2 bumps the second.  Their roles come from the
// format string printed at 0x1002d9e8.
extern uint32 g_nModelsClipped;                         // 0x1008cea8
extern uint32 g_nModelsUnclipped;                       // 0x1008cea4

// The light-colour scale and its cache.  0x1001eb90 fills the first
// (`mov $0x1008be80,%edi` at 0x1001ec3d, then three `fmuls` off it); the
// next function initialises the second to 5.0f per component, which is
// what forces the first frame's rebuild.
extern FVec3  g_vLightScale;                            // 0x1008be80
extern FVec3  g_vLightScaleLast;                        // 0x1008d528

// The two 3x256 colour ramps.  big3e620.cpp: "the three 256-entry ramps at
// 0x1008cebc / 0x1008cfbc / 0x1008d0bc"; d3dhigh.cpp: "0x1008d1bc,
// 0x1008d2bc, 0x1008d3bc -- three 256-entry byte ramps in BSS".  Declared
// as [3][256] because the shipped code reaches channels 1 and 2 as
// base + 0x100 / base + 0x200 rather than as folded immediates.
extern uint8  g_ColorRamp[3][256];                      // 0x1008cebc
extern uint8  g_ColorRamp2[3][256];                     // 0x1008d1bc

// Four per-frame counters zeroed together.  bigb910.cpp names 0x1008d520
// "the shared draw-vertex buffer['s]" count; d3dr5b.cpp declares
// 0x1008d4bc as a uint32.  The other two are by address.
extern uint32 g_nDrawVertCount;                         // 0x1008d520
extern uint32 g_n1008d4bc;                              // 0x1008d4bc
extern uint32 g_n1008d4c0;                              // 0x1008d4c0
extern uint32 g_n1008d4c4;                              // 0x1008d4c4

// The mirror-pass hook.  big26728.cpp declares it `uint32 g_1008ef80` and
// records `g_1008ef80 = 0` at the head of the mirror pass; a function
// pointer is what makes `movl $0x1002b0c0,mem` the natural encoding.
extern void (*g_pfnMirrorHook)();                       // 0x1008ef80

// The frame counter.  0x100263b3 zeroes the same dword.
extern uint32 g_nFrameCount;                            // 0x1008ce90

// The warble clock and its fractional part.  0x10015a63 reads the clock and
// 0x10015abb multiplies by the fraction, so the two really are consumed by
// the wave-table code; 0x1001c26e/0x1001c273 zero both.
extern float  g_fWarbleTime;                            // 0x1008b750
extern float  g_fWarbleFrac;                            // 0x1008b74c

// The statistics counters, every one named by the format string this
// function prints it with.  All int32 (`%d`), all written elsewhere;
// 0x1001eb90's tail zeroes 0x1008be44..0x1008be5c.
extern int32  g_nTextureChanges;                        // 0x1008bd80
extern int32  g_nTextureUploads;                        // 0x1008bd7c
extern int32  g_nWorldPoliesProcessed;                  // 0x1008be58
extern int32  g_nWorldPoliesDrawn;                      // 0x1008be5c
extern int32  g_nLightTests;                            // 0x1008be48
extern int32  g_nLitPolies;                             // 0x1008bf10
extern int32  g_nLitPoliesUploaded;                     // 0x1008be28
extern int32  g_nModelTriangles;                        // 0x1008be50
extern int32  g_nParticlesDrawn;                        // 0x1008be4c
extern int32  g_nClipTests;                             // 0x1008be44
extern int32  g_nVisibleLeaves;                         // 0x1008be40
extern int32  g_nSkyPortals;                            // 0x1008be3c
extern int32  g_nSkyPolies;                             // 0x1008be38
extern int32  g_nVisibleLights;                         // 0x1008bd84
extern int32  g_nRejectedLights;                        // 0x1008bd78
extern int32  g_nTextureUploadSaves;                    // 0x1008be2c
extern int32  g_nTriangles;                             // 0x1008be34
extern float  g_fTriAreaDrawn;                          // 0x1008be30

// The viewport rectangle whose area is the Overdraw denominator.  All the
// bytes prove is (0x1008b8a4 - 0x1008b89c) x (0x1008b8a8 - 0x1008b8a0);
// {left, top, right, bottom} is the only assignment of the four slots that
// makes both differences positive extents.  d3dmisc.cpp names 0x1008b8a8
// `g_nCounterA8`, so the naming is open and the arithmetic is not.
extern int32  g_nViewLeft;                              // 0x1008b89c
extern int32  g_nViewTop;                               // 0x1008b8a0
extern int32  g_nViewRight;                             // 0x1008b8a4
extern int32  g_nViewBottom;                            // 0x1008b8a8


// ======================================================================= //
//  TWO MACROS
//
//  Both are macros rather than helper functions for the same reason: a
//  helper with nine (resp. three) call sites is the one shape that could
//  turn into ours-only rows in the call check if icl declined to inline
//  it, and nothing in the shipped bytes distinguishes a macro from an
//  inlined helper.  See COIN-FLIP 3.
// ======================================================================= //

//  One colour ramp: 256 bytes of a 16.16 accumulator, clamped at 255.
//  `shr` and `jbe` in the shipped loop make the accumulator UNSIGNED, and
//  the store is of the value BEFORE the increment.
//  The store walks the pointer (`*pRamp++`), not
//  `pRamp[i]` -- the shipped loops are `mov %dl,(%ebx) ; inc %ebx ; ... ;
//  dec %ecx ; jne` with the index gone; with that 0x1002d1c0 comes out
//  2,288 bytes (= shipped) and pairs.
#define D3D_BUILD_RAMP(pDest, nStep)                                    \
	{                                                                   \
		uint8* pRamp = (pDest);                                         \
		uint32 nAccum = 0;                                              \
		int32  i;                                                       \
                                                                        \
		i = 256;                                                        \
		while (i--)                                                     \
		{                                                               \
			*pRamp++ = (uint8)(nAccum >> 16);                           \
			nAccum += (nStep);                                          \
                                                                        \
			if (nAccum > (255 << 16))                                   \
				nAccum = (255 << 16);                                   \
		}                                                               \
	}

//  |a - b| for two floats, written as the conditional EXPRESSION the
//  shipped code emits: one `fcom` without a pop, then the subtraction in
//  whichever order is positive, then ONE comparison against the epsilon.
//  Two separate `if`s would need two epsilon literals per component and
//  the pool has exactly one each.
//  The test is written `a < b` -- `b > a` puts the
//  compare's operands the other way round (`fcom` of the scale against
//  the last, `jae`), the shipped code compares the last against the scale
//  (`jbe`).
#define D3D_FDIFF(a, b)   (((a) < (b)) ? ((b) - (a)) : ((a) - (b)))

//  255 << 16 is the ramp clamp; 1 << 16 and 2 << 16 are the two steps.
#define D3D_RAMP_ONE      0x10000
#define D3D_RAMP_DOUBLE   0x20000


// ======================================================================= //
//  0x1002d1c0 -- RenderStruct slot +0xb0, `RenderScene(SceneDesc*)`.
//  2,288 bytes.  Always answers LTFALSE; the caller ignores the answer.
// ======================================================================= //

LTBOOL d3d_RenderScene(SceneDesc* pScene)
{
	// 20,480 bytes, and both the pointer and the size are handed to the
	// setup call together at 0x1002d3a5/0x1002d3ab, so neither is a guess.
	// An aggregate, so it sits BELOW the three scalar slots -- which is
	// exactly where the shipped frame puts it (-0x5028, with -0x8, -0x10
	// and -0x28 above it).
	uint8 nSetupBuffer[0x5000];

	// Five failures, ONE shared exit hosted at the end of the function
	// (0x1002da30).  One `||` hosts the shared block last; a run
	// of separate guarded returns would host it at the last test instead.
	if (!pScene || !g_pPrimarySurface || !g_pD3DDevice || !g_pDirectDraw ||
	    !g_bIn3D)
	{
		return LTFALSE;
	}

	// The POSITIVE guard, so that the warning and its epilogue are hosted
	// after the body -- which is where 0x1002da05 is.  See COIN-FLIP 2.
	if (!g_bInOptimized2D)
	{
		// --------------------------------------------------------------- //
		//  0x1002d228 -- swap the render targets when RenderToFront moves.
		// --------------------------------------------------------------- //
		if (g_varRenderToFront.m_nValue00 != g_nRenderToFrontLast)
		{
			DDSurface* pOldBack = g_pBackSurface;

			g_pBackSurface    = g_pPrimarySurface;
			g_pPrimarySurface = pOldBack;

			g_pBackSurface->lpVtbl->AddAttachedSurface(g_pBackSurface,
			                                           g_pZBuffer);
			g_pPrimarySurface->lpVtbl->DeleteAttachedSurface(
			                              g_pPrimarySurface, 0, g_pZBuffer);
			g_pD3DDevice->lpVtbl->SetRenderTarget(g_pD3DDevice,
			                                      g_pBackSurface, 0);

			g_nRenderToFrontLast = g_varRenderToFront.m_nValue00;
		}

		// --------------------------------------------------------------- //
		//  0x1002d2ac -- the portal gate.  Three blocks merging on one
		//  store, with `mov $0x1,%eax` in the true arm: `&&` yielding 0/1.
		// --------------------------------------------------------------- //
		g_bPortalsLive = g_varDrawPortals.m_nValue00 && sub_10032990();

		// --------------------------------------------------------------- //
		//  0x1002d2cb -- rebuild the saturation table if it went stale.
		// --------------------------------------------------------------- //
		if (!g_bSatTableValid || g_CV_LightSaturate != g_fSatTableLast)
		{
			sub_1006c510(g_CV_LightSaturate);

			g_fSatTableLast  = g_CV_LightSaturate;
			g_bSatTableValid = 1;
		}

		// --------------------------------------------------------------- //
		//  0x1002d314 -- the environment-map pan.  Statement order is the
		//  literal pool's, not the instruction stream's: icl hoists the
		//  divide to the top of the block.
		// --------------------------------------------------------------- //
		g_fEnvPanU = pScene->m_vPosition.x * g_CV_EnvPanSpeed + 0.5f;
		g_fEnvPanV = pScene->m_vPosition.z * g_CV_EnvPanSpeed + 0.5f;

		{
			// ONE expression, TWO lvalues: the shipped code stores with
			// `fsts` (no pop) and then `fstps` off one %st(0).  See
			// COIN-FLIP 4 for the `* 1.0f`, which is a live `fmuls` of the
			// pool's 0x3f800000 at 0x100a61b0.
			float fEnvScale = 0.5f / g_CV_EnvScale * 1.0f;

			g_fEnvScaleU = fEnvScale;
			g_fEnvScaleV = fEnvScale;
		}

		g_nModelsClipped   = 0;
		g_nModelsUnclipped = 0;

		// Three `flds`+`fchs` then three `fstps` in the shipped code, i.e.
		// grouped, which is a vector-valued expression; written as three
		// component assignments here for the reason in COIN-FLIP 5.
		// The SDK vector's unary minus into a temporary,
		// then the three stores -- the shipped code loads and negates all
		// three (z, x, y) before storing any, as the by-value temporary
		// does; three member-wise negations store each as it goes.
		{
			LTVector vNeg = -g_pStruct->m_vGlobalLightDir;

			g_vNegLightDir.x = vNeg.x;
			g_vNegLightDir.y = vNeg.y;
			g_vNegLightDir.z = vNeg.z;
		}

		// --------------------------------------------------------------- //
		//  0x1002d3a2 -- the frame setup.  Everything down to the
		//  ShowTexInfo call is inside this `if`; its `je` target
		//  (0x1002d8fc) is the same one ShowTexInfo's test falls through
		//  to, which is what fixes the extent of the body.
		// --------------------------------------------------------------- //
		if (sub_1001eb90(pScene, nSetupBuffer, 0x5000))
		{
			sub_1002b0e0();

			// ----------------------------------------------------------- //
			//  0x1002d3cf -- rebuild both colour ramps when the light
			//  scale has moved by more than the epsilon in ANY component.
			//  Only the first term stays in the main flow; the second and
			//  third are cold blocks sunk to 0x1002da54 / 0x1002da78, and
			//  the pool's literal order records that.
			// ----------------------------------------------------------- //
			if (D3D_FDIFF(g_vLightScale.x, g_vLightScaleLast.x) > 0.001f ||
			    D3D_FDIFF(g_vLightScale.y, g_vLightScaleLast.y) > 0.001f ||
			    D3D_FDIFF(g_vLightScale.z, g_vLightScaleLast.z) > 0.001f)
			{
				// Written saturate-first; as `if (!g_CV_Saturate)`
				// icl placed the DOUBLE ramps in line, the shipped code the ONE.
				if (g_CV_Saturate)
				{
					D3D_BUILD_RAMP(g_ColorRamp[0], D3D_RAMP_DOUBLE);
					D3D_BUILD_RAMP(g_ColorRamp[1], D3D_RAMP_DOUBLE);
					D3D_BUILD_RAMP(g_ColorRamp[2], D3D_RAMP_DOUBLE);
				}
				else
				{
					D3D_BUILD_RAMP(g_ColorRamp[0], D3D_RAMP_ONE);
					D3D_BUILD_RAMP(g_ColorRamp[1], D3D_RAMP_ONE);
					D3D_BUILD_RAMP(g_ColorRamp[2], D3D_RAMP_ONE);
				}

				D3D_BUILD_RAMP(g_ColorRamp2[0], D3D_RAMP_ONE);
				D3D_BUILD_RAMP(g_ColorRamp2[1], D3D_RAMP_ONE);
				D3D_BUILD_RAMP(g_ColorRamp2[2], D3D_RAMP_ONE);

				// 0x1002d5ec: three dwords through three integer
				// registers, then three stores -- a POD struct copy.
				g_vLightScaleLast = g_vLightScale;
			}

			// 0x1002d60e.  g_pStruct is loaded TWICE for the last two
			// stores, so the source names the member expressions rather
			// than caching the pointer in a local.
			g_nDrawVertCount = 0;
			g_n1008d4bc      = 0;
			g_n1008d4c0      = 0;
			g_n1008d4c4      = 0;

			g_pStruct->m_nCounter4c = 0;
			g_pStruct->m_nCounter48 = 0;

			// 0x1002d636.  Both arms materialise into %edx and merge, so
			// this is a conditional expression, not an if/else round the
			// call.  The device is read BEFORE the
			// condition (`mov 0x1008d988,%eax` between the test and the
			// `je`), as a C++ call's object expression is; through the
			// lpVtbl struct that takes the device in a local first.
			{
				D3DDevice7* pDev = g_pD3DDevice;

				pDev->lpVtbl->SetRenderState(pDev, D3DRENDERSTATE_FILLMODE,
					g_CV_Wireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
			}

			// 0x1002d667 -- the world draw, with the mirror pass hooked in
			// around it.
			if (!g_varShowPortalBounds.m_nValue00)
			{
				g_pfnMirrorHook = sub_1002b0c0;

				sub_10029890((Obj10029890*)pScene);

				g_pfnMirrorHook = LTNULL;
			}

			g_pD3DDevice->lpVtbl->SetRenderState(
			        g_pD3DDevice, D3DRENDERSTATE_FILLMODE, D3DFILL_SOLID);

			// ----------------------------------------------------------- //
			//  0x1002d6b1 -- the four debug blocks.
			// ----------------------------------------------------------- //
			if (g_CV_ShowTextureCounts)
			{
				sub_10021d60(0, "Texture changes: %d", g_nTextureChanges);
				sub_10021d60(0, "Texture uploads: %d", g_nTextureUploads);
			}

			if (g_CV_ShowPolyCounts)
			{
				g_pStruct->ConsolePrintf("World Polies Processed: %d",
				                         g_nWorldPoliesProcessed);
				g_pStruct->ConsolePrintf("World Polies Drawn: %d",
				                         g_nWorldPoliesDrawn);
				g_pStruct->ConsolePrintf("Num Light Tests: %d",
				                         g_nLightTests);
				g_pStruct->ConsolePrintf(
				        "Num Lit Polies: %d (textured and uploaded: %d)",
				        g_nLitPolies, g_nLitPoliesUploaded);
				g_pStruct->ConsolePrintf("Model triangles drawn: %d",
				                         g_nModelTriangles);
				g_pStruct->ConsolePrintf("Particles drawn: %d",
				                         g_nParticlesDrawn);
				g_pStruct->ConsolePrintf("Num Clip Tests: %d",
				                         g_nClipTests);
				g_pStruct->ConsolePrintf("Visible Leaves: %d",
				                         g_nVisibleLeaves);
				g_pStruct->ConsolePrintf("Sky Portals: %d", g_nSkyPortals);
				g_pStruct->ConsolePrintf("Sky Polies (fragments): %d",
				                         g_nSkyPolies);
				g_pStruct->ConsolePrintf("Portals: %d",
				                         sub_1007b8d0()->m_nPortals140);
				g_pStruct->ConsolePrintf("Visible lights: %d, rejected: %d",
				                         g_nVisibleLights,
				                         g_nRejectedLights);
				g_pStruct->ConsolePrintf("Texture upload saves: %d",
				                         g_nTextureUploadSaves);
				g_pStruct->ConsolePrintf("Triangles: %d", g_nTriangles);
			}

			if (g_CV_ShowFillInfo)
			{
				g_pStruct->ConsolePrintf("Tri area drawn: %.3f",
				                         g_fTriAreaDrawn);
				g_pStruct->ConsolePrintf(
				        "Overdraw: %.3f",
				        g_fTriAreaDrawn /
				            (float)((g_nViewRight - g_nViewLeft) *
				                    (g_nViewBottom - g_nViewTop)));
			}

			if (g_varShowTexInfo.m_nValue00)
				sub_1002d0b0();
		}

		// --------------------------------------------------------------- //
		//  0x1002d8fc -- outside the setup `if` again.
		// --------------------------------------------------------------- //

		// SIGNED, because the registered default is -1.0f: "no node
		// selected".  g_p1008beac is named twice, and the shipped code
		// loads it twice.
		if (g_varDrawWorldTree.m_nValue00 >= 0 && g_p1008beac)
		{
			sub_1002cf40((Obj1002cf40*)&g_p1008beac->m_Lookup,
			             g_varDrawWorldTree.m_nValue00);
		}

		// The shipped build SINKS this arm to 0x1002da41.  See COIN-FLIP 1
		// for the spelling that reproduces the sink and what each predicts.
		if (g_varDrawTerrainSections.m_nValue00 && g_p1008beac)
			sub_1002e350(g_p1008beac);

		sub_100145f0();

		// --------------------------------------------------------------- //
		//  0x1002d93d -- rebuild the warble wave table if it went stale.
		//  Same shape as the saturation cache above; the store order
		//  inside the block is the opposite way round and is taken from
		//  the bytes rather than made uniform.
		// --------------------------------------------------------------- //
		if (!g_bWarbleTableValid || g_CV_WarbleScale != g_fWarbleScaleLast)
		{
			sub_10021d60(9, "Rebuilding warble table");

			sub_10015d60();

			g_bWarbleTableValid = 1;
			g_fWarbleScaleLast  = g_CV_WarbleScale;
		}

		// 0x1002d98a.  The accumulator is the term written FIRST
		// (a flat sum emits its terms backwards, and the shipped code
		// emits the product then folds the global into a memory `fadds`).
		// The value never leaves %st(0): the `fsts` stores it to the
		// global, `fstl (%esp)` pushes it to floor as a double and
		// `fstpl -0x10(%ebp)` keeps the double copy that `fsubrl`
		// subtracts from.
		g_fWarbleTime = g_fWarbleTime +
		                g_pSceneDesc->m_f68 * g_CV_WarbleSpeed;
		g_fWarbleFrac = (float)(g_fWarbleTime - floor(g_fWarbleTime));

		if (g_CV_ModelProfile)
		{
			g_pStruct->ConsolePrintf(
			        "ModelProfile: %d clipped, %d unclipped",
			        g_nModelsClipped, g_nModelsUnclipped);
		}

		g_nFrameCount++;

		return LTFALSE;
	}

	// 0x1002da05, the sunk block.  The string 0x100a6380 is the FIRST of
	// this function's twenty-one literals in the .data1 string run
	// (0x100a6380, then 0x100a63c0 "Texture changes: %d", ascending and
	// 16-byte aligned throughout), so the statement really is early in the
	// source even though the block is placed last.
	sub_10021d60(0, "Error: tried to render 3D while in optimized 2D mode.");

	return LTFALSE;
}
