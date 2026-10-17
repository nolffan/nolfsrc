// *********************************************************************** //
//
//  MODULE:   W28_1002EF30_OSQIPNI (.CPP)  -- /Os /Qip /Qip_no_inlining
//
//  PURPOSE:  The sky draw's translation unit: 0x1002ef30/0x1002ef34, the
//            draw-one-sky-polygon pass (was big2ef34.cpp), and 0x10030a60,
//            the sky draw that calls it (was w25_10030a60_os.cpp).  Both
//            files' banners follow unchanged.
//
//  WHY ONE /Os /Qip TU (the register call):
//    * 0x10030a60 calls 0x1002ef34 with the polygon in %eax and an 8-byte
//      outgoing area it never writes -- the /Qip register entry -- and
//      0x1002ef30 is its stack shim (`mov 0x4(%esp),%eax`), so the pair is
//      external and the caller shares its TU.  The function is named for
//      the stack entry, sub_1002ef30, as sub_10026720 is.
//    * both are /Os: 0x10030a60 by measure (its padding, `xor/inc`,
//      pushed arguments), and 0x1002ef34 saves its callee-saved registers
//      with PUSHES (`push %edi ; push %edi ; push %esi ; push %ebx` at
//      0x1002ef49, the /Os prologue 0x10023130 has too), not the /O2
//      `mov %reg,K(%esp)` stores.  big2ef34.cpp built /O2 (8998 against
//      6951 bytes).
//
//  MEASURED.  0x1002ef30, the shim, byte-identical (new row).
//  0x1002ef34 8291 -> 6572 bytes differing (7505 against 6955 bytes; it
//  was 8998 at /O2).  0x10030a60 now calls 0x1002ef34 the register way and
//  is 1228 = 1228 bytes, instruction count equal; what is left is one
//  block-layout difference in the BSP walk: shipped keeps the `> 0.0f`
//  (draw) arm as the fall-through and SINKS the `iSide = 0` arm below the
//  epilogue, ours lays the zero arm inline and jumps over it.  Its byte
//  count went 35 -> 111 only because the jump displacements now land
//  differently (the /Os-without-/Qip copy had the same layout difference
//  plus the stack call).  Tried without effect on that layout: the
//  negated test with the arms swapped, `<= 0.0f`, `0.0f < d`, iSide
//  preset to 0 or to 1, iSide as the comparison's value, the draw call
//  written out in place of DrawSkyPoly.  /Qip_no_inlining is the naming
//  convention's only /Os /Qip choice; the file has no inlining at stake.
//
// *********************************************************************** //

#include "ltbasedefs.h"

// Round to nearest in the current FPU mode (w1_10001000.cpp's helper;
// 0x10015190 out of line): icl inlines a function holding __asm as
// `fstps f ; flds f ; fistpl n` -- the clip passes' thirty bare `fistpl`.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}


struct D3DDevice7;

struct D3DDevice7Vtbl
{
	void*   m_pad_00[20];                               // +0x00 .. +0x4c
	long (__stdcall* SetRenderState)(D3DDevice7*, uint32 nState,
	                                 uint32 nValue);    // +0x50
	long (__stdcall* GetRenderState)(D3DDevice7*, uint32 nState,
	                                 uint32* pValue);   // +0x54
	void*   m_pad_58[3];                                // +0x58 .. +0x60
	long (__stdcall* DrawPrimitive)(D3DDevice7*, uint32, uint32, void*,
	                                uint32, uint32);    // +0x64
	void*   m_pad_68[9];                                // +0x68 .. +0x88
	long (__stdcall* SetTexture)(D3DDevice7*, uint32, void*);  // +0x8c
	long (__stdcall* GetTextureStageState)(D3DDevice7*, uint32 nStage,
	                                       uint32 nType,
	                                       uint32* pValue);  // +0x90
	long (__stdcall* SetTextureStageState)(D3DDevice7*, uint32 nStage,
	                                       uint32 nType,
	                                       uint32 nValue);   // +0x94
};

struct D3DDevice7
{
	D3DDevice7Vtbl* lpVtbl;
};

extern D3DDevice7*      g_pD3DDevice;                   // 0x1008d988


