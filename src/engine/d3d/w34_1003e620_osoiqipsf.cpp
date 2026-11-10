// *********************************************************************** //
//
//  w34_1003e620_osoiqipsf.cpp -- big3e620.cpp renamed
//  with 0x10040850 moved in from d3dr3c_os.cpp.
//
//  FLAGS: /Os /Oi- /Qip /Qsfalign16 (the `_osoiqipsf` convention in
//  build/engine.sh).  0x10040850 calls 0x1003e620 through its /Qip
//  register entry 0x1003e624 (`mov (%ebx),%eax ; call 0x1003e624`), so the
//  two share a /Qip TU.  That move, measured under /Os /Oi-
//  /Qip, found 0x10040850 given icl's 14-byte stack-realigning entry,
//  which the shipped one lacks; /Qsfalign16 ("align stack only for
//  functions with 16-byte variables") drops it.  Measured:
//    * 0x10040850 byte-identical (was 49 bytes off in d3dr3c_os.cpp);
//    * 0x1003e620, the 4-byte stack entry, byte-identical;
//    * 0x1003e624, the body, 7,942 bytes off (9,572 at /O2 /Oi- in
//      big3e620.cpp, unchanged since) -- the figure for /Os /Oi- /Qip.
//    * /Os /Qip /Qip_no_inlining /Qsfalign16 also closes both (body 7,943);
//      /O2 /Oi- /Qip /Qip_no_inlining /Qsfalign16 does not (62 off).
//
// *********************************************************************** //

