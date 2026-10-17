// *********************************************************************** //
//
//  MODULE:   W30_1000F020_OSQIPNI (.CPP)  -- /Os /Qip /Qip_no_inlining
//
//  PURPOSE:  The world lightmap pass and everything that reaches it
//            through a /Qip register entry (from big124a4.cpp,
//            w2_1000f020_os.cpp, w15_1000f650_os.cpp and 0x100123a0 out
//            of w3_10014580_os.cpp):
//
//    0x1000f020, 0x1000f0a0, 0x1000f1b0, 0x1000f270  statics, entered by
//        0x100124a4 in registers (%eax, %edx, %ecx, then the stack slot).
//    0x1000f650  the draw slot; calls 0x100124a4 with the poly in %eax.
//    0x100123a0  the deferred-queue flush; calls 0x100124a4 the same way.
//    0x100124a4  the pass itself, a static (register entry, no shim).
//
//  All six small functions are byte-identical here, and were all near
//  while each file called the others on the stack.  Measured, in order:
//    * one /Os /Qip TU: every call becomes the shipped register call, and
//      /Qip_no_inlining keeps the four one-caller helpers out of line as
//      shipped (plain /Qip folds them into 0x100124a4).  /Os by the
//      helpers' `push` after the frame and 0x100123a0 / 0x1000f650 being
//      /Os-exact apart from their calls;
//    * the helpers take their pointers TYPED (a `void*` parameter copied
//      into a typed local schedules the copy ahead of the other argument
//      moves -- 0x1000f020's prologue);
//    * 0x1000f0a0 / 0x1000f1b0 count down a local assigned AFTER the
//      basis call (the shipped code reloads the stack argument into
//      caller-saved %edi / %edx there; counting the parameter itself keeps
//      it in a callee-saved register and spills pObj);
//    * the texture pair is read into two locals then stored, and the
//      pointer steps are written pSrc++, count--, pDest++ (icl emits
//      these adjacent independent statements in its own order; each
//      order was built);
//    * 0x1000f270 returns `(RVert*)base + used`; 0x1000f650 stores the
//      clip flags before the owner, 0x100123a0 reads the next element
//      before storing the flags and clears +0x04 before +0x08.
//
//  0x100124a4 itself is a near: 8347 bytes against 8353 (it was 10611
//  as a lone /O2 file); see the notes above ClipPoly124's helpers.
//
//  ================== THE EXTENT IS NOT WHAT THE ROW SAYS ================ //
//
//  The function enumeration's row says 2,855 bytes ending at
//  0x10012fcb.  That is WRONG, and the enumeration's suspect list already
//  says why: 0x10012fcb is a bogus enumeration row, "falls through from a
//  cmp %edx,%eax in the preceding body".  It is the `je` of
//  `cmp %edx,%eax ; je 0x100132df` at 0x10012fc9 -- the "all vertices
//  inside, skip this plane" test of the SECOND clip plane, four bytes into
//  the middle of a basic block.
//
//  THE REAL FUNCTION IS 0x100124a4 .. 0x10014544 INCLUSIVE -- 8,353 bytes
//  (0x20A1), 2.9x the stated size.  The evidence:
//
//    * 0x100126f2 is `je 0x10014540`, a forward branch out of the very
//      first page into 0x10014540, and 0x10014540 is `jmp 0x1001254a`, a
//      trampoline back into THIS function's epilogue at 0x1001254a.  A
//      branch pair like that cannot cross a function boundary.
//    * between 0x100124a4 and 0x10014545 there are exactly TWO `ret`s
//      (0x10012550 and nothing else) -- the single epilogue
//      `pop %ebx ; pop %esi ; pop %edi ; mov %ebp,%esp ; pop %ebp ; ret`
//      at 0x1001254a -- and no second `push %ebp ; mov %esp,%ebp`.  Every
//      other exit in the 8 KB is a `jmp` or `je` to 0x1001254a.
//    * 0x10014545..0x1001454f is eleven bytes of 0x90 padding and
//      0x10014550 is a real function (43 bytes, d3dr2c.cpp's
//      sub_10014550, which THIS function calls at 0x100141d9).
//    * the three other enumeration rows inside the range -- 0x10012fcb
//      (2130), 0x1001381d (240) and 0x1001390d (3128) -- are all interior
//      branch targets of the six inlined clip planes.  0x1001381d is
//      inside plane 3's colour interpolation and 0x1001390d is inside
//      plane 4's; neither has a prologue.
//
//  THE ACCEPTANCE TEST is the call sequence over 0x100124a4..0x10014545.
//  That range holds 40 calls -- 23 direct to 19
//  DISTINCT callees, and 17 indirect -- at 209 bytes per call.
//
//  ---------------------------------------------------------------------- //
//  WHAT IT IS: the WORLD LIGHTMAP PASS.  Two callers, both handing the
//  object in %eax (see the convention note below): 0x1000f661 (guarded on
//  pObj->m_p2c) and 0x10012435, which walks a deferred queue at 0x1008aec0
//  setting g_1008b874 from each element's +0x10 before every call.
//
//  FIVE INDEPENDENT TELLS:
//    * the render states it saves and overrides at 0x10012839..0x1001296a
//      are 0x1b/0x19/0x13/0x14/0x17 = ALPHABLENDENABLE, ALPHAFUNC,
//      SRCBLEND, DESTBLEND, ZFUNC, set to 1, 8 (D3DCMP_ALWAYS), 9
//      (D3DBLEND_DESTCOLOR) or 1 (D3DBLEND_ZERO), 3 (D3DBLEND_SRCCOLOR)
//      and 3 (D3DCMP_EQUAL).  ZFUNC=EQUAL plus a modulate blend is a
//      SECOND PASS over already-drawn geometry, and the three states it
//      restores at 0x1001433c..0x1001439f are the same three it saved;
//    * the source vertex is 0x28 bytes with TWO texture-coordinate pairs,
//      at +0x18/+0x1c and +0x20/+0x24.  The function copies pair ONE into
//      the 0x20-byte draw vertex (0x10012735), draws, then overwrites u/v
//      with pair TWO (0x10012824) -- base coordinates and lightmap
//      coordinates of a LithTech world vertex;
//    * `if (g_1008ae20) goto done` at 0x100129d0 -- 0x1008ae20 is
//      d3dr5c.cpp's LMFullBright console variable, and a full-bright
//      lightmap pass is exactly the pass you skip;
//    * `if (g_1008ade0) sub_1000ef60(g_p1008beac, pObj)` at 0x100125d2 --
//      0x1008ade0 is LMAnim and d3dlow.cpp has 0x1000ef60 decoded as
//      "if the +0x14 word has bit 15, clear it and rebuild", i.e. refresh
//      an animating lightmap;
//    * the two bucket-list registrations (0x10012772 out of line, and the
//      same code INLINED at 0x100142d2) push onto 0x1008d4c4, the list
//      d3dr2a.cpp records as `if (g_1008d4c4) sub_10011620();` -- the
//      deferred second-pass queue.
//
//  ---------------------------------------------------------------------- //
//  CALLING CONVENTION.  The shipped function takes its ONE argument in
//  %eax (icl /Qip).  The prologue is
//      push %ebp ; mov %esp,%ebp ; push %eax
//      mov $0x1f44,%eax ; call 0x100805a0        <- _chkstk, 8004 bytes
//      mov 0x1f44(%esp),%eax                     <- reload the spilled arg
//  and both call sites agree: 0x1000f65f is `mov %edx,%eax ; call`, and
//  0x10012432 is `add $-8,%esp ; mov (%ebx),%eax ; call` with nothing
//  pushed.  A register-ABI call is decoded as an ordinary
//  call whose argument arrives in %eax; here it is a /Qip static.
//  Same for the four register-ABI CALLEES 0x1000f020, 0x1000f0a0,
//  0x1000f1b0 and 0x1000f270: each takes arg0 in %eax, arg1 in %edx,
//  arg2 in %ecx and any further argument at ITS OWN stack slot (arg3 at
//  0xc(%esp), arg4 at 0x10(%esp)) -- the same layout d3dqip.cpp and
//  big61a30.cpp record.  Their bodies confirm the count: 0x1000f020 reads
//  0x14(%ebp), 0x1000f0a0 reads 0x14(%ebp) and 0x18(%ebp), 0x1000f1b0
//  reads 0x14(%ebp).  Defined below as /Qip statics with the same
//  arguments in the same order.
//
//  ---------------------------------------------------------------------- //
//  COMPILER: Intel C/C++ 4.0.  (The /O2 verdict below was big124a4.cpp's;
//  the TU is /Os now for its register-ABI neighbours, which are /Os-exact.)
//  The /O2 tells:
//    * outgoing arguments are `add $-N,%esp` then `mov %reg,K(%esp)` at
//      every COM call and at every register-ABI call (0x10012506,
//      0x10012557, 0x10012653, 0x10012685, 0x100126e2, 0x10012839, ...);
//    * `&pObj` addresses are built with `mov $K,%reg ; add %p,%reg` --
//      0x10012cb2 `mov $0x20,%eax ; add %edi,%eax`, 0x1001273e, 0x10012ef8
//      -- not `lea 0x20(%edi),%eax`;
//    * every one of the seven early exits jumps to the ONE epilogue at
//      0x1001254a rather than duplicating it -- that is the /Os row, and
//      it is the only /Os-looking thing here.  The `add $-N,%esp` argument
//      idiom is much stronger, so **/O2** is the verdict; the shared
//      epilogue is a `goto`-shaped source (seven `return`s through one
//      tail) rather than a flag.
//    * cold-block sinking is /O2 behaviour and there is a lot of it: the
//      six clip-epsilon arms at 0x100143b9..0x100144a0, the two pool
//      refills at 0x100144a5/0x10014510, the per-triangle path at
//      0x1001414e and the whole tail at 0x1001429b all sit BELOW the last
//      hot block.
//
//  ---------------------------------------------------------------------- //
//  REGION MAP, with what pins each one.
//
//  0x100124a4..0x10012503  PROLOGUE and the env-map gate.  Three tests --
//      pObj->m_p28->m_p30->m_p2c, g_1008ae00 (EnvMapWorld) and g_1008d950
//      (d3dr3b.cpp: the ValidateDevice result) -- and all three must hold.
//      b80 and b88 are initialised TRUE with `xor %dl,%dl ; inc %dl` and
//      `mov $0x1,%dh`, one byte each.
//
//  0x10012503..0x10012551  sub_1000a700(pObj, &pData, &nSrcCount) --
//      d3dbatchd.cpp decoded it as "hands back where a piece of geometry's
//      data is and how many" -- then sub_1000a740(nVerts), which
//      d3dbatchd.cpp decoded as `cmpl $0x80,arg ; jg` i.e. `nVerts > 128`.
//      That 128 is what fixes the local array: the frame is 0x1f44 and the
//      gap from -0x1e28 to -0xa28 is 0x1400 = 128 * 0x28.
//
//  0x10012551..0x100125d2  sub_1000f020 over the array, then the
//      transform 0x10011420 (d3dbatcha.cpp, three arguments and icl's dead
//      fourth) guarded on `g_1008b874 && !bEnvMap`.  Its `setne` answer is
//      `pSrcVerts != aVerts`.
//
//  0x100125d2..0x10012638  LMAnim refresh, then the "already lit this
//      frame" latch on pObj->m_p44->m_n20 and sub_10039d70(pObj, bDraw).
//
//  0x10012638..0x100126f8  the two build calls -- 0x1000f0a0 (five
//      arguments) when the transform moved the array, 0x1000f1b0 (four)
//      when it did not -- then the second 0x10011420 and the vertex
//      reserve 0x1000f270.  d3dr2c.cpp has 0x1000f270 decoded: "argument
//      in %eax, bounds-checks g_1008d520 + n against g_1008d51c, returns
//      g_1008d524 + (g_1008d520 << 5)", a 32-byte-element reserve
//      returning 0 on failure -- which is why the null test at 0x100126ef
//      goes straight to the epilogue.  The matching COMMIT is at
//      0x100143ae, `add %eax,0x1008d520`.
//
//  0x100126f8..0x100129d0  THE TWO-PASS SWITCH, on sub_1000f640()
//      (0x1000f640 is six bytes, `mov 0x1008add0,%eax ; ret`).  When it is
//      set: copy the 0x20-byte vertex with texture pair ONE, register the
//      object on the 0x1008d4c4 queue through 0x1000eef0 (d3dr3c.cpp has
//      that one decoded field for field -- +0x04, +0x08, +0x0c are the
//      caller's, and +0x10 is written here), save/flush/restore the bound
//      texture around sub_10011620, then swap u/v to texture pair TWO and
//      install the five modulate render states.  When it is clear: copy
//      the vertex with pair TWO directly and install nothing.
//
//  0x100129d0..0x10012a5f  LMFullBright, the lightmap texture bind
//      (0x1000efa0, d3dbatchd.cpp: "binds a texture to a stage"; its own
//      body compares pObj->+0x44 with g_1008e684[stage], so +0x44 is the
//      texture and 0x1008d7cc is the stage), and the three-way path
//      choice on g_1008b874 and g_1008d7a0 (UseD3DClip).
//
//  0x10012a65..0x10012b79  THE UNCLIPPED PATH.  A full projective
//      transform through the 4x3+w block at +0x1dc..+0x218 of the
//      transform object, `fdivrs 1.0f` for 1/w, the three results copied
//      to the vertex with `rep movsl $3` and 1/w stored to +0x0c.  The
//      SAME instruction sequence, operand for operand, is the body of
//      0x1000a8a0 at 0x1000a8d0..0x1000a95e -- an independent copy of the
//      identical expression, which is what pins the operand order.
//
//  0x10012b7e..0x100140c9  THE CLIPPED PATH: nVerts calls to 0x1000d9b0
//      (d3dlow.cpp: transform a vector by the 3x4 at +0x15c), then
//      0x1000f4b0 INLINED with all six of its plane bodies inlined too.
//      This is the same 4.2 KB clipper big16324.cpp carries, block for
//      block, with the same 32-byte vertex; every one of the six blocks
//      here was diffed against that decode and against d3dr3a.cpp's
//      out-of-line 0x10002d40 / 0x100030a0, and the only differences are
//      the reject target (a shared tail here, `return` there) and the
//      register allocation.  The six planes, each read off its own
//      classify loop in this image:
//
//        bit 0x01  near    z  >= g_1008b908   0x10012c1d  fcomps/jae
//        bit 0x04  left    -z <  x            0x10012f93  fchs/fcomps/jb
//        bit 0x08  top     y  <  z            0x10013317  fcomps/jb
//        bit 0x10  right   x  <  z            0x10013693  fcomps/jb
//        bit 0x20  bottom  -z <  y            0x10013a0f  fchs/fcomps/jb
//        bit 0x02  far     z  <= g_1008b910   0x10013d93  fcomps/jbe
//
//      -- the same six in the same order as big16324.cpp and as the table
//      d3dr3c.cpp derived from 0x1000f4b0's six guarded call sites.  The
//      epsilon is +/-0.00001f (0x100a4800/0x100a4820/... = 0x3727c5ac) and
//      the degenerate answer is 0.0f (0x100a4804/0x100a4824/...), six
//      private copies of each, one per inlined plane.
//
//      NOTE that with g_1008d7a0 (UseD3DClip) set and the near bit clear,
//      `nClipFlags &= 1` makes nClipFlags zero and the six planes are
//      SKIPPED at run time -- and with it clear, or with the near bit set,
//      control never reaches them either, because 0x10012a3e sends that
//      case to the per-triangle path.  The six planes are dead code in the
//      shipped binary.  They are here because 0x1000f4b0 was inlined
//      whole, and they are 4.2 KB of the 8.3.
//
//  0x100140e1..0x10014149  THE DRAW: nVerts calls to 0x10014580 (the
//      screen projection, +0x31c..+0x330, decoded in full at 0x10014580)
//      then DrawPrimitive(6, 0x1c4, pVerts, nVerts, 0) through vtable
//      +0x64.  6 is D3DPT_TRIANGLEFAN and 0x1c4 is
//      XYZRHW|DIFFUSE|SPECULAR|TEX1, which is exactly the 32-byte vertex.
//
//  0x1001414e..0x1001424b  THE PER-TRIANGLE PATH: the fan is walked as
//      (v0, vi, vi+1), each triangle copied into a three-vertex local
//      array with three `rep movsl $8`, handed to sub_10014550 (d3dr2c.cpp
//      decoded it: a __cdecl forwarder to 0x1000f2c0 appending a fourth
//      argument of 0) and drawn if it survives.
//
//  0x1001424d..0x10014296  the two remaining exits: the direct
//      DrawPrimitive when the env-map gate is clear, and
//      sub_10024020(pObj) when the texture bind fails.
//
//  0x1001429b..0x100143b4  THE TAIL.  If sub_1000f640() the three render
//      states are restored; otherwise the draw vertices get texture pair
//      ONE written back and the object is pushed onto the 0x1008d4c4
//      queue AGAIN -- this time with 0x1000eef0 INLINED (nIndex is the
//      literal 0, so the bucket slot is a bare `0x1c(%ecx)`), and with
//      0x1000a6a0 inlined inside that, which is where both calls to
//      0x1007fbf0 come from.  0x1000a6a0's shipped body
//      (`mov 0x18(%ecx),%eax ; test ; je refill ; mov (%eax),%edx ;
//      mov %edx,0x18(%ecx)`) matches the inline here exactly, and the pool
//      addresses agree: 0x1008d4e4+0x18 = 0x1008d4fc and
//      0x1008d4e4+0x08 = 0x1008d4ec, 0x1008d4c8+0x18 = 0x1008d4e0 and
//      0x1008d4c8+0x08 = 0x1008d4d0.  Then the frame counter 0x1008be5c
//      and the vertex commit 0x1008d520 += nVerts.
//
//  NOTHING IS UNDECODED.  Every instruction in 0x100124a4..0x10014544 is
//  accounted for by a statement below.  What the acceptance test CANNOT
//  see is stated at the bottom of the file with the coin-flips.
//
// *********************************************************************** //