// *********************************************************************** //
//
//  MODULE:   BIG_2EF34 (.CPP)
//
//  PURPOSE:  One function -- 0x1002ef34, 6,951 bytes.  ONE caller,
//            0x10030ec6, inside the dispatcher at 0x10030a60.
//
//  ------------------------------------------------------------------- //
//  THE EXTENT.  Verified before a byte was decoded.
//  ------------------------------------------------------------------- //
//
//  The stated range 0x1002ef34..0x10030a5b is CORRECT.  Four checks:
//
//    * ONE `ret` in the whole range, at 0x10030891, and ONE
//      `push %ebp ; mov %esp,%ebp`, at 0x1002ef34.  No second prologue,
//      no second epilogue, no `leave`.
//    * EVERY branch target inside the range is inside the range.  A script
//      over the 87 distinct jump targets in the disassembly found nothing
//      below 0x1002ef9f and nothing above 0x10030a56.
//    * 0x10030a5b..0x10030a5f is five `nop` bytes and 0x10030a60 is
//      `push %ebp ; mov %esp,%ebp ; sub $0x830,%esp` -- the next function
//      (and the one that CALLS this one, at 0x10030ec6).
//    * The 458 bytes after the `ret` are /O2 cold-block sinking, not a
//      second function: the block at 0x10030892 is a two-byte `jmp
//      0x1003088b` INTO the epilogue, the six blocks at 0x10030894,
//      0x100308ba, 0x100308e2, 0x1003090a, 0x10030932 and 0x10030957 are
//      the six clip epsilon second halves and each jumps BACKWARDS into
//      its own pass, and 0x1003097a, 0x100309b0, 0x10030a24, 0x10030a48,
//      0x10030a4f and 0x10030a56 all use this frame and jump back to
//      0x100307a4 / 0x100306ae / 0x10030694 / 0x1003088b.  The last
//      instruction in the range, 0x10030a56, is `jmp 0x1003088b`.
//
//  ------------------------------------------------------------------- //
//  WHAT IT IS
//  ------------------------------------------------------------------- //
//
//  d3d.ren's DRAW-ONE-SKY-POLYGON pass -- the 32-byte / one-texture-stage
//  sibling of big6d910.cpp (0x1006d910, 40-byte / two-stage).  Six tells:
//
//    * 0x1008cb5c, read at 0x100305e4, is the console variable
//      **ShowSkySplits** (big21f90.cpp named it).  When it is set the
//      function UNBINDS the stage's texture and draws the polygon
//      untextured -- which is exactly what "show sky splits" means.
//    * the outgoing DrawPrimitive at 0x1003084f is vtable +0x64 with
//      primitive type **6** (D3DPT_TRIANGLEFAN) and FVF **0x1c4** =
//      XYZRHW | DIFFUSE | SPECULAR | TEX1;
//    * every vertex stride is 0x20 = 32 bytes (`shl $0x5`, `sar $0x5`,
//      `add $0x20`, `rep movsl` of 8 dwords), exactly that FVF, and the
//      six clip passes interpolate TWO texture floats (+0x18, +0x1c),
//      which the two-stage vertex does not;
//    * the vertex array runs -0x15fc(%ebp)..-0x5fd(%ebp) = 0x1000 bytes =
//      **128 * 32**, and the second thing the function does is
//      `sub_1000a740(nCount)`, whose body is `cmpl $0x80,arg ; jg` over
//      "Error: vertex buffer overflow".  The bound is read off the frame
//      AND confirmed by the callee;
//    * the source vertices handed back by 0x1000a700 are 0x18 = 24 bytes
//      each (`lea 0x18(%eax),%eax` at 0x1002f04d) and their +0x00 is a
//      POINTER dereferenced for three floats -- a polygon's vertex-pointer
//      list, the same shape big6d910.cpp found;
//    * 0x1002f08c calls 0x1002ec60, which d3dr3d.cpp already decoded as
//      "per-vertex dynamic lighting in DOUBLE precision ... adds
//      (1 - dist/radius)-weighted colour into three ints taken from the
//      vertex's own +0x10/+0x11/+0x12 bytes".  This is its ONLY caller.
//
//  It returns VOID: the single epilogue at 0x1003088b sets no %eax and the
//  caller at 0x10030ecb ignores it.
//
//  THE ARGUMENT ARRIVES IN %eax.  The prologue is
//      push %ebp ; mov %esp,%ebp ; push %eax
//      mov $0x16fc,%eax ; call 0x100805a0        <- _chkstk, frame 5,884
//      mov 0x16fc(%esp),%eax                     <- the pushed %eax back
//      mov %eax,-0x68(%ebp)
//  and the caller does `mov (%esi),%eax ; add $0xfffffff8,%esp ; call` --
//  it opens an eight-byte outgoing area it never writes.  It is
//  decoded as an ordinary one-argument call and move on, which is what
//  is done below.
//
//  ------------------------------------------------------------------- //
//  COMPILER VERDICT: Intel C/C++ 4.0, **/O2**, and a /QIfist TU.
//  ------------------------------------------------------------------- //
//
//  FOR /O2:
//    * COLD-BLOCK SINKING BELOW THE EPILOGUE, ELEVEN-fold (listed in the
//      extent section).  That is the 458 bytes after the `ret`.
//    * OUTGOING ARGUMENTS as `add $-N,%esp` then `mov %reg,K(%esp)` at
//      0x1002efb0, 0x1002f06f, 0x10030602, 0x10030717, 0x10030744,
//      0x10030785, 0x1003084f and 0x10030986.
//    * `&p->m` as `mov $K,%reg ; add %p,%reg` -- 0x1002f228, 0x1002f46e,
//      0x1002f580, 0x10030765 (`mov $0x1008e664,%edx`) and the four twins
//      in passes 3..6 all do `mov $0x20,%eax ; add %edi,%eax` for
//      `pOut++`, never `lea 0x20(%edi),%eax`.
//
//  AGAINST (recorded, not explained away):
//    * six outgoing arguments ARE pushed -- 0x1002ef49..0x1002ef5c (the
//      three for 0x1000a700 plus three alignment pads), 0x1002ef74,
//      0x100309b4..0x100309bb and 0x10030a24..0x10030a2b.  Exactly the
//      same mixture big6d910.cpp records, and for the same reason: the
//      `add/mov` form appears where a slot must be computed.
//    * three `lea` forms survive: `lea -0x5f0(%ebp),%esi` (and its five
//      twins), `lea 0x1c(%ebx),%ecx` / `lea 0x1c(%ebx),%edi` at
//      0x100306d0/0x100306dc and `lea 0x14(%ebx),%edi` at 0x100307b8.
//
//  /QIfist: all THIRTY byte-lerp conversions are a bare
//  `fstps tmpf ; flds tmpf ; fistpl tmpi ; mov tmpi,%dl ; mov %dl,N(%eax)`
//  with NO fnstcw/fldcw pair and no helper call -- 0x1002f30c is the
//  first.  d3dbatchd.cpp records the same signature for 0x10023380.
//
//  *** THE PER-TU INFERENCE BELOW WAS WRONG AND IS STRUCK. ***  This file
//  used to argue that because 0x1002ec60 -- the function this one CALLS --
//  does the full control-word dance while these thirty sites are bare, the
//  two must be in different translation units and the flag is per-TU.
//  0x1003e620 falsifies it: that ONE function contains BOTH idioms, thirty
//  bare `fistpl` in its clipper and three full `fnstcw / or $0xc00 / fldcw
//  / fistpll / fldcw` in its light block.  One function is one TU and one
//  flag, so the difference cannot be a flag.  It tracks the CAST -- a
//  32-bit convert off a float slot against a 64-bit one off a double.
//  Under plain /O2 those thirty sites will still differ in bytes; none of
//  them contains a call.
//
//  If the file is split, the whole of it wants ONE flag -- there is only
//  one function in it.  /O2 first, /Os second.
//
//  ------------------------------------------------------------------- //
//  THE ACCEPTANCE FIGURE AND WHAT IT DOES *NOT* WITNESS.  READ FIRST.
//  ------------------------------------------------------------------- //
//
//  The call count over the range: 6,951 bytes, 18 calls (10 direct, 8 indirect),
//  **386 bytes per call**, across **7 distinct direct callees**.  Anything
//  over 300 B/call is in the weak band, so the sequence
//  agreeing is necessary and nowhere near sufficient.  The shipped
//  sequence, in address order, is
//
//      0x100805a0 (_chkstk)   0x1002ef3d
//      0x1000a700             0x1002ef66
//      0x1000a740             0x1002ef75
//      0x10015190             0x1002efcc
//      0x10015190             0x1002eff3
//      0x10015190             0x1002f01a
//      0x1000d9d0             0x1002f03f
//      0x1002ec60             0x1002f08c
//      *0x8c   (indirect)     0x1003061b
//      *0x8c   (indirect)     0x10030733
//      *0x50   (indirect)     0x1003075e
//      *0xbc   (indirect)     0x10030798
//      *g_1008ce88 (indirect) 0x100307c5
//      *0x64   (indirect)     0x10030888
//      *0xbc   (indirect)     0x1003099c
//      0x10039bd0             0x100309bc
//      *0x8c   (indirect)     0x10030a0b
//      0x10039bd0             0x10030a2c
//
//  and the source below emits exactly that order, with ONE expected
//  difference: icl generates its own `__chkstk` for a 5,884-byte frame and
//  the call-sequence comparison cannot resolve that symbol to 0x100805a0 -- big124a4.cpp and
//  big6d910.cpp both record the same.  So the expected result is
//  **17 of 18 agreeing, with one shipped-only or by-position-only row, and
//  that row is _chkstk**.
//
//  THE GAPS BETWEEN CONSECUTIVE CALLS, largest first, and what pins each:
//
//  * **5,506 bytes, 0x1002f091..0x1003061b** -- from the return of
//    0x1002ec60 to the first indirect call.  **79.2% of the function, and
//    the call-sequence comparison cannot see a byte of it.**  It is the inline transform plus
//    the six-plane clipper.  What pins it instead, none of it guessed:
//
//      - the clipper is SIX near-identical blocks at 0x1002f16e,
//        0x1002f4d9, 0x1002f856, 0x1002fbcb, 0x1002ff40 and 0x100302a2,
//        each opening with `incl 0x1008be44` and gated by mask bits
//        **0x01, 0x04, 0x08, 0x10, 0x20, 0x02 IN THAT ORDER**
//        (0x1002f160, 0x1002f4d0, 0x1002f84d, 0x1002fbc2, 0x1002ff37,
//        0x10030299), testing
//            z >= g_f1008b908 | -z < x | y < z | x < z | -z < y
//            | z <= g_f1008b910
//        read off six separate classify loops (0x1002f19c fcomps/jae,
//        0x1002f50b fchs+fcomps/jb, 0x1002f888 fcomps/jb, 0x1002fbfc
//        fcomps/jb, 0x1002ff72 fchs+fcomps/jb, 0x100302cf fcomps/jbe).
//        That is the SAME six planes in the SAME order that big6d910.cpp,
//        big5a960.cpp, big61a30.cpp, big124a4.cpp and d3dr3a.cpp have each
//        decoded independently -- this is the SIXTH reading of one source
//        and it was diffed against big6d910.cpp expression by expression.
//        The only differences are the reject target (`return;` here) and
//        that this vertex carries TWO texture floats, not four;
//      - the epsilon triples were DUMPED, not assumed: 0x100a67cc,
//        0x100a67d4, 0x100a67dc, 0x100a67e4, 0x100a67ec and 0x100a67f4 are
//        all -9.999999747e-06, and 0x100a682c, 0x100a6824, 0x100a681c,
//        0x100a6814, 0x100a680c and 0x100a6804 are all +9.999999747e-06
//        with a 0.0f immediately after each;
//      - the ARRAY BOUND is read off the frame, not guessed: the six flag
//        arrays are at -0x5f0, -0x514, -0x438, -0x35c, -0x280 and -0x1a4,
//        exactly **0xdc apart**, of which 0xc8 = 200 bytes = **50 int32**
//        is the array and 0x14 is the five int slots the five byte-lerps
//        need (pass 1's are -0x528, -0x524, -0x520, -0x51c, -0x518, which
//        is exactly -0x5f0 + 0xc8 onwards; pass 6's are -0xdc..-0xcc,
//        which is -0x1a4 + 0xc8 onwards).  Identical to big6d910.cpp's and
//        big5a960.cpp's finding;
//      - the FIVE BYTE LERPS are +0x12, +0x11, +0x10, +0x13, +0x17 in that
//        order in ALL SIX passes -- thirty sites, every one of them read
//        (0x1002f2e2.., 0x1002f65f.., 0x1002f9d4.., 0x1002fd49..,
//        0x100300ba.., 0x10030403..);
//      - the inline transform at 0x1002f0aa..0x1002f12c reads TWELVE
//        adjacent floats at 0x100900bc..0x100900e8 in three groups of four
//        and writes +0x08, +0x00, +0x04 -- a 3x4 row-major matrix applied
//        to the position.  Its store order is pinned by a single
//        `fxch %st(2)`; see the coin-flip note.
//
//  * **394 bytes, 0x1002f08c..0x1003061b is bridged only by that one
//    stretch**; the next largest gap is **250 bytes, 0x100307c5..0x10030888**
//    -- the fog callback to the DrawPrimitive.  It holds the screen
//    projection (six floats at 0x1009027c..0x10090290 and `flds 1.0f ;
//    fdivs z`, the 1.0f at 0x100a6800 dumped) and the texture-scale
//    multiply by ELEMENT ZERO of the per-stage pair -- 0x1008e664 and
//    0x1008e668 are exactly what 0x10030770 writes with an index, the
//    same fold big61a30.cpp and big6d910.cpp record.
//
//  * **196 bytes, 0x1002ef75..0x1002efcc** and the 269 bytes to 0x1002f03f
//    are the fill loop, witnessed by four of its own calls.
//
//  * **NO `// UNDECODED:` GAPS.**  Every byte of 0x1002ef34..0x10030a5a is
//    accounted for by a region below.
//
// *********************************************************************** //

