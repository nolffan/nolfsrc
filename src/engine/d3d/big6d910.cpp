// *********************************************************************** //
//
//  MODULE:   BIG_6D910 (.CPP)
//
//  PURPOSE:  One function -- 0x1006d910, 7,204 bytes.  Two callers,
//            0x10072a50 and 0x10072abb.
//
//  ------------------------------------------------------------------- //
//  THE EXTENT.  Verified before a byte was decoded, because three of the
//  last four functions dispatched had wrong extents.
//  ------------------------------------------------------------------- //
//
//  The stated range 0x1006d910..0x1006f534 is CORRECT, and the checks that
//  say so are:
//
//    * ONE `ret` in the whole range, at 0x1006f150, and ONE
//      `push %ebp ; mov %esp,%ebp`, at 0x1006d910.  There is no second
//      prologue and no second epilogue.
//    * EVERY branch target inside the range is inside the range.  A script
//      over the disassembly's 200-odd jump targets found nothing below
//      0x1006d910 and nothing above 0x1006f533.
//    * 0x1006f534..0x1006f53f is twelve `nop` bytes and 0x1006f540 is
//      `push %ebp ; mov %esp,%ebp ; sub $0xf10,%esp` -- the next function.
//
//  The one thing that LOOKS like a second function is not one: the single
//  `ret` sits at 0x1006f150 with 996 bytes of code after it.  That tail is
//  /O2 cold-block sinking -- 0x1006f146 (`jmp 0x1006f176`) jumps FORWARD
//  ACROSS the epilogue into it, every block in it uses this frame
//  (`0x8(%ebp)`, `-0x68(%ebp)`, `-0xa8(%ebp)`) and every one of them ends
//  by jumping BACK, most of them to the shared epilogue at 0x1006f148.
//  So the function's epilogue is in the middle of its own body and the
//  extent is right.
//
//  ------------------------------------------------------------------- //
//  WHAT IT IS
//  ------------------------------------------------------------------- //
//
//  d3d.ren's DRAW-ONE-POLYGON pass, the 40-byte/two-texture-stage sibling
//  of big124a4.cpp (0x100124a4, 32-byte/one-stage).  Both callers walk a
//  linked list of pending items, publish the item's clip mask into
//  0x1008b874 (`mov 0x10(%edx),%eax ; mov %eax,0x1008b874` at 0x10072a44)
//  and call this with the item and a context:
//
//      1006d910(pObj /*0x8(%ebp)*/, pCtx /*0xc(%ebp), never read*/)
//
//  It returns 1 when it issued a DrawPrimitive and 0 otherwise; the first
//  caller ORs the results together (`or %eax,%esi` at 0x10072a55).
//
//  Five independent tells for what it draws:
//    * the outgoing DrawPrimitive at 0x1006f358 is vtable +0x64 with
//      primitive type **6** (D3DPT_TRIANGLEFAN) and FVF **0x2c4** =
//      XYZRHW | DIFFUSE | SPECULAR | TEX2;
//    * every vertex stride in the function is 0x28 = 40 bytes, which is
//      exactly that FVF, and the six clip passes interpolate FOUR
//      texture floats (+0x18, +0x1c, +0x20, +0x24), which a one-stage
//      vertex does not have;
//    * the local vertex array runs -0x1a04(%ebp)..-0x605(%ebp) = 0x1400
//      bytes = **128 * 40**, and the first thing the function does is
//      `sub_1000a740(pObj->m_nCount50)`, whose body is
//      `cmpl $0x80,arg ; jg` over the string "Error: vertex buffer
//      overflow".  The array bound is READ OFF THE FRAME and confirmed by
//      the callee;
//    * the source vertices at pObj+0x54 are 0x18 = 24 bytes each and their
//      +0x00 is a POINTER that is dereferenced for three floats -- a
//      polygon's vertex-pointer list, not an array of positions;
//    * the whole of 0x1006f176..0x1006f2e7 is big61a30.cpp's texture-bind
//      block field for field (g_1008e684[stage], the LRU move-to-front of
//      the +0x1c LTLink through 0x1008e634, `g_1008cbcc->+0x48 +=`,
//      SetTexture at +0x8c, ALPHAREF at +0x50, the pair published into
//      g_1008e664[stage], SetLOD at +0xbc).
//
//  ------------------------------------------------------------------- //
//  COMPILER VERDICT: Intel C/C++ 4.0, **/O2**.  Not unanimous; see below.
//  ------------------------------------------------------------------- //
//
//  FOR /O2:
//    * COLD-BLOCK SINKING BELOW THE EPILOGUE, six-fold: the six clip
//      epsilon second halves at 0x1006f373 / 0x1006f399 / 0x1006f3c1 /
//      0x1006f3e9 / 0x1006f411 / 0x1006f436, the `pVerts == aVerts` arm at
//      0x1006f459, the `pRec == g_a1008e684[stage]` arm at 0x1006f479, the
//      two record-creation arms at 0x1006f4bd / 0x1006f4e9 and the
//      no-+0x2c arm at 0x1006f508.  That is what puts 996 bytes after the
//      `ret`.
//    * OUTGOING ARGUMENTS as `add $-N,%esp` then `mov %reg,K(%esp)` at
//      0x1006d957, 0x1006d9a7, 0x1006da1e, 0x1006f25a, 0x1006f287,
//      0x1006f2c8, 0x1006f31f and 0x1006f45c.
//    * `&p->m` as `mov $K,%reg ; add %p,%reg` -- 0x1006dc71, 0x1006dedb,
//      0x1006e00f, 0x1006f0b2 all do `mov $0x28,%eax ; add %edi,%eax` for
//      `pOut++`, never `lea 0x28(%edi),%eax`.
//
//  AGAINST (recorded, not explained away):
//    * ONE epilogue, cross-jumped to from 0x1006f364 (`return 1` jumps
//      into the middle of the `return 0` epilogue at 0x1006f14a) and from
//      the trampolines at 0x1006f369 / 0x1006f36e.  /O2 usually
//      DUPLICATES.
//    * `xor %eax,%eax ; inc %eax` for `return 1` at 0x1006f361.  Both
//      idioms occur in our own /O2 objects, so this is not decisive.
//    * eleven outgoing arguments ARE pushed (0x1006d924, 0x1006d932,
//      0x1006d9e4, 0x1006db63, 0x1006f136, 0x1006f159, 0x1006f167,
//      0x1006f4c1, 0x1006f522 ...).  Every one of those is a call whose
//      arguments are already in registers or immediates; the `add/mov`
//      form appears where a slot has to be computed.  Both forms in one
//      function is unexplained.
//
//  If the file is split, the whole of it wants ONE flag -- there is only
//  one function in it.  Try /O2 first, /Os second.
//
//  A NOTE THAT IS NOT A COMPILER FLAG: outgoing argument areas here are
//  always a MULTIPLE OF 8.  0x1000a740 takes one argument and gets two
//  pushes (0x1006d924/0x1006d932, `add $0x8,%esp`); 0x10024020 takes one
//  and gets two (0x1006f167/0x1006f16b); 0x10039bd0 takes three
//  (`0x4/0x8/0xc(%esp)` in its own body) and gets four
//  (0x1006f4c1..0x1006f4c8, `add $0x10,%esp`).  Those extra pushes are
//  icl's stack alignment, not arguments, and no source spelling produces
//  or suppresses them.
//
//  ------------------------------------------------------------------- //
//  THE ACCEPTANCE FIGURE AND WHAT IT DOES *NOT* WITNESS.  READ FIRST.
//  ------------------------------------------------------------------- //
//
//  The call count is 7,204 bytes, 20 calls (15 direct, 5 indirect),
//  **360 bytes per call**, across **12 distinct direct callees**.
//  Anything over 300 B/call is in the weak band, so the sequence
//  agreeing is necessary and nowhere near sufficient.  Here is the
//  call-free accounting and what pins each region instead.
//
//  * **ONE CALL-FREE STRETCH OF 5,587 BYTES, 0x1006db65..0x1006f138** --
//    the last call before it is 0x1000d9b0 at 0x1006db65 and the next is
//    0x1000d940 at 0x1006f138.  That is **77.6% of the function** and
//    the call-sequence comparison cannot see a byte of it.  It is the six-plane clipper.
//    What pins it instead, and none of it is guessed:
//
//      - it is SIX near-identical blocks whose heads are 0x1006dbc9,
//        0x1006df52, 0x1006e2f8, 0x1006e696, 0x1006ea34 and 0x1006edb6,
//        each opening with `incl 0x1008be44` and gated by mask bits
//        **0x01, 0x04, 0x08, 0x10, 0x20, 0x02 IN THAT ORDER**
//        (0x1006dbba, 0x1006df4a, 0x1006e2f0, 0x1006e68e, 0x1006ea2c,
//        0x1006edae), testing
//            z >= g_f1008b908 | -z < x | y < z | x < z | -z < y
//            | z <= g_f1008b910
//        read off six separate classify loops in this image
//        (0x1006dbe5 fcomps/jae, 0x1006df77 fchs+fcomps/jb, 0x1006e31d
//        fcomps/jb, 0x1006e6bb fcomps/jb, 0x1006ea59 fchs+fcomps/jb,
//        0x1006eddb fcomps/jbe);
//      - that is the SAME six planes in the SAME order with the same
//        -0.00001f / +0.00001f / 0.0f epsilon triple (0x100ab6e4 ..
//        0x100ab740, dumped and checked) and the same
//        `pIn = pOutStart ; nIn = (pOut-pOutStart)/40 ; pOut += nIn` tail
//        (0x1006df17, 0x1006e2bd, 0x1006e677, 0x1006ea15, 0x1006ed97,
//        0x1006f0eb -- all six visible as `cltd ; idiv $0x28`) that
//        big5a960.cpp, big61a30.cpp, big124a4.cpp and d3dr3a.cpp have each
//        decoded independently.  This is the FIFTH reading of one source;
//        it was diffed against big124a4.cpp's, expression by expression,
//        and the only differences are the reject target (`return 0` here,
//        a shared tail there) and two extra texture floats;
//      - the ARRAY BOUND is read off the frame, not guessed: the six flag
//        arrays are at -0x5f0, -0x514, -0x438, -0x35c, -0x280 and -0x1a4,
//        exactly **0xdc apart**, of which 0xc8 = 200 bytes = **50 int32**
//        is the array and 0x14 is the five int slots the five byte-lerps
//        need (pass 1's are -0x528, -0x524, -0x520, -0x51c, -0x518, which
//        is precisely -0x5f0 + 0xc8 onwards).  Identical to big5a960.cpp's
//        finding;
//      - the FIVE BYTE LERPS are +0x12, +0x11, +0x10, +0x13, +0x17 in that
//        order in ALL SIX passes -- thirty sites, every one of them read
//        (0x1006dd4f.., 0x1006e0f5.., 0x1006e493.., 0x1006e831..,
//        0x1006ebc2.., 0x1006ef35..).  That is 0x10023380's body extended
//        with the second texture pair, exactly as big5a960.cpp records.
//
//  * **THE INLINE PROJECTIVE TRANSFORM, 0x1006da68..0x1006db44 (220 bytes,
//    no calls).**  Pinned outright: it reads sixteen floats at
//    0x1008ba5c..0x1008ba98, and 0x1008ba5c = **0x1008b880 + 0x1dc**, so
//    the block is the +0x1dc..+0x218 field set of the transform object
//    0x1008b880 -- the SAME sixteen fields, in the same operand order,
//    that big124a4.cpp decoded at 0x10012a65 and that are the body of
//    0x1000a8a0 at 0x1000a8d0.  `fdivrs 0x100ab6e0` is `1.0f /` (the
//    constant was dumped: 0x3f800000).  Written here in big124a4.cpp's
//    exact spelling.
//
//  * **THE VERTEX-FILL LOOP, 0x1006d94e..0x1006d992 (69 bytes, one call).**
//    Witnessed only by the 0x1000d9d0 inside it; the field offsets are
//    read directly (source stride 0x18 at `add $0x18,%esi`, dest stride
//    0x28 at `add $0x28,%ebx`, +0x10 <- 0xffffffff, +0x20/+0x24 <- the
//    source's +0x0c/+0x10).
//
//  * **THE TEXTURE-SCALE LOOP, 0x1006f2f7..0x1006f31d (38 bytes, no
//    calls).**  Pinned by the two absolute addresses 0x1008e664 and
//    0x1008e668, which are element ZERO of the per-stage pair that
//    0x1006f2b3 writes with an index -- the same fold big61a30.cpp records
//    at 0x10061f29/0x10061f35.
//
//  * **NO `// UNDECODED:` GAPS.**  Every byte of 0x1006d910..0x1006f533 is
//    accounted for by a region below.
//
//  * **THE EXPECTED CALL-SEQUENCE RESULT, predicted from the disassembly
//    before any build.**  The shipped sequence, in address order, is
//
//      0x100805a0 (_chkstk), 0x1000a740, 0x1000d9d0, 0x1006d8e0,
//      0x10039d70, 0x1006d730, 0x1000d9b0, 0x1000d940, 0x1000efa0,
//      0x10024020, *0x8c, *0x50, *0xbc, *0x64, 0x1006d820, *0xbc,
//      0x10039bd0, 0x10039bd0, 0x1000efa0, 0x10024020
//
//    -- 20 calls, 15 direct and 5 indirect.  The source below emits
//    exactly that order, with ONE expected difference: icl generates its
//    own `__chkstk` for the 6,920-byte frame and the comparison cannot resolve
//    that symbol to 0x100805a0, which big124a4.cpp already recorded for
//    the same reason.  So the expected result is **19 of 20 agreeing,
//    with one by-position-only or one shipped-only row, and that row is
//    _chkstk**.  Anything else is a decode error or a block-placement
//    difference; if it is the latter it will show as equal counts on both
//    sides with the odd rows all being the four hand-placed cold blocks
//    named in coin-flip 4.
//
//  ------------------------------------------------------------------- //
//  REGION MAP
//  ------------------------------------------------------------------- //
//
//  0x1006d910..0x1006d943  PROLOGUE and the overflow guard.
//      `mov $0x1b08,%eax ; call 0x100805a0` is _chkstk, so the frame is
//      6,920 bytes; the four pushes that follow are icl's dead slot plus
//      %edi/%esi/%ebx (three `pop`s at 0x1006f14a..0x1006f14c against four
//      pushes, `mov %ebp,%esp` covering the difference).  Then
//      `sub_1000a740(pObj->m_nCount50)` and `return 0` through the
//      trampoline at 0x1006f36e.
//
//  0x1006d943..0x1006d994  THE FILL.  aVerts[i] built from
//      pObj->m_aSrc54[i]: three floats through the source's +0x00
//      POINTER, white diffuse, the +0x18/+0x1c pair through the thiscall
//      setter 0x1000d9d0 and the +0x20/+0x24 pair copied straight.
//
//  0x1006d994..0x1006d9d3  `sub_1006d8e0(&pVerts, &nVerts)`, which is
//      `sub_1000a8a0(&pVerts, &nVerts, g_1008b880, 0)` -- d3dr2d.cpp
//      already decoded that forwarder.  0 -> `return 0` via 0x1006f369.
//
//  0x1006d9d3..0x1006d9f7  THE DRAW DECISION.  `if (!pObj->m_p2c)` ->
//      0x1006f508; else `sub_10039d70(pObj, 0)` and 0 -> 0x1006f151.  The
//      two sunk blocks are byte-identical: `sub_1000efa0(pObj,
//      g_n1008d7cc)`, nonzero -> join the texture block, zero ->
//      `sub_10024020(pObj)` and `return 0`.  They are written out TWICE
//      below so the call sequence carries both copies; see the coin-flip
//      note.
//
//  0x1006d9f7..0x1006da3b  `pVerts != aVerts` -- big124a4.cpp's `bMoved`.
//      Moved: reset the pair and rebuild through 0x1006d730.  Not moved:
//      rebuild in place through 0x1006d820 (sunk at 0x1006f459) and jump
//      straight to the texture block.  0x1006d730 and 0x1006d820 are
//      109-byte twins that fill the SAME vertex and additionally compute
//      the +0x20/+0x24 pair by projecting the position against
//      pObj+0x34/+0x38/+0x3c; they differ only in two constants
//      (0x100ab5c4/0x100ab5cc vs 0x100ab5d4/0x100ab5dc) and in fsubrs vs
//      fsubs.  Both take arg0 in %eax, arg1 in %edx, arg2 in %ecx and arg3
//      on the stack (icl /Qip) -- decode that as an ordinary
//      call, which is what is done.
//
//  0x1006da3b..0x1006db49  THE UNCLIPPED PATH.  `g_n1008b874 == 0`: one
//      projective transform per vertex through 0x1008b880+0x1dc..+0x218,
//      the three results assembled in a named three-float local (the
//      `rep movsl $3` at 0x1006db2c copies it into the vertex) and 1/w
//      into +0x0c.  Straight to the texture block.
//
//  0x1006db49..0x1006db9a  THE CLIPPED PATH's pre-transform: nVerts calls
//      to 0x1000d9b0(pVert, g_1008b880), then g_1008b874 is RE-READ
//      (0x1006db73).  0x1000a8a0 re-reads it in exactly the same place
//      (0x1000a9e1) before handing it to its clipper, which is why the
//      second read is in the source and not a missed CSE.
//
//  0x1006db9a..0x1006f11c  THE SIX CLIP PASSES.  See the accounting above.
//      The `if (g_p1008d7a0) { nClip &= 1; if (!nClip) goto project; }`
//      at 0x1006db7e..0x1006db94 is big5a960.cpp's UseD3DClip test, and
//      its `goto` skips the `pVerts = pIn ; nVerts = nIn` epilogue at
//      0x1006f107 exactly as big124a4.cpp's early `goto L_Draw` does.
//
//  0x1006f11c..0x1006f148  THE PROJECTION: nVerts calls to
//      0x1000d940(pVert, g_1008b880).
//
//  0x1006f148..0x1006f151  THE EPILOGUE -- the only one.  Entered at
//      0x1006f148 for `return 0` and at 0x1006f14a for `return 1`.
//
//  0x1006f151..0x1006f176  L_NoDrawB, the 0x10039d70-said-no arm, sunk to
//      just past the epilogue and FALLING THROUGH into the texture block
//      (0x1006f165 is `jne 0x1006f176`, the very next address).
//
//  0x1006f176..0x1006f2e7  THE TEXTURE BIND.  big61a30.cpp's block.
//
//  0x1006f2e7..0x1006f369  THE SCALE-AND-DRAW.  tu0/tv0 scaled by
//      g_a1008e664[0], DrawPrimitive(TRIANGLEFAN, 0x2c4, pVerts, nVerts,
//      0), `incl 0x1008be5c`, `return 1`.
//
//  0x1006f369..0x1006f373  Two `return 0` trampolines, 0x1006f369 for the
//      0x1006d8e0 failure and 0x1006f36e for the 0x1000a740 failure.
//
//  0x1006f373..0x1006f459  THE SIX SUNK CLIP EPSILON ARMS, one per pass,
//      each `flds +1e-5 ; fcomp ; jb <back into the divide> ; fstp ; fstp
//      [; fstp] ; flds 0.0f ; fstpl <fT> ; jmp <back past the divide>`.
//      That is the `else fT = 0.0f;` half of `fDen < -eps || fDen > eps`.
//
//  0x1006f459..0x1006f479  L_Rebuild0: sub_1006d820, then the texture
//      block.
//
//  0x1006f479..0x1006f4af  L_ElseLOD: the record was already bound, so
//      only the SetLOD survives.
//
//  0x1006f4af..0x1006f4bd  The two `break` trampolines out of the record
//      search, `mov %eax,%edx ; jmp 0x1006f1d7`.
//
//  0x1006f4bd..0x1006f508  L_MakeRec: the two sub_10039bd0 arms.
//
//  0x1006f508..0x1006f534  L_NoDrawA, byte for byte L_NoDrawB.
//
//  ------------------------------------------------------------------- //
//  COIN-FLIPS, both spellings stated so one build settles each.
//  ------------------------------------------------------------------- //
//
//  1. THE DUPLICATED "no draw" TAIL.  0x1006f151..0x1006f176 and
//     0x1006f508..0x1006f534 are the same four instructions twice, and
//     they are the reason 0x1000efa0 and 0x10024020 each appear TWICE in
//     the shipped call sequence.  Written twice below, so the sequence
//     matches.  The alternative, which is very likely what the shipped
//     source said and which icl would then have tail-duplicated by itself:
//
//         bDraw = 0;
//         if (pObj->m_p2c)
//             bDraw = sub_10039d70(pObj, 0);
//         if (!bDraw)
//         {
//             if (!sub_1000efa0(pObj, g_n1008d7cc))
//             {
//                 sub_10024020(pObj);
//                 return 0;
//             }
//             goto L_Tex;
//         }
//
//     If a build shows icl emitting two copies from that, it is the better
//     spelling and the duplication below should be replaced by it.  If it
//     emits one, the duplication below is right and this is settled.
//
//  2. `float fT` vs `double fT`.  All six passes spill the parameter with
//     `fstpl` and reload it with `fmull` (0x1006dcd4/0x1006dcde and the
//     five twins).  That is an 8-byte slot, which a `double` would give
//     directly -- but so does an icl x87 register spill of a `float`
//     expression, and big5a960.cpp / big124a4.cpp both wrote `float` and
//     got the same bytes.  Written `float` here for consistency with the
//     four earlier decodes; `double fT;` is the one-token alternative.
//
//  3. THE TEXTURE-SCALE MULTIPLY ORDER.  0x1006f2ff loads tu0 then
//     `fmuls g_1008e664`, but 0x1006f308 loads g_1008e668 then
//     `fmuls tv0` -- the two are spelled with opposite operand order in
//     the bytes.  Written as `*=` for both below.  The alternative that
//     reproduces the asymmetry literally is
//         pV->m_fU0 = pV->m_fU0 * g_a1008e664[0].m_fInvW00;
//         pV->m_fV0 = g_a1008e664[0].m_fInvH04 * pV->m_fV0;
//     (icl emits the source's LEFT operand as the load).
//
//  4. THE HAND-PLACED COLD BLOCKS.  Four arms are written below as
//     `goto` targets laid out at the BOTTOM of the function in the SHIPPED
//     ADDRESS ORDER -- L_Rebuild0 (0x1006f459), L_ElseLOD (0x1006f479),
//     L_MakeRec (0x1006f4bd) and L_NoDrawA (0x1006f508) -- plus L_NoDrawB
//     (0x1006f151) immediately after the epilogue.  This is big5a960.cpp's
//     technique for its four destructor blocks, and it is here for one
//     reason: it makes the EMITTED call sequence match without depending
//     on icl re-deriving the same sinking decisions.  The structured
//     spelling of the same logic is
//
//         if (pVerts != aVerts) { pVerts = aVerts; nVerts = ...;
//                                 sub_1006d730(...); ...clip/transform... }
//         else                  { sub_1006d820(...); }
//         ...
//         if (!pRec) { ...sub_10039bd0... }
//         if (pRec != g_a1008e684[nStage]) { ...bind... }
//         else                             { ...LOD only... }
//
//     and if a build shows icl sinking those by itself into the shipped
//     order, that spelling is the better one and the labels below should
//     go.  The logic is identical either way; only the block placement
//     differs.
//
//  ------------------------------------------------------------------- //
//  WHAT IS *NOT* PINNED
//  ------------------------------------------------------------------- //
//
//  * THE SIX DEAD 0.0f CONSTANTS interleaved with the epsilons
//    (0x100ab718, 0x100ab720, 0x100ab728, 0x100ab730, 0x100ab738,
//    0x100ab740 are 0.0f and each is loaded by exactly one sunk arm) are
//    reproduced; the 1.0f slots big5a960.cpp and big61a30.cpp found dead
//    in their copies are not present in this one's constant run.
//  * THE STORE ORDER inside the inline projective loop (x, then rhw as a
//    double spill, then y, then z, software-pipelined) is icl's schedule,
//    not a source order.
//  * THE FRAME SLOT ORDER.  icl overlaps -0x70/-0x78/-0x80/-0x88 between
//    the pointer/counter roles and the six `fT` doubles; nothing in the
//    source controls that.
//  * pObj's SECOND ARGUMENT.  0xc(%ebp) is never read (grep over the whole
//    disassembly: zero hits).  It is declared because both call sites push
//    it.
//  * THE VERTEX'S SPECULAR DWORD (+0x14) IS NEVER WRITTEN by this
//    function -- not by the fill loop, not by 0x1006d730 and not by
//    0x1006d820 -- yet every one of the six clip passes INTERPOLATES its
//    top byte (+0x17).  Confirmed by grep: the only `0x14(%reg)` and
//    `0x17(%reg)` sites in the whole 7K are the texture record's +0x14 at
//    0x1006f205/0x1006f252 and the thirty clipper byte-lerps.  So the
//    specular the FVF advertises comes in with the buffer, and the
//    reconstruction leaves it alone too.  Recorded, not worked around.
//  * +0x0c (RHW) is likewise written only on the unclipped path (the
//    inline transform) and by 0x1000d940 on the clipped one; the fill
//    loop leaves it.
//
// *********************************************************************** //