#include "ltbasedefs.h"


// ======================================================================= //
//  THE 32-BYTE DRAW VERTEX -- big16324.cpp's RVert, unchanged.  Every
//  offset below is a load or a store in these bytes: +0x00..+0x08 the
//  position, +0x0c the reciprocal w (0x10012b64), +0x10..+0x13 the four
//  diffuse bytes the clipper interpolates in the order r, g, b, a
//  (0x12, 0x11, 0x10, 0x13), +0x17 the one specular byte it also
//  interpolates, +0x18/+0x1c the texture coordinates.
// ======================================================================= //
struct LTVec3
{
	float x;                                            // +0x00
	float y;                                            // +0x04
	float z;                                            // +0x08
};

struct RVert                                            // 0x20 bytes
{
	LTVec3 m_vPos;                                      // +0x00
	float  m_fRHW;                                      // +0x0c
	uint8  m_b;                                         // +0x10
	uint8  m_g;                                         // +0x11
	uint8  m_r;                                         // +0x12
	uint8  m_a;                                         // +0x13
	uint8  m_sb;                                        // +0x14
	uint8  m_sg;                                        // +0x15
	uint8  m_sr;                                        // +0x16
	uint8  m_sa;                                        // +0x17
	float  m_fU;                                        // +0x18
	float  m_fV;                                        // +0x1c
};


