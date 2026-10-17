// *********************************************************************** //
//  READ THIS FIRST.  0x100051a0 is now written as the
//  shipped code is built: the 40-byte-vertex clip driver (the twin of
//  d3dr3c_os.cpp's 0x1000f4b0) over the six plane bodies 0x10002310 ..
//  0x10003410 INLINED (w1_10002310.cpp / w44_10002d40.cpp compile them
//  byte-identical out of line), with the RoundF helper on the colour
//  bytes -- see the block above the function.  The extent is 0x100051a0
//  .. 0x10006840 (5792 bytes; the extent table now splits off
//  0x10006840 and 0x10007620, which the enumerator had merged into this
//  row).  Measured: 1509 of 1523 instructions matched, 5808 bytes against
//  5792, the first 58 bytes identical.  The long decode below describes
//  the earlier macro spelling; its readings of the bytes still stand.
//  Measured and NOT taken: `pV = &(*ppVerts)[0]`
//  in the six plane bodies (20 -> 61 differing instructions; each plane
//  alone 35..37); the driver's three loads in all six orders (pVerts or
//  nVerts first: 18, but d3dr3c_os.cpp's identical out-of-line driver
//  needs pOut first) and its locals in all 24 declaration orders (no
//  effect); plane 1's `pOutStart = *ppOut` moved above its classify loop
//  (15: the store of pOutStart lands in the driver, but the shipped
//  FORWARDS pOut's register there and ours still reloads -0xd0, and the
//  move breaks w1_10002310.cpp's identical out-of-line plane).  What is
//  left is that forwarding: the shipped plane 1 reuses the driver's
//  pOut (%eax, stored to -0xd0 and -0xcc at once) and nVerts (%edx),
//  ours reloads both, and the 12 extra bytes push the first classify
//  loop past a 16-byte boundary (5808 against 5792).
// *********************************************************************** //
// *********************************************************************** //
//
//  MODULE:   BIG_051A0 (.CPP)
//
//  PURPOSE:  ONE function -- 0x100051a0 .. 0x1000683a, 5,786 bytes.
//            d3d.ren's SIX-PLANE HOMOGENEOUS POLYGON CLIPPER for the
//            40-byte two-texture-pair vertex, standing on its own: the
//            same clipper that big5a960.cpp, big61a30.cpp, big124a4.cpp,
//            big6d910.cpp and big70d10.cpp each carry INLINE inside a
//            larger draw routine, here compiled as a separate callable
//            with no transform, no fill, no texture bind and no draw.
//
//            ZERO calls in the whole 5,786 bytes.
//
//  ------------------------------------------------------------------- //
//  1. THE EXTENT.  THE STATED ROW IS WRONG; IT SPANS FIVE FUNCTIONS.
//  ------------------------------------------------------------------- //
//
//  The listing row reads `0x100051a0  13647 bytes, end 0x100086ef`.
//  That range holds **five** functions, not one.  Read off the bytes:
//
//    * FIVE `ret`s in the range -- 0x10006704 (`ret $0x4`), 0x10006f22,
//      0x10007d02, 0x100085d4 and 0x100086ec (all `ret $0x8`).
//    * FIVE `push %ebp ; mov %esp,%ebp` prologues -- 0x100051a0,
//      0x10006840, 0x10007620, 0x10008340 and 0x10008680, each preceded
//      by multi-byte lea-nop padding (0x1000683a, 0x1000761a, 0x10008339,
//      0x1000867c).  Each has its own `sub $N,%esp`: 0x714, 0x588, ...
//
//  Call counts per function:
//
//      0x100051a0..0x1000683a   5,786 B   **0 calls (0 direct, 0 indirect)**
//      0x10006840..0x1000761a   3,546 B   27 calls (1 direct, 26 indirect)
//      0x10007620..0x10008335   3,349 B   27 calls (1 direct, 26 indirect)
//      0x10008340..0x1000867b     827 B   13 calls (0 direct, 13 indirect)
//      0x10008680..0x100086ef     111 B    1 call  (0 direct,  1 indirect)
//                                          --------------------------------
//                                          68 calls (2 direct, 66 indirect)
//
//  which is EXACTLY the 68/2 quoted for the whole row.  So all
//  68 calls belong to the OTHER FOUR functions.  **This function has
//  none.**  It is decoded here; the other four are NOT in this file.
//
//  Within 0x100051a0..0x1000683a:
//    * ONE `ret`, at 0x10006704, with 310 bytes of code after it.  Every
//      one of those bytes is /O2 cold-block sinking and every block was
//      proved: 0x10006707 / 0x10006730 / 0x10006760 / 0x10006790 /
//      0x100067c0 / 0x100067f0 are the six clip-epsilon arms (each jumps
//      BACK to 0x10005351 / 0x100056d8 / 0x10005a5a / 0x10005dd8 /
//      0x1000615a / 0x100064d1 or to the `fT = 0.0f` continuation
//      0x1000535b / 0x100056e0 / 0x10005a62 / 0x10005de0 / 0x10006162 /
//      0x100064db), and 0x10006810 / 0x10006817 / 0x1000681e /
//      0x10006825 / 0x1000682c / 0x10006833 are the six `return 0` arms,
//      all `xor %edx,%edx ; jmp 0x100066f4`.  All twelve use THIS frame
//      and all twelve branch back into 0x100051a0..0x10006704.
//    * EVERY branch target in the range is inside the range.  A pass over
//      all 68 distinct jump targets found nothing below 0x100051a0 and
//      nothing above 0x10006835.
//    * 0x1000683a..0x1000683f is lea-nop padding (`lea 0x0(%esi),%esi`);
//      0x10006840 is the next function's prologue.
//
//  NOTHING calls 0x100051a0 directly.  It is reached only through a
//  function pointer at `0x440(%edx)`, called at 0x10006d31 and
//  0x10007b11; 0x10008a44 installs it with
//  `movl $0x100051a0,0x440(%ebx)` (bytes `c7 83 40 04 00 00 a0 51 00 10`
//  at 0x10008ad0).
//
//  ------------------------------------------------------------------- //
//  2. THE SIGNATURE, read off the two indirect call sites.
//  ------------------------------------------------------------------- //
//
//      10006d1b  lea  -0x54c(%ebp),%eax        ; &nVerts
//      10006d21  mov  -0x1c(%ebp),%edx         ; the renderer state
//      10006d24  mov  -0x34(%ebp),%ecx         ; the clip mask
//      10006d27  push %eax                     ; third argument
//      10006d28  mov  0x440(%edx),%eax
//      10006d2e  lea  -0x48(%ebp),%edx         ; &pVerts
//      10006d31  call *%eax
//      10006d33  test %eax,%eax                ; LTBOOL result
//      10006d3b  mov  -0x48(%ebp),%eax         ; the callee wrote it
//      10006d41  flds 0x8(%eax)                ; ...and it is a vertex*
//
//  and identically at 0x10007b11.  So the convention is
//  (%ecx, %edx, one stack dword) with `ret $0x4` -- that is __fastcall,
//  the spelling d3dr2d.cpp, big17c14.cpp, big2ef34.cpp, big0f6f0.cpp and
//  big61a30.cpp all pinned for the %ecx-then-%edx pair.  A
//  register-ABI call is still an ordinary call; it is decoded here as an
//  ordinary function whose first two arguments arrive in registers.
//
//      %ecx        -> nClip      spilled to -0xc0(%ebp) at 0x100051b4,
//                                masked with `and $0x1,%ecx` at
//                                0x100051c9 and re-read at every pass
//                                (`mov -0xc0(%ebp),%eax ; test $B,%al`).
//      %edx        -> ppVerts    spilled to -0x90(%ebp) at 0x100051ba,
//                                dereferenced at 0x10005200 for the input
//                                and written back at 0x100066eb.
//      0x8(%ebp)   -> pnVerts    dereferenced at 0x100051ec and written
//                                back at 0x100066ed.
//      %eax        -> the LTBOOL result, `mov %edx,%eax` at 0x100066f4.
//
//  ------------------------------------------------------------------- //
//  3. THE TWIN.  0x10003c40 IS THE SAME SOURCE, 32-BYTE VERTEX.
//  ------------------------------------------------------------------- //
//
//  0x10003c40 (also stored into `0x440(%ebx)`, in 0x10008a44's OTHER arm)
//  is byte-identical for its first 0x60 bytes -- the same `sub $0x714`
//  frame, the same three spills, the same `-0xc0`/`-0x90` argument slots,
//  the same g_n1008d7a0 early-out and the same four initialising stores
//  out of g_p1008b87c -- and it differs from this function in exactly two
//  systematic ways:
//
//    * VERTEX STRIDE 32, not 40.  `shl $0x5,%edx` x17 and
//      `add $0x20,%eax|%edx|%esi` x6 each, against this function's
//      `lea (%eax,%eax,4),%eax ; lea (%ecx,%eax,8),%eax` and
//      `add $0x28,...` x6 each.
//    * ONE texture pair, not two.  Its census is fmul 48 = 6 x 8
//      (2 position + 2 texture + 4 byte lerps) against this function's
//      60 = 6 x 10; fsubs/fadds 28 against 40; fstps 56 against 68.  The
//      byte rows are IDENTICAL -- movzbl 60, fildl 60, fistpl 30 -- so it
//      has the SAME five byte lerps and the same six planes
//      (`add $0x4,%ecx` x6 for the six flag-array walks).
//
//  So THIS file's body applies to 0x10003c40 as well, with `Vert051a0`
//  cut down to one texture pair (drop m_fU1/m_fV1) and the copy shortened
//  to eight dwords; that function is not written here.
//
//  Neither is reached by a direct call: a scan of every `e8` in
//  0x10001000..0x100a0000 whose displacement resolves finds NOTHING
//  landing on 0x100051a0, 0x10003c40, or on any of the other four
//  prologues in the stated row.
//
//  ------------------------------------------------------------------- //
//  4. THE ACCEPTANCE FIGURE AND WHAT IT DOES *NOT* WITNESS.  READ FIRST.
//  ------------------------------------------------------------------- //
//
//  **THE CALL-SEQUENCE COMPARISON IS COMPLETELY BLIND ON THIS FUNCTION.  CALL-FREE COVERAGE
//  IS 5,786 OF 5,786 BYTES = 100.0%.**  There is not one `call` in the
//  range, direct or indirect.  The expected call-sequence result is
//  `shipped calls 0, ours 0` and it proves NOTHING; do not quote it.
//
//  There are **NO `// UNDECODED:` GAPS** -- every byte of
//  0x100051a0..0x1000683a is covered by a region below.
//
//  What pins the decode instead is the other three instruments,
//  all three of which came out clean.
//
//  ------------------------------------------------------------------- //
//  4a. AN INSTRUCTION CENSUS WITH NO RESIDUE.
//  ------------------------------------------------------------------- //
//
//  Counts are over the whole 5,786 bytes,
//  `awk -F'\t' 'NF>2{print $3}' fn.txt | awk '{print $1}' | sort |
//   uniq -c`.  EVERY row is exact and EVERY mnemonic in the function is
//  in the table -- there is no residue.
//
//   | mnemonic | shipped | what the structure below requires |
//   |----------|---------|-----------------------------------|
//   | fmul     |  60     | 6 passes x 10 uses of fT (2 position + 4 texture + 4 of the 5 byte lerps) |
//   | fmulp    |   6     | the 5th byte lerp of each pass, whose operand order forces the pop form |
//   | fildl    |  60     | 6 x 5 byte-lerps x 2 (the integer difference and the base) |
//   | fistpl   |  30     | 6 x 5 byte-lerps x 1 |
//   | movzbl   |  60     | the same 30 byte-lerps x 2 widenings; NOTHING ELSE in the function widens a byte |
//   | fcomps   |   6     | the six classify tests, one per plane |
//   | fcomp    |  12     | 6 x `fDen < -1e-5` + 6 x `fDen > 1e-5` |
//   | fnstsw/sahf | 18/18| those 6 + 12, one pair each |
//   | fdivp    |   6     | one divide per pass |
//   | idiv/cltd|   6/6   | six x `nIn = (pOut - pOutStart) / 40` |
//   | fsub     |   6     | the six `pWalk->? - pPrev->?` that OPEN each fDen |
//   | fsubs    |  40     | 6 x 6 lerp differences (2 position + 4 texture) + 4 fDen tails (passes 2,3,4,5) |
//   | fadds    |  40     | 6 x 6 lerp bases + the same 4 fDen tails |
//   | faddp    |  32     | 30 byte-lerp adds + 2 numerators (passes 2 and 5) |
//   | fsubrs   |   2     | `g_f1008b908 - pPrev->z` (pass 1) and `g_f1008b910 - pPrev->z` (pass 6) |
//   | fsubrp   |   2     | passes 3 and 4's `pPrev->y - pPrev->z` / `pPrev->x - pPrev->z` (DE E8+i is Intel FSUBP) |
//   | fchs     |   8     | 2 classify negations (2,5) + 4 numerator negations (2,3,4,5) + 2 result negations (2,5) |
//   | fld      |   2     | passes 2 and 5 duplicating fVal before negating it |
//   | fsts     |   2     | pass 3's `y = fVal` and pass 4's `x = fVal` |
//   | fstps    |  68     | 38 destination stores + 30 byte-lerp float temps |
//   | fstp     |  16     | the six sunk epsilon arms' x87 cleanup: 2+3+3+3+3+2 |
//   | fxch     |  14     | 6 (the 5th byte lerp) + 8 (1+2+1+1+2+1 in the numerators/results) |
//   | flds     | 106     | 6 classify + 12 fDen opens + 4 numerators (2,3,4,5) + 36 lerps + 18 constants + 30 byte temps |
//   | lea      |  60     | 40 real + 20 lea-nop pads (see below) |
//   | inc      |  12     | 6 x `g_n1008be44++` + 6 x `i++` |
//   | dec      |   6     | 6 x `j = nIn - 1` |
//   | xor      |  18     | 6 x `nInside = 0` + 6 x `i = 0` + 6 x `return 0` |
//   | test     |  25     | 1 (g_n1008d7a0) + 6 x (nClip & BIT) + 6 x nInside==0 + 6 x nIn<=0 + 6 x abInside[j] |
//   | cmp      |  30     | 6 x (pIn==pEnd guard, nInside!=nIn, pWalk!=pEnd, abInside[j]!=abInside[i], i<nIn) |
//   | je       |  37     | 1 + 6 x 6 |
//   | jne      |   7     | 1 (the `nClip` early-out) + 6 classify back-edges |
//   | jmp      |  13     | 1 early `return 1` + 6 epsilon arms + 6 reject arms |
//   | jae      |   1     | pass 1's classify (`z >= near`) |
//   | jbe      |   7     | 6 in-line epsilon low tests + pass 6's classify (`z <= far`) |
//   | jb       |  10     | 4 classify (passes 2,3,4,5) + 6 sunk-arm high tests |
//   | jl / jle |   6/6   | the six edge loops' `i < nIn` and the six `nIn <= 0` guards |
//   | sub      |  37     | 1 (`sub $0x714,%esp`) + 30 byte-lerp integer differences + 6 `pOut - pOutStart` |
//   | add      |  36     | 6 x (pWalk++ classify, nInside+=nFlag, pFlag++, pOut++ after copy, pOut++ after lerp, pWalk++ edge) |
//   | and      |   1     | `nClip &= 1` under g_n1008d7a0 |
//   | mov      | 580     | decomposed below |
//   | ret/push/pop | 1/1/1 | the single epilogue |
//   | nop      |   2     | alignment |
//
//  60 = 6x10, 60 = 6x5x2, 30 = 6x5 and 40/40/32 are the load-bearing
//  rows: **six clip planes, each with two position LERPs, four texture
//  LERPs and five byte LERPs, and nothing else.**  A seventh plane, a
//  missing plane, a third position LERP, a fifth texture coordinate or a
//  sixth colour byte would each move at least three of those rows, and
//  none of them moves.
//
//  `flds` 106 decomposes with nothing over, by OPERAND:
//      0x8(%edx) x10 = 4 classify (1,2,5,6) + 2 fDen opens (1,6)
//                      + 4 numerator pPrev->z loads (2,3,4,5)
//      0x4(%edx) x3  = 1 classify (3) + 2 fDen opens (3,5)
//      (%edx)    x3  = 1 classify (4) + 2 fDen opens (2,4)
//      0x8(%esi) x6  = 2 fDen opens (1,6) + 4 z-lerps (2,3,4,5)
//      (%esi)    x6  = 4 x-lerps (1,3,5,6) + 2 fDen opens (2,4)
//      0x4(%esi) x6  = 4 y-lerps (1,2,4,6) + 2 fDen opens (3,5)
//      0x18/0x1c/0x20/0x24(%esi) x6 each = 24 texture lerps
//      0x100a412c+8k x6, 0x100a415c+8k x6, 0x100a4160+8k x6 = 18 epsilon/zero
//      -0x700(%ebp)..-0x618(%ebp) x30 = the byte-lerp float temps
//                                                        ---- 106 exactly.
//
//  `lea` 60 = 20 lea-nop pads (`lea 0x0(%esi,%eiz,1),%esi` and friends)
//  plus 40 real, and the 40 decompose:
//      6 x `lea (%eax,%eax,4),%eax` + 6 x `lea (%ecx,%eax,8),%eax`
//                                      = the six `pEnd = pIn + nIn`
//      6 x `lea -0xNNN(%ebp),%edx`     = the six `pFlag = abInside`
//      6 x `lea -0x5(%edx,%edx,4),?`  + 6 x `lea (%ecx,%?,8),%ebx`
//                                      = the six `pPrev = pIn + (nIn-1)`
//      5 x `lea (%eax,%eax,4),%edx`   + 5 x `lea (%esi,%edx,8),%esi`
//                                      = `pOut = pOut + nIn`, passes 1..5
//  -- the SIXTH `pOut = pOut + nIn` is absent because pOut is dead after
//  pass 6 (see the writeback region).
//
//  `mov` 580 decomposes over the same skeleton:
//      240 = 6 passes x 40 -- the ten-dword `*pOut = *pPrev` (20 movs)
//            plus the 20 movs of the surrounding load/spill
//      150 = 30 byte-lerps x 5 (`%eax,-0xe8`, `%edx,-0xe8`,
//            `-0xd0(%ebp),%eax`, `-0xNNN(%ebp),%dl`, `%dl,0xNN(%eax)`)
//      the rest are the per-pass bookkeeping enumerated one line at a
//      time in a `sort | uniq -c` table.  The rows
//      that matter are all exactly 30 or exactly 6:
//        30 x `mov %eax,-0xe8(%ebp)`      the byte differences
//        24 x `mov %edx,-0xe8(%ebp)` + 6 x `mov %ebx,-0xe8(%ebp)` the bases
//        30 x `mov -0xNNN(%ebp),%dl`      the int slots, 5 per pass
//        30 x `mov %dl,0xNN(%eax)`        the byte stores, 5 per pass
//         6 x each of `mov 0xNN(%ebx),%eax` / `mov %eax,0xNN(%edi)` for
//           NN = 0x04..0x24 plus `mov (%ebx),%ecx` / `mov %ecx,(%edi)`
//                                          the six 10-dword copies
//         6 x `mov 0x1008be44,%edx` / `mov %edx,0x1008be44`
//         6 x `mov -0xc0(%ebp),%eax`      the six `nClip & BIT` reloads
//
//  ------------------------------------------------------------------- //
//  4b. FRAME ARITHMETIC THAT DECOMPOSES EXACTLY.
//  ------------------------------------------------------------------- //
//
//  `sub $0x714,%esp` = 1,812 bytes, and it decomposes with NO slack
//  except 8 bytes of alignment and one dead dword:
//
//    -0x714 .. -0x709   12   the three callee-saved spills, written
//                            `mov %ebx,(%esp)` / `%esi,0x4(%esp)` /
//                            `%edi,0x8(%esp)` at 0x100051a9..0x100051b1
//                            and reloaded at 0x100066f6..0x100066fd.
//    -0x708 .. -0x701    8   alignment; nothing addresses it.
//    -0x700 .. -0x615  236   THIRTY 8-byte-aligned float temps, stepping
//                            by 8 from -0x700 to -0x618 -- DUMPED, all
//                            thirty distinct, none repeated.  29 x 8 + 4
//                            = 236 EXACTLY, and -0x614 is the next
//                            object.  Each is used once as
//                            `fstps -0xNNN(%ebp) ; flds -0xNNN(%ebp) ;
//                             fistpl <int slot>` -- 6 passes x 5 byte
//                            lerps.  Same shape as big70d10.cpp's
//                            -0x10f0..-0x1005.
//    -0x614 .. -0x0ed 1320   SIX x 0xdc, the six clip passes' state.  The
//                            flag arrays are -0x614, -0x538, -0x45c,
//                            -0x380, -0x2a4 and -0x1c8, EXACTLY 0xdc
//                            apart, and all six are indexed as
//                            `-0xNNN(%ebp,%edx,4)` / `(%ebp,%edi,4)`.
//                            0xdc = 0xc8 + 0x14 = **50 int32** of flags
//                            plus five int slots.  Each pass's five int
//                            slots were DUMPED and land exactly at
//                            (array + 0xc8):
//                              -0x614 -> -0x54c -0x548 -0x544 -0x540 -0x53c
//                              -0x538 -> -0x470 -0x46c -0x468 -0x464 -0x460
//                              -0x45c -> -0x394 -0x390 -0x38c -0x388 -0x384
//                              -0x380 -> -0x2b8 -0x2b4 -0x2b0 -0x2ac -0x2a8
//                              -0x2a4 -> -0x1dc -0x1d8 -0x1d4 -0x1d0 -0x1cc
//                              -0x1c8 -> -0x100  -0xfc  -0xf8  -0xf4  -0xf0
//                            The bound FIFTY and the count FIVE are both
//                            read off the frame, not guessed; same
//                            finding as big5a960.cpp, big6d910.cpp and
//                            big70d10.cpp.
//    -0x0ec              4   dead; nothing addresses it.
//    -0x0e8              4   the integer scratch every byte lerp stages
//                            its `movzbl ; movzbl ; sub` through.
//    -0x0d4 .. -0x0cc   12   nIn, pOut, pOutStart (in that frame order).
//    -0x0c8, -0x0c4      8   the edge loop's `i` and `pIn`/`pWalk` spills.
//    -0x0c0              4   nClip.
//    -0x098              4   pIn's spill across the classify loop.
//    -0x090              4   the ppVerts argument.
//    -0x008              4   %edi's spill across the 40-byte copy.
//
//  The 30 x 8 temp block landing EXACTLY against the six 0xdc clip
//  blocks, and the six 0xdc blocks' five int slots landing exactly at
//  +0xc8 with the next array at +0xdc, is the strongest single piece of
//  evidence here.  It pins the flag-array bound (50) and the count of
//  byte lerps (five per pass, thirty in all) independently of anything
//  else, and there is no call anywhere for it to lean on.
//
//  ------------------------------------------------------------------- //
//  4c. EXPRESSION-BY-EXPRESSION CORRESPONDENCE WITH A DECODED SIBLING.
//  ------------------------------------------------------------------- //
//
//  The six passes were DIFFED against big70d10.cpp's and big5a960.cpp's
//  (themselves the fifth and sixth independent readings of this source),
//  not re-derived.  Everything agreed, address by address:
//
//    - the six mask bits are **0x01, 0x04, 0x08, 0x10, 0x20, 0x02 IN THAT
//      ORDER** -- 0x10005208 `test $0x1,%al`, 0x10005574 `test $0x4,%al`,
//      0x100058fc `test $0x8,%al`, 0x10005c75 `test $0x10,%al`,
//      0x10005ff6 `test $0x20,%al`, 0x1000637b `test $0x2,%al`;
//    - the six classify tests are
//          z >= g_f1008b908 | -z < x | y < z | x < z | -z < y
//          | z <= g_f1008b910
//      read off 0x10005250 (flds 0x8 ; fcomps 0x1008b908 / jae),
//      0x100055c0 (flds 0x8 ; fchs ; fcomps (%edx) / jb), 0x100059xx
//      (flds 0x4 ; fcomps 0x8 / jb), 0x10005cxx (flds (%edx) ;
//      fcomps 0x8 / jb), 0x10006xxx (flds 0x8 ; fchs ; fcomps 0x4 / jb),
//      0x100063c0 (flds 0x8 ; fcomps 0x1008b910 / jbe);
//    - the six denominators and numerators are big70d10's, term for term.
//      All six were re-read here at 0x10005338, 0x100056b8, 0x10005a38,
//      0x10005db8, 0x10006138 and 0x100064b8, including the two `fsubrs`
//      against the plane globals (0x10005353, 0x100064d3), the two
//      `faddp` numerators (0x100056d8, 0x1000615a) and the two `fsubrp`
//      numerators (0x10005a5a, 0x10005dd8);
//    - the epsilon triple is the same and the constants were DUMPED out
//      of the image: 0x100a412c, 0x100a4134, 0x100a413c, 0x100a4144,
//      0x100a414c and 0x100a4154 are all `ac c5 27 b7` = -9.999999747e-06;
//      0x100a415c, 0x100a4164, 0x100a416c, 0x100a4174, 0x100a417c and
//      0x100a4184 are all `ac c5 27 37` = +9.999999747e-06; 0x100a4160,
//      0x100a4168, 0x100a4170, 0x100a4178, 0x100a4180 and 0x100a4188 are
//      all `00 00 00 00` = 0.0f.  The sunk arms read them in REVERSE pass
//      order (pass 1 uses the highest pair);
//    - **the SIX DEAD 1.0f CONSTANTS big5a960.cpp records are here too**:
//      0x100a4130, 0x100a4138, 0x100a4140, 0x100a4148, 0x100a4150 and
//      0x100a4158 are all `00 00 80 3f` = 1.0f, interleaved with the six
//      -1e-5 epsilons at 0x100a412c + 8k, and referenced from NOWHERE in
//      this function.  That is a second, independent confirmation that
//      this is the same source macro; see NOT PINNED (1) below;
//    - the six `pIn = pOutStart ; nIn = (pOut - pOutStart)/40` tails are
//      all visible as `mov $0x28,%edx ; mov %edx,%ebx ; cltd ; idiv %ebx`
//      at 0x1000555a, 0x100058e2, 0x10005c5b, 0x10005fdc, 0x10006361 and
//      0x100066d4;
//    - the FIVE BYTE LERPS are +0x12, +0x11, +0x10, +0x13, +0x17 in that
//      order in ALL SIX passes -- thirty sites, all read, and the thirty
//      `mov %dl,0xNN(%eax)` stores counted;
//    - the FOUR TEXTURE LERPS are +0x18, +0x1c, +0x20, +0x24 in that
//      order in all six passes -- twenty-four sites, all read.
//
//  This is the SEVENTH independent reading of this one clipper source
//  (d3dr3a.cpp, big5a960.cpp, big61a30.cpp, big124a4.cpp, big6d910.cpp,
//  big70d10.cpp and here); it is reused, not re-derived.
//
//  ------------------------------------------------------------------- //
//  5. FOUR READINGS THAT ARE THIS FUNCTION'S OWN, NOT INHERITED.
//  ------------------------------------------------------------------- //
//
//  (a) THE REJECT IS `return 0;`, NOT `goto L_Reject;`.  There are SIX
//      separate `xor %edx,%edx ; jmp 0x100066f4` blocks (0x10006810,
//      0x10006817, 0x1000681e, 0x10006825, 0x1000682c, 0x10006833), one
//      per pass, each reached by BOTH of its own pass's two reject
//      branches (the empty-input guard and the `nInside == 0` test) and
//      by no other pass's.  A single `goto` to a shared label would emit
//      ONE such block, as big70d10.cpp's 0x10072639 does.  Six blocks,
//      each merging its own pass's two arms and no more, means the
//      statement is textual six times -- i.e. it is inside the macro.
//      See COIN-FLIP (1) for the exact alternative spelling.
//
//  (b) THE RESULT IS ROUTED THROUGH %edx.  Both `return 1` sites
//      (0x100051d4, the g_n1008d7a0 early-out, and 0x100066ef, the normal
//      end) are `mov $0x1,%edx` and all eight exits converge on
//      `mov %edx,%eax` at 0x100066f4 followed by ONE register-restore and
//      ONE `ret $0x4`.  That is the "shared exit through a result
//      variable" shape produced by plain `return` statements plus icl's
//      tail merging, and it is why the epilogue is not duplicated
//      per-exit.
//
//  (c) THE 40-BYTE COPY IS UNROLLED, NOT `rep movsl`.  `*pOut = *pPrev`
//      is ten separate dword load/store pairs in every one of the six
//      passes (0x100052d1..0x10005316, 0x10005651.., 0x100059d1..,
//      0x10005d51.., 0x100060d1.., 0x10006451..) -- counted as six each
//      of `mov 0xNN(%ebx),%eax` / `mov %eax,0xNN(%edi)` for the nine
//      non-zero offsets plus six `mov (%ebx),%ecx` / `mov %ecx,(%edi)`.
//      The rule that struct assignment is always `rep movsl` does
//      not hold at this size in this translation unit -- **exactly the
//      finding big5a960.cpp records for the same statement in the same
//      clipper** (its note 4).  Recorded, not worked around.
//
//  (d) PASS 1 HAS NO `pOutStart = pOut` STORE.  Passes 2..6 each emit
//      `mov %eax,-0xcc(%ebp)` at the head of the edge loop (0x1000560e,
//      0x10005987, 0x10005d07, 0x10006087, 0x10006410); pass 1 does not.
//      That is because 0x100051de loads g_p1008b87c ONCE and stores it to
//      BOTH -0xd0 (pOut) and -0xcc (pOutStart), so pass 1's
//      `pOutStart = pOut` is a copy of a value icl already knows and it
//      is value-numbered away.  The statement IS in the source -- the
//      other five prove it -- so it stays in the macro below.
//
//  ------------------------------------------------------------------- //
//  6. COMPILER VERDICT: Intel C/C++ 4.0, **/O2**.
//  ------------------------------------------------------------------- //
//
//  FOR /O2:
//    * COLD-BLOCK SINKING BELOW THE EPILOGUE, twelve-fold -- the 310-byte
//      tail enumerated under THE EXTENT above.
//    * `&p->m` as `mov $K,%reg ; add %p,%reg`: all six `pOut++` at the
//      end of a lerp are `mov $0x28,%ecx ; add %eax,%ecx` (0x10005518,
//      0x100058a2, 0x10005c1b, 0x10005f9c, 0x1000631f, 0x10006698), never
//      `lea 0x28(%eax),%ecx`.  That is the /O2 pattern.
//    * the six divisors are HOISTED INTO A REGISTER --
//      `mov $0x28,%edx ... mov %edx,%ebx ; cltd ; idiv %ebx` -- rather
//      than divided by an immediate.
//    * `mov $0x1,%edi`/`mov $0x1,%esi` with `mov $0x0,%eax` for the
//      classify flag at every one of the six sites, never the /Os
//      `xor ; inc` pair.
//
//  AGAINST (recorded, not explained away):
//    * ONE epilogue, cross-jumped to by all eight exits.  /O2 usually
//      DUPLICATES.  Same anomaly big70d10.cpp records for 0x10070d10.
//
//  There is no call in the function, so the outgoing-argument test
//  (`add $-N,%esp` vs `push`) has no site to look at here.  The CALLER
//  0x10006d1b pushes, but that is a different translation unit.
//
//  One function in the file, so it wants ONE flag.  Try /O2 first,
//  /Os second.
//
//  ------------------------------------------------------------------- //
//  7. WHAT IS *NOT* PINNED -- read before trusting any byte.
//  ------------------------------------------------------------------- //
//
//  (1) SIX DEAD 1.0f CONSTANTS (0x100a4130 + 8k).  Each clip pass's
//      source mentions a 1.0f that icl folded away and the spelling below
//      does not.  big5a960.cpp hit the same six and said the same;
//      unresolved there and unresolved here.  The most likely shape is a
//      `fT` clamp or a `1.0f - fT` complement that the /O2 value numberer
//      removed, but nothing in the bytes distinguishes the candidates.
//
//  (2) THE THIRTY `(uint8)` CASTS.  `fstps <temp> ; flds <temp> ;
//      fistpl <int slot>` with NO fnstcw/fldcw dance and no helper call
//      at any of the thirty sites is /QIfist.  Under plain /O2 a
//      `(uint8)(int32)f` cast emits the control-word dance instead and
//      those thirty sites will differ in bytes.  They contain no calls,
//      so nothing witnesses them but the census rows above.
//
//  (3) THE DECLARATION ORDER of nIn / pOut / pOutStart.  The frame order
//      is nIn (-0xd4) < pOut (-0xd0) < pOutStart (-0xcc) < nClip (-0xc0);
//      the four initialising STORES are emitted pOut, pOutStart, nIn,
//      pIn (0x100051e6, 0x100051ee, 0x100051fa, 0x10005200) and the
//      source below is written in that statement order.  Whether the
//      declaration order that produces that frame order is the one below
//      is not proved.
//
//  ------------------------------------------------------------------- //
//  8. COIN-FLIPS -- both spellings given, one build settles each.
//  ------------------------------------------------------------------- //
//
//  (1) THE REJECT.  Written below as a textual `return LTFALSE;` inside
//      the macro, which is what six separate `xor %edx,%edx` blocks say.
//      THE ALTERNATIVE, if that comes out with one block instead of six:
//          #define CLIP051A0_HEAD(...)  ... if (nInside == 0) goto L_Rej;
//      with, just above the epilogue,
//          L_Rej:  return LTFALSE;
//      (that is big70d10.cpp's spelling exactly).
//
//  (2) THE CLASSIFY FLAG.  Written below as
//          nFlag = (INSIDE) ? 1 : 0;  nInside += nFlag;  *pFlag = nFlag;
//      which is what `mov $0x1,%esi ; mov $0x0,%eax ; jcc ; mov %eax,%esi
//      ; add %esi,%ebx ; mov %esi,(%ecx)` says.  THE ALTERNATIVE, the
//      one-liner big5a960.cpp uses:
//          nInside += (abInside[i] = (INSIDE) ? 1 : 0);
//      -- but that one indexes the array where this function walks a
//      pointer (`add $0x4,%ecx` at 0x10005271), so the pointer form is
//      the one written.
//
//  (3) THE EARLY OUT.  Written below as
//          if (g_n1008d7a0) { nClip &= 1; if (!nClip) return LTTRUE; }
//      Two `mov %ecx,-0xc0(%ebp)` stores (0x100051b4 and 0x100051cc) say
//      nClip is assigned twice; the alternative is that the parameter is
//      used directly and only the masked value is stored, i.e.
//          uint32 nClip = nClipIn;  if (g_n1008d7a0) { ... }
//      versus the parameter being named `nClip` outright.  The version
//      below names the parameter `nClip` and lets icl spill it, which is
//      what the first store is.
//
//  ------------------------------------------------------------------- //
//  9. REGION MAP.  Every byte of 0x100051a0..0x1000683a is here.
//  ------------------------------------------------------------------- //
//
//   0x100051a0..0x100051dd  prologue, the three register spills, the
//                           g_n1008d7a0 / `nClip &= 1` early `return 1`.
//   0x100051de..0x1000520f  pOut / pOutStart / nIn / pIn initialisation.
//   0x10005210..0x1000556d  PASS 1, bit 0x01, NEAR   (z >= g_f1008b908).
//   0x1000556e..0x100058f5  PASS 2, bit 0x04, LEFT   (-z < x).
//   0x100058f6..0x10005c6e  PASS 3, bit 0x08, TOP    (y < z).
//   0x10005c6f..0x10005fef  PASS 4, bit 0x10, RIGHT  (x < z).
//   0x10005ff0..0x10006374  PASS 5, bit 0x20, BOTTOM (-z < y).
//   0x10006375..0x100066bd  PASS 6, bit 0x02, FAR    (z <= g_f1008b910).
//   0x100066be..0x100066ee  the writeback: *ppVerts = pIn, *pnVerts = nIn.
//   0x100066ef..0x10006706  `return 1` and the single epilogue.
//   0x10006707..0x1000680f  the six sunk clip-epsilon arms (`fT = 0.0f`),
//                           in reverse pass order.
//   0x10006810..0x10006839  the six sunk `return 0` arms, in reverse pass
//                           order (pass 6 lowest at 0x10006810, pass 1
//                           highest at 0x10006833).
//   0x1000683a..0x1000683f  lea-nop padding to the next function.
//
//  (The pass boundaries are the `mov -0xc0(%ebp),%eax` that reloads
//  nClip; it is both the previous pass's `nInside == nIn` join and the
//  next pass's `if (nClip & BIT)` test, so each boundary address is
//  reached from both.)
//
// *********************************************************************** //