#include "ltbasedefs.h"


// ---------------------------------------------------------------------- //
//  The D3D7 device.  Two vtable slots are reached: +0x50 SetRenderState
//  (0x1006f2a1) and +0x64 DrawPrimitive (0x1006f358), plus +0x8c
//  SetTexture (0x1006f276).  Same declaration big61a30.cpp uses.
// ---------------------------------------------------------------------- //
struct D3DDevice;

struct D3DDeviceVtbl
{
	uint8			m_pad_00[0x50];
	long (__stdcall* SetRenderState)(D3DDevice*, uint32, uint32);   // +0x50
	uint8			m_pad_54[0x64 - 0x54];
	long (__stdcall* DrawPrimitive)(D3DDevice*, uint32, uint32, void*,
	                                uint32, uint32);                // +0x64
	uint8			m_pad_68[0x8c - 0x68];
	long (__stdcall* SetTexture)(D3DDevice*, uint32, void*);        // +0x8c
};

struct D3DDevice
{
	D3DDeviceVtbl*	lpVtbl;
};


// IDirectDrawSurface7: only +0xbc, SetLOD (0x1006f2db and 0x1006f49b).
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
//  The 40-byte vertex.  FVF 0x2c4 = XYZRHW | DIFFUSE | SPECULAR | TEX2,
//  read straight off the DrawPrimitive at 0x1006f327.  Field for field
//  big5a960.cpp's Vert5a960; the two extra floats at +0x20 and +0x24 are
//  interpolated by every one of the six clip passes here, which no 32-byte
//  vertex would have.
// ---------------------------------------------------------------------- //
union VColor6d910
{
	uint32		m_n;
	uint8		m_b[4];
};

