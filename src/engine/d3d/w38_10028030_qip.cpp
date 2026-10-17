// *********************************************************************** //
//
//  MODULE:   W38_10028030_QIP (.CPP)  (was big28030.cpp)
//
//  Built /O2 /Qip, named after its stack entry 0x10028030, with
//  0x10025680 (from d3dmid.cpp) beside it: 0x10025680 calls the /Qip
//  register entry 0x1002803c, which only forms in this TU.  Measured
//  (the probe, repeated): 0x10025680 and the 0x10028030 stack entry
//  byte-identical; the body 0x1002803c stays near.
//
//  PURPOSE:  One function.  The enumeration row is
//                0x1002803c, 6,232 bytes
//            and BOTH of its numbers are wrong.  See EXTENT below.
//
//            CORRECTED START:  0x10028030   (the /Qip STACK entry)
//            CORRECTED END:    0x10029890   (exclusive)
//            CORRECTED SIZE:   6,240 bytes  (0x1860)
//
//            The row carried "0x10028030..0x10029890,
//            6,236 bytes", inherited from big299a8.cpp's diff.  The two
//            halves of that are inconsistent: 0x10029890 - 0x10028030 is
//            0x1860 = 6,240, and 6,236 ends at 0x1002988c, which is four
//            bytes INSIDE the function's last instruction.  Refuted below
//            with the bytes.
//
//            What it is: d3d.ren's WORLD-POLYGON CLIP-AND-DRAW.  Given a
//            poly-ish object, an in/out vertex-array pointer, an in/out
//            vertex count and a 4x4, it fetches the object's source
//            vertices, transforms them by that 4x4 with a perspective
//            divide into a 40-vertex scratch array, stamps one flat
//            diffuse colour and a white specular, then either (a) if the
//            clip mask is empty, applies the camera's own projective 4x4
//            (0x1008b880 + 0x1dc) in line and draws, or (b) runs the
//            standard six-plane frustum clipper, projects the survivors
//            through 0x10014580 and draws.  One DrawPrimitive triangle
//            fan.  Returns 1 if anything was drawn, 0 if the polygon was
//            clipped away entirely.
//
//  Compiler: Intel C/C++ 4.0, /O2.  The /O2 tells:
//              * every outgoing argument block is `add $-N,%esp` followed
//                by `mov %reg,K(%esp)` -- not one `push` in 6.2 KB
//                (0x10028079, 0x10028089, 0x100282a0, 0x10029710,
//                 0x10029734);
//              * ONE epilogue (0x10029781..0x1002978f), with the two
//                return values merged into `%edx` and one `mov %edx,%eax`
//                at 0x1002977f;
//              * the six clip-epsilon `else fT = 0.0f;` arms are all sunk
//                below the epilogue, to 0x10029790..0x1002988f, and every
//                one of them jumps BACKWARDS into the body.
//            Register-passing convention (/Qip): arg0 in %eax, arg1 in
//            %edx, arg2 in %ecx, arg3 on the stack at 0x14(%ebp).  That
//            is decoded as an ordinary call whose arguments
//            arrive in registers, which is what is done here.
//
// ----------------------------------------------------------------------- //
//  EXTENT -- THE ROW IS WRONG AT BOTH ENDS
// ----------------------------------------------------------------------- //
//
//      An extent check of 0x1002803c, 6232 bytes:
//      -> "!! THIS ROW STARTS AT A REGISTER ENTRY.  The stack entry, and
//          the real function, starts at 0x10028030 (12 bytes earlier,
//          3 argument load(s)).  Re-address the row and add 12 to its
//          size."
//      -> ret 1 (0x1002978f), push %ebp frames 1 (0x1002803c),
//         branch targets 0 outside the range, last instruction ends
//         exactly at the stated end.
//
//  (a) THE START IS TWELVE BYTES LATE, AND THE TWELVE BYTES ARE THREE
//      ARGUMENT LOADS -- so the function takes THREE register arguments,
//      not two, before a single byte of it is read:
//
//        10028029  ret $0xc              <- last insn of the PREVIOUS fn
//        1002802c  lea 0x0(%esi,%eiz,1),%esi   <- 4 bytes of padding
//        10028030  mov 0x4(%esp),%eax    <- /Qip STACK entry, arg0
//        10028034  mov 0x8(%esp),%edx    <- arg1
//        10028038  mov 0xc(%esp),%ecx    <- arg2
//        1002803c  push %ebp             <- /Qip REGISTER entry (the row)
//        1002803d  mov %esp,%ebp
//        1002803f  sub $0xc30,%esp
//
//      A FOURTH argument stays on the stack and is read at 0x100280c6 as
//      `mov 0x14(%ebp),%edx`: after `push %ebp` at the register entry,
//      ebp+8/+0xc/+0x10 are the three slots the stack entry read and
//      ebp+0x14 is the fourth.  All three callers write it (see CALL
//      SITES below), so the signature is four arguments and the twelve
//      bytes told us so.
//
//      NOTHING in .text references 0x10028030.  All three callers enter
//      at 0x1002803c, the register entry, which is the `/Qip`
//      two-entry residual (shipped calls f+0xc where we call f).
//
//  (b) THE END: 6,240, NOT 6,236, AND NO TRAILING PADDING.
//
//        1002988b  jmp 0x100294ef        <- plane 1's sunk epsilon arm,
//                                           the LAST instruction, 5 bytes
//        10029890  mov 0x4(%esp),%eax    <- the NEXT function's /Qip
//        10029894  push %ebp                stack entry (one argument
//                                           load; big26728.cpp calls it
//                                           sub_10029894(Sink26728*))
//
//      An extent check on 0x10028030 with 6,236 reports "last instruction
//      0x1002988b ends at 0x10029890, PAST the stated end -- the row ends
//      mid-instruction".  With 6,240 every check passes:
//
//        ret 1            0x1002978f
//        push %ebp        1            0x1002803c
//        /Qip             start is the STACK entry
//        branch targets   0 outside the range or mid-instruction
//        last instruction ends exactly at the stated end
//
//      ONE `ret` and ONE prologue.  The size is four
//      bytes larger than the row said, and there is no alignment
//      padding at all between this function's last instruction and the
//      next function's first: 0x1002988b + 5 = 0x10029890 exactly.
//      The row's own arithmetic also lands here: 6,232 + 12 = 6,244 =
//      6,240 + the 4 bytes of lea-nop padding at 0x1002802c that the
//      row's end had swallowed from the wrong side.
//
// ----------------------------------------------------------------------- //
//  THE ACCEPTANCE TEST, AND HOW LITTLE OF THE FUNCTION IT SEES
// ----------------------------------------------------------------------- //
//
//      The call count over 0x10028030..0x10029890:
//      6,240 bytes, 5 calls (4 direct, 1 indirect), 1248 bytes per call
//
//  FOUR distinct direct callees, and NO _chkstk -- the frame is 0xc30 =
//  3,120 bytes, under the probe threshold, so the benign
//  large-frame residual DOES NOT APPLY to this function:
//      0x100232b0   0x1000a700   0x1000d9b0   0x10014580
//
//  EXPECTED: 5 shipped / 5 ours, 5 in agreement (4 by resolved target,
//  1 by position only -- the DrawPrimitive through *0x64(%eax)), 0
//  shipped-only, 0 ours-only.  Do NOT add /Ge; there is no _chkstk to
//  close.
//
//  IT DOES NOT NEED /Oi-, AND /Oi- WOULD BE HARMFUL -- the same
//  judgement big299a8.cpp made, for the same reason measured in these
//  bytes.  There is no sqrt/fabs/tan/atan shape anywhere in the function
//  (the census below has no `fsqrt` and no double-argument `push %ecx ;
//  push %ecx ; fstpl (%esp)` call), so /Oi- has nothing to win; and the
//  six `*pOut = *pPrev` 32-byte struct copies ship OPEN-CODED as eight
//  dword `mov` pairs (0x100283db..0x1002841c and five siblings at
//  0x1002872b, 0x10028a7b, 0x10028dcb, 0x1002911b, 0x1002946b), which
//  turning intrinsic expansion off risks turning into six ours-only
//  `memcpy` rows.
//
//  THE CALL SITES ARE ALL IN THE FIRST 640 AND THE LAST 380 BYTES:
//      0x10028084  0x100232b0      0x100280a3  0x1000a700
//      0x100282aa  0x1000d9b0      0x1002971a  0x10014580
//      0x10029771  *0x64(%eax)
//
//  **0x100282af .. 0x10029719 -- 5,226 bytes, 83.8% of the function --
//  contains not one call of any kind**, and 0x10029776..0x1002988f (the
//  epilogue plus the six sunk epsilon arms, 282 bytes, 4.5%) is a second
//  call-free stretch after the last call.  5,508 bytes, 88.3%, hold no
//  call at all.  The two call-bearing clusters --
//  0x10028030..0x100282ae (639 bytes) and 0x1002971a..0x10029775 (92
//  bytes) -- are 731 bytes, so **the call sequence witnesses 11.7% of the
//  function.**  Of the blind part, 0x100282dc..0x100296b5 (5,082 bytes,
//  81.4%) is the six inlined clip planes.  Everything that pins that
//  region is below, and none of it is the call sequence.
//
// ----------------------------------------------------------------------- //
//  INSTRUMENT 1 -- THE FRAME DECOMPOSES EXACTLY, SLOTS DUMPED NOT ASSUMED
// ----------------------------------------------------------------------- //
//  `sub $0xc30,%esp` at 0x1002803f is 3,120 bytes and every byte of it is
//  named.  The offsets below are the ones that actually appear in the
//  disassembly (`grep -oE '\-0x[0-9a-f]+\(%ebp\)'`, EIGHTY distinct, all
//  accounted for):
//
//   ebp-0xc30 .. -0xc25     12   %ebx/%esi/%edi spill.  Stored at
//                                (%esp)/0x4(%esp)/0x8(%esp) at
//                                0x10028045..0x1002804d, i.e. at the very
//                                bottom of the frame.
//   ebp-0xc24 .. -0xc09     28   alignment slack, never referenced
//   ebp-0xc08 .. -0xb1d    236   THIRTY float temps, 8 apart:
//                                -0xc08,-0xc00,...,-0xb20.  Six planes x
//                                { fRed fGreen fBlue fAlpha fSpec }.
//                                Each is `fstps` then `flds` then
//                                `fistpl`, 4 live bytes in an 8-byte
//                                slot -- 29*8 + 4 = 236.  The census
//                                below has fstps 30 and flds 30 on
//                                exactly these thirty addresses, no more,
//                                no fewer.
//   ebp-0xb1c                4   pData (out-param of 0x1000a700;
//                                `lea -0xb1c(%ebp),%edx` at 0x10028092)
//   ebp-0xb18 .. -0x619   1280   RVert aVerts[40] -- 40 * 0x20 EXACTLY.
//                                0xb18 - 0x618 = 0x500 = 1,280, and
//                                1280/32 = 40.  -0xb18 is `lea`d twice
//                                (0x100280b3 the transform's %ebx,
//                                0x1002818a the `*ppVerts = aVerts`).
//                                THIS IS WHERE THE 40 COMES FROM, off
//                                this function's own `sub`, exactly as
//                                the twin's 0x1000/0x20 gives it 128.
//   ebp-0x618 .. -0x0f1   1320   SIX clip blocks of 0xdc:
//                                0xc8 = int32 abInside[50], then
//                                0x14 = five int slots for the five
//                                (uint8)float conversions.
//                                The six flag-array bases are
//                                -0x618, -0x53c, -0x460, -0x384,
//                                -0x2a8, -0x1cc -- 0xdc apart, each
//                                `lea`d exactly once (0x10028338,
//                                0x10028682, 0x100289da, 0x10028d23,
//                                0x10029074, 0x100293c7) and each the
//                                base of exactly two
//                                `mov -0xXXX(%ebp,%reg,4)` indexings.
//                                The six five-int groups start at
//                                -0x550, -0x474, -0x398, -0x2bc,
//                                -0x1e0, -0x104 -- each exactly 0xc8
//                                into its block.  -0x618 + 6*0xdc
//                                = -0xf0.
//   ebp-0x0f0                4   THE shared int every `fildl` reads.
//                                120 references, and all 60 movzbl->
//                                fildl pairs use this one slot.
//   ebp-0x0ec .. -0x0dd     16   slack, never referenced
//   ebp-0x0dc .. -0x0c0     32   the eight loop scalars:
//                                 -0xdc  nColor, then nIn  (28 refs;
//                                        two non-overlapping locals in
//                                        one slot -- 3 refs before the
//                                        transform loop are the colour,
//                                        the 25 from 0x10028309 on are
//                                        nIn)
//                                 -0xd8  pOut       (71 refs)
//                                 -0xd4  pOutStart  (12 refs = 6 x 2)
//                                 -0xd0  pIn        (21 refs)
//                                 -0xcc  a WRITE-ONLY spill of pPrev,
//                                        six stores and no read
//                                 -0xc8  the six edge loops' `i` home
//                                        (9 refs)
//                                 -0xc4  icl's spare spill slot
//                                        (15 refs, 12 of them stores)
//                                 -0xc0  nClipFlags ( 8 refs = 2 stores
//                                        + ONE READ PER PLANE GATE, which
//                                        is a sixth independent count of
//                                        the six planes)
//   ebp-0x0bc .. -0x091     44   slack, never referenced
//   ebp-0x090                4   arg1 spill, ppVerts    (6 refs)
//   ebp-0x08c                4   arg2 spill, pnVerts   (10 refs)
//   ebp-0x088 .. -0x009    128   slack, never referenced
//   ebp-0x008                4   the six edge loops' `mov %edi,-0x8(%ebp)`
//                                / `mov -0x8(%ebp),%edi` around the
//                                32-byte copy -- 12 refs = 6 x 2, the
//                                same rehost of `i` the twin found in ITS
//                                -0x8
//   ebp-0x004                4   never referenced
//
//     12 + 28 + 236 + 4 + 1280 + 1320 + 4 + 16 + 32 + 44 + 8 + 128 + 8
//        = 3,120 = 0xc30
//
//  TWO ARRAY BOUNDS ARE READ OFF THE FRAME, not assumed: aVerts[40] from
//  0x500/0x20, and abInside[50] from 0xc8/4.  The 40 is the ONE measured
//  difference expected, and it is confirmed here from this
//  function's own frame with no reference to the twin.
//
// ----------------------------------------------------------------------- //
//  INSTRUMENT 2 -- AN INSTRUCTION CENSUS WITH NO RESIDUE
// ----------------------------------------------------------------------- //
//  1,630 instructions in 0x10028030..0x1002988f (the extent check's count, and
//  objdump's once its three continuation lines are discounted).  Every
//  mnemonic in the function is in this table and every row falls out of
//  the structure claimed below.  The rows over five are decomposed BY
//  OPERAND.  Where a row differs from big299a8.cpp's the reason is given,
//  and every one of those reasons is a structural difference this file
//  reports.
//
//   | mnemonic | n   | what the claimed structure requires |
//   |----------|-----|-------------------------------------|
//   | mov      | 623 | bulk (620 `mov` + 3 `movl` immediate-to-memory) |
//   | flds     | 118 | decomposed below -- the load-bearing row |
//   | add      |  66 | decomposed below |
//   | fstps    |  63 | 30 byte-lerp temps + 12 texture (6x2) + 14 position + 7 transform (3 + 4) |
//   | movzbl   |  60 | 6 planes x 5 byte lerps x 2 widenings.  NOTHING else widens a byte |
//   | fildl    |  60 | the same 30 byte lerps x 2 (difference, base) |
//   | fmul     |  53 | 6 x 8 uses of fT + 2 (transform 1, x and y) + 3 (transform 2, x/y/z) |
//   | faddp    |  48 | 30 byte-lerp adds + 2 numerators (planes 2,5) + 8 (transform 1: 4 rows x 2) + 8 (transform 2: 4 rows x 2) |
//   | je       |  42 | 6 planes x 6 (gate, pIn==pEnd, nInside==0, nInside!=nIn, abInside[j], abInside[j]!=abInside[i]) + 6 (the ShowSplits else, transform 2's guard, the 0x1000d9b0 guard, UseD3DClip's TWO, the 0x10014580 guard) |
//   | sub      |  37 | 30 byte-lerp integer differences (24 `%edx,%eax` + 6 `%ebx,%eax`) + 6 pointer differences (1 `%edx,%eax` + 5 `%edx,%ecx`) + the prologue |
//   | fadds    |  36 | 24 = 6 x 4 lerp bases + 4 fDen tails (planes 2,3,4,5) + 4 transform-1 row translations + 4 transform-2 row translations |
//   | test     |  32 | 24 = 6 x (bit mask, nInside==0, nIn<=0, abInside[j]) + EIGHT scalar: ShowSplits, transform 1's guard AND back edge, g_1008b874, transform 2's guard, the 0x1000d9b0 guard, UseD3DClip, the 0x10014580 guard |
//   | lea      |  30 | 20 lea-nop pads + 10 real -- decomposed below |
//   | fistpl   |  30 | the same 30 byte lerps x 1 |
//   | cmp      |  30 | 6 x 5 (pIn==pEnd, nInside!=nIn, pWalk!=pEnd, abInside[j]!=abInside[i], i<nIn) |
//   | fsubs    |  28 | 24 = 6 x 4 lerp differences + 4 fDen tails |
//   | fmuls    |  24 | 12 = transform 1's 4 rows x 3 + 12 = transform 2's 4 rows x 3.  ZERO texture scales -- see (**) |
//   | sahf     |  18 | the 6 classify + 12 epsilon compares, one each |
//   | fxch     |  18 | 6 (the 5th byte lerp) + 8 (planes 2..5's numerators/results) + 4 (transform 2's x87 juggling) |
//   | fnstsw   |  18 | ditto sahf |
//   | shl      |  17 | 6 (pEnd = pIn + nIn) + 6 (pPrev = pIn + nIn-1) + FIVE (pOut += nIn) -- see (*) |
//   | fstp     |  16 | the six sunk arms' x87 cleanup: 2+3+3+3+3+2 |
//   | xor      |  13 | 6 x `nInside = 0` + 6 x `i = 0` + 1 x `return 0` |
//   | inc      |  12 | 6 x `g_1008be44++` + 6 x `i++` |
//   | fcomp    |  12 | 6 x `fDen < -1e-5` + 6 x `fDen > 1e-5` |
//   | dec      |  10 | 6 x `j = nIn - 1` + FOUR loop counters (the two transforms and the two per-vertex call loops) |
//   | jne      |  10 | 6 classify back-edges + 3 loop back-edges (transform 2, 0x1000d9b0, 0x10014580) + 1 = the clip-mask fork at 0x1002819f |
//   | jmp      |  10 | the six sunk epsilon arms + the ShowSplits else-skip (0x1002806e) + transform 2's jump to the draw (0x10028274) + the 0x10014580 loop's (0x10029728) + the `return 0` block's jump to the epilogue (0x10029732) |
//   | jb       |  10 | 4 classify (planes 2,3,4,5) + 6 sunk-arm high tests |
//   | jle      |   7 | the six `nIn <= 0` guards + transform 1's own guard |
//   | jbe      |   7 | 6 in-line epsilon low tests + plane 6's classify (`z <= far`) |
//   | fmulp    |   7 | the 5th byte lerp of each plane + transform 1's z |
//   | jl       |   6 | the six edge loops' `i < nIn` |
//   | sar      |   6 | six `(pOut - pOutStart) / 32` |
//   | fsub     |   6 | the six `pCur->? - pPrev->?` that OPEN each fDen |
//   | fdivp    |   6 | one divide per plane |
//   | fcomps   |   6 | the six classify tests, one per plane |
//   | nop      |   5 | alignment |
//   | call     |   5 | the five call sites listed above |
//   | fchs     |   8 | 2 classify negations (2,5) + 4 numerator negations (2,3,4,5) + 2 result negations (2,5) |
//   | fdivrs   |   2 | transform 1's `1.0f / fW` AND transform 2's -- TWO here where the twin has one |
//   | fsubrs   |   2 | `g_1008b908 - pPrev->z` (plane 1) and `g_1008b910 - pPrev->z` (plane 6) |
//   | fsubrp   |   2 | planes 3 and 4's `pPrev->y - pPrev->z` / `pPrev->x - pPrev->z` (DE E8+i is Intel FSUBP -- see the objdump warning) |
//   | fsts     |   2 | plane 3's `y = fZ` and plane 4's `x = fZ` |
//   | fld      |   2 | planes 2 and 5 duplicating fZ before negating it |
//   | and      |   1 | `nClipFlags &= 1` under g_1008d7a0 |
//   | incl     |   1 | `g_1008be5c++` after DrawPrimitive |
//   | jg       |   1 | transform 1's back edge (SIGNED -- see REGION 3) |
//   | jae      |   1 | plane 1's classify (`z >= near`) |
//   | push     |   1 | `push %ebp` |
//   | pop      |   1 | `pop %ebp` |
//   | ret      |   1 | the single epilogue |
//
//  `flds` 118 decomposes BY OPERAND with nothing over:
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
//      0x100a6070+8k x6 (-1e-5) + 0x100a60a0..0x100a60cc x12 (+1e-5, 0.0)
//                    = 18 constants
//      -0xc08(%ebp) .. -0xb20(%ebp) x30 = the byte-lerp float temps
//      (%eax) x3, 0x4(%eax) x3, 0x8(%eax) x3 = the source position read
//                      TWICE by transform 1 (the w and x rows) and ONCE
//                      by transform 2 (its x row)
//      0x10/0x14/0x18(%edx) x1 each + 0x20/0x24/0x28(%edx) x1 each = 6
//                      = transform 1's y and z matrix rows
//      0x1008ba8c/90/94 + 0x1008ba6c/70/74 + 0x1008ba7c/80/84 x1 each = 9
//                      = transform 2's w, y and z matrix rows
//                                                        ---- 118 exactly
//
//  `add` 66, by operand:
//      %eax,%ecx  11 = 6 `pOut++` after the lerp (`mov $0x20,%ecx ; add`)
//                      + FIVE `pOut += nIn`         (see (*) )
//      $0x20,%eax  7 = the six `pOut++` after the 32-byte copy + the
//                      transform-2 loop's `pWalk++`
//      %esi,%ebx   6 = `nInside += nFlag`
//      $0x4,%ecx   6 = `pFlag++`
//      $0x20,%esi  6 = the six edge loops' `pCur++`
//      $0x20,%edx  6 = the six classify loops' `pWalk++`
//      %eax,%edx   5 } together with `add %edx,%eax` x1 = SIX
//      %edx,%eax   1 }   `pEnd = pIn + nIn`
//      %eax,%ebx   5 } together with `add %ecx,%ebx` x1 = SIX
//      %ecx,%ebx   1 }   `pPrev = pIn + (nIn - 1)`
//      $0x20,%ebx  3 = transform 1's `pDst++` + the 0x1000d9b0 loop's
//                      `pWalk++` + the 0x10014580 loop's `pWalk++`
//      $0x18,%eax  1 = THE SOURCE RECORD STRIDE, 24 bytes, `pData++`
//      $-N,%esp / $N,%esp 8 = the eight outgoing-argument-area
//                      adjustments (0x10028079, 0x10028089, 0x100280ae,
//                      0x100282a0+0x100282b2, 0x10029710+0x10029722,
//                      0x10029734 -- the last of which the epilogue
//                      closes)
//                                                        ---- 66 exactly
//
//  `lea` 30, by operand:
//      20 pads: `lea 0x0(%esi,%eiz,1),%esi` x11,
//               `lea 0x0(%edi,%eiz,1),%edi` x5,
//               `lea 0x0(%esi),%esi` x3, `lea 0x0(%edi),%edi` x1
//      10 real: SIX flag-array bases (one per plane -- a seventh plane
//               would make it seven), ONE `-0xb1c(%ebp)` (the single
//               out-parameter of 0x1000a700 -- the twin needs TWO because
//               its count is a local; ours arrives as `int32*` arg2 and
//               is passed straight through), TWO `-0xb18(%ebp)` (the
//               transform's %ebx and `*ppVerts = aVerts`), and ONE
//               `-0x2(%edx,%eax,1)` = `g_1008be34 + *pnVerts - 2`.
//                                                        ---- 30 exactly
//
//  (*) `shl` 17 and `add %eax,%ecx` 11 both say FIVE, not six,
//  `pOut += nIn`, and the sites prove it individually:
//      pEnd:  shl 0x10028320 0x1002866a 0x100289c2 0x10028d0b 0x1002905c
//             0x100293b1                                       -- SIX
//      pPrev: shl 0x100283a3 0x100286fd 0x10028a4e 0x10028d9b 0x100290ee
//             0x1002943f                                       -- SIX
//      pOut+=nIn: shl 0x10028645 0x1002899d 0x10028ce6 0x10029037
//             0x1002938c                                       -- FIVE
//      and the matching `add %eax,%ecx` at 0x10028648, 0x100289a0,
//      0x10028ce9, 0x1002903a, 0x1002938f                      -- FIVE
//  Plane 6's copy is absent because pOut is dead after it -- the same
//  dead `pOut +=` big051a0.cpp found in ITS pass 6 by counting `lea`,
//  and big299a8.cpp found in ITS plane 6 by these same two rows.  This
//  is the FOURTH independent route to that one dead statement, and the
//  first from a function with a different vertex bound.
//
//  (**) `fmuls` 24 with ZERO texture scales is the census row that says
//  this function does NOT scale u,v by the bound texture's 1/w,1/h.  The
//  twin's 14 is 12 matrix + 2 scales; ours is 12 + 12 and neither
//  0x1008e664 nor 0x1008e668 appears anywhere in the function.  The
//  clipper still lerps 0x18/0x1c (twelve `flds`), so the vertex HAS a uv
//  pair -- it is simply never written by this function.
//
//  A SEVENTH PLANE, A MISSING PLANE, A SECOND TEXTURE PAIR, A SIXTH
//  COLOUR BYTE or a THIRD position lerp would each move at least three
//  of movzbl/fildl/fistpl/flds/fsubs/fadds/cmp/test, and none of them
//  moves.
//
//  ONE PREDICTED CENSUS DIVERGENCE, and it is a known toolchain gap
//  rather than a decode error.  The thirty `(uint8)float` conversions
//  ship as `fstps <temp> ; flds <temp> ; fistpl <int>` with no
//  fnstcw/fldcw and no helper call.  Plain
//  /O2 icl instead emits a shld/neg/or/shr idiom at every such site and
//  `/Qrcd` cuts it back (icl 4.0 has NO /QIfist -- it parses as
//  /QIf and is silently ignored).  So expect our fistpl 30 -> 0 with
//  shld/neg/or/shr appearing at exactly 30, and the fstps/flds rows to
//  move by 30 with them.  It costs no CALL either way, so the call-sequence
//  figure is unaffected.
//
// ----------------------------------------------------------------------- //
//  INSTRUMENT 3 -- THE LITERAL POOL, READ IN ORDER
// ----------------------------------------------------------------------- //
//  Our pool run is 0x100a6060..0x100a60d0 and it is a transcript.  Every
//  word dumped from the image, with the code
//  address that loads it (a blank means icl emitted the literal and never
//  loads it -- verified by grepping a full objdump of the image, so
//  "unreferenced" means unreferenced by the WHOLE image, not just by this
//  function):
//
//      0x100a6060   1.0        --            <- see (b)
//      0x100a6064   1.0        0x100280f7    <- transform 1's `fdivrs`
//      0x100a6068   1.0        --            <- see (b)
//      0x100a606c   1.0        0x100281df    <- transform 2's `fdivrs`
//      0x100a6070  -1e-5       0x10028444    plane 1 low epsilon
//      0x100a6074   1.0        --
//      0x100a6078  -1e-5       0x10028798    plane 2
//      0x100a607c   1.0        --
//      0x100a6080  -1e-5       0x10028aea    plane 3
//      0x100a6084   1.0        --
//      0x100a6088  -1e-5       0x10028e38    plane 4
//      0x100a608c   1.0        --
//      0x100a6090  -1e-5       0x1002918a    plane 5
//      0x100a6094   1.0        --
//      0x100a6098  -1e-5       0x100294d4    plane 6
//      0x100a609c   1.0        --
//      0x100a60a0  +1e-5       0x10029870  } plane 6's sunk arm
//      0x100a60a4   0.0        0x10029885  }
//      0x100a60a8  +1e-5       0x10029840  } plane 5
//      0x100a60ac   0.0        0x10029857  }
//      0x100a60b0  +1e-5       0x10029810  } plane 4
//      0x100a60b4   0.0        0x10029827  }
//      0x100a60b8  +1e-5       0x100297e0  } plane 3
//      0x100a60bc   0.0        0x100297f7  }
//      0x100a60c0  +1e-5       0x100297b0  } plane 2
//      0x100a60c4   0.0        0x100297c7  }
//      0x100a60c8  +1e-5       0x10029790  } plane 1
//      0x100a60cc   0.0        0x100297a5  }
//      0x100a60d0   1.0        --            <- see (b)
//      0x100a60d4   1.0        0x10029a9f    <- the NEXT function's
//                                              (0x100299a0, the twin)
//
//  Four things it settles:
//
//  (a) THE TWO TRANSFORMS ARE BOTH WRITTEN BEFORE THE CLIPPER, AND IN
//      THE ORDER THEY APPEAR IN THE CODE.  0x100a6064 and 0x100a606c are
//      the function's first two loaded literals and they are the `1.0f`
//      of transform 1's and transform 2's reciprocals, at ascending pool
//      addresses at ascending code addresses.  Nothing else in 6.2 KB
//      emits a float literal outside the clipper, so there is no colour
//      scale, no texture scale, no fog constant and no z bias anywhere.
//
//  (b) THE CLIP RUN IS BIT-FOR-BIT big051a0.cpp's AND big299a8.cpp's --
//      SEVEN UNREFERENCED 1.0f INCLUDED, IN THE SAME POSITIONS.  This is
//      the THIRD independent copy, and the answer is yes:
//
//        big051a0  0x100a412c..0x100a4188 + trailing 0x100a418c
//        ours      0x100a6070..0x100a609c + trailing 0x100a60d0
//        twin      0x100a60d8..0x100a6104 + trailing 0x100a6138
//
//      six (-1e-5, 1.0-unreferenced) pairs at ascending addresses in
//      plane order, then six (+1e-5, 0.0) pairs in REVERSE plane order,
//      then one trailing unreferenced 1.0f.  Identical shape, identical
//      reversal, identical seven.
//
//      AND OURS ADDS A DATA POINT THE OTHER TWO COULD NOT GIVE, because
//      ours is the only one of the three with TWO divides outside the
//      clipper.  The four words 0x100a6060/64/68/6c are 1.0, 1.0-LOADED,
//      1.0, 1.0-LOADED: one unreferenced 1.0f immediately BEFORE each of
//      the two `fdivrs` literals.  That makes the count exactly ONE
//      UNREFERENCED 1.0f PER FLOAT DIVISION IN THE SOURCE -- two
//      transform divides, six clip divides, eight unreferenced 1.0f
//      (0x100a6060, 0x100a6068, and the six in the clip run) -- and it
//      predicts that the "trailing" 1.0f each of the three pools ends
//      with is not trailing at all: 0x100a60d0 is the leading member of
//      the TWIN's transform-divide pair, whose loaded member is
//      0x100a60d4, and 0x100a4188/0x100a6138 are the same thing for
//      whatever follows those two.  The mechanism is unproved (it
//      looks like a reciprocal-multiply candidate icl materialises for
//      every `/` and then discards), so it stays in NOT PINNED below --
//      but the eight-for-eight count is exact, and it is the first
//      explanation any of the three copies has produced.
//
//  (c) SIX PLANES, in the order near, left, top, right, bottom, far --
//      the six -1e-5 loads are in ASCENDING pool order at ascending code
//      addresses, one per plane, and the six sunk arms consume the other
//      twelve.  There is no room in the run for a seventh.
//
//  (d) THE POOL'S LOWER BOUNDARY CANNOT BE PINNED.  0x100a6000..
//      0x100a605c is referenced by nothing in the image at all, so
//      whether 0x100a6060 is ours or the previous function's cannot be
//      settled from references.  Under (b) it is ours.
//
// ----------------------------------------------------------------------- //
//  INSTRUMENT 4 -- SIBLING CORRESPONDENCE, EXPRESSION BY EXPRESSION
// ----------------------------------------------------------------------- //
//  Two siblings, not one, and they cover different halves:
//
//  A.  0x100299a8 (big299a8.cpp, the MIRROR-SURFACE DRAW) is this
//      function's near twin: same geometry fetch, same 4x4 transform
//      loop, same six-plane clipper.  Every claim big299a8.cpp makes is
//      checked against these bytes below, four CONFIRMED and one
//      REFUTED.
//  B.  0x1006d910 (big6d910.cpp) carries the OTHER transform -- the
//      inline projective pass through 0x1008b880+0x1dc that the twin
//      does not have at all -- in a decoded, expression-by-expression
//      form.  Its `if (!nFlags) { ...projective... goto L_Tex; }` is
//      this function's 0x10028198..0x10028274 statement for statement.
//
//  --- CLAIM 1: "Same 26-instruction per-vertex projective transform
//      loop, whose `fdivrs` dumps 1.0f from 0x100a6064 (yours) against
//      0x100a60d4 (the twin's)."   CONFIRMED, with the count corrected.
//
//      Diffed instruction by instruction, ours 0x100280df..0x10028155
//      against the twin's 0x10029a87..0x10029b11.  The four matrix rows
//      are 43 x87 instructions, not 26, and they agree OPERAND FOR
//      OPERAND including the operand order inside every product:
//
//        ours                              twin
//        flds 0x8(%eax);fmuls 0x38(%edx)   flds 0x8(%eax);fmuls 0x38(%ebx)
//        flds 0x4(%eax);fmuls 0x34(%edx)   flds 0x4(%eax);fmuls 0x34(%ebx)
//        faddp                             faddp
//        flds (%eax);   fmuls 0x30(%edx)   flds (%eax);   fmuls 0x30(%ebx)
//        faddp; fadds 0x3c(%edx)           faddp; fadds 0x3c(%ebx)
//        fdivrs 0x100a6064                 fdivrs 0x100a60d4
//        flds 0x8(%eax);fmuls 0x8(%edx)    flds 0x8(%eax);fmuls 0x8(%ebx)
//        ... x row, `fmul %st(1),%st`, fstps  (dest)
//        flds 0x18(%edx);fmuls 0x8(%eax)   flds 0x18(%ebx);fmuls 0x8(%eax)
//        ... y row, `fmul %st(1),%st`, fstps 0x4(dest)
//        flds 0x20(%edx);fmuls (%eax)      flds 0x20(%ebx);fmuls (%eax)
//        ... z row, `fmulp`,              fstps 0x8(dest)
//
//      Note the ROW-INTERNAL TERM ORDER, which is source-visible under
//      the known pattern "an FP sum emits its terms backwards": the w, x and y
//      rows emit z,y,x (so the source writes x,y,z as a flat sum) while
//      the z row emits x,y,z (so the source's z row is the same flat sum
//      and icl simply scheduled it forwards -- both functions do it, in
//      the same row, which is what says it is not a source difference).
//      0x100a6064 dumps as 0x3f800000 = 1.0f, the same constant as the
//      twin's 0x100a60d4, and D8 /7 `fdivrs` with a MEMORY operand is
//      st0 = mem / st0 = 1.0f / fW (the swapped-mnemonic hazard is only
//      the DE register forms).
//
//  --- CLAIM 2: "Same 0x18 source vertex stride."   CONFIRMED.
//      `add $0x18,%eax` at 0x10028161, applied to the -0xb1c slot that
//      0x1000a700 filled.  ONE such `add` in the whole function.  The
//      twin reaches the same 0x18 with `lea 0x18(%edx),%eax`.  And
//      +0x00 of that record is a POINTER, not an embedded vector:
//      0x100280dc is `mov (%eax),%eax` and the three float loads that
//      follow are off the LOADED value.
//
//  --- CLAIM 3: "Same `lea -0x2(%eax,%edx,1)` triangle-count update."
//      CONFIRMED IN FORM, REFUTED IN PLACE.  Ours is
//      `lea -0x2(%edx,%eax,1),%edx` at 0x10028186 with %edx =
//      g_1008be34 and %eax = *pnVerts -- the same instruction with the
//      base and index swapped (the "SIB base/index roles" pattern).  But
//      the twin does it at the very END, immediately before
//      DrawPrimitive (0x1002af46); ours does it EARLY, at 0x10028186,
//      before the clip mask is even read, on the UNCLIPPED count.  So
//      the shipped statement counts the triangles the polygon would have
//      had, not the ones drawn -- and in ours it is executed even on the
//      twelve paths that `return 0` and draw nothing.  That is a real
//      behavioural difference between the two functions and it is
//      reproduced here as it ships.
//
//  --- CLAIM 4: "Same six 0xdc clip blocks, at -0x618/-0x53c/-0x460/
//      -0x384/-0x2a8/-0x1cc."   CONFIRMED EXACTLY, six `lea ...,%ecx`
//      at 0x10028338, 0x10028682, 0x100289da, 0x10028d23, 0x10029074,
//      0x100293c7, 0xdc apart, -0x618 + 6*0xdc = -0xf0.  And plane for
//      plane the clipper is the same expressions: the gate bits in the
//      order 0x01, 0x04, 0x08, 0x10, 0x20, 0x02 (0x1002830f, 0x10028656,
//      0x100289ae, 0x10028cf7, 0x10029048, 0x1002939d); the classifies
//      `z >= near` / `-z < x` / `y < z` / `x < z` / `-z < y` /
//      `z <= far`; the six fDens; the six numerators including planes 3
//      and 4's Intel-FSUBP `pPrev->y - pPrev->z` / `pPrev->x -
//      pPrev->z`; the position stores including planes 2/5's
//      fld/fchs/fxch pair and planes 3/4's `fsts`; and the five byte
//      lerps at 0x12, 0x11, 0x10, 0x13, 0x17 in that order in all six
//      planes.  Constants dumped from OUR pool, not copied: 0x100a6070/
//      78/80/88/90/98 are all 0xb727c5ac = -9.999999747e-06, and
//      0x100a60a0..0x100a60cc alternate 0x3727c5ac = +9.999999747e-06
//      with 0x00000000.
//
//  --- CLAIM 5: "MEASURED DIFFERENCE: your vertex block is 0x500 = 40
//      vertices where the twin's is 0x1000 = 128."   CONFIRMED off our
//      own frame: -0xb18 .. -0x619 between the pData slot and the first
//      clip block, 0x500 = 1,280 = 40 * 0x20.  See INSTRUMENT 1.
//
//  --- CLAIM 6: "Both are reached from the same caller, yours at
//      0x100267ff."   REFUTED TWICE.  There are THREE callers, not one,
//      and none of them is at 0x100267ff:
//          0x10025697   0x10025859   0x10027910
//      0x10027910 is the one big26728.cpp decoded (its banner calls the
//      block "0x100278e7 g_1008b874 = 0x3f and the 0x1002803c gate").
//      All three are quoted under CALL SITES below and all three agree
//      on the four-argument signature; the two new ones are what pin
//      arg3's type, because both pass the literal `$0x1008ba9c` =
//      0x1008b880 + 0x21c.
//
//  --- WHAT THE TWIN GOT WRONG ABOUT OUR TRANSFORM LOOP.
//      big299a8.cpp says: "The twin writes BOTH 0x10 and 0x14 of the
//      destination vertex as 0xffffffff (0x1002814f/0x10028152)".  Only
//      0x14 is 0xffffffff.  0x1002814f is `mov %edi,0x14(%ebx)` with
//      %edi = -1 (hoisted from `mov $0xffffffff,%eax ; mov %eax,%edi` at
//      0x100280c1/c9), but 0x10028152 is `mov %esi,0x10(%ebx)` and %esi
//      is reloaded from -0xdc(%ebp) at the TOP of every iteration
//      (0x100280d6) -- and -0xdc holds the ShowSplits-conditional colour
//      computed at 0x1002805c..0x10028079, which is a whole statement
//      the twin's diff missed.  See REGION 2.
//
// ----------------------------------------------------------------------- //
//  WHAT THE FUNCTION IS -- and the CALL SITES, read not guessed
// ----------------------------------------------------------------------- //
//  THREE CALLERS, all entering at the register entry 0x1002803c:
//
//   1. 0x10025697, inside a 32-byte frameless wrapper at 0x10025680:
//        10025680  mov 0x4(%esp),%eax            ; pObj, its own argument
//        10025684  sub $0x1c,%esp
//        10025687  movl $0x1008ba9c,0xc(%esp)    ; arg3 = &g_mat1008ba9c
//        1002868f  lea 0x10(%esp),%edx           ; arg1 = &pVerts
//        10025693  lea 0x14(%esp),%ecx           ; arg2 = &nVerts
//        10025697  call 0x1002803c
//        1002569c  add $0x1c,%esp ; ret
//      -- the result is DISCARDED here, which is why the return value is
//      an int32 nobody is obliged to read.
//
//   2. 0x10025859, inside the deferred-list flush at 0x10025820:
//        10025840  mov 0x10(%ebx),%eax ; mov 0x14(%ebx),%ebp
//        10025846  mov %eax,0x1008b874           ; the list record's own
//                                                ; clip mask, published
//        1002584b  mov (%ebx),%eax               ; arg0 = pRec->m_00
//        1002584d  mov %esi,0xc(%esp)            ; arg3 = &g_mat1008ba9c
//        10025851  lea 0x10(%esp),%edx           ; arg1
//        10025855  lea 0x14(%esp),%ecx           ; arg2
//        10025859  call 0x1002803c
//      -- and THIS is what says our `g_n1008b874` read at 0x10028198 is
//      a read of something the CALLER set.  We never write it.
//
//   3. 0x10027910, inside 0x10026728's mirror pass, decoded in
//      big26728.cpp:
//          g_1008b874 = 0x3f;
//          if (sub_1002803c(pObj, &pVerts, &nVerts, pXForm))
//          {   ... the stencil states, then the caller overwrites
//              pVerts[j].m_b/m_g/m_r and draws ... }
//      -- the same four arguments, the same `test %eax,%eax ; je`, and
//      big26728.cpp already declares the prototype this file uses.
//
//  FIVE TELLS FOR WHAT IT DOES, none of them a guess:
//   1. IT IS A WORLD-POLYGON DRAW, NOT A MIRROR OVERLAY.  It touches no
//      render state at all -- there is no SetRenderState anywhere (the
//      twin brackets itself with ALPHABLENDENABLE 1/0); its only device
//      call is DrawPrimitive.  Every caller sets the state around it.
//   2. THE COLOUR IS A DEBUG COLOUR.  0x1008cac4 is g_CV_ShowSplits
//      (big21f90.cpp names it from the console-variable table), and the
//      diffuse dword is the POLY's address when ShowSplits is on and the
//      SURFACE's address (pObj->m_p28, the same +0x28 big26728.cpp
//      declares) when it is off -- one colour per split, or one colour
//      per surface.  Using an address as an RGBA is the standard trick.
//   3. THE TEXTURE STAGE IS UNBOUND, NOT BOUND.  0x100232b0 is
//      d3dbatchd.cpp's "unbinds a texture stage" (big572b4.cpp,
//      big44ff4.cpp, big5a960.cpp all name it) and we call it with
//      g_n1008d7d0, the current stage, exactly as big44ff4.cpp does at
//      0x1004500b.  The twin instead BINDS through 0x10015320 and
//      returns early if the bind fails.  An untextured flat-shaded draw
//      is what the FVF-with-TEX1-but-no-uv-writes says too.
//   4. THE TWO TRANSFORMS ARE THE OBJECT'S 4x4 AND THE CAMERA'S.  arg3
//      is a caller-supplied 4x4 (two callers pass 0x1008ba9c =
//      0x1008b880+0x21c); the second, inline, transform reads
//      0x1008ba5c..0x1008ba98 = 0x1008b880+0x1dc..+0x218, which
//      big24090.cpp, big6d910.cpp and big70d10.cpp all identify as the
//      transform object's projective block, and it writes +0x0c (the
//      RHW) which is exactly what 0x10014580 fills on the other path.
//   5. THE VERTEX AND THE PRIMITIVE.  DrawPrimitive (vtable +0x64,
//      index 25) with type 6 = D3DPT_TRIANGLEFAN and FVF 0x1c4 =
//      XYZRHW|DIFFUSE|SPECULAR|TEX1, over the 0x20 stride whose
//      +0x00/04/08 are floats, +0x0c the RHW, +0x10..0x13 four bytes,
//      +0x14..0x17 four more and +0x18/+0x1c floats -- big26728.cpp's
//      RVert26728, field for field.
//
// ----------------------------------------------------------------------- //
//  A WARNING ABOUT THE DUMPS
// ----------------------------------------------------------------------- //
//  objdump prints binutils' SWAPPED mnemonics for the x87 DE E0/E8/F0/F8
//  register forms.  Verified with `as` in this working directory before
//  believing either:
//      as: `fdivrp %st,%st(1)` assembles to DE F9, and `fdivp` to DE F1
//      -> so binutils is self-consistent and INVERTED w.r.t. Intel:
//         DE F0+i is Intel FDIVRP (ST(i) = ST(0)/ST(i)) and objdump
//         calls it "fdivp"; DE E8+i is Intel FSUBP (ST(i) = ST(i)-ST(0))
//         and objdump calls it "fsubrp".
//  All six `de f1` divides in this function are therefore
//  numerator/denominator with the NUMERATOR in st0 -- read against the
//  stack state at each site, e.g. plane 1's 0x1002845d has st0 =
//  g_f1008b908 - pPrev->z and st1 = fDen, so fT = num/fDen.  And planes
//  3 and 4's `de ea` (0x10028afe, 0x10028e4c) is st2 = st2 - st0 =
//  `pPrev->y - pPrev->z` / `pPrev->x - pPrev->z` and not the reverse.
//  big051a0.cpp and big299a8.cpp reach the same reading of the same
//  bytes.
//
// *********************************************************************** //