// Only ltbasedefs.h, exactly as d3dr4b.cpp, big5a960.cpp and
// big70d10.cpp have it.
#include "ltbasedefs.h"

// Round to nearest in the current FPU mode: w1_10001000.cpp's helper
// (0x10015190 out of line), the clipper family's colour-byte conversion.
// Inlined, it is the shipped `fstps t ; flds t ; fistpl n ; mov n,%dl`.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}


// ======================================================================= //
//  DECLARATIONS
// ======================================================================= //

// ---------------------------------------------------------------------- //
//  THE VERTEX.  Stride 0x28 = 40, from `add $0x28,%edx` in every classify
//  loop, `add $0x28,%esi` in every edge loop, `mov $0x28,%ebx ; idiv` in
//  every tail and the `lea (%eax,%eax,4) ; lea (%ecx,%eax,8)` (x40) that
//  computes pEnd and pPrev.  The fields are exactly big70d10.cpp's
//  Vert70d10 / big5a960.cpp's RVert: +0x0c is never touched by name here
//  but IS copied by the ten-dword `*pOut = *pPrev`
//  (`mov 0xc(%ebx),%eax ; mov %eax,0xc(%edi)`, six sites), so it must be
//  declared for the copy to be 40 bytes.
// ---------------------------------------------------------------------- //
struct Vert051a0                                        // 0x28 bytes
{
	LTVector v;                                         // +0x00  x, y, z
	float	m_0c;                                       // +0x0c
	uint8	m_10;                                       // +0x10
	uint8	m_11;                                       // +0x11
	uint8	m_12;                                       // +0x12
	uint8	m_13;                                       // +0x13
	uint8	m_14;                                       // +0x14
	uint8	m_15;                                       // +0x15
	uint8	m_16;                                       // +0x16
	uint8	m_17;                                       // +0x17
	float	m_18;                                       // +0x18
	float	m_1c;                                       // +0x1c
	float	m_20;                                       // +0x20
	float	m_24;                                       // +0x24
};