struct Vec6d910
{
	float	x;                                          // +0x00
	float	y;                                          // +0x04
	float	z;                                          // +0x08
};

struct Vert6d910
{
	// 0x1000d9d0 -- d3dbatchb.cpp's 21-byte two-field __thiscall setter
	// (`mov 0x8(%esp),%eax ; mov 0xc(%esp),%edx ; mov %eax,0x18(%ecx) ;
	//  mov %edx,0x1c(%ecx) ; ret $0x8`), called at 0x1006d97a with the
	// vertex in %ecx.
	void			sub_1000d9d0(float fU, float fV);

	// The same store pair INLINE, for 0x1006d730 (w9_1006d730_os.cpp's
	// measurement: inline, icl evaluates the two loads before either store).
	void			SetUV0(float fU, float fV) { m_fU0 = fU; m_fV0 = fV; }

	Vec6d910		m_vPos;                             // +0x00
	float			m_fRHW;                             // +0x0c
	VColor6d910		m_Diffuse10;                        // +0x10
	VColor6d910		m_Specular14;                       // +0x14
	float			m_fU0;                              // +0x18
	float			m_fV0;                              // +0x1c
	float			m_fU1;                              // +0x20
	float			m_fV1;                              // +0x24
};


// ---------------------------------------------------------------------- //
//  The SOURCE vertex, pObj+0x54.  Stride 0x18 from `add $0x18,%esi` at
//  0x1006d988 (and again at 0x1006d7c0 inside 0x1006d730).  +0x00 is a
//  POINTER: 0x1006d94e does `mov (%esi),%eax` and then reads (%eax),
//  0x4(%eax) and 0x8(%eax).  +0x04/+0x08 go to the vertex's +0x18/+0x1c
//  and +0x0c/+0x10 to its +0x20/+0x24.  +0x14 is never touched and exists
//  only because the stride says 24.
// ---------------------------------------------------------------------- //
struct SrcVert6d910
{
	Vec6d910*	m_pPos00;                               // +0x00
	float		m_fU004;                                // +0x04
	float		m_fV008;                                // +0x08
	float		m_fU10c;                                // +0x0c
	float		m_fV110;                                // +0x10
	uint32		m_n14;                                  // +0x14
};