// *********************************************************************** //
//  REGION MAP
// *********************************************************************** //
//
//  0x1002ef34..0x1002ef99  PROLOGUE, the geometry fetch and the overflow
//      guard.  `mov $0x16fc,%eax ; call 0x100805a0` is _chkstk, so the
//      frame is 5,884 bytes; four `push`es follow at 0x1002ef49..0x1002ef4c
//      (%edi TWICE, then %esi and %ebx) against three `pop`s at
//      0x1003088b..0x1003088d (%ebx, %esi, %edi), with `mov %ebp,%esp`
//      covering the difference -- icl's dead 4-byte slot.  Then
//      `sub_1000a700(pObj, &pSrc, &nCount)` and
//      `sub_1000a740(nCount)`; the failure `return;` goes through the
//      two-byte trampoline at 0x10030892.
//
//  0x1002ef99..0x1002f069  THE FILL.  aVerts[i] built from pSrc[i]: the
//      position through the source's +0x00 POINTER as a STRUCT ASSIGNMENT
//      (`rep movsl` of 3 dwords at 0x1002efa8 -- big6d910.cpp's copy of
//      this loop emits three separate `mov`s, so that one assigns fields
//      and this one assigns the vector), then three colour bytes scaled by
//      g_f1008d730/734/738 and rounded through 0x10015190, the alpha byte
//      from g_1008be60, and the texture pair through the __thiscall setter
//      0x1000d9d0.  pSrc is RE-READ from its frame slot after every call
//      (0x1002efaa, 0x1002efd4, 0x1002effb, 0x1002f02c, 0x1002f044)
//      because its address was handed to 0x1000a700.
//
//  0x1002f069..0x1002f091  `sub_1002ec60(pObj, aVerts, nCount)` -- the
//      per-vertex dynamic lighting d3dr3d.cpp decoded.  Four outgoing
//      slots, three of them written; the fourth is icl's alignment.
//
//  0x1002f091..0x1002f12d  THE INLINE 3x4 TRANSFORM.  Twelve floats at
//      0x100900bc..0x100900e8, three groups of four, results to +0x08,
//      +0x00, +0x04.  Software-pipelined: the +0x00 and +0x04 stores are
//      sunk across the back edge (`fstps -0x20(%eax)` / `fstps -0x1c(%eax)`
//      at the TOP of the next iteration, peeled once at 0x1002f127).
//
//  0x1002f12d..0x1002f16e  THE CLIP MASK and the clipper's four opening
//      assignments.  `nClip = 0x3f; if (g_p1008d7a0) nClip = 1;` -- a
//      BYTE local at -0x6c(%ebp) (`mov $0x3f,%al`, `xor %al,%al ; inc %al`,
//      and six `test $BIT,%dl` reads).  This is big5a960.cpp's UseD3DClip
//      test written the other way up: there the mask came from
//      g_1008b874 and was ANDed with 1; here there is no incoming mask at
//      all and the two constants are spelled out.
//
//  0x1002f16e..0x1002f4cd  CLIP PASS 1, bit 0x01, NEAR: z >= g_f1008b908.
//  0x1002f4cd..0x1002f84a  CLIP PASS 2, bit 0x04, LEFT: -z < x.
//  0x1002f84a..0x1002fbbf  CLIP PASS 3, bit 0x08, TOP: y < z.
//  0x1002fbbf..0x1002ff34  CLIP PASS 4, bit 0x10, RIGHT: x < z.
//  0x1002ff34..0x10030296  CLIP PASS 5, bit 0x20, BOTTOM: -z < y.
//  0x10030296..0x100305d2  CLIP PASS 6, bit 0x02, FAR: z <= g_f1008b910.
//
//  0x100305d2..0x100305e4  `pVerts = pIn ; nVerts = nIn` -- the clipper's
//      epilogue.  Unlike big6d910.cpp there is no early exit past it: the
//      UseD3DClip case here still runs every pass's mask test and falls
//      through to this.
//
//  0x100305e4..0x10030631  ShowSkySplits.  `if (g_CV_ShowSkySplits)` ->
//      UNBIND COPY A.
//
//  0x10030631..0x100306ae  FIND THIS STAGE'S TEXTURE RECORD.  big61a30.cpp
//      and big6d910.cpp's block field for field, including the DOUBLED
//      null tests: 0x1003063c and 0x10030646 are two `je`s off the ONE
//      `test %esi,%esi` at 0x1003063a, and 0x1003065c and 0x10030664 are
//      two `je`s off the ONE `test %edi,%edi` at 0x10030656.  icl 4.0 does
//      not fold a redundant null test, so the source tests each twice.
//
//  0x100306ae..0x100307a4  BIND.  g_a1008e684[stage] published, the LRU
//      move-to-front of the +0x1c CheapLTLink through 0x1008e634,
//      `g_p1008cbcc->+0x48 +=`, SetTexture at +0x8c, ALPHAREF at +0x50,
//      the inverse-size pair into g_a1008e664[stage], the counter at
//      0x1008bd80, SetLOD at +0xbc.
//
//  0x100307a4..0x1003084f  FOG, PROJECT AND SCALE, one loop.  Per vertex:
//      the __fastcall through g_1008ce88 with the vertex in %ecx and
//      &vertex->m_Specular14 in %edx (0x10023570, the routine
//      sub_1002b0e0 stores there, writes 0x3(%edx) -- the specular alpha),
//      then rhw = 1/z and x, y, z through six floats at
//      0x1009027c..0x10090290, then the texture pair scaled by
//      g_a1008e664[0].
//
//  0x1003084f..0x10030892  DRAW AND EPILOGUE.  DrawPrimitive(TRIANGLEFAN,
//      0x1c4, pVerts, nVerts, 0) through vtable +0x64, then the ONLY
//      epilogue.  0x10030892 is a two-byte `jmp` into it.
//
//  0x10030894..0x1003097a  THE SIX SUNK CLIP EPSILON ARMS, one per pass,
//      each `flds +1e-5 ; fcomp ; jb <back into the divide> ; fstp ; fstp
//      [; fstp] ; flds 0.0f ; fstpl <fT> ; jmp <back past the divide>`.
//      That is the `else fT = 0.0f;` half of `fDen < -eps || fDen > eps`.
//
//  0x1003097a..0x100309b0  L_ElseLOD: the record was already bound to this
//      stage, so only the SetLOD survives.
//
//  0x100309b0..0x10030a24  L_MakeRec's non-empty-list arm, and UNBIND
//      COPY B at 0x100309d8.
//
//  0x10030a24..0x10030a48  L_MakeRec's empty-list arm.
//
//  0x10030a48..0x10030a56  The two `break` trampolines out of the record
//      search, `mov %eax,%edx ; jmp 0x10030694`.
//
//  0x10030a56..0x10030a5b  The clipper's `return;` trampoline, reached by
//      all twelve reject branches.
//
// *********************************************************************** //
//  COIN-FLIPS, both spellings stated so one build settles each.
// *********************************************************************** //
//
//  1. THE TRANSFORM'S STATEMENT ORDER.  Written below as three locals
//     computed Z, X, Y and then STORED z, x, y.  That is not a guess about
//     taste, it is what the single `fxch %st(2)` at 0x1002f11c forces:
//     with [Y',X',Z'] on the x87 stack, storing z then x then y costs ONE
//     `fxch %st(2)` and storing x then y then z costs ONE `fxch %st(1)`
//     -- the shipped byte is `d9 ca`, not `d9 c9`, and the first store is
//     to +0x08.  What is NOT pinned is that the three results go through
//     named locals at all rather than a struct; they must go through
//     SOMETHING, because all twelve loads of (%eax)/0x4(%eax)/0x8(%eax)
//     precede all three stores and no compiler may sink a store past a
//     load of the same field.  The alternative, if the three-local form
//     misses:
//         LTVector vT;
//         vT.z = ...;  vT.x = ...;  vT.y = ...;
//         pV->m_vPos.z = vT.z;  pV->m_vPos.x = vT.x;  pV->m_vPos.y = vT.y;
//
//  2. THE FIRST TRANSFORM ROW'S OPERAND ORDER.  Rows two and three load
//     the CONSTANT and multiply by the member in all six terms
//     (`flds 0x100900c4 ; fmuls 0x8(%eax)`), and row one does that for its
//     y and x terms but the OTHER way round for its z term
//     (`flds 0x8(%eax) ; fmuls 0x100900e4` at 0x1002f0b9) and emits the y
//     term before the z term.  All three rows are written uniformly below.
//     The literal spelling of the asymmetry, if it matters, is
//         fZ = g_a100900bc[8] * pV->m_vPos.x
//            + g_a100900bc[9] * pV->m_vPos.y
//            + pV->m_vPos.z * g_a100900bc[10]
//            + g_a100900bc[11];
//     but the far likelier explanation is scheduling: this is the first
//     expression in the loop body and swapping the pair lets the second
//     multiply issue under the first FADD.
//
//  3. THE DUPLICATED UNBIND.  0x100305ed..0x1003062c and
//     0x100309d8..0x10030a1f are the same six statements twice, and they
//     are the reason vtable slot +0x8c appears THREE times in the shipped
//     call sequence (once as the real SetTexture at 0x10030733 and once
//     per unbind).  Written twice below so the sequence matches.  The
//     alternative, very likely what the shipped source said and which icl
//     would then have tail-duplicated by itself:
//
//         if (!g_CV_ShowSkySplits)
//         {
//             pBlock = pObj->m_p28->m_pBlock30;
//             if (pBlock) { ...find, make, bind...; goto L_Draw; }
//         }
//     L_Unbind:
//         nStage = g_n1008d7d0;
//         if (g_a1008e684[nStage])
//         {
//             g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, nStage, 0);
//             g_a1008e684[nStage] = LTNULL;
//         }
//         goto L_Draw;
//
//     If a build shows icl emitting two copies from that, it is the better
//     spelling.  Note that the shipped cross-jump at 0x10030646 lands at
//     0x100309e0 -- INSIDE copy B, one instruction past its
//     `nStage = g_n1008d7d0` -- which is exactly what a compiler does when
//     it duplicates a block and finds nStage already live.
//
//  4. `float fT` vs `double fT`.  All six passes spill the parameter with
//     `fstpl` and reload it with `fmull` (0x1002f28b/0x1002f295 and the
//     five twins).  That is an 8-byte slot, which a `double` would give
//     directly -- but so does an icl x87 register spill of a `float`
//     expression, and big5a960.cpp, big124a4.cpp and big6d910.cpp all
//     wrote `float` and got the same bytes.  `double fT;` is the
//     one-token alternative.
//
//  5. THE HAND-PLACED COLD BLOCKS.  Five arms are written below as `goto`
//     targets laid out at the BOTTOM of the function in the SHIPPED
//     ADDRESS ORDER -- L_ElseLOD (0x1003097a), L_MakeRec (0x100309b0),
//     L_UnbindB (0x100309d8) and the reject (0x10030a56).  This is
//     big5a960.cpp's and big6d910.cpp's technique and it is here for one
//     reason: it makes the EMITTED call sequence match without depending
//     on icl re-deriving the same sinking decisions.  The structured
//     spelling is the obvious if/else; only the block placement differs.
//
//  6. THE TEXTURE-SCALE MULTIPLY ORDER.  0x1003082e loads the vertex's u
//     and does `fmuls 0x1008e664`, and 0x10030836 does the same for v with
//     0x1008e668 -- both member-left this time (unlike big6d910.cpp's copy,
//     where the second was constant-left).  Written as `*=` for both.
//
//  7. THE RECIPROCAL IN THE PROJECTION LOOP.  Written `pWalk->m_fRHW`
//     in all three divides, because two of the three RELOAD it from the
//     vertex (`fmuls 0xc(%ebx)` at 0x10030816 and 0x10030828) and only the
//     first uses the value still on the x87 stack -- a memory reload is
//     evidence of a member read, and icl forwarding one just-stored value
//     is ordinary.  The alternative, if that misses, is a named local:
//         float fOOZ = 1.0f / pWalk->m_vPos.z;
//         pWalk->m_fRHW = fOOZ;
//         pWalk->m_vPos.x = (...) * fOOZ;   /* and y, z */
//     which puts the same three multiplies against one value.
//
// *********************************************************************** //
//  WHAT IS *NOT* PINNED
// *********************************************************************** //
//
//  * THE SIX DEAD 1.0f CONSTANTS interleaved with the epsilons
//    (0x100a67d0, 0x100a67d8, 0x100a67e0, 0x100a67e8, 0x100a67f0,
//    0x100a67f8 are all 1.0f and NONE of them is loaded by this function;
//    0x100a67fc is a seventh).  big5a960.cpp and big61a30.cpp found the
//    same dead slots in their copies of this constant block.  Nothing
//    below produces them and nothing below needs to.
//  * THE FRAME SLOT ORDER.  icl overlaps -0x70/-0x78/-0x80/-0x88/-0x90/
//    -0x98/-0xa0 between the pointer/counter roles and the six `fT`
//    doubles across the six passes; nothing in the source controls that.
//  * THE VERTEX'S SPECULAR DWORD (+0x14) is written only by the __fastcall
//    at 0x100307c5 (which writes its byte +0x3) and interpolated at +0x17
//    by all six clip passes.  The fill loop leaves it alone, and so does
//    the reconstruction.  Its +0x0c (RHW) is likewise written only by the
//    projection loop.
//  * WHAT 0x1000a700's THIRD OUT-PARAMETER IS COUNTED IN.  It is used as
//    an unsigned loop bound (`test %eax,%eax ; jbe` at 0x1002ef91) and as
//    a signed one after the first clip pass (`test %ecx,%ecx ; jle` at
//    0x1002f1f8), so both `uint32` and `int32` appear below exactly where
//    the shipped compares put them.
//
// *********************************************************************** //




