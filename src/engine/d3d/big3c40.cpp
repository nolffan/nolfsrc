// *********************************************************************** //
//
//  MODULE:   BIG_3C40 (.CPP)
//
//  PURPOSE:  ONE function -- 0x10003c40 .. 0x1000519a, 5,466 bytes of code
//            (+ 6 bytes of lea-nop padding to 0x100051a0).  d3d.ren's
//            SIX-PLANE HOMOGENEOUS POLYGON CLIPPER for the **32-byte
//            ONE-texture-pair vertex**, standing on its own: the same
//            source as 0x100051a0 (src/engine/d3d/big051a0.cpp), which is
//            the 40-byte TWO-texture-pair instantiation of it.
//
//            ZERO calls in the whole 5,466 bytes.
//
//  GROUND TRUTH is the retail d3d.ren.
//
//  ------------------------------------------------------------------- //
//  0. WHAT IS FROM THIS FUNCTION'S BYTES AND WHAT IS FROM THE TWIN.
//     Read this before anything else.  (A twin-assisted decode
//     is only worth its cost if the distinction is explicit.)
//  ------------------------------------------------------------------- //
//
//  CONFIRMED FROM 0x10003c40'S OWN BYTES (every region below):
//    * the extent, the one `ret`, the one prologue, all 68 branch targets;
//    * the whole frame (`sub $0x714`, every slot dumped -- section 4b);
//    * all six mask bits and all six classify tests, by address;
//    * all six denominators, numerators, epsilon tests and result stores,
//      instruction by instruction, at the addresses cited per region;
//    * the vertex stride (32) and the field offsets (+0x00..+0x1c);
//    * the five byte lerps and their order (+0x12,+0x11,+0x10,+0x13,+0x17)
//      at all thirty sites;
//    * the two texture lerps (+0x18,+0x1c) at all twelve sites;
//    * THIS function's own float pool, dumped from 0x100a40cc..0x100a4128
//      (NOT copied from the twin's pool at 0x100a412c);
//    * the full instruction census, decomposed by operand, no residue;
//    * the six sunk epsilon arms and the six sunk `return 0` arms;
//    * the writeback and the single epilogue.
//
//  TAKEN FROM THE TWIN, NOT RE-DERIVED HERE (two items only):
//    (T1) THE CALLING CONVENTION AND THE ARGUMENT NAMES.  big051a0.cpp
//         read the ABI off the two indirect call sites 0x10006d31 and
//         0x10007b11 that reach 0x100051a0 through `0x440(%edx)`.  Those
//         sites are in the OTHER arm of the same dispatcher, so they do
//         not literally call 0x10003c40.  What IS confirmed here from
//         this function's own bytes is the shape: `%ecx` spilled to
//         -0xc0 and masked with $1, `%edx` spilled to -0x90 and
//         dereferenced, `0x8(%ebp)` dereferenced, `ret $0x4`, result in
//         %eax.  That is the same __fastcall shape; the twin supplies the
//         reading that the pushed dword is `int32* pnVerts` and that %edx
//         is `Vert** ppVerts`.  Both are also forced by the writeback
//         (`mov %eax,(%edx) ; mov %ebx,(%ecx)` at 0x1000504e).
//    (T2) THE MEANING (not the shape) OF THE 30 BYTE LERPS' FIELD NAMES.
//         +0x10..+0x13 as a D3D diffuse BGRA union and +0x17 as the
//         specular alpha is the twin's naming (and d3dr3a.cpp's).  This
//         function's bytes prove only the OFFSETS and the ORDER.  The
//         FVF word at the install site (below) independently proves
//         DIFFUSE and SPECULAR are both present, so the naming is
//         corroborated but not re-derived.
//
//  Everything else below was read off 0x10003c40 and is cited by address.
//
//  ------------------------------------------------------------------- //
//  1. THE EXTENT.  THE STATED ROW IS RIGHT -- unusually.
//  ------------------------------------------------------------------- //
//
//  Claimed: `0x10003c40, 5,472 bytes` => end 0x100051a0 exclusive.
//  0x100051a0 - 0x10003c40 = 0x1560 = 5,472.  Evidence that this is ONE
//  function and that the row is correct (contrast big051a0.cpp, whose row
//  spanned five functions):
//
//    * ONE `ret` in the range: 0x10005067, `ret $0x4` (c2 04 00).  A scan
//      of every decoded instruction in 0x10003c40..0x1000519a finds no
//      other `ret` and no `leave`.
//    * ONE `push %ebp ; mov %esp,%ebp` prologue, at 0x10003c40 itself.
//      The NEXT prologue in the image is 0x100051a0 -- the twin -- and it
//      sits behind lea-nop padding:
//        1000519a  8d b6 00 00 00 00   lea 0x0(%esi),%esi   (6 bytes)
//        100051a0  55                  push %ebp
//      So the function's LAST instruction is 0x10005195 `jmp 0x10005057`,
//      ending at 0x1000519a; 0x1000519a..0x1000519f is padding; the
//      claimed end 0x100051a0 is therefore correct as a row boundary and
//      the CODE extent is 0x10003c40..0x1000519a = **5,466 bytes**.
//    * EVERY branch target is inside the range and on an instruction
//      boundary.  87 branches, 68 DISTINCT targets, min 0x10003c7e, max
//      0x10005193; a set-membership test of all 68 against the 1,414
//      decoded instruction addresses came back with zero misses.
//      (Sixty-eight distinct targets is the same number big051a0.cpp
//      reports for its own body -- the same control-flow graph.)
//    * the call count over 0x10003c40..0x1000519a
//        => 5,466 bytes, **0 calls (0 direct, 0 indirect)**.
//      With the padding included (0x100051a0) it is still 0 calls.
//      There is no second function hiding in the range to own any calls.
//
//  ------------------------------------------------------------------- //
//  2. HOW IT IS REACHED, AND THE INDEPENDENT PROOF OF THE TWO DIFFERENCES.
//  ------------------------------------------------------------------- //
//
//  Nothing calls 0x10003c40 directly.  A scan of the whole image for the
//  literal dword 0x10003c40 finds exactly ONE occurrence, at file offset
//  0x8a8d, which is the immediate of
//
//      10008a87  c7 83 40 04 00 00 40 3c 00 10
//                movl $0x10003c40,0x440(%ebx)
//
//  and 0x10008a40 is a two-armed setup routine.  Its two arms are the
//  whole story of this function:
//
//      10008a66  cmp  $0x1,%edx                    ; %edx = 0x428(%ebx)
//      10008a69  jne  0x10008ab5
//    ARM A (the 32-byte format):
//      10008a71  movl $0x20,0x438(%ebx)            ; **STRIDE 32**
//      10008a7d  movl $0x10003bb0,0x43c(%ebx)
//      10008a87  movl $0x10003c40,0x440(%ebx)      ; **THIS FUNCTION**
//      10008a91  movl $0x1c4,0x444(%ebx)           ; **FVF 0x1c4**
//    ARM B (the 40-byte format):
//      10008abb  movl $0x28,0x438(%ebx)            ; stride 40
//      10008ac7  movl $0x10003bf0,0x43c(%ebx)
//      10008ad1  movl $0x100051a0,0x440(%ebx)      ; the twin
//      10008adb  movl $0x2c4,0x444(%ebx)           ; FVF 0x2c4
//
//  0x1c4 = D3DFVF_XYZRHW|DIFFUSE|SPECULAR|**TEX1**;
//  0x2c4 = D3DFVF_XYZRHW|DIFFUSE|SPECULAR|**TEX2**.
//  16 + 4 + 4 + 8 = 32 and 16 + 4 + 4 + 16 = 40.  So BOTH claimed
//  differences -- stride 32 and one texture pair -- are proved a second
//  time, from OUTSIDE the function, by the dispatcher that installs it.
//
//  ------------------------------------------------------------------- //
//  3. THE CLAIMED DIFFERENCES FROM THE TWIN, EACH CHECKED.
//  ------------------------------------------------------------------- //
//
//  (C1) "byte-identical to 0x100051a0 for its first 0x60 bytes".
//       **CONFIRMED, with one two-byte qualification.**  A byte-for-byte
//       diff of 0x10003c40 against 0x100051a0 over 0x120 bytes returns
//       exactly this set of differing offsets: {0x3a, 0x3b} and then
//       everything from 0x61.  So:
//         +0x00..+0x39  identical
//         +0x3a..+0x3b  DIFFER -- but they are the low half of a RELATIVE
//                       DISPLACEMENT: `e9 d9 13 00 00` (jmp 0x10005057)
//                       against `e9 16 15 00 00` (jmp 0x100066f4).  Same
//                       instruction, same meaning, different exit address.
//         +0x3c..+0x60  identical
//         +0x61 onward  diverge for real
//       and +0x60 is the FIRST STRUCTURAL DIFFERENCE, one byte wide:
//         10003ca0  8b 00   mov (%eax),%eax     ; pIn -> %eax
//         10005200  8b 08   mov (%eax),%ecx     ; pIn -> %ecx
//       which is exactly D4 below.  The prologue, the three callee-saved
//       spills, both nClip spills, the g_n1008d7a0 early-out, the
//       `mov $0x1,%edx ; jmp <exit>` and the whole four-store
//       initialisation are byte-identical.  The "0x60" in the claim is
//       right to the byte; the claim just does not mention that the jmp
//       displacement inside it cannot be identical.
//
//  (C2) "vertex stride 32 rather than 40: `shl $0x5` x17 and `add $0x20`
//       x6 each".  **CONFIRMED, and the x17 is exact.**  Census:
//         shl $0x5,%edx  x6   (pEnd  = pIn + nIn,  one per pass)
//         shl $0x5,%ebx  x6   (pPrev = pIn + nIn-1, one per pass)
//         shl $0x5,%ecx  x5   (pOut  = pOut + nIn, passes 1..5 ONLY)
//                        ---
//                        17
//       and `add $0x20,%edx` x6 (pWalk++ classify), `add $0x20,%esi` x6
//       (pWalk++ edge), `add $0x20,%eax` x6 (pOut++ after the copy),
//       plus `mov $0x20,%ecx ; add %eax,%ecx` x6 (pOut++ after the lerp).
//       There is NO `lea (%eax,%eax,4)` / `lea (%ecx,%eax,8)` anywhere:
//       `lea` is 31 here against the twin's 60, and 25 of the 31 are
//       lea-nop padding.  See 4a.
//
//  (C3) "ONE texture pair: fmul 48 = 6x8, fsubs/fadds 28, fstps 56".
//       **CONFIRMED EXACTLY.**  fmul 48 (all `fmul %st(1),%st`), fmulp 6,
//       fsubs 28, fadds 28, fstps 56.  By operand:
//         flds  0x18(%esi) x6 + 0x1c(%esi) x6  = 12 texture loads
//         fsubs 0x18(%ebx) x6 + 0x1c(%ebx) x6  = 12 texture differences
//         fadds 0x18(%ebx) x6 + 0x1c(%ebx) x6  = 12 texture bases
//         fstps 0x18(%eax) x6 + 0x1c(%eax) x6  = 12 texture stores
//       There is NO reference to +0x20 or +0x24 of any vertex pointer
//       anywhere in the function.  The twin has 24 of each.
//
//  (C4) "the byte rows are IDENTICAL: movzbl 60, fildl 60, fistpl 30".
//       **CONFIRMED EXACTLY** -- movzbl 60, fildl 60, fistpl 30, and the
//       thirty `mov %dl,0xNN(%eax)` stores at NN = 0x12,0x11,0x10,0x13,
//       0x17, six of each.
//
//  (C5) "it is d3d.ren's six-plane clipper reached through a
//       function-pointer slot rather than called directly."
//       **CONFIRMED** -- see section 2.
//
//  ------------------------------------------------------------------- //
//  3b. FURTHER DIFFERENCES FROM THE TWIN.
//  ------------------------------------------------------------------- //
//
//  (D1) **THE SIX DIVISIONS ARE `sar $0x5`, NOT `idiv`.**  The twin's
//       census has `idiv 6 / cltd 6` for the six
//       `nIn = (pOut - pOutStart) / 40`.  THIS function has **no `idiv`
//       and no `cltd` at all**; it has `sar $0x5` x6 instead:
//         10003fc2  mov %eax,%ecx ; sub %edx,%ecx ; sar $0x5,%ecx
//       at 0x10003fc6, 0x1000430e, 0x10004657, 0x100049a8, 0x10004cfd
//       and (on %eax) 0x10005034.  Stride 32 is a power of two and a
//       pointer difference is an exact multiple of the stride, so icl
//       emits a bare arithmetic shift with no sign-bias -- there is no
//       `add $31` and no `and $31` anywhere.  Same C statement, different
//       codegen; it is recorded because it removes two whole census rows.
//
//  (D2) **THE `pOut = pOut + nIn` DEATH IN PASS 6 SHOWS UP IN A DIFFERENT
//       ROW.**  The twin proves it with `lea` 40 splitting 12/6/12/10.
//       Here the same fact falls out of `shl`: six `shl $0x5,%edx`, six
//       `shl $0x5,%ebx`, but only **five** `shl $0x5,%ecx` -- and the
//       five sit immediately after the first five `sar $0x5` (0x10003fcf,
//       0x10004317, 0x10004660, 0x100049b1, 0x10004d06) while the sixth
//       `sar` at 0x10005034 is followed straight by
//       `mov %eax,-0xd4(%ebp) ; mov %edx,%eax` and the writeback.  Also
//       `add %eax,%ecx` = 11 = 6 (pOut++ after lerp) + 5 (pOut += nIn).
//       INDEPENDENT CONFIRMATION of the twin's finding, in a row the twin
//       does not use.
//
//  (D3) **PASS 1 READS nClip OUT OF THE PARAMETER REGISTER, NOT THE
//       FRAME.**  The twin has six `mov -0xc0(%ebp),%eax ; test $B,%al`.
//       Here pass 1 is `mov %ecx,%edx ; test $0x1,%dl` (0x10003ca2 --
//       %ecx still holds the incoming argument) and only passes 2..6 do
//       `mov -0xc0(%ebp),%edx ; test $B,%dl` (5 sites).  Census row
//       `mov -0xc0(%ebp),%edx` = 5, not 6.  Pure register allocation.
//
//  (D4) **pIn LIVES IN %eax, NOT %ecx.**  The twin says pIn is %ecx
//       across all six passes and that the writeback reads it from %ecx.
//       Here it is %eax: `mov (%eax),%eax` at 0x10003ca0 loads it,
//       `mov %edx,%eax` at the end of each tail re-seats it
//       (0x10003fd4, ..., 0x1000503d) and the writeback is
//       `mov %eax,(%edx)` at 0x1000504e.  Register allocation only.
//
//  (D5) **THE CLASSIFY FLAG USES %edi IN PASS 1 AND %esi IN PASSES 2..6.**
//       `mov $0x1,%edi` x1 (0x10003cfb) + `mov $0x1,%esi` x5, with
//       `mov $0x0,%eax` x6 and the matching `mov %eax,%edi` / `%eax,%esi`.
//       The twin reports `mov $0x1,%edi`/`mov $0x1,%esi` too, so this is
//       the same shape; noted for the census arithmetic.
//
//  (D6) **THE COPY IS EIGHT DWORDS, NOT TEN.**  `mov 0xNN(%ebx),%eax ;
//       mov %eax,0xNN(%edi)` for NN = 0x04,0x08,0x0c,0x10,0x14,0x18,0x1c
//       (7 offsets x 6 passes = 42 + 42) plus `mov (%ebx),%ecx ;
//       mov %ecx,(%edi)` x6 = **48 loads + 48 stores**.  The highest
//       offset touched by the copy anywhere in the function is 0x1c.
//       That is the stride-32 consequence of the twin's note 5(c): the
//       struct assignment is UNROLLED, not `rep movsl` (there is no
//       `rep movsl` and no `cld` in the function).
//
//  (D7) **ONE `nop`, not two** (0x1000436b).  Alignment only.
//
//  (D8) **THIS FUNCTION HAS ITS OWN FLOAT POOL, and it DOES carry the six
//       dead 1.0f.**  See section 4c -- dumped, not inherited.
//
//  ------------------------------------------------------------------- //
//  4. THE ACCEPTANCE FIGURE AND WHAT IT DOES *NOT* WITNESS.  READ FIRST.
//  ------------------------------------------------------------------- //
//
//  **THE CALL-SEQUENCE COMPARISON IS COMPLETELY BLIND ON THIS FUNCTION.
//  CALL-FREE COVERAGE IS 5,466 OF 5,466 BYTES = 100.0%.**  The count is
//  `0 calls (0 direct, 0 indirect)`, so the check is vacuous here.  It proves NOTHING
//  and must not be quoted as a figure.
//
//  There are **NO `// UNDECODED:` GAPS** -- every byte of
//  0x10003c40..0x1000519a is covered by a region below, and the six
//  padding bytes 0x1000519a..0x1000519f are named.
//
//  What pins the decode is the other three instruments.  All three
//  came out clean and all three were computed from THIS function.
//
//  ------------------------------------------------------------------- //
//  4a. AN INSTRUCTION CENSUS WITH NO RESIDUE.
//  ------------------------------------------------------------------- //
//
//  Over all 5,466 bytes / 1,414 instructions.  EVERY mnemonic in the
//  function is in the table and every row is accounted for.
//
//   | mnemonic  | shipped | what the structure below requires |
//   |-----------|---------|-----------------------------------|
//   | fmul      |   48    | 6 passes x 8 uses of fT (2 position + 2 texture + 4 of the 5 byte lerps) |
//   | fmulp     |    6    | the 5th byte lerp of each pass (operand order forces the pop form) |
//   | fildl     |   60    | 6 x 5 byte lerps x 2 (integer difference, integer base) |
//   | fistpl    |   30    | 6 x 5 byte lerps x 1 |
//   | movzbl    |   60    | the same 30 byte lerps x 2 widenings; NOTHING else widens a byte |
//   | fcomps    |    6    | the six classify tests, one per plane |
//   | fcomp     |   12    | 6 x `fDen < -1e-5` + 6 x `fDen > 1e-5` |
//   | fnstsw/sahf |18/18  | those 6 + 12, one pair each |
//   | fdivp     |    6    | one divide per pass |
//   | **sar**   |  **6**  | 6 x `nIn = (pOut - pOutStart)` over stride 32 -- see D1 |
//   | **shl**   | **17**  | 6 pEnd + 6 pPrev + 5 pOut+=nIn -- see C2, D2 |
//   | idiv/cltd |  0/0    | ABSENT.  The twin has 6/6.  See D1. |
//   | fsub      |    6    | the six `pWalk->? - pPrev->?` that OPEN each fDen |
//   | fsubs     |   28    | 6 x 4 lerp differences (2 position + 2 texture) + 4 fDen tails (passes 2,3,4,5) |
//   | fadds     |   28    | 6 x 4 lerp bases + the same 4 fDen tails |
//   | faddp     |   32    | 30 byte-lerp adds + 2 numerators (passes 2 and 5) |
//   | fsubrs    |    2    | `g_f1008b908 - pPrev->z` (pass 1, 0x10003de7) and `g_f1008b910 - pPrev->z` (pass 6, 0x10004e57) |
//   | fsubrp    |    2    | passes 3 and 4's `pPrev->y - pPrev->z` / `pPrev->x - pPrev->z` (0x1000447e, 0x100047cc) |
//   | fchs      |    8    | 2 classify negations (2,5) + 4 numerator negations (2,3,4,5) + 2 result negations (2,5) |
//   | fld       |    2    | passes 2 and 5 duplicating fVal before negating it |
//   | fsts      |    2    | pass 3's `y = fVal` (0x1000449d) and pass 4's `x = fVal` (0x100047ef) |
//   | fstps     |   56    | 26 destination stores + 30 byte-lerp float temps |
//   | fstp      |   16    | the six sunk epsilon arms' x87 cleanup: 2+3+3+3+3+2 |
//   | fxch      |   14    | 6 (the 5th byte lerp) + 8 (1+2+1+1+2+1 in the numerators/results) |
//   | flds      |   94    | decomposed below |
//   | lea       |   31    | 25 lea-nop pads + 6 real -- decomposed below |
//   | inc       |   12    | 6 x `g_n1008be44++` + 6 x `i++` |
//   | dec       |    6    | 6 x `j = nIn - 1` |
//   | xor       |   18    | 6 x `nInside = 0` + 6 x `i = 0` + 6 x `return 0` |
//   | test      |   25    | 1 (g_n1008d7a0) + 6 (nClip & BIT) + 6 (nInside==0) + 6 (nIn<=0) + 6 (abInside[j]) |
//   | cmp       |   30    | 6 (pIn==pEnd guard) + 6 (nInside!=nIn) + 6 (pWalk!=pEnd) + 6 (abInside[j]!=abInside[i]) + 6 (i<nIn) |
//   | je        |   37    | 1 + 6 x 6 |
//   | jne       |    7    | 1 (the nClip early-out) + 6 classify back-edges |
//   | jmp       |   13    | 1 early `return 1` + 6 epsilon arms + 6 reject arms |
//   | jae       |    1    | pass 1's classify (`z >= near`) |
//   | jbe       |    7    | 6 in-line epsilon low tests + pass 6's classify (`z <= far`) |
//   | jb        |   10    | 4 classify (passes 2,3,4,5) + 6 sunk-arm high tests |
//   | jl / jle  |   6/6   | the six edge loops' `i < nIn` and the six `nIn <= 0` guards |
//   | sub       |   37    | 1 (`sub $0x714,%esp`) + 30 byte-lerp integer differences + 6 `pOut - pOutStart` |
//   | add       |   53    | decomposed below |
//   | and       |    1    | `nClip &= 1` under g_n1008d7a0 |
//   | mov       |  533    | decomposed below |
//   | ret/push/pop | 1/1/1| the single epilogue |
//   | nop       |    1    | alignment (0x1000436b) |
//
//  **THE TABLE SUMS TO EXACTLY 1,414, WHICH IS THE NUMBER OF DECODED
//  INSTRUCTIONS IN 0x10003c40..0x1000519a.  THERE IS NO RESIDUE: every
//  instruction in the function is in a row above, and every row is
//  explained by the structure below.**
//
//  48 = 6x8, 60 = 6x5x2, 30 = 6x5, 28/28/32 and 12 texture sites are the
//  load-bearing rows: **six clip planes, each with two position LERPs,
//  TWO texture LERPs and five byte LERPs, and nothing else.**  A seventh
//  plane, a missing plane, a third position LERP, a SECOND texture pair
//  or a sixth colour byte would each move at least three of those rows,
//  and none of them moves.
//
//  `flds` 94 decomposes with NOTHING over, BY OPERAND:
//      0x8(%edx) x10 = 4 classify (1,2,5,6) + 2 fDen opens (1,6)
//                      + 4 numerator pPrev->z loads (2,3,4,5)
//      0x4(%edx) x3  = 1 classify (3) + 2 fDen opens (3,5)
//      (%edx)    x3  = 1 classify (4) + 2 fDen opens (2,4)
//      0x8(%esi) x6  = 2 fDen opens (1,6) + 4 z-lerps (2,3,4,5)
//      (%esi)    x6  = 4 x-lerps (1,3,5,6) + 2 fDen opens (2,4)
//      0x4(%esi) x6  = 4 y-lerps (1,2,4,6) + 2 fDen opens (3,5)
//      0x18(%esi) x6 + 0x1c(%esi) x6  = 12 texture lerps  (twin: 24)
//      0x100a40cc+8k x6  + 0x100a40fc..0x100a4128 x12     = 18 constants
//      -0x700(%ebp)..-0x618(%ebp) x30 = the byte-lerp float temps
//                                                        ---- 94 exactly.
//  (The twin's 106 minus the twelve texture loads this vertex does not
//   have is exactly 94.)
//
//  `lea` 31 = **25 lea-nop pads + 6 real**, and the 6 real are ALL
//  `lea -0xNNN(%ebp),%ecx`, one per pass -- the six `pFlag = abInside`:
//      -0x614 -0x538 -0x45c -0x380 -0x2a4 -0x1c8.
//  The pads are 10 x `lea 0x0(%esi,%eiz,1),%esi`, 9 x
//  `lea 0x0(%edi,%eiz,1),%edi`, 4 x `lea 0x0(%esi),%esi`, 2 x
//  `lea 0x0(%edi),%edi`.  The twin's other 34 real leas are the
//  stride-40 address arithmetic; here they are `shl`+`add`.
//
//  `add` 53 decomposes with nothing over:
//       6 x `add $0x20,%edx`  pWalk++ in the classify loop
//       6 x `add $0x20,%esi`  pWalk++ in the edge loop
//       6 x `add $0x20,%eax`  pOut++ after the copy
//       6 x `add $0x4,%ecx`   pFlag++
//       6 x `add %eax,%edx`   pEnd  = pIn + (nIn<<5)
//       6 x `add %eax,%ebx`   pPrev = pIn + ((nIn-1)<<5)
//       5 x `add %esi,%ebx` + 1 x `add %edi,%ebx`   nInside += nFlag
//      11 x `add %eax,%ecx` = 6 pOut++ after the lerp + 5 pOut += nIn
//                                                        ---- 53 exactly.
//
//  `sub` 37 decomposes with nothing over:
//       1 x `sub $0x714,%esp`
//      24 x `sub %edx,%eax` + 6 x `sub %ebx,%eax`  = the 30 byte-lerp
//            integer differences (the 5th of each pass uses %ebx)
//       5 x `sub %edx,%ecx` + 1 x `sub %edx,%eax`  = the six
//            `pOut - pOutStart`                     ---- 37 exactly.
//
//  `mov` 533 = 216 STRUCTURAL + 317 bookkeeping:
//      48 copy loads  (7 x `mov 0xNN(%ebx),%eax` x6 + `mov (%ebx),%ecx` x6)
//      48 copy stores (7 x `mov %eax,0xNN(%edi)` x6 + `mov %ecx,(%edi)` x6)
//      60 `mov %eax,-0xe8(%ebp)` x30 + `%edx,-0xe8` x24 + `%ebx,-0xe8` x6
//      30 `mov -0xNNN(%ebp),%dl`   the 30 int slots
//      30 `mov %dl,0xNN(%eax)`     the 30 byte stores
//     ---
//     216
//  and the 317 bookkeeping movs land in rows of exactly 42/12/11/7/6/5/4/2
//  plus 35 singletons; the pass-indexed ones are:
//      42 x `mov -0xd0(%ebp),%eax`  = 30 byte-lerp pOut reloads + 6 copy
//                                     + 6 tail
//      12 x `mov -0xd4(%ebp),%edx`  = 6 pEnd + 6 `nInside != nIn`
//      12 x `mov %eax,%esi`, 7 x `mov %eax,%edi`  register seats
//      11 x `mov -0xd0(%ebp),%ecx`  = 6 lerp entry + 5 `pOutStart = pOut`
//      11 x `mov %ecx,-0xd0(%ebp)` + 7 x `mov %eax,-0xd0(%ebp)` = 18 pOut
//            stores = 6 after copy + 6 after lerp + 5 tail + 1 init
//       6 x `mov 0x1008be44,%ecx` / `mov %ecx,0x1008be44`
//       6 x `mov %eax,-0x98(%ebp)` / `mov -0x98(%ebp),%eax` (pIn spill)
//       6 x `mov %edi,-0x8(%ebp)`  / `mov -0x8(%ebp),%edi`  (%edi spill)
//       6 x `mov $0x0,%eax` (classify flag 0)
//       6 x `mov $0x20,%ecx` (the /O2 `pOut++` after the lerp)
//       6 x `mov -0xd4(%ebp),%eax` (the six `i < nIn`)
//       6 x `mov -0xcc(%ebp),%edx` (the six tails' pOutStart)
//       5 x `mov %ecx,-0xd4(%ebp)` + 1 x `mov %eax,-0xd4(%ebp)` = 6 nIn
//       5 x `mov %ecx,-0xcc(%ebp)` + 1 x `mov %eax,-0xcc(%ebp)` -- FIVE
//            in-pass `pOutStart = pOut` (passes 2..6) plus ONE in the
//            initialisation.  **PASS 1 HAS NONE** -- the twin's note 5(d)
//            reproduced here, from this function's own bytes.
//       5 x `mov -0xc0(%ebp),%edx` + 1 x `mov %ecx,%edx`
//       5 x `mov $0x1,%esi`  + 1 x `mov $0x1,%edi`
//       2 x `mov %ecx,-0xc0(%ebp)`  the two nClip stores  (COIN-FLIP 3)
//       2 x `mov $0x1,%edx`         the two `return LTTRUE`
//       2 x `mov %edx,0x8(%ecx)`    the two INTEGER z-stores of the plane
//                                   globals (0x10003e0f, 0x10004e7f)
//      12 x `mov -0xNNN(%ebp,%edx,4),%eax`  = abInside[j], twice per pass
//                                   (once for the copy test, once after)
//      35 singletons: the prologue (5), the initialisation (8), the
//            writeback (5), the epilogue (5), the six-arm register seats
//            and the two plane-global loads.
//
//  ------------------------------------------------------------------- //
//  4b. FRAME ARITHMETIC THAT DECOMPOSES EXACTLY.
//  ------------------------------------------------------------------- //
//
//  `sub $0x714,%esp` (0x10003c43) = 1,812 bytes -- the SAME frame as the
//  twin, slot for slot, DUMPED here from this function's own operands (a
//  sort -u of every `-0xNNN(%ebp)` in the 5,466 bytes):
//
//    -0x714 .. -0x709   12   the three callee-saved spills, written
//                            `mov %edi,0x8(%esp)` / `%esi,0x4(%esp)` /
//                            `%ebx,(%esp)` at 0x10003c49..0x10003c53 and
//                            reloaded at 0x10005059..0x10005064.
//    -0x708 .. -0x701    8   alignment; nothing addresses it.
//    -0x700 .. -0x615  236   THIRTY 8-byte float temps stepping by 8 from
//                            -0x700 to -0x618 -- DUMPED, all thirty
//                            distinct, none repeated, 29 x 8 + 4 = 236
//                            exactly, and -0x614 is the next object.
//                            Each is used ONCE, as
//                            `fstps -0xNNN(%ebp) ; flds -0xNNN(%ebp) ;
//                             fistpl <int slot>` -- 6 passes x 5 byte
//                            lerps.  First: 0x10003e54/5a/60.  Last:
//                            0x10004fe2/e8/ee (-0x618 -> -0xf0).
//    -0x614 .. -0x0ed 1320   SIX x 0xdc, the six clip passes' state.  The
//                            flag arrays are -0x614, -0x538, -0x45c,
//                            -0x380, -0x2a4 and -0x1c8, EXACTLY 0xdc
//                            apart, each indexed `-0xNNN(%ebp,%edx,4)`
//                            and `-0xNNN(%ebp,%edi,4)`.
//                            0xdc = 0xc8 + 0x14 = **50 int32** of flags
//                            plus five int slots.  All thirty int slots
//                            were DUMPED and land exactly at array+0xc8:
//                              -0x614 -> -0x54c -0x548 -0x544 -0x540 -0x53c
//                              -0x538 -> -0x470 -0x46c -0x468 -0x464 -0x460
//                              -0x45c -> -0x394 -0x390 -0x38c -0x388 -0x384
//                              -0x380 -> -0x2b8 -0x2b4 -0x2b0 -0x2ac -0x2a8
//                              -0x2a4 -> -0x1dc -0x1d8 -0x1d4 -0x1d0 -0x1cc
//                              -0x1c8 -> -0x100  -0xfc  -0xf8  -0xf4  -0xf0
//                            The bound FIFTY and the count FIVE are read
//                            off THIS frame, not assumed.
//                            NOTE: 0xdc does NOT change with the vertex
//                            stride -- the flag array is int32[50] either
//                            way -- which is why this block is identical
//                            to the twin's despite the 32/40 difference.
//    -0x0ec              4   dead; nothing addresses it.
//    -0x0e8              4   the integer scratch every byte lerp stages
//                            `movzbl ; movzbl ; sub` through (60 writes).
//    -0x0d4 .. -0x0cc   12   nIn, pOut, pOutStart (in that frame order).
//    -0x0c8, -0x0c4      8   the edge loop's `i` and the pIn/pWalk spills.
//    -0x0c0              4   nClip.
//    -0x098              4   pIn's spill across the classify loop.
//    -0x090              4   the ppVerts argument.
//    -0x008              4   %edi's spill across the 32-byte copy.
//
//  The thirty 8-byte temps landing EXACTLY against the six 0xdc clip
//  blocks, and the six blocks' five int slots landing exactly at +0xc8
//  with the next array at +0xdc, is again the strongest single piece of
//  evidence: it pins the flag-array bound (50) and the byte-lerp count
//  (five per pass, thirty in all) with no call anywhere to lean on.
//
//  ------------------------------------------------------------------- //
//  4c. THIS FUNCTION'S OWN FLOAT POOL -- DUMPED, NOT INHERITED.
//  ------------------------------------------------------------------- //
//
//  0x10003c40 has its OWN pool, immediately BELOW the twin's (the twin's
//  starts at 0x100a412c).  Dumped straight out of the retail d3d.ren:
//
//    100a40cc  ac c5 27 b7  -9.999999747e-06      pass 1 low epsilon
//    100a40d0  00 00 80 3f   1.0f                 **DEAD**
//    100a40d4  ac c5 27 b7  -9.999999747e-06      pass 2
//    100a40d8  00 00 80 3f   1.0f                 **DEAD**
//    100a40dc  ac c5 27 b7  -9.999999747e-06      pass 3
//    100a40e0  00 00 80 3f   1.0f                 **DEAD**
//    100a40e4  ac c5 27 b7  -9.999999747e-06      pass 4
//    100a40e8  00 00 80 3f   1.0f                 **DEAD**
//    100a40ec  ac c5 27 b7  -9.999999747e-06      pass 5
//    100a40f0  00 00 80 3f   1.0f                 **DEAD**
//    100a40f4  ac c5 27 b7  -9.999999747e-06      pass 6
//    100a40f8  00 00 80 3f   1.0f                 **DEAD**
//    100a40fc  ac c5 27 37  +9.999999747e-06      sunk arm, pass 6
//    100a4100  00 00 00 00   0.0f                 sunk arm, pass 6
//    100a4104  +1e-5   100a4108  0.0f             sunk arm, pass 5
//    100a410c  +1e-5   100a4110  0.0f             sunk arm, pass 4
//    100a4114  +1e-5   100a4118  0.0f             sunk arm, pass 3
//    100a411c  +1e-5   100a4120  0.0f             sunk arm, pass 2
//    100a4124  +1e-5   100a4128  0.0f             sunk arm, pass 1
//
//  * The six low epsilons are read in FORWARD pass order (0x10003dd4,
//    0x10004118, 0x1000446a, 0x100047b8, 0x10004b0a, 0x10004e44) and the
//    six high/zero PAIRS in REVERSE (pass 1 at 0x1000506a reads the
//    HIGHEST pair 0x100a4124/0x100a4128; pass 6 at 0x10005150 reads the
//    LOWEST 0x100a40fc/0x100a4100).  Exactly the twin's pattern.
//  * **THE SIX DEAD 1.0f ARE HERE TOO.**  0x100a40d0 + 8k, k = 0..5, all
//    `00 00 80 3f`, interleaved with the six -1e-5, and referenced from
//    NOWHERE: the `flds` operand census above lists every constant load
//    in the function and not one of the six appears.  That is the FOURTH
//    function (after big5a960, big051a0 and this) to carry them.  See
//    NOT PINNED (1).
//  * 0x100a40c0..0x100a40c8 are three zero dwords belonging to whatever
//    precedes; nothing in this function loads them.
//
//  ------------------------------------------------------------------- //
//  4d. EXPRESSION-BY-EXPRESSION CORRESPONDENCE WITH THE TWIN.
//  ------------------------------------------------------------------- //
//
//  Every one of the following was RE-READ off 0x10003c40's bytes at the
//  address cited and then compared with big051a0.cpp.  All agree.
//
//    - the six mask bits are **0x01, 0x04, 0x08, 0x10, 0x20, 0x02 IN THAT
//      ORDER** -- 0x10003ca4 `test $0x1,%dl`, 0x10003fe2 `test $0x4,%dl`,
//      0x1000432a `test $0x8,%dl`, 0x10004673 `test $0x10,%dl`,
//      0x100049c4 `test $0x20,%dl`, 0x10004d19 `test $0x2,%dl`;
//    - the six classify tests, read off this function:
//        1  0x10003cf0  flds 0x8(%edx) ; fcomps 0x1008b908 ; jae
//        2  0x10004023  flds 0x8(%edx) ; fchs ; fcomps (%edx)    ; jb
//        3  0x10004370  flds 0x4(%edx) ; fcomps 0x8(%edx)        ; jb
//        4  0x100046c0  flds (%edx)    ; fcomps 0x8(%edx)        ; jb
//        5  0x10004a10  flds 0x8(%edx) ; fchs ; fcomps 0x4(%edx) ; jb
//        6  0x10004d60  flds 0x8(%edx) ; fcomps 0x1008b910 ; jbe
//      i.e.  z >= near | -z < x | y < z | x < z | -z < y | z <= far;
//    - the six denominators/numerators, re-read at 0x10003dcc, 0x1000410c,
//      0x1000445c, 0x100047ac, 0x10004afc and 0x10004e3c, including the
//      two `fsubrs` against the plane globals (0x10003de7, 0x10004e57),
//      the two `faddp` numerators (0x1000412c, 0x10004b1e) and the two
//      `fsubrp` numerators (0x1000447e, 0x100047cc);
//    - the epsilon triple and its constants -- see 4c, dumped;
//    - the six `pIn = pOutStart ; nIn = pOut - pOutStart` tails at
//      0x10003fbc, 0x10004304, 0x1000464d, 0x1000499e, 0x10004cf3 and
//      0x10005026, all `mov -0xcc(%ebp),%edx ; sub ; sar $0x5` (D1);
//    - the FIVE BYTE LERPS are +0x12, +0x11, +0x10, +0x13, +0x17 in that
//      order in ALL SIX passes -- thirty sites, all read, thirty
//      `mov %dl,0xNN(%eax)` stores counted;
//    - the TWO TEXTURE LERPS are +0x18, +0x1c in that order in all six
//      passes -- twelve sites, all read.
//
//  ------------------------------------------------------------------- //
//  5. FOUR READINGS THAT ARE THIS FUNCTION'S OWN.
//  ------------------------------------------------------------------- //
//
//  (a) THE REJECT IS `return 0;`, NOT `goto L_Reject;`.  SIX separate
//      `xor %edx,%edx ; jmp 0x10005057` blocks at 0x10005170, 0x10005177,
//      0x1000517e, 0x10005185, 0x1000518c and 0x10005193, one per pass in
//      REVERSE pass order (pass 6 lowest, pass 1 highest), each reached by
//      BOTH of its own pass's two reject branches and by no other pass's
//      -- e.g. pass 1's `je 0x10005193` at 0x10003cd5 (the empty-input
//      guard) and at 0x10003d20 (`nInside == 0`).  A shared `goto` label
//      would emit ONE block.  See COIN-FLIP (1).
//  (b) THE RESULT IS ROUTED THROUGH %edx.  Both `return 1` sites
//      (0x10003c74 early-out, 0x10005052 normal end) are `mov $0x1,%edx`
//      and all eight exits converge on `mov %edx,%eax` at 0x10005057
//      followed by ONE register restore and ONE `ret $0x4`.
//  (c) THE 32-BYTE COPY IS UNROLLED, NOT `rep movsl` -- eight dword
//      load/store pairs in every one of the six passes, opening at
//      0x10003d71, 0x100040b1, 0x10004401, 0x10004751, 0x10004aa1 and
//      0x10004de1 (`mov (%ebx),%ecx ; mov %ecx,(%edi)`) and running to
//      0x1c.  No `rep movsl` anywhere.  Same finding as
//      big5a960.cpp and big051a0.cpp for this same statement.
//  (d) PASS 1 HAS NO `pOutStart = pOut` STORE.  Passes 2..6 each emit
//      `mov %ecx,-0xcc(%ebp)` at the head of the edge loop (0x1000406d
//      and four more); pass 1 does not.  0x10003c7e loads g_p1008b87c
//      ONCE and stores it to BOTH -0xd0 (0x10003c86) and -0xcc
//      (0x10003c8e), so pass 1's `pOutStart = pOut` is value-numbered
//      away.  The statement IS in the source -- the other five prove it.
//
//  ------------------------------------------------------------------- //
//  6. COMPILER VERDICT: Intel C/C++ 4.0, **/O2**.
//  ------------------------------------------------------------------- //
//
//  FOR /O2:
//    * COLD-BLOCK SINKING BELOW THE EPILOGUE, twelve-fold: 0x1000506a..
//      0x10005199, 304 bytes after the `ret` at 0x10005067, all of it
//      proved (six epsilon arms + six reject arms, region map below).
//    * `&p->m` as `mov $K,%reg ; add %p,%reg`: all six `pOut++` at the end
//      of a lerp are `mov $0x20,%ecx ; add %eax,%ecx` (0x10003f90,
//      0x100042d8, 0x10004621, 0x10004972, 0x10004cc7, 0x10005000), never
//      `lea 0x20(%eax),%ecx`.  The /O2 pattern.
//    * `mov $0x1,%esi`/`mov $0x1,%edi` with `mov $0x0,%eax` at every one
//      of the six classify sites, never the /Os `xor ; inc` pair.
//  AGAINST (recorded, not explained away):
//    * ONE epilogue, cross-jumped to by all eight exits.  /O2 usually
//      DUPLICATES.  The same anomaly big70d10.cpp and big051a0.cpp record.
//  NOTE: the twin's third /O2 argument -- "the six divisors are HOISTED
//  INTO A REGISTER, `mov $0x28,%ebx ; cltd ; idiv %ebx`" -- HAS NO SITE
//  HERE, because stride 32 divides by shifting.  One /O2 witness
//  fewer; the other three stand.
//
//  There is no call in the function, so the outgoing-argument test
//  (`add $-N,%esp` vs `push`) has no site to look at.
//
//  One function in the file, so it wants ONE flag.  Try /O2 first,
//  /Os second.
//
//  ------------------------------------------------------------------- //
//  7. WHAT IS *NOT* PINNED.
//  ------------------------------------------------------------------- //
//
//  (1) SIX DEAD 1.0f CONSTANTS (0x100a40d0 + 8k).  Confirmed present and
//      confirmed unreferenced (4c).  Each clip pass's source mentions a
//      1.0f that icl folded away and the spelling below does not.  Now
//      seen in big5a960.cpp, big051a0.cpp and here; still unresolved.
//      The likely shape is an `fT` clamp or a `1.0f - fT` complement the
//      /O2 value numberer removed, but nothing in the bytes chooses.
//  (2) THE THIRTY `(uint8)` CASTS.  `fstps <temp> ; flds <temp> ;
//      fistpl <int slot>` with NO fnstcw/fldcw dance and no helper call at
//      any of the thirty sites (e.g. 0x10003e54/5a/60) is /QIfist.  Under
//      plain /O2 a `(uint8)(int32)f` cast emits the control-word dance and
//      those thirty sites will differ in bytes.  Nothing witnesses them
//      but the census rows.
//  (3) THE DECLARATION ORDER of nIn / pOut / pOutStart.  Frame order is
//      nIn (-0xd4) < pOut (-0xd0) < pOutStart (-0xcc) < nClip (-0xc0); the
//      four initialising STORES are emitted pOut, pOutStart, nIn, pIn
//      (0x10003c86, 0x10003c8e, 0x10003c9a, 0x10003ca0) and the source is
//      written in that statement order.  Whether the declaration order
//      that produces that frame order is the one below is not proved.
//  (4) `m_fRHW` AT +0x0c.  It is never touched by name; it is only carried
//      by the eight-dword copy (`mov 0xc(%ebx),%eax ; mov %eax,0xc(%edi)`,
//      six sites).  The NAME is the twin's/FVF's (0x1c4 = XYZRHW); the
//      only thing this function proves is that 32 bytes are copied.
//  (5) WHETHER THE PARAMETER IS `uint32` OR `DWORD`-shaped.  Only bit
//      tests and `and $1` are performed on it.
//
//  ------------------------------------------------------------------- //
//  8. COIN-FLIPS -- both spellings given, one build settles each.
//  ------------------------------------------------------------------- //
//
//  (1) THE REJECT.  Written below as a textual `return LTFALSE;` inside
//      the macro, which is what six separate `xor %edx,%edx` blocks say.
//      THE ALTERNATIVE, if that comes out with one block instead of six:
//          #define CLIP3C40_HEAD(...)  ... if (nInside == 0) goto L_Rej;
//      with, just above the epilogue,   L_Rej:  return LTFALSE;
//      (big70d10.cpp's spelling).
//  (2) THE CLASSIFY FLAG.  Written below as
//          nFlag = (INSIDE) ? 1 : 0;  nInside += nFlag;  *pFlag = nFlag;
//      which is what `mov $0x1,%esi ; mov $0x0,%eax ; jcc ; mov %eax,%esi
//      ; add %esi,%ebx ; mov %esi,(%ecx) ; add $0x4,%ecx` says
//      (0x1000402c..0x10004042).  THE ALTERNATIVE, big5a960.cpp's:
//          nInside += (abInside[i] = (INSIDE) ? 1 : 0);
//      -- but that indexes the array where this function walks a pointer
//      (`add $0x4,%ecx`), so the pointer form is written.
//  (3) THE EARLY OUT.  Written below as
//          if (g_n1008d7a0) { nClip &= 1; if (!nClip) return LTTRUE; }
//      Two `mov %ecx,-0xc0(%ebp)` stores (0x10003c54 and 0x10003c6c) say
//      nClip is assigned twice; the alternative is
//          uint32 nClip = nClipIn;  if (g_n1008d7a0) { ... }
//      The version below names the parameter `nClip` and lets icl spill
//      it, which is what the first store is.
//  (4) THE SIX DIVISIONS.  Written as `(int32)(pOut - pOutStart)`, a
//      pointer difference, which is what `sub ; sar $0x5` with NO sign
//      bias means (a pointer difference is an exact multiple, so icl may
//      shift).  THE ALTERNATIVE, if a build emits `idiv`:
//          nIn = (int32)(((uint8*)pOut - (uint8*)pOutStart) / 32);
//      -- but that would be an unsigned/`shr` form, and the shipped
//      instruction is `sar`, so the pointer-difference spelling is the
//      one written.
//
//  ------------------------------------------------------------------- //
//  9. REGION MAP.  Every byte of 0x10003c40..0x100051a0 is here.
//  ------------------------------------------------------------------- //
//
//   0x10003c40..0x10003c7d  prologue, the three register spills, the
//                           g_n1008d7a0 / `nClip &= 1` early `return 1`.
//   0x10003c7e..0x10003ca1  pOut / pOutStart / nIn / pIn initialisation.
//   0x10003ca2..0x10003fdb  PASS 1, bit 0x01, NEAR   (z >= g_f1008b908).
//   0x10003fdc..0x10004323  PASS 2, bit 0x04, LEFT   (-z < x).
//   0x10004324..0x1000466c  PASS 3, bit 0x08, TOP    (y < z).
//   0x1000466d..0x100049bd  PASS 4, bit 0x10, RIGHT  (x < z).
//   0x100049be..0x10004d12  PASS 5, bit 0x20, BOTTOM (-z < y).
//   0x10004d13..0x1000503e  PASS 6, bit 0x02, FAR    (z <= g_f1008b910).
//   0x1000503f..0x10005051  the writeback: *ppVerts = pIn, *pnVerts = nIn.
//   0x10005052..0x10005069  `return 1` and the single epilogue.
//   0x1000506a..0x1000516f  the six sunk clip-epsilon arms (`fT = 0.0f`),
//                           in FORWARD pass order (pass 1 at 0x1000506a,
//                           pass 6 at 0x10005150), reading the high/zero
//                           constant pairs in REVERSE.  Includes the
//                           lea-nop padding between them (0x1000508a,
//                           0x100050b2/b9, 0x100050e2/e9, 0x10005112/19,
//                           0x10005142/49).
//   0x10005170..0x10005199  the six sunk `return 0` arms, in reverse pass
//                           order (pass 6 lowest at 0x10005170, pass 1
//                           highest at 0x10005193).
//   0x1000519a..0x1000519f  lea-nop padding to the next function
//                           (0x100051a0 = the twin).
//
//  (The pass boundaries are the `mov -0xc0(%ebp),%edx` that reloads
//  nClip; it is both the previous pass's `nInside == nIn` join and the
//  next pass's `if (nClip & BIT)` test, so each boundary address is
//  reached from both.  Pass 1's boundary is the parameter register
//  instead -- see D3.)
//
// *********************************************************************** //