// ---------------------------------------------------------------------- //
//  ltlink.h's CheapLTLink, as the texture record's +0x1c member.  The
//  dl_Remove / dl_TieOff / dl_Insert sequence at 0x1006f20d..0x1006f23d
//  reads +0x00 as prev and +0x04 as next.
// ---------------------------------------------------------------------- //
struct Link6d910
{
	Link6d910*	m_pPrev;                                // +0x00
	Link6d910*	m_pNext;                                // +0x04
	void*		m_pData;                                // +0x08
};


// ---------------------------------------------------------------------- //
//  The per-stage texture record, field for field big61a30.cpp's Tex61a30.
//  Its vtable is never called from here, so slot 0 is a plain pointer.
// ---------------------------------------------------------------------- //
struct Tex6d910
{
	void*		m_pVtbl00;                              // +0x00
	float		m_fInvW04;                              // +0x04
	float		m_fInvH08;                              // +0x08
	DDSurface*	m_pSurface0c;                           // +0x0c
	uint32		m_nBytes10;                             // +0x10
	uint16		m_nStamp14;                             // +0x14
	uint16		m_nAlphaRef16;                          // +0x16
	uint8		m_pad_18[0x1c - 0x18];
	Link6d910	m_Link1c;                               // +0x1c
	uint8		m_pad_28[0x30 - 0x28];
	Tex6d910*	m_pNext30;                              // +0x30
	uint8		m_pad_34[0x42 - 0x34];
	uint16		m_nStage42;                             // +0x42
	uint16		m_nDirty44;                             // +0x44
};


// The block reached through pObj->m_p28->m_p30: a list head at +0x0c and a
// sixteen-bit stamp at +0x30 (`mov %ax,0x30(%edi)` at 0x1006f19c).
struct TexBlock6d910
{
	uint8			m_pad_00[0x0c];
	Tex6d910*		m_pHead0c;                          // +0x0c
	uint8			m_pad_10[0x30 - 0x10];
	uint16			m_nStamp30;                         // +0x30
};


// The pair published per stage at 0x1008e664 -- `mov %ecx,(%edx,%eax,8)`
// and `mov %esi,0x4(%edx,%eax,8)` at 0x1006f2b3/0x1006f2b6.
struct TexScale6d910
{
	float		m_fInvW00;                              // +0x00
	float		m_fInvH04;                              // +0x04
};


// 0x1008cbcc: only +0x48 is touched here (`add %edx,0x48(%eax)`).
struct Stats6d910
{
	uint8		m_pad_00[0x48];
	uint32		m_nBytes48;                             // +0x48
};


// ---------------------------------------------------------------------- //
//  THE TRANSFORM OBJECT.  Only the projective block at +0x1dc..+0x218 is
//  read by name here -- 0x1008ba5c..0x1008ba98 is 0x1008b880 + 0x1dc..0x218
//  exactly.  Kept apart from the opaque `uint8[]` view every other user of
//  0x1008b880 in the project takes, and reached through a cast, exactly as
//  big124a4.cpp does.
// ---------------------------------------------------------------------- //
struct XForm6d910
{
	uint8	m_pad_000[0x1dc];                           // +0x000 .. +0x1db
	float	m_f1dc;                                     // +0x1dc
	float	m_f1e0;                                     // +0x1e0
	float	m_f1e4;                                     // +0x1e4
	float	m_f1e8;                                     // +0x1e8
	float	m_f1ec;                                     // +0x1ec
	float	m_f1f0;                                     // +0x1f0
	float	m_f1f4;                                     // +0x1f4
	float	m_f1f8;                                     // +0x1f8
	float	m_f1fc;                                     // +0x1fc
	float	m_f200;                                     // +0x200
	float	m_f204;                                     // +0x204
	float	m_f208;                                     // +0x208
	float	m_f20c;                                     // +0x20c
	float	m_f210;                                     // +0x210
	float	m_f214;                                     // +0x214
	float	m_f218;                                     // +0x218
};


// ---------------------------------------------------------------------- //
//  The item.  Only the seven fields the bytes read are named.  +0x34,
//  +0x38 and +0x3c are read by 0x1006d730 / 0x1006d820, not here, and are
//  present so the layout is honest about where +0x50 sits.
// ---------------------------------------------------------------------- //
struct TexHolder6d910
{
	uint8			m_pad_00[0x30];
	TexBlock6d910*	m_pBlock30;                         // +0x30
};

struct Obj6d910
{
	uint8			m_pad_00[0x28];
	TexHolder6d910*	m_p28;                              // +0x28
	void*			m_p2c;                              // +0x2c
	uint8			m_pad_30[0x34 - 0x30];
	float			m_f34;                              // +0x34
	float			m_f38;                              // +0x38
	float			m_f3c;                              // +0x3c
	uint8			m_pad_40[0x50 - 0x40];
	uint16			m_nCount50;                         // +0x50
	uint8			m_pad_52[0x54 - 0x52];
	SrcVert6d910	m_aSrc54[1];                        // +0x54
};


// ---------------------------------------------------------------------- //
//  Externals.  Relocations are masked, so only the shapes matter.
// ---------------------------------------------------------------------- //
extern D3DDevice*		g_pD3DDevice;                   // 0x1008d988

extern uint16			g_w1008b870;                    // 0x1008b870
extern uint32			g_n1008b874;                    // 0x1008b874
extern Vert6d910*		g_p1008b87c;                    // 0x1008b87c
extern uint8			g_1008b880[];                   // 0x1008b880
extern float			g_f1008b908;                    // 0x1008b908
extern float			g_f1008b910;                    // 0x1008b910
extern uint32			g_n1008bd80;                    // 0x1008bd80
extern uint32			g_n1008be44;                    // 0x1008be44
extern uint32			g_n1008be5c;                    // 0x1008be5c
extern uint16			g_w1008bea8;                    // 0x1008bea8
extern Stats6d910*		g_p1008cbcc;                    // 0x1008cbcc
extern void*			g_p1008d7a0;                    // 0x1008d7a0
extern uint32			g_n1008d7cc;                    // 0x1008d7cc
extern uint32			g_n1008d7d0;                    // 0x1008d7d0
extern Link6d910*		g_p1008e634;                    // 0x1008e634
extern TexScale6d910	g_a1008e664[];                  // 0x1008e664
extern Tex6d910*		g_a1008e684[];                  // 0x1008e684

// 0x1000a740 -- d3dbatchd.cpp.  `cmpl $0x80,arg ; jg` over "Error: vertex
// buffer overflow"; returns 1 when the count fits.
extern "C" int32 sub_1000a740(uint32 nVerts);

// 0x1000d940 and 0x1000d9b0 -- d3dlow.cpp's two transform helpers, both
// __cdecl (vertex, transform object).  0x1000d9b0 forwards to 0x1000a770
// with the 3x4 at +0x15c; 0x1000d940 is the screen projection through
// +0x31c..+0x330.
extern "C" void  sub_1000d940(Vert6d910* pVert, uint8* pXForm);
extern "C" void  sub_1000d9b0(Vert6d910* pVert, uint8* pXForm);

// 0x1000efa0 -- d3dbatchd.cpp, "binds a texture to a stage".
extern "C" int32 sub_1000efa0(Obj6d910* pObj, uint32 nStage);