// ---------------------------------------------------------------------- //
//  Externals.  Relocations are masked, so only the shapes matter; each
//  one is named by the address the shipped code loads it from, and each
//  is already named the same way by big70d10.cpp / big5a960.cpp.
// ---------------------------------------------------------------------- //

// 0x1008d7a0 -- UseD3DClip.  `mov 0x1008d7a0,%eax ; test %eax,%eax` at
// 0x100051c0, the only site in this function.  big0f6f0.cpp,
// big124a4.cpp and big24090.cpp all name it uint32.
extern uint32			g_n1008d7a0;

// 0x1008b87c -- the clipper's output bank.  Read ONCE at 0x100051de and
// stored to BOTH pOut and pOutStart; see note 5(d).
extern Vert051a0*		g_p1008b87c;

// 0x1008b908 / 0x1008b910 -- the near and far plane constants.  Each is
// used three ways: as the classify comparand (`fcomps 0x1008b908` at
// 0x10005253, `fcomps 0x1008b910` at 0x100063c3), as the numerator
// (`fsubrs` at 0x10005353 and 0x100064d3) and as an INTEGER MOVE into
// pOut->m_vPos.z (`mov 0x1008b908,%edx ; mov %edx,0x8(%ecx)` at
// 0x10005375, and the same pair at 0x100064f5), which is icl copying a
// float lvalue with `mov`s.
extern float			g_f1008b908;
extern float			g_f1008b910;