// *********************************************************************** //
//
//  MODULE:   BIG_3E620 (.CPP)
//
//  PURPOSE:  ONE function -- 0x1003e620 .. 0x10040775, 8,533 bytes of body
//            (+ 3 bytes of alignment padding to 0x10040778).  d3d.ren's
//            DRAW-ONE-LIT-POLYGON pass for the 32-byte one-texture-pair
//            vertex: fetch the geometry, build 128 vertices, apply the
//            software dynamic lights, transform, run the six-plane
//            homogeneous clipper INLINE, project, bind the texture and
//            draw (once, or three times when the surface asks for it),
//            then hand the polygon on to the LMDynamic pass 0x1003dd50.
//
//            24 calls, 11 direct and 13 indirect, 356 bytes per call.
//            THE LARGEST FUNCTION LEFT UNDECODED IN THE MODULE.
//
//  GROUND TRUTH is the retail d3d.ren.
//
//  ------------------------------------------------------------------- //
//  0. WHAT IS FROM THIS FUNCTION'S BYTES AND WHAT IS REUSED.
//     Read this first (a sibling-assisted decode is only worth its cost
//     if the borrowing is explicit).
//  ------------------------------------------------------------------- //
//
//  CONFIRMED FROM 0x1003e620'S OWN BYTES, cited by address below:
//    * the extent, the one `ret`, the one prologue, all 109 branch targets;
//    * the frame (`_chkstk 0x176c`) -- every object DUMPED, including the
//      thirty 4-byte lerp temps and the six 0xdc clip blocks (section 5);
//    * the whole float pool 0x100a9b1c..0x100a9b9c, DUMPED from the image;
//    * the geometry fetch, the 128 bound, and the 24-byte source vertex;
//    * the fill loop's three ramp tables and its two indirect callees;
//    * the ENTIRE software-light block, instruction for instruction
//      against 0x1002ec60 (section 3a) -- including the `2*c - 255`
//      colour idiom, verified against the hardware (section 8);
//    * the inline projective transform and its sixteen matrix operands;
//    * all six clip masks, classify tests, denominators, numerators,
//      epsilon tests and result stores, by address;
//    * the projection loop, the texture-bind block field for field, the
//      three DrawPrimitive sites and their render states;
//    * the tail: `if (pObj->m_pLights2c) sub_1003dd50(...)`, the counter,
//      the single epilogue, and all twelve sunk cold blocks.
//
//  REUSED RATHER THAN RE-DERIVED (each named where it is used):
//    (R1) THE SHAPE OF THE SIX CLIP PASSES.  big3c40.cpp (0x10003c40) is
//         this clipper standing alone as a callable for the SAME 32-byte
//         one-texture-pair vertex; big6d910.cpp carries it inline in the
//         40-byte draw routine and supplies the C spelling of each plane.
//         What was verified HERE, from these bytes: the six mask bits, the
//         six classify tests and their operands, the vertex stride, the
//         field offsets, the five byte lerps and their order, the two
//         texture lerps, the flag-array bound (50), the block size (0xdc)
//         and the thirty staging temps.  A masked mnemonic diff of pass 3
//         (0x1003f010..0x1003f3a9, 215 instructions) against big3c40's
//         pass 3 (0x10004324..0x1000466d, 218) aligns 166 and EVERY
//         differing hunk is `fstpl <slot>`+`fmull <slot>` here against
//         `fmul %st(1),%st` there -- this copy spills fT to the frame
//         because the inline has more live values.  Nothing structural.
//    (R2) THE MEANING of the diffuse/specular byte names (+0x10..+0x13 a
//         BGRA union, +0x17 the specular alpha).  These bytes prove only
//         the OFFSETS and the ORDER; the naming is big3c40.cpp's and
//         d3dr3a.cpp's, corroborated here by FVF 0x1c4 at the two
//         DrawPrimitive sites (XYZRHW|DIFFUSE|SPECULAR|TEX1).
//    (R3) THE C SPELLING of the software light block.  big64cc0.cpp
//         decoded the same block at 0x10064fdd -- including the
//         `c - (255.0f - c)` idiom and the sunk `< 0` clamps -- and
//         d3dr3d.cpp summarised the out-of-line copy 0x1002ec60.  The
//         instructions were diffed HERE against 0x1002ec60 (section 3a)
//         and the two differences are recorded.
//    (R4) THE 24-BYTE SOURCE VERTEX and the TEXTURE-RECORD layout, which
//         are big2ef34.cpp's `SrcVert2ef34` and `Tex2ef34` field for
//         field.  Every offset was re-read off these bytes; only the
//         names are borrowed.
//
//  ------------------------------------------------------------------- //
//  1. THE EXTENT.  The row's figures are CORRECT; here is the check.
//  ------------------------------------------------------------------- //
//
//      Extent check of 0x1003e620, 8536 bytes:
//        0x1003e620..0x10040778  8,536 bytes  2057 instructions
//        ret               1        0x1004026c
//        push %ebp frames  1        0x1003e624
//        /Qip              start is the STACK entry; register entry at
//                                   0x1003e624
//        branch targets    0 outside the range or mid-instruction
//        last instruction  ends flush at the stated end, BUT the final 3
//                          bytes are alignment padding -- the body ends
//                          at 0x10040775
//
//  Re-run and confirmed independently here:
//
//    * THE ENUMERATION ROW IS THE REGISTER ENTRY, NOT THE FUNCTION.
//      The enumeration row says `0x1003e624, 8532`.  0x1003e620
//      is `8b 44 24 04  mov 0x4(%esp),%eax` -- a FOUR-BYTE /Qip stack
//      entry that lifts argument 0 into %eax and falls into the register
//      entry at 0x1003e624 (`push %ebp ; mov %esp,%ebp ; push %eax ;
//      mov $0x176c,%eax ; call 0x100805a0`).  big3dd50.cpp's banner
//      already records this pair from the other side (it is the four
//      bytes its own row walked over).  So the function is
//      0x1003e620..0x10040775 = 8,533 bytes of code, 8,536 to the row
//      boundary.
//    * ONE `ret`, at 0x1004026c, and ONE `push %ebp ; mov %esp,%ebp`, at
//      0x1003e624.  No `leave`, no second prologue, no second epilogue.
//    * THE 1,288 BYTES AFTER THE `ret` ARE /O2 COLD-BLOCK SINKING, not a
//      second function.  TWELVE blocks, every one of them using THIS
//      frame and branching BACK above the `ret`:
//        0x1004026d  the software light block (into 0x1003e74c);
//        0x1004058e  `jmp 0x10040266`, the 0x1000a740 failure trampoline;
//        0x10040593 / 0x100405b9 / 0x100405e1 / 0x10040609 / 0x10040631 /
//                    0x10040659  the SIX clip-epsilon arms, in forward
//                    pass order, each jumping back into its own pass
//                    (0x1003ea22/0x1003ea32, 0x1003edc2/0x1003edd0,
//                     0x1003f164/0x1003f172, 0x1003f4fa/0x1003f508,
//                     0x1003f898/0x1003f8a6, 0x1003fc31/0x1003fc3e);
//        0x1004067c  L_ElseLOD (into 0x10040017);
//        0x100406b2  L_MakeRec, its two arms and the shared
//                    sub_10025680 failure exit (into 0x1003ff21 /
//                    0x10040266);
//        0x1004070a / 0x10040717  the two record-search `break`
//                    trampolines, both `jmp 0x1003ff07`;
//        0x10040721  the light block's own loop tail (into 0x10040290 /
//                    0x1003e74c);
//        0x10040748 / 0x10040757 / 0x10040766  the three `< 0` colour
//                    clamps (into 0x10040565 / 0x10040554 / 0x10040576).
//      The last instruction of the body is 0x10040770 `jmp 0x10040576`,
//      ending at 0x10040775; 0x10040775..0x10040777 is `8d 76 00`
//      (`lea 0x0(%esi),%esi`) lea-nop padding and 0x10040778 is the next
//      function.
//    * EVERY branch target is inside the range and on an instruction
//      boundary.  A set-membership test of all 109 distinct targets
//      against the 2,057 decoded instruction addresses returned zero
//      misses; the lowest is 0x1003e68f and the highest 0x10040766.
//    * THE CALL ARITHMETIC LEAVES NO ROOM for a second function.
//      The call count over the whole range gives 24 calls; the
//      addresses are enumerated in section 6 and every one of them is
//      accounted for by a region below.
//
//  ------------------------------------------------------------------- //
//  2. THE SIGNATURE AND THE ONE CALLER.
//  ------------------------------------------------------------------- //
//
//  ONE argument, arriving in %eax at the register entry, and VOID.
//
//      1003e620  mov  0x4(%esp),%eax          ; the stack entry
//      1003e624  push %ebp ; mov %esp,%ebp
//      1003e627  push %eax                    ; -> -0x4(%ebp)
//      1003e628  mov  $0x176c,%eax
//      1003e62d  call 0x100805a0              ; _chkstk, frame 5,996
//      1003e632  mov  0x176c(%esp),%eax       ; the spilled argument back
//      1003e653  mov  %eax,-0xbc(%ebp)        ; pObj, for the whole body
//
//  THE ONLY CALLER in the image is 0x10040878, inside d3dr3c.cpp's
//  sub_10040850 (a scan of every resolvable `e8` in .text finds exactly
//  one call to 0x1003e624 and none to 0x1003e620):
//
//      1004086b  mov 0x10(%ebx),%eax
//      10040871  mov %eax,0x1008b874          ; publish the clip mask
//      10040876  mov (%ebx),%eax              ; the geometry
//      10040878  call 0x1003e624              ; nothing pushed
//
//  -- exactly the caller shape big70d10.cpp and big6d910.cpp record for
//  their own copies ("publishes the item's clip mask into 0x1008b874
//  first ... then recycles the item onto the free list at 0x1008d4fc").
//  d3dr3c.cpp:413 already declares it `extern void sub_1003e624(void* p);`
//  and calls it with the argument in %eax.  The epilogue sets no %eax
//  (0x10040266 `pop %ebx ; pop %esi ; pop %edi ; mov %ebp,%esp ;
//  pop %ebp ; ret`) and the caller ignores the result, so the return type
//  is void.  A register-ABI call is decoded as an ordinary
//  call whose argument arrives in %eax; it is declared __cdecl below.
//
//  The five `push`es at 0x1003e639..0x1003e63d are `%edi %edi %esi %ebx
//  %edi` against three `pop`s at 0x10040266..0x10040268 (`%ebx %esi
//  %edi`), with `mov %ebp,%esp` covering the difference -- three real
//  callee-saved registers plus icl's dead alignment dwords, the same
//  arrangement big2ef34.cpp records ("four `push`es ... against three
//  `pop`s ... icl's dead 4-byte slot").  One of them is eaten by the
//  `add $0x8,%esp` after the single-dword `push` at 0x1003e667.
//
//  ------------------------------------------------------------------- //
//  3. WHAT IT IS.  Six independent tells.
//  ------------------------------------------------------------------- //
//
//    * IT IS THE 32-BYTE / ONE-TEXTURE-PAIR MEMBER of the draw-one-polygon
//      family.  Every vertex stride is 0x20 (`shl $0x5` x19, `sar $0x5`
//      x6, `add $0x20,%reg` x11, `rep movsl` of EIGHT dwords x6), the
//      frame's vertex array is 0x1000 = 128 x 32 bytes, and both
//      DrawPrimitive sites pass FVF **0x1c4** = XYZRHW | DIFFUSE |
//      SPECULAR | TEX1 with primitive type **6** (D3DPT_TRIANGLEFAN)
//      -- 0x1004008e/0x10040099, 0x1004017b/0x10040186,
//      0x10040202/0x1004020d.  The six clip passes interpolate exactly
//      TWO texture floats (+0x18, +0x1c), which the 40-byte member does
//      not.
//    * THE GEOMETRY COMES FROM 0x1000a700 (0x1003e659), d3dbatchd.cpp's
//      "hands back where a piece of geometry's index data lives and how
//      many entries it has", and the count is immediately handed to
//      0x1000a740 (0x1003e668), whose body is `cmpl $0x80,arg ; jg` over
//      "Error: vertex buffer overflow".  So the 128 bound is enforced
//      twice: by the callee and by the frame.
//    * THE SOURCE VERTICES ARE 24 BYTES (`lea 0x18(%eax),%eax` at
//      0x1003e713) and their +0x00 is a POINTER dereferenced for three
//      floats -- big2ef34.cpp's `SrcVert2ef34`, a polygon's
//      vertex-pointer list.
//    * IT IS THE **LIT** MEMBER, not the white/sky one.  The fill takes
//      three adjacent source bytes at +0x16, +0x15, +0x14 through three
//      256-entry ramps at 0x1008cebc / 0x1008cfbc / 0x1008d0bc into the
//      destination's +0x12, +0x11, +0x10 and then the alpha byte from
//      0x1008be60 into +0x13 (0x1003e6a5..0x1003e6e5).  That is
//      0x100598a0's four-statement body with the OTHER ramp set (that
//      helper uses 0x1008d1bc / 0x1008d2bc / 0x1008d3bc; big70d10.cpp
//      records it inlined verbatim), and there is no `call 0x100598a0`
//      anywhere in the 8.5 K.
//    * IT CARRIES THE SOFTWARE DYNAMIC-LIGHT PASS INLINE.  The 878-byte
//      sunk block at 0x1004026d is 0x1002ec60's body -- d3dr3d.cpp:846
//      describes that function as "per-vertex dynamic lighting in DOUBLE
//      precision ... obj->d[0x2c] gives a light list; for each light it
//      copies three doubles out, forms the squared distance to the vertex
//      against the light radius at +0x1a8, and on a hit adds
//      (1 - dist/radius)-weighted colour into three ints taken from the
//      vertex's own +0x10/+0x11/+0x12 bytes, doubling them when
//      g_1008cb98 is set and clamping each at 255" -- and every one of
//      those landmarks is here (section 3a).
//    * THE TAIL HANDS THE POLYGON TO 0x1003dd50 (0x10040258), which
//      big3dd50.cpp decoded as the LMDynamic modulated second pass, and
//      says has exactly ONE caller: 0x10040258.  Its three register
//      arguments are `mov -0xbc(%ebp),%eax` (pObj), `mov -0x664(%ebp),%edx`
//      (pVerts) and `mov -0x668(%ebp),%ecx` (nVerts) -- the pointer and
//      the count this function has just drawn, exactly as big3dd50.cpp's
//      banner predicts.  The gate is `if (pObj->m_pLights2c)`
//      (0x1004023c `mov 0x2c(%eax),%eax ; test ; je`), which is the same
//      light list the software pass walks.
//
//  So: LMDynamic ON  -> skip the software lights, draw, and let
//  0x1003dd50 add one modulated pass per dynamic light;
//      LMDynamic OFF -> fold the lights into the vertex colours here.
//  The gate is `mov 0x10091200,%edx ; test %edx,%edx ; je 0x1004026d` at
//  0x1003e72c, and 0x10091200 is big3dd50.cpp's / d3dr5b.cpp's
//  **LMDynamic** console variable (`g_10091200.sub_10021f40("LMDynamic",
//  1.0f, 0, 0)`).  That the two arms are alternatives is the single
//  clearest statement of what this function is.
//
//  ------------------------------------------------------------------- //
//  3a. THE SOFTWARE LIGHT BLOCK IS 0x1002ec60, INSTRUCTION FOR
//      INSTRUCTION.  This is the strongest single correspondence here.
//  ------------------------------------------------------------------- //
//
//  0x1004026d..0x10040589 plus 0x10040721..0x10040746 plus the three
//  clamp arms 0x10040748..0x10040775 is 878 bytes; 0x1002ec60 is 724
//  bytes of body (0x1002ec60..0x1002ef24) doing the same work out of
//  line.  Landmark for landmark, in order, ours against its:
//
//      pObj->m_pLights2c, test, exit    1004027f / 1002ec6e
//      per-light loop head              10040290 / 1002ec7d
//      pLight->m_pObject at +0x04       1004029c / 1002ec85
//      `add $0x8` then rep movsl $3     1004029f / 1002ec88   (m_vPos)
//      movzbl +0x94 / +0x95 / +0x96     100402a9 / 1002ec99
//      g_1008be48++                     100402be / 1002ecae
//      pObj->m_p28->m_pPlane34          100402d7 / 1002ecc6
//      the plane dot product            100402da / 1002ecc9
//      `flds 0.0f` hoisted              1004030b / 1002ecf0
//      three fildl of the colour bytes  1004031b / 1002ecfe
//      `fsubs 0xc(%eax)` (- plane d)    10040339 / 1002ed14
//      fcomp / sahf / jbe / fchs (abs)  10040344 / 1002ed1d
//      three `flds 255 ; fsub ; DE E8`  10040348 / 1002ed21
//      fcomps +0x1a8 ; jae (radius)     10040383 / 1002ed56
//      1.0f/radius and radius*radius    1004039e / 1002ed72
//      movzwl pObj->+0x50 ; test        100403be / 1002ed91
//      the rotated double re-store      100403d7 / 1002eda6
//      the squared distance, z first    100403fd / 1002edc0
//      fldl radius2 ; fcomp ; jbe       10040427 / 1002ede4
//      call 0x1007fc98 (double sqrt)    1004044a / 1002edfe
//      fmull invRadius ; fsubrs 1.0f    1004044f / 1002ee03
//      three fistpll with the FULL
//        fnstcw/or 0xc00/fldcw dance    1004046b / 1002ee1b
//      g_1008cb98 (Saturate) doubling   100404b3 / 1002ee4f
//      three `cmp $0xff ; jle` clamps    10040544 / 1002eeb7
//      the three stores +0x12/11/10     10040576 / 1002eedd
//      `add $0x20` ; dec ; jne          1004057f / 1002eee6
//      pLight = pLight->m_pNext         10040721 / 1002eef0
//      three sunk `< 0` clamp arms      10040748 / 1002ef0d
//
//  TWO DIFFERENCES, both recorded rather than smoothed over:
//    (a) 0x1002ec60 is a called function with four `push`es and its own
//        `sub $0x9c,%esp`; here it is inlined into this frame, so every
//        `N(%esp)` becomes a `-0xNN(%ebp)` and the register seats rotate.
//    (b) 0x1002ec60 reads its vertex pointer and count from arguments
//        (0xb0/0xb4(%esp)); here the pointer comes from -0xc0(%ebp)
//        (which 0x10040279 and 0x1004073d set from -0x664, the vertex
//        array) and the count from `movzwl 0x50(%eax)` -- pObj's OWN
//        +0x50, NOT the -0x668 count 0x1000a700 handed back.  That is
//        read off 0x100403be and it is the same observation big64cc0.cpp
//        makes ("the count is +0x50, NOT the +0x52 the build loop may
//        have used.  Read off the bytes").
//
//  THE POOL PROVES THE SOURCE ORDER, AND IT IS NOT THE ADDRESS ORDER.
//  0x1002ec60's own literal run is 0x100a6740(255) 0x100a6744(255)
//  0x100a6748(255) 0x100a674c(0.0) 0x100a6750(1.0, dead)
//  0x100a6754(1.0) 0x100a6758(1.0).  THIS function's run OPENS with the
//  identical seven-slot pattern -- 0x100a9b1c(255) 0x100a9b20(255)
//  0x100a9b24(255) 0x100a9b28(0.0) 0x100a9b2c(1.0, DEAD)
//  0x100a9b30(1.0) 0x100a9b34(1.0) -- and only THEN comes the
//  transform's `fdivrs` 1.0f at 0x100a9b3c and the clipper's twelve
//  epsilon pairs.  icl emits pool entries in SOURCE order, so
//  **the light pass is written BEFORE the transform and the clipper and
//  was sunk to the bottom by /O2**, which no call, frame slot or branch
//  in this function shows.  It also means the whole block is one
//  `if (!g_nLMDynamic) { ... }` statement in source position, not a
//  hand-placed `goto` target.
//
//  ------------------------------------------------------------------- //
//  4. COMPILER VERDICT: Intel C/C++ 4.0, **/O2**.  Same split as
//     big2ef34.cpp and big6d910.cpp.
//  ------------------------------------------------------------------- //
//
//  FOR /O2:
//    * COLD-BLOCK SINKING BELOW THE EPILOGUE, TWELVE-fold -- the 1,289
//      bytes enumerated under THE EXTENT.  /Os cross-jumps instead.
//    * OUTGOING ARGUMENTS as `add $-N,%esp` then `mov %reg,K(%esp)` at
//      0x1003e6f3, 0x1003ff8a, 0x1003ffb7, 0x1003fff8, 0x10040086,
//      0x100400cb, 0x10040153, 0x10040173, 0x100401af, 0x100401d8,
//      0x100401fa, 0x1004043e and 0x10040688.
//    * `&p->m` as `mov $K,%reg ; add %p,%reg` -- 0x1003e9c9
//      (`mov $0x20,%eax ; add %edi,%eax`) and its five twins in the other
//      passes, plus `mov $0x1008e664,%edx` at 0x1003ffd8.
//  AGAINST (recorded, not explained away):
//    * SOME outgoing arguments ARE pushed: 0x1003e64a..0x1003e64c (the
//      three for 0x1000a700), 0x1003e667, 0x1003e887/0x1003e888,
//      0x1003fe8d/0x1003fe8e, and the four-push groups at
//      0x100406b6..0x100406bd / 0x100406ef..0x100406f6.  Exactly the
//      mixture big6d910.cpp and big2ef34.cpp both record, and for the
//      same reason: `add`/`mov` where a slot must be computed, `push`
//      where the value is already in a register or an immediate.  It is
//      unexplained there and unexplained here.
//    * ONE epilogue rather than several, with every early exit jumping to
//      it.  That is the /Os row; there is only one exit value (void), so
//      the signal is weak.
//  Outgoing areas are always a multiple of 8: 0x1000a740 takes one
//  argument and gets two slots, 0x10025680 takes one and gets two,
//  0x1000d9b0 / 0x10014580 take two and get two, 0x10039bd0 takes three
//  and gets four pushes, the double `sqrt` gets two.  icl's alignment,
//  not arguments.
//
//  A GENUINE NEW FINDING, and it FALSIFIES a claim in two existing
//  files.  big2ef34.cpp calls itself "a /QIfist TU" because all thirty of
//  its byte-lerp conversions are a bare `fstps ; flds ; fistpl` with no
//  control-word dance, and reasons: "d3dr3d.cpp records that 0x1002ec60,
//  the function this one CALLS, does the full control-word dance, so the
//  two are in DIFFERENT translation units and the flag really is
//  per-TU."  **THIS FUNCTION HAS BOTH, IN ONE FUNCTION.**  Its thirty
//  clip byte-lerps are bare (0x1003f1e7 `fstps -0x1708(%ebp)` /
//  0x1003f1ed `flds -0x1708(%ebp)` / 0x1003f1f3 `fistpl -0x3c8(%ebp)`,
//  and twenty-nine twins), while its three light conversions do the full
//  `fnstcw ; movzwl ; or $0xc00 ; fldcw ; fistpll ; fldcw`
//  (0x1004046b, 0x100404bc, 0x100404fe).  One function is one translation
//  unit and one flag, so the difference cannot be /QIfist: it tracks the
//  CAST -- a 32-bit `fistpl` off a value already rounded through a
//  4-byte float slot in the clipper, a 64-bit `fistpll` off a double in
//  the light pass.  Neither contains a call, so this costs nothing here,
//  but the per-TU inference should be struck from big2ef34.cpp and
//  d3dr3d.cpp.
//
//  ------------------------------------------------------------------- //
//  5. THE FRAME.  `_chkstk 0x176c` = 5,996 bytes, and it decomposes
//     EXACTLY.  Every row is DUMPED (a sort -u of every `-0xNNN(%ebp)`
//     operand in the 8,533 bytes), not assumed.
//  ------------------------------------------------------------------- //
//
//    -0x1770 .. -0x1759   24   NEVER ADDRESSED.  0x18 is exactly the
//                              largest outgoing area the body opens
//                              (`add $0xffffffe8,%esp`, the six dwords of
//                              DrawPrimitive) -- icl's reserve, the same
//                              row big3dd50.cpp records.
//    -0x1758 .. -0x166d  236   THIRTY 4-byte float temps stepping by 8
//                              from -0x1758 to -0x1670 (29 x 8 + 4 = 236
//                              exactly, and -0x166c is the next object).
//                              ALL THIRTY DUMPED, all distinct, and every
//                              one is touched EXACTLY TWICE and ONLY as a
//                              4-byte float: `fstps X ; flds X ;
//                              fistpl <int slot>`.  30 = 6 passes x 5
//                              byte lerps.  First -0x1758 (0x1003ea9b);
//                              pass 3 uses -0x1708..-0x16e8; last -0x1670
//                              (0x1003fdba).  This is big3c40.cpp's row
//                              "-0x700 .. -0x615  236  THIRTY 8-byte
//                              float temps ... 29 x 8 + 4 = 236 exactly".
//    -0x166c .. -0x066d 4096   aVerts[128], 128 x 32.  Base from
//                              `lea -0x166c(%ebp),%ebx` (0x1003e64d) and
//                              `lea -0x166c(%ebp),%eax` (0x1003e726); the
//                              top is fixed by -0x66c being the next
//                              object.  0x1000a740's `cmpl $0x80` agrees.
//    -0x66c                4   pSrc      (out-param of 0x1000a700)
//    -0x668                4   nVerts    (out-param of 0x1000a700)
//    -0x664                4   pVerts
//    -0x660 -0x65c -0x658 12   vLightPos, three floats.  ONLY 4-byte
//                              float and `rep movsl $3` accesses; used
//                              only by the light block.
//    -0x654 -0x650 -0x64c 12   vScreen, three floats -- the transform's
//                              named result, copied out with
//                              `rep movsl $3` at 0x1003e845.
//    -0x648 .. -0x121   1320   SIX x 0xdc, the six clip passes' state.
//                              The flag arrays are -0x648, -0x56c,
//                              -0x490, -0x3b4, -0x2d8 and -0x1fc,
//                              EXACTLY 0xdc apart, each indexed
//                              `-0xNNN(%ebp,%eax,4)` and
//                              `-0xNNN(%ebp,%edx,4)` (four references
//                              each, DUMPED).  0xdc = 0xc8 + 0x14 =
//                              **50 int32** of flags plus FIVE int slots,
//                              and all thirty int slots were dumped and
//                              land exactly at array+0xc8:
//                                -0x648 -> -0x580 -0x57c -0x578 -0x574 -0x570
//                                -0x56c -> -0x4a4 -0x4a0 -0x49c -0x498 -0x494
//                                -0x490 -> -0x3c8 -0x3c4 -0x3c0 -0x3bc -0x3b8
//                                -0x3b4 -> -0x2ec -0x2e8 -0x2e4 -0x2e0 -0x2dc
//                                -0x2d8 -> -0x210 -0x20c -0x208 -0x204 -0x200
//                                -0x1fc -> -0x134 -0x130 -0x12c -0x128 -0x124
//                              The bound FIFTY and the count FIVE are read
//                              off THIS frame.  0xdc is stride-independent
//                              (the flags are int32 either way), which is
//                              why it is byte-for-byte big3c40.cpp's and
//                              big6d910.cpp's block.
//    -0x120                8   the integer scratch.  132 references: 60
//                              writes + 60 reads for the byte lerps'
//                              `movzbl ; movzbl ; sub` staging, plus the
//                              light block's three `fistpll` (which need
//                              all eight bytes) and their three
//                              `add -0x120(%ebp),%reg`.
//    -0x11c -0x118 -0x114 12   icl's float-to-int control-word temps
//                              (saved cw, the %eax spill, the modified
//                              cw).  Compiler-generated; no declaration.
//    -0x108 -0x100 -0xf8
//      -0xf0               32  FOUR 8-byte doubles in the light block:
//                              1/radius and the three `2*c-255` colour
//                              terms (0x38/0x40/0x48 in 0x1002ec60).
//    -0xe8 -0xe0 -0xd8
//      -0xd0               32  radius*radius and the three double copies
//                              of vLightPos.
//    -0xc8 .. -0x78           the SHARED scalar pool.  Six of these slots
//                              are the six passes' `fT`, one per pass, and
//                              each is touched exactly ELEVEN times as an
//                              8-byte double -- two stores (the divide and
//                              the sunk `fT = 0.0f` arm) plus NINE
//                              `fmull`: pass1 -0xa8, pass2 -0xa0,
//                              pass3 -0x98, pass4 -0x90, pass5 -0x88,
//                              pass6 -0x80.  NINE = 2 position + 2
//                              texture + 5 byte lerps, and that is the
//                              load-bearing number in this frame.  The
//                              same slots host each pass's pointers and
//                              counters in the OTHER passes (block scopes
//                              are disjoint), plus -0x78 for the
//                              transform's `fstl`/`fldl` fOOW spill,
//                              -0xb0 for the light radius, -0xb8/-0xc0 for
//                              the light walk, -0xbc for pObj and
//                              -0xc8/-0xd8 for pOut and nClip.
//    -0x8                  4   a compiler temp (the pRec spill across the
//                              SetLOD call at 0x1004069b).
//    -0x4                  4   the pushed argument (dead after 0x1003e653).
//
//  THE ARITHMETIC, `push %eax` first so the allocation covers
//  -0x1770..-0x5 and the three callee-saved pushes sit BELOW it:
//
//      -0x1770..-0x1759     24   icl's outgoing reserve (unaddressed)
//      -0x1758..-0x166d    236   the thirty lerp temps
//      -0x166c..-0x066d   4096   aVerts[128]
//      -0x066c..-0x0649     36   pSrc + nVerts + pVerts + vLightPos(12)
//                                + vScreen(12)
//      -0x0648..-0x0121   1320   the six 0xdc clip blocks
//      -0x0120..-0x0111     16   the qword int scratch (8) + the two
//                                control-word temps (4 + 4); note that
//                                -0x11c ALIASES the qword's high half,
//                                which the shipped order proves --
//                                `fldcw -0x11c` is read at 0x1004048f
//                                BEFORE `fistpll -0x120` overwrites it
//                                at 0x10040495
//      -0x0110..-0x0109      8   UNADDRESSED
//      -0x0108..-0x00c1     72   NINE 8-byte doubles (-0x108, -0x100,
//                                -0xf8, -0xf0, -0xe8, -0xe0, -0xd8,
//                                -0xd0, -0xc8)
//      -0x00c0..-0x00b9      8   -0xc0 (pWalk base) + -0xbc (pObj)
//      -0x00b8..-0x00b1      8   -0xb8 (the light walk) + 4 unaddressed
//      -0x00b0..-0x0079     56   SEVEN 8-byte doubles (-0xb0 and the six
//                                clip `fT` at -0xa8..-0x80)
//      -0x0078..-0x0071      8   the transform's fOOW spill
//      -0x0070..-0x0009    104   UNADDRESSED
//      -0x0008..-0x0005      4   the pRec spill across the SetLOD call
//
//    24 + 236 + 4096 + 36 + 1320 + 16 + 8 + 72 + 8 + 8 + 56 + 8 + 104
//      + 4 = **5,996 = 0x176c EXACTLY**, with 112 bytes unaddressed in
//    two gaps (the 8 at -0x110 and the 104 at -0x70) -- icl's slack for
//    live ranges that ended up in registers.  Every other byte is
//    claimed by an object above, and the only inference left is which of
//    -0xc8..-0x78 belongs to which named local; the six `fT` are pinned
//    by the eleven-touch count.
//
//  ------------------------------------------------------------------- //
//  6. THE ACCEPTANCE FIGURE AND WHAT IT DOES *NOT* WITNESS.  READ FIRST.
//  ------------------------------------------------------------------- //
//
//  The call count says 8,536 bytes, **24 calls (11 direct, 13
//  indirect), 356 bytes per call**, across **TEN distinct direct
//  callees** -- counted mechanically, not by eye: 0x100805a0,
//  0x1000a700, 0x1000a740, 0x1000d9b0, 0x1000d9d0, 0x10014580,
//  0x1003dd50, 0x1007fc98, 0x10025680 and 0x10039bd0 (the last called
//  twice, which is why 11 direct rows give 10 distinct targets).
//  The ranking is on DISTINCT callees rather than call count;
//  ten across a real control-flow graph is respectable discrimination
//  even though the coverage is poor.
//  Anything over 300 B/call is in the weak band; at 356 the
//  sequence agreeing is a sanity check, not the evidence, and this banner
//  does not lean on it.
//
//  **CALL-FREE COVERAGE: the last call before the gap is 0x1000d9b0 at
//  0x1003e889 and the next is 0x10014580 at 0x1003fe8f.
//  0x1003e88e..0x1003fe8e is 5,633 bytes with ZERO calls in it -- 66.0%
//  of the function, and it is the whole six-plane clipper.  A second
//  stretch, 0x1003e70a..0x1003e886 (380 bytes), is the inline projective
//  transform.  Together 6,013 of 8,533 bytes -- 70.5% -- are invisible
//  to the call-sequence comparison.**  Quote that beside the figure, never instead of it.
//
//  The shipped sequence, in address order, predicted from the
//  disassembly before any build:
//
//     1  0x100805a0  _chkstk                        0x1003e62d
//     2  0x1000a700  geometry -> &pSrc, &nVerts      0x1003e659
//     3  0x1000a740  the 128 bound                   0x1003e668
//     4  *g_pfn1008ce8c  (INDIRECT)                  0x1003e6eb
//     5  0x1000d9d0  the u/v setter (__thiscall)     0x1003e705
//     6  0x1000d9b0  camera transform, per vertex    0x1003e889
//     7  0x10014580  screen projection, per vertex   0x1003fe8f
//     8  *0x8c  SetTexture                (INDIRECT) 0x1003ffa6
//     9  *0x50  SetRenderState 0x18       (INDIRECT) 0x1003ffd1
//    10  *0xbc  SetLOD                    (INDIRECT) 0x1004000b
//    11  *0x08  pRec's own vtable slot 2  (INDIRECT) 0x1004006f
//    12  *0x64  DrawPrimitive             (INDIRECT) 0x100400bf
//    13  *0x50  SetRenderState 0x1c := 0  (INDIRECT) 0x100400e8
//    14  *0x50  SetRenderState 0x1b := 1  (INDIRECT) 0x10040170
//    15  *0x64  DrawPrimitive             (INDIRECT) 0x100401ac
//    16  *0x50  SetRenderState 0x1b := 0  (INDIRECT) 0x100401cc
//    17  *0x50  SetRenderState 0x1c := 1  (INDIRECT) 0x100401f5
//    18  *0x64  DrawPrimitive (the else arm) (INDIR) 0x10040233
//    19  0x1003dd50  the LMDynamic pass              0x10040258
//    20  0x1007fc98  the CRT's double sqrt           0x1004044a
//    21  *0xbc  SetLOD (L_ElseLOD)        (INDIRECT) 0x1004069e
//    22  0x10039bd0  make a record (list non-empty)  0x100406be
//    23  0x10025680  the give-up exit                0x100406e2
//    24  0x10039bd0  make a record (list empty)      0x100406f7
//
//  The source below issues those twenty-four in exactly that order.
//
//  PREDICTED RESIDUALS, all four recognisable:
//    * `_chkstk` (1).  Our build emits its own probe for the 5,996-byte
//      frame and the call-sequence comparison cannot resolve that symbol to 0x100805a0, so
//      expect ONE shipped-only row at 0x1003e62d.  This is benign; do NOT
//      try /Ge.
//    * `sqrt` (20).  **THIS TU NEEDS `/O2 /Oi-`.**  icl holds `sqrt` as
//      an intrinsic and expands `fsqrt`; /Oi-
//      brings the call back, and build/engine.sh has a D3D_OI_TUS list
//      for exactly this.  Without the flag this is one shipped-only row
//      at 0x1007fc98.  Do NOT write it off, and do NOT use
//      `#pragma function(sqrt)` -- icl ignores it.
//    * THE /Qip TWO-ENTRY OFFSET on 0x1003dd50 (19).  big3dd50.cpp
//      records that function as tier Z with a register-only entry; the
//      shipped `call 0x1003dd50` is its only entry, so this one should
//      resolve cleanly, but if the pairing lands on a stack/register
//      pair the small-constant-offset row is that, not a wrong callee.
//    * BLOCK PLACEMENT on the indirect rows.  Twelve of the thirteen
//      indirect calls are inside hand-placed sunk blocks or inside the
//      `if (A && B)` whose arm order is coin-flip (1) below.  If the
//      result comes back with equal counts on both sides and all the odd
//      rows indirect, that is the benign pattern -- but only
//      AFTER the other spelling of coin-flip (1) has been tried.
//
//  ------------------------------------------------------------------- //
//  7. THE FLOAT POOL, DUMPED FROM THE RETAIL d3d.ren AT THESE ADDRESSES.
//     Read in order it is a transcript of the function's statements.
//  ------------------------------------------------------------------- //
//
//    100a9b1c  00 00 7f 43   255.0f      light red    (0x10040364)
//    100a9b20  00 00 7f 43   255.0f      light green  (0x10040352)
//    100a9b24  00 00 7f 43   255.0f      light blue   (0x10040348)
//    100a9b28  00 00 00 00     0.0f      the fabs test (0x1004030b)
//    100a9b2c  00 00 80 3f     1.0f      **DEAD** -- never loaded
//    100a9b30  00 00 80 3f     1.0f      1.0f / radius (0x1004039e)
//    100a9b34  00 00 80 3f     1.0f      1.0f - dist/radius (0x10040455)
//    100a9b38  00 00 80 3f     1.0f      **DEAD** -- never loaded
//    100a9b3c  00 00 80 3f     1.0f      1.0f / w, the transform
//                                        (0x1003e7a5)
//    100a9b40  ac c5 27 b7  -9.999999747e-06   pass 1 low epsilon
//    100a9b44  00 00 80 3f     1.0f      **DEAD**
//    100a9b48/4c  -1e-5 / 1.0f(dead)     pass 2
//    100a9b50/54  -1e-5 / 1.0f(dead)     pass 3
//    100a9b58/5c  -1e-5 / 1.0f(dead)     pass 4
//    100a9b60/64  -1e-5 / 1.0f(dead)     pass 5
//    100a9b68/6c  -1e-5 / 1.0f(dead)     pass 6
//    100a9b70  ac c5 27 37  +9.999999747e-06   pass 6 high epsilon
//    100a9b74  00 00 00 00     0.0f      pass 6 `fT = 0.0f`
//    100a9b78/7c  +1e-5 / 0.0f           pass 5   (0x10040631)
//    100a9b80/84  +1e-5 / 0.0f           pass 4   (0x10040609)
//    100a9b88/8c  +1e-5 / 0.0f           pass 3   (0x100405e1)
//    100a9b90/94  +1e-5 / 0.0f           pass 2   (0x100405b9)
//    100a9b98/9c  +1e-5 / 0.0f           pass 1   (0x10040593)
//    100a9ba0                            the next function's pool.
//
//  Three things read off that and nowhere else:
//    (a) THE LIGHT PASS IS WRITTEN FIRST (section 3a).  Its seven slots
//        open the run, ahead of the transform's 1.0f and the clipper.
//    (b) THE SIX `-1e-5` PAIRS ARE IN FORWARD PASS ORDER and the six
//        `+1e-5` pairs in REVERSE -- the low test is emitted in the hot
//        block and the high test in the sunk arm, and the sunk arms were
//        laid out bottom-up.  big3c40.cpp records the identical pattern.
//    (c) THE DEAD 1.0f AFTER EVERY EPSILON.  Seven unreferenced 1.0f in
//        the run (0x100a9b2c, 0x100a9b38, and one after each low
//        epsilon).  icl reserves a pool slot for a literal it then folds;
//        the `fT = num/den` statements each carry one.  Their presence is
//        what says the six epsilon tests are six separate `||` pairs and
//        not one shared helper.
//
//  ------------------------------------------------------------------- //
//  8. THE x87 REGISTER FORMS WERE VERIFIED AGAINST HARDWARE, not read
//     off objdump.  Binutils prints Intel's DE E0/E8/
//     F0/F8 mnemonics swapped, and this function turns on three of them.
//  ------------------------------------------------------------------- //
//
//  A 64-bit harness executed each opcode with ST(0)=3, ST(1)=10:
//
//      bytes   objdump prints        MEASURED semantics
//      de e1   fsubp  %st,%st(1)     -7   => ST(i) = ST(0) - ST(i)
//      de e9   fsubrp %st,%st(1)      7   => ST(i) = ST(i) - ST(0)
//      de f1   fdivp  %st,%st(1)     0.3  => ST(i) = ST(0) / ST(i)
//      de f9   fdivrp %st,%st(1)     3.33 => ST(i) = ST(i) / ST(0)
//      d8 e1   fsub   %st(1),%st     -7   => ST(0) = ST(0) - ST(1)
//      d8 e9   fsubr  %st(1),%st      7   => ST(0) = ST(1) - ST(0)
//
//  So objdump's `fsubp`/`fsubrp` and `fdivp`/`fdivrp` are both inverted
//  for the register forms.  Two readings here
//  depend on it:
//    * the six `de f1` at 0x1003ea2a and its five twins are FDIVRP:
//      `fT = ST(0)/ST(1)` = numerator/denominator, not the reciprocal;
//    * the three `de ea`/`de eb` in the light setup (0x10040350,
//      0x1004035a, 0x1004036c) are FSUBP, ST(i) = ST(i) - ST(0), so with
//      ST(0) = `255.0f - c` the value stored is `c - (255.0f - c)`.
//      A second harness run with a five-deep stack confirmed it:
//      C2=30, K=255 gives -195 = 2*30 - 255.  That is the same reading
//      big64cc0.cpp reached ("It is not nonsense -- 2*c-255 maps a byte
//      onto [-255,+255], which is what a light that can darken as well
//      as brighten needs"), and it is transcribed literally below.
//
//  ------------------------------------------------------------------- //
//  9. AN INSTRUCTION CENSUS WITH NO RESIDUE.  Every count is from
//     `sed 's/.*\t//' dis.txt | awk '{print $1}' | sort | uniq -c` over
//     the whole 8,533 bytes, and every row is EXACT.
//  ------------------------------------------------------------------- //
//
//    | mnemonic | shipped | what the structure below requires |
//    |----------|---------|-----------------------------------|
//    | fmull    |  56     | 6 passes x 9 uses of fT (2 position + 2 texture + 5 byte lerps) = 54, plus the light block's `* fInvRadius` and `* fBlue` (0x1004044f, 0x100404f2) |
//    | fildl    |  63     | 6 x 5 byte-lerps x 2 (the difference and the base) = 60, plus the light block's three colour bytes |
//    | fistpl   |  30     | 6 x 5 byte-lerps x 1.  EXACTLY, and nothing else in the function converts a float to an int32 |
//    | fistpll  |   3     | the light block's three `(int32)(double)` |
//    | fnstcw   |   3     | one per fistpll |
//    | fldcw    |   6     | two per fistpll (set and restore) |
//    | fdivp    |   6     | one divide per clip pass |
//    | fdivrs   |   1     | `1.0f / w` in the transform (0x100a9b3c dumped) |
//    | fdivl    |   1     | `1.0f / fRadius` in the light block |
//    | fcomps   |   7     | the six classify loops + the light block's `dist >= radius` |
//    | fcomp    |  14     | 6 x `fDen < -1e-5` + 6 x `fDen > 1e-5` in the sunk arms, + the light block's fabs test and its `radius2 > distSqr` |
//    | sar      |   6     | six x `nIn = (pOut - pOutStart) / 32`, `sar $0x5` |
//    | shl      |  19     | 6 x `pEnd = pIn + nIn` + 6 x `pPrev = pIn + (nIn-1)` + **FIVE** x `pOut = pOut + nIn`, all `shl $0x5`, plus the `shl $0x10`/`shl $0x8` of the constant colour.  FIVE, not six: every `sar $0x5` (6, at 0x1003ec5b, 0x1003effc, 0x1003f395, 0x1003f72e, 0x1003facf, 0x1003fe4a) is followed by a `shl $0x5 ; add` EXCEPT the last -- **pass 6's `pOut = pOut + nIn` is DEAD** and icl deleted it, because nothing reads pOut after the sixth pass.  That is exactly the finding recorded for big051a0 ("the 10 rather than 12 says pass 6's `pOut += nIn` is dead"), reached here from a different row, and it is the strongest single confirmation that the macro's tail is written the way it is |
//    | rep movsl|   8     | 6 x `*pOut = *pPrev` ($0x8 = 32 bytes) + `pWalk->m_vPos = vScreen` ($0x3) + `vLightPos = pLight->m_vPos` ($0x3) |
//    | incl     |   8     | 6 x `g_n1008be44++` (one per pass) + `g_n1008bd80++` + `g_n1008be5c++`.  `g_1008be48++` and the two 0x1008b874-adjacent counters are `mov/inc/mov`, not `incl` -- see below |
//    | movzbl   |  74     | 60 byte-lerp widenings + 3 fill ramp indices + 3 light colour bytes + 3 light vertex bytes + 3 constant-colour ramp reads + `(uint8)pRec->m_nStage42` + `(uint8)g_n1008d7d0` |
//    | movzwl   |  16     | pObj->+0x50 in the light block, g_w1008b870, 2 x the unrolled record search, 4 more m_nStage42, 2 x m_nStamp14, m_nAlphaRef16, m_nDirty44 x2, and the three control-word reads |
//    | fchs     |   9     | 2 classify negations (passes 2, 5) + 4 numerator negations (passes 2,3,4,5) + 2 result negations (passes 2, 5) + the light block's fabs |
//    | fsubrp   |   5     | passes 3 and 4's `pPrev->y - pPrev->z` / `pPrev->x - pPrev->z` (DE E8+i, Intel FSUBP) + the light block's three `c - (255-c)` |
//    | fsubrs   |   3     | `g_f1008b908 - pPrev->z`, `g_f1008b910 - pPrev->z`, and `1.0f - dist*invRadius` |
//    | fsub     |   9     | the six `pWalk->? - pPrev->?` that open each fDen + the light block's three `255.0f - c` |
//    | fsubl    |   3     | the light block's three `pWalk->pos - vLightPos` in double |
//    | fsts     |   2     | pass 3's `y = fVal` and pass 4's `x = fVal` |
//    | fld      |   2     | passes 2 and 5 duplicating fVal before negating it |
//    | fstl/fldl|  1 / 6  | the transform's fOOW spill/reload plus the light block's five double reloads |
//    | fmulp    |   1     | the transform's third `* fOOW` |
//    | idiv     |   0     | stride 32 divides with `sar`, never `idiv` -- the 40-byte members of this family have six `idiv $0x28` here |
//    | ret      |   1     | the single epilogue |
//
//  56 = 6 x 9 + 2, 63 = 6 x 5 x 2 + 3, 30 = 6 x 5 and the six `fdivp`
//  are the load-bearing rows: **six clip passes, each with two position
//  LERPs, TWO texture LERPs and five byte LERPs, and nothing else.**  A
//  seventh plane, a missing plane, a second texture pair (which would
//  make fmull 6 x 11 = 66, as big70d10.cpp's does) or a sixth byte would
//  move at least three of those rows, and none of them moves.  The
//  `fistpl` 30 against `fistpll` 3 is the /QIfist finding in section 4.
//
//  ------------------------------------------------------------------- //
//  10. REGION MAP.  Every byte of 0x1003e620..0x10040775 is here.
//  ------------------------------------------------------------------- //
//
//   0x1003e620..0x1003e677  PROLOGUE, the geometry fetch and the 128
//                           bound.  The failure `return;` goes through
//                           the two-byte trampoline at 0x1004058e.
//   0x1003e678..0x1003e725  THE FILL.  aVerts[i] from pSrc[i]: the
//                           position through the source's +0x00 POINTER
//                           as three dword moves, the three ramped
//                           colour bytes, the alpha byte from
//                           0x1008be60, the __fastcall through
//                           g_pfn1008ce8c that writes the specular
//                           dword, and the texture pair through the
//                           __thiscall setter 0x1000d9d0.  pSrc is
//                           RE-READ from its frame slot after every
//                           store because its address was handed to
//                           0x1000a700 (0x1003e689, 0x1003e6a5,
//                           0x1003e6b8, 0x1003e6cb, 0x1003e6ed,
//                           0x1003e70a).
//   0x1003e726..0x1003e73f  `if (!g_nLMDynamic)` -> the software light
//                           block, SUNK to 0x1004026d.
//   0x1003e740..0x1003e758  `pWalk = pVerts` (both arms).
//   0x1003e759..0x1003e861  THE UNCLIPPED PATH.  `g_n1008b874 == 0`:
//                           one projective transform per vertex through
//                           the 4x4 at 0x1008ba5c..0x1008ba98,
//                           `fdivrs 1.0f` for the reciprocal, the three
//                           results assembled in the named three-float
//                           local vScreen (the `rep movsl $3` at
//                           0x1003e845 copies it in) and the reciprocal
//                           into +0x0c.  Then `jmp 0x1003fe9d` --
//                           straight to the texture block, no clip and
//                           no projection.
//   0x1003e862..0x1003e89b  THE CLIPPED PATH's pre-transform: nVerts
//                           calls to 0x1000d9b0(pVert, g_1008b880), then
//                           g_n1008b874 is RE-READ at 0x1003e897.
//   0x1003e89c..0x1003e8ba  UseD3DClip.  `nClip = g_n1008b874;
//                           if (g_n1008d7a0) { nClip &= 1;
//                           if (!nClip) goto L_Project; }` -- and the
//                           `goto` skips the clipper's own
//                           `pVerts = pIn ; nVerts = nIn` epilogue at
//                           0x1003fe53, landing at 0x1003fe68.
//   0x1003e8bb..0x1003ec6e  CLIP PASS 1, bit 0x01, NEAR: z >= g_f1008b908.
//   0x1003ec6f..0x1003f00f  CLIP PASS 2, bit 0x04, LEFT: -z < x.
//   0x1003f010..0x1003f3a8  CLIP PASS 3, bit 0x08, TOP: y < z.
//   0x1003f3a9..0x1003f741  CLIP PASS 4, bit 0x10, RIGHT: x < z.
//   0x1003f742..0x1003fae2  CLIP PASS 5, bit 0x20, BOTTOM: -z < y.
//   0x1003fae3..0x1003fe52  CLIP PASS 6, bit 0x02, FAR: z <= g_f1008b910.
//   0x1003fe53..0x1003fe67  the clipper's epilogue, `pVerts = pIn ;
//                           nVerts = nIn`, skipped by the UseD3DClip
//                           early exit.
//   0x1003fe68..0x1003fe9c  L_Project: nVerts calls to
//                           0x10014580(pVert, g_1008b880).
//   0x1003fe9d..0x1003ff20  FIND THIS STAGE'S TEXTURE RECORD.
//                           big2ef34.cpp / big61a30.cpp / big6d910.cpp's
//                           block field for field, including the DOUBLED
//                           null test: 0x1003feca and 0x1003fed6 are two
//                           `je`s off the ONE `test %esi,%esi` at
//                           0x1003fec4, and icl 4.0 does not fold a
//                           redundant null test.
//   0x1003ff21..0x10040016  L_Bind.  g_a1008e684[stage] published, the
//                           LRU move-to-front of the +0x1c CheapLTLink
//                           through 0x1008e634, `g_p1008cbcc->+0x48 +=`,
//                           SetTexture at +0x8c, ALPHAREF at +0x50, the
//                           inverse-size pair into g_a1008e664[stage],
//                           the counter at 0x1008bd80, SetLOD at +0xbc.
//   0x10040017..0x1004005a  L_Draw: scale the texture pair by stage
//                           zero's published inverse size.
//   0x1004005b..0x10040235  THE DRAW.  One DrawPrimitive normally; when
//                           the record's own vtable slot 2 says yes AND
//                           g_n1008ceac is set, THREE calls and four
//                           render-state changes, with every vertex's
//                           diffuse overwritten by the constant colour
//                           in between.
//   0x10040236..0x1004026c  `if (pObj->m_pLights2c) sub_1003dd50(pObj,
//                           pVerts, nVerts);`, `g_n1008be5c++`, and the
//                           single epilogue.
//   0x1004026d..0x10040589  THE SOFTWARE LIGHT BLOCK, sunk (section 3a).
//   0x1004058a..0x10040592  the 0x1000a740 failure trampoline.
//   0x10040593..0x1004067b  THE SIX SUNK CLIP EPSILON ARMS, one per
//                           pass, in forward pass order.
//   0x1004067c..0x100406b1  L_ElseLOD.
//   0x100406b2..0x10040709  L_MakeRec's two arms and the shared
//                           sub_10025680 give-up exit.
//   0x1004070a..0x10040720  the two record-search `break` trampolines.
//   0x10040721..0x10040747  the light block's loop tail and its join
//                           back to 0x1003e74c.
//   0x10040748..0x10040775  the three sunk `< 0` colour clamps.
//
//  ------------------------------------------------------------------- //
//  11. COIN-FLIPS, each with both spellings stated so one build settles
//      it.  Only the first can move a call.
//  ------------------------------------------------------------------- //
//
//   1. **THE ONLY ARM ORDER THAT MOVES CALLS.**  At 0x10040071 and
//      0x1004007e two `je 0x100401fa` guard the three-draw path; the
//      single-draw path sits BELOW it at 0x100401fa and the three-draw
//      path falls through and ends `jmp 0x10040236`.  The rule
//      ("success falls through, error is jumped to") makes this
//          if (pRec->Slot08() && g_n1008ceac) { three draws } else { one }
//      which is what is written.  The other spelling,
//          if (!pRec->Slot08() || !g_n1008ceac) { one } else { three }
//      emits the ONE-draw arm inline and sinks the three-draw arm, which
//      would predict the DrawPrimitive at 0x10040233 FIRST in the
//      sequence and the other two last -- three shipped-only and three
//      ours-only rows, all indirect.  If the result looks like that,
//      swap the arms.
//
//   2. `if (fDist < 0.0f) fDist = -fDist;` (written) versus
//      `fDist = (fDist < 0.0f) ? -fDist : fDist;`.  0x1004037f
//      `jbe 0x10040383` jumps OVER the `fchs`, so the negation is the
//      fall-through; the 0.0f is loaded far earlier (0x1004030b) as a
//      hoisted pool constant, so the compare's operand order says
//      nothing.  big64cc0.cpp writes the `if`; so does this.  No call
//      either way.
//
//   3. `vLightPos` as a struct copy plus three `double` locals (written)
//      versus three `double` locals initialised straight from
//      `pLight->m_vPos`.  The `rep movsl $3` at 0x100402a7 forces a
//      float 12-byte copy, and `fsubl -0xd0(%ebp)` in the vertex loop
//      forces double lvalues, so BOTH exist; what is not pinned is
//      whether the three doubles are declared inside the light loop (and
//      icl rematerialised them into the rotated loop's latch, which is
//      the 0x100403d7 block) or outside it.  Written outside.
//      big64cc0.cpp's copy has float locals only and loads each
//      component twice, so this really is a different source revision.
//
//   4. THE CONSTANT COLOUR at 0x100400eb..0x10040138 is transcribed
//      literally:
//          n  = ((r << 16) & 0x00ff0000) | ((g << 8) & 0x0000ff00);
//          n  = (n & 0xffffff00) | b;
//          n  = (n & 0x00ffffff) | 0xff000000;
//      The two masks on the ACCUMULATOR (`and $0xffffff00` at 0x10040119
//      and `and $0xffffff` at 0x1004012f) are not redundant-looking
//      artefacts: the values being OR-ed in are already `movzbl`, so icl
//      would have had no reason to mask them, and masking the
//      accumulator instead is only legal if the source asked for the
//      field to be cleared.  The alternative is a `RGBA_MAKE`-style
//      macro whose four terms carry their own `& 0xff`; that would leave
//      the two accumulator masks unexplained.  No call either way.
//
//   5. `nStage` as a named local (written, `mov 0x1008d7d0,%edx ;
//      mov %edx,-0x80(%ebp)` at 0x1003fea3/0x1003feac) versus
//      `g_n1008d7d0` written out at all four uses.  The slot is SHARED
//      with pass 6's `fT`, which a function-scope local is not, so the
//      local must be block-scoped or absent; written as a local because
//      that is the only way to say it.  No call either way.
//
//   6. `pBlock->m_pHead0c` versus a cached `pCur`.  The third test of
//      the list head at 0x100406b2 uses %esi, the register the walk
//      cached it in, and the link-in at 0x100406ca..0x100406d2 works
//      through the same register.  big6d910.cpp writes
//      `pBlock->m_pHead0c` at all three sites and that is followed here;
//      a cached local would produce the same code and the same calls.
//
//   7. `while (nLeft--)` at every loop (written) versus an explicit
//      `> 0` guard.  Five of the seven loop guards are `test ; je`
//      (0x1003e770, 0x1003e879, 0x1003fe7f, 0x1004002e, 0x10040141) and
//      two are the unsigned forms `test ; jbe` (0x1003e680, the fill)
//      and `test ; ja` (0x100403d0, the light vertex loop).  Under icl,
//      `while (n > 0)` on an unsigned gives `jbe` and `while (n)`
//      gives `je`, so the two odd ones were probably spelled `> 0`.  All
//      seven bodies are `dec`-counted (census: `dec` 7), so the loop
//      SHAPE is settled; only the guard encoding is open, and no call
//      moves.
//
//  ------------------------------------------------------------------- //
//  12. WHAT COULD NOT BE PINNED.  There are NO `// UNDECODED:` gaps --
//      every byte is covered by a region above -- but five OPERANDS are
//      named only by address:
//  ------------------------------------------------------------------- //
//
//    * 0x1008ce8c's callee.  d3dr3d.cpp records that 0x1002b0e0 stores
//      one of 0x10023570 / 0x100234d0 / 0x100235d0 / 0x100234e0 there
//      per surface format, and big17c14.cpp reads the same slot with the
//      same register pair.  WHICH of the four is installed at run time is
//      not decidable here, and the slot's true prototype beyond
//      "(%ecx, %edx), nothing pushed" is not either.
//    * the vtable slot at +0x08 of the texture record (0x1004006f).  It
//      takes no arguments, returns a truth value in %eax and gates the
//      three-draw path.  No other decoded file calls it, so it is
//      declared `_v08()` and named by its offset.
//    * 0x1008ceac.  d3dqip3.cpp records the two stores that set it
//      (0x100264bd and 0x100264c4) but not what it means.
//    * the three 256-entry ramps at 0x1008cebc / 0x1008cfbc / 0x1008d0bc.
//      big16324.cpp names the OTHER set (0x1008d1bc) `g_ColorRamp2`;
//      these are the first set and no decoded file names them, so they
//      are `g_aColorRamp1008cebc` etc.
//    * 0x1008d7a0 (UseD3DClip), 0x1008b874 (the caller's clip mask) and
//      0x1008b880 (the transform object) are named from big051a0.cpp,
//      big6d910.cpp and big2ef34.cpp, which is naming and not decoding.
//
// *********************************************************************** //