// 0x10024020 -- __cdecl, one argument.  Pops the element free list at
// 0x1008d4fc and links the object onto 0x1008d4bc.
extern "C" void  sub_10024020(Obj6d910* pObj);

// 0x10039bd0 -- d3dmid.cpp's stack-ABI entry; the body is at 0x10039bdc.
extern "C" Tex6d910* sub_10039bd0(TexBlock6d910* pBlock, uint32 nStage,
                                  uint32 bLink);

// 0x10039d70 -- __cdecl, two arguments, arg0 at 0x8(%ebp) and arg1 at
// 0xc(%ebp); big124a4.cpp calls it the same way.
extern "C" int32 sub_10039d70(Obj6d910* pObj, int32 bDraw);

// 0x1006d8e0 -- d3dr2d.cpp, 43 bytes, THIS is its one caller.  Forwards to
// 0x1000a8a0 with g_1008b880 third and a literal zero fourth.
extern "C" int32 sub_1006d8e0(Vert6d910** ppVerts, int32* pnVerts);

// 0x1006d730 and 0x1006d820 -- the vertex builders, REGISTER-ONLY in the
// shipped image (arg0 %eax, arg1 %edx, arg2 %ecx, arg3 on the stack, no
// stack entry): /Qip statics of this TU.  Moved here from w9_1006d730_os.cpp
//, which had measured their bodies byte-identical at /Os /Qip
// /Qip_no_inlining with a throw-away caller; this file is built with that
// flag set (build/engine.sh D3D_QIP_OS_NI_TUS).
//
// The object at pObj+0x28, seen from here: an ordinary C++ virtual on slot 0
// that hands back the two basis vectors (`mov (%eax),%ebx ; mov %eax,%ecx ;
// mov (%ebx),%eax ; call *%eax`, two stack out-params) -- big0ae20.cpp's
// Basis0ae20.  The same +0x28 pointer the draw below reads as a
// TexHolder6d910.
class Basis6d730
{
public:
	virtual void Slot00(Vec6d910* pOut0, Vec6d910* pOut1) = 0;      // +0x00
};

// 0x1008beac's object: only the projection distance at +0xf8 is read.
struct S1001e520Root
{
	uint8			m_pad_000[0xf8];
	float			m_ff8;                              // +0xf8
};

extern S1001e520Root*	g_1008beac;                     // 0x1008beac
extern float			g_1008e66c;                     // 0x1008e66c  stage 1 1/w
extern float			g_1008e670;                     // 0x1008e670  stage 1 1/h


// ======================================================================= //
//  0x1006d730 -- 237 bytes.  Fill nCount destination vertices: position,
//  white diffuse, the first UV pair, and a second, PROJECTED UV pair (the
//  vertex relative to the object's origin dotted with the two basis
//  vectors, over the projection distance, re-centred by +0.5 and scaled by
//  stage 1's inverse texture size).  The loop counts a COPY of nCount
//  down, and the first UV pair goes through an inline setter (SetUV0),
//  whose two loads icl schedules before either store.
// ======================================================================= //
static void sub_1006d730(Obj6d910* pObj, Vert6d910* pDest,
                         SrcVert6d910* pSrc, int32 nCount)
{
	Vec6d910	vS;
	Vec6d910	vT;
	Vec6d910*	pPos;
	float		fX, fY, fZ;
	int32		i;

	((Basis6d730*)pObj->m_p28)->Slot00(&vS, &vT);

	for (i = nCount; i > 0; i--)
	{
		pDest->m_vPos.x = pSrc->m_pPos00->x;
		pDest->m_vPos.y = pSrc->m_pPos00->y;
		pDest->m_vPos.z = pSrc->m_pPos00->z;
		pDest->m_Diffuse10.m_n = 0xffffffff;
		pDest->SetUV0(pSrc->m_fU004, pSrc->m_fV008);

		pPos = pSrc->m_pPos00;
		fX = pPos->x - pObj->m_f34;
		fY = pPos->y - pObj->m_f38;
		fZ = pPos->z - pObj->m_f3c;

		pDest->m_fU1 = g_1008e66c
		             * ((fX * vS.x + fY * vS.y + fZ * vS.z)
		                / g_1008beac->m_ff8 + 0.5f);
		pDest->m_fV1 = g_1008e670
		             * ((fX * vT.x + fY * vT.y + fZ * vT.z)
		                / g_1008beac->m_ff8 + 0.5f);

		pSrc++;
		pDest++;
	}
}


// ======================================================================= //
//  0x1006d820 -- 192 bytes.  The same, writing only the projected pair.
// ======================================================================= //
static void sub_1006d820(Obj6d910* pObj, Vert6d910* pDest,
                         SrcVert6d910* pSrc, int32 nCount)
{
	Vec6d910	vS;
	Vec6d910	vT;
	Vec6d910*	pPos;
	float		fX, fY, fZ;
	int32		i;

	((Basis6d730*)pObj->m_p28)->Slot00(&vS, &vT);

	for (i = nCount; i > 0; i--)
	{
		pPos = pSrc->m_pPos00;
		fX = pPos->x - pObj->m_f34;
		fY = pPos->y - pObj->m_f38;
		fZ = pPos->z - pObj->m_f3c;

		pDest->m_fU1 = g_1008e66c
		             * ((fX * vS.x + fY * vS.y + fZ * vS.z)
		                / g_1008beac->m_ff8 + 0.5f);
		pDest->m_fV1 = g_1008e670
		             * ((fX * vT.x + fY * vT.y + fZ * vT.z)
		                / g_1008beac->m_ff8 + 0.5f);

		pSrc++;
		pDest++;
	}
}


// The frame says fifty: each pass's flag array is 0xc8 = 200 bytes and the
// six arrays are 0xdc apart (-0x5f0, -0x514, -0x438, -0x35c, -0x280,
// -0x1a4).
#define CLIP6D910_MAX	50


// ---------------------------------------------------------------------- //
//  0x10023380's body, INLINE and EXTENDED, as a macro so that no call can
//  be emitted for it.  d3dbatchd.cpp describes that helper as "two floats
//  at +0x18 and +0x1c interpolated normally, then five bytes (+0x12,
//  +0x11, +0x10, +0x13, +0x17)"; this vertex has TWO texture pairs, so the
//  six passes here carry FOUR floats (+0x18, +0x1c, +0x20, +0x24) and the
//  same five bytes in the same order.  There is no `call 0x10023380`
//  anywhere in the shipped 7K.
//
//  The byte differences are taken in the INTEGER unit (`movzbl ; movzbl ;
//  sub` at 0x1006dd4f) and only then converted; the result goes back
//  through a named float slot and a bare `fistpl` (`fstps -0x1af0(%ebp) ;
//  flds -0x1af0(%ebp) ; fistpl -0x528(%ebp)` at 0x1006dd79), which is
//  /QIfist -- there is no fnstcw/fldcw dance and no helper call at any of
//  the thirty sites.  Under plain /O2 a `(uint8)(int32)f` cast emits the
//  control-word dance instead and those sites will differ in bytes; they
//  contain no calls.
// ---------------------------------------------------------------------- //
// Round to nearest in the current FPU mode: w1_10001000.cpp's helper
// (0x10015190 out of line).  Inlined, it is the shipped `fstps t ;
// flds t ; fistpl n`.
// Measured: 7961 -> 7271 bytes (shipped 7204), 7128 ->
// 6225 differing, the first 23 bytes identical: it pairs.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}

#define CLIP6D910_LERP                                                       \
	pOut->m_fU0 = (pWalk->m_fU0 - pPrev->m_fU0) * fT + pPrev->m_fU0;         \
	pOut->m_fV0 = (pWalk->m_fV0 - pPrev->m_fV0) * fT + pPrev->m_fV0;         \
	pOut->m_fU1 = (pWalk->m_fU1 - pPrev->m_fU1) * fT + pPrev->m_fU1;         \
	pOut->m_fV1 = (pWalk->m_fV1 - pPrev->m_fV1) * fT + pPrev->m_fV1;         \
                                                                             \
	fRed   = (pWalk->m_Diffuse10.m_b[2] - pPrev->m_Diffuse10.m_b[2]) * fT    \
	       + pPrev->m_Diffuse10.m_b[2];                                      \
	pOut->m_Diffuse10.m_b[2] = (uint8)RoundF(fRed);                                  \
	fGreen = (pWalk->m_Diffuse10.m_b[1] - pPrev->m_Diffuse10.m_b[1]) * fT    \
	       + pPrev->m_Diffuse10.m_b[1];                                      \
	pOut->m_Diffuse10.m_b[1] = (uint8)RoundF(fGreen);                                \
	fBlue  = (pWalk->m_Diffuse10.m_b[0] - pPrev->m_Diffuse10.m_b[0]) * fT    \
	       + pPrev->m_Diffuse10.m_b[0];                                      \
	pOut->m_Diffuse10.m_b[0] = (uint8)RoundF(fBlue);                                 \
	fAlpha = (pWalk->m_Diffuse10.m_b[3] - pPrev->m_Diffuse10.m_b[3]) * fT    \
	       + pPrev->m_Diffuse10.m_b[3];                                      \
	pOut->m_Diffuse10.m_b[3] = (uint8)RoundF(fAlpha);                                \
	fSpec  = (pWalk->m_Specular14.m_b[3] - pPrev->m_Specular14.m_b[3]) * fT  \
	       + pPrev->m_Specular14.m_b[3];                                     \
	pOut->m_Specular14.m_b[3] = (uint8)RoundF(fSpec);