// ======================================================================= //
//  THE 0x28-BYTE SOURCE VERTEX.  Stride 0x28 is read off three separate
//  loops -- 0x1001274b, 0x1001282d and 0x100142c6 -- and the second
//  texture pair off the two that copy +0x20/+0x24 into the draw vertex's
//  +0x18/+0x1c.  The first 0x20 bytes are copied wholesale with
//  `rep movsl $8`, so they are an embedded RVert.
// ======================================================================= //
struct SrcVert                                          // 0x28 bytes
{
	RVert  m_Vert;                                      // +0x00
	float  m_fU2;                                       // +0x20
	float  m_fV2;                                       // +0x24
};


// ======================================================================= //
//  THE DIRECT3D 7 DEVICE AT 0x1008d988.  Index = offset/4 against
//  IDirect3DDevice7; the five slots this function uses are +0x50
//  SetRenderState (20), +0x54 GetRenderState (21), +0x64 DrawPrimitive
//  (25), +0x88 GetTexture (34) and +0x8c SetTexture (35).
// ======================================================================= //
struct D3DDevice7;

struct D3DDevice7Vtbl
{
	void*  m_pad_00[20];                                // +0x00 .. +0x4c
	long (__stdcall* SetRenderState)(D3DDevice7*, uint32 nState,
	                                 uint32 nValue);    // +0x50
	long (__stdcall* GetRenderState)(D3DDevice7*, uint32 nState,
	                                 uint32* pValue);   // +0x54
	void*  m_pad_58[3];                                 // +0x58 .. +0x60
	long (__stdcall* DrawPrimitive)(D3DDevice7*, uint32 nType, uint32 nFVF,
	                                void* pVerts, uint32 nCount,
	                                uint32 nFlags);     // +0x64
	void*  m_pad_68[8];                                 // +0x68 .. +0x84
	long (__stdcall* GetTexture)(D3DDevice7*, uint32 nStage,
	                             void** ppTexture);     // +0x88
	long (__stdcall* SetTexture)(D3DDevice7*, uint32 nStage,
	                             void* pTexture);       // +0x8c
};

struct D3DDevice7
{
	D3DDevice7Vtbl* lpVtbl;
};

#define D3DPT_TRIANGLEFAN                       6
#define D3DFVF_CLIPVERTEX                   0x1c4  // XYZRHW|DIFFUSE|SPEC|TEX1

#define D3DRENDERSTATE_SRCBLEND              0x13
#define D3DRENDERSTATE_DESTBLEND             0x14
#define D3DRENDERSTATE_ZFUNC                 0x17
#define D3DRENDERSTATE_ALPHAFUNC             0x19
#define D3DRENDERSTATE_ALPHABLENDENABLE      0x1b


// ======================================================================= //
//  THE OBJECT.  Every member is an offset this function or one of its
//  decoded callees touches.  +0x14 is 0x1000ef60's animating-lightmap
//  flag word (d3dlow.cpp), +0x44 is the texture 0x1000efa0 compares
//  against g_1008e684[stage] (d3dbatchd.cpp), +0x28 is the holder
//  d3dr3c.cpp names for 0x1000eef0.
// ======================================================================= //
struct Table124a4
{
	uint8       m_pad_00[0x1c];                         // +0x00 .. +0x1b
	void*       m_pBuckets[1];                          // +0x1c  0x100142db
	uint8       m_pad_20[0x0c];                         // +0x20 .. +0x2b
	void*       m_p2c;                                  // +0x2c  0x100124cc
};

struct Ctx124a4
{
	uint8       m_pad_00[0x30];                         // +0x00 .. +0x2f
	Table124a4* m_pTable30;                             // +0x30
	uint8       m_pad_34[0x04];                         // +0x34
	uint32      m_nFlags38;                             // +0x38  0x10012503
};

struct Lit124a4
{
	uint8       m_pad_00[0x20];                         // +0x00 .. +0x1f
	int32       m_nDone20;                              // +0x20
};

struct Obj124a4
{
	uint8       m_pad_00[0x14];                         // +0x00 .. +0x13
	uint16      m_wFlags14;                             // +0x14  0x1000ef60
	uint8       m_pad_16[0x12];                         // +0x16 .. +0x27
	Ctx124a4*   m_pCtx28;                               // +0x28
	void*       m_p2c;                                  // +0x2c
	uint8       m_pad_30[0x14];                         // +0x30 .. +0x43
	Lit124a4*   m_pTexture44;                           // +0x44
};


// ======================================================================= //
//  THE BUCKET LIST -- d3dr3c.cpp's Elem1000eef0 / Bucket1000eef0, and the
//  two pools 0x1002e2d0 creates with element sizes 0x18 and 0x0c.  The
//  pool header's +0x08 is the grow count and +0x18 the free-list head,
//  which is what makes 0x1008d4fc and 0x1008d4ec members of 0x1008d4e4.
// ======================================================================= //
struct Elem124a4                                        // 0x18 bytes
{
	void*       m_pOwner;                               // +0x00
	uint32      m_n04;                                  // +0x04
	uint32      m_n08;                                  // +0x08
	uint32      m_n0c;                                  // +0x0c
	uint32      m_n10;                                  // +0x10
	Elem124a4*  m_pNext;                                // +0x14
};

struct Bucket124a4                                      // 0x0c bytes
{
	void*       m_pTable;                               // +0x00
	Elem124a4*  m_pElements;                            // +0x04
	Bucket124a4* m_pNextInList;                         // +0x08
};

struct Pool124a4
{
	uint8       m_pad_00[0x08];                         // +0x00 .. +0x07
	uint32      m_nGrow08;                              // +0x08
	uint8       m_pad_0c[0x0c];                         // +0x0c .. +0x17
	void*       m_pFree18;                              // +0x18
};


// ======================================================================= //
//  GLOBALS -- every one of them is an address this function touches.
// ======================================================================= //

extern D3DDevice7*  g_pD3DDevice;                       // 0x1008d988

// The clipper's state, all six named already by d3dr3a / d3dr3c /
// big16324.
extern uint32       g_1008b874;                         // clip flags
extern RVert*       g_1008b87c;                         // scratch cursor
extern uint32       g_1008be44;                         // clipped-poly count
extern float        g_1008b908;                         // near plane
extern float        g_1008b910;                         // far plane
extern uint8        g_1008b880[];                       // the transform object

// Console variables, read as their leading dword the way big16324.cpp and
// big61a30.cpp read them.  Names from d3dr5c.cpp's registration lists.
extern uint32       g_1008ae00;                         // EnvMapWorld
extern uint32       g_1008ae20;                         // LMFullBright
extern uint32       g_1008ade0;                         // LMAnim
extern uint32       g_1008ae40;                         // FixSparkleys
extern uint32       g_1008d7a0;                         // UseD3DClip
extern uint32       g_1008cb98;                         // Saturate

extern uint32       g_1008d950;                         // d3dr3b: ValidateDevice
extern uint32       g_1008d7cc;                         // the texture stage
extern uint8*       g_p1008beac;                        // the light grid
extern uint32       g_1008be5c;                         // frame counter
extern uint32       g_1008d520;                         // vertices used
extern Bucket124a4* g_1008d4c4;                         // the second-pass list
extern Elem124a4*   g_1008d4fc;                         // element free list
extern Bucket124a4* g_1008d4e0;                         // bucket free list
extern Pool124a4    g_1008d4e4;                         // element pool
extern Pool124a4    g_1008d4c8;                         // bucket pool


// ======================================================================= //
//  NEIGHBOURS.  Each signature is read off the call site in these bytes
//  and, where the file exists, cross-checked against the decode named.
// ======================================================================= //

// 0x1000a700 -- d3dbatchd.cpp.  (geometry, &data, &count).
extern void   sub_1000a700(void* pGeom, void** ppOutData, uint32* pnOutCount);

// 0x1000a740 -- d3dbatchd.cpp.  `cmpl $0x80,arg ; jg`, SIGNED.
extern LTBOOL sub_1000a740(int32 nVerts);

// 0x1000f640 -- d3dneutral.cpp.  Six bytes, `mov 0x1008add0,%eax ; ret`.
extern uint32 sub_1000f640();

// 0x1000ef60 -- d3dlow.cpp, decoded in full.
extern void   sub_1000ef60(void* pA, void* pB);

// 0x1000efa0 -- d3dbatchd.cpp.  Binds a texture to a stage.
// int32 here (as big6d910.cpp has it): as LTBOOL icl predicts a zero answer
// and keeps 0x100124a4's bind-failure call inline; the shipped code sinks it.
extern int32  sub_1000efa0(void* pObj, uint32 nStage);

// 0x1000eef0 -- d3dr3c.cpp, decoded field for field.  Called with THREE
// arguments: the fourth dword the call site pushes is icl's alignment pad
// (`push %edi`, as before every 0x10011420 call), not a zero.
extern Elem124a4* sub_1000eef0(void* pOwner, Bucket124a4** ppList,
                               uint32 nIndex);