// 0x1008be44 -- incremented once per EXECUTED clip pass:
// `mov 0x1008be44,%edx ; inc %edx ; mov %edx,0x1008be44`, six sites,
// each inside its pass's `if (nClip & BIT)` and before the classify loop.
extern uint32			g_n1008be44;

#define CLIP_EPSILON    0.00001f
#define CLIP_MAX_VERTS  50

static inline float ClipEdgeNearA(LTVector* a, LTVector* b, LTVector* dst)
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


static inline float ClipEdgeLeftA(LTVector* a, LTVector* b, LTVector* dst)
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


static inline float ClipEdgeTopA(LTVector* a, LTVector* b, LTVector* dst)
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


static inline float ClipEdgeFarA(LTVector* a, LTVector* b, LTVector* dst)
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


static inline float ClipEdgeRightA(LTVector* a, LTVector* b, LTVector* dst)
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


static inline float ClipEdgeBottomA(LTVector* a, LTVector* b, LTVector* dst)
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


static inline void LerpVert051a0(Vert051a0* a, Vert051a0* b, Vert051a0* dst, float t)
{
	dst->m_18 = (b->m_18 - a->m_18) * t + a->m_18;
	dst->m_1c = (b->m_1c - a->m_1c) * t + a->m_1c;
	dst->m_20 = (b->m_20 - a->m_20) * t + a->m_20;
	dst->m_24 = (b->m_24 - a->m_24) * t + a->m_24;
	dst->m_12 = (uint8)RoundF((b->m_12 - a->m_12) * t + a->m_12);
	dst->m_11 = (uint8)RoundF((b->m_11 - a->m_11) * t + a->m_11);
	dst->m_10 = (uint8)RoundF((b->m_10 - a->m_10) * t + a->m_10);
	dst->m_13 = (uint8)RoundF((b->m_13 - a->m_13) * t + a->m_13);
	dst->m_17 = (uint8)RoundF((b->m_17 - a->m_17) * t + a->m_17);
}