// ---------------------------------------------------------------------- //
//  The D3D7 device.  Three vtable slots are reached: +0x50 SetRenderState
//  (0x1003075e), +0x64 DrawPrimitive (0x10030888) and +0x8c SetTexture
//  (0x10030733, 0x1003061b and 0x10030a0b).  Same declaration
//  big61a30.cpp and big6d910.cpp use.
// ---------------------------------------------------------------------- //
// (The D3D7 device is declared once, at the top of this file.)


// IDirectDrawSurface7: only +0xbc, SetLOD (0x10030798 and 0x1003099c).
struct DDSurface;

struct DDSurfaceVtbl
{
	uint8			m_pad_00[0xbc];
	long (__stdcall* SetLOD)(DDSurface*, uint32);                   // +0xbc
};

struct DDSurface
{
	DDSurfaceVtbl*	lpVtbl;
};


// ---------------------------------------------------------------------- //
//  The 32-byte vertex.  FVF 0x1c4 = XYZRHW | DIFFUSE | SPECULAR | TEX1,
//  read straight off the DrawPrimitive at 0x10030857, and 0x20 is every
//  stride in the function.  Field for field big6d910.cpp's Vert6d910
//  minus its second texture pair -- the six clip passes here interpolate
//  +0x18 and +0x1c only.
// ---------------------------------------------------------------------- //
union VColor2ef34
{
	uint32		m_n;
	uint8		m_b[4];
};

struct Vec2ef34
{
	float	x;                                          // +0x00
	float	y;                                          // +0x04
	float	z;                                          // +0x08
};

struct Vert2ef34
{
	// 0x1000d9d0 -- d3dbatchb.cpp's 21-byte two-field __thiscall setter
	// (`push %esi ; mov 0x8(%esp),%eax ; mov 0xc(%esp),%edx ;
	//   mov %eax,0x18(%ecx) ; mov %edx,0x1c(%ecx) ; add $0x4,%esp ;
	//   ret $0x8`), called at 0x1002f03f with the vertex in %ecx.
	void			sub_1000d9d0(float fU, float fV);

	Vec2ef34		m_vPos;                             // +0x00
	float			m_fRHW;                             // +0x0c
	VColor2ef34		m_Diffuse10;                        // +0x10
	VColor2ef34		m_Specular14;                       // +0x14
	float			m_fU0;                              // +0x18
	float			m_fV0;                              // +0x1c
};


// ---------------------------------------------------------------------- //
//  The SOURCE vertex handed back by 0x1000a700.  Stride 0x18 from
//  `lea 0x18(%eax),%eax` at 0x1002f04d.  +0x00 is a POINTER: 0x1002ef9f
//  does `mov (%eax),%esi` and then copies three dwords through it.
//  +0x04/+0x08 go to the vertex's +0x18/+0x1c and the three bytes
//  +0x14/+0x15/+0x16 to its +0x10/+0x11/+0x12.  +0x0c..+0x13 and +0x17
//  are never touched and exist only because the stride says 24.
// ---------------------------------------------------------------------- //
struct SrcVert2ef34
{
	Vec2ef34*	m_pPos00;                               // +0x00
	float		m_fU04;                                 // +0x04
	float		m_fV08;                                 // +0x08
	uint8		m_pad_0c[0x14 - 0x0c];
	uint8		m_nBlue14;                              // +0x14
	uint8		m_nGreen15;                             // +0x15
	uint8		m_nRed16;                               // +0x16
	uint8		m_pad_17;                               // +0x17
};


// ---------------------------------------------------------------------- //
//  ltlink.h's CheapLTLink, as the texture record's +0x1c member.  The
//  dl_Remove / dl_TieOff / dl_Insert sequence at 0x100306ca..0x100306fd
//  reads +0x00 as prev and +0x04 as next.
// ---------------------------------------------------------------------- //
struct Link2ef34
{
	Link2ef34*	m_pPrev;                                // +0x00
	Link2ef34*	m_pNext;                                // +0x04
	void*		m_pData;                                // +0x08
};


// The per-stage texture record, field for field big61a30.cpp's Tex61a30
// and big6d910.cpp's Tex6d910.  Its vtable is never called from here, so
// slot 0 is a plain pointer.
struct Tex2ef34
{
	void*		m_pVtbl00;                              // +0x00
	float		m_fInvW04;                              // +0x04
	float		m_fInvH08;                              // +0x08
	DDSurface*	m_pSurface0c;                           // +0x0c
	uint32		m_nBytes10;                             // +0x10
	uint16		m_nStamp14;                             // +0x14
	uint16		m_nAlphaRef16;                          // +0x16
	uint8		m_pad_18[0x1c - 0x18];
	Link2ef34	m_Link1c;                               // +0x1c
	uint8		m_pad_28[0x30 - 0x28];
	Tex2ef34*	m_pNext30;                              // +0x30
	uint8		m_pad_34[0x42 - 0x34];
	uint16		m_nStage42;                             // +0x42
	uint16		m_nDirty44;                             // +0x44
};


// The block reached through pObj->m_p28->m_pBlock30: a list head at +0x0c
// and a sixteen-bit stamp at +0x30 (`mov %ax,0x30(%esi)` at 0x10030658).
struct TexBlock2ef34
{
	uint8			m_pad_00[0x0c];
	Tex2ef34*		m_pHead0c;                          // +0x0c
	uint8			m_pad_10[0x30 - 0x10];
	uint16			m_nStamp30;                         // +0x30
};


// The pair published per stage at 0x1008e664 -- `mov %ecx,(%edx,%eax,8)`
// and `mov %esi,0x4(%edx,%eax,8)` at 0x10030770/0x10030773.
struct TexScale2ef34
{
	float		m_fInvW00;                              // +0x00
	float		m_fInvH04;                              // +0x04
};


// 0x1008cbcc: only +0x48 is touched here (`add %edx,0x48(%eax)`).
struct Stats2ef34
{
	uint8		m_pad_00[0x48];
	uint32		m_nBytes48;                             // +0x48
};


// ---------------------------------------------------------------------- //
//  The item.  Only two fields are read: +0x28 at 0x10030634 and, through
//  it, +0x30 at 0x10030637.  Everything else the function needs about the
//  geometry comes back out of 0x1000a700.
// ---------------------------------------------------------------------- //
struct TexHolder2ef34
{
	uint8			m_pad_00[0x30];
	TexBlock2ef34*	m_pBlock30;                         // +0x30
};

struct Obj2ef34
{
	uint8			m_pad_00[0x28];
	TexHolder2ef34*	m_p28;                              // +0x28
};


// ---------------------------------------------------------------------- //
//  Externals.  Relocations are masked, so only the shapes matter.
// ---------------------------------------------------------------------- //

extern uint16			g_w1008b870;                    // 0x1008b870
extern Vert2ef34*		g_p1008b87c;                    // 0x1008b87c
extern float			g_f1008b908;                    // 0x1008b908  near
extern float			g_f1008b910;                    // 0x1008b910  far
extern uint32			g_n1008bd80;                    // 0x1008bd80
extern uint32			g_n1008be44;                    // 0x1008be44
extern uint8			g_b1008be60;                    // 0x1008be60
extern uint16			g_w1008bea8;                    // 0x1008bea8
extern int32			g_CV_ShowSkySplits;             // 0x1008cb5c
extern Stats2ef34*		g_p1008cbcc;                    // 0x1008cbcc
extern float			g_f1008d730;                    // 0x1008d730
extern float			g_f1008d734;                    // 0x1008d734
extern float			g_f1008d738;                    // 0x1008d738
extern void*			g_p1008d7a0;                    // 0x1008d7a0  UseD3DClip
extern uint32			g_n1008d7d0;                    // 0x1008d7d0  stage
// 0x1008e634 is the 12-byte list HEAD OBJECT, not a pointer variable.
// 0x10038a90 settles it: at 0x10038e3d it plants the IMMEDIATE $0x1008e634
// into the new link's m_pPrev and then loads 0x1008e638 -- i.e. &head and
// head.m_pNext.  This function reads the head's +0 field instead, which is
// the same instruction a pointer load would emit, so the old spelling was
// byte-correct here and wrong about what the object is.  The two functions
// also insert at opposite ends: 0x10038a90 at the front (after the head),
// this one after head.m_pPrev, i.e. at the back.
extern Link2ef34		g_Link1008e634;                 // 0x1008e634
extern TexScale2ef34	g_a1008e664[];                  // 0x1008e664
extern Tex2ef34*		g_a1008e684[];                  // 0x1008e684

// 0x100900bc .. 0x100900e8 -- twelve adjacent floats, read in three groups
// of four at 0x1002f0b0..0x1002f11b.  A 3x4 row-major matrix: row 0 goes
// to the vertex's x, row 1 to its y, row 2 to its z.
extern float			g_a100900bc[12];                // 0x100900bc