// Only ltbasedefs.h, exactly as big3c40.cpp, big051a0.cpp, big6d910.cpp
// and big2ef34.cpp have it.
#include "ltbasedefs.h"
#include <math.h>


// ======================================================================= //
//  DECLARATIONS
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  THE DRAW VERTEX.  Stride 0x20 = 32, from `add $0x20,%edx` in every
//  classify loop (6), `add $0x20,%esi` in every edge loop (6),
//  `add $0x20,%eax` after every copy (6), `mov $0x20,%eax ; add %edi,%eax`
//  after every lerp (6), `shl $0x5` x18 for pEnd/pPrev/pOut, `sar $0x5`
//  x6 for the six divisions, `add $0x20,%ebx` in the fill, transform,
//  projection, uv-scale, colour-fill and light loops -- and,
//  independently, FVF 0x1c4 at all three DrawPrimitive sites.
//
//  The fields are read off the shipped operands:
//    +0x00 / +0x04 / +0x08   flds/fstps (%esi) 0x4 0x8, (%ecx) 0x4 0x8,
//                            (%edx) 0x4 0x8 -- x, y, z.  Also the
//                            `rep movsl $3` destination at 0x1003e845.
//    +0x0c                   written ONLY by the transform
//                            (`fstps 0xc(%ebx)` at 0x1003e84a) and
//                            otherwise carried by the 8-dword copy.
//                            FVF 0x1c4 says XYZRHW.
//    +0x10 +0x11 +0x12 +0x13 four bytes of ONE dword: movzbl/mov %dl in
//                            the six clip passes, three ramp stores in
//                            the fill, three clamp stores in the light
//                            block, and ONE dword store of the constant
//                            colour at 0x1004014a.
//    +0x14..+0x17            +0x14 is `&pVert->m_Specular14`, the second
//                            argument of the g_pfn1008ce8c call
//                            (0x1003e686 `lea 0x14(%ebx),%esi`); only
//                            +0x17 is named, as the fifth byte lerp.
//    +0x18 / +0x1c           flds 0x18(%esi)/0x1c(%esi), fsubs/fadds
//                            0x18(%eax)/0x1c(%eax), fstps 0x18(%esi)/
//                            0x1c(%esi) -- SIX of each in the clipper,
//                            written by 0x1000d9d0 in the fill and
//                            scaled at 0x1004003b.  ONE texture pair.
//  Nothing anywhere in the 8,533 bytes references +0x20 or +0x24 of a
//  vertex pointer.  8 dwords = 32 bytes = the `rep movsl` copy.
// ---------------------------------------------------------------------- //
struct Vec3e620
{
	float			x;                                  // +0x00
	float			y;                                  // +0x04
	float			z;                                  // +0x08
};