// Only ltbasedefs.h, exactly as big051a0.cpp, big5a960.cpp and
// big70d10.cpp have it.
#include "ltbasedefs.h"


// ======================================================================= //
//  THE SIX SINGLE-PLANE CLIPPERS, INLINED.  Each pass is
//  the 32-byte family member (w1_10001000.cpp's 0x10001000 near, 0x10001330
//  left, 0x10001990 right, 0x10001cb0 bottom, 0x10001fe0 far; top as
//  w1_10002310.cpp's 40-byte 0x100029e0) written as an inline over
//  (&pIn, &nIn, &pOut) -- the bodies below are theirs verbatim, so the
//  earlier per-pass macros and the notes above about /QIfist and the
//  classify shape are superseded: the byte conversions are RoundF, the
//  crossing is an inline over the vertex POSITION, nInside an int32.
//
//  NEAR, 208 bytes (was 7663 of 7967): every pass, both loops, the sunk
//  epsilon arms, the six `xor %edx,%edx ; jmp` rejects and the single
//  epilogue line up.  What gave the shape:
//    * the passes are INLINE FUNCTIONS over (&pIn, &nIn, &pOut) -- pOut
//      lives in the frame (-0xd0) because its address is taken; a macro
//      over the outer variables keeps it in a register (4719 bytes);
//    * the body is ITSELF an inline (ClipAll3c40) under a one-line
//      __fastcall wrapper: that is the result routed through %edx to ONE
//      epilogue; written directly, every `return` gets its own epilogue,
//      and a bRet variable with `goto` puts the result in the frame;
//    * the set-up reads *pnVerts and *ppVerts before g_p1008b87c (any
//      order with pOut first spills nClip and reloads it for pass 1).
//  Now IDENTICAL: the driver has its own pOutStart, assigned with pOut
//  and read first (`pOut = pOutStart = g_p1008b87c`; never read, it keeps
//  the one load in both shipped slots, and pOut first no longer spills
//  nClip), and the far pass sets pV first (CLIP3C40_VERTFIRST).
// ======================================================================= //