// ---------------------------------------------------------------------- //
//  The six clip passes.  Everything except the plane test, the
//  denominator, the numerator and which of x/y/z the plane pins is
//  identical; the macro is expanded once per plane, which is what the six
//  near-identical shipped blocks are.
//
//  The classify loop is emitted pointer-walking (`add $0x28,%edx ;
//  cmp %edi,%edx` at 0x1006dbf5/0x1006dc09) and the edge loop
//  index-walking on the flag array and pointer-walking on the vertices
//  (`mov -0x5f0(%ebp,%eax,4),%eax` at 0x1006dc5a, `add $0x28,%ebx` at
//  0x1006defa); both are written that way below, as big124a4.cpp does.
//
//  `if (nInside == 0)` rejects the whole polygon -- the shipped target is
//  0x1006f148, the `xor %eax,%eax` epilogue, not a "next primitive" label,
//  because there is only one primitive here.  The `cmp %esi,%edx ;
//  je 0x1006f148` guard at the TOP of each classify loop (0x1006dbdd) is
//  the same reject: icl rotated the loop and folded "loop never ran
//  therefore nInside is still zero".
// ---------------------------------------------------------------------- //
#define CLIP6D910_HEAD(BIT, INSIDE)                                          \
	if (nClip & (BIT))                                                       \
	{                                                                        \
		int32       abInside[CLIP6D910_MAX];                                 \
		float       fRed, fGreen, fBlue, fAlpha, fSpec;                      \
		Vert6d910  *pEnd, *pWalk, *pPrev;                                    \
		int32      *pFlag;                                                   \
		int32       nInside, nFlag, i, j;                                    \
		float       fDen, fT, fVal;                                          \
                                                                             \
		g_n1008be44++;                                                       \
                                                                             \
		nInside = 0;                                                         \
		pFlag   = abInside;                                                  \
		pEnd    = pIn + nIn;                                                 \
                                                                             \
		for (pWalk = pIn; pWalk != pEnd; pWalk++)                            \
		{                                                                    \
			nFlag    = (INSIDE) ? 1 : 0;                                     \
			nInside += nFlag;                                                \
			*pFlag   = nFlag;                                                \
			pFlag++;                                                         \
		}                                                                    \
                                                                             \
		if (nInside == 0)                                                    \
			goto L_Reject;                                                   \
                                                                             \
		if (nInside != nIn)                                                  \
		{                                                                    \
			pOutStart = pOut;                                                \
                                                                             \
			j     = nIn - 1;                                                 \
			pPrev = pIn + (nIn - 1);                                         \
                                                                             \
			for (i = 0; i < nIn; i++)                                        \
			{                                                                \
				pWalk = &pIn[i];                                             \
                                                                             \
				if (abInside[j])                                             \
				{                                                            \
					*pOut = *pPrev;                                          \
					pOut++;                                                  \
				}                                                            \
                                                                             \
				if (abInside[j] != abInside[i])                              \
				{

#define CLIP6D910_TAIL                                                       \
					CLIP6D910_LERP                                           \
					pOut++;                                                  \
				}                                                            \
                                                                             \
				pPrev = pWalk;                                               \
				j     = i;                                                   \
			}                                                                \
                                                                             \
			pIn  = pOutStart;                                                \
			nIn  = (int32)(pOut - pOutStart);                                \
			pOut = pOut + nIn;                                               \
		}                                                                    \
	}