union VColor3e620
{
	uint32			m_n;
	uint8			m_b[4];
};

struct Vert3e620
{
	Vec3e620		m_vPos;                             // +0x00
	float			m_fRHW;                             // +0x0c
	VColor3e620		m_Diffuse10;                        // +0x10
	VColor3e620		m_Specular14;                       // +0x14
	float			m_fU0;                              // +0x18
	float			m_fV0;                              // +0x1c

	// 0x1000d9d0 -- d3dbatchb.cpp's 21-byte two-field __thiscall setter
	// (`mov 0x8(%esp),%eax ; mov 0xc(%esp),%edx ; mov %eax,0x18(%ecx) ;
	// mov %edx,0x1c(%ecx) ; ret $0x8`).  Both arguments move with `mov`
	// and not `fld`/`fstp`, so they are BIT PATTERNS: the source hands
	// over the raw dwords.  DECLARED BUT NOT DEFINED here, exactly as
	// big6d910.cpp does, so icl has to emit the call -- the rule
	// "a tiny accessor spelled as a field read makes no call at all".
	void			sub_1000d9d0(uint32 nU, uint32 nV);
};                                                      // = 0x20


// ---------------------------------------------------------------------- //
//  THE SOURCE VERTEX.  0x18 = 24 bytes (`lea 0x18(%eax),%eax` at
//  0x1003e713), and +0x00 is a POINTER dereferenced for three dwords at
//  0x1003e693..0x1003e6a2.  Field for field big2ef34.cpp's
//  SrcVert2ef34; every offset re-read here.
// ---------------------------------------------------------------------- //
struct SrcVert3e620
{
	Vec3e620*		m_pPos00;                           // +0x00
	uint32			m_nU04;                             // +0x04
	uint32			m_nV08;                             // +0x08
	uint8			m_pad_0c[0x14 - 0x0c];
	uint8			m_nBlue14;                          // +0x14
	uint8			m_nGreen15;                         // +0x15
	uint8			m_nRed16;                           // +0x16
	uint8			m_pad_17;                           // +0x17
};                                                      // = 0x18


// ---------------------------------------------------------------------- //
//  The plane reached through pObj->m_p28->m_pPlane34.  Read as four
//  floats at (%eax), 0x4, 0x8 (the normal, multiplied by the light
//  position) and 0xc (subtracted) -- 0x100402e9..0x10040339.
//  big64cc0.cpp reads the same +0x28 -> +0x34 chain the same way.
// ---------------------------------------------------------------------- //
struct Plane3e620
{
	Vec3e620		m_vNormal;                          // +0x00
	float			m_fDist;                            // +0x0c
};


// ---------------------------------------------------------------------- //
//  The light and its list node.  Offsets from 0x1004029c..0x100403be and
//  0x10040727; identical to big64cc0.cpp's LightObj/LightRef and to
//  0x1002ec60's and 0x10064800's.
// ---------------------------------------------------------------------- //
struct LightObj3e620
{
	uint8			m_pad_00[0x94];
	uint8			m_nR94;                             // +0x94
	uint8			m_nG95;                             // +0x95
	uint8			m_nB96;                             // +0x96
	uint8			m_pad_97[0x1a8 - 0x97];
	float			m_fRadius1a8;                       // +0x1a8
};

struct LightRef3e620
{
	LightRef3e620*	m_pNext;                            // +0x00
	LightObj3e620*	m_pObject;                          // +0x04
	Vec3e620		m_vPos;                             // +0x08
};


// ---------------------------------------------------------------------- //
//  ltlink.h's CheapLTLink, as the texture record's +0x1c member.  The
//  dl_Remove / dl_TieOff / dl_Insert sequence at 0x1003ff3d..0x1003ff6f
//  reads +0x00 as prev and +0x04 as next.
// ---------------------------------------------------------------------- //
struct Link3e620
{
	Link3e620*		m_pPrev;                            // +0x00
	Link3e620*		m_pNext;                            // +0x04
	void*			m_pData;                            // +0x08
};


struct DDSurface3e620;

struct DDSurfaceVtbl3e620
{
	uint8			m_pad_00[0xbc];
	long (__stdcall* SetLOD)(DDSurface3e620*, uint32);   // +0xbc
};

struct DDSurface3e620
{
	DDSurfaceVtbl3e620*	lpVtbl;
};


// ---------------------------------------------------------------------- //
//  THE PER-STAGE TEXTURE RECORD.  Field for field big2ef34.cpp's
//  Tex2ef34, big61a30.cpp's Tex61a30 and big6d910.cpp's Tex6d910 -- with
//  ONE addition this function forces and those three did not see:
//  +0x00 IS A REAL VTABLE AND SLOT 2 IS CALLED.
//
//      1004005b  mov 0x1008d7d0,%eax
//      10040060  mov 0x1008e684(,%eax,4),%ecx     ; this
//      10040067  mov (%ecx),%eax                  ; the vptr
//      10040069  mov 0x8(%eax),%eax               ; slot +0x08
//      1004006f  call *%eax                       ; nothing pushed
//      10040071  test %eax,%eax
//
//  big2ef34.cpp says of its own copy "Its vtable is never called from
//  here, so slot 0 is a plain pointer"; here it is called, so the class
//  is declared polymorphic with three slots and the fields start at
//  +0x04 behind the implicit vptr (the "pad that forgot the
//  vptr" -- the arithmetic is written out so it cannot drift).
// ---------------------------------------------------------------------- //
struct TexBase3e620
{
	// The two slots below +0x08 are never reached from this function.
	virtual void	_v00() = 0;                         // vtbl +0x00
	virtual void	_v04() = 0;                         // vtbl +0x04
	virtual int32	_v08() = 0;                         // vtbl +0x08

	float			m_fInvW04;                          // +0x04
	float			m_fInvH08;                          // +0x08
	DDSurface3e620*	m_pSurface0c;                       // +0x0c
	uint32			m_nBytes10;                         // +0x10
	uint16			m_nStamp14;                         // +0x14
	uint16			m_nAlphaRef16;                      // +0x16
	uint8			m_pad_18[0x1c - 0x18];
	Link3e620		m_Link1c;                           // +0x1c
	uint8			m_pad_28[0x30 - 0x28];
};

// The record's texture half as a BASE class (w24_10059380_os.cpp's
// TexBase0ae20), so 0x1003dd50's bind can reach it through a base pointer.
struct Tex3e620 : public TexBase3e620
{
	Tex3e620*		m_pNext30;                          // +0x30
	uint8			m_pad_34[0x42 - 0x34];
	uint16			m_nStage42;                         // +0x42
	uint16			m_nDirty44;                         // +0x44
};


// The block reached through pObj->m_p28->m_pBlock30: a list head at
// +0x0c (0x1003fec1) and a sixteen-bit stamp at +0x30
// (`mov %ax,0x30(%edi)` at 0x1003fec6).
struct TexBlock3e620
{
	uint8			m_pad_00[0x0c];
	Tex3e620*		m_pHead0c;                          // +0x0c
	uint8			m_pad_10[0x30 - 0x10];
	uint16			m_nStamp30;                         // +0x30
};


// pObj->m_p28: the texture block at +0x30 (0x1003feaf) and the plane at
// +0x34 (0x100402e0).
struct Holder3e620
{
	uint8			m_pad_00[0x30];
	TexBlock3e620*	m_pBlock30;                         // +0x30
	Plane3e620*		m_pPlane34;                         // +0x34
};


// ---------------------------------------------------------------------- //
//  THE ARGUMENT.  Only the four fields this function reads are named:
//  +0x28 (0x1003fea9, 0x100402d7), +0x2c (0x1004023c, 0x1004027f) and
//  +0x50 (0x100403be).  +0x4c..+0x54 are 0x1000a700's own fields --
//  that callee is handed THIS pointer -- and are declared so the layout
//  is honest about where +0x50 sits.
// ---------------------------------------------------------------------- //
struct Obj3e620
{
	uint8			m_pad_00[0x28];
	Holder3e620*	m_p28;                              // +0x28
	LightRef3e620*	m_pLights2c;                        // +0x2c
	uint8			m_pad_30[0x4c - 0x30];
	void*			m_pIndices4c;                       // +0x4c
	uint16			m_nCount50;                         // +0x50
	uint16			m_nCount52;                         // +0x52
	uint8			m_Embedded54[4];                    // +0x54
};


// ---------------------------------------------------------------------- //
//  THE D3D7 DEVICE AT 0x1008d988.  Three vtable slots are reached:
//    +0x50  SetRenderState  (0x1003ffd1, 0x100400e8, 0x10040170,
//                            0x100401cc, 0x100401f5)
//    +0x64  DrawPrimitive   (0x100400bf, 0x100401ac, 0x10040233)
//    +0x8c  SetTexture      (0x1003ffa6)
//  Same declaration big2ef34.cpp, big61a30.cpp, big6d910.cpp and
//  big3dd50.cpp use.
// ---------------------------------------------------------------------- //
struct D3DDevice3e620;

struct D3DDeviceVtbl3e620
{
	uint8			m_pad_00[0x50];
	long (__stdcall* SetRenderState)(D3DDevice3e620*, uint32, uint32);
	long (__stdcall* GetRenderState)(D3DDevice3e620*, uint32, uint32*);   // +0x54 (0x1003dd50)
	uint8			m_pad_58[0x64 - 0x58];
	long (__stdcall* DrawPrimitive)(D3DDevice3e620*, uint32, uint32,
	                                void*, uint32, uint32);
	uint8			m_pad_68[0x8c - 0x68];
	long (__stdcall* SetTexture)(D3DDevice3e620*, uint32, void*);
	long (__stdcall* GetTextureStageState)(D3DDevice3e620*, uint32, uint32,
	                                       uint32*);                    // +0x90 (0x1003dd50)
	long (__stdcall* SetTextureStageState)(D3DDevice3e620*, uint32, uint32,
	                                       uint32);                     // +0x94 (0x1003dd50)
};

struct D3DDevice3e620
{
	D3DDeviceVtbl3e620*	lpVtbl;
};


// The 12-byte pair published per stage at 0x1008e664 --
// `mov %ecx,(%edx,%eax,8) ; mov %esi,0x4(%edx,%eax,8)` at
// 0x1003ffe3/0x1003ffe6 makes the element EIGHT bytes.
struct TexScale3e620
{
	float			m_fInvW00;                          // +0x00
	float			m_fInvH04;                          // +0x04
};

// 0x1008cbcc -> +0x48, `add %edx,0x48(%eax)` at 0x1003ff78.
struct Stats3e620
{
	uint8			m_pad_00[0x48];
	uint32			m_nBytes48;                         // +0x48
};


// ---------------------------------------------------------------------- //
//  THE TRANSFORM OBJECT at 0x1008b880.  Handed whole to 0x1000d9b0 and
//  0x10014580, and its 4x4 projective block read directly by the inline
//  transform: 0x1008ba5c = 0x1008b880 + 0x1dc, and the sixteen operands
//  the transform uses are exactly +0x1dc..+0x218 in row order.  Same
//  member names big6d910.cpp uses, and the same block big124a4.cpp,
//  big3dd50.cpp and big70d10.cpp read.
// ---------------------------------------------------------------------- //
struct XForm3e620
{
	uint8			m_pad_000[0x1dc];
	float			m_f1dc, m_f1e0, m_f1e4, m_f1e8;     // 0x1008ba5c..
	float			m_f1ec, m_f1f0, m_f1f4, m_f1f8;     // 0x1008ba6c..
	float			m_f1fc, m_f200, m_f204, m_f208;     // 0x1008ba7c..
	float			m_f20c, m_f210, m_f214, m_f218;     // 0x1008ba8c..
};


// ---------------------------------------------------------------------- //
//  Externals.  Relocations are masked, so only the shapes matter; each
//  is named by the address the shipped code loads it from.
// ---------------------------------------------------------------------- //
extern D3DDevice3e620*	g_pD3DDevice;                   // 0x1008d988

extern uint16			g_w1008b870;                    // 0x1008b870
extern uint32			g_n1008b874;                    // 0x1008b874  clip mask
extern Vert3e620*		g_p1008b87c;                    // 0x1008b87c  out bank
extern uint8			g_1008b880[];                   // 0x1008b880
extern float			g_f1008b908;                    // 0x1008b908  near
extern float			g_f1008b910;                    // 0x1008b910  far
extern uint32			g_n1008bd80;                    // 0x1008bd80
extern uint32			g_n1008be44;                    // 0x1008be44
extern uint32			g_n1008be48;                    // 0x1008be48
extern uint8			g_b1008be60;                    // 0x1008be60  alpha
extern uint32			g_n1008be5c;                    // 0x1008be5c
extern uint16			g_w1008bea8;                    // 0x1008bea8
extern int32			g_CV_Saturate;                  // 0x1008cb98
extern int32			g_CV_FogEnable;                 // 0x1008cb0c
extern Stats3e620*		g_p1008cbcc;                    // 0x1008cbcc
extern uint32			g_n1008ceac;                    // 0x1008ceac
extern void*			g_p1008d7a0;                    // 0x1008d7a0  UseD3DClip
extern uint32			g_n1008d7d0;                    // 0x1008d7d0  stage
extern Link3e620*		g_p1008e634;                    // 0x1008e634
extern TexScale3e620	g_a1008e664[];                  // 0x1008e664
extern Tex3e620*		g_a1008e684[];                  // 0x1008e684
extern int32			g_CV_LMDynamic;                 // 0x10091200