struct Vert3c40                                         // 32 bytes
{
	LTVector v;                                         // +0x00  x, y, z
	float  rhw;                                         // +0x0c
	uint8  b, g, r, a;                                  // +0x10..+0x13
	uint8  sb, sg, sr, sa;                              // +0x14..+0x17
	float  tu;                                          // +0x18
	float  tv;                                          // +0x1c
};

// 0x1008d7a0 -- UseD3DClip.
extern uint32			g_n1008d7a0;
// 0x1008b87c -- the clipper's output bank.
extern Vert3c40*		g_p1008b87c;
extern uint32  g_1008be44;                              // 0x1008be44  clip counter
extern float   g_1008b908;                              // 0x1008b908  near plane z
extern float   g_1008b910;                              // 0x1008b910  far plane z

#define CLIP_EPSILON    0.00001f
#define CLIP_MAX_VERTS  50

// Round to nearest in the current FPU mode; 0x10015190 out of line.
inline int32 RoundF(float f)
{
	int32 n;
	__asm fld f
	__asm fistp n
	return n;
}

// 0x10023380, the vertex lerp (out of line there, inlined here).
static inline void LerpClipVert(Vert3c40* a, Vert3c40* b, Vert3c40* dst, float t)
{
	dst->tu = (b->tu - a->tu) * t + a->tu;
	dst->tv = (b->tv - a->tv) * t + a->tv;
	dst->r  = (uint8)RoundF((b->r  - a->r)  * t + a->r);
	dst->g  = (uint8)RoundF((b->g  - a->g)  * t + a->g);
	dst->b  = (uint8)RoundF((b->b  - a->b)  * t + a->b);
	dst->a  = (uint8)RoundF((b->a  - a->a)  * t + a->a);
	dst->sa = (uint8)RoundF((b->sa - a->sa) * t + a->sa);
}