// 0x10011420 -- d3dbatcha.cpp, three arguments plus icl's dead fourth.
extern LTBOOL sub_10011420(SrcVert** ppVerts, int32* pnVerts, void* pXForm);

// 0x10011620 -- d3dbatchb.cpp: walks the list at 0x1008d4c4.  No argument
// is read by the body; the call site pushes none.
extern void   sub_10011620();

// 0x10039d70 -- __cdecl, arg0 at 0x8(%ebp), arg1 at 0xc(%ebp).
extern int32  sub_10039d70(void* pObj, int32 bFirst);

// 0x1000d9b0 -- d3dlow.cpp.  (vector, object): transform by the 3x4 at
// object+0x15c.
extern void   sub_1000d9b0(void* pVert, void* pXForm);

// 0x10014580 -- the screen projection, decoded in full at that address:
// rhw = 1/z, x = (x*m31c + z*m320)*rhw, y = (y*m324 + z*m328)*rhw,
// z = (z*m32c + m330)*rhw.
extern void   sub_10014580(void* pVert, void* pXForm);

// 0x10014550 -- d3dr2c.cpp.  Forwards to 0x1000f2c0 with a fourth
// argument of 0.
extern int32  sub_10014550(void* pA, void* pB, void* pC);

// 0x10024020 -- __cdecl, one argument.  Its body pops the SAME element
// free list at 0x1008d4fc and links the object onto 0x1008d4bc.
extern void   sub_10024020(void* pObj);

// 0x1007fbf0 -- d3dbatchc.cpp.  (pool, pool->m_nGrow08).
extern void   sub_1007fbf0(Pool124a4* pPool, uint32 nCount);


// ======================================================================= //
//  THE TRANSFORM OBJECT.  Only the projective block at +0x1dc..+0x218 is
//  read by name here; every other user of 0x1008b880 in the project takes
//  it as an opaque `uint8[]`, so the two views are kept apart and the
//  block is reached through a cast, exactly where the bytes read it.
// ======================================================================= //
struct XForm124a4
{
	uint8   m_pad_000[0x1dc];                           // +0x000 .. +0x1db
	float   m_f1dc;                                     // +0x1dc
	float   m_f1e0;                                     // +0x1e0
	float   m_f1e4;                                     // +0x1e4
	float   m_f1e8;                                     // +0x1e8
	float   m_f1ec;                                     // +0x1ec
	float   m_f1f0;                                     // +0x1f0
	float   m_f1f4;                                     // +0x1f4
	float   m_f1f8;                                     // +0x1f8
	float   m_f1fc;                                     // +0x1fc
	float   m_f200;                                     // +0x200
	float   m_f204;                                     // +0x204
	float   m_f208;                                     // +0x208
	float   m_f20c;                                     // +0x20c
	float   m_f210;                                     // +0x210
	float   m_f214;                                     // +0x214
	float   m_f218;                                     // +0x218
};



// The world vertex, 0x18 bytes: a pointer to the shared position, then
// the two texture pairs.
struct WorldVert1000f020
{
	LTVec3* m_pPos;                                     // +0x00
	float   m_fU04;                                     // +0x04
	float   m_fV08;                                     // +0x08
	float   m_fU0c;                                     // +0x0c
	float   m_fV10;                                     // +0x10
	uint32  m_n14;                                      // +0x14
};

// big0ae20.cpp's basis interface at +0x28: an ordinary C++ virtual on
// slot 0 handing back the two texture-space axes.
class Basis1000f020
{
public:
	virtual void Slot00(LTVec3* pOut0, LTVec3* pOut1) = 0;          // +0x00
};

struct Obj1000f020
{
	uint8           m_pad_00[0x28];
	Basis1000f020*  m_p28;                              // +0x28
	uint8           m_pad_2c[0x34 - 0x2c];
	LTVec3          m_vOrigin34;                        // +0x34
};

// big0ae20.cpp's per-stage inverse texture size pair.
struct TexScale0ae20
{
	float		m_fInvW00;                              // +0x00
	float		m_fInvH04;                              // +0x04
};

// big0f6f0.cpp's spelling of the specular hook (__fastcall, the vertex
// and its specular dword).
typedef void (__fastcall* PFN_1008ce8c)(void* pA, void* pB);
extern PFN_1008ce8c   g_pfn_1008ce8c;                   // 0x1008ce8c

extern TexScale0ae20  g_a1008e664[];                    // 0x1008e664

extern unsigned long  g_1008d51c;                       // 0x1008d51c  pool capacity
extern unsigned long  g_1008d524;                       // 0x1008d524  pool base

// 0x10023fb0 -- grow the screen-vertex pool to the given capacity; zero
// on failure.
extern "C" LTBOOL sub_10023fb0(uint32 nCapacity);

// The light grid's texture-space scale, +0xf8.
#define GRIDSCALE1000f020   (*(float*)(g_p1008beac + 0xf8))


// ======================================================================= //
//  0x1000f020
// ======================================================================= //
static void sub_1000f020(uint32 nFlag, SrcVert* pDest, WorldVert1000f020* pSrc, int32 nCount)
{
	float fU, fV;


	while (nCount > 0)
	{
		pDest->m_Vert.m_vPos.x = pSrc->m_pPos->x;
		pDest->m_Vert.m_vPos.y = pSrc->m_pPos->y;
		pDest->m_Vert.m_vPos.z = pSrc->m_pPos->z;
		*(uint32*)&pDest->m_Vert.m_b = 0xffffffff;
		g_pfn_1008ce8c(pDest, &pDest->m_Vert.m_sb);

		fU = pSrc->m_fU04;
		fV = pSrc->m_fV08;
		pDest->m_Vert.m_fU = fU;
		pDest->m_Vert.m_fV = fV;
		pDest->m_fU2 = pSrc->m_fU0c;
		pDest->m_fV2 = pSrc->m_fV10;

		pSrc++;
		nCount--;
		pDest++;
	}
}


// ======================================================================= //
//  0x1000f0a0
// ======================================================================= //
static void sub_1000f0a0(uint32 nFlag, Obj1000f020* pObj, SrcVert* pDest,
                         WorldVert1000f020* pSrc, int32 nCount)
{
	LTVec3 vAxisU, vAxisV;
	float fDX, fDY, fDZ;
	float fU, fV;
	int32 nLeft;

	pObj->m_p28->Slot00(&vAxisU, &vAxisV);

	nLeft = nCount;
	while (nLeft > 0)
	{
		pDest->m_Vert.m_vPos.x = pSrc->m_pPos->x;
		pDest->m_Vert.m_vPos.y = pSrc->m_pPos->y;
		pDest->m_Vert.m_vPos.z = pSrc->m_pPos->z;
		*(uint32*)&pDest->m_Vert.m_b = 0xffffffff;
		g_pfn_1008ce8c(pDest, &pDest->m_Vert.m_sb);

		fDX = pSrc->m_pPos->x - pObj->m_vOrigin34.x;
		fDY = pSrc->m_pPos->y - pObj->m_vOrigin34.y;
		fDZ = pSrc->m_pPos->z - pObj->m_vOrigin34.z;

		fU = pSrc->m_fU04;
		fV = pSrc->m_fV08;
		pDest->m_Vert.m_fU = fU;
		pDest->m_Vert.m_fV = fV;

		pDest->m_fU2 = ((fDX * vAxisU.x + fDY * vAxisU.y + fDZ * vAxisU.z)
		                / GRIDSCALE1000f020 + 0.5f) * g_a1008e664[0].m_fInvW00;
		pDest->m_fV2 = ((fDX * vAxisV.x + fDY * vAxisV.y + fDZ * vAxisV.z)
		                / GRIDSCALE1000f020 + 0.5f) * g_a1008e664[0].m_fInvH04;

		pSrc++;
		nLeft--;
		pDest++;
	}
}


// ======================================================================= //
//  0x1000f1b0
// ======================================================================= //
static void sub_1000f1b0(Obj1000f020* pObj, SrcVert* pDest,
                         WorldVert1000f020* pSrc, int32 nCount)
{
	LTVec3 vAxisU, vAxisV;
	float fDX, fDY, fDZ;
	int32 nLeft;

	pObj->m_p28->Slot00(&vAxisU, &vAxisV);

	nLeft = nCount;
	while (nLeft > 0)
	{
		fDX = pSrc->m_pPos->x - pObj->m_vOrigin34.x;
		fDY = pSrc->m_pPos->y - pObj->m_vOrigin34.y;
		fDZ = pSrc->m_pPos->z - pObj->m_vOrigin34.z;

		pDest->m_fU2 = ((fDX * vAxisU.x + fDY * vAxisU.y + fDZ * vAxisU.z)
		                / GRIDSCALE1000f020 + 0.5f) * g_a1008e664[0].m_fInvW00;
		pDest->m_fV2 = ((fDX * vAxisV.x + fDY * vAxisV.y + fDZ * vAxisV.z)
		                / GRIDSCALE1000f020 + 0.5f) * g_a1008e664[0].m_fInvH04;

		pSrc++;
		nLeft--;
		pDest++;
	}
}


// ======================================================================= //
//  0x1000f270
// ======================================================================= //
static RVert* sub_1000f270(int32 nCount)
{
	if (nCount + g_1008d520 > g_1008d51c)
	{
		if (!sub_10023fb0(g_1008d51c + 1500 + nCount))
			return 0;
	}

	return (RVert*)g_1008d524 + g_1008d520;
}