#include "ltbasedefs.h"


// ======================================================================= //
//  TYPES.  Every offset below is read off a shipped instruction in THIS
//  function and the address is quoted.  Nothing is padded to a size that
//  is not measured.
// ======================================================================= //

struct Vec28030
{
	float           x;                                  // +0x00
	float           y;                                  // +0x04
	float           z;                                  // +0x08
};

union VColor28030
{
	uint32          m_n;
	uint8           m_b[4];
};

// ----------------------------------------------------------------------- //
//  THE DRAW VERTEX.  0x20 bytes: the stride is `add $0x20,%esi` /
//  `add $0x20,%edx` / `add $0x20,%ebx` / `add $0x20,%eax` / `shl $0x5`
//  throughout, and the FVF handed to DrawPrimitive at 0x1002973c is
//  0x1c4 = XYZRHW|DIFFUSE|SPECULAR|TEX1, which lays out exactly this
//  way.  Fields, with a writer for each:
//    +0x00/04/08  fstps (%ebx)/0x4(%ebx)/0x8(%ebx)   0x10028116/32/55
//                 and fstps (%eax)/0x4(%eax)/0x8(%eax) 0x1002825f..67
//    +0x0c        written ONLY by transform 2 (`fstps 0xc(%eax)` at
//                 0x10028261, the reciprocal) and otherwise merely
//                 copied by `*pOut = *pPrev` (0x100283ff).  On the
//                 clipped path 0x10014580 fills it (big572b4.cpp:
//                 "+0x0c the reciprocal w").
//    +0x10        mov %esi,0x10(%ebx) with %esi = the debug colour
//                 0x10028152
//    +0x14        mov %edi,0x14(%ebx) with %edi = 0xffffffff
//                 0x1002814f
//    +0x17        the FIFTH byte lerp of every clip plane 0x10028607
//    +0x18/+0x1c  never written by this function; lerped by all six clip
//                 planes (`fstps 0x18(%eax)` / `0x1c(%eax)`, twelve
//                 sites from 0x1002848d)
//  The four diffuse bytes are lerped in the order 0x12, 0x11, 0x10,
//  0x13 (0x1002849e, 0x100284e5, 0x1002852c, and the fourth at the same
//  0x47-byte spacing) then the specular's 0x17 -- b,g,r,a in memory,
//  r,g,b,a in source order, the same as big051a0.cpp's macro and the
//  twin's.
// ----------------------------------------------------------------------- //
struct RVert28030                                       // 0x20 bytes
{
	Vec28030        m_vPos;                             // +0x00
	float           m_fRHW;                             // +0x0c
	VColor28030     m_Diffuse10;                        // +0x10
	VColor28030     m_Specular14;                       // +0x14
	float           m_fU;                               // +0x18
	float           m_fV;                               // +0x1c
};