// The three 256-entry byte ramps the fill drives the source colour
// through, and whose LAST entry the constant colour reads
// (0x1008cfbb = 0x1008cebc + 0xff, and so on).  0x100598a0 is the same
// four-statement body over the OTHER set (0x1008d1bc / 0x1008d2bc /
// 0x1008d3bc), which big16324.cpp calls g_ColorRamp2.
extern uint8			g_aRamp1008cebc[256];           // 0x1008cebc  red
extern uint8			g_aRamp1008cfbc[256];           // 0x1008cfbc  green
extern uint8			g_aRamp1008d0bc[256];           // 0x1008d0bc  blue

// 0x1008ce8c -- the indirect call at 0x1003e6eb takes BOTH arguments in
// registers, %ecx = the vertex (i.e. &m_vPos) and %edx =
// &m_Specular14, and nothing is pushed; big17c14.cpp reads the same slot
// with the same pair at 0x10017d29 and d3dr3d.cpp records that
// 0x1002b0e0 installs one of 0x10023570 / 0x100234d0 / 0x100235d0 /
// 0x100234e0 there per surface format.  __fastcall is the only spelling
// with that register pair.
typedef void (__fastcall *Pfn1008ce8c)(Vec3e620* pPos, uint32* pOut);
extern Pfn1008ce8c		g_pfn1008ce8c;                  // 0x1008ce8c

// 0x1000a700 -- d3dbatchd.cpp.  (geometry, &data, &count); the three
// dwords are pushed right to left at 0x1003e64a..0x1003e64c.
extern "C" void  sub_1000a700(Obj3e620* pGeom, void** ppOutData,
                              uint32* pnOutCount);

// 0x1000a740 -- d3dbatchd.cpp.  `cmpl $0x80,arg ; jg` over "Error:
// vertex buffer overflow"; returns 1 when the count fits.
extern "C" int32 sub_1000a740(uint32 nVerts);

// 0x1000d9b0 -- d3dlow.cpp, __cdecl (vertex, transform object); it
// forwards to 0x1000a770 with the 3x4 at +0x15c and ORs the vertex's
// clip code into 0x1008b874, which is why that global is re-read after
// the loop.
extern "C" void  sub_1000d9b0(Vert3e620* pVert, uint8* pXForm);

// 0x10014580 -- the screen projection through +0x31c..+0x330, decoded in
// full at that address and called the same way by big124a4.cpp,
// big572b4.cpp and big24090.cpp.
extern "C" void  sub_10014580(Vert3e620* pVert, uint8* pXForm);

// 0x10025680 -- d3dmid.cpp, 32 bytes, __cdecl, ONE argument.  The
// `push %edi` in front of it at 0x100406da/0x100406ef is icl's alignment
// dword (the outgoing area is `add $0x8,%esp`), not a second argument.
extern "C" void  sub_10025680(Obj3e620* pObj);

// 0x10039bd0 -- d3dmid.cpp's stack-ABI entry; the body is at 0x10039bdc.
extern "C" Tex3e620* sub_10039bd0(TexBlock3e620* pBlock, uint32 nStage,
                                  uint32 bLink);

// 0x1003dd50 -- big3dd50.cpp's LMDynamic modulated second pass.  Tier Z:
// arg0 in %eax, arg1 in %edx, arg2 in %ecx, with a dead 16-byte
// outgoing area (`add $0xfffffff0,%esp` at 0x10040255).  big3dd50.cpp
// names 0x10040258 as its ONE caller and predicts these three
// arguments.  A register-ABI call is decoded as an ordinary call.
// ======================================================================= //
//  0x1003dd50 -- MOVED HERE from big3dd50.cpp, whose
//  declarations and banner notes follow (types renamed onto this file's).
//  It is a /Qip REGISTER-ONLY static of this TU: its prologue stores
//  %eax/%edx/%ecx into the frame (`mov %eax,-0x8(%ebp) ; mov %edx,
//  -0x18(%ebp) ; mov %ecx,-0x14(%ebp)` at 0x1003dd5d), no stack shim
//  precedes it, and its one caller is 0x1003e624 below (0x10040258,
//  `call 0x1003dd50` with the three arguments in registers).
// ======================================================================= //
// ======================================================================= //
struct CD3DConVar
{
	int32		m_nValue00;                             // +0x00
	float		m_fValue04;                             // +0x04
	uint8		m_pad_08[0x20 - 0x08];
};

extern CD3DConVar	g_varLMDynamic;                     // 0x10091200
extern CD3DConVar	g_varLMDynamicSize;                 // 0x100911c0
extern CD3DConVar	g_varLMDynamicScale;                // 0x100911e0

// 0x1008cb98 -- big21f90.cpp's `g_CV_Saturate`, the int32 backing store of
// the "Saturate" console variable.  Read once, at 0x1003e2d9, as the guard
// on the repeated DrawPrimitive.  big124a4.cpp and big19d64.cpp branch on
// the same global the same way.
extern int32		g_CV_Saturate;                      // 0x1008cb98


// ======================================================================= //
//  THE 4x4 MATRIX, and MakeInverse / Inverse from src/LT2/sdk/inc/ltmatrix.h.
//
//  d3dr4b.cpp decoded the out-of-line pair at 0x10015740 (MakeInverse) and
//  0x1000ac10 (Inverse).  Here both are INLINE -- there is no call in
//  0x1003df53..0x1003e11f except the `fabs` at 0x1003dfff -- so the bodies
//  have to be written in-class, and they are transcribed from the header
//  rather than re-derived.  Every landmark of the header body is present,
//  in order, and every address below was read off THIS function:
//
//    0x1003df53  `mov $0x1008ba5c,%esi ; lea -0x154(%ebp),%edi ;
//                 mov $0x10,%ecx ; rep movsl`        ret = *this
//    0x1003df6b  the i loop: four dwords copied per row (the inner
//                `mov 0x10(%edx,%eax,4)` / `mov %edi,0x10(%ecx,%eax,4)`
//                with %eax running -4..-1), then FOUR zero stores at
//                -0xf8/-0xfc/-0x100/-0x104 (pOut[7]..pOut[4], emitted high
//                to low), then `movl $0x3f800000,0x10(%ecx,%esi,4)` =
//                pOut[i+4] = 1.0f, then `mov %esi,-0x94(%ebp,%esi,4)` =
//                rowMap[i] = i.  Row stride 0x20 = eight floats, m stride
//                0x10 = four.  The header's setup loop exactly.
//    0x1003dfcc  `flds 0x100a9aec` -- dumped, 0x3a83126f = 0.001f, the
//                header's fLargest seed -- and `mov $0xffffffff,%esi`,
//                iLargest = -1.
//    0x1003dfe5  the pivot search: rowMap[iTest] * 0x20, + iRow*4,
//                `call 0x1007fc90` (fabs), `fcomp` against fLargest,
//                `jae` past the two updates.
//    0x1003e020  `cmp $0xffffffff,%esi ; je` -- return LTFALSE.  It lands
//                on the SAME copy-out as the normal exit, which is
//                MakeInverse ignoring Inverse's result and returning ret
//                either way.
//    0x1003e029  the three-way row swap through iTemp.
//    0x1003e03d  `flds 0x100a9b10` (dumped, 1.0f) then
//                `fdivs (%ebx,%edi,4)` -- mul = 1.0f / pRow[iRow]; the j
//                loop runs pRow to pRow+0x20, eight floats.
//    0x1003e072  `movl $0x3f800000,(%ebx,%edi,4)` -- pRow[iRow] = 1.0f.
//    0x1003e07b  the elimination loop, `cmp %edi,%esi ; je` for
//                `if (i == iRow) continue;`, `fchs` for the negation,
//                %eax running -8..-1 over the eight columns, and
//                `movl $0x0,(%edx,%ecx,2)` with %ecx = iRow*2 (spilled at
//                -0x28, 0x1003e094) -- pScaleRow[iRow] = 0.0f.
//    0x1003e0d8  the copy-out: pIn = mat[rowMap[i]] + 4 is
//                `lea -0x104(%ebp,%eax,1)` = -0x114 + 0x10, four dwords
//                per row.
//    0x1003e10d  `lea -0x84(%ebp),%edi ; lea -0x154(%ebp),%esi ;
//                 mov $0x10,%ecx ; rep movsl`        mInv = ret
//
//  The return type is written LTBOOL and discarded, as the header has it.
//
//  No longer transcribed -- the SDK's own LTMatrix
//  (ltbasedefs.h -> ltmatrix.h) with MakeInverse called as the header has
//  it, as was done for the twin big0ae20.cpp.  The same bytes as the
//  transcribed Mat3dd50 copy (measured 2530 at /O2 /Oi-, 1961 at /Os
//  /Oi-, both ways).
// ======================================================================= //

// 0x1008ba5c = 0x1008b880 + 0x1dc -- the 4x4 projective block of the
// transform object every other user of 0x1008b880 in the project takes as
// an opaque `uint8[]`.  Copied WHOLE here (`mov $0x1008ba5c,%esi ;
// mov $0x10,%ecx ; rep movsl` at 0x1003df53), so it is one matrix-shaped
// object at this site; big6d910.cpp's XForm6d910 names the same sixteen
// floats m_f1dc..m_f218.
extern LTMatrix		g_mat1008ba5c;                      // 0x1008ba5c


// ======================================================================= //
//  THE RENDER-STATE SAVER.  Eight bytes, state id at +0x00 and the saved
//  value at +0x04, exactly as d3dbatcha.cpp's C1000adc0 -- the same
//  constructor compiled OUT OF LINE at 0x1000adc0.  Here it is inlined at
//  all four sites, so both bodies are written in-class.
//
//  The constructor's Get is handed the PARAMETER and its Set is handed the
//  MEMBER reloaded from the frame (`mov -0x178(%ebp),%eax` at 0x1003ddc8
//  and its three copies) -- so the source names the member the second
//  time, not the argument.  d3dbatcha.cpp records the same split.  The
//  destructor reloads BOTH members from the frame (0x1003e4ee /
//  0x1003e4fe and its three copies).
// ======================================================================= //
class CStateSave
{
public:
	CStateSave(uint32 nState, uint32 nValue)
	{
		m_nState00 = nState;

		g_pD3DDevice->lpVtbl->GetRenderState(g_pD3DDevice, nState,
		                                     &m_nOld04);
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, m_nState00,
		                                     nValue);
	}

	~CStateSave()
	{
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, m_nState00,
		                                     m_nOld04);
	}

	uint32	m_nState00;                                 // +0x00
	uint32	m_nOld04;                                   // +0x04
};


// ======================================================================= //
//  THE TEXTURE RECORD AND ITS BLOCK.  Field for field big2ef34.cpp's
//  Tex2ef34 / TexBlock2ef34 (and big61a30.cpp's and big6d910.cpp's): the
//  bind block here reads exactly the same offsets in the same order.
//    +0x04/+0x08 the inverse-size pair    (0x1003e4b1, 0x1003e4b4)
//    +0x0c       the surface              (0x1003e471, 0x1003e4cf)
//    +0x10       the byte count           (0x1003e449)
//    +0x14       the frame stamp          (0x1003e409, 0x1003e456)
//    +0x16       the alpha reference      (0x1003e480)
//    +0x1c       the CheapLTLink          (0x1003e411..0x1003e441)
//    +0x30       the next-in-block chain  (0x1003e3bf, 0x1003e3d2,
//                                          0x1003e5e4, 0x1003e5ec)
//    +0x42       the stage                (0x1003e3b3, 0x1003e3f5, ...)
//    +0x44       the dirty flag           (0x1003e4c4, 0x1003e584)
// ======================================================================= //
// (R4: Link3e620, TexBase3e620, Tex3e620, TexBlock3e620, TexScale3e620
// and Stats3e620 are this TU's Link3e620, TexBase3e620, Tex3e620,
// TexBlock3e620, TexScale3e620 and Stats3e620 -- the same offsets.)

// ltlink.h's dl_Remove / dl_Insert, flat and file-static: under /Qip an
// external inline that calls an inline makes icl emit an out-of-line copy,
// and this TU's image holds none.
static inline void dl_Remove3e620(Link3e620* pLink)
{
	pLink->m_pPrev->m_pNext = pLink->m_pNext;
	pLink->m_pNext->m_pPrev = pLink->m_pPrev;
	pLink->m_pPrev = pLink->m_pNext = pLink;
}

static inline void dl_Insert3e620(Link3e620* pAfter, Link3e620* pLink)
{
	pLink->m_pPrev = pAfter;
	pLink->m_pNext = pAfter->m_pNext;
	pLink->m_pPrev->m_pNext = pLink->m_pNext->m_pPrev = pLink;
}


// ======================================================================= //
//  THE SURFACE (argument 0) AND ITS PIECES
//
//  pThis is only touched at TWO offsets in this copy, both reads:
//    +0x28  the object whose vtable slot 0 hands back the two basis
//           vectors (0x1003dd7c) and whose +0x30 is the texture block
//           (0x1003e383) -- and the same +0x28 that 0x1006a860 reads out
//           of its second argument at 0x1006a884
//    +0x2c  the head of the light list (0x1003e123)
//  The twin ALSO writes +0x48 and +0x49; this copy does not touch them,
//  so they are not declared here.
// ======================================================================= //
struct Vec3dd50
{
	float	x;                                          // +0x00
	float	y;                                          // +0x04
	float	z;                                          // +0x08
};

// The object at pThis->+0x28.  0x1003dd92 is `mov (%ecx),%eax ;
// mov (%eax),%eax ; call *%eax` with the object in %ecx and NOT on the
// stack, and only the two out-params in the 8-byte outgoing area which
// nothing cleans -- so this is an ORDINARY C++ VIRTUAL CALL on vtable slot
// 0 (__thiscall, `ret $0x8`), not the COM `lpVtbl->F(p)` spelling the
// device and the light interface below use.  Written as an abstract class
// so icl emits exactly that shape.
//
// UNLIKE THE TWIN, this copy also reads a DATA member: 0x1003e383 /
// 0x1003e389 is `mov 0x28(%eax),%eax ; mov 0x30(%eax),%edi` and what comes
// out is used as a texture block -- +0x0c walked as a list head, +0x30
// stamped.  The vtable pointer occupies +0x00, so the member sits 0x2c
// bytes past it.
class Basis3dd50
{
public:
	virtual void Slot00(Vec3dd50* pOut0, Vec3dd50* pOut1) = 0;      // +0x00

	uint8			m_pad_04[0x30 - 0x04];
	TexBlock3e620*	m_pBlock30;                         // +0x30
};

// The light's +0x04 interface.  Only vtable slot +0x38 is called, with
// nothing but `this` pushed (0x1003e18b..0x1003e197), and it answers a
// float in st(0).
// R4: w8_1006a860_os.cpp's spelling (identical there) -- the engine's
// CObjLight::GetLightRadius, a VIRTUAL __thiscall taking one unused
// argument, on a class deriving from a fourteen-slot base: `this` in %ecx
// and the slot loaded into %eax before `call *%eax`, as shipped.
struct LightBase3dd50
{
	virtual void Slot00() = 0;  virtual void Slot04() = 0;
	virtual void Slot08() = 0;  virtual void Slot0c() = 0;
	virtual void Slot10() = 0;  virtual void Slot14() = 0;
	virtual void Slot18() = 0;  virtual void Slot1c() = 0;
	virtual void Slot20() = 0;  virtual void Slot24() = 0;
	virtual void Slot28() = 0;  virtual void Slot2c() = 0;
	virtual void Slot30() = 0;  virtual void Slot34() = 0;
};

struct LightIface3dd50 : public LightBase3dd50
{
	virtual float Slot38(void* pUnused) = 0;                        // +0x38
};

// The light record.  +0x00 chains the list (0x1003e318), +0x04 is the
// interface above, and +0x08/+0x0c/+0x10 are the three floats subtracted
// from the world-space vertex: x at 0x1003e259 (`fsubs 0x8(%ebx)`), y at
// 0x1003e228 (`fsubp`, its operand loaded by `flds 0xc(%ebx)` at
// 0x1003e21e) and z at 0x1003e209 (`fsubrp`, operand loaded by
// `flds 0x10(%ebx)` at 0x1003e1f4).  World minus light in all three -- for
// the two DE-form pops that means `de eb` FSUBP and `de e2` FSUBRP,
// with objdump's swapped spelling applying to the first.
// They are read as a position, x then y then z.
struct Light3dd50
{
	Light3dd50*			m_pNext00;                      // +0x00
	union
	{
	LightIface3dd50*	m_pIface04;                     // +0x04
	void*				m_hObj04;                       // (w8_1006a860_os.cpp's union)
	};
	float				m_fX08;                         // +0x08
	float				m_fY0c;                         // +0x0c
	float				m_fZ10;                         // +0x10
};

struct Surf3dd50
{
	uint8			m_pad_00[0x28];
	Basis3dd50*		m_p28;                              // +0x28
	Light3dd50*		m_pLights2c;                        // +0x2c
};


// ======================================================================= //
//  THE DESCRIPTOR, 0x1c bytes at -0x44.  0x1006d550 fills +0x0c, +0x10,
//  +0x14 and +0x18 (`mov %edx,0xc(%ebx)` at 0x1006d575, `0x10(%ebx)` at
//  0x1006d584, `movl $0x0,0x14(%ebx)` at 0x1006d589 and `0x18(%ebx)` at
//  0x1006d5d2) and 0x1006d210 reads +0x0c and +0x10 back (0x1006d222,
//  0x1006d22b).  Nothing in THIS function ever reads it, so no member is
//  named; the size is the frame's, bounded above by -0x28 -- SEE THE
//  COIN-FLIP note in the banner.
//
//  Both members are callee-pop __thiscall: 0x1006d550 is `ret $0x10`
//  (four stack dwords) and 0x1006d210 is `ret $0x4` (one).
// ======================================================================= //
class CLightDesc3dd50
{
public:
	int32	sub_1006d550(Surf3dd50* pSurf, uint32 nA, uint32 nB,
	                     uint32 nC);                    // 0x1006d550
	int32	sub_1006d210(void* pMap);                   // 0x1006d210

	uint8	m_pad_00[0x1c];
};


// ======================================================================= //
//  EXTERNALS.  Relocations are masked, so only the shapes matter.
// ======================================================================= //
extern uint16			g_w1008b870;                    // 0x1008b870
extern uint16			g_w1008bea8;                    // 0x1008bea8
extern uint32			g_n1008bd80;                    // 0x1008bd80
extern uint32			g_n1008d7d0;                    // 0x1008d7d0  stage

// 0x1006a860 -- __cdecl, four stack dwords, plain `ret`; its own body opens
// with the SAME `mov 0x10091200,%eax ; test ; je` guard this function opens
// with, reads argument 1's +0x28 -> +0x34 and argument 2's +0x04 and +0x10,
// and answers a pointer in %eax.  THE FOURTH ARGUMENT IS 2.0f HERE
// (`push $0x40000000` at 0x1003e15e) where the twin pushes 1.0f.
extern "C" void* sub_1006a860(CLightDesc3dd50* pDesc, Surf3dd50* pSurf,
                              Light3dd50* pLight, float fVal);