struct Bucket123a0                                      // the lightmap
{
	void*        m_pTable00;                            // +0x00
	Bucket123a0* m_pNext04;                             // +0x04
	Elem124a4*   m_pElements08;                         // +0x08
};


struct Poly1000f650
{
	uint8        m_pad_00[0x2c];                        // +0x00
	void*        m_p2c;                                 // +0x2c
	uint8        m_pad_30[0x14];                        // +0x30
	Bucket123a0* m_pLightmap44;                         // +0x44
};

extern Bucket123a0*   g_1008aec0;                       // 0x1008aec0  the deferred queue

// 0x100124a0 -- the pass below: external, so /Qip gives it the two-entry
// pair (the stack-entry shim at 0x100124a0, the body at 0x100124a4), and
// both callers here enter at the register entry.
// Round to nearest in the current FPU mode: w1_10001000.cpp's helper
// (0x10015190 out of line).  Inlined, it is the shipped `fstps t ;
// flds t ; fistpl n` at each of the thirty byte lerps.
// Measured over the true extent 0x100124a4..0x10014544:
// 9001 -> 8059 bytes (shipped 8352), census residue 797 -> 389.  The row
// is still cut at 2855 bytes by the bogus start 0x10012fcb
// (a known bad enumeration row), so it cannot be paired.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}

void sub_100124a0(Obj124a4* pObj);

// struct_bank.h's sb_Allocate (the out-of-line copy is 0x1000a6a0).
inline void* sb_Allocate124a4(Pool124a4* pBank)
{
	void* pRet;

	pRet = pBank->m_pFree18;
	if (!pRet)
	{
		sub_1007fbf0(pBank, pBank->m_nGrow08);
		pRet = pBank->m_pFree18;
		if (!pRet)
			return 0;
	}

	pBank->m_pFree18 = *(void**)pRet;
	return pRet;
}

// ======================================================================= //
//  0x1000f650 -- 151 bytes.
// ======================================================================= //
void sub_1000f650(void* pvPoly)
{
	Poly1000f650* pPoly = (Poly1000f650*)pvPoly;
	Bucket123a0*  pLightmap;
	Elem124a4*    pElem;

	if (pPoly->m_p2c)
	{
		sub_100124a0((Obj124a4*)pPoly);
		return;
	}

	pLightmap = pPoly->m_pLightmap44;
	if (!pLightmap)
		return;

	pElem = (Elem124a4*)sb_Allocate124a4(&g_1008d4e4);
	if (!pElem)
		return;

	if (!pLightmap->m_pElements08)
	{
		pLightmap->m_pNext04 = g_1008aec0;
		g_1008aec0 = pLightmap;
	}

	pElem->m_n10 = g_1008b874;
	pElem->m_pOwner = pPoly;
	pElem->m_pNext = pLightmap->m_pElements08;
	pLightmap->m_pElements08 = pElem;
}


// ======================================================================= //
//  0x100123a0 -- flush the deferred lightmap queue (260 bytes).
//
//  Fog colour is forced to 0x10023930(g_1008caf4..fc) around the flush
//  (render state 0x22 = D3DRENDERSTATE_FOGCOLOR, saved with
//  GetRenderState, vtable +0x54, and restored with SetRenderState, +0x50).
//  The queue at 0x1008aec0 is a list of buckets (+0x04 next, +0x08 the
//  element list); each element carries the object (+0x00), the clip flags
//  (+0x10) and the next element (+0x14).  Every element goes through
//  0x100124a4's lightmap pass and back onto the free list at 0x1008d4fc
//  (through its +0x00), and the buckets are emptied.
//
//  The call to 0x100124a4 is its /Qip REGISTER entry (the object in %eax).
// ======================================================================= //


// A render state saved on entry and put back on exit, inlined.
struct State123a0
{
	uint32  m_nState;
	uint32  m_nSaved;
};

extern uint32           g_1008caf4;                     // 0x1008caf4  fog colour r
extern uint32           g_1008caf8;                     // 0x1008caf8  fog colour g
extern uint32           g_1008cafc;                     // 0x1008cafc  fog colour b

extern "C" uint32 sub_10023930(uint32 r, uint32 g, uint32 b);

void sub_100123a0()
{
	State123a0   fog;
	uint32       nColor;
	Bucket123a0* pBucket;
	Bucket123a0* pNextBucket;
	Elem124a4*   pElem;
	Elem124a4*   pNextElem;

	nColor = sub_10023930(g_1008caf4, g_1008caf8, g_1008cafc);

	fog.m_nState = 0x22;
	g_pD3DDevice->lpVtbl->GetRenderState(g_pD3DDevice, fog.m_nState, &fog.m_nSaved);
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, fog.m_nState, nColor);

	for (pBucket = g_1008aec0; pBucket; pBucket = pNextBucket)
	{
		pElem       = pBucket->m_pElements08;
		pNextBucket = pBucket->m_pNext04;

		for (; pElem; pElem = pNextElem)
		{
			pNextElem  = pElem->m_pNext;
			g_1008b874 = pElem->m_n10;

			sub_100124a0((Obj124a4*)pElem->m_pOwner);

			pElem->m_pOwner      = g_1008d4fc;
			g_1008d4fc = pElem;
		}

		pBucket->m_pNext04     = LTNULL;
		pBucket->m_pElements08 = LTNULL;
	}

	g_1008aec0 = LTNULL;

	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, fog.m_nState, fog.m_nSaved);
}



// ----------------------------------------------------------------------- //
//  THE FAN DRAWER, INLINED.  0x10012a0c .. 0x1001424b is
//  one inline function's body, and it calls two more, each inlined:
//
//    DrawFan124a4(pVerts, nVerts, pXForm, nFVF)   per-triangle, or
//      XFormClip124a4(&pVerts, &nVerts, pXForm)   0x1000a8a0's body:
//                                                 project whole, or to
//                                                 camera space, clip,
//                                                 project the survivors
//        ClipPoly124(flags, ppVerts, pnVerts)     0x1000f4b0, d3dr3c_os.cpp
//          ClipNear124 .. ClipFar124              0x10001000 .. 0x10001fe0,
//                                                 w1_10001000.cpp /
//                                                 w44_10001670.cpp
//
//  The four bodies are those files' byte-identical out-of-line functions,
//  copied over this TU's 32-byte RVert.  What it settled, all measured:
//    * THE FRAME.  0x1b8c -> 0x1f44 exactly: the per-triangle local is
//      32 vertices (-0x4e0 .. -0xe0, 0x400 bytes; only three are used), the
//      projection result is an LTVector copied with `rep movsl $3` (the
//      header's MatVMul_H), and DrawFan's four parameters keep their own
//      slots at the bottom of the frame (-0x1f30 .. -0x1f24) -- which is
//      why the shipped code stores the constant transform and the FVF to
//      memory at 0x10012a28 / 0x10012a32 before testing anything.
//    * THE SLOT ORDER.  icl lays the frame out in reverse order of
//      appearance: the per-triangle path written AFTER the transform-and-
//      draw (the `else` of the clip test) puts aTri / pTriVerts just under
//      the scalars and the clip planes below them, as shipped; written
//      first, the two swap.
//    * THE PLANES AS FUNCTIONS over (&pVerts, &nVerts, &pOut), not blocks
//      over the driver's locals: normalised instruction match 0.784 (the
//      old longhand blocks) -> 0.891 (blocks over the driver's locals,
//      w23_10050e60_os.cpp's old CLIP_BLOCK) -> 0.964 (these), and the
//      driver's flag word and vertex pointer land in the shipped slots.
//  Now 8347 bytes against 8353, 2043 of 2044 instructions matched: the
//  prologue's flags set b80, bMoved, b88 in that order; the done-latch's
//  false arm skips the draw test (`goto L_NoDraw`, the shipped threaded
//  `je 0x100126a9`); ClipPoly124 loads pOut through a temporary and
//  ClipNear124 takes pOutStart before its counter.  Left: the unclipped
//  transform's second row (MatVMul_H's), the fan's early pVerts load and
//  the bind-failure block's placement.
// ----------------------------------------------------------------------- //
#define CLIP_EPSILON    0.00001f
#define CLIP_MAX_VERTS  50

// 0x10023380, the vertex lerp (out of line there, inlined here).
static inline void LerpRVert(RVert* a, RVert* b, RVert* dst, float t)
{
	dst->m_fU = (b->m_fU - a->m_fU) * t + a->m_fU;
	dst->m_fV = (b->m_fV - a->m_fV) * t + a->m_fV;
	dst->m_r  = (uint8)RoundF((b->m_r  - a->m_r)  * t + a->m_r);
	dst->m_g  = (uint8)RoundF((b->m_g  - a->m_g)  * t + a->m_g);
	dst->m_b  = (uint8)RoundF((b->m_b  - a->m_b)  * t + a->m_b);
	dst->m_a  = (uint8)RoundF((b->m_a  - a->m_a)  * t + a->m_a);
	dst->m_sa = (uint8)RoundF((b->m_sa - a->m_sa) * t + a->m_sa);
}