// 0x1009027c .. 0x10090290 -- six adjacent floats, the screen projection
// at 0x100307ee..0x10030822.  Not contiguous with the block above.
extern float			g_f1009027c;                    // 0x1009027c
extern float			g_f10090280;                    // 0x10090280
extern float			g_f10090284;                    // 0x10090284
extern float			g_f10090288;                    // 0x10090288
extern float			g_f1009028c;                    // 0x1009028c
extern float			g_f10090290;                    // 0x10090290

// 0x1008ce88 -- the fog/specular callback.  %ecx then %edx, in that order,
// is __fastcall; d3dr2d.cpp pinned that global across fourteen call sites
// and big17c14.cpp / big61a30.cpp / big64cc0.cpp all declare it this way.
// sub_1002b0e0 stores 0x10023570 there, whose body reads 0x8(%ecx) (the
// vertex's z) and writes 0x3(%edx).
typedef void (__fastcall* PFN_1008ce88)(Vert2ef34* pVert, VColor2ef34* pOut);

extern PFN_1008ce88		g_pfn_1008ce88;                 // 0x1008ce88

// 0x1000a700 -- d3dbatchd.cpp, 49 bytes, 10 callers.  Hands back where a
// piece of geometry's vertex records are and how many there are.
extern "C" void  sub_1000a700(Obj2ef34* pObj, void** ppOutData,
                              uint32* pnOutCount);

// 0x1000a740 -- d3dbatchd.cpp.  `cmpl $0x80,arg ; jg` over "Error: vertex
// buffer overflow"; returns 1 when the count fits.
extern "C" int32 sub_1000a740(uint32 nVerts);

// 0x10015190 -- d3dbatchd.cpp's round-to-nearest float-to-int, __cdecl,
// one float on the stack, result in %eax.
extern "C" int32 sub_10015190(float fVal);

// 0x1002ec60 -- d3dr3d.cpp: per-vertex dynamic lighting in double
// precision.  __cdecl, three arguments; 0x1002f08c is its ONLY caller.
extern "C" void  sub_1002ec60(Obj2ef34* pObj, Vert2ef34* pVerts,
                              uint32 nVerts);

// 0x10039bd0 -- d3dmid.cpp's stack-ABI entry; the body is at 0x10039bdc.
extern "C" Tex2ef34* sub_10039bd0(TexBlock2ef34* pBlock, uint32 nStage,
                                  uint32 bLink);


// 0x1002ef5d: aVerts is -0x15fc(%ebp) and the pointer above it is
// -0x5fc(%ebp), so the array is 0x1000 bytes = 128 * 32.  0x1000a740's own
// `cmpl $0x80` agrees.
#define VERT2EF34_MAX	128


// ======================================================================= //
//  THE CLIPPER -- 0x1000f4b0 and its six plane bodies, which the shipped
//  function INLINES (0x1002f14e .. 0x100305d2).  The bodies are the ones
//  w1_10001000.cpp / w44_10001670.cpp compile byte-identical out of line,
//  on this file's 32-byte Vert2ef34; the driver is d3dr3c_os.cpp's 0x1000f4b0.
//  Named here by plane, not by address: in a /Qip TU icl also emits each
//  helper out of line (unreferenced; the link drops them), and an address
//  in the name would make the byte comparison report those copies as the shipped
//  0x10001000.. rows.  The planes are, in order, 0x10001000 near,
//  0x10001330 left, 0x10001670 top, 0x10001990 right, 0x10001cb0 bottom,
//  0x10001fe0 far; ClipPoly is 0x1000f4b0.  Inlined, the caller's
//  pVerts / nVerts are address-taken, which is what fixes the frame.
// ======================================================================= //
#define CLIP_EPSILON    0.00001f
#define CLIP_MAX_VERTS  50

static inline void LerpVert2ef34(Vert2ef34* a, Vert2ef34* b, Vert2ef34* dst, float t)
{
	dst->m_fU0 = (b->m_fU0 - a->m_fU0) * t + a->m_fU0;
	dst->m_fV0 = (b->m_fV0 - a->m_fV0) * t + a->m_fV0;
	dst->m_Diffuse10.m_b[2]  = (uint8)RoundF((b->m_Diffuse10.m_b[2]  - a->m_Diffuse10.m_b[2])  * t + a->m_Diffuse10.m_b[2]);
	dst->m_Diffuse10.m_b[1]  = (uint8)RoundF((b->m_Diffuse10.m_b[1]  - a->m_Diffuse10.m_b[1])  * t + a->m_Diffuse10.m_b[1]);
	dst->m_Diffuse10.m_b[0]  = (uint8)RoundF((b->m_Diffuse10.m_b[0]  - a->m_Diffuse10.m_b[0])  * t + a->m_Diffuse10.m_b[0]);
	dst->m_Diffuse10.m_b[3]  = (uint8)RoundF((b->m_Diffuse10.m_b[3]  - a->m_Diffuse10.m_b[3])  * t + a->m_Diffuse10.m_b[3]);
	dst->m_Specular14.m_b[3] = (uint8)RoundF((b->m_Specular14.m_b[3] - a->m_Specular14.m_b[3]) * t + a->m_Specular14.m_b[3]);
}