// ----------------------------------------------------------------------- //
//  THE SOURCE RECORD.  Stride 0x18, read off `add $0x18,%eax` at
//  0x10028161 -- and independently off `lea 0x18(%edx),%eax` at
//  0x10029b57 in the twin 0x100299a8, which walks the same array from
//  the same 0x1000a700 call.  +0x00 is a POINTER, not an embedded
//  vector: 0x100280dc is `mov (%eax),%eax` and the float loads that
//  follow are off the LOADED value.  The remaining 20 bytes are touched
//  by nothing in this function -- note that unlike the twin we do NOT
//  read +0x04 and +0x08, because we write no texture coordinates, so
//  our own bytes pin only the stride and the pointer.
// ----------------------------------------------------------------------- //
struct SrcRef28030                                      // 0x18 bytes
{
	Vec28030*       m_pPos;                             // +0x00
	uint8           m_pad_04[0x14];                     // +0x04 .. +0x17
};

// ----------------------------------------------------------------------- //
//  THE CALLER-SUPPLIED MATRIX.  arg3, at 0x14(%ebp), cached in %edx by
//  `mov 0x14(%ebp),%edx` at 0x100280c6.  Row-major 4x4, and twelve of
//  the sixteen elements plus all four translations are loaded: the w row
//  is 0x30/0x34/0x38 + 0x3c (0x100280e2..0x100280f4), x is 0x00/0x04/
//  0x08 + 0x0c (0x10028100..0x10028111), y is 0x10/0x14/0x18 + 0x1c
//  (0x10028118..0x1002812d) and z is 0x20/0x24/0x28 + 0x2c
//  (0x10028135..0x1002814a).  big26728.cpp already calls it Mat26728
//  and identifies it as src/LT2/sdk/inc/ltmatrix.h's LTMatrix; two of the
//  three callers pass &g_mat1008ba9c.
// ----------------------------------------------------------------------- //
struct Mat28030
{
	float           m[4][4];                            // +0x00 .. +0x3f
};