// 0x10039bd0 -- d3dmid.cpp's stack-ABI entry; the body is at 0x10039bdc.
// Three arguments; big2ef34.cpp, big6d910.cpp and big43e10.cpp all reach
// it with the same (block, stage, bLink) triple.



// ======================================================================= //
//  THE VERTEX.  Stride 0x20 (`add $0x20,%eax` at 0x1003e293) and FVF
//  0x1c4 = D3DFVF_XYZRHW | DIFFUSE | SPECULAR | TEX1 (0x1003e2bc), which
//  is x, y, z, rhw, diffuse, specular, u, v = 32 bytes exactly.  This
//  function READS +0x00/+0x04/+0x08 and WRITES +0x18/+0x1c; the four
//  middle dwords it never touches are named from the FVF and nothing else.
// ======================================================================= //
struct Vert3dd50
{
	float	m_fX00;                                     // +0x00
	float	m_fY04;                                     // +0x04
	float	m_fZ08;                                     // +0x08
	float	m_fRHW0c;                                   // +0x0c
	uint32	m_nDiffuse10;                               // +0x10
	uint32	m_nSpecular14;                              // +0x14
	float	m_fU18;                                     // +0x18
	float	m_fV1c;                                     // +0x1c
};


// ======================================================================= //
//  0x1003dd50 .. 0x1003e612   (2,243 bytes)   1 caller, 0x10040258
//
//  The three arguments arrive in %eax, %edx and %ecx with a dead 16-byte
//  outgoing area (tier Z).  They are written as ordinary
//  parameters.
// ======================================================================= //

// ======================================================================= //
//  THE BIND: w24_10059380_os.cpp's identical BindBlock59380
//  -- w3_10014580_os.cpp's 0x10015320 as an inline -- over this file's
//  types, called once below with a LOD of 0.
// ======================================================================= //
static inline Tex3e620* FindRec3dd50(Tex3e620* pHead, uint32 nStage)
{
	Tex3e620* pCur;

	for (pCur = pHead; pCur; pCur = pCur->m_pNext30)
	{
		if (pCur->m_nStage42 == (uint8)nStage)
			return pCur;
	}

	return LTNULL;
}

static inline uint32 StageOf3dd50(Tex3e620* pRec)
{
	return pRec->m_nStage42;
}

static inline void SetScale3dd50(TexScale3e620* pScales, uint32 nStage, float fInvW, float fInvH)
{
	pScales[nStage].m_fInvW00 = fInvW;
	pScales[nStage].m_fInvH04 = fInvH;
}

static inline void BindRec3dd50(Tex3e620* pRec)
{
	TexBase3e620* pTex = pRec;

	g_a1008e684[StageOf3dd50(pRec)] = (Tex3e620*)pTex;

	if (pTex->m_nStamp14 != g_w1008bea8)
	{
		dl_Remove3e620(&pRec->m_Link1c);
		dl_Insert3e620(g_p1008e634, &pRec->m_Link1c);

		g_p1008cbcc->m_nBytes48 += pTex->m_nBytes10;

		pTex->m_nStamp14 = g_w1008bea8;
	}

	g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, (uint8)StageOf3dd50(pRec),
	                                 pTex->m_pSurface0c);

	if (pTex->m_nAlphaRef16)
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, 0x18, pTex->m_nAlphaRef16);

	SetScale3dd50(g_a1008e664, StageOf3dd50(pRec), pTex->m_fInvW04, pTex->m_fInvH08);

	g_n1008bd80++;
}

static inline int32 BindBlock3dd50(TexBlock3e620* pBlock, uint32 nStage, uint32 nLOD)
{
	Tex3e620* pRec;
	Tex3e620* pHead;
	Tex3e620* pNew;

	if (!pBlock)
		return 0;

	pBlock->m_nStamp30 = g_w1008b870;

	pHead = pBlock->m_pHead0c;
	pRec  = LTNULL;

	if (pHead)
		pRec = FindRec3dd50(pHead, nStage);

	if (!pRec)
		goto L_MakeRec;

	if (pRec == g_a1008e684[nStage])
		goto L_ElseLOD;

L_Bind:
	BindRec3dd50(pRec);

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

static void sub_1003dd50(Surf3dd50* pThis, Vert3dd50* pVerts, uint32 nVerts)
{
	// -- 0x1003dd66 ----------------------------------------------------- //
	//  The whole body is inside one positive test, and this is the ONLY
	//  path that reaches 0x1003dd6f without running a destructor -- which
	//  is why the four savers below have to be declared AFTER it.
	// ------------------------------------------------------------------- //
	if (!g_varLMDynamic.m_nValue00)
		return;

	// The frame order, lowest slot first.  vBasis0 at -0x190 and vBasis1
	// at -0x184 are the two 12-byte out-params of the slot-0 call; the uv
	// loop reads vBasis0 at -0x190/-0x18c/-0x188 (0x1003e251, 0x1003e22a,
	// 0x1003e1f7) and vBasis1 at -0x184/-0x180/-0x17c (0x1003e281,
	// 0x1003e271, 0x1003e279), which is what fixes which is which.
	Vec3dd50			vBasis0;                        // -0x190
	Vec3dd50			vBasis1;                        // -0x184

	// -- 0x1003dd92: __thiscall, two stack out-params, `ret $0x8`.  THE
	//    TWIN'S TWO `pThis->m_b48 = pThis->m_b49 = (uint8)LMDynamicSize`
	//    STORES ARE NOT HERE; 0x1003dd76 loads pThis and goes straight to
	pThis->m_p28->Slot00(&vBasis0, &vBasis1);

	// -- 0x1003dd9b..0x1003dece ----------------------------------------- //
	//  0x1b ALPHABLENDENABLE := 1, 0x13 SRCBLEND := 9 (D3DBLEND_DESTCOLOR),
	//  0x14 DESTBLEND := 2 (D3DBLEND_ONE), 0x22 ZWRITEENABLE := 0.  The
	//  four slots ascend in construction order and the four restores at
	//  0x1003e4eb..0x1003e57c run in exact reverse.  THE ONLY IMMEDIATE
	//  THAT DIFFERS FROM THE TWIN IS THE 9 AT 0x1003de25.
	// ------------------------------------------------------------------- //
	CStateSave			sAlphaBlend(0x1b, 1);           // -0x178
	CStateSave			sSrcBlend(0x13, 9);             // -0x170
	CStateSave			sDestBlend(0x14, 2);            // -0x168
	CStateSave			sZWrite(0x22, 0);               // -0x160

	//    `mov 0x28(%eax),%ecx`.
	uint32				nOldAddress;                    // -0x158

	// -- 0x1003decf..0x1003df52 ----------------------------------------- //
	//  Stage 0.  0x0c = D3DTSS_ADDRESS := 3 (D3DTADDRESS_CLAMP) -- a
	//  projected map must not tile -- and 1 = D3DTSS_COLOROP := 2
	//  (D3DTOP_SELECTARG1).  Plain statements, not a scoped class: the
	//  restores below are in the SAME order, and the second one restores a
	//  CONSTANT 4 rather than anything saved.
	// ------------------------------------------------------------------- //
	g_pD3DDevice->lpVtbl->GetTextureStageState(g_pD3DDevice, 0, 0x0c,
	                                           &nOldAddress);
	g_pD3DDevice->lpVtbl->SetTextureStageState(g_pD3DDevice, 0, 0x0c, 3);
	g_pD3DDevice->lpVtbl->SetTextureStageState(g_pD3DDevice, 0, 1, 2);

	// -- 0x1003df53..0x1003e11f: the inlined MakeInverse; see the class.
	LTMatrix			mInv;                           // -0x84

	mInv = g_mat1008ba5c.MakeInverse();

	CLightDesc3dd50		desc;                           // -0x44

	Light3dd50*			pLight;
	void*				pMap;
	Vert3dd50*			pWalk;
	uint32				nLeft;
	float				fScale;
	float				fW, fWX, fWY, fWZ, fDX, fDY, fDZ;


	// =================================================================== //
	//  0x1003e120..0x1003e321 -- THE PER-LIGHT LOOP.
	//
	//  `mov 0x2c(%eax),%ebx ; test %ebx,%ebx ; je 0x1003e322` then a body
	//  ending `mov (%ebx),%ebx ; test %ebx,%ebx ; jne 0x1003e130` -- the
	//  guarded do-while /O2 makes of a while loop over a +0x00 chain.  All
	//  THREE failure tests inside jump to 0x1003e318, the tail, not out of
	//  the loop.
	// =================================================================== //
	pLight = pThis->m_pLights2c;

	while (pLight)
	{
		// -- 0x1003e14e: __thiscall on `desc`, four stack dwords,
		//    `ret $0x10`.  DIFFERENCE 2 FROM THE TWIN: ONE
		//    `mov 0x100911c0,%eax` at 0x1003e133 feeds BOTH 0x8(%esp) and
		//    0xc(%esp), where the twin zeroes them and stamps the two
		//    bytes at pThis->+0x48/+0x49 instead.  0x1006d550's `ja` on
		//    argument 2 and `jbe` on argument 3 fall through to
		//    `movzbl 0x48(%esi)` and `movzbl 0x49(%esi)` when they are
		//    zero, which is what makes the two spellings the same request.
		if (desc.sub_1006d550(pThis, 1,
		                      g_varLMDynamicSize.m_nValue00,
		                      g_varLMDynamicSize.m_nValue00))
		{
			// -- 0x1003e166: __cdecl, four dwords, the fourth pushed as
			//    the raw immediate 0x40000000 = 2.0f (the twin pushes
			//    0x3f800000).  Only 0xc of the 0x10 is popped because the
			//    top slot is re-used one instruction later as 0x1006d210's
			//    argument.
			pMap = sub_1006a860(&desc, pThis, pLight, 2.0f);

			// -- 0x1003e176: __thiscall, one dword, `ret $0x4`.
			if (desc.sub_1006d210(pMap))
			{
				// -- 0x1003e183: the THIRD test, on the pointer that came
				//    back from 0x1006a860.
				if (pMap)
				{
					// -- 0x1003e18b: the light's +0x04 interface is loaded
					//    TWICE (`mov 0x4(%ebx),%eax` then
					//    `mov 0x4(%ebx),%ecx`) and pushed as the only
					//    argument to its own vtable slot +0x38 -- the COM
					//    spelling.  The float that comes back is scaled by
					//    LMDynamicScale (0x100911e4 = the record + 4) and
					//    reciprocated; 0x100a9af8 was dumped and is 1.0f.
					fScale = 1.0f
					       / (pLight->m_pIface04->Slot38(pLight->m_hObj04)
					          * g_varLMDynamicScale.m_fValue04);

					// ---------------------------------------------- //
					//  0x1003e199..0x1003e2a2 -- THE UV LOOP.  266
					//  bytes and NOT ONE CALL; see the call-free note
					//  in the banner.
					//
					//  Per vertex: undo the projection with mInv, take
					//  the result to world space, subtract the light
					//  position, and project the difference onto the
					//  two basis vectors the surface handed back.
					//
					//  mInv is the 64-byte block at -0x84, so
					//  mInv.m[r][c] is -0x84 + r*0x10 + c*4.  icl emits
					//  the four rows in the order w, z, y, x -- row 3 at
					//  0x1003e1be (-0x4c/-0x50/-0x54/-0x48), row 2 at
					//  0x1003e1dc (-0x5c/-0x60/-0x64/-0x58), row 1 at
					//  0x1003e201 (-0x6c/-0x70/-0x74/-0x68) and row 0 at
					//  0x1003e234 (-0x7c/-0x80/-0x84/-0x78) -- because
					//  the dot products below associate
					//  last-term-backwards and the z term is wanted
					//  first.  The divide is `fdivrs 0x100a9b00`
					//  (dumped, 1.0f) and both adds are `fadds` of
					//  0x100a9b04 / 0x100a9b08 (dumped, both 0.5f).
					//  The loop is entered by `jmp 0x1003e1be` past the
					//  v store, which /O2 has software-pipelined to the
					//  top of the NEXT trip (`fstps -0x4(%eax)` with
					//  %eax already advanced by 0x20) with a copy at
					//  0x1003e29d for the last one; the counter is
					//  `dec %edx ; jne`, a countdown, not an index.
					//
					//  4 BYTES LEFT (was 12).  Closed: the loop as a
					//  `for` with both steps in its header and nLeft
					//  set before pWalk (the shipped `mov -0x14,%edx`
					//  ahead of `mov -0x18,%eax`), and row 1's z
					//  product read through a float pointer to the
					//  matrix (icl sorts a sum's products by their
					//  operands' address forms -- w2_1000a8a0_os.cpp;
					//  one of 128 spellings of that row).  Left: the w
					//  row's z product loads the vertex first in the
					//  shipped code (`flds 0x8(%eax) ; fmuls -0x4c`)
					//  and the matrix first in ours.  Not moved by the
					//  128 m[3][c] / fm[12+c] x member / pv[c] spellings
					//  of the w row in all six term orders and both
					//  product orders, nor by those of rows 0 and 2.
					// ---------------------------------------------- //
					nLeft = nVerts;
					pWalk = pVerts;

					for (; nLeft; nLeft--, pWalk++)
					{
						const float* fm = &mInv.m[0][0];

						fW = 1.0f / (pWalk->m_fX00 * mInv.m[3][0]
						           + pWalk->m_fY04 * mInv.m[3][1]
						           + pWalk->m_fZ08 * mInv.m[3][2]
						           + mInv.m[3][3]);

						fWX = (pWalk->m_fX00 * mInv.m[0][0]
						     + pWalk->m_fY04 * mInv.m[0][1]
						     + pWalk->m_fZ08 * mInv.m[0][2]
						     + mInv.m[0][3]) * fW;

						fWY = (pWalk->m_fX00 * mInv.m[1][0]
						     + pWalk->m_fY04 * mInv.m[1][1]
						     + pWalk->m_fZ08 * fm[6]
						     + mInv.m[1][3]) * fW;

						fWZ = (pWalk->m_fX00 * mInv.m[2][0]
						     + pWalk->m_fY04 * mInv.m[2][1]
						     + pWalk->m_fZ08 * mInv.m[2][2]
						     + mInv.m[2][3]) * fW;

						fDX = fWX - pLight->m_fX08;
						fDY = fWY - pLight->m_fY0c;
						fDZ = fWZ - pLight->m_fZ10;

						pWalk->m_fU18 = (vBasis0.x * fDX
						               + vBasis0.y * fDY
						               + vBasis0.z * fDZ) * fScale + 0.5f;

						pWalk->m_fV1c = (vBasis1.x * fDX
						               + vBasis1.y * fDY
						               + vBasis1.z * fDZ) * fScale + 0.5f;

					}

					// -- 0x1003e2d6: vtable +0x64, six dwords, `add
					//    $-0x18,%esp`.  6 = D3DPT_TRIANGLEFAN, 0x1c4 =
					//    XYZRHW | DIFFUSE | SPECULAR | TEX1 -- the same
					//    call the caller made on the same array six
					//    instructions before it called this function.
					g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, 6,
					                                    0x1c4, pVerts,
					                                    nVerts, 0);

					// -- 0x1003e2d9..0x1003e315 -------------------- //
					//  DIFFERENCE 7, THE ONE BLOCK THE TWIN HAS NO
					//  COUNTERPART FOR.  `mov 0x1008cb98,%eax ;
					//  test %eax,%eax ; je 0x1003e318` and then a
					//  byte-for-byte repeat of the fifteen instructions
					//  above, down to the same 0x14(%esp) := 0, the
					//  same 0x1c4, the same 6 and the same reload of
					//  -0x14 and -0x18.  The guard sits BETWEEN the two
					//  calls, not before the first, so it is a second
					//  statement and not a two-trip loop (coin-flip 1).
					//  Drawing the modulate-add pass twice doubles it;
					//  0x1008cb98 is big21f90.cpp's Saturate.
					// ---------------------------------------------- //
					if (g_CV_Saturate)
					{
						g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, 6,
						                                    0x1c4, pVerts,
						                                    nVerts, 0);
					}
				}
			}
		}

		pLight = pLight->m_pNext00;
	}

	// -- 0x1003e322 / 0x1003e34f: the two stage states, restored in the
	//    SAME order they were set.  The second goes back to the constant 4
	//    (D3DTOP_MODULATE), not to anything that was saved.
	g_pD3DDevice->lpVtbl->SetTextureStageState(g_pD3DDevice, 0, 0x0c,
	                                           nOldAddress);
	g_pD3DDevice->lpVtbl->SetTextureStageState(g_pD3DDevice, 0, 1, 4);

	// =================================================================== //
	//  0x1003e37a..0x1003e4ea -- FIND-OR-MAKE AND BIND THE RECORD.
	//  big2ef34.cpp's block with its two unbind copies removed: here every
	//  failure simply leaves the function, so all of them land on the
	//  destructor tail at 0x1003e4eb.
	//
	//  DIFFERENCE 8: the block comes from `pThis->m_p28->m_pBlock30`
	//  (0x1003e37a `mov -0x8(%ebp),%eax ; mov 0x28(%eax),%eax ;
	//  mov 0x30(%eax),%edi`) where the twin reads the global 0x1008beb8.
	//  0x1003e37d loads the stage BETWEEN those two, which is why nStage
	//  is assigned before the null test rather than after it.
	// =================================================================== //
	BindBlock3dd50(pThis->m_p28->m_pBlock30, g_n1008d7d0, 0);

L_Done:
	// -- 0x1003e4eb..0x1003e57f: the four destructors, in reverse
	//    construction order, then the single `jmp 0x1003dd6f` to the one
	//    epilogue.  Nothing sets %eax and the caller ignores the result,
	//    so this returns void.
	return;
}





// ---------------------------------------------------------------------- //
//  THE CLIPPER, INLINE.  0x1003e8bb..0x1003fe67 is
//  d3dr3c_os.cpp's driver 0x1000f4b0 with its six plane bodies inlined --
//  w55_10016324_os.cpp's set, here on the 32-byte Vert3e620, and the same
//  bodies w1_10001000.cpp / w44_10001670.cpp compile byte-identical out of
//  line.  The plane bodies take &pVerts / &nVerts, so both stay in their
//  frame slots (-0x664 / -0x668, stored and reloaded at 0x1003fe5c..
//  0x1003fe6e) as shipped; the old CLIP3E620_HEAD/TAIL macros, written
//  over function locals, kept pVerts in a register and had no -0x664.
//
//  0x10023380's body is LerpVert3e620 below: TWO texture floats and the
//  five bytes +0x12, +0x11, +0x10, +0x13, +0x17, each through RoundF (the
//  shipped `fstps -0x1708(%ebp) ; flds -0x1708(%ebp) ; fistpl -0x3c8(%ebp)`
//  at 0x1003f1e7).  This function also contains three conversions that DO
//  the control-word dance (the light block), so that is not a per-TU flag.
// ---------------------------------------------------------------------- //
// Round to nearest in the current FPU mode: w1_10001000.cpp's helper
// (0x10015190 out of line).  Inlined, it is the shipped `fstps t ;
// flds t ; fistpl n`.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}