static inline float ClipEdgeNear(Vec2ef34* a, Vec2ef34* b, Vec2ef34* dst)
{
	float fDenom = b->z - a->z;
	float t;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = (g_f1008b908 - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	dst->y = (b->y - a->y) * t + a->y;
	dst->z = g_f1008b908;

	return t;
}


static inline float ClipEdgeLeft(Vec2ef34* a, Vec2ef34* b, Vec2ef34* dst)
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


static inline float ClipEdgeTop(Vec2ef34* a, Vec2ef34* b, Vec2ef34* dst)
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


static inline float ClipEdgeRight(Vec2ef34* a, Vec2ef34* b, Vec2ef34* dst)
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


static inline float ClipEdgeBottom(Vec2ef34* a, Vec2ef34* b, Vec2ef34* dst)
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


static inline float ClipEdgeFar(Vec2ef34* a, Vec2ef34* b, Vec2ef34* dst)
{
	float fDenom = b->z - a->z;
	float t;

	if (fDenom < -CLIP_EPSILON || fDenom > CLIP_EPSILON)
		t = (g_f1008b910 - a->z) / fDenom;
	else
		t = 0.0f;

	dst->x = (b->x - a->x) * t + a->x;
	dst->y = (b->y - a->y) * t + a->y;
	dst->z = g_f1008b910;

	return t;
}


// 0x10001000
static inline LTBOOL ClipPlaneNear(uint32* pUnused, Vert2ef34** ppVerts, int32* pnVerts, Vert2ef34** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert2ef34*  pOutStart;
	Vert2ef34*  pV;
	Vert2ef34*  pEnd;
	Vert2ef34*  pPrev;
	Vert2ef34*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = *ppVerts;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.z >= g_f1008b908);

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
				float t = ClipEdgeNear(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpVert2ef34(pPrev, pCur, *ppOut, t);

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


// 0x10001330
static inline LTBOOL ClipPlaneLeft(uint32* pUnused, Vert2ef34** ppVerts, int32* pnVerts, Vert2ef34** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert2ef34*  pOutStart;
	Vert2ef34*  pV;
	Vert2ef34*  pEnd;
	Vert2ef34*  pPrev;
	Vert2ef34*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

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
				float t = ClipEdgeLeft(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpVert2ef34(pPrev, pCur, *ppOut, t);

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


// 0x10001670
static inline LTBOOL ClipPlaneTop(uint32* pUnused, Vert2ef34** ppVerts, int32* pnVerts, Vert2ef34** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert2ef34*  pOutStart;
	Vert2ef34*  pV;
	Vert2ef34*  pEnd;
	Vert2ef34*  pPrev;
	Vert2ef34*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

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
				float t = ClipEdgeTop(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpVert2ef34(pPrev, pCur, *ppOut, t);

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


// 0x10001990
static inline LTBOOL ClipPlaneRight(uint32* pUnused, Vert2ef34** ppVerts, int32* pnVerts, Vert2ef34** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert2ef34*  pOutStart;
	Vert2ef34*  pV;
	Vert2ef34*  pEnd;
	Vert2ef34*  pPrev;
	Vert2ef34*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

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
				float t = ClipEdgeRight(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpVert2ef34(pPrev, pCur, *ppOut, t);

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


// 0x10001cb0
static inline LTBOOL ClipPlaneBottom(uint32* pUnused, Vert2ef34** ppVerts, int32* pnVerts, Vert2ef34** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert2ef34*  pOutStart;
	Vert2ef34*  pV;
	Vert2ef34*  pEnd;
	Vert2ef34*  pPrev;
	Vert2ef34*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

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
				float t = ClipEdgeBottom(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpVert2ef34(pPrev, pCur, *ppOut, t);

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


// 0x10001fe0
static inline LTBOOL ClipPlaneFar(uint32* pUnused, Vert2ef34** ppVerts, int32* pnVerts, Vert2ef34** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert2ef34*  pOutStart;
	Vert2ef34*  pV;
	Vert2ef34*  pEnd;
	Vert2ef34*  pPrev;
	Vert2ef34*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

	nInside = 0;
	pV      = *ppVerts;
	pFlag   = anInside;
	pEnd    = pV + *pnVerts;

	for (; pV != pEnd; ++pV)
	{
		uint32 bIn = (pV->m_vPos.z <= g_f1008b910);

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
				float t = ClipEdgeFar(&pPrev->m_vPos, &pCur->m_vPos, &(*ppOut)->m_vPos);

				LerpVert2ef34(pPrev, pCur, *ppOut, t);

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


// 0x1000f4b0 -- d3dr3c_os.cpp's driver.
static inline LTBOOL ClipPoly(uint32 nClipFlags, Vert2ef34** ppVerts, int32* pnVerts)
{
	Vert2ef34*     pOut;
	Vert2ef34*     pOutStart;
	Vert2ef34*     pVerts;
	int32      nVerts;
	uint32     anUnused[6];

	if (g_p1008d7a0)
	{
		nClipFlags &= 1;
		if (!nClipFlags)
			return LTTRUE;
	}

	pOut   = pOutStart = g_p1008b87c;
	pVerts = *ppVerts;
	nVerts = *pnVerts;

	if (nClipFlags & 0x01)
	{
		if (!ClipPlaneNear(&anUnused[0], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x04)
	{
		if (!ClipPlaneLeft(&anUnused[1], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x08)
	{
		if (!ClipPlaneTop(&anUnused[2], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x10)
	{
		if (!ClipPlaneRight(&anUnused[3], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x20)
	{
		if (!ClipPlaneBottom(&anUnused[4], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (!(nClipFlags & 0x02) ||
	    ClipPlaneFar(&anUnused[5], &pVerts, &nVerts, &pOut))
	{
		*ppVerts = pVerts;
		*pnVerts = nVerts;
		return LTTRUE;
	}

	return LTFALSE;
}



// ======================================================================= //
//  0x1002ef34 .. 0x10030a5a   (6,951 bytes)   1 caller, 0x10030ec6
//
//  The single argument arrives in %eax (see the banner); it is written as
//  an ordinary parameter, as a register-ABI argument is decoded.
// ======================================================================= //
void sub_1002ef30(Obj2ef34* pObj)
{
	// The declaration order is the frame order the shipped code uses,
	// lowest slot first: the thirty byte-lerp float temps at -0x16e8..
	// -0x1600 come out of the macro, aVerts sits at -0x15fc and the
	// pSrc/nCount/pVerts triple immediately above it at -0x5fc/-0x5f8/
	// -0x5f4, with the six flag arrays from -0x5f0 upwards.
	Vert2ef34		aVerts[VERT2EF34_MAX];

	SrcVert2ef34*	pSrc;                       // -0x5fc, address taken
	uint32			nVerts;                     // -0x5f8, address taken
	Vert2ef34*		pVerts;                     // -0x5f4

	Vert2ef34*		pWalk;
	uint32			nLeft;



	float			fX, fY, fZ;

	TexBlock2ef34*	pBlock;
	Tex2ef34*		pRec;
	Tex2ef34*		pCur;
	uint32			nStage;

	// ------------------------------------------------------------------ //
	//  0x1002ef34 -- `mov $0x16fc,%eax ; call 0x100805a0` is _chkstk, so
	//  the frame is 5,884 bytes.  Then the geometry fetch and the overflow
	//  guard; 128 is 0x1000a740's own `cmpl $0x80,arg ; jg` AND the
	//  array's frame footprint (0x15fc - 0x5fc = 0x1000 = 128 * 32).
	// ------------------------------------------------------------------ //
	// aVerts is named BEFORE the fetch (measured): that puts aVerts at
	// -0x15fc with pSrc / nVerts above it at -0x5fc / -0x5f8, as shipped.
	// Named after the call, the two scalars land below the array instead
	// (and declaration order changes nothing).
	pWalk = aVerts;

	sub_1000a700(pObj, (void**)&pSrc, &nVerts);

	if (!sub_1000a740(nVerts))
		goto L_Reject;			// 0x10030892, a two-byte trampoline

	// ------------------------------------------------------------------ //
	//  0x1002ef99 -- BUILD THE VERTICES.  The position is a STRUCT
	//  ASSIGNMENT through the source's +0x00 pointer (`rep movsl` of three
	//  dwords at 0x1002efa8).  The three colour bytes are widened in the
	//  integer unit, scaled and rounded through 0x10015190, highest
	//  destination byte first; the alpha byte comes straight out of
	//  g_1008be60 (d3dhigh.cpp records the same +0x13 <- 0x1008be60 store
	//  in 0x100598a0, the out-of-line version of this colour block).  The
	//  texture pair goes through the __thiscall setter.
	//
	//  pSrc is re-read from its frame slot after every call because its
	//  address went to 0x1000a700; the source therefore steps pSrc itself
	//  and does not cache it in a register-eligible local.
	// ------------------------------------------------------------------ //
	nLeft = nVerts;

	while (nLeft--)
	{
		pWalk->m_vPos = *pSrc->m_pPos00;

		pWalk->m_Diffuse10.m_b[2] =
			(uint8)sub_10015190((float)pSrc->m_nRed16   * g_f1008d730);
		pWalk->m_Diffuse10.m_b[1] =
			(uint8)sub_10015190((float)pSrc->m_nGreen15 * g_f1008d734);
		pWalk->m_Diffuse10.m_b[0] =
			(uint8)sub_10015190((float)pSrc->m_nBlue14  * g_f1008d738);
		pWalk->m_Diffuse10.m_b[3] = g_b1008be60;

		pWalk->sub_1000d9d0(pSrc->m_fU04, pSrc->m_fV08);

		pSrc++;
		pWalk++;
	}

	// -- 0x1002f069: the per-vertex dynamic lighting.  pVerts is set to
	//    aVerts here (0x1002f072) and is the second argument.
	pVerts = aVerts;

	sub_1002ec60(pObj, pVerts, nVerts);

	// ------------------------------------------------------------------ //
	//  0x1002f091 -- THE INLINE 3x4 TRANSFORM.  Twelve adjacent floats at
	//  0x100900bc..0x100900e8; row 2 is computed FIRST and stored FIRST,
	//  which is what the single `fxch %st(2)` at 0x1002f11c pins (see
	//  coin-flip 1).  All twelve loads of the position precede all three
	//  stores, so the results must go through locals.
	// ------------------------------------------------------------------ //
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		fZ = g_a100900bc[8]  * pWalk->m_vPos.x
		   + g_a100900bc[9]  * pWalk->m_vPos.y
		   + g_a100900bc[10] * pWalk->m_vPos.z
		   + g_a100900bc[11];

		fX = g_a100900bc[0]  * pWalk->m_vPos.x
		   + g_a100900bc[1]  * pWalk->m_vPos.y
		   + g_a100900bc[2]  * pWalk->m_vPos.z
		   + g_a100900bc[3];

		fY = g_a100900bc[4]  * pWalk->m_vPos.x
		   + g_a100900bc[5]  * pWalk->m_vPos.y
		   + g_a100900bc[6]  * pWalk->m_vPos.z
		   + g_a100900bc[7];

		pWalk->m_vPos.z = fZ;
		pWalk->m_vPos.x = fX;
		pWalk->m_vPos.y = fY;

		pWalk++;
	}

	// ------------------------------------------------------------------ //
	//  0x1002f12d -- THE CLIP MASK.  big5a960.cpp's UseD3DClip test with
	//  the two constants spelled out: all six planes normally, the near
	//  plane only when the D3D clipper is on.  A BYTE local -- `mov
	//  $0x3f,%al` / `xor %al,%al ; inc %al` / six `test $BIT,%dl`.
	// ------------------------------------------------------------------ //
	if (!ClipPoly(0x3f, &pVerts, (int32*)&nVerts))
		goto L_Reject;

	// ------------------------------------------------------------------ //
	//  0x100305e4 -- ShowSkySplits.  big21f90.cpp named 0x1008cb5c; when
	//  it is set the stage's texture is UNBOUND and the polygon is drawn
	//  untextured, which is what "show sky splits" means.
	//
	//  This block is UNBIND COPY A.  The `!pBlock` arm below jumps BACK
	//  into it (0x1003063c -> 0x100305ed) and the shipped `je` at
	//  0x100305eb jumps FORWARD over it into the record search, so the
	//  block really does sit here, physically ahead of the search.
	// ------------------------------------------------------------------ //
	if (g_CV_ShowSkySplits)
	{
L_UnbindA:
		nStage = g_n1008d7d0;

		if (g_a1008e684[nStage])
		{
			g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, nStage, LTNULL);
			g_a1008e684[nStage] = LTNULL;
		}

		goto L_Draw;			// 0x1003062c
	}

	// ------------------------------------------------------------------ //
	//  0x10030631 -- FIND (OR MAKE) THIS STAGE'S TEXTURE RECORD.
	//  big61a30.cpp's and big6d910.cpp's block field for field.
	// ------------------------------------------------------------------ //
	pBlock = pObj->m_p28->m_pBlock30;

	if (!pBlock)
		goto L_UnbindA;			// 0x1003063c

	nStage = g_n1008d7d0;

	// 0x10030646 -- the SECOND `je` off the ONE `test %esi,%esi` at
	// 0x1003063a.  icl 4.0 does not fold a redundant null test, so the
	// source tests pBlock twice; the second test's target is the middle of
	// UNBIND COPY B, one instruction past its own `nStage = g_n1008d7d0`,
	// which is what a compiler emits when it duplicates a block and finds
	// the value already live.
	if (!pBlock)
		goto L_UnbindB;

	pBlock->m_nStamp30 = g_w1008b870;

	pRec = LTNULL;

	// The list test is emitted TWICE -- 0x1003065c and 0x10030664 -- and
	// big61a30.cpp records the same pair.  The first of them goes straight
	// to L_MakeRec, icl having folded "the loop never ran, so pRec is
	// still null".
	if (pBlock->m_pHead0c)
	{
		pCur = pBlock->m_pHead0c;

		while (pCur)
		{
			// `cmp %ecx,%edx` at 0x10030670 puts the RECORD's field in the
			// destination, so it is the source's left operand.  The two
			// `break` trampolines are sunk at 0x10030a48 / 0x10030a4f.
			if (pCur->m_nStage42 == (uint8)nStage)
			{
				pRec = pCur;
				break;
			}

			pCur = pCur->m_pNext30;
		}
	}

	// 0x10030698.  A freshly-made record joins the bind at 0x100306ae
	// WITHOUT going through the comparison below (both create arms
	// `jmp 0x100306ae`), which is why the comparison sits inside the
	// found-it arm and not after the two of them.
	if (!pRec)
		goto L_MakeRec;

	if (pRec == g_a1008e684[nStage])
		goto L_ElseLOD;

L_Bind:
	// -- 0x100306ae ---------------------------------------------------- //
	g_a1008e684[pRec->m_nStage42] = pRec;

	if (pRec->m_nStamp14 != g_w1008bea8)
	{
		// dl_Remove / dl_TieOff / dl_Insert, ltlink.h:100..145 --
		// 0x100306ca..0x100306fa, prev at +0x00 and next at +0x04.
		pRec->m_Link1c.m_pPrev->m_pNext = pRec->m_Link1c.m_pNext;
		pRec->m_Link1c.m_pNext->m_pPrev = pRec->m_Link1c.m_pPrev;
		pRec->m_Link1c.m_pPrev = &pRec->m_Link1c;
		pRec->m_Link1c.m_pNext = &pRec->m_Link1c;

		pRec->m_Link1c.m_pPrev = g_Link1008e634.m_pPrev;
		pRec->m_Link1c.m_pNext = g_Link1008e634.m_pPrev->m_pNext;
		pRec->m_Link1c.m_pNext->m_pPrev = &pRec->m_Link1c;
		pRec->m_Link1c.m_pPrev->m_pNext = &pRec->m_Link1c;

		g_p1008cbcc->m_nBytes48 += pRec->m_nBytes10;

		pRec->m_nStamp14 = g_w1008bea8;
	}

	// 0x10030733 -- vtable +0x8c, three dwords.
	g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, (uint8)pRec->m_nStage42,
	                                 pRec->m_pSurface0c);

	if (pRec->m_nAlphaRef16)
	{
		// 0x1003075e -- vtable +0x50, state 0x18 = D3DRENDERSTATE_ALPHAREF.
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, 0x18,
		                                     pRec->m_nAlphaRef16);
	}

	// 0x10030765 -- the pair goes out BEFORE the counter, as in
	// big6d910.cpp's copy.  Both moves are INTEGER moves of floats.
	g_a1008e664[pRec->m_nStage42].m_fInvW00 = pRec->m_fInvW04;
	g_a1008e664[pRec->m_nStage42].m_fInvH04 = pRec->m_fInvH08;

	g_n1008bd80++;

	if (pRec->m_nDirty44)
	{
		// 0x10030798 -- vtable +0xbc.
		pRec->m_pSurface0c->lpVtbl->SetLOD(pRec->m_pSurface0c, 0);
		pRec->m_nDirty44 = 0;
	}

L_Draw:
	// ------------------------------------------------------------------ //
	//  0x100307a4 -- FOG, PROJECT AND SCALE, one loop.
	//
	//  The __fastcall through g_1008ce88 takes the vertex in %ecx and
	//  &vertex->m_Specular14 in %edx (`lea 0x14(%ebx),%edi` hoisted out of
	//  the loop at 0x100307b8 and `add $0x20,%edi` inside it).  0x10023570,
	//  the routine sub_1002b0e0 stores in that slot, reads 0x8(%ecx) and
	//  writes 0x3(%edx) -- the specular alpha, i.e. the fog byte.
	//
	//  The reciprocal is spelled `pWalk->m_fRHW` in all three projective
	//  divides because TWO of the three reload it from memory
	//  (`fmuls 0xc(%ebx)` at 0x10030816 and 0x10030828); the third uses
	//  the value still on the x87 stack.  1.0f is 0x100a6800, dumped.
	//
	//  0x1008e664 and 0x1008e668 are ELEMENT ZERO of the per-stage array
	//  0x10030770 wrote with an index -- the same fold big61a30.cpp and
	//  big6d910.cpp record.
	// ------------------------------------------------------------------ //
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		g_pfn_1008ce88(pWalk, &pWalk->m_Specular14);

		pWalk->m_fRHW = 1.0f / pWalk->m_vPos.z;

		pWalk->m_vPos.x = (pWalk->m_vPos.x * g_f1009027c
		                 + pWalk->m_vPos.z * g_f10090280) * pWalk->m_fRHW;
		pWalk->m_vPos.y = (pWalk->m_vPos.y * g_f10090284
		                 + pWalk->m_vPos.z * g_f10090288) * pWalk->m_fRHW;
		pWalk->m_vPos.z = (pWalk->m_vPos.z * g_f1009028c
		                 + g_f10090290) * pWalk->m_fRHW;

		pWalk->m_fU0 *= g_a1008e664[0].m_fInvW00;
		pWalk->m_fV0 *= g_a1008e664[0].m_fInvH04;

		pWalk++;
	}

	// -- 0x1003084f: vtable +0x64, six dwords.  6 = D3DPT_TRIANGLEFAN,
	//    0x1c4 = XYZRHW | DIFFUSE | SPECULAR | TEX1.
	g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, 6, 0x1c4, pVerts,
	                                    nVerts, 0);

L_Reject:
	// -- 0x1003088b: the only epilogue in the function.  It sets no %eax
	//    and the caller ignores the result, so this returns void.  Every
	//    reject in the six clip passes reaches it through the trampoline
	//    at 0x10030a56 and the 0x1000a740 failure through 0x10030892.
	return;


	// ================================================================== //
	//  THE SUNK TAIL, 0x1003097a..0x10030a5a, in shipped address order.
	//  These are cold arms of the tests above; icl put them after the
	//  DrawPrimitive and they are placed here by hand so the emitted call
	//  sequence matches.  See coin-flip 5.
	// ================================================================== //

L_ElseLOD:
	// -- 0x1003097a: the record is already bound to this stage, so only
	//    the LOD reset survives.  The same three statements as the tail of
	//    L_Bind; big61a30.cpp and big6d910.cpp record the same duplication.
	if (pRec->m_nDirty44)
	{
		pRec->m_pSurface0c->lpVtbl->SetLOD(pRec->m_pSurface0c, 0);
		pRec->m_nDirty44 = 0;
	}

	goto L_Draw;

L_MakeRec:
	// -- 0x100309b0: `test %edi,%edi ; je 0x10030a24` on the SAME list
	//    head that was walked above -- a third test of it.  The third
	//    argument is 1 when the list is non-empty and 0 when it is empty.
	//    UNBIND COPY B sits BETWEEN the two arms in the shipped bytes
	//    (0x100309d8, between 0x100309bc and 0x10030a2c), which is what
	//    puts the third *0x8c between the two 0x10039bd0 calls in the
	//    shipped sequence.
	if (pBlock->m_pHead0c)
	{
		pRec = sub_10039bd0(pBlock, nStage, 1);

		if (pRec)
		{
			pRec->m_pNext30              = pBlock->m_pHead0c->m_pNext30;
			pBlock->m_pHead0c->m_pNext30 = pRec;
			goto L_Bind;			// 0x100309d3
		}

L_UnbindB:
		// -- 0x100309d8: byte for byte UNBIND COPY A; see coin-flip 3.
		nStage = g_n1008d7d0;

		if (g_a1008e684[nStage])
		{
			g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, nStage, LTNULL);
			g_a1008e684[nStage] = LTNULL;
		}

		goto L_Draw;			// 0x10030a1f
	}

	// -- 0x10030a24: the empty-list arm.  Its failure re-reads
	//    g_n1008d7d0 and then jumps INTO copy B one instruction past copy
	//    B's own read (0x10030a46 -> 0x100309e0).
	pRec = sub_10039bd0(pBlock, nStage, 0);

	if (!pRec)
		goto L_UnbindB;

	goto L_Bind;				// 0x10030a38
}


// *********************************************************************** //
//
//  w25_10030a60_os.cpp -- 0x10030a60, 1228 bytes: draw the sky objects.
//  /Os: after the `ret` at 0x10030f29 comes `mov %esi,%esi` to the next
//  4 and then 0x90s to 16, and the body uses `xor %eax,%eax ; inc %eax`
//  for 1 and pushed arguments (the /O2 build was 1381 bytes, 1139 differ).
//
//  Called with no arguments by 0x1005f580 (w17_1005f580.cpp) once the sky
//  transform 0x1008ff60 is set up.  Z test and Z write off, the sky fog
//  range in, fog enable and texture stage 1's COLOROP saved, stage 1
//  disabled; then every sky object with its visible bit (+0x88 & 1):
//    * type 3 (model):   normal blend (0x10023d70(0)), 0x10061a30;
//    * type 7 (sprite):  normal blend if its alpha is not 0xff, else
//                        0x10023e50(0); then 0x1005a960;
//    * type 2 (world model): fog off if the object asks (+0x88 & 0x80);
//      the blend from its first polygon's surface flags (0x10023d70 with
//      the additive bit when bit 3 is set, else SRCALPHA/ONE and
//      0x10023e50(0)); its alpha into 0x1008be60 and its colour, /255,
//      into 0x1008d730..0x1008d738; then walk its BSP from the root with
//      an explicit 512-entry stack, drawing (0x1002ef34) every polygon
//      whose plane faces the sky view point, unless its surface has
//      flag 4; fog enable restored.
//  Then alpha blend off, Z test and Z write back, the normal fog range
//  and stage 1's COLOROP restored.
//
//  0x1002ef34 (big2ef34.cpp) is called at 0x10030ec6 the /Qip register
//  way: the polygon in %eax (`mov (%esi),%eax`) and an eight-byte
//  outgoing area it never writes.  Written here as the ordinary stack
//  call big2ef34.cpp defines (that file is plain /O2 and its prologue is
//  written for a stack argument), which is correct at run time; byte
//  identity would need both functions in one /O2 /Qip TU with 0x1002ef34
//  static.
//
//  RESULT: IDENTICAL -- int32 iSide assigned from the
//  compare and tested alone, and a one-site volatile reload of the
//  polygon for the draw (see the walk).  Earlier: near, 1222 vs 1226
//  bytes, 35 differ, all in the BSP walk's draw step.  Measured with a scratch /Os /Qip /Qip_no_inlining copy
//  holding a static 0x1002ef34: the register call then matches, but the
//  front/back test still lays out differently (ours sinks the draw arm,
//  shipped sinks `iSide = 0`); if/else in both orders, `<=`, a flag
//  assigned before the test, `iSide = (d > 0)`, a separate `if (iSide)`
//  all measured.  The draw and the plane distance are inline helpers:
//  without them icl keeps the surface pointer across the FP compare,
//  where shipped reloads it.
//
// *********************************************************************** //


#define D3DRENDERSTATE_ZENABLE           7
#define D3DRENDERSTATE_ZWRITEENABLE      14
#define D3DRENDERSTATE_SRCBLEND          19
#define D3DRENDERSTATE_DESTBLEND         20
#define D3DRENDERSTATE_ALPHABLENDENABLE  27
#define D3DRENDERSTATE_FOGENABLE         28
#define D3DRENDERSTATE_FOGSTART          36
#define D3DRENDERSTATE_FOGEND            37
#define D3DBLEND_ONE                     2
#define D3DBLEND_SRCALPHA                5
#define D3DTSS_COLOROP                   1
#define D3DTOP_DISABLE                   1

struct LTVec
{
	float x, y, z;
};

// The world-model BSP (w10_10073400_osoi.cpp's WNode73700 family).
struct WPlane30a60
{
	LTVec   m_vNormal;                                  // +0x00
	float   m_fDist;                                    // +0x0c
};

struct WSurface30a60
{
	uint8           m_pad_00[0x34];
	WPlane30a60*    m_pPlane34;                         // +0x34
	uint32          m_nFlags38;                         // +0x38
};

struct WPoly30a60
{
	uint8           m_pad_00[0x28];
	WSurface30a60*  m_pSurface28;                       // +0x28
};

struct WNode30a60
{
	WPoly30a60*     m_pPoly00;                          // +0x00
	WNode30a60*     m_pSides04[2];                      // +0x04 back, +0x08 front
	uint8           m_pad_0c[0x16 - 0x0c];
	uint8           m_Flags16;                          // +0x16
};

struct WBody30a60
{
	uint8           m_pad_00[0x9c];
	WNode30a60*     m_pRoot9c;                          // +0x9c
	WPoly30a60**    m_pPolies_a0;                       // +0xa0
	uint32          m_nPolies_a4;                       // +0xa4
};

// A sky object (big66f08.cpp's Object66f08 layout, with the type byte
// w10_10073400_osoi.cpp reads at +0xbc).  Its vtable +0x38 is called
// __thiscall with no arguments and tested for zero.
class SkyObj30a60
{
public:
	virtual void    Slot00() = 0;
	virtual void    Slot04() = 0;
	virtual void    Slot08() = 0;
	virtual void    Slot0c() = 0;
	virtual void    Slot10() = 0;
	virtual void    Slot14() = 0;
	virtual void    Slot18() = 0;
	virtual void    Slot1c() = 0;
	virtual void    Slot20() = 0;
	virtual void    Slot24() = 0;
	virtual void    Slot28() = 0;
	virtual void    Slot2c() = 0;
	virtual void    Slot30() = 0;
	virtual void    Slot34() = 0;
	virtual int32   Slot38() = 0;                       // vtbl +0x38

	uint8           m_pad_04[0x88 - 0x04];              // +0x04
	uint32          m_nFlags88;                         // +0x88
	uint8           m_pad_8c[0x94 - 0x8c];              // +0x8c
	uint8           m_nR94;                             // +0x94
	uint8           m_nG95;                             // +0x95
	uint8           m_nB96;                             // +0x96
	uint8           m_nA97;                             // +0x97
	uint8           m_pad_98[0xbc - 0x98];              // +0x98
	uint8           m_nType0bc;                         // +0x0bc
	uint8           m_pad_0bd[0x1a8 - 0xbd];
	WBody30a60*     m_pBody1a8;                         // +0x1a8
};

#define OBJTYPE_WORLDMODEL  2
#define OBJTYPE_MODEL       3
#define OBJTYPE_SPRITE      7

// w17_1005f580.cpp's scene (0x1008beb0): the sky objects are the array
// at +0x9c, the count at +0xa0.
struct Scene1005f580
{
	uint8           m_pad_00[0x9c];
	SkyObj30a60**   m_pSkyObjs_9c;                      // +0x9c
	int32           m_nSkyObjs_a0;                      // +0xa0
	uint8           m_pad_a4[0xbc - 0xa4];
	float           m_fFar_bc;                          // +0xbc
};

// The 0x4f8-byte transform (big66f08.cpp's XForm66f08): the view point is
// at +0x4b4 (0x1008b880 + 0x4b4 = 0x1008bd34, the main transform's eye
// point; here 0x1008ff60 + 0x4b4 = 0x10090414).
struct D3DXForm
{
	uint8           m_pad_000[0x4b4];
	LTVec           m_vViewPos4b4;                      // +0x4b4
	uint8           m_pad_4c0[0x4f8 - 0x4c0];
};

struct Ctx61a30;
class  Obj61a30;
struct CtxPG5a960;
class  ObjPG5a960;
struct Obj2ef34;

extern Scene1005f580*   g_1008beb0;                     // 0x1008beb0
extern D3DXForm         g_1008ff60;                     // 0x1008ff60 sky transform
extern float            g_1008cb00;                     // 0x1008cb00  SkyFogFarZ
extern float            g_1008cb04;                     // 0x1008cb04  SkyFogNearZ
extern float            g_1008cb08;                     // 0x1008cb08  FogNearZ
extern float            g_100893c8;                     // 0x100893c8  FogFarZ
extern uint32           g_nZEnable1008d984;             // 0x1008d984
extern uint8            g_1008be60;                     // 0x1008be60
extern float            g_f1008d730;                    // 0x1008d730
extern float            g_f1008d734;                    // 0x1008d734
extern float            g_f1008d738;                    // 0x1008d738

extern void sub_10023d70(uint32 bAdditive);             // 0x10023d70
extern void sub_10023e50(uint32 bZWriteEnable);         // 0x10023e50
extern void sub_10061a30(Ctx61a30* pCtx, Obj61a30* pObj);           // 0x10061a30
extern void sub_1005a960(CtxPG5a960* pCtx, ObjPG5a960* pObj);       // 0x1005a960
// sub_1002ef30 (register entry 0x1002ef34) is defined above.


static inline float PlaneDist(WPlane30a60* pPlane, LTVec* pPos)
{
	return pPlane->m_vNormal.x * pPos->x + pPlane->m_vNormal.y * pPos->y +
	       pPlane->m_vNormal.z * pPos->z - pPlane->m_fDist;
}

static inline void DrawSkyPoly(WPoly30a60* pPoly)
{
	if (!(pPoly->m_pSurface28->m_nFlags38 & 4))
		sub_1002ef30((Obj2ef34*)pPoly);
}

static inline uint32 GetSurfaceFlags(SkyObj30a60* pObj)
{
	if (pObj->m_pBody1a8 && pObj->Slot38())
	{
		if (pObj->m_pBody1a8->m_nPolies_a4 > 0)
			return pObj->m_pBody1a8->m_pPolies_a0[0]->m_pSurface28->m_nFlags38;
		return 0;
	}

	return 0;
}

void sub_10030a60()
{
	WNode30a60*     stack[512];
	WNode30a60**    pSP;
	WNode30a60*     pNode;
	SkyObj30a60*    pObj;
	uint32          nFogEnable, nColorOp, nFlags;
	int32           iSide;
	int32           i;

	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_ZWRITEENABLE, 0);
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_ZENABLE, 0);
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_FOGSTART, *(uint32*)&g_1008cb04);
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_FOGEND, *(uint32*)&g_1008cb00);
	g_pD3DDevice->lpVtbl->GetRenderState(g_pD3DDevice, D3DRENDERSTATE_FOGENABLE, &nFogEnable);
	g_pD3DDevice->lpVtbl->GetTextureStageState(g_pD3DDevice, 1, D3DTSS_COLOROP, &nColorOp);
	g_pD3DDevice->lpVtbl->SetTextureStageState(g_pD3DDevice, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	for (i = 0; i < g_1008beb0->m_nSkyObjs_a0; i++)
	{
		pObj = g_1008beb0->m_pSkyObjs_9c[i];

		if (!(pObj->m_nFlags88 & 1))
			continue;

		if (pObj->m_nType0bc == OBJTYPE_WORLDMODEL)
		{
			if (pObj->m_nFlags88 & 0x80)
				g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_FOGENABLE, 0);

			nFlags = GetSurfaceFlags(pObj);
			if (nFlags & 8)
			{
				sub_10023d70((nFlags & 0x80000) != 0);
			}
			else
			{
				g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
				g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
				sub_10023e50(0);
			}

			g_1008be60 = pObj->m_nA97;

			if (pObj->Slot38())
			{
				g_f1008d730 = (float)pObj->m_nR94 * (1.0f / 255.0f);
				g_f1008d734 = (float)pObj->m_nG95 * (1.0f / 255.0f);
				g_f1008d738 = (float)pObj->m_nB96 * (1.0f / 255.0f);

				if (pObj->m_pBody1a8->m_nPolies_a4 != 0)
				{
					pSP = stack;
					pNode = pObj->m_pBody1a8->m_pRoot9c;

					for (;;)
					{
						while (pNode->m_Flags16 & 3)
						{
							if (pSP == stack)
								goto Done;
							pSP--;
							pNode = *pSP;
						}

						// The side as an int32 flag tested
						// on its own puts the `iSide = 0` arm past the loop
						// as shipped, and the one-site volatile read of
						// m_pPoly00 is shipped's reload of it after the FP
						// compare (icl otherwise keeps it in %edx).
						iSide = PlaneDist(pNode->m_pPoly00->m_pSurface28->m_pPlane34,
						              &g_1008ff60.m_vViewPos4b4) > 0.0f;
						if (iSide)
							DrawSkyPoly(*(WPoly30a60* volatile*)&pNode->m_pPoly00);

						*pSP++ = pNode->m_pSides04[iSide];
						pNode = pNode->m_pSides04[!iSide];
					}
				}
			}

		Done:
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_FOGENABLE, nFogEnable);
		}
		else if (pObj->m_nType0bc == OBJTYPE_SPRITE)
		{
			if (pObj->m_nA97 != 0xff)
				sub_10023d70(0);
			else
				sub_10023e50(0);

			sub_1005a960((CtxPG5a960*)&g_1008ff60, (ObjPG5a960*)pObj);
		}
		else if (pObj->m_nType0bc == OBJTYPE_MODEL)
		{
			sub_10023d70(0);
			sub_10061a30((Ctx61a30*)&g_1008ff60, (Obj61a30*)pObj);
		}
	}

	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_ALPHABLENDENABLE, 0);
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_ZENABLE, g_nZEnable1008d984);
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_ZWRITEENABLE, 1);
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_FOGSTART, *(uint32*)&g_1008cb08);
	g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, D3DRENDERSTATE_FOGEND, *(uint32*)&g_100893c8);
	g_pD3DDevice->lpVtbl->SetTextureStageState(g_pD3DDevice, 1, D3DTSS_COLOROP, nColorOp);
}