// ----------------------------------------------------------------------- //
//  THE TRANSFORM OBJECT at 0x1008b880.  Two disjoint uses, and they are
//  kept apart exactly as big6d910.cpp keeps them:
//    * as an OPAQUE object taken by address and handed to 0x1000d9b0 and
//      0x10014580 (`mov $0x1008b880,%edi` at 0x10028290 and 0x10029705);
//    * as the sixteen floats at +0x1dc..+0x218, which are the sixteen
//      absolute addresses 0x1008ba5c..0x1008ba98 that transform 2 loads.
//      0x1008ba5c = 0x1008b880 + 0x1dc exactly; big24090.cpp,
//      big6d910.cpp and big70d10.cpp all record the same identity.
// ----------------------------------------------------------------------- //
struct XForm28030
{
	uint8           m_pad_000[0x1dc];                   // +0x000 .. +0x1db
	float           m_f1dc;                             // 0x1008ba5c
	float           m_f1e0;                             // 0x1008ba60
	float           m_f1e4;                             // 0x1008ba64
	float           m_f1e8;                             // 0x1008ba68
	float           m_f1ec;                             // 0x1008ba6c
	float           m_f1f0;                             // 0x1008ba70
	float           m_f1f4;                             // 0x1008ba74
	float           m_f1f8;                             // 0x1008ba78
	float           m_f1fc;                             // 0x1008ba7c
	float           m_f200;                             // 0x1008ba80
	float           m_f204;                             // 0x1008ba84
	float           m_f208;                             // 0x1008ba88
	float           m_f20c;                             // 0x1008ba8c
	float           m_f210;                             // 0x1008ba90
	float           m_f214;                             // 0x1008ba94
	float           m_f218;                             // 0x1008ba98
};

// ----------------------------------------------------------------------- //
//  THE OBJECT.  arg0.  Only +0x28 is read directly here
//  (`mov 0x28(%ebx),%eax` at 0x10028070), and 0x1000a700 reads
//  +0x4c/+0x50/+0x52/+0x54 out of the same pointer.  Same shape
//  big26728.cpp calls Obj26728 / Sub26728 -- and the same +0x28 the twin
//  dereferences for its texture, which is why it is declared as a
//  pointer here even though these bytes only ever use its VALUE.
// ----------------------------------------------------------------------- //
struct Sub28030;

struct Obj28030
{
	uint8           m_pad_00[0x28];                     // +0x00 .. +0x27
	Sub28030*       m_p28;                              // +0x28
};


// ======================================================================= //
//  THE DIRECT3D 7 DEVICE AT 0x1008d988 -- big124a4.cpp's / big572b4.cpp's
//  declaration.  ONE slot is used here: +0x64 DrawPrimitive (index 25).
//  Unlike every sibling in this family the function touches no render
//  state at all, so +0x50 does not appear.
// ======================================================================= //
struct D3DDevice7;

struct D3DDevice7Vtbl
{
	void*  m_pad_00[25];                                // +0x00 .. +0x60
	long (__stdcall* DrawPrimitive)(D3DDevice7*, uint32 nType, uint32 nFVF,
	                                void* pVerts, uint32 nCount,
	                                uint32 nFlags);     // +0x64
};

struct D3DDevice7
{
	D3DDevice7Vtbl* lpVtbl;
};

#define D3DPT_TRIANGLEFAN                             6
#define D3DFVF_CLIPVERTEX                         0x1c4  // XYZRHW|DIFFUSE
                                                         // |SPECULAR|TEX1


// ======================================================================= //
//  GLOBALS.  Relocations are masked, so only the shapes matter.  Each is
//  named by the address these bytes touch and each name is one an earlier
//  decode already fixed.
// ======================================================================= //

extern D3DDevice7*  g_pD3DDevice;                       // 0x1008d988

// 0x1008cac4 -- ShowSplits.  big21f90.cpp's console-variable table names
// it `int32 g_CV_ShowSplits`.  Read ONCE, at 0x1002805c, and used only to
// choose which address becomes the debug colour.
extern int32        g_CV_ShowSplits;

// 0x1008d7d0 -- the current texture stage.  d3dqip3.cpp ("current
// stage"), big43e10.cpp and big6d910.cpp all name it uint32.  Read once,
// at 0x1002807c, and handed straight to 0x100232b0.
extern uint32       g_n1008d7d0;

// 0x1008b874 -- the clip-plane mask.  READ ONLY in this function, twice
// (0x10028198 and 0x100282b8) and never written; big572b4.cpp and the
// twin both WRITE 0x3f into it, and so do two of our three callers.  The
// second read is skipped when the 0x1000d9b0 loop is empty, in which case
// %eax still carries the first read -- the proof that the source
// reads the global twice into the same local and icl eliminated the
// reload only on the path with no intervening call.
extern uint32       g_n1008b874;

// 0x1008b87c -- the clipper's output bank.  Read ONCE at 0x100282dc and
// stored to BOTH pOut and pOutStart (0x100282e7 / 0x100282ef), exactly as
// big051a0.cpp records at 0x100051de and the twin at 0x10029bdc.
extern RVert28030*  g_p1008b87c;

// 0x1008b880 -- the transform object.  Taken by ADDRESS
// (`mov $0x1008b880,%edi` at 0x10028290 and 0x10029705), so it is an
// object, not a pointer.  big572b4.cpp / big124a4.cpp same.
extern uint8        g_1008b880[];

// 0x1008b908 / 0x1008b910 -- the near and far plane constants.  Each is
// used twice: as plane 1's / plane 6's classify comparand (0x10028353,
// 0x100293e3) and as the `fsubrs` numerator plus the integer store into
// pOut->m_vPos.z (0x10028457/0x10028479, 0x100294e7/0x10029509).
extern float        g_f1008b908;
extern float        g_f1008b910;

// 0x1008be34 -- the frame's triangle counter.  `lea -0x2(%edx,%eax,1)`
// at 0x10028186 is `+= *pnVerts - 2`, the triangle count of a fan.  The
// twin 0x100299a8 does the identical thing at 0x1002af46 -- but at the
// END of the function, where ours is at the beginning.
extern uint32       g_n1008be34;