#define CLIP_EPSILON    0.00001f
#define CLIP_MAX_VERTS  50

static inline void LerpVert3e620(Vert3e620* a, Vert3e620* b, Vert3e620* dst, float t)
{
	dst->m_fU0 = (b->m_fU0 - a->m_fU0) * t + a->m_fU0;
	dst->m_fV0 = (b->m_fV0 - a->m_fV0) * t + a->m_fV0;
	dst->m_Diffuse10.m_b[2]  = (uint8)RoundF((b->m_Diffuse10.m_b[2]  - a->m_Diffuse10.m_b[2])  * t + a->m_Diffuse10.m_b[2]);
	dst->m_Diffuse10.m_b[1]  = (uint8)RoundF((b->m_Diffuse10.m_b[1]  - a->m_Diffuse10.m_b[1])  * t + a->m_Diffuse10.m_b[1]);
	dst->m_Diffuse10.m_b[0]  = (uint8)RoundF((b->m_Diffuse10.m_b[0]  - a->m_Diffuse10.m_b[0])  * t + a->m_Diffuse10.m_b[0]);
	dst->m_Diffuse10.m_b[3]  = (uint8)RoundF((b->m_Diffuse10.m_b[3]  - a->m_Diffuse10.m_b[3])  * t + a->m_Diffuse10.m_b[3]);
	dst->m_Specular14.m_b[3] = (uint8)RoundF((b->m_Specular14.m_b[3] - a->m_Specular14.m_b[3]) * t + a->m_Specular14.m_b[3]);
}


static inline float ClipEdgeNear(Vec3e620* a, Vec3e620* b, Vec3e620* dst)
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


static inline float ClipEdgeLeft(Vec3e620* a, Vec3e620* b, Vec3e620* dst)
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


static inline float ClipEdgeTop(Vec3e620* a, Vec3e620* b, Vec3e620* dst)
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


static inline float ClipEdgeRight(Vec3e620* a, Vec3e620* b, Vec3e620* dst)
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


static inline float ClipEdgeBottom(Vec3e620* a, Vec3e620* b, Vec3e620* dst)
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