// ======================================================================= //
//  0x1006d910 .. 0x1006f533   (7,204 bytes)   2 callers
// ======================================================================= //
int32 sub_1006d910(Obj6d910* pObj, void* pCtx)
{
	// The declaration order is the frame order the shipped code uses,
	// lowest slot first: aVerts at -0x1a04, the pVerts/nVerts pair
	// immediately above it at -0x604/-0x600, everything else higher.
	Vert6d910		aVerts[128];
	Vert6d910*		pVerts;
	int32			nVerts;

	Vert6d910*		pWalk;
	SrcVert6d910*	pSrc;
	uint32			nLeft;

	uint32			nFlags;
	uint32			nClip;

	Vert6d910*		pIn;
	Vert6d910*		pOut;
	Vert6d910*		pOutStart;
	int32			nIn;

	Vert6d910*		pV;
	Vec6d910		vScreen;
	float			fOOW;
	XForm6d910*		pM;

	TexBlock6d910*	pBlock;
	Tex6d910*		pRec;
	Tex6d910*		pCur;
	uint32			nStage;

	// -- 0x1006d913: `mov $0x1b08,%eax ; call 0x100805a0` is _chkstk, so
	//    the frame is 6,920 bytes.  0x1006d921: the guard.  128 is
	//    0x1000a740's own `cmpl $0x80,arg ; jg` AND the array's frame
	//    footprint (0x1a04 - 0x604 = 0x1400 = 128 * 40).
	if (!sub_1000a740(pObj->m_nCount50))
		goto L_Reject;			// 0x1006f36e, a trampoline into 0x1006f148

	// -- 0x1006d943: build the vertices.  Three floats through the source
	//    entry's +0x00 POINTER, white diffuse into +0x10, the first
	//    texture pair through the thiscall setter and the second pair
	//    copied straight.  Every float here moves as an INTEGER `mov`.
	pWalk = aVerts;
	pSrc  = pObj->m_aSrc54;
	nLeft = pObj->m_nCount50;

	while (nLeft--)
	{
		pWalk->m_vPos.x = pSrc->m_pPos00->x;
		pWalk->m_vPos.y = pSrc->m_pPos00->y;
		pWalk->m_vPos.z = pSrc->m_pPos00->z;

		pWalk->m_Diffuse10.m_n = 0xffffffff;

		pWalk->sub_1000d9d0(pSrc->m_fU004, pSrc->m_fV008);

		pWalk->m_fU1 = pSrc->m_fU10c;
		pWalk->m_fV1 = pSrc->m_fV110;

		pSrc++;
		pWalk++;
	}

	// -- 0x1006d994: d3dr2d.cpp's forwarder into 0x1000a8a0.  It may move
	//    the pair; `pVerts != aVerts` afterwards is big124a4.cpp's
	//    `bMoved`.
	pVerts = aVerts;
	nVerts = pObj->m_nCount50;

	if (!sub_1006d8e0(&pVerts, &nVerts))
		goto L_Reject;			// 0x1006f369, the second trampoline

	// ------------------------------------------------------------------ //
	//  0x1006d9d3 -- THE DRAW DECISION.  Both false arms are SUNK below
	//  the epilogue in the shipped bytes (0x1006f508 and 0x1006f151) and
	//  both are the same four instructions.  They are written as labelled
	//  blocks at the BOTTOM of this function, in the shipped address
	//  order, so that the emitted call sequence matches; see the note on
	//  hand-placed cold blocks in the banner.
	// ------------------------------------------------------------------ //
	if (!pObj->m_p2c)
		goto L_NoDrawA;

	// 0x1006d9e1.  The second argument is a literal 0 (`push $0x0` at
	// 0x1006d9e4), where big124a4.cpp's copy passes a live flag.
	if (!sub_10039d70(pObj, 0))
		goto L_NoDrawB;

	// -- 0x1006d9f7: big124a4.cpp's `bMoved`.  The EQUAL arm is the sunk
	//    one (`je 0x1006f459`), so it is the goto here.
	if (pVerts == aVerts)
		goto L_Rebuild0;

	// 0x1006da0b -- the array moved, so start again from the untransformed
	// source.  Both stores are in the shipped bytes (0x1006da14 and
	// 0x1006da21) before the call.
	pVerts = aVerts;
	nVerts = pObj->m_nCount50;

	sub_1006d730(pObj, aVerts, pObj->m_aSrc54, pObj->m_nCount50);

	// -- 0x1006da3b ----------------------------------------------------- //
	nFlags = g_n1008b874;

	if (!nFlags)
	{
		// -------------------------------------------------------------- //
		//  0x1006da4e -- NO CLIPPING WANTED.  One projective transform per
		//  vertex through 0x1008b880's +0x1dc..+0x218 block, `fdivrs 1.0f`
		//  for the reciprocal (0x1006da8c; 0x100ab6e0 was dumped and is
		//  0x3f800000), the three results assembled in a named three-float
		//  local -- the `rep movsl $3` at 0x1006db2c copies it into the
		//  vertex -- and the reciprocal into +0x0c.  The identical
		//  instruction sequence, operand for operand, is 0x1000a8d0 inside
		//  0x1000a8a0 and 0x10012a65 inside big124a4.cpp.
		// -------------------------------------------------------------- //
		pM = (XForm6d910*)g_1008b880;

		pWalk = pVerts;
		nLeft = nVerts;

		while (nLeft--)
		{
			fOOW = 1.0f / (pWalk->m_vPos.x * pM->m_f20c
			             + pWalk->m_vPos.y * pM->m_f210
			             + pWalk->m_vPos.z * pM->m_f214
			             + pM->m_f218);

			vScreen.x = (pWalk->m_vPos.x * pM->m_f1dc
			           + pWalk->m_vPos.y * pM->m_f1e0
			           + pWalk->m_vPos.z * pM->m_f1e4
			           + pM->m_f1e8) * fOOW;

			vScreen.y = (pWalk->m_vPos.x * pM->m_f1ec
			           + pWalk->m_vPos.y * pM->m_f1f0
			           + pWalk->m_vPos.z * pM->m_f1f4
			           + pM->m_f1f8) * fOOW;

			vScreen.z = (pWalk->m_vPos.x * pM->m_f1fc
			           + pWalk->m_vPos.y * pM->m_f200
			           + pWalk->m_vPos.z * pM->m_f204
			           + pM->m_f208) * fOOW;

			pWalk->m_vPos = vScreen;
			pWalk->m_fRHW = fOOW;
			pWalk++;
		}

		// 0x1006db44 -- straight to the texture block, no projection.
		goto L_Tex;
	}

	// -- 0x1006db49: the clipped path's pre-transform, then g_1008b874 is
	//    RE-READ at 0x1006db73.  0x1000a8a0 re-reads it in the same place
	//    (0x1000a9e1) before handing it to its own clipper, so the second
	//    read is source and not a missed CSE.
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		sub_1000d9b0(pWalk, g_1008b880);
		pWalk++;
	}

	nFlags = g_n1008b874;

	// -- 0x1006db7b: UseD3DClip.  When the D3D clipper is on, only the
	//    near plane is done here; if that bit is clear the whole clipper
	//    is skipped AND so is the `pVerts = pIn ; nVerts = nIn` epilogue
	//    at 0x1006f107 -- exactly 0x1000f4b0's early `return 1`.
	nClip = nFlags;

	if (g_p1008d7a0)
	{
		nClip &= 1;

		if (!nClip)
			goto L_Project;
	}

	// -- 0x1006db9a: all four assignments are in the shipped bytes
	//    (0x1006dba5, 0x1006dbab, 0x1006dbb7, 0x1006dbbd).
	pOut      = *(Vert6d910* volatile*)&g_p1008b87c;
	pOutStart = g_p1008b87c;
	pIn       = pVerts;
	nIn       = nVerts;

	// ------------------------------------------------------------------ //
	//  0x1006dbc9 -- bit 0x01, NEAR.  z >= g_f1008b908.
	//  fcomps 0x1008b908 / jae at 0x1006dbe8.  Epsilons 0x100ab6e4 (-1e-5)
	//  and 0x100ab73c / 0x100ab740 (+1e-5 / 0.0f) -- all three dumped.
	// ------------------------------------------------------------------ //
	CLIP6D910_HEAD(0x01, pWalk->m_vPos.z >= g_f1008b908)
		fDen = pWalk->m_vPos.z - pPrev->m_vPos.z;

		if (fDen < -0.00001f || fDen > 0.00001f)
			fT = (g_f1008b908 - pPrev->m_vPos.z) / fDen;
		else
			fT = 0.0f;

		pOut->m_vPos.x = (pWalk->m_vPos.x - pPrev->m_vPos.x) * fT
		               + pPrev->m_vPos.x;
		pOut->m_vPos.y = (pWalk->m_vPos.y - pPrev->m_vPos.y) * fT
		               + pPrev->m_vPos.y;
		// 0x1006dcfe -- stored as an INTEGER move, so it is a plain
		// float lvalue copy of the global.
		pOut->m_vPos.z = g_f1008b908;
	CLIP6D910_TAIL

	// ------------------------------------------------------------------ //
	//  0x1006df52 -- bit 0x04, LEFT.  -z < x.
	//  fchs / fcomps (%edx) / jb at 0x1006df7a.  Epsilons 0x100ab6ec and
	//  0x100ab734 / 0x100ab738.
	// ------------------------------------------------------------------ //
	CLIP6D910_HEAD(0x04, -pWalk->m_vPos.z < pWalk->m_vPos.x)
		fDen = (pWalk->m_vPos.x - pPrev->m_vPos.x)
		     + pWalk->m_vPos.z - pPrev->m_vPos.z;

		if (fDen < -0.00001f || fDen > 0.00001f)
			fT = -(pPrev->m_vPos.x + pPrev->m_vPos.z) / fDen;
		else
			fT = 0.0f;

		pOut->m_vPos.y = (pWalk->m_vPos.y - pPrev->m_vPos.y) * fT
		               + pPrev->m_vPos.y;
		fVal = (pWalk->m_vPos.z - pPrev->m_vPos.z) * fT + pPrev->m_vPos.z;
		// 0x1006e0a8 stores z first, then x -- fld/fchs/fxch keeps both.
		pOut->m_vPos.z = fVal;
		pOut->m_vPos.x = -fVal;
	CLIP6D910_TAIL

	// ------------------------------------------------------------------ //
	//  0x1006e2f8 -- bit 0x08, TOP.  y < z.
	//  flds 0x4(%edx) / fcomps 0x8(%edx) / jb at 0x1006e320.  Epsilons
	//  0x100ab6f4 and 0x100ab72c / 0x100ab730.
	// ------------------------------------------------------------------ //
	CLIP6D910_HEAD(0x08, pWalk->m_vPos.y < pWalk->m_vPos.z)
		fDen = (pWalk->m_vPos.y - pPrev->m_vPos.y)
		     - pWalk->m_vPos.z + pPrev->m_vPos.z;

		if (fDen < -0.00001f || fDen > 0.00001f)
			fT = -(pPrev->m_vPos.y - pPrev->m_vPos.z) / fDen;
		else
			fT = 0.0f;

		pOut->m_vPos.x = (pWalk->m_vPos.x - pPrev->m_vPos.x) * fT
		               + pPrev->m_vPos.x;
		fVal = (pWalk->m_vPos.z - pPrev->m_vPos.z) * fT + pPrev->m_vPos.z;
		// 0x1006e445: fsts 0x4(%ecx) then fstps 0x8(%ecx) -- y then z.
		pOut->m_vPos.y = fVal;
		pOut->m_vPos.z = fVal;
	CLIP6D910_TAIL

	// ------------------------------------------------------------------ //
	//  0x1006e696 -- bit 0x10, RIGHT.  x < z.
	//  flds (%edx) / fcomps 0x8(%edx) / jb at 0x1006e6bd.  Epsilons
	//  0x100ab6fc and 0x100ab724 / 0x100ab728.
	// ------------------------------------------------------------------ //
	CLIP6D910_HEAD(0x10, pWalk->m_vPos.x < pWalk->m_vPos.z)
		fDen = (pWalk->m_vPos.x - pPrev->m_vPos.x)
		     - pWalk->m_vPos.z + pPrev->m_vPos.z;

		if (fDen < -0.00001f || fDen > 0.00001f)
			fT = -(pPrev->m_vPos.x - pPrev->m_vPos.z) / fDen;
		else
			fT = 0.0f;

		pOut->m_vPos.y = (pWalk->m_vPos.y - pPrev->m_vPos.y) * fT
		               + pPrev->m_vPos.y;
		fVal = (pWalk->m_vPos.z - pPrev->m_vPos.z) * fT + pPrev->m_vPos.z;
		// 0x1006e7e4: fsts (%ecx) then fstps 0x8(%ecx) -- x then z.
		pOut->m_vPos.x = fVal;
		pOut->m_vPos.z = fVal;
	CLIP6D910_TAIL

	// ------------------------------------------------------------------ //
	//  0x1006ea34 -- bit 0x20, BOTTOM.  -z < y.
	//  fchs / fcomps 0x4(%edx) / jb at 0x1006ea5e.  Epsilons 0x100ab704
	//  and 0x100ab71c / 0x100ab720.
	// ------------------------------------------------------------------ //
	CLIP6D910_HEAD(0x20, -pWalk->m_vPos.z < pWalk->m_vPos.y)
		fDen = (pWalk->m_vPos.y - pPrev->m_vPos.y)
		     + pWalk->m_vPos.z - pPrev->m_vPos.z;

		if (fDen < -0.00001f || fDen > 0.00001f)
			fT = -(pPrev->m_vPos.y + pPrev->m_vPos.z) / fDen;
		else
			fT = 0.0f;

		pOut->m_vPos.x = (pWalk->m_vPos.x - pPrev->m_vPos.x) * fT
		               + pPrev->m_vPos.x;
		fVal = (pWalk->m_vPos.z - pPrev->m_vPos.z) * fT + pPrev->m_vPos.z;
		// 0x1006eb80 stores z first, then y.
		pOut->m_vPos.z = fVal;
		pOut->m_vPos.y = -fVal;
	CLIP6D910_TAIL

	// ------------------------------------------------------------------ //
	//  0x1006edb6 -- bit 0x02, FAR.  z <= g_f1008b910.  LAST, as in
	//  0x1000f4b0 and in all four earlier decodes.
	//  fcomps 0x1008b910 / jbe at 0x1006edde.  Epsilons 0x100ab70c and
	//  0x100ab714 / 0x100ab718.
	// ------------------------------------------------------------------ //
	CLIP6D910_HEAD(0x02, pWalk->m_vPos.z <= g_f1008b910)
		fDen = pWalk->m_vPos.z - pPrev->m_vPos.z;

		if (fDen < -0.00001f || fDen > 0.00001f)
			fT = (g_f1008b910 - pPrev->m_vPos.z) / fDen;
		else
			fT = 0.0f;

		pOut->m_vPos.x = (pWalk->m_vPos.x - pPrev->m_vPos.x) * fT
		               + pPrev->m_vPos.x;
		pOut->m_vPos.y = (pWalk->m_vPos.y - pPrev->m_vPos.y) * fT
		               + pPrev->m_vPos.y;
		// 0x1006eef0 -- integer move again.
		pOut->m_vPos.z = g_f1008b910;
	CLIP6D910_TAIL

	// -- 0x1006f107: the clipper's own epilogue, skipped by the
	//    UseD3DClip early exit above.
	pVerts = pIn;
	nVerts = nIn;