// 0x1008be44 -- the clipped-polygon counter, `++` once per LIVE plane
// (six `mov ; inc ; mov` groups at 0x10028323, 0x1002866d, 0x100289c5,
// 0x10028d0e, 0x1002905f, 0x100293b4).
extern uint32       g_n1008be44;

// 0x1008be5c -- the frame counter, `incl 0x1008be5c` at 0x10029774,
// immediately after DrawPrimitive.  big124a4.cpp, big6d910.cpp,
// bigb910.cpp and d3dr3d.cpp all name it and all four record the same
// idiom in the same place.
extern uint32       g_n1008be5c;

// 0x1008d7a0 -- UseD3DClip.  `mov 0x1008d7a0,%edx ; test %edx,%edx ; je`
// at 0x100282c3, the only site.
extern uint32       g_n1008d7a0;


// ======================================================================= //
//  NEIGHBOURS.  Each signature is read off the call site in these bytes
//  and cross-checked against the decode named.
// ======================================================================= //

// 0x100232b0 -- d3dbatchd.cpp, "unbinds a texture stage".  One dword in
// the outgoing area at 0x10028081; big572b4.cpp calls it `sub_100232b0(0)`
// and big44ff4.cpp `sub_100232b0(g_n1008d7d0)`, which is our site.
extern "C" void   sub_100232b0(uint32 nStage);

// 0x1000a700 -- d3dbatchd.cpp.  Dumped in full (36 bytes) by big299a8.cpp:
//     if (g_1008cb84) { *ppData = pObj->m_p4c; *pnCount = pObj->m_w52; }
//     else            { *ppData = pObj + 0x54; *pnCount = pObj->m_w50; }
// The count is a `movzwl` of a WORD written through an `int32*`.  Our
// third argument is arg2 PASSED STRAIGHT THROUGH (0x1002808c/0x10028098),
// which is why every later count read in this function is a fresh
// `mov (%pnVerts),%reg` and why the transform guard is a SIGNED `jle`.
extern "C" void   sub_1000a700(void* pGeom, void** ppOutData,
                               int32* pnOutCount);

// 0x1000d9b0 -- d3dlow.cpp, __cdecl (vertex, transform object).  Its body
// is four instructions of argument shuffling and a tail call to
// 0x1000a770, a pure 3x4 matrix-vector multiply that writes x,y,z back in
// place and touches no global -- so it does NOT change g_n1008b874, and
// the re-read at 0x100282b8 is icl declining to keep a global live across
// a call (big572b4.cpp records the same at 0x100573b4).
extern "C" void   sub_1000d9b0(RVert28030* pVert, uint8* pXForm);

// 0x10014580 -- the screen projection through the transform object's
// +0x31c..+0x330, decoded in full at that address; d3dr2a.cpp,
// big124a4.cpp and big572b4.cpp all call it __cdecl (vertex, xform).
extern "C" void   sub_10014580(RVert28030* pVert, uint8* pXForm);


// The scratch vertex array is FORTY entries: 0x500 = 1,280 bytes between
// the pData slot at -0xb1c and the first clip block at -0x618, over a
// measured 0x20 stride.  See INSTRUMENT 1.  The twin's identical array is
// 128; nothing but each function's own `sub $N,%esp` decides it.
#define VERTS28030_MAX   40


// ======================================================================= //
//  0x10028030 .. 0x1002988f   (6,240 bytes, 5 calls)
//
//  Register ABI: %eax = pObj, %edx = ppVerts, %ecx = pnVerts, and the
//  fourth argument on the stack at 0x14(%ebp) = pXForm.  The stack entry
//  at 0x10028030 loads the first three from 0x4/0x8/0xc(%esp) and falls
//  through into the register entry at 0x1002803c, which spills %edx and
//  %ecx to -0x90/-0x8c and keeps %eax in %ebx.  Written __cdecl, as
//  for any register-ABI call, and big26728.cpp declares it the same way from the
//  other side of the call.
//
//  Returns int32: 1 if the polygon was drawn (0x1002977a `mov $0x1,%edx`)
//  and 0 if it was clipped away (0x10029730 `xor %edx,%edx`), merged into
//  one `mov %edx,%eax` at 0x1002977f.  Two of the three callers test it.
// ======================================================================= //
// Round to nearest in the current FPU mode: w1_10001000.cpp's helper
// (0x10015190 out of line).  Inlined, it is the shipped `fstps t ;
// flds t ; fistpl n` at each of the thirty byte lerps.
// Measured: 8666 -> 6138 bytes (shipped 6232), 7898 ->
// 5421 differing; the frame was 0xc28 against 0xc30.
//
// THE CLIPPER IS d3dr3c_os.cpp's 0x1000f4b0 INLINED, with
// its six plane bodies inlined into it -- w55_10016324_os.cpp's set over
// this file's 32-byte RVert28030, the bodies w1_10001000.cpp and
// w44_10001670.cpp compile byte-identical out of line.  It is what the
// shipped frame says: the planes' *ppOut is the memory-resident pOut at
// -0xd8 (`mov -0xd8,%eax ; mov %edi,-0x8 ; mov %eax,%edi ; add $0x20` at
// 0x100283db is `*(*ppOut)++ = *pPrev`), pIn/nIn are the driver's
// pVerts/nVerts at -0xd0/-0xdc, and nClipFlags its parameter at -0xc0.
// Measured: the frame is the shipped 0xc30 exactly, the body 6292 bytes
// against 6232, and it now pairs (a 46-byte common opening):
// attempted -> near.  The out-of-line copies icl emits for the static
// plane bodies are COMDATs nothing references; the link drops them.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}


#define CLIP_EPSILON    0.00001f
#define CLIP_MAX_VERTS  50

static inline void LerpRVert28030(RVert28030* a, RVert28030* b, RVert28030* dst, float t)
{
	dst->m_fU = (b->m_fU - a->m_fU) * t + a->m_fU;
	dst->m_fV = (b->m_fV - a->m_fV) * t + a->m_fV;
	dst->m_Diffuse10.m_b[2]  = (uint8)RoundF((b->m_Diffuse10.m_b[2]  - a->m_Diffuse10.m_b[2])  * t + a->m_Diffuse10.m_b[2]);
	dst->m_Diffuse10.m_b[1]  = (uint8)RoundF((b->m_Diffuse10.m_b[1]  - a->m_Diffuse10.m_b[1])  * t + a->m_Diffuse10.m_b[1]);
	dst->m_Diffuse10.m_b[0]  = (uint8)RoundF((b->m_Diffuse10.m_b[0]  - a->m_Diffuse10.m_b[0])  * t + a->m_Diffuse10.m_b[0]);
	dst->m_Diffuse10.m_b[3]  = (uint8)RoundF((b->m_Diffuse10.m_b[3]  - a->m_Diffuse10.m_b[3])  * t + a->m_Diffuse10.m_b[3]);
	dst->m_Specular14.m_b[3] = (uint8)RoundF((b->m_Specular14.m_b[3] - a->m_Specular14.m_b[3]) * t + a->m_Specular14.m_b[3]);
}


static inline float ClipEdgeNear(Vec28030* a, Vec28030* b, Vec28030* dst)
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


static inline float ClipEdgeLeft(Vec28030* a, Vec28030* b, Vec28030* dst)
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


static inline float ClipEdgeTop(Vec28030* a, Vec28030* b, Vec28030* dst)
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


static inline float ClipEdgeRight(Vec28030* a, Vec28030* b, Vec28030* dst)
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


static inline float ClipEdgeBottom(Vec28030* a, Vec28030* b, Vec28030* dst)
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