static inline float ClipEdgeFar(Vec3e620* a, Vec3e620* b, Vec3e620* dst)
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
static inline LTBOOL Plane32_10001000(uint32* pUnused, Vert3e620** ppVerts, int32* pnVerts, Vert3e620** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert3e620*  pOutStart;
	Vert3e620*  pV;
	Vert3e620*  pEnd;
	Vert3e620*  pPrev;
	Vert3e620*  pCur;
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

				LerpVert3e620(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001330(uint32* pUnused, Vert3e620** ppVerts, int32* pnVerts, Vert3e620** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert3e620*  pOutStart;
	Vert3e620*  pV;
	Vert3e620*  pEnd;
	Vert3e620*  pPrev;
	Vert3e620*  pCur;
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

				LerpVert3e620(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001670(uint32* pUnused, Vert3e620** ppVerts, int32* pnVerts, Vert3e620** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert3e620*  pOutStart;
	Vert3e620*  pV;
	Vert3e620*  pEnd;
	Vert3e620*  pPrev;
	Vert3e620*  pCur;
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

				LerpVert3e620(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001990(uint32* pUnused, Vert3e620** ppVerts, int32* pnVerts, Vert3e620** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert3e620*  pOutStart;
	Vert3e620*  pV;
	Vert3e620*  pEnd;
	Vert3e620*  pPrev;
	Vert3e620*  pCur;
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

				LerpVert3e620(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001cb0(uint32* pUnused, Vert3e620** ppVerts, int32* pnVerts, Vert3e620** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert3e620*  pOutStart;
	Vert3e620*  pV;
	Vert3e620*  pEnd;
	Vert3e620*  pPrev;
	Vert3e620*  pCur;
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

				LerpVert3e620(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001fe0(uint32* pUnused, Vert3e620** ppVerts, int32* pnVerts, Vert3e620** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert3e620*  pOutStart;
	Vert3e620*  pV;
	Vert3e620*  pEnd;
	Vert3e620*  pPrev;
	Vert3e620*  pCur;
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

				LerpVert3e620(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Clip32_1000f4b0(uint32 nClipFlags, Vert3e620** ppVerts, int32* pnVerts)
{
	Vert3e620*     pOut;
	Vert3e620*     pVerts;
	int32      nVerts;
	uint32     anUnused[6];

	if (g_p1008d7a0)
	{
		nClipFlags &= 1;
		if (!nClipFlags)
			return LTTRUE;
	}

	pOut   = g_p1008b87c;
	pVerts = *ppVerts;
	nVerts = *pnVerts;

	if (nClipFlags & 0x01)
	{
		if (!Plane32_10001000(&anUnused[0], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x04)
	{
		if (!Plane32_10001330(&anUnused[1], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x08)
	{
		if (!Plane32_10001670(&anUnused[2], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x10)
	{
		if (!Plane32_10001990(&anUnused[3], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x20)
	{
		if (!Plane32_10001cb0(&anUnused[4], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (!(nClipFlags & 0x02) ||
	    Plane32_10001fe0(&anUnused[5], &pVerts, &nVerts, &pOut))
	{
		*ppVerts = pVerts;
		*pnVerts = nVerts;
		return LTTRUE;
	}

	return LTFALSE;
}


// ======================================================================= //
//  0x1003e620 .. 0x10040775   (8,533 bytes)   ONE caller, 0x10040878
//
//  __cdecl with one argument: under /Qip the register entry at
//  0x1003e624 takes it in %eax and the four-byte stack entry at
//  0x1003e620 lifts it out of 0x4(%esp).  That is decoded
//  as an ordinary call.
// ======================================================================= //

// d3dr2a.cpp's 0x1005fa80, inline here: run each vertex through
// 0x10014580 against 0x1008b880.
static inline void ProjectVerts3e620(Vert3e620* pVerts, uint32 nVerts)
{
	while (nVerts--)
	{
		sub_10014580(pVerts, g_1008b880);
		pVerts++;
	}
}


void sub_1003e620(Obj3e620* pObj)
{
	// icl 4.0 places the memory-resident locals in order of FIRST USE,
	// lowest slot first (measured on a test TU: naming
	// aVerts before the 0x1000a700 call moves it below pSrc/nVerts): the
	// lerp temps are the inlined plane bodies', then aVerts at -0x166c,
	// pSrc/nVerts/pVerts at -0x66c/-0x668/-0x664, vLightPos (-0x660),
	// vScreen (-0x654), then the six clip blocks.
	//
	// MEASURED: 8112 -> 8402 bytes (shipped 8532), agree
	// 0.07 -> 0.39 at own-address pairing, still ATTEMPTED:
	// the frame is 0x175c against the shipped 0x176c.  Everything below
	// the spill area now sits exactly 0x10 above its shipped slot; the 16
	// bytes are in the spill area: the shipped code keeps pWalk (-0xc0)
	// in a slot of its own where ours shares the 8-byte distSqr slot, and
	// the dead area at the top of the frame (-0x74..-0x9, nothing
	// addresses it) is 8 bytes larger than ours.  Tried and measured no
	// help: the 0x1000d9b0 loop as an inline (icl declines to inline it
	// once the clipper is inlined), the light block as an inline (not
	// inlined), the two loops reading pWalk instead of re-assigning it.
	// The unaddressed run at the top of the frame is
	// icl's SPILL-SLOT residue from the inlined clipper, not any
	// variable: with only the near plane it is 0 bytes, near+left+far 0,
	// four planes 20, all six 100 (the shipped run is 108).  anUnused,
	// the planes' pUnused argument (both deletable: identical bytes),
	// plane 1's pOutStart read first, every plane's, &(*ppVerts)[0] and
	// the driver's load order leave the frame at 0x175c.
	Vert3e620		aVerts[128];

	SrcVert3e620*	pSrc;
	uint32			nVerts;
	Vert3e620*		pVerts;

	Vec3e620		vLightPos;
	Vec3e620		vScreen;

	Vert3e620*		pWalk;
	uint32			nLeft;

	uint32			nFlags;

	float			fOOW;
	XForm3e620*		pM;

	LightRef3e620*	pRef;
	LightObj3e620*	pLightObj;
	Plane3e620*		pPlane;
	float			fDist;
	// float, not double: `flds 1.0f ; fdivl` at 0x1004039e
	// and `fsubrs 1.0f` at 0x10040455 are FLOAT constants; with double
	// locals icl folds them to double pool entries.  The 8-byte slots are
	// x87 spills, and fLightX..Z are the hoisted invariants' copies.
	float			fLightR, fLightG, fLightB;
	float			fRadius, fInvRadius, fRadiusSqr;
	float			fLightX, fLightY, fLightZ;
	float			fDistSqr, fAtten;
	int32			nR, nG, nB;

	TexBlock3e620*	pBlock;
	Tex3e620*		pRec;
	Tex3e620*		pHead;
	uint32			nStage;
	uint32			nColour;

	// ------------------------------------------------------------------ //
	//  0x1003e620..0x1003e677  PROLOGUE, THE GEOMETRY AND THE BOUND.
	//  0x1003e659 -- d3dbatchd.cpp's 0x1000a700, three dwords pushed
	//  right to left at 0x1003e64a..0x1003e64c and `add $0xc,%esp`.
	//  0x1003e668 -- 0x1000a740, whose own body is `cmpl $0x80,arg ; jg`
	//  over "Error: vertex buffer overflow".  128 is also the frame
	//  footprint of aVerts (0x166c - 0x66c = 0x1000 = 128 * 32).
	// ------------------------------------------------------------------ //
	pWalk = aVerts;

	sub_1000a700(pObj, (void**)&pSrc, &nVerts);

	if (!sub_1000a740(nVerts))
		return;					// 0x1003e672 -> 0x1004058e -> 0x10040266

	// ------------------------------------------------------------------ //
	//  0x1003e678..0x1003e725  THE FILL.  `pSrc` is RE-READ from its
	//  frame slot before every group (0x1003e689, 0x1003e6a5,
	//  0x1003e6b8, 0x1003e6cb, 0x1003e6ed, 0x1003e70a) because its
	//  ADDRESS was handed to 0x1000a700 and icl must assume aliasing --
	//  the known pattern "a value reloaded from its slot right after a store
	//  through an unrelated pointer is an address-taken local".
	//
	//  The position moves as THREE SEPARATE DWORD `mov`s
	//  (0x1003e693..0x1003e6a2), not a `rep movsl`, so the source
	//  assigns the three FIELDS and not the vector -- the same
	//  distinction big2ef34.cpp records from the other side ("this one
	//  assigns the vector" where big6d910.cpp's "emits three separate
	//  `mov`s").
	//
	//  The three ramped bytes are 0x100598a0's body over the FIRST ramp
	//  set, destination byte HIGHEST FIRST (+0x12, +0x11, +0x10), then
	//  the alpha byte from 0x1008be60 into +0x13.
	// ------------------------------------------------------------------ //
	nLeft = nVerts;

	while (nLeft--)
	{
		pWalk->m_vPos.x = pSrc->m_pPos00->x;
		pWalk->m_vPos.y = pSrc->m_pPos00->y;
		pWalk->m_vPos.z = pSrc->m_pPos00->z;

		pWalk->m_Diffuse10.m_b[2] = g_aRamp1008cebc[pSrc->m_nRed16];
		pWalk->m_Diffuse10.m_b[1] = g_aRamp1008cfbc[pSrc->m_nGreen15];
		pWalk->m_Diffuse10.m_b[0] = g_aRamp1008d0bc[pSrc->m_nBlue14];
		pWalk->m_Diffuse10.m_b[3] = g_b1008be60;

		// 0x1003e6eb -- `mov 0x1008ce8c,%eax ; call *%eax` with %ecx =
		// the vertex and %edx = &m_Specular14, nothing pushed.
		g_pfn1008ce8c(&pWalk->m_vPos, &pWalk->m_Specular14.m_n);

		// 0x1003e705 -- the __thiscall setter, two raw dwords.
		pWalk->sub_1000d9d0(pSrc->m_nU04, pSrc->m_nV08);

		pSrc++;
		pWalk++;
	}

	// ------------------------------------------------------------------ //
	//  0x1003e726..0x1003e73f  THE LMDynamic GATE.  0x10091200 is
	//  big3dd50.cpp's / d3dr5b.cpp's LMDynamic console variable, and
	//  0x1003e73a `je 0x1004026d` sends the software light pass to the
	//  BOTTOM of the function.
	//
	//  ** THE FLOAT POOL SAYS THIS `if` IS WRITTEN HERE, NOT AT THE
	//     BOTTOM (section 3a), and it now is: icl sinks
	//     the block to the bottom by itself, `sqrt` stays call 20 of 24,
	//     and written here vLightPos takes its shipped slot just above
	//     pVerts (-0x660) -- icl lays the frame out in order of first
	//     use, which is also why aVerts is named before the 0x1000a700
	//     call below.  Coin-flip note A at the end is settled. **
	// ------------------------------------------------------------------ //
	pVerts = aVerts;

	if (!g_CV_LMDynamic)
	{
		pRef = pObj->m_pLights2c;

		while (pRef)
		{
			pLightObj = pRef->m_pObject;			// 0x1004029c
			vLightPos = pRef->m_vPos;				// 0x100402a2, rep movsl $3

			// 0x100402be -- `mov 0x1008be48,%esi ; inc %esi ;
			// mov %esi,0x1008be48`, not `incl`.
			g_n1008be48++;

			// 0x100402d7..0x10040339.  Emitted last term first, which is
			// the known pattern "an FP sum emits its terms backwards".
			pPlane = pObj->m_p28->m_pPlane34;
			fDist  = pPlane->m_vNormal.x * vLightPos.x
			       + pPlane->m_vNormal.y * vLightPos.y
			       + pPlane->m_vNormal.z * vLightPos.z
			       - pPlane->m_fDist;

			// 0x1004030b `flds 0x100a9b28` (dumped: 0.0f) and 0x10040344
			// `fcomp %st(1) ; fnstsw ; sahf ; jbe ; fchs` -- fabs written as
			// a conditional negate.  See coin-flip (2).
			if (fDist < 0.0f)
				fDist = -fDist;

			// 0x10040348..0x10040378.  `flds 255.0f ; fsub %st(i),%st ;
			// DE E8+i`, and DE E8+i is ST(i) = ST(i) - ST(0) (section 8), so
			// the value stored is `c - (255.0f - c)`.  The POOL order
			// (0x100a9b1c red, 0x100a9b20 green, 0x100a9b24 blue) gives the
			// SOURCE order red, green, blue; the emission is the reverse.
			fLightR = pLightObj->m_nR94 - (255.0f - pLightObj->m_nR94);
			fLightG = pLightObj->m_nG95 - (255.0f - pLightObj->m_nG95);
			fLightB = pLightObj->m_nB96 - (255.0f - pLightObj->m_nB96);

			// 0x10040383 `fcomps 0x1a8(%ebx) ; ... ; jae 0x10040721`; the
			// member is read a SECOND time at 0x1004038b.
			if (fDist >= pLightObj->m_fRadius1a8)
				goto L_NextLight;

			fRadius = pLightObj->m_fRadius1a8;

			// 0x1004039e..0x100403ca.  0x100a9b30 is 1.0f (dumped).  The
			// square is stored first (-0xe8) and the reciprocal second
			// (-0x108), which is the x87 stack order, not the source order.
			fInvRadius = 1.0f / fRadius;
			fRadiusSqr = fRadius * fRadius;

			// 0x100403be -- `movzwl 0x50(%eax),%esi`: pObj's OWN +0x50, NOT
			// the count 0x1000a700 handed back into -0x668.  Read off the
			// bytes; big64cc0.cpp makes the same observation.
			nLeft = pObj->m_nCount50;

			if (nLeft == 0)
				goto L_NextLight;		// 0x100403d0 `ja` / 0x100403d2

			pWalk = pVerts;

			// 0x100403d7 is the rotated loop's latch and 0x100403fd the
			// entry, so the three double copies below are rematerialised in
			// the latch and hoisted into the preheader -- the known pattern "`for`
			// rematerialises a loop invariant into both predecessors".
			fLightX = vLightPos.x;
			fLightY = vLightPos.y;
			fLightZ = vLightPos.z;

			do
			{
				// 0x100403fd..0x10040425, z first.
				fDistSqr =
				    (pWalk->m_vPos.x - fLightX) * (pWalk->m_vPos.x - fLightX)
				  + (pWalk->m_vPos.y - fLightY) * (pWalk->m_vPos.y - fLightY)
				  + (pWalk->m_vPos.z - fLightZ) * (pWalk->m_vPos.z - fLightZ);

				// 0x10040427 `fldl -0xe8(%ebp) ; fcomp %st(1) ; ... ; jbe`.
				if (fRadiusSqr > fDistSqr)
				{
					// 0x1004043e..0x1004045b -- the CRT's DOUBLE sqrt at
					// 0x1007fc98, reached through
					// `add $0xfffffff8,%esp ; fstpl (%esp) ; call`, then
					// `fmull -0x108(%ebp)` and `fsubrs 0x100a9b34` (1.0f,
					// dumped).  THIS IS THE CALL THAT NEEDS /Oi-.
					fAtten = 1.0f - (float)sqrt(fDistSqr) * fInvRadius;

					// 0x10040463..0x1004053a.  The three `(int32)` casts are
					// icl's inline double->int sequence with the FULL
					// control-word dance (section 4).
					nR = pWalk->m_Diffuse10.m_b[2] + (int32)(fLightR * fAtten);
					nG = pWalk->m_Diffuse10.m_b[1] + (int32)(fLightG * fAtten);
					nB = pWalk->m_Diffuse10.m_b[0] + (int32)(fLightB * fAtten);

					// 0x1004053a..0x10040542 -- `add %eax,%eax` x3.
					// 0x1008cb98 is big21f90.cpp's Saturate console variable.
					if (g_CV_Saturate)
					{
						nR = nR + nR;
						nG = nG + nG;
						nB = nB + nB;
					}

					// 0x10040544..0x1004057c, with the three `< 0` arms sunk
					// to 0x10040757 / 0x10040748 / 0x10040766.
					if (nR > 255)     nR = 255;
					else if (nR < 0)  nR = 0;

					if (nG > 255)     nG = 255;
					else if (nG < 0)  nG = 0;

					if (nB > 255)     nB = 255;
					else if (nB < 0)  nB = 0;

					pWalk->m_Diffuse10.m_b[2] = (uint8)nR;
					pWalk->m_Diffuse10.m_b[1] = (uint8)nG;
					pWalk->m_Diffuse10.m_b[0] = (uint8)nB;
				}

				pWalk++;			// 0x1004057f
			}
			while (--nLeft);

L_NextLight:
			pRef = pRef->m_pNext;	// 0x10040721
		}

		// -- 0x10040737 / 0x10040743: the duplicated `pWalk = pVerts` and the
		//    join back into the hot path.
	}

	// -- 0x1003e740 / 0x10040737: the same two instructions in BOTH
	//    predecessors of 0x1003e74c.
	pWalk = pVerts;

	// -- 0x1003e74c: the clip mask the caller published at 0x10040871.
	nFlags = g_n1008b874;

	if (!nFlags)
	{
		// -------------------------------------------------------------- //
		//  0x1003e759..0x1003e861 -- NO CLIPPING WANTED.  One projective
		//  transform per vertex through 0x1008b880's +0x1dc..+0x218
		//  block (0x1008ba5c..0x1008ba98 absolute, all sixteen operands
		//  read off the shipped `fmuls`/`fadds`), `fdivrs 1.0f` for the
		//  reciprocal (0x1003e7a5; 0x100a9b3c dumped and is 0x3f800000),
		//  the three results assembled in a NAMED three-float local --
		//  the `rep movsl $3` at 0x1003e845 copies it into the vertex --
		//  and the reciprocal into +0x0c (0x1003e84a, reloaded from the
		//  `fstl -0x78(%ebp)` double spill at 0x1003e7e5).
		//  Instruction for instruction big6d910.cpp's 0x1006da4e and
		//  big124a4.cpp's 0x10012a65.
		// -------------------------------------------------------------- //
		pM = (XForm3e620*)g_1008b880;

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

		// 0x1003e85d -- straight to the texture block: no clip, no
		// projection.
		goto L_Tex;
	}

	// ------------------------------------------------------------------ //
	//  0x1003e862..0x1003e89b  THE CLIPPED PATH's pre-transform, then
	//  g_1008b874 is RE-READ at 0x1003e897.  0x1000d9b0 ORs each
	//  vertex's clip code into that global, which is why the second read
	//  is source and not a missed CSE -- big6d910.cpp records the same
	//  pair and cites 0x1000a8a0 doing it in the same place.
	// ------------------------------------------------------------------ //
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		sub_1000d9b0(pWalk, g_1008b880);
		pWalk++;
	}

	nFlags = g_n1008b874;

	// ------------------------------------------------------------------ //
	//  0x1003e89c..0x1003e8ba  UseD3DClip.  When the D3D clipper is on
	//  only the near plane is done here, and if that bit is clear the
	//  whole clipper is skipped AND so is the `pVerts = pIn ;
	//  nVerts = nIn` epilogue at 0x1003fe53 -- 0x1003e8b5 jumps to
	//  0x1003fe68, past it.
	// ------------------------------------------------------------------ //
	if (!Clip32_1000f4b0(nFlags, &pVerts, &nVerts))
		return;

L_Project:
	// -- 0x1003fe68..0x1003fe9c: camera space -> screen, one call per
	//    vertex, through the same transform object.
	ProjectVerts3e620(pVerts, nVerts);

L_Tex:
	// ------------------------------------------------------------------ //
	//  0x1003fe9d -- FIND (OR MAKE) THIS STAGE'S TEXTURE RECORD, then
	//  bind it.  big2ef34.cpp / big61a30.cpp / big6d910.cpp's block
	//  field for field; every offset re-read here.
	//
	//  The ONE difference from those three: when there is no block, or
	//  when no record can be made, this function calls 0x10025680 and
	//  RETURNS (0x100406da) instead of falling through to the draw.
	//
	//  The walk and the bind are this file's FindRec3dd50
	//  and BindRec3dd50 inlines (w24_10059380_os.cpp's identical set): the
	//  shipped `mov %ebx,%esi` copy of the record at 0x1003ff25, the
	//  `mov $0x1008e664,%edx ; (%edx,%eax,8)` scale pair at 0x1003ffd8,
	//  the stage kept in %edx for the walk, and pRec spilled to -0x8
	//  around L_ElseLOD's SetLOD are all theirs; written flat they were
	//  not (agree 0.08 -> 0.39 with this change alone).
	// ------------------------------------------------------------------ //
	nStage = g_n1008d7d0;
	pBlock = pObj->m_p28->m_pBlock30;

	if (!pBlock)
		goto L_GiveUp;			// 0x1003feb4 -> 0x100406da

	pBlock->m_nStamp30 = g_w1008b870;

	pHead = pBlock->m_pHead0c;
	pRec  = LTNULL;

	if (pHead)
		pRec = FindRec3dd50(pHead, nStage);

	if (!pRec)
		goto L_MakeRec;

	if (pRec == g_a1008e684[nStage])
		goto L_ElseLOD;

L_Bind:
	// -- 0x1003ff21 ---------------------------------------------------- //
	BindRec3dd50(pRec);

	if (pRec->m_nDirty44)
	{
		// 0x1004000b -- the surface's own vtable +0xbc.
		pRec->m_pSurface0c->lpVtbl->SetLOD(pRec->m_pSurface0c, 0);
		pRec->m_nDirty44 = 0;
	}

L_Draw:
	// ------------------------------------------------------------------ //
	//  0x10040017 -- scale the texture pair by stage ZERO's published
	//  inverse size.  0x1008e664 and 0x1008e668 are ELEMENT ZERO of the
	//  per-stage array 0x1003ffd4 just wrote with an index; the fold is
	//  the one big6d910.cpp and big61a30.cpp both record.  The loop is
	//  software-pipelined -- the two `fstps -0x8(%eax)` / `-0x4(%eax)`
	//  are sunk across the back edge and peeled once at 0x10040055.
	// ------------------------------------------------------------------ //
	pWalk = pVerts;
	nLeft = nVerts;

	while (nLeft--)
	{
		pWalk->m_fU0 *= g_a1008e664[0].m_fInvW00;
		pWalk->m_fV0 *= g_a1008e664[0].m_fInvH04;
		pWalk++;
	}

	// ------------------------------------------------------------------ //
	//  0x1004005b -- THE DRAW.  The record's own vtable slot 2 is asked
	//  first (no arguments, truth value in %eax) and 0x1008ceac second;
	//  BOTH must hold for the three-pass path.  The single-draw arm sits
	//  BELOW at 0x100401fa and the three-draw arm falls through, which
	//  is the `&&` spelling -- see coin-flip (1) in the banner, the only
	//  coin-flip here that can move a call.
	//
	//  6 = D3DPT_TRIANGLEFAN, 0x1c4 = XYZRHW | DIFFUSE | SPECULAR |
	//  TEX1, and the sixth argument is a literal 0 at all three sites.
	//  0x1b = ALPHABLENDENABLE, 0x1c = FOGENABLE, and 0x1008cb0c is
	//  big21f90.cpp's FogEnable console variable -- d3dr3a.cpp records
	//  it "gates the two SetRenderState(FOGENABLE) calls".
	// ------------------------------------------------------------------ //
	if (g_a1008e684[g_n1008d7d0]->_v08() && g_n1008ceac)
	{
		// 0x100400bf.
		g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, 6, 0x1c4,
		                                    pVerts, nVerts, 0);

		if (g_CV_FogEnable)
		{
			// 0x100400e8.
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, 0x1c, 0);
		}

		// -------------------------------------------------------------- //
		//  0x100400eb..0x10040138 -- the constant colour, transcribed
		//  literally; see coin-flip (4).  The three bytes are the LAST
		//  entry of the same three ramps the fill used
		//  (0x1008cfbb = 0x1008cebc + 0xff, 0x1008d0bb = 0x1008cfbc +
		//  0xff, 0x1008d1bb = 0x1008d0bc + 0xff), i.e. full brightness,
		//  and the alpha is forced to 0xff.
		// -------------------------------------------------------------- //
		nColour = (((uint32)g_aRamp1008cebc[255] << 16) & 0x00ff0000)
		        | (((uint32)g_aRamp1008cfbc[255] <<  8) & 0x0000ff00);
		nColour = (nColour & 0xffffff00) | (uint32)g_aRamp1008d0bc[255];
		nColour = (nColour & 0x00ffffff) | 0xff000000;

		// 0x10040143..0x10040151 -- one dword store per vertex.
		pWalk = pVerts;
		nLeft = nVerts;

		while (nLeft--)
		{
			pWalk->m_Diffuse10.m_n = nColour;
			pWalk++;
		}

		// 0x10040170.
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, 0x1b, 1);

		// 0x100401ac -- the same fan again, now additively blended.
		g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, 6, 0x1c4,
		                                    pVerts, nVerts, 0);

		// 0x100401cc.
		g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, 0x1b, 0);

		if (g_CV_FogEnable)
		{
			// 0x100401f5.
			g_pD3DDevice->lpVtbl->SetRenderState(g_pD3DDevice, 0x1c, 1);
		}
	}
	else
	{
		// 0x10040233 -- the ordinary single pass.
		g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, 6, 0x1c4,
		                                    pVerts, nVerts, 0);
	}

	// ------------------------------------------------------------------ //
	//  0x10040236..0x1004026c -- hand the drawn fan to the LMDynamic
	//  modulated pass and bump the frame counter.  big3dd50.cpp's
	//  banner predicts these three arguments from the callee's side and
	//  names 0x10040258 as its ONE caller.
	// ------------------------------------------------------------------ //
	if (pObj->m_pLights2c)
		sub_1003dd50((Surf3dd50*)pObj, (Vert3dd50*)pVerts, nVerts);

	g_n1008be5c++;

	return;					// 0x1004026c, the single epilogue


	// ================================================================== //
	//  THE SUNK TAIL, 0x1004026d..0x10040775, in SHIPPED ADDRESS ORDER.
	//  These are cold arms of the tests above; /O2 put them below the
	//  epilogue and they are placed here by hand so that the emitted
	//  call sequence matches.  The six clip-epsilon arms
	//  (0x10040593..0x1004067b) and the three `< 0` clamp arms
	//  (0x10040748..0x10040775) are icl's own sinking of the `else`
	//  halves already written above and have no source of their own.
	// ================================================================== //



L_ElseLOD:
	// -- 0x1004067c: the record is already bound to this stage, so only
	//    the LOD reset survives.  The same three statements as the tail
	//    of L_Bind; big6d910.cpp and big61a30.cpp record the same
	//    duplication.  pRec is spilled to -0x8(%ebp) across the call
	//    (0x1004069b) -- a dead-slot temp, not a source variable.
	if (pRec->m_nDirty44)
	{
		pRec->m_pSurface0c->lpVtbl->SetLOD(pRec->m_pSurface0c, 0);
		pRec->m_nDirty44 = 0;
	}

	goto L_Draw;				// 0x100406ad -> 0x10040017


L_MakeRec:
	// -- 0x100406b2: `test %esi,%esi ; je 0x100406ef` on the SAME list
	//    head that was walked above -- a THIRD test of it, in the
	//    register the walk cached it in.  The third argument is 1 when
	//    the list is non-empty and 0 when it is empty; both calls are
	//    four pushes (three arguments plus icl's alignment dword) with
	//    `add $0x10,%esp`.
	//
	//    The give-up block sits BETWEEN the two arms in the shipped
	//    bytes (0x100406da), which is why this is spelled with gotos
	//    rather than if/else: the if/else spelling issues 0x10039bd0
	//    twice and THEN 0x10025680, where the shipped order is
	//    0x10039bd0, 0x10025680, 0x10039bd0.  See coin-flip note B.
	if (!pBlock->m_pHead0c)
		goto L_MakeRecEmpty;

	pRec = sub_10039bd0(pBlock, nStage, 1);		// 0x100406be

	if (!pRec)
		goto L_GiveUp;

	pRec->m_pNext30              = pBlock->m_pHead0c->m_pNext30;
	pBlock->m_pHead0c->m_pNext30 = pRec;

	goto L_Bind;				// 0x100406d5 -> 0x1003ff21

L_GiveUp:
	// -- 0x100406da: no block, or no record could be made.  The
	//    `push %edi` in front of the argument is icl's alignment dword
	//    (the outgoing area is `add $0x8,%esp`), not a second argument;
	//    big64cc0.cpp declares 0x10025680 with ONE argument.
	sub_10025680(pObj);			// 0x100406e2
	return;						// 0x100406ea -> 0x10040266

L_MakeRecEmpty:
	pRec = sub_10039bd0(pBlock, nStage, 0);		// 0x100406f7

	if (!pRec)
		goto L_GiveUp;

	goto L_Bind;				// 0x10040705 -> 0x1003ff21
}


// *********************************************************************** //
//  TWO PLACEMENT NOTES, both stated so a build settles them.  Neither is
//  a decode question: both are about WHERE a block whose contents are
//  fully decoded was written in the original.
//
//  A. THE SOFTWARE LIGHT BLOCK.  [SETTLED: written in its
//     source position below the fill; icl sinks it itself and the call
//     sequence is unchanged.  The note is kept for the record.]
//     The float pool proves it is written
//     between the fill and the transform (section 3a) and that /O2 sank
//     it; it is spelled here as `if (!g_CV_LMDynamic) goto L_Lights;`
//     with the body at the bottom so that the CALL SEQUENCE matches --
//     `sqrt` is call 20 of 24 in the shipped order, and writing the
//     block in its source position would put it at position 6 and cost
//     one shipped-only plus one ours-only row at 0x1007fc98.  The
//     source-position spelling is
//
//         if (!g_CV_LMDynamic)
//         {
//             ... the light loop ...
//         }
//
//         pWalk = pVerts;
//         nFlags = g_n1008b874;
//
//     and it is almost certainly what the original said.  If a build
//     shows icl sinking that block by itself -- which /O2 should, since
//     it sank it in the shipped image -- then that spelling is strictly
//     better and should replace the `goto`.
//
//  B. L_MakeRec's TWO ARMS.  Written with gotos so the give-up call
//     lands between them, as the shipped bytes have it.  The natural
//     if/else spelling,
//
//         if (pBlock->m_pHead0c) { pRec = sub_10039bd0(pBlock, nStage, 1);
//                                  if (!pRec) goto L_GiveUp; ... }
//         else                   { pRec = sub_10039bd0(pBlock, nStage, 0);
//                                  if (!pRec) goto L_GiveUp; }
//         goto L_Bind;
//
//     is big6d910.cpp's and would predict two adjacent 0x10039bd0 rows
//     followed by 0x10025680 -- one shipped-only and one ours-only.
// *********************************************************************** //


// ======================================================================= //
//  0x10040850 (from d3dr3c_os.cpp; its 0x1000eef0 types copied with it).
// ======================================================================= //

struct Elem1000eef0                                     // 0x18 bytes
{
	void*             m_pOwner;                         // +0x00
	void*             m_p04;                            // +0x04  caller sets
	void*             m_p08;                            // +0x08  caller sets
	void*             m_p0c;                            // +0x0c  caller sets
	uint32            m_n10;                            // +0x10
	Elem1000eef0*     m_pNext;                          // +0x14
};

struct Bucket1000eef0                                   // 0x0c bytes
{
	void*             m_pTable;                         // +0x00
	Elem1000eef0*     m_pElements;                      // +0x04
	Bucket1000eef0*   m_pNextInList;                    // +0x08
};

struct Table1000eef0
{
	uint8             m_pad_00[0x1c];                   // +0x00..+0x1b
	Bucket1000eef0*   m_pBuckets[1];                    // +0x1c
};

struct Holder1000eef0
{
	uint8             m_pad_00[0x30];                   // +0x00..+0x2f
	Table1000eef0*    m_pTable;                         // +0x30
};

struct Owner1000eef0
{
	uint8             m_pad_00[0x28];                   // +0x00..+0x27
	Holder1000eef0*   m_pHolder;                        // +0x28
};

// ======================================================================= //
//  0x10040850 -- 110 bytes.  DECODED, every instruction.
//
//  Tear down the whole bucket list 0x1000eef0 builds: walk every bucket,
//  walk its elements, push each element onto the free list at 0x1008d4fc
//  reusing its +0x00 as the link, then push the bucket onto the free list
//  at 0x1008d4e0 reusing ITS +0x00 as the link, and finally empty the
//  list head.
//
//  Element +0x00 is read into %eax and passed to 0x1003e624 BEFORE being
//  overwritten with the free-list link, and %eax is the register this
//  compiler uses for a first argument to its own functions (see the
//  register-argument note in src/engine/d3d/d3dblit.cpp).  Written here as
//  an ordinary call and left to icl.
//
//  `*(uint32*)((uint8*)pBucket->m_pTable + 0x20) = 0` is the one store
//  that is not obviously a link: it clears +0x20 of the TABLE the bucket
//  points back at, once per bucket.
// ======================================================================= //

extern Bucket1000eef0* g_1008d4c0;                      // 0x1008d4c0  live list
extern Elem1000eef0*   g_1008d4fc;                      // 0x1008d4fc  elem free
extern Bucket1000eef0* g_1008d4e0;                      // 0x1008d4e0  bucket free
extern uint32          g_1008b874;                      // 0x1008b874


//  STILL A NEAR, and the residual is the calling convention, not the body.
//  At /Os the body is the shipped 110 bytes but for ONE instruction:
//  shipped hands pElem->m_pOwner to 0x1003e624 in %eax ONLY, where this
//  build also stores it to (%esp).  0x1003e624 is the /Qip REGISTER entry
//  of 0x1003e620 (big3e620.cpp -- `mov 0x4(%esp),%eax` falls into it), and
//  icl only calls a register entry from inside the callee's own TU.  So
//  the shipped caller shares a translation unit with that 8.5 KB body,
//  which here is built /O2 /Oi- in a file of its own.  Not closable from
//  source without modelling that TU.  (At /O2 it is further off: a loop
//  alignment nop the shipped bytes do not have.)
//
//  Modelled.  This function appended to a copy of
//  big3e620.cpp, calling the register entry, under /Os /Oi- /Qip, /O2 /Qip
//  /Qip_no_inlining and /Os /Qip /Qip_no_inlining: the call becomes the
//  register call, but icl then gives THIS function its 14-byte stack
//  realign stub (`push %ebx ; mov %esp,%ebx ; ... and $-16,%esp`) and a
//  second body, which the shipped image does not have (88 of 124 bytes
//  differ).  Left here.  (Side finding for big3e620.cpp: its own body goes
//  from 9572 to 7942 bytes differing at /Os /Oi- /Qip, with the 4-byte
//  stack entry 0x1003e620 identical.)
//  CLOSED.  Moved here, beside 0x1003e620, with this whole
//  TU at /Os /Oi- /Qip /Qsfalign16: the call becomes the register call,
//  and /Qsfalign16 is what removes the 14-byte realign stub measured
//  (above) -- byte-identical, and the 0x1003e620 stack entry with it.
void sub_10040850()
{
	Bucket1000eef0* pBucket;
	Bucket1000eef0* pNextBucket;
	Elem1000eef0*   pElem;
	Elem1000eef0*   pNextElem;

	pBucket = g_1008d4c0;
	while (pBucket)
	{
		pElem       = pBucket->m_pElements;
		pNextBucket = pBucket->m_pNextInList;

		while (pElem)
		{
			pNextElem = pElem->m_pNext;

			g_1008b874 = pElem->m_n10;
			sub_1003e620((Obj3e620*)pElem->m_pOwner);

			pElem->m_pOwner = (void*)g_1008d4fc;
			g_1008d4fc = pElem;

			pElem = pNextElem;
		}

		*(uint32*)((uint8*)pBucket->m_pTable + 0x20) = 0;

		pBucket->m_pTable = (void*)g_1008d4e0;
		g_1008d4e0 = pBucket;

		pBucket = pNextBucket;
	}

	g_1008d4c0 = LTNULL;
}