L_Project:
	// -- 0x1006f11c: camera space -> screen, one call per vertex.
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		sub_1000d940(pWalk, g_1008b880);
		pWalk++;
	}

	goto L_Tex;

L_Reject:
	// -- 0x1006f148: the only epilogue in the function.  `return 1` at
	//    0x1006f361 cross-jumps INTO it at 0x1006f14a.
	return 0;


	// ------------------------------------------------------------------ //
	//  0x1006f151 -- the 0x10039d70-said-no arm.  Sunk here, immediately
	//  after the epilogue, and it FALLS THROUGH into the texture block
	//  (0x1006f165 is `jne 0x1006f176`, the very next block).
	// ------------------------------------------------------------------ //
L_NoDrawB:
	if (!sub_1000efa0(pObj, g_n1008d7cc))
	{
		sub_10024020(pObj);
		goto L_Reject;			// 0x1006f174
	}

L_Tex:
	// ------------------------------------------------------------------ //
	//  0x1006f176 -- FIND (OR MAKE) THIS STAGE'S TEXTURE RECORD, then bind
	//  it.  big61a30.cpp's block field for field.
	// ------------------------------------------------------------------ //
	nStage = g_n1008d7d0;
	pBlock = pObj->m_p28->m_pBlock30;

	if (!pBlock)
		goto L_Draw;

	pBlock->m_nStamp30 = g_w1008b870;

	pRec = LTNULL;

	// The list test is emitted TWICE -- 0x1006f19a and 0x1006f1a8 -- and
	// icl 4.0 does not fold a redundant null test, so the source really
	// does test it twice.  big61a30.cpp records the same pair.
	if (pBlock->m_pHead0c)
	{
		pCur = pBlock->m_pHead0c;

		while (pCur)
		{
			// `cmp %ecx,%edx` at 0x1006f1b3 puts the RECORD's field in the
			// destination, so it is the source's left operand.  The two
			// `break` trampolines are sunk at 0x1006f4af / 0x1006f4b6.
			if (pCur->m_nStage42 == (uint8)g_n1008d7d0)
			{
				pRec = pCur;
				break;
			}

			pCur = pCur->m_pNext30;
		}
	}

	// 0x1006f1db.  A freshly-made record joins the bind at 0x1006f1f1
	// WITHOUT going through the comparison below (both create arms
	// `jmp 0x1006f1f1`), which is why the comparison sits inside the
	// found-it arm and not after the two of them.
	if (!pRec)
		goto L_MakeRec;

	if (pRec == g_a1008e684[nStage])
		goto L_ElseLOD;

L_Bind:
	// -- 0x1006f1f1 ---------------------------------------------------- //
	g_a1008e684[pRec->m_nStage42] = pRec;

	if (pRec->m_nStamp14 != g_w1008bea8)
	{
		// dl_Remove / dl_TieOff / dl_Insert, ltlink.h:100..145 --
		// 0x1006f20d..0x1006f23d, prev at +0x00 and next at +0x04.
		pRec->m_Link1c.m_pPrev->m_pNext = pRec->m_Link1c.m_pNext;
		pRec->m_Link1c.m_pNext->m_pPrev = pRec->m_Link1c.m_pPrev;
		pRec->m_Link1c.m_pPrev = &pRec->m_Link1c;
		pRec->m_Link1c.m_pNext = &pRec->m_Link1c;

		pRec->m_Link1c.m_pPrev = g_p1008e634;
		pRec->m_Link1c.m_pNext = g_p1008e634->m_pNext;
		pRec->m_Link1c.m_pNext->m_pPrev = &pRec->m_Link1c;
		pRec->m_Link1c.m_pPrev->m_pNext = &pRec->m_Link1c;

		g_p1008cbcc->m_nBytes48 += pRec->m_nBytes10;

		pRec->m_nStamp14 = g_w1008bea8;
	}

	// 0x1006f276 -- vtable +0x8c, three dwords.
	g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, (uint8)pRec->m_nStage42,
	                                 pRec->m_pSurface0c);

	if (pRec->m_nAlphaRef16)
	{
		// 0x1006f2a1 -- vtable +0x50, state 0x18 = ALPHAREF.
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, 0x18,
		                                     pRec->m_nAlphaRef16);
	}

	// 0x1006f2b3 -- the pair goes out BEFORE the counter here, which is
	// the other way round from big61a30.cpp's copy of the same block.
	g_a1008e664[pRec->m_nStage42].m_fInvW00 = pRec->m_fInvW04;
	g_a1008e664[pRec->m_nStage42].m_fInvH04 = pRec->m_fInvH08;

	g_n1008bd80++;

	if (pRec->m_nDirty44)
	{
		// 0x1006f2db -- vtable +0xbc.
		pRec->m_pSurface0c->lpVtbl->SetLOD(pRec->m_pSurface0c, 0);
		pRec->m_nDirty44 = 0;
	}

L_Draw:
	// ------------------------------------------------------------------ //
	//  0x1006f2e7 -- scale the FIRST texture pair by stage zero's
	//  published inverse size.  0x1008e664 and 0x1008e668 are ELEMENT ZERO
	//  of the per-stage array 0x1006f2b3 wrote with an index; the fold is
	//  the same one big61a30.cpp records at 0x10061f29/0x10061f35.
	// ------------------------------------------------------------------ //
	pV    = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		pV->m_fU0 *= g_a1008e664[0].m_fInvW00;
		pV->m_fV0 *= g_a1008e664[0].m_fInvH04;
		pV++;
	}

	// -- 0x1006f31f: vtable +0x64, six dwords.  6 = D3DPT_TRIANGLEFAN,
	//    0x2c4 = XYZRHW | DIFFUSE | SPECULAR | TEX2.
	g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, 6, 0x2c4, pVerts,
	                                    nVerts, 0);

	g_n1008be5c++;

	// -- 0x1006f361: `xor %eax,%eax ; inc %eax`, then a cross-jump INTO
	//    the shared epilogue at 0x1006f14a.
	return 1;


	// ================================================================== //
	//  THE SUNK TAIL, 0x1006f459..0x1006f533, in shipped address order.
	//  These four blocks are cold arms of the tests above; icl put them
	//  after the DrawPrimitive and they are placed here by hand so the
	//  emitted call sequence matches.  See the banner.
	// ================================================================== //

L_Rebuild0:
	// -- 0x1006f459: the array did NOT move, so rebuild in place through
	//    0x1006d730's twin, and go straight to the texture block: no
	//    clip, no transform.
	sub_1006d820(pObj, pVerts, pObj->m_aSrc54, nVerts);
	goto L_Tex;

L_ElseLOD:
	// -- 0x1006f479: the record is already bound to this stage, so only
	//    the LOD reset survives.  The same three statements as the tail of
	//    L_Bind; big61a30.cpp records the same duplication.
	if (pRec->m_nDirty44)
	{
		pRec->m_pSurface0c->lpVtbl->SetLOD(pRec->m_pSurface0c, 0);
		pRec->m_nDirty44 = 0;
	}

	goto L_Draw;

L_MakeRec:
	// -- 0x1006f4bd: `test %esi,%esi ; je 0x1006f4e9` on the SAME list
	//    head that was walked above -- a third test of it.  The third
	//    argument is 1 when the list is non-empty and 0 when it is empty.
	if (pBlock->m_pHead0c)
	{
		pRec = sub_10039bd0(pBlock, nStage, 1);

		if (!pRec)
			goto L_Draw;

		pRec->m_pNext30              = pBlock->m_pHead0c->m_pNext30;
		pBlock->m_pHead0c->m_pNext30 = pRec;
	}
	else
	{
		pRec = sub_10039bd0(pBlock, nStage, 0);

		if (!pRec)
			goto L_Draw;
	}

	goto L_Bind;

L_NoDrawA:
	// -- 0x1006f508: the no-+0x2c arm, byte for byte L_NoDrawB.  It is a
	//    SECOND copy in the shipped bytes, which is why 0x1000efa0 and
	//    0x10024020 each appear twice in the call sequence; see
	//    coin-flip 1.
	if (!sub_1000efa0(pObj, g_n1008d7cc))
	{
		sub_10024020(pObj);
		goto L_Reject;			// 0x1006f52f
	}

	goto L_Tex;
}