static inline float ClipEdgeNear(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeLeft(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeTop(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeRight(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeBottom(LTVector* a, LTVector* b, LTVector* dst)
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

static inline float ClipEdgeFar(LTVector* a, LTVector* b, LTVector* dst)
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

// A pass's first two set-up statements; the far pass has pV first.
#define CLIP3C40_FLAGFIRST  pFlag = anInside; pV = *ppVerts;
#define CLIP3C40_VERTFIRST  pV = *ppVerts; pFlag = anInside;

// One single-plane pass: the family member's body, verbatim.
#define CLIP3C40_PASS(NAME, INSIDE, EDGE, SETUP)                             \
static inline LTBOOL NAME(Vert3c40** ppVerts, int32* pnVerts, Vert3c40** ppOut) \
{                                                                            \
	uint32     anInside[CLIP_MAX_VERTS];                                     \
	Vert3c40*  pOutStart;                                                    \
	Vert3c40*  pV;                                                           \
	Vert3c40*  pEnd;                                                         \
	Vert3c40*  pPrev;                                                        \
	Vert3c40*  pCur;                                                         \
	uint32*    pFlag;                                                        \
	int32      nInside;                                                      \
	int32      i;                                                            \
	int32      iPrev;                                                        \
                                                                             \
	++g_1008be44;                                                            \
                                                                             \
	nInside = 0;                                                             \
	SETUP                                                                    \
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
				float t = EDGE(&pPrev->v, &pCur->v, &(*ppOut)->v);           \
                                                                             \
				LerpClipVert(pPrev, pCur, *ppOut, t);                        \
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

CLIP3C40_PASS(ClipNear3c40,   pV->v.z >= g_1008b908,  ClipEdgeNear, CLIP3C40_FLAGFIRST)
CLIP3C40_PASS(ClipLeft3c40,   -pV->v.z < pV->v.x,     ClipEdgeLeft, CLIP3C40_FLAGFIRST)
CLIP3C40_PASS(ClipTop3c40,    pV->v.y < pV->v.z,      ClipEdgeTop, CLIP3C40_FLAGFIRST)
CLIP3C40_PASS(ClipRight3c40,  pV->v.x < pV->v.z,      ClipEdgeRight, CLIP3C40_FLAGFIRST)
CLIP3C40_PASS(ClipBottom3c40, -pV->v.z < pV->v.y,     ClipEdgeBottom, CLIP3C40_FLAGFIRST)
CLIP3C40_PASS(ClipFar3c40,    pV->v.z <= g_1008b910,  ClipEdgeFar, CLIP3C40_VERTFIRST)


// ======================================================================= //
//  0x10003c40 .. 0x1000519a   (5,466 bytes, 0 calls)
//
//  __fastcall: nClip in %ecx, ppVerts in %edx, pnVerts pushed, `ret $0x4`.
// ======================================================================= //
static inline LTBOOL ClipAll3c40(uint32 nClip, Vert3c40** ppVerts,
                                 int32* pnVerts)
{
	Vert3c40*	pIn;
	Vert3c40*	pOut;
	Vert3c40*	pOutStart;
	int32		nIn;

	if (g_n1008d7a0)
	{
		nClip &= 1;

		if (!nClip)
			return LTTRUE;
	}

	pOut = pOutStart = g_p1008b87c;
	nIn  = *pnVerts;
	pIn  = *ppVerts;

	if (nClip & 0x01)
		if (!ClipNear3c40(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x04)
		if (!ClipLeft3c40(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x08)
		if (!ClipTop3c40(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x10)
		if (!ClipRight3c40(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x20)
		if (!ClipBottom3c40(&pIn, &nIn, &pOut))
			return LTFALSE;

	if (nClip & 0x02)
		if (!ClipFar3c40(&pIn, &nIn, &pOut))
			return LTFALSE;

	*ppVerts = pIn;
	*pnVerts = nIn;
	return LTTRUE;
}

LTBOOL __fastcall sub_10003c40(uint32 nClip, Vert3c40** ppVerts,
                               int32* pnVerts)
{
	return ClipAll3c40(nClip, ppVerts, pnVerts);
}