static inline LTBOOL sub_10002310(uint32* pUnused, Vert051a0** ppVerts, int32* pnVerts, Vert051a0** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert051a0*  pOutStart;
	Vert051a0*  pV;
	Vert051a0*  pEnd;
	Vert051a0*  pPrev;
	Vert051a0*  pCur;
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
		uint32 bIn = (pV->v.z >= g_f1008b908);

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
				float t = ClipEdgeNearA(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpVert051a0(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_10002670(uint32* pUnused, Vert051a0** ppVerts, int32* pnVerts, Vert051a0** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert051a0*  pOutStart;
	Vert051a0*  pV;
	Vert051a0*  pEnd;
	Vert051a0*  pPrev;
	Vert051a0*  pCur;
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
		uint32 bIn = (-pV->v.z < pV->v.x);

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
				float t = ClipEdgeLeftA(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpVert051a0(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_100029e0(uint32* pUnused, Vert051a0** ppVerts, int32* pnVerts, Vert051a0** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert051a0*  pOutStart;
	Vert051a0*  pV;
	Vert051a0*  pEnd;
	Vert051a0*  pPrev;
	Vert051a0*  pCur;
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
		uint32 bIn = (pV->v.y < pV->v.z);

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
				float t = ClipEdgeTopA(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpVert051a0(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_10002d40(uint32* pUnused, Vert051a0** ppVerts, int32* pnVerts, Vert051a0** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert051a0*  pOutStart;
	Vert051a0*  pV;
	Vert051a0*  pEnd;
	Vert051a0*  pPrev;
	Vert051a0*  pCur;
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
		uint32 bIn = (pV->v.x < pV->v.z);

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
				float t = ClipEdgeRightA(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpVert051a0(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_100030a0(uint32* pUnused, Vert051a0** ppVerts, int32* pnVerts, Vert051a0** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert051a0*  pOutStart;
	Vert051a0*  pV;
	Vert051a0*  pEnd;
	Vert051a0*  pPrev;
	Vert051a0*  pCur;
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
		uint32 bIn = (-pV->v.z < pV->v.y);

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
				float t = ClipEdgeBottomA(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpVert051a0(pPrev, pCur, *ppOut, t);

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


static inline LTBOOL sub_10003410(uint32* pUnused, Vert051a0** ppVerts, int32* pnVerts, Vert051a0** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	Vert051a0*  pOutStart;
	Vert051a0*  pV;
	Vert051a0*  pEnd;
	Vert051a0*  pPrev;
	Vert051a0*  pCur;
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
		uint32 bIn = (pV->v.z <= g_f1008b910);

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
				float t = ClipEdgeFarA(&pPrev->v, &pCur->v, &(*ppOut)->v);

				LerpVert051a0(pPrev, pCur, *ppOut, t);

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


// ======================================================================= //
//  THE CLIPPER, as the shipped code is built: 0x100051a0 is
//  the 40-byte-vertex twin of d3dr3c_os.cpp's 0x1000f4b0 -- the same
//  UseD3DClip test, the same output bank, the same six planes in the same
//  order -- with the six plane bodies 0x10002310 .. 0x10003410 (byte-
//  identical out of line in w1_10002310.cpp / w44_10002d40.cpp) INLINED.
//  The plane bodies take &pVerts / &nVerts / &pOut, so those three keep
//  frame slots and are stored and reloaded around every plane -- which is
//  the shipped `mov -0xd0(%ebp),%eax ; ... ; mov %eax,-0xd0(%ebp)` round
//  trip of pOut in every vertex copy, and the RoundF temporaries give the
//  thirty `fstps t ; flds t ; fistpl n` colour conversions.  The earlier
//  macro-expanded passes (the plane bodies written out in place) are in
//  git history; they kept pOut in a register.
//
//  The driver is itself an INLINE body under the exported function: the
//  shipped code has ONE epilogue (`mov %edx,%eax` at 0x100066f4) that
//  every exit reaches with the answer in %edx -- `mov $0x1,%edx ; jmp`
//  for the UseD3DClip early-out, `xor %edx,%edx ; jmp` for each plane's
//  reject -- which is how icl lowers an inlined callee's several
//  `return`s.  Written with the driver's returns in the exported function
//  itself, /O2 gives seven epilogues (0.966 matched); with a result
//  variable and gotos, the variable goes to the frame (0.980); as an
//  inline driver: 1509 of 1523 instructions matched, same count (0.991).
// ======================================================================= //
static inline LTBOOL ClipPoly051a0(uint32 nClipFlags, Vert051a0** ppVerts,
                                     int32* pnVerts)
{
	Vert051a0*  pOut;
	Vert051a0*  pOutStart;
	Vert051a0*  pVerts;
	int32       nVerts;
	uint32      anUnused[6];

	if (g_n1008d7a0)
	{
		nClipFlags &= 1;
		if (!nClipFlags)
			return LTTRUE;
	}

	// pOutStart is never read; see big3c40.cpp.
	pOut   = pOutStart = g_p1008b87c;
	pVerts = *ppVerts;
	nVerts = *pnVerts;

	if (nClipFlags & 0x01)
	{
		if (!sub_10002310(&anUnused[0], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x04)
	{
		if (!sub_10002670(&anUnused[1], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x08)
	{
		if (!sub_100029e0(&anUnused[2], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x10)
	{
		if (!sub_10002d40(&anUnused[3], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (nClipFlags & 0x20)
	{
		if (!sub_100030a0(&anUnused[4], &pVerts, &nVerts, &pOut))
			return LTFALSE;
	}

	if (!(nClipFlags & 0x02) ||
	    sub_10003410(&anUnused[5], &pVerts, &nVerts, &pOut))
	{
		*ppVerts = pVerts;
		*pnVerts = nVerts;
		return LTTRUE;
	}

	return LTFALSE;
}


LTBOOL __fastcall sub_100051a0(uint32 nClipFlags, Vert051a0** ppVerts,
                               int32* pnVerts)
{
	return ClipPoly051a0(nClipFlags, ppVerts, pnVerts);

}