static inline float ClipEdgeNear124(LTVector* a, LTVector* b, LTVector* dst)
{
	float fDenom = b->z - a->z;
	float t;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = (g_1008b908 - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	dst->y = (b->y - a->y) * t + a->y;
	dst->z = g_1008b908;

	return t;
}

static inline float ClipEdgeLeft124(LTVector* a, LTVector* b, LTVector* dst)
{
	float fDenom = (b->x - a->x) + b->z - a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = -(a->x + a->z) / fDenom;
	else
		t = 0.0f;

	dst->y = (b->y - a->y) * t + a->y;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->x = -fZ;

	return t;
}

static inline float ClipEdgeTop124(LTVector* a, LTVector* b, LTVector* dst)
{
	float fDenom = (b->y - a->y) - b->z + a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = -(a->y - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->y = fZ;

	return t;
}

static inline float ClipEdgeRight124(LTVector* a, LTVector* b, LTVector* dst)
{
	float fDenom = (b->x - a->x) - b->z + a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = -(a->x - a->z) / fDenom;
	else
		t = 0.0f;

	dst->y = (b->y - a->y) * t + a->y;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->x = fZ;

	return t;
}

static inline float ClipEdgeBottom124(LTVector* a, LTVector* b, LTVector* dst)
{
	float fDenom = (b->y - a->y) + b->z - a->z;
	float t;
	float fZ;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = -(a->y + a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	fZ = (b->z - a->z) * t + a->z;
	dst->z = fZ;
	dst->y = -fZ;

	return t;
}

static inline float ClipEdgeFar124(LTVector* a, LTVector* b, LTVector* dst)
{
	float fDenom = b->z - a->z;
	float t;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = (g_1008b910 - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	dst->y = (b->y - a->y) * t + a->y;
	dst->z = g_1008b910;

	return t;
}


// ----------------------------------------------------------------------- //
//  0x10001000 -- near plane
// ----------------------------------------------------------------------- //
static inline LTBOOL ClipNear124(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	pOutStart = *ppOut;
	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.z >= g_1008b908);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeNear124((LTVector*)&pPrev->m_vPos, (LTVector*)&pCur->m_vPos, (LTVector*)&(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// ----------------------------------------------------------------------- //
//  0x10001330 -- left plane
// ----------------------------------------------------------------------- //
static inline LTBOOL ClipLeft124(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (-pV->m_vPos.z < pV->m_vPos.x);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeLeft124((LTVector*)&pPrev->m_vPos, (LTVector*)&pCur->m_vPos, (LTVector*)&(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// ----------------------------------------------------------------------- //
//  0x10001670 -- top plane
// ----------------------------------------------------------------------- //
static inline LTBOOL ClipTop124(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.y < pV->m_vPos.z);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeTop124((LTVector*)&pPrev->m_vPos, (LTVector*)&pCur->m_vPos, (LTVector*)&(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// ----------------------------------------------------------------------- //
//  0x10001990 -- right plane
// ----------------------------------------------------------------------- //
static inline LTBOOL ClipRight124(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.x < pV->m_vPos.z);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeRight124((LTVector*)&pPrev->m_vPos, (LTVector*)&pCur->m_vPos, (LTVector*)&(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// ----------------------------------------------------------------------- //
//  0x10001cb0 -- bottom plane
// ----------------------------------------------------------------------- //
static inline LTBOOL ClipBottom124(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (-pV->m_vPos.z < pV->m_vPos.y);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeBottom124((LTVector*)&pPrev->m_vPos, (LTVector*)&pCur->m_vPos, (LTVector*)&(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


// ----------------------------------------------------------------------- //
//  0x10001fe0 -- far plane
// ----------------------------------------------------------------------- //
static inline LTBOOL ClipFar124(uint32* pUnused, RVert** ppVerts, int32* pnVerts, RVert** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert*  pOutStart;
	RVert*  pV;
	RVert*  pEnd;
	RVert*  pPrev;
	RVert*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.z <= g_1008b910);

		nInside += bIn;
		*pFlag++ = bIn;
	}

	if (nInside == 0)
		return LTFALSE;

	if (nInside != *pnVerts)
	{
		pOutStart = *ppOut;
		iPrev     = *pnVerts - 1;
		pPrev     = *ppVerts + iPrev;

		for (i = 0; i < *pnVerts; ++i)
		{
			pCur = *ppVerts + i;

			if (anInside[iPrev])
				*(*ppOut)++ = *pPrev;

			if (anInside[iPrev] != anInside[i])
			{
				float t = ClipEdgeFar124((LTVector*)&pPrev->m_vPos, (LTVector*)&pCur->m_vPos, (LTVector*)&(*ppOut)->m_vPos);

				LerpRVert(pPrev, pCur, *ppOut, t);

				++(*ppOut);
			}

			iPrev = i;
			pPrev = pCur;
		}

		*pnVerts = *ppOut - pOutStart;
		*ppVerts = pOutStart;
		*ppOut  += *pnVerts;
	}

	return LTTRUE;
}


static inline LTBOOL ClipPoly124(uint32 nClipFlags, RVert** ppVerts, int32* pnVerts)
{
	RVert*  pOut;
	RVert*  pVerts;
	int32      nVerts;
	uint32     anUnused[6];

	if (g_1008d7a0)
	{
		nClipFlags &= 1;
		if (!nClipFlags)
			return LTTRUE;
	}

	RVert* pT = g_1008b87c;
	pOut   = pT;
	pVerts = *ppVerts;
	nVerts = *pnVerts;

	if (nClipFlags & 0x01)
	{
		if (!ClipNear124(&anUnused[0], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x04)
	{
		if (!ClipLeft124(&anUnused[1], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x08)
	{
		if (!ClipTop124(&anUnused[2], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x10)
	{
		if (!ClipRight124(&anUnused[3], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x20)
	{
		if (!ClipBottom124(&anUnused[4], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	// The far plane is written the other way round from the five above:
	// the shipped tail FALLS THROUGH into the write-back and jumps to the
	// shared `return 0` block (`je` to it), where the five-guards spelling
	// inverts the last test and lays the failure block first.  Measured:
	// this spelling is byte-identical, the other differs in the last 32.
	if (!(nClipFlags & 0x02) ||
	    ClipFar124(&anUnused[5], &pVerts, &nVerts, &pOut))
	{
		*ppVerts = pVerts;
		*pnVerts = nVerts;
		return LTTRUE;
	}

	return LTFALSE;
}



// 0x1000a8a0's body (w2_1000a8a0_os.cpp), inlined: project the fan whole
// when nothing needs clipping, else to camera space, clip, and project
// what survives.
static inline LTBOOL XFormClip124a4(RVert** ppVerts, int32* pnVerts, XForm124a4* pM)
{
	LTVector      vScreen;           // -0xa14
	RVert*        pWalk;
	int32         nLeft;
	float         fOOW;

	if (!g_1008b874)
	{
		// ---------------------------------------------------------- //
		//  0x10012a65 -- NO CLIPPING WANTED.  One projective transform
		//  per vertex through the block at +0x1dc..+0x218 (the `rep
		//  movsl $3` at 0x10012b4f copies the result into the vertex),
		//  the reciprocal into +0x0c.  The identical instruction
		//  sequence, operand for operand, is 0x1000a8d0.
		// ---------------------------------------------------------- //
		pWalk = *ppVerts;
		nLeft = *pnVerts;

		while (nLeft--)
		{
			fOOW = MatVMul_H(&vScreen, (LTMatrix*)&pM->m_f1dc, (LTVector*)&pWalk->m_vPos);

			*(LTVector*)&pWalk->m_vPos = vScreen;
			pWalk->m_fRHW = fOOW;
			pWalk++;
		}
	}
	else
	{
		// 0x10012b7e -- to camera space.
		pWalk = *ppVerts;
		nLeft = *pnVerts;

		while (nLeft--)
		{
			sub_1000d9b0(pWalk, pM);
			pWalk++;
		}

		if (!ClipPoly124(g_1008b874, ppVerts, pnVerts))
			return LTFALSE;

		// 0x100140e1 -- sub_1005fa80 (d3dr2a.cpp) inlined: the screen
		// projection of every survivor.
		pWalk = *ppVerts;
		nLeft = *pnVerts;

		while (nLeft--)
		{
			sub_10014580(pWalk, pM);
			pWalk++;
		}
	}

	return LTTRUE;
}


// 0x10012a26 .. 0x1001424b -- the fan drawn through the transform object:
// per triangle through 0x10014550 when D3D cannot clip it, else
// transformed (and clipped) whole and drawn.  Inlined at its one call;
// its parameters keep their own frame slots (-0x1f30 .. -0x1f24), which
// is how the shipped code comes to store the constant transform and FVF
// before the tests.
static inline void DrawFan124a4(RVert* pVerts, int32 nVerts, void* pXForm, uint32 nFVF)
{
	RVert         aTri[32];          // -0x4e0
	RVert*        pB;
	RVert*        pC;
	RVert*        pTriVerts;         // -0xe0
	int32         nTriVerts;         // -0xdc

	// 0x10012a26 .. 0x10012a4e.  With D3D clipping off, or with the near
	// bit set (D3D cannot clip that), go per-triangle.
	if (!g_1008b874 || (g_1008d7a0 && !(g_1008b874 & 1)))
	{
		if (XFormClip124a4(&pVerts, &nVerts, (XForm124a4*)pXForm))
		{
			// 0x1001410b -- vtable +0x64, six arguments, nothing unwound.
			g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, D3DPT_TRIANGLEFAN,
			                                    nFVF, pVerts, nVerts, 0);
		}
	}
	else
	{

	// ------------------------------------------------------------------ //
	//  0x1001414e -- the fan walked as (v0, vi, vi+1), each triangle
	//  copied into a three-vertex local with three `rep movsl $8` and
	//  handed to the out-of-line clipper.  nVerts itself is the counter:
	//  it is decremented and stored back at 0x1001423f.
	// ------------------------------------------------------------------ //
	if (nVerts > 2)
	{
		pB = pVerts + 1;
		pC = pVerts + 2;

		do
		{
			aTri[0] = pVerts[0];
			aTri[1] = *pB;
			aTri[2] = *pC;

			pTriVerts = aTri;
			nTriVerts = 3;

			if (sub_10014550(&pTriVerts, &nTriVerts, pXForm))
				g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice,
					D3DPT_TRIANGLEFAN, nFVF, pTriVerts, nTriVerts, 0);

			pC++;
			pB++;
			nVerts--;
		}
		while (nVerts > 2);
	}
	}

}


// ----------------------------------------------------------------------- //
//  0x100124a4 .. 0x10014544   (8,353 bytes; the row's 2,855 is wrong -- see
//  the extent note at the top of the file).
//
//  The shipped entry takes pObj in %eax: a /Qip static.
// ----------------------------------------------------------------------- //
void sub_100124a0(Obj124a4* pObj)
{
	SrcVert       aVerts[128];       // -0x1e28, 0x1400 bytes = 128 * 0x28

	Ctx124a4*     pCtx;
	Table124a4*   pTable;
	Bucket124a4*  pBucket;
	Elem124a4*    pElem;

	void*         pData;             // -0x1e34
	int32         nSrcCount;         // -0x1e30
	int32         nVerts;            // -0x1e2c
	SrcVert*      pSrcVerts;         // -0xa28
	SrcVert*      pSrcWalk;
	RVert*        pVertBuf;          // -0x74
	RVert*        pDest;

	void*         pSavedTexture;     // -0xa24
	uint32        nSavedAlphaBlend;  // -0xa20
	uint32        nSavedAlphaFunc;   // -0xa1c
	uint32        nSavedZFunc;       // -0xa18

	uint32        nFlagBit;
	int32         nLeft;
	// bDraw / bMoved are unsigned (LTBOOL): as int32 icl turns their
	// `==` / `!=` into branches where the shipped code has sete/setne.
	LTBOOL        bDraw;
	LTBOOL        bMoved;            // -0x70
	LTBOOL        b80;               // -0x80
	LTBOOL        b88;               // -0x88
	LTBOOL        bEnvMap;           // -0x78
	LTBOOL        bTransform;

	pCtx   = pObj->m_pCtx28;

	// 0x100124c3 / 0x100124d4 -- `xor %dl,%dl ; inc %dl` and `mov $0x1,%dh`,
	// two one-byte TRUEs materialised before the branch.
	b80    = LTTRUE;
	bMoved = 0;
	b88    = LTTRUE;

	// 0x100124d2 .. 0x10012500 -- three tests, all three must hold, and the
	// false arm of each is the same `bEnvMap = 0`.
	if (pCtx->m_pTable30->m_p2c && g_1008ae00 && g_1008d950)
		bEnvMap = LTTRUE;
	else
		bEnvMap = LTFALSE;

	// 0x10012503 -- `and $0x100000,%esi`, kept in %esi across both calls
	// that consume it (0x10012570 and 0x10012673).
	nFlagBit = pCtx->m_nFlags38 & 0x100000;

	// 0x10012529 / 0x1001253e.  0x1000a740 is `nVerts > 128` and the array
	// below is 128 elements for that reason.
	sub_1000a700(pObj, &pData, (uint32*)&nSrcCount);
	nVerts = nSrcCount;

	if (!sub_1000a740(nVerts))
		return;

	// 0x10012566 / 0x10012578.
	pSrcVerts = aVerts;
	sub_1000f020(nFlagBit, aVerts, (WorldVert1000f020*)pData, nVerts);

	// 0x10012580 .. 0x100125cb.  `setne` on `pSrcVerts != aVerts`.
	if (g_1008b874 && !bEnvMap)
	{
		if (!sub_10011420(&pSrcVerts, &nVerts, g_1008b880))
			return;

		b80    = LTFALSE;
		bMoved = (pSrcVerts != aVerts);
	}

	// 0x100125d2 -- LMAnim.
	if (g_1008ade0)
		sub_1000ef60(g_p1008beac, pObj);

	// 0x100125ee .. 0x10012636.  The `sete` answer is reused as the second
	// argument of 0x10039d70 and as the value tested afterwards, which is
	// what makes it one variable and not two tests.
	bDraw = (pObj->m_pTexture44->m_nDone20 == 0);

	if (bDraw)
		pObj->m_pTexture44->m_nDone20 = 1;
	else if (!pObj->m_p2c)
		goto L_NoDraw;

	if (pObj->m_p2c)
		bDraw = sub_10039d70(pObj, bDraw);

L_NoDraw:
	bTransform = b80;

	if (bDraw)
	{
		// 0x10012638.
		b88 = LTFALSE;

		if (bMoved)
		{
			// 0x10012647 -- the array moved, so start again from the
			// untransformed source and rebuild through the five-argument
			// form.  Both stores are here in the shipped bytes
			// (0x10012656 and 0x10012662).
			pSrcVerts = aVerts;
			nVerts    = nSrcCount;
			sub_1000f0a0(nFlagBit, (Obj1000f020*)pObj, aVerts,
			             (WorldVert1000f020*)pData, nSrcCount);
			bTransform = LTTRUE;
		}
		else
		{
			// 0x10012685.
			sub_1000f1b0((Obj1000f020*)pObj, pSrcVerts,
			             (WorldVert1000f020*)pData, nVerts);
		}
	}

	// 0x100126a9 / 0x100126b0 / 0x100126cb.
	if (bTransform && !bEnvMap)
	{
		if (!sub_10011420(&pSrcVerts, &nVerts, g_1008b880))
			return;
	}

	// 0x100126dc -- reserve nVerts 32-byte vertices; 0 on failure, and the
	// failure arm is the trampoline at 0x10014540 into the epilogue.
	pVertBuf = sub_1000f270(nVerts);
	if (!pVertBuf)
		return;

	// 0x100126f8 -- `mov 0x1008add0,%eax ; ret`.
	if (sub_1000f640())
	{
		// -------------------------------------------------------------- //
		//  0x10012705 -- FIRST PASS.  The 0x20-byte vertex verbatim, then
		//  texture pair ONE written over its own copy.  The compiler does
		//  not elide the second pair of stores (no dead-store
		//  elimination through memory), so they are in the source.
		// -------------------------------------------------------------- //
		pSrcWalk = pSrcVerts;
		pDest    = pVertBuf;
		nLeft    = nVerts;

		while (nLeft--)
		{
			*pDest      = pSrcWalk->m_Vert;
			pDest->m_fU = pSrcWalk->m_Vert.m_fU;
			pDest->m_fV = pSrcWalk->m_Vert.m_fV;
			pDest++;
			pSrcWalk++;
		}

		// 0x10012766 -- four arguments, 0x10 unwound; d3dr3c.cpp records
		// the fourth as dead.
		pElem = sub_1000eef0(pObj, &g_1008d4c4, 0);

		pElem->m_n04 = g_1008d520;
		pElem->m_n08 = nVerts;
		pElem->m_n0c = bMoved;
		pElem->m_n10 = b88 ? g_1008b874 : 0;

		// 0x100127b3 / 0x100127db / 0x100127ea -- save, flush, restore.
		if (!b88)
			g_pD3DDevice->lpVtbl->GetTexture(g_pD3DDevice, g_1008d7cc,
			                                 &pSavedTexture);

		sub_10011620();

		if (!b88)
			g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, g_1008d7cc,
			                                 pSavedTexture);

		// 0x10012811 -- swap in texture pair TWO.
		pSrcWalk = pSrcVerts;
		pDest    = pVertBuf;
		nLeft    = nVerts;

		while (nLeft--)
		{
			pDest->m_fU = pSrcWalk->m_fU2;
			pDest->m_fV = pSrcWalk->m_fV2;
			pDest++;
			pSrcWalk++;
		}

		// 0x10012839 .. 0x1001296a -- three states saved, five written.
		g_pD3DDevice->lpVtbl->GetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_ALPHABLENDENABLE, &nSavedAlphaBlend);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_ALPHABLENDENABLE, 1);

		g_pD3DDevice->lpVtbl->GetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_ALPHAFUNC, &nSavedAlphaFunc);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_ALPHAFUNC, 8);

		// 0x100128bf -- Saturate picks DESTCOLOR over ZERO.
		if (g_1008cb98)
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_SRCBLEND, 9);
		else
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
				D3DRENDERSTATE_SRCBLEND, 1);

		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_DESTBLEND, 3);

		g_pD3DDevice->lpVtbl->GetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_ZFUNC, &nSavedZFunc);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_ZFUNC, 3);
	}
	else
	{
		// 0x1001296f -- SINGLE PASS: the 0x20-byte vertex and texture pair
		// TWO in one go.
		pSrcWalk = pSrcVerts;
		pDest    = pVertBuf;
		nLeft    = nVerts;

		while (nLeft--)
		{
			*pDest      = pSrcWalk->m_Vert;
			pDest->m_fU = pSrcWalk->m_fU2;
			pDest->m_fV = pSrcWalk->m_fV2;
			pDest++;
			pSrcWalk++;
		}
	}

	// 0x100129d0 -- LMFullBright: nothing to modulate with.
	if (g_1008ae20)
		goto L_Done;

	// 0x100129dd -- bind the lightmap to the current stage; a failed
	// bind goes straight to the epilogue (0x10014289), NOT through the
	// tail.
	if (b88)
	{
		if (!sub_1000efa0(pObj, g_1008d7cc))
			goto L_BindFailed;
	}

	// 0x10012a01.
	if (bEnvMap)
	{
		// 0x10012a0c -- the four values the fan drawer works from.
		DrawFan124a4(pVertBuf, nVerts, g_1008b880, D3DFVF_CLIPVERTEX);
	}
	else
	{
		// 0x1001424d -- no env-map gate, so nothing was transformed: the
		// reserved buffer goes straight out with the literal FVF.
		g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, D3DPT_TRIANGLEFAN,
		                                    D3DFVF_CLIPVERTEX, pVertBuf,
		                                    nVerts, 0);
	}
	goto L_Done;

L_BindFailed:
	// 0x10014289 -- straight to the epilogue, NOT through the tail.
	sub_10024020(pObj);
	return;

L_Done:
	// ------------------------------------------------------------------ //
	//  0x1001429b -- THE TAIL.
	// ------------------------------------------------------------------ //
	// 0x1001429b -- `call 0x1000f640 ; test %eax,%eax ; jne 0x1001433c`, so
	// the FALL-THROUGH arm is the requeue and the restore is the `else`.
	// That order is what puts 0x10011620 at 0x10014335, ahead of the three
	// SetRenderState calls at 0x1001433c..0x1001439f.
	if (!sub_1000f640())
	{
		// 0x100142a8 -- texture pair ONE back into the draw vertices.
		pSrcWalk = pSrcVerts;
		pDest    = pVertBuf;
		nLeft    = nVerts;

		while (nLeft--)
		{
			pDest->m_fU = pSrcWalk->m_Vert.m_fU;
			pDest->m_fV = pSrcWalk->m_Vert.m_fV;
			pDest++;
			pSrcWalk++;
		}

		// -------------------------------------------------------------- //
		//  0x100142d2 -- 0x1000eef0 INLINED with nIndex = 0 (the bucket
		//  slot is a bare `0x1c(%ecx)`), and 0x1000a6a0 inlined inside it
		//  twice, which is where both 0x1007fbf0 calls come from.  Field
		//  for field this is d3dr3c.cpp's decode of 0x1000eef0.
		// -------------------------------------------------------------- //
		pTable  = pObj->m_pCtx28->m_pTable30;
		pBucket = (Bucket124a4*)pTable->m_pBuckets[0];

		if (!pBucket)
		{
			// 0x100144dc / 0x10014510 -- sub_1000a6a0(&g_1008d4c8).
			pBucket = (Bucket124a4*)sb_Allocate124a4(&g_1008d4c8);

			pBucket->m_pTable      = pTable;
			pTable->m_pBuckets[0]  = pBucket;
			pBucket->m_pElements   = LTNULL;
			pBucket->m_pNextInList = g_1008d4c4;
			g_1008d4c4             = pBucket;
		}

		// 0x100142e6 / 0x100144a5 -- sub_1000a6a0(&g_1008d4e4).
		pElem = (Elem124a4*)sb_Allocate124a4(&g_1008d4e4);

		// 0x100142fd.
		pElem->m_pNext       = pBucket->m_pElements;
		pBucket->m_pElements = pElem;
		pElem->m_pOwner      = pObj;

		pElem->m_n04 = g_1008d520;
		pElem->m_n08 = nVerts;
		pElem->m_n0c = bMoved;
		pElem->m_n10 = g_1008b874;

		// 0x1001432c -- FixSparkleys.
		if (g_1008ae40)
			sub_10011620();
	}
	else
	{
		// 0x1001433c -- the three saved states back, in the order they
		// were saved.
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_ALPHABLENDENABLE, nSavedAlphaBlend);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_ALPHAFUNC, nSavedAlphaFunc);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice,
			D3DRENDERSTATE_ZFUNC, nSavedZFunc);
	}

	// 0x100143a2 -- the frame counter and the vertex commit that pairs
	// with 0x1000f270's reserve.
	g_1008be5c++;
	g_1008d520 += nVerts;
}


// *********************************************************************** //
//
//  WHAT THE ACCEPTANCE TEST CANNOT SEE.
//
//  1. THE SIX CLIP PLANES ARE 4.2 KB WITH NO CALLS IN THEM AT ALL --
//     0x10012bc6..0x100140c9, half the function.  The call sequence is blind to
//     every instruction in them.  What pins them instead is that they are
//     the SAME six blocks big16324.cpp already carries for the same
//     32-byte vertex, which were themselves diffed against d3dr3a.cpp's
//     out-of-line 0x10002d40 / 0x100030a0.  Every plane test, denominator,
//     numerator, epsilon and field order below was re-read off THIS
//     image's bytes and then compared with that decode; they agree
//     instruction for instruction, including the two `fsubrp`/`faddp`
//     forms that pick the numerator's sign and the per-plane choice of
//     which position component is copied and which is negated.  Three
//     independent decodes agreeing is better evidence than a call
//     sequence, but it is not a call sequence, so it is stated here.
//
//  2. THE UNCLIPPED PROJECTION (0x10012a65..0x10012b79) IS ALSO CALL-FREE.
//     It is pinned by 0x1000a8d0..0x1000a95e, an independent copy of the
//     identical x87 sequence elsewhere in the same image.
//
//  3. _CHKSTK.  0x100124ad is `mov $0x1f44,%eax ; call 0x100805a0`, the
//     compiler's own stack probe for the 8004-byte frame.  Our object will
//     emit its own `__chkstk`, which the comparison cannot resolve to 0x100805a0,
//     so expect ONE shipped-only row at the head of the sequence.  It is
//     the toolchain, not a missing statement.
//
//  4. BLOCK PLACEMENT.  The shipped build sinks the per-triangle path,
//     the two short-circuit exits and the whole tail below the clipper.
//     The `goto`s above are written to reproduce that order.  If the
//     three DrawPrimitive calls come back permuted against the shipped
//     order, that is placement, not a decode error -- equal counts on both
//     sides, all indirect, is the known placement pattern.
//
//  ----------------------------------------------------------------------
//  COIN-FLIPS -- both spellings, so one build settles each.
//
//  (1) THE `bTransform` MERGE.  0x10012683 jumps from the end of the
//      0x1000f0a0 arm to 0x100126b0, which is INSIDE the `if (b80)` whose
//      test is at 0x100126a9; the 0x1000f1b0 arm falls into that test.
//      Written above as
//
//          bTransform = b80;
//          ... if (bMoved) { ...; bTransform = LTTRUE; } else { ... }
//          if (bTransform && !bEnvMap) { ... }
//
//      which is the only spelling found that produces that shape without
//      a goto (icl folds the constant TRUE and jumps past the test).  The
//      literal alternative, if the merge comes back wrong, is
//
//          if (bMoved) { ...; goto L_Env; }
//          ...
//          if (b80) { L_Env: if (!bEnvMap) { ... } }
//
//      -- a goto INTO the block, which is legal C and exactly the bytes.
//
//  (2) THE INTERPOLANT'S AND THE RECIPROCAL'S TYPE.  All six planes spill
//      fT with `fstpl` and reload it with `fmull`, and the reciprocal is
//      spilled with `fstl -0x80(%ebp)` and reloaded with `fldl`.  Both are
//      64-bit, and big16324.cpp records the same for its own fT and still
//      chose `float` because d3dr3a.cpp's out-of-line twin has the same
//      source with the value never spilled.  Kept `float` here for
//      consistency with that file.  The alternative is the one-word change
//
//          double  fDenom, fT, fZ;      /* in all six blocks */
//          double  fOOW;
//
//      and it is worth one build, because if they really are doubles every
//      multiply operand type changes with them.
//
//  (3) THE EPSILON TEST'S OPERAND ORDER.  Each plane emits
//      `flds -1e-5f ; fcomp %st(1) ; jbe <second test>` and, out of line,
//      `flds +1e-5f ; fcomp %st(1) ; jb <divide>`, i.e. the CONSTANT is
//      the x87 stack top in both.  Written as big16324.cpp writes it,
//
//          if (fDenom < -0.00001f || fDenom > 0.00001f) fT = n / d;
//          else fT = 0.0f;
//
//      The spelling that renders the constant-first comparison and the
//      `jbe`/`jb` equality edges literally is the inverted form
//
//          if (-0.00001f <= fDenom && 0.00001f >= fDenom) fT = 0.0f;
//          else fT = n / fDenom;
//
//      Only the second changes the sense of the two branches; try that one
//      and nothing else.
//
//  (4) THE ELEMENT FIELD ORDER.  0x10012780..0x1001278c stores +0x04,
//      +0x0c, +0x08 and 0x10014314..0x10014320 stores +0x04, +0x0c, +0x08
//      as well -- both one rotation off the declaration order.  Written
//      +0x04, +0x08, +0x0c above, taking the odd one as store scheduling
//      between three independent slots.  The literal alternative is to
//      swap the middle two statements in BOTH places:
//
//          pElem->m_n04 = g_1008d520;
//          pElem->m_n0c = bMoved;
//          pElem->m_n08 = nVerts;
//
//  (5) THE FLAG WORD'S TYPE AT THE TERNARY.  0x10012792 tests b88 as a
//      BYTE and 0x100127ac stores a DWORD, so `pElem->m_n10 = b88 ?
//      g_1008b874 : 0;` is written with a uint32 member.  If the shipped
//      shape wants a statement instead of a ternary it is
//
//          if (b88) pElem->m_n10 = g_1008b874; else pElem->m_n10 = 0;
//
//      which stores twice rather than selecting; the shipped bytes select
//      into %edx first, so the ternary is the reading taken.
//
// *********************************************************************** //