static inline float ClipEdgeFar(Vec28030* a, Vec28030* b, Vec28030* dst)
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
// Each plane body opens `pV = &(*ppVerts)[0];` rather than
// `pV = *ppVerts;` -- w23_100525b0_os.cpp's finding: it keeps the
// inlined driver's pVerts a memory slot, as the shipped 0x1002803c has it
// (normalised instruction diff 279 -> 176).
static inline LTBOOL Plane32_10001000(uint32* pUnused, RVert28030** ppVerts, int32* pnVerts, RVert28030** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert28030*  pOutStart;
	RVert28030*  pV;
	RVert28030*  pEnd;
	RVert28030*  pPrev;
	RVert28030*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
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

				LerpRVert28030(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001330(uint32* pUnused, RVert28030** ppVerts, int32* pnVerts, RVert28030** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert28030*  pOutStart;
	RVert28030*  pV;
	RVert28030*  pEnd;
	RVert28030*  pPrev;
	RVert28030*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
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

				LerpRVert28030(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001670(uint32* pUnused, RVert28030** ppVerts, int32* pnVerts, RVert28030** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert28030*  pOutStart;
	RVert28030*  pV;
	RVert28030*  pEnd;
	RVert28030*  pPrev;
	RVert28030*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
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

				LerpRVert28030(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001990(uint32* pUnused, RVert28030** ppVerts, int32* pnVerts, RVert28030** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert28030*  pOutStart;
	RVert28030*  pV;
	RVert28030*  pEnd;
	RVert28030*  pPrev;
	RVert28030*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
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

				LerpRVert28030(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001cb0(uint32* pUnused, RVert28030** ppVerts, int32* pnVerts, RVert28030** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert28030*  pOutStart;
	RVert28030*  pV;
	RVert28030*  pEnd;
	RVert28030*  pPrev;
	RVert28030*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
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

				LerpRVert28030(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Plane32_10001fe0(uint32* pUnused, RVert28030** ppVerts, int32* pnVerts, RVert28030** ppOut)
{
	uint32     anInside[CLIP_MAX_VERTS];
	RVert28030*  pOutStart;
	RVert28030*  pV;
	RVert28030*  pEnd;
	RVert28030*  pPrev;
	RVert28030*  pCur;
	uint32*    pFlag;
	int32      nInside;
	int32      i;
	int32      iPrev;

	++g_n1008be44;

	nInside = 0;
	pFlag   = anInside;
	pV      = &(*ppVerts)[0];
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

				LerpRVert28030(pPrev, pCur, *ppOut, t);

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
static inline LTBOOL Clip32_1000f4b0(uint32 nClipFlags, RVert28030** ppVerts, int32* pnVerts)
{
	RVert28030*     pOut;
	RVert28030*     pOutStart;
	RVert28030*     pVerts;
	int32      nVerts;
	uint32     anUnused[6];

	if (g_n1008d7a0)
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


int32 sub_10028030(Obj28030* pObj, RVert28030** ppVerts, int32* pnVerts,
                   Mat28030* pXForm)
{
	// -- the frame, in the order INSTRUMENT 1 names it ------------------ //
	RVert28030    aVerts[VERTS28030_MAX];
	SrcRef28030*  pData;

	RVert28030*   pWalk;
	RVert28030*   pDst;
	Vec28030*     pPos;
	XForm28030*   pM;
	uint32        nColor;
	int32         nCount;
	uint32        nLeft;
	int32         iVert;
	float         fOOW;
	float         fX, fY, fZ;

	// ------------------------------------------------------------------ //
	//  REGION 1 -- 0x10028030..0x1002805a.  PROLOGUE.
	//
	//    10028030  mov 0x4(%esp),%eax    ; arg0, the STACK entry
	//    10028034  mov 0x8(%esp),%edx    ; arg1
	//    10028038  mov 0xc(%esp),%ecx    ; arg2
	//    1002803c  push %ebp             ; the REGISTER entry
	//    1002803d  mov %esp,%ebp
	//    1002803f  sub $0xc30,%esp       ; 3,120 -- no _chkstk, under 4K
	//    10028045  mov %edi,0x8(%esp)
	//    10028049  mov %esi,0x4(%esp)
	//    1002804d  mov %ebx,(%esp)
	//    10028050  mov %edx,-0x90(%ebp)  ; ppVerts spilled
	//    10028056  mov %ecx,-0x8c(%ebp)  ; pnVerts spilled
	//
	//  arg3 is NOT spilled: it is loaded straight into %edx at 0x100280c6
	//  inside the transform preheader and used from there.  Nothing in
	//  this function reads it again, which is why the whole clipper runs
	//  with %edx meaning pPrev.
	// ------------------------------------------------------------------ //

	// ------------------------------------------------------------------ //
	//  REGION 2 -- 0x1002805c..0x10028079.  THE DEBUG COLOUR.
	//
	//    1002805c  mov 0x1008cac4,%edx           ; g_CV_ShowSplits
	//    10028062  mov %eax,%ebx                 ; pObj -> %ebx
	//    10028064  test %edx,%edx
	//    10028066  je 0x10028070
	//    10028068  mov %ebx,-0xdc(%ebp)          ; nColor = (uint32)pObj
	//    1002806e  jmp 0x10028079
	//    10028070  mov 0x28(%ebx),%eax
	//    10028073  mov %eax,-0xdc(%ebp)          ; nColor = pObj->m_p28
	//
	//  THE STATEMENT THE TWIN'S DIFF MISSED.  Both arms store an ADDRESS
	//  into the slot that later becomes the vertex's DIFFUSE dword
	//  (0x10028152 `mov %esi,0x10(%ebx)` with %esi reloaded from -0xdc):
	//  the polygon's own address when ShowSplits is on, its surface's
	//  when it is off.  One colour per BSP split, or one per surface.
	//
	//  ARM ORDER.  The TRUE arm falls through and the false arm is jumped
	//  to, with a `jmp` over it -- the plain if/else pattern, hosted at
	//  its first arm.  Neither arm makes a call, so this costs nothing in
	//  the call sequence; it is recorded because the block placement is
	//  source-visible.  The alternative spelling
	//      nColor = (uint32)pObj->m_p28;
	//      if (g_CV_ShowSplits) nColor = (uint32)pObj;
	//  emits the load first and predicts the same five calls.
	//
	//  IT IS BEFORE THE TWO CALLS, so it is before them in the source:
	//  icl had no reason to hoist a two-instruction select over
	//  0x100232b0 and 0x1000a700, and every other statement in this
	//  function stays where it is written.
	// ------------------------------------------------------------------ //
	if (g_CV_ShowSplits)
		nColor = (uint32)pObj;
	else
		nColor = (uint32)pObj->m_p28;

	// ------------------------------------------------------------------ //
	//  REGION 3 -- 0x10028079..0x100280a7.  UNBIND, THEN THE GEOMETRY.
	//
	//    10028079  add $0xfffffff8,%esp          ; 8 bytes: ONE dword
	//    1002807c  mov 0x1008d7d0,%eax
	//    10028081  mov %eax,(%esp)
	//    10028084  call 0x100232b0               ; CALL 1
	//    10028089  add $0xfffffff8,%esp          ; 8 MORE, not closed
	//    1002808c  mov -0x8c(%ebp),%eax          ; pnVerts
	//    10028092  lea -0xb1c(%ebp),%edx         ; &pData
	//    10028098  mov %eax,0x8(%esp)
	//    1002809c  mov %edx,0x4(%esp)
	//    100280a0  mov %ebx,(%esp)               ; pObj
	//    100280a3  call 0x1000a700               ; CALL 2
	//    100280ae  add $0x10,%esp                ; closes BOTH
	//
	//  Two openings and one closing: icl opened 8 bytes for the one-dword
	//  call and 8 more for the three-dword call, then reclaimed 0x10 in
	//  one `add`.  The arithmetic balances (-8 -8 +0x10), so there is no
	//  hidden fourth argument to either call.
	//
	//  THE THIRD ARGUMENT IS arg2 ITSELF, not the address of a local.
	//  That is the structural difference from the twin, which has to
	//  `lea` two out-parameters (its own nVerts AND its own pData) and
	//  therefore reads its count as an unsigned WORD.  Ours arrives as an
	//  `int32*`, is filled in place, and is re-read through the pointer
	//  seven times (0x100280b1, 0x1002817e, 0x100281af, 0x10028286,
	//  0x10028301, 0x100296fb, 0x10029761) -- never cached in a local,
	//  because every one of those reads is separated from the last by a
	//  store through a pointer icl must assume aliases it.
	// ------------------------------------------------------------------ //
	sub_100232b0(g_n1008d7d0);

	sub_1000a700(pObj, (void**)&pData, pnVerts);

	// ------------------------------------------------------------------ //
	//  REGION 4 -- 0x100280a8..0x1002816c.  THE OBJECT TRANSFORM.
	//  0xc4 bytes, 55 instructions, no calls.
	//
	//    100280b1  mov (%eax),%ecx               ; nCount = *pnVerts
	//    100280b3  lea -0xb18(%ebp),%ebx         ; pDst = aVerts
	//    100280b9  test %ecx,%ecx
	//    100280bb  jle 0x10028172                ; SIGNED zero guard
	//    100280c1  mov $0xffffffff,%eax
	//    100280c6  mov 0x14(%ebp),%edx           ; pXForm, arg3
	//    100280c9  mov %eax,%edi                 ; %edi = 0xffffffff
	//    100280d0  mov -0xb1c(%ebp),%eax         ; pData, reloaded
	//    100280d6  mov -0xdc(%ebp),%esi          ; nColor, reloaded
	//    100280dc  mov (%eax),%eax               ; pPos = pData->m_pPos
	//    100280de  dec %ecx
	//    100280df  flds 0x8(%eax) ; fmuls 0x38(%edx)      ]
	//    100280e5  flds 0x4(%eax) ; fmuls 0x34(%edx)      ]
	//    100280eb  faddp                                  ]  the w row
	//    100280ed  flds (%eax)    ; fmuls 0x30(%edx)      ]
	//    100280f2  faddp ; fadds 0x3c(%edx)               ]
	//    100280f7  fdivrs 0x100a6064             ; fOOW = 1.0f / fW
	//    100280fd  <the x row> ; fmul %st(1),%st ; fstps (%ebx)
	//    10028118  <the y row> ; fmul %st(1),%st ; fstps 0x4(%ebx)
	//    10028135  <the z row> ; fmulp %st,%st(1)
	//    1002814f  mov %edi,0x14(%ebx)           ; specular = 0xffffffff
	//    10028152  mov %esi,0x10(%ebx)           ; diffuse  = nColor
	//    10028155  fstps 0x8(%ebx)
	//    10028158  mov -0xb1c(%ebp),%eax
	//    1002815e  add $0x20,%ebx                ; pDst++
	//    10028161  add $0x18,%eax                ; pData++
	//    10028164  mov %eax,-0xb1c(%ebp)
	//    1002816a  test %ecx,%ecx ; jg 0x100280d0
	//
	//  WHAT PINS IT:
	//    * the divide is `fdivrs 0x100a6064` and 0x100a6064 dumps as
	//      0x3f800000 = 1.0f, so `1.0f / fW` and not `fW / K`.  D8 /7 is
	//      FDIVR with a MEMORY operand -- the swapped-mnemonic hazard is
	//      only the DE register forms -- so st0 = mem / st0.
	//    * fOOW never touches memory: it lives in %st(1) across all three
	//      products (`fmul %st(1),%st` twice and `fmulp` the third time,
	//      which is what pops it).  A value kept in st across a
	//      whole loop is a register-allocated local.
	//    * THE W ROW IS COMPUTED FIRST.  It has to be: the reciprocal is
	//      the operand the other three multiply by.  So the source cannot
	//      have the divide after the x/y/z statements.
	//    * THE LOOP GUARD AND BACK EDGE ARE SIGNED -- `jle` at 0x100280bb
	//      and `jg` at 0x1002816c -- where the other three loops in this
	//      function are `test ; je` / `dec ; jne`.  That is
	//      exactly the contrast between these two functions: "a
	//      `for (i = 0; i < nVerts; i++)` on a SIGNED count would guard
	//      with `jle`; the twin 0x1002803c, whose count arrives through
	//      an int32*, does exactly that at 0x100280bb".  So this loop is
	//      the counted `for` and the other three are `while (n--)`.
	//    * pData IS WALKED IN PLACE.  It is reloaded at the top of every
	//      iteration and stored back at the bottom, because its address
	//      was passed to 0x1000a700 and the stores through pDst may alias
	//      it.  A separate `pSrc = pData;` local would be register-
	//      allocated and neither reloaded nor stored; the source
	//      increments pData itself.
	//    * pPos is a LOCAL, loaded once per iteration.  Unlike the twin
	//      -- which loads pSrc->m_pPos twice because its fog call needs
	//      it again after the stores -- there is one load here and one
	//      use, so one local.
	//    * 0x14(%ebx) takes the loop-INVARIANT -1, hoisted into %edi in
	//      the preheader; 0x10(%ebx) takes the per-iteration reload of
	//      nColor.  Both are whole dwords, not bytes.
	//
	//  COIN-FLIP: the two integer stores.  The bytes write 0x14 before
	//  0x10.  They are independent stores off one base and icl scheduled
	//  the invariant one first; the source order is not recoverable from
	//  the pool (no literals) or from the call sequence (no calls).
	//  Written in EMISSION order below.  The other spelling -- diffuse
	//  then specular, which is the field order and the twin's order --
	//  predicts the same five calls.
	// ------------------------------------------------------------------ //
	nCount = *pnVerts;
	pDst   = aVerts;

	for (iVert = 0; iVert < nCount; iVert++)
	{
		pPos = pData->m_pPos;

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

		pDst->m_Specular14.m_n = 0xffffffff;
		pDst->m_Diffuse10.m_n  = nColor;

		pData++;
		pDst++;
	}

	// ------------------------------------------------------------------ //
	//  REGION 5 -- 0x10028172..0x1002819f.  THE TRIANGLE COUNT, THE
	//  OUT-PARAMETER, AND THE CLIP-MASK FORK.
	//
	//    10028172  mov -0x8c(%ebp),%eax          ; pnVerts
	//    10028178  mov 0x1008be34,%edx
	//    1002817e  mov (%eax),%eax               ; *pnVerts
	//    10028180  mov -0x90(%ebp),%ecx          ; ppVerts
	//    10028186  lea -0x2(%edx,%eax,1),%edx    ; += *pnVerts - 2
	//    1002818a  lea -0xb18(%ebp),%eax         ; aVerts
	//    10028190  mov %edx,0x1008be34
	//    10028196  mov %eax,(%ecx)               ; *ppVerts = aVerts
	//    10028198  mov 0x1008b874,%eax
	//    1002819d  test %eax,%eax
	//    1002819f  jne 0x10028280                ; -> REGION 7, the clipper
	//
	//  0x10028172 is ALSO the transform loop's zero-trip target, so both
	//  statements run even for an empty polygon -- and `g_1008be34 +=
	//  -2` on an empty one is what the shipped code does.
	//
	//  The counter is bumped on the UNCLIPPED count, here, at the top of
	//  the function, on every path including the twelve that draw nothing.
	//  The twin does the same arithmetic with the same `lea` immediately
	//  before DrawPrimitive instead.  Read off the address, not assumed.
	// ------------------------------------------------------------------ //
	g_n1008be34 += *pnVerts - 2;

	*ppVerts = aVerts;

	if (!g_n1008b874)
	{
		// -------------------------------------------------------------- //
		//  REGION 6 -- 0x100281a5..0x10028274.  NO CLIPPING WANTED: the
		//  camera's own projective 4x4, in line, and the RHW.  0xd0
		//  bytes, 63 instructions, no calls.
		//
		//    100281a5  mov -0x8c(%ebp),%eax
		//    100281af  mov (%eax),%edx           ; nLeft = *pnVerts
		//    100281b1  mov (%ecx),%eax           ; pWalk = *ppVerts
		//    100281b3  test %edx,%edx
		//    100281b5  je 0x10029734             ; -> the draw
		//    100281bb  flds 0x1008ba94 ; fmuls 0x8(%eax)     ]
		//    100281c4  flds 0x1008ba90 ; fmuls 0x4(%eax)     ]
		//    100281cd  faddp                                 ]  the w row
		//    100281cf  flds 0x1008ba8c ; fmuls (%eax)        ]
		//    100281d7  faddp ; fadds 0x1008ba98              ]
		//    100281df  fdivrs 0x100a606c         ; fOOW = 1.0f / fW
		//    100281e5  <x row: flds 0x8(%eax);fmuls 0x1008ba64 ...>
		//    10028209  <y row: flds 0x1008ba74;fmuls 0x8(%eax) ...>
		//    1002822b  <z row: flds 0x1008ba7c;fmuls (%eax) ...>
		//    1002825d  fxch %st(2)
		//    1002825f  fstps (%eax)              ; x
		//    10028261  fstps 0xc(%eax)           ; m_fRHW = fOOW
		//    10028264  fstps 0x8(%eax)           ; z
		//    10028267  fstps 0x4(%eax)           ; y
		//    1002826a  add $0x20,%eax            ; pWalk++
		//    1002826d  dec %edx ; jne 0x100281bb
		//    10028274  jmp 0x10029734            ; -> the draw
		//
		//  WHAT PINS IT:
		//    * the sixteen absolute addresses 0x1008ba5c..0x1008ba98 are
		//      0x1008b880 + 0x1dc..+0x218 exactly.  big6d910.cpp's
		//      0x1006da4e block is this same loop and says so, and
		//      big24090.cpp and big70d10.cpp record the same identity.
		//      Read through a cast, as big6d910.cpp and big124a4.cpp do.
		//    * 0x100a606c dumps as 0x3f800000 = 1.0f, dumped from OUR
		//      pool.  It is the SECOND `fdivrs` in the function and the
		//      second loaded literal in the pool, in that order -- which
		//      is INSTRUMENT 3(a)'s statement-order evidence that this
		//      block is written after REGION 4 and before the clipper.
		//    * THE RESULTS GO THROUGH THREE SCALAR TEMPORARIES, and the
		//      bytes force it: every read of the source position is a
		//      `(%eax)` / `0x4(%eax)` / `0x8(%eax)` and the LAST of them
		//      (0x1002824e) is fifteen bytes BEFORE the FIRST store
		//      (0x1002825f).  The loop transforms the vertex IN PLACE, so
		//      a source that assigned straight to `pWalk->m_vPos.x` would
		//      make the y and z rows read the value just written and icl
		//      could not hoist those loads above the store.  Three
		//      register-allocated floats (they never touch memory -- the
		//      census has no fstpl/fldl spill anywhere) plus the four
		//      assignments at the end is the only shape that emits this.
		//    * FOUR stores, not three plus a copy.  big6d910.cpp's copy
		//      of this loop puts the three results in a `Vec` local and
		//      assigns the whole vector, which emits `rep movsl $3`
		//      (0x1006db2c) plus one `fstps`; ours has no `rep movsl`
		//      anywhere -- the census has none -- and four `fstps` off
		//      one base, which is three scalar temporaries and four
		//      componentwise assignments.  The known pattern: "a whole-vector copy
		//      adjusts the base; a componentwise copy indexes it".  A
		//      real difference from that sibling, recorded rather than
		//      copied.
		//    * the store ORDER x, w, z, y is the x87 stack unwinding
		//      (`fxch %st(2)` at 0x1002825d then four pops), not a source
		//      order: all four values are live in st0..st3 when the first
		//      one is stored.  So the four assignments are a COIN-FLIP in
		//      order; written x, y, z, RHW (big6d910.cpp's order), and no
		//      call moves under any permutation.
		//    * `test ; je` then `dec ; jne`, on the same `int32*` count
		//      REGION 4 guarded with `jle`: this loop is `while (n--)`
		//      and that one is a counted `for`.  Two different loop forms
		//      over the same value, and the bytes are explicit about it.
		//
		//  OPERAND ORDER INSIDE THE PRODUCTS varies by row here (w, y and
		//  z load the matrix and multiply by the vertex; x loads the
		//  vertex and multiplies by the matrix).  The same mixture, in
		//  the same rows, appears in REGION 4 and in the twin's copy of
		//  REGION 4, which is what says it is icl's scheduling and not a
		//  source fact.  Written uniformly, as big6d910.cpp writes it.
		// -------------------------------------------------------------- //
		pM = (XForm28030*)g_1008b880;

		pWalk = *ppVerts;
		nLeft = (uint32)*pnVerts;

		while (nLeft--)
		{
			fOOW = 1.0f / (pWalk->m_vPos.x * pM->m_f20c
			             + pWalk->m_vPos.y * pM->m_f210
			             + pWalk->m_vPos.z * pM->m_f214
			             + pM->m_f218);

			fX = (pWalk->m_vPos.x * pM->m_f1dc
			    + pWalk->m_vPos.y * pM->m_f1e0
			    + pWalk->m_vPos.z * pM->m_f1e4
			    + pM->m_f1e8) * fOOW;

			fY = (pWalk->m_vPos.x * pM->m_f1ec
			    + pWalk->m_vPos.y * pM->m_f1f0
			    + pWalk->m_vPos.z * pM->m_f1f4
			    + pM->m_f1f8) * fOOW;

			fZ = (pWalk->m_vPos.x * pM->m_f1fc
			    + pWalk->m_vPos.y * pM->m_f200
			    + pWalk->m_vPos.z * pM->m_f204
			    + pM->m_f208) * fOOW;

			pWalk->m_vPos.x = fX;
			pWalk->m_vPos.y = fY;
			pWalk->m_vPos.z = fZ;
			pWalk->m_fRHW   = fOOW;

			pWalk++;
		}

		goto L_Draw;
	}

	// ------------------------------------------------------------------ //
	//  REGION 7 -- 0x10028280..0x100282d9.  THE CLIP CLASSIFY PASS, THE
	//  RE-READ, AND THE UseD3DClip SHORTCUT.
	//
	//    10028280  mov -0x8c(%ebp),%edx
	//    10028286  mov (%edx),%edx               ; nLeft = *pnVerts
	//    10028288  mov (%ecx),%ebx               ; pWalk = *ppVerts
	//    1002828a  test %edx,%edx
	//    1002828c  je 0x100282bd
	//    1002828e  mov %edx,%esi
	//    10028290  mov $0x1008b880,%edi          ; hoisted, BY ADDRESS
	//    100282a0  add $-8,%esp ; mov %edi,0x4(%esp) ; mov %ebx,(%esp)
	//    100282aa  call 0x1000d9b0               ; CALL 3
	//    100282af  add $0x20,%ebx ; add $8,%esp ; dec %esi ; jne
	//    100282b8  mov 0x1008b874,%eax           ; RE-READ
	//    100282bd  mov %eax,-0xc0(%ebp)          ; nClipFlags
	//    100282c3  mov 0x1008d7a0,%edx
	//    100282c9  test %edx,%edx ; je 0x100282dc
	//    100282cd  and $0x1,%eax
	//    100282d0  mov %eax,-0xc0(%ebp)
	//    100282d6  je 0x100296ef                 ; -> REGION 16
	//
	//  Three things to read off it:
	//    * THE RE-READ AT 0x100282b8 IS SKIPPED WHEN THE LOOP IS EMPTY,
	//      in which case %eax still holds the value REGION 5 tested.  So
	//      the source really does read the global a second time after the
	//      loop, and icl proved it unchanged on the no-call path only.
	//      The twin reaches the same conclusion from a `mov $0x3f,%eax`
	//      that is dead unless the loop is skipped, and big572b4.cpp and
	//      big6d910.cpp both record the identical pair.  We have the
	//      stronger form of the evidence, because our first read is a
	//      real load rather than a constant.
	//    * TWO stores to -0xc0 (0x100282bd and 0x100282d0) -- the
	//      assignment and then the `&= 1`.  big051a0.cpp calls out the
	//      same two stores at 0x100051b4/cc.
	//    * `mov $0x1008b880,%edi` hoisted out of the loop: the transform
	//      object is taken by ADDRESS.
	//
	//  The `je 0x100296ef` jumps past the entire clipper AND past the
	//  `*ppVerts = pVerts ; *pnVerts = nVerts` write-back at 0x100296d3:
	//  that is the inlined driver's `return LTTRUE` under UseD3DClip.
	//  The reject paths (0x10029730 `xor %edx,%edx`) are its LTFALSE.
	// ------------------------------------------------------------------ //
	pWalk = *ppVerts;
	nLeft = (uint32)*pnVerts;

	while (nLeft--)
	{
		sub_1000d9b0(pWalk, g_1008b880);
		pWalk++;
	}

	if (!Clip32_1000f4b0(g_n1008b874, ppVerts, pnVerts))
		return 0;


	// ------------------------------------------------------------------ //
	//  REGION 16 -- 0x100296ef..0x10029728.  THE SCREEN PROJECTION.
	//
	//    100296ef  mov -0x8c(%ebp),%eax      <- REGION 7's UseD3DClip `je`
	//    100296f5  mov -0x90(%ebp),%edx
	//    100296fb  mov (%eax),%eax           ; nLeft = *pnVerts
	//    100296fd  mov (%edx),%ebx           ; pWalk = *ppVerts
	//    100296ff  test %eax,%eax ; je 0x10029734
	//    10029703  mov %eax,%esi
	//    10029705  mov $0x1008b880,%edi
	//    10029710  add $-8,%esp ; two movs ; call 0x10014580   ; CALL 4
	//    1002971f  add $0x20,%ebx ; add $8,%esp ; dec %esi ; jne
	//    10029728  jmp 0x10029734
	//
	//  BOTH out-parameters are re-read through their pointers here, which
	//  is what the two stores of REGION 15 are for -- and on the
	//  UseD3DClip path they still hold what REGION 5 put there.
	//  `test ; je` then `dec ; jne`: `while (nLeft--)`, like REGION 6 and
	//  REGION 7 and unlike REGION 4.
	// ------------------------------------------------------------------ //
	pWalk = *ppVerts;
	nLeft = (uint32)*pnVerts;

	while (nLeft--)
	{
		sub_10014580(pWalk, g_1008b880);
		pWalk++;
	}

L_Draw:

	// ------------------------------------------------------------------ //
	//  REGION 17 -- 0x10029730..0x1002978f.  DRAW, COUNT, RETURN.
	//
	//    10029730  xor %edx,%edx            ; the TWELVE rejects' answer
	//    10029732  jmp 0x1002977f
	//    10029734  add $0xffffffe8,%esp     ; 24 bytes: SIX dwords
	//    10029737  mov 0x1008d988,%eax
	//    1002973c  movl $0x1c4,0x8(%esp)    ; FVF
	//    10029744  mov -0x90(%ebp),%edx
	//    1002974a  movl $0x6,0x4(%esp)      ; D3DPT_TRIANGLEFAN
	//    10029752  mov (%edx),%edx          ; *ppVerts
	//    10029754  mov %eax,(%esp)          ; this
	//    10029757  mov %edx,0xc(%esp)       ; vertices
	//    1002975b  mov -0x8c(%ebp),%edx
	//    10029761  mov (%edx),%edx          ; *pnVerts
	//    10029763  movl $0x0,0x14(%esp)     ; flags
	//    1002976b  mov %edx,0x10(%esp)      ; count
	//    1002976f  mov (%eax),%eax
	//    10029771  call *0x64(%eax)         ; CALL 5, INDIRECT
	//    10029774  incl 0x1008be5c
	//    1002977a  mov $0x1,%edx
	//    1002977f  mov %edx,%eax            ; the merged return value
	//    10029781  mov (%esp),%ebx
	//    10029784  mov 0x4(%esp),%esi
	//    10029788  mov 0x8(%esp),%edi
	//    1002978c  mov %ebp,%esp
	//    1002978e  pop %ebp
	//    1002978f  ret
	//
	//  THE DrawPrimitive AREA IS SIX DWORDS (`add $-0x18`), which is the
	//  slot's six parameters: this, type, FVF, vertices, count, flags --
	//  and all six are written, so the prototype is not a guess.  It is
	//  never closed with an `add`: the epilogue's `mov %ebp,%esp`
	//  swallows it.
	//
	//  ONE EPILOGUE AND ONE MERGED RETURN.  The known pattern: "a single shared
	//  epilogue means the conditions were OR-merged, or the ifs nest
	//  around one return".  Here it is the second: the twelve `return 0`
	//  sites (six `pIn == pEnd` at 0x10028344, 0x1002868e, 0x100289e6,
	//  0x10028d2f, 0x10029080, 0x100293cd and six `nInside == 0` at
	//  0x1002837c, 0x100286ca, 0x10028a1b, 0x10028d68, 0x100290bb,
	//  0x1002940a) all `je 0x10029730`, which materialises 0 in %edx and
	//  falls into the one `mov %edx,%eax`.  The `xor` block sits between
	//  the last plane and the draw, sunk behind a 6-byte lea-nop pad at
	//  0x1002972a, which is where icl puts a block referenced only by
	//  backward-jumping predecessors.
	//
	//  TWELVE BRANCH SITES, SIX SOURCE STATEMENTS.  The source below has
	//  ONE `return 0;` per plane, the `nInside == 0` one.  The other six
	//  are icl's: each plane's classify loop is ROTATED, so its pre-test
	//  `cmp <pEnd>,<pIn> ; je` handles the empty-polygon case, and on
	//  that path nInside is provably still 0 -- so icl aims the peeled
	//  test straight at the same `return 0` instead of at the loop exit.
	//  big299a8.cpp records the same twelve-for-six at its own twelve
	//  sites.  Six extra guards written out by hand would give twelve
	//  statements AND twelve `test`s, which is not what ships: the census
	//  has 24 plane tests, four per plane, not five.
	//
	//  `incl 0x1008be5c` is the frame counter, and it is INSIDE the
	//  drawn path only -- the rejects skip it.  big124a4.cpp, big6d910.cpp
	//  and bigb910.cpp all record the same increment immediately after
	//  their own DrawPrimitive.
	// ------------------------------------------------------------------ //
	g_pD3DDevice->lpVtbl->DrawPrimitive(g_pD3DDevice, D3DPT_TRIANGLEFAN,
	                                    D3DFVF_CLIPVERTEX, *ppVerts,
	                                    (uint32)*pnVerts, 0);

	g_n1008be5c++;

	return 1;
}


// *********************************************************************** //
//  REGION 18 -- 0x10029790..0x1002988f.  THE SIX SUNK EPSILON ARMS.
//
//  No source of its own: these are the `else fT = 0.0f;` arms of the six
//  `if (fDen < -1e-5f || fDen > 1e-5f)` above, which /O2 sinks below the
//  epilogue.  Each is
//
//      flds <+1e-5> ; fcomp %st(k) ; fnstsw ; sahf
//      jb   <back into the divide>            ; fDen > +1e-5 after all
//      fstp %st(0) x2 or x3                   ; drop the operands
//      flds <0.0f>
//      jmp  <back into the lerp>
//
//  and every one of them jumps BACKWARDS into the body, which is why the
//  256 bytes are not a separate function:
//
//      0x10029790 -> 0x10028455 / 0x1002845f    plane 1   2 fstp
//      0x100297b0 -> 0x100287ac / 0x100287b4    plane 2   3 fstp
//      0x100297e0 -> 0x10028afe / 0x10028b06    plane 3   3 fstp
//      0x10029810 -> 0x10028e4c / 0x10028e54    plane 4   3 fstp
//      0x10029840 -> 0x1002919e / 0x100291a6    plane 5   3 fstp
//      0x10029870 -> 0x100294e5 / 0x100294ef    plane 6   2 fstp
//
//  2+3+3+3+3+2 = 16 is the `fstp` row of the census, and the six `jmp`
//  out of them are six of the function's ten.  Planes 1 and 6 need one
//  fewer because their numerators are `fsubrs` against a global and so
//  leave one fewer value on the stack than planes 2..5's fchs/faddp and
//  fchs/fsubrp pairs.  The seven-byte `lea 0x0(%esi,%eiz,1),%esi` /
//  `lea 0x0(%edi,%eiz,1),%edi` pairs at 0x100297d2, 0x10029802,
//  0x10029832 and 0x10029862 are icl aligning each arm to 0x10; they are
//  8 of the 20 lea-nop pads in the census.
//
//  The sense of each test, read off plane 1 and identical in all six:
//    in line at 0x10028444, `flds -1e-5 ; fcomp %st(1) ; jbe <arm>` --
//    taken when -1e-5 <= fDen, so the fall-through (the divide) needs
//    fDen < -1e-5, the FIRST term of the `||`;
//    in the arm at 0x10029790, `flds +1e-5 ; fcomp %st(1) ; jb <divide>`
//    -- taken when +1e-5 < fDen, the SECOND term.  Getting either
//    backwards inverts the guard, so both were read against the stack
//    state rather than off the mnemonic.
//
// *********************************************************************** //


// *********************************************************************** //
//  THE ONE INDIRECT CALL, AND WHAT PINS ITS TARGET
// *********************************************************************** //
//  The call-sequence comparison matches indirect calls BY POSITION ONLY, so it confirms that
//  one of them happens and where, and nothing about what it reaches.
//
//  0x10029771  `mov 0x1008d988,%eax ; mov (%eax),%eax ; call *0x64(%eax)`
//      A VTABLE SLOT AT A KNOWN OFFSET on a named object.  0x1008d988 is
//      g_pD3DDevice; big124a4.cpp, big572b4.cpp, big26728.cpp and
//      big299a8.cpp all fix +0x64 as IDirect3DDevice7::DrawPrimitive
//      (index 25).  Pinned twice over here: the SIX dwords in the
//      outgoing area match the slot's six parameters exactly, and the
//      literals 6 (D3DPT_TRIANGLEFAN) and 0x1c4
//      (XYZRHW|DIFFUSE|SPECULAR|TEX1) are only meaningful as
//      DrawPrimitive's first two.  0x1c4 also has to be the FVF of the
//      0x20-byte vertex the whole function has been building, and it is.
//
//  BLOCK PLACEMENT.  It does not sit in a sunk cold block, so the
//  "equal counts on both sides, all indirect" pattern should NOT appear
//  here.  There is nothing else indirect in the function.
//
// *********************************************************************** //
//  THERE ARE NO `// UNDECODED:` GAPS, AND HERE IS WHY THAT IS A CLAIM
//  AND NOT AN OMISSION
// *********************************************************************** //
//  Every one of the 1,630 instructions in 0x10028030..0x1002988f is in
//  the census above and every census row is accounted for by the source
//  above.  The one place where a gap could hide is the interior of clip
//  planes 1..6, which were read at the gate, the counter, the classify, the
//  epsilon pair, the denominator, the numerator, the position stores and
//  the tail in every plane, but not instruction by instruction through
//  all thirty byte lerps.  That interior is closed mechanically instead:
//
//    grep movzbl:  0x10(%esi) x6  0x10(%ebx) x6
//                  0x11(%esi) x6  0x11(%ebx) x6
//                  0x12(%esi) x6  0x12(%ebx) x6
//                  0x13(%esi) x6  0x13(%ebx) x6
//                  0x17(%esi) x6  0x17(%ebx) x6      = 60, nothing else
//    grep the stores: `mov %dl,0x10(%eax)` x6, 0x11 x6, 0x12 x6,
//                  0x13 x6, 0x17 x6                  = 30, and there is
//                  no OTHER byte store in the function at all
//    and the thirty sites appear at ASCENDING addresses in the SAME
//    order in every plane, 0x47 bytes apart every time:
//        plane 1  0x1002849e 12, 0x100284e5 11, 0x1002852c 10,
//                 0x10028573 13, 0x100285ba 17
//        plane 2  0x100287f6 / 883d / 8884 / 88cb / 8912
//        plane 3  0x10028b3f / b86 / bcd / c14 / c5b
//        plane 4  0x10028e90 / ed7 / f1e / f65 / fac
//        plane 5  0x100291e5 / 922c / 9273 / 92ba / 9301
//        plane 6  0x1002952e / 9575 / 95bc / 9603 / 964a
//    -- 0x12, 0x11, 0x10, 0x13, 0x17, i.e. red, green, blue, alpha,
//    specular-alpha, six times.  There is no room in those counts for a
//    sixth byte, a reordering, or a plane that does something different,
//    and the twelve `fstps 0x18(%eax)` / `fstps 0x1c(%eax)` close the
//    texture pair the same way.  The six 32-byte copies are closed by
//    the eight `mov 0xK(%ebx),%eax ; mov %eax,0xK(%edi)` pairs at
//    K = 0x00,0x04,0x08,0x0c,0x10,0x14,0x18,0x1c in each of
//    0x100283db, 0x1002872b, 0x10028a7b, 0x10028dcb, 0x1002911b,
//    0x1002946b -- 48 loads and 48 stores, no more.
//
// *********************************************************************** //
//  WHAT COULD NOT BE PINNED
// *********************************************************************** //
//  (1) THE EIGHT UNREFERENCED 1.0f IN THE LITERAL POOL -- 0x100a6060,
//      0x100a6068, and 0x100a6074/7c/84/8c/94/9c (one per clip plane,
//      each immediately after that plane's -1e-5).  Nothing in the whole
//      image loads any of them (checked with a full objdump, not just
//      this function's range).  What CAN now be said, which neither
//      big051a0.cpp nor big299a8.cpp could, is that the count is exactly
//      ONE PER FLOAT DIVISION: this function has eight divides (two
//      transforms plus six planes) and eight unreferenced 1.0f, and the
//      two extra ones sit immediately BEFORE the two `fdivrs` literals
//      they belong to, in pool order.  That reading also disposes of the
//      "trailing unreferenced 1.0f" all three pools end with -- under it,
//      0x100a60d0 is not our trailer but the LEADING member of the
//      twin's transform-divide pair (whose loaded member is 0x100a60d4).
//      It looks like a reciprocal-multiply candidate icl materialises for
//      every `/` and usually discards, but that is not proved as the
//      mechanism and no source change is available to test it from here.
//      A build of this file will emit its own pool; comparing that pool
//      against 0x100a6060..0x100a60cc is the cheapest test of the clip
//      source's shape that exists, and it should be run.
//
//  (2) 0x1000a700's SOURCE RECORD BEYOND +0x00.  The stride is 0x18 and
//      only +0x00 is read HERE -- unlike the twin, which also reads +0x04
//      and +0x08 as u and v.  Declared as 20 bytes of padding after the
//      pointer rather than importing the twin's field names into a
//      function that does not touch them.
//
//  (3) THE OBJECT BEYOND +0x28.  Our bytes read `pObj->m_p28` and hand
//      pObj itself to 0x1000a700, which reads +0x4c/+0x50/+0x52/+0x54 out
//      of it.  Those four are d3dbatchd.cpp's, not ours, and are not
//      modelled.
//
//  (4) WHY THE DIFFUSE COLOUR IS AN ADDRESS.  The reading is forced --
//      0x1008cac4 is g_CV_ShowSplits and both arms store a pointer value
//      into the vertex's diffuse dword -- but it is unknown what palette
//      convention makes an address a sensible colour, and the caller at
//      0x10027910 immediately overwrites the low three bytes
//      (`m_b = 0; m_g = 0; m_r = 255;` in big26728.cpp), leaving only the
//      ALPHA byte of whatever we wrote.  So on that path the statement is
//      nearly dead, and on the other two callers' paths it is not.  What
//      the colour MEANS is not decoded; what it IS, is.
//
//  (5) THE THREE UNREFERENCED FRAME GAPS (28 bytes at -0xc24, 16 at
//      -0xec, 44 at -0xbc, 128 at -0x88 and the dword at -0x4).  They
//      balance the frame arithmetic exactly and no instruction touches
//      them, so they are icl's alignment and its spare spill space; it
//      is unknown which locals of the original they were reserved for.
//      Likewise -0xcc, which takes six stores of pPrev and is never
//      read, and -0xc4, twelve of whose fifteen references are stores:
//      both are spills icl emitted and did not use.
//
// *********************************************************************** //
//  THE REMAINING GENUINE COIN-FLIPS, both spellings given
// *********************************************************************** //
//  (a) THE DEBUG COLOUR'S ARM ORDER (REGION 2).  Written
//          if (g_CV_ShowSplits) nColor = (uint32)pObj;
//          else                 nColor = (uint32)pObj->m_p28;
//      which puts the ShowSplits arm inline and jumps over it to the
//      else, matching 0x10028066's `je 0x10028070` and the `jmp` at
//      0x1002806e.  The alternative is
//          nColor = (uint32)pObj->m_p28;
//          if (g_CV_ShowSplits) nColor = (uint32)pObj;
//      which emits the load unconditionally and needs no `jmp`.  Neither
//      arm makes a call, so the call sequence cannot settle it; only the block
//      layout differs.
//
//  (b) THE TWO INTEGER STORES IN REGION 4.  Written specular then
//      diffuse, which is emission order (0x1002814f before 0x10028152).
//      The other spelling is diffuse then specular -- the field order,
//      and the order the twin's single store implies.  Two independent
//      stores off one base with a loop-invariant value in one of them:
//      icl schedules the invariant first whichever way it is written, so
//      this may not be recoverable at all.  Same five calls either way.
//
//  (c) THE FOUR ASSIGNMENTS AT THE END OF REGION 6.  Written x, y, z,
//      RHW.  The bytes store x, RHW, z, y, which is the x87 stack
//      unwinding after `fxch %st(2)` and not a source order -- all four
//      values are live in st0..st3 when the first is stored, so every
//      permutation of the four statements emits the same four `fstps`.
//
//  (d) THE `if (!g_n1008b874) { ... goto L_Draw; }` SPELLING (REGIONS
//      5/6).  Written as a positive guard with a `goto`, which is
//      big6d910.cpp's proven spelling for the identical fork
//      (0x1006da3b) and matches 0x1002819f's `jne` to the clip path plus
//      0x10028274's `jmp` to the draw.  The alternative is
//          if (g_n1008b874) { ...the clipper... }
//          else             { ...the inline projection... }
//      which icl lays out with the clipper inline instead.  REGION 6
//      makes NO calls, so the call sequence is identical under both --
//      this one really is free, and it is recorded only because the
//      question is whether either arm makes a call, and here neither does.
//
//  (e) THE ORDER OF THE THREE STATEMENTS IN EACH PLANE'S TAIL.  Written
//          nIn  = (int32)(pOut - pOutStart);
//          pIn  = pOutStart;
//          pOut = pOut + nIn;
//      following big299a8.cpp.  Planes 1..5 emit the `pIn` store BEFORE
//      the subtraction (0x10028632 against 0x1002863a) and plane 6 emits
//      it AFTER (0x100296c4 against 0x100296c2) -- the same source, two
//      schedules, which is what proves the order is not source-visible.
//
//  (f) THE STATEMENT ORDER IN REGION 5.  Written `g_n1008be34 += ...`
//      then `*ppVerts = aVerts;`, which is emission order (0x10028190
//      before 0x10028196).  Both are stores to unrelated locations with
//      their operands computed by the two `lea`s before them, so icl
//      could have ordered them either way.
//
// *********************************************************************** //
//  WHAT CAME FROM THESE BYTES AND WHAT CAME FROM THE TWIN
// *********************************************************************** //
//  FROM THIS FUNCTION'S OWN BYTES, with nothing taken on trust:
//    * the whole extent, including the four-byte refutation of the size
//      the row carried, and the argument COUNT (four) from the three
//      argument loads plus 0x14(%ebp);
//    * the signature, cross-checked against THREE call sites (two of
//      which were not known before) rather than one;
//    * the entire frame decomposition, and with it aVerts[40] and
//      abInside[50];
//    * the whole census, every row decomposed by operand, including the
//      shl/add pair that kills plane 6's `pOut += nIn`;
//    * the pool run, dumped from 0x100a6060..0x100a60d0 with every load
//      site located and every unreferenced word checked against a full
//      objdump of the image;
//    * REGION 2 (the ShowSplits colour) -- absent from the twin and
//      MIS-READ by the twin's diff, which called both stores 0xffffffff;
//    * REGION 3's `sub_100232b0(g_n1008d7d0)` -- the twin has a texture
//      BIND with an early return here, we have an UNBIND with no test;
//    * REGION 5's early triangle-count update, where the twin's is last;
//    * REGION 6 in its entirety -- the inline projective transform and
//      the `if (!g_n1008b874)` fork, which the twin does not have;
//    * REGION 4's signed loop form against the other three loops'
//      `while (n--)`;
//    * the return value and the twelve `return 0` sites, where the twin
//      returns void;
//    * every plane's classify, epsilon, denominator, numerator and
//      position stores, re-derived from the stack state at each site
//      (including both `de f1` and `de ea` readings, re-verified with
//      `as` in this working directory);
//    * the write-back's two entry points and the projection loop.
//
//  TAKEN FROM THE TWIN AND THEN CONFIRMED HERE:
//    * that the transform loop is the same 4x4-with-divide -- confirmed
//      instruction by instruction, operand by operand, against
//      0x10029a87..0x10029b11, and the "26 instructions" corrected to 43;
//    * the six 0xdc clip blocks and their offsets -- confirmed at the
//      six `lea` sites;
//    * the 0x18 source stride -- confirmed at 0x10028161;
//    * the `lea -0x2` triangle count -- confirmed in form, refuted in
//      position;
//    * the 40-vertex bound -- confirmed off our own `sub $0xc30`;
//    * the seven unreferenced 1.0f -- confirmed present, in the same
//      positions, and extended to eight with an explanation.
//
//  TAKEN FROM THE TWIN AND NOT INDEPENDENTLY CONFIRMABLE HERE:
//    * 0x1000a700's body (the 36 bytes big299a8.cpp dumped).  Our site
//      only shows three arguments and their types; the branch inside it
//      on g_1008cb84 is that file's reading, and this file relies on it
//      only for the fact that the third argument is an out-parameter.
//    * that +0x28 of the object is a POINTER rather than a plain dword.
//      These bytes use only its VALUE.  big26728.cpp and big299a8.cpp
//      both dereference it, so it is declared as a pointer; if that is
//      wrong, only the cast in REGION 2 changes.
//
//  TAKEN FROM big6d910.cpp (a SECOND sibling):
//    * the expression-level spelling of REGION 6, and the identification
//      of 0x1008ba5c as 0x1008b880 + 0x1dc.  Both are confirmed here:
//      the sixteen addresses are consecutive at 4-byte spacing from
//      0x1008ba5c, and the arithmetic is exact.  Where that file's copy
//      of the same loop differs from ours -- its `rep movsl $3` against
//      our four `fstps` -- ours is written from ours.
//
// *********************************************************************** //
//  ONE BONUS FINDING FOR WHOEVER TAKES THE NEIGHBOURS
// *********************************************************************** //
//  0x10025680 (32 bytes, 0x10025680..0x1002569f) is a complete, trivial
//  wrapper that nobody has decoded and that this file's call-site work
//  hands over for free:
//
//      void sub_10025680(Obj28030* pObj)
//      {
//          RVert28030* pVerts;
//          int32       nVerts;
//
//          sub_1002803c(pObj, &pVerts, &nVerts, &g_mat1008ba9c);
//      }
//
//  -- `sub $0x1c,%esp`, the four arguments, `add $0x1c,%esp ; ret`, and
//  the result discarded.  0x1008ba9c = 0x1008b880 + 0x21c.  The other
//  caller, 0x10025820, is the deferred-polygon list flush: it walks a
//  singly-linked list from 0x1008d4bc, publishes each record's +0x10 into
//  g_1008b874, calls us with the record's +0x00, and pushes the record
//  onto the free list at 0x1008d4fc.  Both belong with whatever TU owns
//  0x1008d4bc.
//
// *********************************************************************** //


// ======================================================================= //
//  0x10025680   32 bytes.  Draw one polygon through the world transform.
//
//  The /Qip stack entry `mov 0x4(%esp),%eax`, then the call into
//  0x1002803c's REGISTER entry (pObj in %eax, the two locals' addresses
//  in %edx/%ecx, 0x1008ba9c in the one stack slot), result discarded.
//  0x1008ba9c = 0x1008b880 + 0x21c.  Callers 0x100406da and 0x10066a65.
// ======================================================================= //
extern uint32 g_1008ba9c;                               // 0x1008ba9c

void sub_10025680(Obj28030* pObj)
{
	RVert28030* pVerts;
	int32       nVerts;

	sub_10028030(pObj, &pVerts, &nVerts, (Mat28030*)&g_1008ba9c);
}


// ======================================================================= //
//  0x10025820   109 bytes.  Flush the deferred-polygon list.
//
//  Moved here from w4_10023fb0.cpp: its call at 0x10025859 enters
//  0x1002803c's /Qip register entry, so it lives in 0x10028030's TU.
//  Walks the pending list at 0x1008d4bc, publishes each record's clip
//  flags into 0x1008b874, draws its polygon, and pushes the record onto
//  the free list at 0x1008d4fc.
// ======================================================================= //
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

void sub_10025820()
{
	Elem24024*  pElem;
	Elem24024*  pNext;
	RVert28030* pVerts;
	int32       nVerts;

	pElem = g_1008d4bc;
	while (pElem)
	{
		pNext = pElem->m_pNext;
		g_n1008b874 = pElem->m_nClipFlags;
		sub_10028030(pElem->m_pObj, &pVerts, &nVerts,
		             (Mat28030*)&g_1008ba9c);
		pElem->m_pNextFree = g_1008d4fc;
		g_1008d4fc = pElem;
		pElem = pNext;
	}

	g_1008d4bc = LTNULL;
}
