// *********************************************************************** //
//
//  w29_10073c00_osoiqip.cpp -- d3d.ren's pixel-format converter module,
//  0x10073c00..0x10079f84: the blitter table 0x100898b0's converters, the
//  single-pixel pack/unpack table 0x100899b0's entries, the image-size
//  helper and PFormat::IsSame.  LithTech's fmconvert.cpp, as icl built it.
//
//    0x10073c00 .. 0x10074ea0   rows 0/1 (was w19_10073c00_osoiqip.cpp)
//    0x100754d0 .. 0x10076df0   rows 2/3 (was w20_100754d0_os.cpp)
//    0x10077110 .. 0x10078910   row 7 and the DXT rows (was w21_77110_os.cpp,
//                               w21_77680_os.cpp, w21_fmconvert.h)
//    0x100799b0 .. 0x10079e70   pack / unpack one pixel (was
//                               w22_100799b0_os.cpp, d3dr5c.cpp's 0x10079b90,
//                               d3dhigh.cpp's 0x10079c70 / 0x10079d70)
//    0x10079f30 / 0x10079f3c    image size, a /Qip pair (was d3dhigh.cpp)
//    0x10079f80 / 0x10079f84    PFormat::IsSame, a /Qip pair (was
//                               d3dbatcha.cpp's 0x10079f84)
//  (0x10079f70, PFormat's bytes-per-pixel lookup, is still d3dr4c.cpp's.)
//
//  WHY ONE TU (measured).  Three /Qip register calls cross the
//  old file boundaries: the copier 0x10073c08 calls 0x10079f3c; 0x100746b0,
//  0x10075920 and 0x10076df0 call 0x10073c08 (`add $-8,%esp; mov
//  %esi,%edx; call`); all eight IsSame callers call 0x10079f84, the body
//  behind the 4-byte shim 0x10079f80.  icl emits a two-entry pair, and a
//  caller entering it by register, only with callee and caller in one /Qip
//  TU.  Here: the shims 0x10079f30 and 0x10079f80 are identical and
//  0x10079f84 stays identical; 0x10075920 / 0x10076df0's copier calls come
//  out as shipped (18 -> 6 and 20 -> 6 mnemonic blocks).
//
//  FLAGS: /Os /Oi- /Qip (the _osoiqip convention).  /Os by the 0x90
//  padding, /Oi- because the copier CALLS memcpy.  Measured on each old
//  file: /Os /Oi- /Qip against /Os gives the same functions (w20 six, w21
//  four, w22 three, d3dhigh ten identical) -- the flags move nothing.
//  What does move converter bytes is the TU's CONTENT: icl's IPO inlining
//  of the static helpers depends on the whole TU (0x100740a0 16 -> 22
//  blocks, 0x10074350 21 -> 18, 0x10074a50 15 -> 10 as the pack functions
//  join), so the converters' residue is not settled per function.
//
//  ONE TYPE SET.  The four files' views were reconciled onto rows 0/1's:
//  PFormat is C10079f84 (+0x04 type, +0x08 masks, +0x18 bits, +0x28
//  shifts), the manager's to-tables are one flat `m_ToTables[9 * 256]`
//  at +0x70.  Measured: rows 2/3's `DownTables` sub-object laid over it
//  in a union costs 0x10074ea0 four blocks (15 -> 19); rewriting their
//  `Get(bits)` as `&m_ToTables[bits << 8]` costs nothing and improves
//  0x10075920 / 0x10076df0.
//
// *********************************************************************** //

// ======================================================================= //
//
//  (was w19_10073c00_osoiqip.cpp) The first row of d3d.ren's pixel-format
//  blitter table (0x100898b0, [src type][dst type], 8 per row, each
//  `uint32 (FormatMgr*, ConvertRequest*)`, cdecl; FormatMgr::ConvertPixels
//  0x1007a130 in w29_10079fd0_osoiqip.cpp dispatches through it):
//
//    0x10073c00  (8 + 492)  the same-format copy (the table's diagonal:
//                           [0][0], [4][4], [5][5], [6][6], [7][7]) -- a
//                           /Qip two-entry pair, stack entry 0x10073c00,
//                           register entry 0x10073c08;
//    0x10073e00  (668)      [0][1] 8-bit palettised -> 8-bit;
//    0x100740a0  (676)      [0][2] 8-bit palettised -> 16-bit;
//    0x10074350  (862)      [0][3] 8-bit palettised -> 32-bit;
//    0x100746b0  (925)      [1][1] 8-bit -> 8-bit;
//    0x10074a50  (1104)     [1][2] 8-bit -> 16-bit;
//    0x10074ea0  (1570)     [1][3] 8-bit -> 32-bit.
//
//  The same module as lithtech.exe's FMConvert blitters, decoded in
//  src/engine/lithtech/fmconvert.cpp (0x0045d1e0 fm_BlitSame, 0x0045d3b0,
//  0x0045d5e0, 0x0045d7d0, 0x0045da30, 0x0045dc90, 0x0045ddf0): the
//  helper objects, the loop skeleton and the guards come from there.  icl
//  inlines every helper, so the shipped d3d.ren bodies call nothing but
//  PFormat::IsSame (0x10079f84, d3dbatcha.cpp's C10079f84), the image-size
//  helper 0x10079f30 and memcpy.
//
//  FLAGS, from the bytes: 0x10073c00 CALLS memcpy (0x100805d0), so /Oi-;
//  0x100746b0 calls 0x10073c08 -- the /Qip register entry (%eax = manager,
//  %edx = request) -- so the copy and that caller share a /Qip TU; and
//  the padding after each body is 0x90 fill, not /O2's multi-byte nops.
//  Hence /Os /Oi- /Qip (the _osoiqip convention in build/engine.sh).
//
//  0x10073cc7 calls 0x10079f3c, the register entry of 0x10079f30 (the
//  image-size helper, below), with %eax/%edx/%ecx loaded; the 16/32-bit
//  converters (0x10075920, 0x10076df0) enter 0x10073c08 at its register
//  entry.  Both need the one TU this file now is.
//
//  LOOPS: every loop here is decrement-first, `while (n) { n--; ... }` --
//  the shipped `dec` at the top and `test; jne` at the bottom (as in
//  d3dr2a.cpp's 0x1005fa80 note); `for (n = N; n; n--)` fuses `dec; jne`.
//
// ======================================================================= //


typedef unsigned long  uint32;
typedef unsigned short uint16;
typedef unsigned char  uint8;
typedef uint32 PValue;

extern "C" void* __cdecl memcpy(void* pDst, const void* pSrc, unsigned int nBytes);
#pragma function(memcpy)

// 0x10089870: log2 bytes per pixel, by format type (d3ddata.cpp).
extern uint32 g_10089870[8];

// 0x10079f30 -- bytes in a (type, width, height) image (d3dhigh.cpp).
uint32 sub_10079f30(uint32 nFormat, uint32 nWidth, uint32 nHeight);

// A pixel format: +0x04 type, +0x08 the four masks (A, R, G, B), +0x18 the
// bit count of each plane, +0x28 the low bit of each plane.  0x38 bytes.
// IsSame is 0x10079f84 (thiscall, `ret $0x4`): type and four masks equal.
class C10079f84
{
public:
	unsigned int sub_10079f80(C10079f84* pOther);   // 0x10079f80 / 0x10079f84 IsSame, below

	void*   m_pVTable;                              // +0x00
	uint32  m_nType;                                // +0x04
	uint32  m_Masks[4];                             // +0x08
	uint32  m_nBits[4];                             // +0x18
	uint32  m_nShifts[4];                           // +0x28
};                                                  // 0x38

struct ConvertRequest
{
	C10079f84* m_pSrcFormat;                        // +0x00
	uint8*     m_pSrcData;                          // +0x04
	uint32     m_nSrcPitch;                         // +0x08
	uint8*     m_pSrcPalette;                       // +0x0c
	C10079f84* m_pDstFormat;                        // +0x10
	uint8*     m_pDstData;                          // +0x14
	uint32     m_nDstPitch;                         // +0x18
	uint32     m_nWidth;                            // +0x1c
	uint32     m_nHeight;                           // +0x20
	uint32     m_nFlags;                            // +0x24  bit 0: OR in
};

// Rows 2/3 and 7 and the DXT rows call the format PFormat.
typedef C10079f84 PFormat;

// The manager: +0x00 the PValue (A8R8G8B8) format -- what the guards
// compare the destination against -- +0x38 a 565 format, +0x70 nine
// 256-byte "8 bits -> n bits" tables, +0x970 nine pointers to the
// "n bits -> 8 bits" tables.
class FormatMgr
{
public:
	C10079f84 m_FormatPValue;                       // +0x000
	C10079f84 m_Format565;                          // +0x038
	uint8     m_ToTables[9 * 256];                  // +0x070  nine 256-entry tables
	uint8*    m_pFromTables[9];                     // +0x970
};

uint32 sub_10073c00(FormatMgr* pMgr, ConvertRequest* pRequest);
uint32 sub_10073e00(FormatMgr* pMgr, ConvertRequest* pRequest);
uint32 sub_100740a0(FormatMgr* pMgr, ConvertRequest* pRequest);
uint32 sub_10074350(FormatMgr* pMgr, ConvertRequest* pRequest);
uint32 sub_100746b0(FormatMgr* pMgr, ConvertRequest* pRequest);
uint32 sub_10074a50(FormatMgr* pMgr, ConvertRequest* pRequest);
uint32 sub_10074ea0(FormatMgr* pMgr, ConvertRequest* pRequest);


// The helper objects every blitter builds on its own stack (lithtech's
// FMFormatConvert / FMFormatRead / FMPaletteRead).  icl expands all of them
// -- the shipped bodies call nothing -- so they are written as inline and
// static functions over plain structs.  Two spellings are read off the
// bytes: the converter's tables are `base + (bits << 8)` (`shl $0x8` then
// `add` of a separately formed `lea 0x70(pMgr)`; array indexing gives
// `imul $0x100`), and the 8/16-bit converters narrow after EVERY term
// (`movzbl %dl` / `movzwl %dx` after each `or`), which is the sequential
// `nColor |= term` form and not one OR expression.
//
// THE FRAME.  The shipped bodies keep the source and
// destination cursors in two ADJACENT frame slots, step them in memory,
// and spill the PValue to the slot after them although nothing reads it
// back.  icl promotes scalars and struct members to registers even with
// their address taken, but not arrays: the cursors are `uint8* pCur[2]`
// and the PValue `PValue value[1]`, both at function scope (declared in
// the converter, the value slot moves below the reader's `ch[4]` and the
// frame grows a slot); the row width is loaded before the OR/store
// branch.  The no-op `and $0xffffffff` sits in the READER and the WRITER,
// not on the cursor: a palette read is unmasked, a converted pixel's
// store and every format read are masked (AT8/16/32, fr_ReadN), and the
// PValue fast paths store unmasked (0x10074350) or through AT32
// (0x10075cd0) as the bytes show.  With that, 0x10073e00, 0x100740a0,
// 0x10074a50 and 0x100754d0 have the shipped frame and the shipped first
// 84..92 bytes, 0x10076300 / 0x10076870 the first 30.  Still different:
// register naming from the first loop on (the shipped %ebx carries the
// flags, then the width, then the PValue; ours %ebp -- `test $1,%bl`
// against `test $1,%ebp`), and the down-table lookups' (index, table)
// operand order the single-pixel functions record below.  Measured
// without effect on that naming: the cursor stores before/after `y--`,
// the declaration order, `(uint8)` / `!= 0` / `1u` flag tests, the pitch
// updates' order, the palette load before/after fc_Init (worse), a
// `BlitCursor` struct or two one-element arrays for the cursors (both
// promote or reorder: 652 / 651 bytes against the array's 680).

// A pixel through a cursor: the shipped code puts a no-op `and
// $0xffffffff` on every pixel address it reads or writes.
#define AT8(p)   (*(uint8*)((uint32)(p) & ~0UL))
#define AT16(p)  (*(uint16*)((uint32)(p) & ~0UL))
#define AT32(p)  (*(uint32*)((uint32)(p) & ~0UL))

// PValue -> destination format.
struct FormatConvert
{
	uint8*     m_pTables[4];
	C10079f84* m_pFormat;
};

inline void fc_Init(FormatConvert* pConvert, FormatMgr* pMgr, C10079f84* pFormat)
{
	pConvert->m_pFormat = pFormat;
	pConvert->m_pTables[0] = (uint8*)((pFormat->m_nBits[0] << 8) + (uint32)pMgr->m_ToTables);
	pConvert->m_pTables[1] = (uint8*)((pFormat->m_nBits[1] << 8) + (uint32)pMgr->m_ToTables);
	pConvert->m_pTables[2] = (uint8*)((pFormat->m_nBits[2] << 8) + (uint32)pMgr->m_ToTables);
	pConvert->m_pTables[3] = (uint8*)((pFormat->m_nBits[3] << 8) + (uint32)pMgr->m_ToTables);
}

static uint8 fc_Convert8(FormatConvert* pConvert, PValue* pValue)
{
	uint8 nColor;

	nColor  = pConvert->m_pTables[0][*pValue >> 24]          << pConvert->m_pFormat->m_nShifts[0];
	nColor |= pConvert->m_pTables[1][(*pValue >> 16) & 0xff] << pConvert->m_pFormat->m_nShifts[1];
	nColor |= pConvert->m_pTables[2][(*pValue >> 8) & 0xff]  << pConvert->m_pFormat->m_nShifts[2];
	nColor |= pConvert->m_pTables[3][*pValue & 0xff]         << pConvert->m_pFormat->m_nShifts[3];

	return nColor;
}

static uint16 fc_Convert16(FormatConvert* pConvert, PValue* pValue)
{
	uint16 nColor;

	nColor  = pConvert->m_pTables[0][*pValue >> 24]          << pConvert->m_pFormat->m_nShifts[0];
	nColor |= pConvert->m_pTables[1][(*pValue >> 16) & 0xff] << pConvert->m_pFormat->m_nShifts[1];
	nColor |= pConvert->m_pTables[2][(*pValue >> 8) & 0xff]  << pConvert->m_pFormat->m_nShifts[2];
	nColor |= pConvert->m_pTables[3][*pValue & 0xff]         << pConvert->m_pFormat->m_nShifts[3];

	return nColor;
}

static PValue fc_Convert32(FormatConvert* pConvert, PValue* pValue)
{
	PValue value = *pValue;

	return (pConvert->m_pTables[0][value >> 24]          << pConvert->m_pFormat->m_nShifts[0]) |
	       (pConvert->m_pTables[1][(value >> 16) & 0xff] << pConvert->m_pFormat->m_nShifts[1]) |
	       (pConvert->m_pTables[2][(value >> 8) & 0xff]  << pConvert->m_pFormat->m_nShifts[2]) |
	       (pConvert->m_pTables[3][value & 0xff]         << pConvert->m_pFormat->m_nShifts[3]);
}

// Source format -> PValue.
struct FormatRead
{
	C10079f84* m_pFormat;
	uint8*     m_pTables[4];
};

inline void fr_Init(FormatRead* pRead, FormatMgr* pMgr, C10079f84* pFormat)
{
	pRead->m_pTables[0] = pMgr->m_pFromTables[pFormat->m_nBits[0]];
	pRead->m_pTables[1] = pMgr->m_pFromTables[pFormat->m_nBits[1]];
	pRead->m_pTables[2] = pMgr->m_pFromTables[pFormat->m_nBits[2]];
	pRead->m_pTables[3] = pMgr->m_pFromTables[pFormat->m_nBits[3]];
	pRead->m_pFormat = pFormat;
}

static PValue fr_Read8(FormatRead* pRead, uint8* pPos)
{
	uint32 ch[4];
	uint8* pPixel = (uint8*)((uint32)pPos & ~0UL);

	ch[0] = pRead->m_pTables[0][(*pPixel & pRead->m_pFormat->m_Masks[0]) >> pRead->m_pFormat->m_nShifts[0]];
	ch[1] = pRead->m_pTables[1][(*pPixel & pRead->m_pFormat->m_Masks[1]) >> pRead->m_pFormat->m_nShifts[1]];
	ch[2] = pRead->m_pTables[2][(*pPixel & pRead->m_pFormat->m_Masks[2]) >> pRead->m_pFormat->m_nShifts[2]];
	ch[3] = pRead->m_pTables[3][(*pPixel & pRead->m_pFormat->m_Masks[3]) >> pRead->m_pFormat->m_nShifts[3]];

	return (ch[0] << 24) | (ch[1] << 16) | (ch[2] << 8) | ch[3];
}

static PValue fr_Read16(FormatRead* pRead, uint8* pPos)
{
	uint32 ch[4];
	uint16* pPixel = (uint16*)((uint32)pPos & ~0UL);

	ch[0] = pRead->m_pTables[0][(*pPixel & pRead->m_pFormat->m_Masks[0]) >> pRead->m_pFormat->m_nShifts[0]];
	ch[1] = pRead->m_pTables[1][(*pPixel & pRead->m_pFormat->m_Masks[1]) >> pRead->m_pFormat->m_nShifts[1]];
	ch[2] = pRead->m_pTables[2][(*pPixel & pRead->m_pFormat->m_Masks[2]) >> pRead->m_pFormat->m_nShifts[2]];
	ch[3] = pRead->m_pTables[3][(*pPixel & pRead->m_pFormat->m_Masks[3]) >> pRead->m_pFormat->m_nShifts[3]];

	return (ch[0] << 24) | (ch[1] << 16) | (ch[2] << 8) | ch[3];
}

// A 32-bit source is eight bits a plane: the shift alone, no table.
static PValue fr_Read32(FormatRead* pRead, uint8* pPos)
{
	uint32 ch[4];
	uint32* pPixel = (uint32*)((uint32)pPos & ~0UL);

	ch[0] = (uint8)(*pPixel >> pRead->m_pFormat->m_nShifts[0]);
	ch[1] = (uint8)(*pPixel >> pRead->m_pFormat->m_nShifts[1]);
	ch[2] = (uint8)(*pPixel >> pRead->m_pFormat->m_nShifts[2]);
	ch[3] = (uint8)(*pPixel >> pRead->m_pFormat->m_nShifts[3]);

	return (ch[0] << 24) | (ch[1] << 16) | (ch[2] << 8) | ch[3];
}

// A palette of A, R, G, B byte quads -> PValue.
static PValue pal_ReadRGB(uint8* pPalette, uint8* pSrc)
{
	return (pPalette[*pSrc * 4 + 1] << 16) | (pPalette[*pSrc * 4 + 2] << 8) | pPalette[*pSrc * 4 + 3];
}

inline bool IsCompressed(uint32 nType)
{
	return nType == 4 || nType == 5 || nType == 6;
}


// 0x10073c08's two tests want an int answer.
static inline int IsCompressedInt(uint32 nType)
{
	return nType == 4 || nType == 5 || nType == 6;
}


// ----------------------------------------------------------------------- //
//  0x10073c00 -- source and destination share a format: rows copied, or
//  OR-ed (the destination into the SOURCE, as lithtech's 0x0045d1e0 does);
//  a compressed (DXT, types 4..6) pair is one memcpy of the image size.
// ----------------------------------------------------------------------- //
uint32 sub_10073c00(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	uint32 nWidth, nRowBytes, nDwords, nWords, nBytes, x, y;
	uint8* pSrcRow;
	uint8* pDstRow;

	nWidth    = pRequest->m_nWidth;
	nRowBytes = nWidth << g_10089870[pRequest->m_pSrcFormat->m_nType];
	y         = pRequest->m_nHeight;
	pSrcRow   = pRequest->m_pSrcData;
	pDstRow   = pRequest->m_pDstData;

	if (IsCompressedInt(pRequest->m_pSrcFormat->m_nType) != IsCompressedInt(pRequest->m_pDstFormat->m_nType))
		return 1;

	if (IsCompressedInt(pRequest->m_pSrcFormat->m_nType))
	{
		if (pRequest->m_nFlags & 1)
			return 1;

		memcpy(pRequest->m_pDstData, pRequest->m_pSrcData,
		       sub_10079f30(pRequest->m_pSrcFormat->m_nType, nWidth, pRequest->m_nHeight));
		return 0;
	}

	if (pRequest->m_nFlags & 1)
	{
		nWords  = 0;
		nDwords = 0;

		if (((uint32)pSrcRow | (uint32)pDstRow) & 3)
		{
			if (((uint32)pSrcRow | (uint32)pDstRow) & 1)
			{
				nBytes = nRowBytes;
			}
			else
			{
				nWords = nRowBytes >> 1;
				nBytes = nRowBytes - nWords * 2;
			}
		}
		else
		{
			nDwords = nRowBytes >> 2;
			nBytes  = nRowBytes - nDwords * 4;
		}

		while (y)
		{
			uint8* pIn  = pDstRow;
			uint8* pOut = pSrcRow;

			y--;

			x = nDwords;
			while (x)
			{
				x--;
				*(uint32*)pOut |= *(uint32*)pIn;
				pIn  += 4;
				pOut += 4;
			}

			x = nWords;
			while (x)
			{
				x--;
				*(uint16*)pOut |= *(uint16*)pIn;
				pIn  += 2;
				pOut += 2;
			}

			x = nBytes;
			while (x)
			{
				x--;
				*pOut |= *pIn;
				pIn++;
				pOut++;
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}
	else
	{
		while (y)
		{
			y--;
			memcpy(pDstRow, pSrcRow, nRowBytes);

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}

	return 0;
}


// ----------------------------------------------------------------------- //
//  0x10073e00 -- [0][1] 8-bit palettised -> 8-bit.
// ----------------------------------------------------------------------- //
uint32 sub_10073e00(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	uint8*        pPalette;
	FormatConvert convert;
	uint8*        pCur[2];
	PValue        value[1];
	uint8*        pSrcRow;
	uint8*        pDstRow;
	uint32        x, y;

	pPalette = pRequest->m_pSrcPalette;
	fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

	y       = pRequest->m_nHeight;
	pSrcRow = pRequest->m_pSrcData;
	pDstRow = pRequest->m_pDstData;

	while (y)
	{
		pCur[0] = pSrcRow;
		pCur[1] = pDstRow;

		y--;

		x = pRequest->m_nWidth;
		if (pRequest->m_nFlags & 1)
		{
			while (x)
			{
				x--;
				value[0] = pal_ReadRGB(pPalette, pCur[0]);
				AT8(pCur[1]) |= fc_Convert8(&convert, &value[0]);

				pCur[0] += sizeof(uint8);
				pCur[1] += sizeof(uint8);
			}
		}
		else
		{
			while (x)
			{
				x--;
				value[0] = pal_ReadRGB(pPalette, pCur[0]);
				AT8(pCur[1]) = fc_Convert8(&convert, &value[0]);

				pCur[0] += sizeof(uint8);
				pCur[1] += sizeof(uint8);
			}
		}

		pSrcRow += pRequest->m_nSrcPitch;
		pDstRow += pRequest->m_nDstPitch;
	}

	return 0;
}


// ----------------------------------------------------------------------- //
//  0x100740a0 -- [0][2] 8-bit palettised -> 16-bit.
// ----------------------------------------------------------------------- //
// The row width is read INSIDE each arm (`x = width` in
// the OR arm and again in the store arm), not once before the flag test:
// 563 -> 558 bytes differing, and the body no longer ends in a
// bare `ret` (the shipped row ends in a `mov %esi,%esi` pad, which the
// epilogue test reads as no ret), so it pairs as a near on the
// unchanged shipped first 84 bytes.  The other converters keep the single read
// before the branch -- measured worse on each (10074ea0, 10075cd0,
// 100746b0, 10075920, 10074350, 10074a50, 100754d0).
uint32 sub_100740a0(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	uint8*        pPalette;
	FormatConvert convert;
	uint8*        pCur[2];
	PValue        value[1];
	uint8*        pSrcRow;
	uint8*        pDstRow;
	uint32        x, y;

	pPalette = pRequest->m_pSrcPalette;
	fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

	y       = pRequest->m_nHeight;
	pSrcRow = pRequest->m_pSrcData;
	pDstRow = pRequest->m_pDstData;

	while (y)
	{
		pCur[0] = pSrcRow;
		pCur[1] = pDstRow;

		y--;

		if (pRequest->m_nFlags & 1)
		{
			x = pRequest->m_nWidth;
			while (x)
			{
				x--;
				value[0] = pal_ReadRGB(pPalette, pCur[0]);
				AT16(pCur[1]) |= fc_Convert16(&convert, &value[0]);

				pCur[0] += sizeof(uint8);
				pCur[1] += sizeof(uint16);
			}
		}
		else
		{
			x = pRequest->m_nWidth;
			while (x)
			{
				x--;
				value[0] = pal_ReadRGB(pPalette, pCur[0]);
				AT16(pCur[1]) = fc_Convert16(&convert, &value[0]);

				pCur[0] += sizeof(uint8);
				pCur[1] += sizeof(uint16);
			}
		}

		pSrcRow += pRequest->m_nSrcPitch;
		pDstRow += pRequest->m_nDstPitch;
	}

	return 0;
}


// ----------------------------------------------------------------------- //
//  0x10074350 -- [0][3] 8-bit palettised -> 32-bit.  A PValue destination
//  takes the palette read as it is; any other goes through a converter.
// ----------------------------------------------------------------------- //
uint32 sub_10074350(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	if (!pRequest->m_pDstFormat->sub_10079f80(&pMgr->m_FormatPValue))
	{
		uint8*        pPalette;
		FormatConvert convert;
		uint8*        pCur[2];
		PValue        value[1];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		pPalette = pRequest->m_pSrcPalette;
		fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					value[0] = pal_ReadRGB(pPalette, pCur[0]);
					AT32(pCur[1]) |= fc_Convert32(&convert, &value[0]);

					pCur[0] += sizeof(uint8);
					pCur[1] += sizeof(PValue);
				}
			}
			else
			{
				while (x)
				{
					x--;
					value[0] = pal_ReadRGB(pPalette, pCur[0]);
					AT32(pCur[1]) = fc_Convert32(&convert, &value[0]);

					pCur[0] += sizeof(uint8);
					pCur[1] += sizeof(PValue);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}
	else
	{
		uint8*        pPalette;
		uint8*        pCur[2];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		pPalette = pRequest->m_pSrcPalette;

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					*(PValue*)pCur[1] |= pal_ReadRGB(pPalette, pCur[0]);

					pCur[0] += sizeof(uint8);
					pCur[1] += sizeof(PValue);
				}
			}
			else
			{
				while (x)
				{
					x--;
					*(PValue*)pCur[1] = pal_ReadRGB(pPalette, pCur[0]);

					pCur[0] += sizeof(uint8);
					pCur[1] += sizeof(PValue);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}

	return 0;
}


// ----------------------------------------------------------------------- //
//  0x100746b0 -- [1][1] 8-bit -> 8-bit.  One format both sides is the
//  row copy, entered at its /Qip register entry (0x100746d7).
// ----------------------------------------------------------------------- //
uint32 sub_100746b0(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	FormatRead    read;
	FormatConvert convert;
	uint8*        pCur[2];
	PValue        value[1];
	uint8*        pSrcRow;
	uint8*        pDstRow;
	uint32        x, y;

	if (pRequest->m_pSrcFormat->sub_10079f80(pRequest->m_pDstFormat))
		return sub_10073c00(pMgr, pRequest);

	fr_Init(&read, pMgr, pRequest->m_pSrcFormat);
	fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

	y       = pRequest->m_nHeight;
	pSrcRow = pRequest->m_pSrcData;
	pDstRow = pRequest->m_pDstData;

	while (y)
	{
		pCur[0] = pSrcRow;
		pCur[1] = pDstRow;

		y--;

		x = pRequest->m_nWidth;
		if (pRequest->m_nFlags & 1)
		{
			while (x)
			{
				x--;
				value[0] = fr_Read8(&read, pCur[0]);
				AT8(pCur[1]) |= fc_Convert8(&convert, &value[0]);

				pCur[0] += sizeof(uint8);
				pCur[1] += sizeof(uint8);
			}
		}
		else
		{
			while (x)
			{
				x--;
				value[0] = fr_Read8(&read, pCur[0]);
				AT8(pCur[1]) = fc_Convert8(&convert, &value[0]);

				pCur[0] += sizeof(uint8);
				pCur[1] += sizeof(uint8);
			}
		}

		pSrcRow += pRequest->m_nSrcPitch;
		pDstRow += pRequest->m_nDstPitch;
	}

	return 0;
}


// ----------------------------------------------------------------------- //
//  0x10074a50 -- [1][2] 8-bit -> 16-bit.
// ----------------------------------------------------------------------- //
uint32 sub_10074a50(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	FormatRead    read;
	FormatConvert convert;
	uint8*        pCur[2];
	PValue        value[1];
	uint8*        pSrcRow;
	uint8*        pDstRow;
	uint32        x, y;

	fr_Init(&read, pMgr, pRequest->m_pSrcFormat);
	fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

	y       = pRequest->m_nHeight;
	pSrcRow = pRequest->m_pSrcData;
	pDstRow = pRequest->m_pDstData;

	while (y)
	{
		pCur[0] = pSrcRow;
		pCur[1] = pDstRow;

		y--;

		x = pRequest->m_nWidth;
		if (pRequest->m_nFlags & 1)
		{
			while (x)
			{
				x--;
				value[0] = fr_Read8(&read, pCur[0]);
				AT16(pCur[1]) |= fc_Convert16(&convert, &value[0]);

				pCur[0] += sizeof(uint8);
				pCur[1] += sizeof(uint16);
			}
		}
		else
		{
			while (x)
			{
				x--;
				value[0] = fr_Read8(&read, pCur[0]);
				AT16(pCur[1]) = fc_Convert16(&convert, &value[0]);

				pCur[0] += sizeof(uint8);
				pCur[1] += sizeof(uint16);
			}
		}

		pSrcRow += pRequest->m_nSrcPitch;
		pDstRow += pRequest->m_nDstPitch;
	}

	return 0;
}


// ----------------------------------------------------------------------- //
//  0x10074ea0 -- [1][3] 8-bit -> 32-bit.  As 0x10074350, with a format reader.
// ----------------------------------------------------------------------- //
uint32 sub_10074ea0(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	if (!pRequest->m_pDstFormat->sub_10079f80(&pMgr->m_FormatPValue))
	{
		FormatRead    read;
		FormatConvert convert;
		uint8*        pCur[2];
		PValue        value[1];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		fr_Init(&read, pMgr, pRequest->m_pSrcFormat);
		fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					value[0] = fr_Read8(&read, pCur[0]);
					AT32(pCur[1]) |= fc_Convert32(&convert, &value[0]);

					pCur[0] += sizeof(uint8);
					pCur[1] += sizeof(PValue);
				}
			}
			else
			{
				while (x)
				{
					x--;
					value[0] = fr_Read8(&read, pCur[0]);
					AT32(pCur[1]) = fc_Convert32(&convert, &value[0]);

					pCur[0] += sizeof(uint8);
					pCur[1] += sizeof(PValue);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}
	else
	{
		FormatRead    read;
		uint8*        pCur[2];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		fr_Init(&read, pMgr, pRequest->m_pSrcFormat);

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					*(PValue*)pCur[1] |= fr_Read8(&read, pCur[0]);

					pCur[0] += sizeof(uint8);
					pCur[1] += sizeof(PValue);
				}
			}
			else
			{
				while (x)
				{
					x--;
					*(PValue*)pCur[1] = fr_Read8(&read, pCur[0]);

					pCur[0] += sizeof(uint8);
					pCur[1] += sizeof(PValue);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}

	return 0;
}


// ======================================================================= //
//
//  (was w20_100754d0_os.cpp) The format manager's pixel converters for a
//  16- or 32-bit source, entered through the 8x8 table at 0x100898b0
//  (FormatMgr::ConvertPixels, 0x1007a130, calls
//  g_100898b0[src type][dst type](pMgr, pRequest), cdecl):
//
//    0x100754d0 (1104)  [2][1]  16-bit -> 8-bit
//    0x10075920  (933)  [2][2]  16-bit -> 16-bit
//    0x10075cd0 (1578)  [2][3]  16-bit -> 32-bit
//    0x10076300 (1388)  [3][1]  32-bit -> 8-bit
//    0x10076870 (1400)  [3][2]  32-bit -> 16-bit
//    0x10076df0  (800)  [3][3]  32-bit -> 32-bit
//
//  THE IDIOM (all six, and the neighbouring rows 0/1/4..7):
//    * a PValue is 0xAARRGGBB, channel i of a format being A, R, G, B for
//      i = 0..3 (PFormat +0x08 masks, +0x18 bit counts, +0x28 shifts);
//    * a 16-bit source channel is `(pixel & mask) >> shift`, scaled up to
//      0..255 through the manager's +0x970 table for that bit count; a
//      32-bit source channel is `(uint8)(pixel >> shift)` -- no mask, no
//      table (the up tables are still fetched, and stored, but unused);
//    * a destination channel is the manager's +0x70 down table for its bit
//      count, indexed by the PValue byte and shifted left by its shift,
//      the channels OR-ed in the destination's width (the 8- and 16-bit
//      forms re-truncate after every OR: `movzbl`/`movzwl`);
//    * rows: `while (height) { height--; ... }`, source and destination
//      advanced by their pitches; per row, request flag bit 0 selects OR-ing
//      into the destination instead of storing over it; within a row
//      `while (width) { width--; ... }`.
//  Fast paths: 0x10075920 and 0x10076df0 hand over to the plain copier
//  0x10073c00 when the two formats are the same (0x10075920) / when the
//  destination is the manager's own PValue format (0x10076df0);
//  0x10075cd0 writes the PValue itself when the destination is that
//  format; 0x10076300/0x10076870 take the source pixel as the PValue when
//  the source is. "The manager's format" is the PFormat at FormatMgr+0,
//  passed to PFormat::IsSame (0x10079f80) as the manager pointer itself.
//
//  The six are written in rows 0/1's shape (one shape for
//  all twelve converters, see THE FRAME in rows 0/1): the reader
//  and writer are the FormatRead / FormatConvert helper objects -- which is
//  what the shipped frame's two never-touched dwords are, each object's
//  unused m_pFormat (+0x00 of the read object, +0x24 = +0x10 of the
//  convert object) -- the cursors a two-element array, the PValue a
//  one-element array, the pixel read through fr_Read16 / fr_Read32 with
//  the `& ~0` and the `uint32 ch[4]` (the four re-reads of the pixel).
//  0x100754d0 now has the shipped frame (0x94) and the shipped first 92
//  bytes; the residue across the six is register naming and the
//  table-lookup operand order (index, table) the single-pixel functions
//  record below.
//
// ======================================================================= //


// ======================================================================= //
//  0x100754d0, 1104 bytes -- table [2][1], 16-bit to 8-bit.
// ======================================================================= //
uint32 sub_100754d0(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	FormatRead    read;
	FormatConvert convert;
	uint8*        pCur[2];
	PValue        value[1];
	uint8*        pSrcRow;
	uint8*        pDstRow;
	uint32        x, y;

	fr_Init(&read, pMgr, pRequest->m_pSrcFormat);
	fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

	y       = pRequest->m_nHeight;
	pSrcRow = pRequest->m_pSrcData;
	pDstRow = pRequest->m_pDstData;

	while (y)
	{
		pCur[0] = pSrcRow;
		pCur[1] = pDstRow;

		y--;

		x = pRequest->m_nWidth;
		if (pRequest->m_nFlags & 1)
		{
			while (x)
			{
				x--;
				value[0] = fr_Read16(&read, pCur[0]);
				AT8(pCur[1]) |= fc_Convert8(&convert, &value[0]);

				pCur[0] += sizeof(uint16);
				pCur[1] += sizeof(uint8);
			}
		}
		else
		{
			while (x)
			{
				x--;
				value[0] = fr_Read16(&read, pCur[0]);
				AT8(pCur[1]) = fc_Convert8(&convert, &value[0]);

				pCur[0] += sizeof(uint16);
				pCur[1] += sizeof(uint8);
			}
		}

		pSrcRow += pRequest->m_nSrcPitch;
		pDstRow += pRequest->m_nDstPitch;
	}

	return 0;
}

// ======================================================================= //
//  0x10075920, 933 bytes -- table [2][2], 16-bit to 16-bit.
//  Two formats that are the same go to the plain copier.
// ======================================================================= //
uint32 sub_10075920(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	FormatRead    read;
	FormatConvert convert;
	uint8*        pCur[2];
	PValue        value[1];
	uint8*        pSrcRow;
	uint8*        pDstRow;
	uint32        x, y;

	if (pRequest->m_pSrcFormat->sub_10079f80(pRequest->m_pDstFormat))
		return sub_10073c00(pMgr, pRequest);

	fr_Init(&read, pMgr, pRequest->m_pSrcFormat);
	fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

	y       = pRequest->m_nHeight;
	pSrcRow = pRequest->m_pSrcData;
	pDstRow = pRequest->m_pDstData;

	while (y)
	{
		pCur[0] = pSrcRow;
		pCur[1] = pDstRow;

		y--;

		x = pRequest->m_nWidth;
		if (pRequest->m_nFlags & 1)
		{
			while (x)
			{
				x--;
				value[0] = fr_Read16(&read, pCur[0]);
				AT16(pCur[1]) |= fc_Convert16(&convert, &value[0]);

				pCur[0] += sizeof(uint16);
				pCur[1] += sizeof(uint16);
			}
		}
		else
		{
			while (x)
			{
				x--;
				value[0] = fr_Read16(&read, pCur[0]);
				AT16(pCur[1]) = fc_Convert16(&convert, &value[0]);

				pCur[0] += sizeof(uint16);
				pCur[1] += sizeof(uint16);
			}
		}

		pSrcRow += pRequest->m_nSrcPitch;
		pDstRow += pRequest->m_nDstPitch;
	}

	return 0;
}

// ======================================================================= //
//  0x10075cd0, 1578 bytes -- table [2][3], 16-bit to 32-bit.
//  A destination in the manager's own format takes the PValue as it is.
// ======================================================================= //
uint32 sub_10075cd0(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	if (!pRequest->m_pDstFormat->sub_10079f80(&pMgr->m_FormatPValue))
	{
		FormatRead    read;
		FormatConvert convert;
		uint8*        pCur[2];
		PValue        value[1];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		fr_Init(&read, pMgr, pRequest->m_pSrcFormat);
		fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					value[0] = fr_Read16(&read, pCur[0]);
					AT32(pCur[1]) |= fc_Convert32(&convert, &value[0]);

					pCur[0] += sizeof(uint16);
					pCur[1] += sizeof(PValue);
				}
			}
			else
			{
				while (x)
				{
					x--;
					value[0] = fr_Read16(&read, pCur[0]);
					AT32(pCur[1]) = fc_Convert32(&convert, &value[0]);

					pCur[0] += sizeof(uint16);
					pCur[1] += sizeof(PValue);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}
	else
	{
		FormatRead    read;
		uint8*        pCur[2];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		fr_Init(&read, pMgr, pRequest->m_pSrcFormat);

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					AT32(pCur[1]) |= fr_Read16(&read, pCur[0]);

					pCur[0] += sizeof(uint16);
					pCur[1] += sizeof(PValue);
				}
			}
			else
			{
				while (x)
				{
					x--;
					AT32(pCur[1]) = fr_Read16(&read, pCur[0]);

					pCur[0] += sizeof(uint16);
					pCur[1] += sizeof(PValue);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}

	return 0;
}

// ======================================================================= //
//  0x10076300, 1388 bytes -- table [3][1], 32-bit to 8-bit.
//  A source in the manager's own format IS the PValue.
// ======================================================================= //
uint32 sub_10076300(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	if (pRequest->m_pSrcFormat->sub_10079f80(&pMgr->m_FormatPValue))
	{
		FormatConvert convert;
		uint8*        pCur[2];
		PValue        value[1];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					value[0] = AT32(pCur[0]);
					AT8(pCur[1]) |= fc_Convert8(&convert, &value[0]);

					pCur[0] += sizeof(PValue);
					pCur[1] += sizeof(uint8);
				}
			}
			else
			{
				while (x)
				{
					x--;
					value[0] = AT32(pCur[0]);
					AT8(pCur[1]) = fc_Convert8(&convert, &value[0]);

					pCur[0] += sizeof(PValue);
					pCur[1] += sizeof(uint8);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}
	else
	{
		FormatRead    read;
		FormatConvert convert;
		uint8*        pCur[2];
		PValue        value[1];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		fr_Init(&read, pMgr, pRequest->m_pSrcFormat);
		fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					value[0] = fr_Read32(&read, pCur[0]);
					AT8(pCur[1]) |= fc_Convert8(&convert, &value[0]);

					pCur[0] += sizeof(PValue);
					pCur[1] += sizeof(uint8);
				}
			}
			else
			{
				while (x)
				{
					x--;
					value[0] = fr_Read32(&read, pCur[0]);
					AT8(pCur[1]) = fc_Convert8(&convert, &value[0]);

					pCur[0] += sizeof(PValue);
					pCur[1] += sizeof(uint8);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}

	return 0;
}

// ======================================================================= //
//  0x10076870, 1400 bytes -- table [3][2], 32-bit to 16-bit.
//  A source in the manager's own format IS the PValue.
// ======================================================================= //
uint32 sub_10076870(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	if (pRequest->m_pSrcFormat->sub_10079f80(&pMgr->m_FormatPValue))
	{
		FormatConvert convert;
		uint8*        pCur[2];
		PValue        value[1];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					value[0] = AT32(pCur[0]);
					AT16(pCur[1]) |= fc_Convert16(&convert, &value[0]);

					pCur[0] += sizeof(PValue);
					pCur[1] += sizeof(uint16);
				}
			}
			else
			{
				while (x)
				{
					x--;
					value[0] = AT32(pCur[0]);
					AT16(pCur[1]) = fc_Convert16(&convert, &value[0]);

					pCur[0] += sizeof(PValue);
					pCur[1] += sizeof(uint16);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}
	else
	{
		FormatRead    read;
		FormatConvert convert;
		uint8*        pCur[2];
		PValue        value[1];
		uint8*        pSrcRow;
		uint8*        pDstRow;
		uint32        x, y;

		fr_Init(&read, pMgr, pRequest->m_pSrcFormat);
		fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

		y       = pRequest->m_nHeight;
		pSrcRow = pRequest->m_pSrcData;
		pDstRow = pRequest->m_pDstData;

		while (y)
		{
			pCur[0] = pSrcRow;
			pCur[1] = pDstRow;

			y--;

			x = pRequest->m_nWidth;
			if (pRequest->m_nFlags & 1)
			{
				while (x)
				{
					x--;
					value[0] = fr_Read32(&read, pCur[0]);
					AT16(pCur[1]) |= fc_Convert16(&convert, &value[0]);

					pCur[0] += sizeof(PValue);
					pCur[1] += sizeof(uint16);
				}
			}
			else
			{
				while (x)
				{
					x--;
					value[0] = fr_Read32(&read, pCur[0]);
					AT16(pCur[1]) = fc_Convert16(&convert, &value[0]);

					pCur[0] += sizeof(PValue);
					pCur[1] += sizeof(uint16);
				}
			}

			pSrcRow += pRequest->m_nSrcPitch;
			pDstRow += pRequest->m_nDstPitch;
		}
	}

	return 0;
}

// ======================================================================= //
//  0x10076df0, 800 bytes -- table [3][3], 32-bit to 32-bit.
//  A destination in the manager's own format goes to the plain copier.
// ======================================================================= //
// The row width is read inside each arm, as 0x100740a0's:
// that alone swaps the two parameters' registers to the shipped ones
// (pRequest %ebx, pMgr %esi) and the first 118 bytes now match -- near
// (was attempted with the first 14; 560 -> 615 bytes differing,
// the residue being the loop's register naming as before).
uint32 sub_10076df0(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	FormatRead    read;
	FormatConvert convert;
	uint8*        pCur[2];
	PValue        value[1];
	uint8*        pSrcRow;
	uint8*        pDstRow;
	uint32        x, y;

	if (pRequest->m_pDstFormat->sub_10079f80(&pMgr->m_FormatPValue))
		return sub_10073c00(pMgr, pRequest);

	fr_Init(&read, pMgr, pRequest->m_pSrcFormat);
	fc_Init(&convert, pMgr, pRequest->m_pDstFormat);

	y       = pRequest->m_nHeight;
	pSrcRow = pRequest->m_pSrcData;
	pDstRow = pRequest->m_pDstData;

	while (y)
	{
		pCur[0] = pSrcRow;
		pCur[1] = pDstRow;

		y--;

		if (pRequest->m_nFlags & 1)
		{
			x = pRequest->m_nWidth;
			while (x)
			{
				x--;
				value[0] = fr_Read32(&read, pCur[0]);
				AT32(pCur[1]) |= fc_Convert32(&convert, &value[0]);

				pCur[0] += sizeof(PValue);
				pCur[1] += sizeof(PValue);
			}
		}
		else
		{
			x = pRequest->m_nWidth;
			while (x)
			{
				x--;
				value[0] = fr_Read32(&read, pCur[0]);
				AT32(pCur[1]) = fc_Convert32(&convert, &value[0]);

				pCur[0] += sizeof(PValue);
				pCur[1] += sizeof(PValue);
			}
		}

		pSrcRow += pRequest->m_nSrcPitch;
		pDstRow += pRequest->m_nDstPitch;
	}

	return 0;
}



// ======================================================================= //
//
//  (was w21_fmconvert.h) The format manager's pixel converters' shared view of
//  the manager, the request and the two per-blit helper objects, for
//  w21_77110_os.cpp (0x10077110 / 0x100773e0, the 32-bit palettised
//  sources) and w21_77680_os.cpp (0x10077680 / 0x10078910, the DXT
//  decompressors).  Each converter is a slot of the table at 0x100898b0
//  (FormatMgr::ConvertPixels, 0x1007a130, w29_10079fd0_osoiqip.cpp), called
//  `(FormatMgr*, ConvertRequest*)` cdecl.
//
//  The layouts are LithTech's FMConvertMgr / FMConvertRequest /
//  FMFormatConvert / FMFormatRead (src/engine/lithtech/fmconvert.cpp reads
//  them off lithtech.exe) and every offset is re-read off d3d.ren:
//    PFormat        +0x04 type, +0x08 masks[4], +0x18 bits[4], +0x28 shifts[4]
//    FormatMgr      +0x38 the 565 format (0x10077706 `mov 0x50(%esi)` is
//                   its bits[0]), +0x70 nine 256-byte to-tables, +0x970 nine
//                   from-table pointers
//    ConvertRequest +0x00 src format, +0x04 src, +0x08 src pitch, +0x0c
//                   palette, +0x10 dst format, +0x14 dst, +0x18 dst pitch,
//                   +0x1c width, +0x20 height, +0x24 flags (bit 0 = OR)
//
// ======================================================================= //

#include "ltpvalue.h"

struct PalEntry
{
	uint8	m_a, m_r, m_g, m_b;
};

// PValue -> destination format.  The table pointers are built as integer
// sums: `(uint8*)((uint32)m_ToTables + (bits << 8))` keeps icl's
// `shl $0x8; add` with one `lea 0x70(%mgr)` per table (0x10077125..
// 0x1007715c); indexing m_ToTables[bits] gives `imul $0x100` instead.
class FmtConvert
{
public:
	void Init(FormatMgr* pMgr, PFormat* pFormat)
	{
		m_pFormat = pFormat;
		m_pTables[CP_ALPHA] = (uint8*)((uint32)pMgr->m_ToTables + (pFormat->m_nBits[CP_ALPHA] << 8));
		m_pTables[CP_RED]   = (uint8*)((uint32)pMgr->m_ToTables + (pFormat->m_nBits[CP_RED] << 8));
		m_pTables[CP_GREEN] = (uint8*)((uint32)pMgr->m_ToTables + (pFormat->m_nBits[CP_GREEN] << 8));
		m_pTables[CP_BLUE]  = (uint8*)((uint32)pMgr->m_ToTables + (pFormat->m_nBits[CP_BLUE] << 8));
	}

	// Every OR is narrowed on its own (a `movzwl` after each term,
	// 0x10077210 / 0x1007723c / 0x10077264 / 0x10077279), and the terms
	// are emitted alpha first.
	uint16 Convert16(PValue* pValue)
	{
		PValue value = *pValue;

		return (uint16)((uint16)((uint16)((uint16)(m_pTables[CP_ALPHA][PValue_GetA(value)] << m_pFormat->m_nShifts[CP_ALPHA]) |
		                                           (m_pTables[CP_RED][PValue_GetR(value)]   << m_pFormat->m_nShifts[CP_RED])) |
		                                  (m_pTables[CP_GREEN][PValue_GetG(value)] << m_pFormat->m_nShifts[CP_GREEN])) |
		                         (m_pTables[CP_BLUE][PValue_GetB(value)] << m_pFormat->m_nShifts[CP_BLUE]));
	}

	PValue Convert32(PValue* pValue)
	{
		PValue value = *pValue;

		return (m_pTables[CP_ALPHA][PValue_GetA(value)] << m_pFormat->m_nShifts[CP_ALPHA]) |
		       (m_pTables[CP_RED][PValue_GetR(value)]   << m_pFormat->m_nShifts[CP_RED])   |
		       (m_pTables[CP_GREEN][PValue_GetG(value)] << m_pFormat->m_nShifts[CP_GREEN]) |
		       (m_pTables[CP_BLUE][PValue_GetB(value)]  << m_pFormat->m_nShifts[CP_BLUE]);
	}

	uint8*   m_pTables[NUM_COLORPLANES];    // +0x00
	PFormat* m_pFormat;                     // +0x10
};

// Source format -> PValue.
class FmtRead
{
public:
	void Init(FormatMgr* pMgr, PFormat* pFormat)
	{
		m_pTables[CP_ALPHA] = pMgr->m_pFromTables[pFormat->m_nBits[CP_ALPHA]];
		m_pTables[CP_RED]   = pMgr->m_pFromTables[pFormat->m_nBits[CP_RED]];
		m_pTables[CP_GREEN] = pMgr->m_pFromTables[pFormat->m_nBits[CP_GREEN]];
		m_pTables[CP_BLUE]  = pMgr->m_pFromTables[pFormat->m_nBits[CP_BLUE]];

		m_pFormat = pFormat;
	}

	PValue Read16(uint16* pSrc)
	{
		uint32 nData = *pSrc;
		uint32 planes[NUM_COLORPLANES];

		planes[CP_ALPHA] = m_pTables[CP_ALPHA][(m_pFormat->m_Masks[CP_ALPHA] & nData) >> m_pFormat->m_nShifts[CP_ALPHA]];
		planes[CP_RED]   = m_pTables[CP_RED][(m_pFormat->m_Masks[CP_RED] & nData) >> m_pFormat->m_nShifts[CP_RED]];
		planes[CP_GREEN] = m_pTables[CP_GREEN][(m_pFormat->m_Masks[CP_GREEN] & nData) >> m_pFormat->m_nShifts[CP_GREEN]];
		planes[CP_BLUE]  = m_pTables[CP_BLUE][(m_pFormat->m_Masks[CP_BLUE] & nData) >> m_pFormat->m_nShifts[CP_BLUE]];

		return (planes[CP_ALPHA] << 24) | (planes[CP_RED] << 16) | (planes[CP_GREEN] << 8) | planes[CP_BLUE];
	}

	PFormat* m_pFormat;                     // +0x00
	uint8*   m_pTables[NUM_COLORPLANES];    // +0x04
};

// A 32-bit (ARGB) palette.
class PalRead
{
public:
	PValue ReadARGB(uint8* pSrc)
	{
		return (m_pPalette[*pSrc].m_a << 24) | (m_pPalette[*pSrc].m_r << 16) |
		       (m_pPalette[*pSrc].m_g << 8) | m_pPalette[*pSrc].m_b;
	}

	PalEntry* m_pPalette;
};



// ======================================================================= //
//
//  (was w21_77110_os.cpp) The format manager's two 32-bit-palettised-source
//  converters, slots of the table at 0x100898b0 (w21_fmconvert.h):
//    0x10077110 (719 bytes)  row 7 col 2, +0xe8: 8-bit index into an ARGB
//                            palette -> 16-bit destination
//    0x100773e0 (667 bytes)  row 7 col 3, +0xec: the same -> 32-bit
//  LithTech's fm_BlitPal32To16 / fm_BlitPal32To32 (lithtech.exe 0x0045e9f0
//  / 0x0045eb90, src/engine/lithtech/fmconvert.cpp) as icl built them.
//
//  FLAGS: /Os.  The padding after each body is plain 0x90, and /Os is what
//  gives the shipped `lea 0x70(%mgr); shl $0x8; add` table pointers -- /O2
//  emits `imul $0x100` for the same source.  (In this /Os /Oi- /Qip TU the
//  two are the same bytes as in their old /Os file: measured.)
//
//  WHAT THE BYTES FIX, per pixel and per row:
//    * source and destination cursors, the pixel value and the palette
//      pointer all live in the frame (0x14, 0x18, 0x1c, 0x20) and are
//      stored and reloaded every pixel -- the value's store (0x100771ef) is
//      never even read back.  icl keeps only ARRAYS (and objects it calls
//      methods on) in memory; every scalar or struct spelling was measured
//      register-allocated.  So the four are one uint32 array, regs[].
//    * both cursors are masked before every dereference, `and
//      $0xffffffff` (0x100771af / 0x10077249): icl does not fold `x & ~0`,
//      and the immediate form needs the cursor loaded from memory first --
//      which is the array again.
//    * the palette is stored ONCE, after the height test and before the
//      row loop (0x10077173, the loop re-enters at 0x10077177), but loaded
//      at the top (0x1007712f), hence the local and the `if (y) do {}`.
//    * both counters are decremented at the TOP of their loops (0x10077182,
//      0x100771c0); `while (n--)` puts the `dec` at the bottom.
//
//  STATUS: near.  Logic, calls-free inlining, frame slots of regs[] and
//  the control flow are the shipped ones; what remains is scheduling --
//  icl hoists the destination cursor's load above the pixel's conversion,
//  where the shipped code loads it after the first two terms, which moves
//  three spill slots (frame 0x40/0x44 against 0x4c/0x44).  Every spelling
//  tried (destination read first, |= vs = ... |, value through an
//  out-parameter, cursors through a pointer, separate objects) came back
//  the same or worse.
//
// ======================================================================= //

uint32 sub_10077110(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	FmtConvert convert;
	uint8*     pSrcRow;
	uint8*     pDstRow;
	uint32     x, y;
	uint32     regs[4];
	uint8*     pPalette;
	uint16     nColor;

	pPalette = pRequest->m_pSrcPalette;
	convert.Init(pMgr, pRequest->m_pDstFormat);

	y = pRequest->m_nHeight;
	pSrcRow = pRequest->m_pSrcData;
	pDstRow = pRequest->m_pDstData;

	if (y)
	{
	regs[3] = (uint32)pPalette;

	do
	{
		y--;
		regs[1] = (uint32)pDstRow;
		regs[0] = (uint32)pSrcRow;

		if (pRequest->m_nFlags & 1)
		{
			x = pRequest->m_nWidth;
			while (x)
			{
				x--;
				regs[2] = ((PalRead*)&regs[3])->ReadARGB((uint8*)(regs[0] & 0xffffffff));
				nColor = convert.Convert16(&regs[2]);
				*(uint16*)(regs[1] & 0xffffffff) |= nColor;
				regs[0] += 1;
				regs[1] += 2;
			}
		}
		else
		{
			x = pRequest->m_nWidth;
			while (x)
			{
				x--;
				regs[2] = ((PalRead*)&regs[3])->ReadARGB((uint8*)(regs[0] & 0xffffffff));
				nColor = convert.Convert16(&regs[2]);
				*(uint16*)(regs[1] & 0xffffffff) = nColor;
				regs[0] += 1;
				regs[1] += 2;
			}
		}

		pSrcRow += pRequest->m_nSrcPitch;
		pDstRow += pRequest->m_nDstPitch;
	} while (y);
	}

	return 0;
}


uint32 sub_100773e0(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	FmtConvert convert;
	uint8*     pSrcRow;
	uint8*     pDstRow;
	uint32     x, y;
	uint32     regs[4];
	uint8*     pPalette;
	PValue     nColor;

	pPalette = pRequest->m_pSrcPalette;
	convert.Init(pMgr, pRequest->m_pDstFormat);

	y = pRequest->m_nHeight;
	pSrcRow = pRequest->m_pSrcData;
	pDstRow = pRequest->m_pDstData;

	if (y)
	{
	regs[3] = (uint32)pPalette;

	do
	{
		y--;
		regs[1] = (uint32)pDstRow;
		regs[0] = (uint32)pSrcRow;

		if (pRequest->m_nFlags & 1)
		{
			x = pRequest->m_nWidth;
			while (x)
			{
				x--;
				regs[2] = ((PalRead*)&regs[3])->ReadARGB((uint8*)(regs[0] & 0xffffffff));
				nColor = convert.Convert32(&regs[2]);
				*(uint32*)(regs[1] & 0xffffffff) |= nColor;
				regs[0] += 1;
				regs[1] += 4;
			}
		}
		else
		{
			x = pRequest->m_nWidth;
			while (x)
			{
				x--;
				regs[2] = ((PalRead*)&regs[3])->ReadARGB((uint8*)(regs[0] & 0xffffffff));
				nColor = convert.Convert32(&regs[2]);
				*(uint32*)(regs[1] & 0xffffffff) = nColor;
				regs[0] += 1;
				regs[1] += 4;
			}
		}

		pSrcRow += pRequest->m_nSrcPitch;
		pDstRow += pRequest->m_nDstPitch;
	} while (y);
	}

	return 0;
}


// ======================================================================= //
//
//  (was w21_77680_os.cpp) The format manager's DXT (S3TC) decompressors,
//  slots of the table at 0x100898b0 (w21_fmconvert.h), entered directly
//  with (FormatMgr*, ConvertRequest*) -- no forwarder, unlike lithtech.exe:
//    0x10077680 (4744 bytes)  rows 4/5/6 (DXT1/3/5) col 2: -> 16-bit
//    0x10078910 (4248 bytes)  rows 4/5/6 col 3: -> 32-bit
//  LithTech's fm_BlitDXTTo16 / fm_BlitDXTTo32 (src/engine/lithtech/
//  partfm.cpp 0x0045f6a0, fmcvt32.cpp 0x00460160), re-read off d3d.ren:
//    * source type 5 = DXT3 (explicit 4-bit alpha), 6 = DXT5 (interpolated
//      alpha; 0x100776c8 builds the alpha to-table and takes the shift);
//      the flags are BYTES (`mov $0x0,%bl`, `xor %bl,%bl; inc %bl`).
//    * the 16-entry alpha ramp is d3dneutral.cpp's g_AlphaExpand4to8
//      (0x100919d0, filled by 0x1007a1d0), converted up front into
//      aAlpha[] for any alpha format (0x100777d1..0x10077850).
//    * endpoints through the reader on the manager's 565 format (+0x38),
//      interpolants with unsigned `div $3` (/Os: no magic multiply).
//    * 16-bit DXT5: the endpoint compare and the /7 and /5 ladders are
//      SIGNED (`jle`, `idiv`, 0x10078150/0x10078165); in the 32-bit twin
//      they are unsigned (`jbe`, `div`, 0x100793a4/0x100793b8).  After the
//      ladder all eight entries go through the table from the ARRAY
//      (0x1007827c reloads aAlpha[0], not the endpoint register).
//    * DXT3 masks and ORs in two stores per pixel (0x10078585/0x10078595).
//
//  FLAGS: /Os -- 0x90 padding, `div` for the /3 /5 /7 divides, `shl $0x8`
//  table pointers.
//
//  STATUS: near, both.  Same logic, same blocks and calls-free inlining,
//  sizes within 3% (4856 / 4187 against 4744 / 4248).  Residue is
//  register allocation and icl's strength reduction of the inner loop
//  (ours keeps xb*8 as an accumulator where shipped recomputes
//  `lea (row,xb,8)` from xb).
//
// ======================================================================= //

extern PValue g_AlphaExpand4to8[16];                   // 0x100919d0 the alpha ramp (i * 0x11) << 24, d3dneutral.cpp, filled by 0x1007a1d0

#define FMTYPE_DXT3	5
#define FMTYPE_DXT5	6

uint32 sub_10077680(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	FmtRead    read;
	FmtConvert convert;
	uint16     aColors[4];
	uint16     aAlpha[16];
	uint32     aC0[4];
	uint32     aC1[4];
	PValue     c[4];
	uint16     nColor0, nColor1;
	uint32     aBits[2];
	uint32     nBlocksX, nBlocksY, xb, yb;
	uint32     nBlockShift, nColorOffset;
	uint32     nAlphaOr, nAlpha8, nAlphaShift;
	uint8      bHasAlphaBlock, bInterpAlpha;
	uint32     nAlphaMaskInv;
	uint8*     pAlphaToTable;
	uint32     nType;

	nAlphaOr       = PVALUE_ALPHAMASK;
	bInterpAlpha   = 0;
	nAlpha8        = 0xff;
	bHasAlphaBlock = 0;

	nType = pRequest->m_pSrcFormat->m_nType;

	if (nType == FMTYPE_DXT3)
	{
		bHasAlphaBlock = 1;
	}
	else if (nType == FMTYPE_DXT5)
	{
		pAlphaToTable  = (uint8*)((uint32)pMgr->m_ToTables + (pRequest->m_pDstFormat->m_nBits[CP_ALPHA] << 8));
		nAlphaOr       = 0;
		nAlpha8        = 0;
		nAlphaShift    = pRequest->m_pDstFormat->m_nShifts[CP_ALPHA];
		bInterpAlpha   = 1;
		bHasAlphaBlock = 1;
	}

	nAlphaMaskInv = ~pRequest->m_pDstFormat->m_Masks[CP_ALPHA];
	read.Init(pMgr, &pMgr->m_Format565);
	convert.Init(pMgr, pRequest->m_pDstFormat);

	if (bHasAlphaBlock)
	{
		uint32 i;

		nColorOffset = 8;
		nBlockShift  = 4;

		for (i = 0; i < 16; i++)
			aAlpha[i] = convert.Convert16(&g_AlphaExpand4to8[i]);
	}
	else
	{
		nColorOffset = 0;
		nBlockShift  = 3;
	}

	nBlocksY = pRequest->m_nHeight >> 2;
	nBlocksX = pRequest->m_nWidth  >> 2;

	for (yb = 0; yb < nBlocksY; yb++)
	{
		for (xb = 0; xb < nBlocksX; xb++)
		{
			uint8*  pBlock;
			uint16* pDest;
			uint32  nBits;

			pBlock = pRequest->m_pSrcData + (xb << nBlockShift)
			         + ((yb * nBlocksX) << nBlockShift) + nColorOffset;

			nColor0 = ((uint16*)pBlock)[0];
			nColor1 = ((uint16*)pBlock)[1];

			c[0] = read.Read16(&nColor0);
			c[1] = read.Read16(&nColor1);

			PValue_Get(c[0], aC0[0], aC0[1], aC0[2], aC0[3]);
			PValue_Get(c[1], aC1[0], aC1[1], aC1[2], aC1[3]);

			c[0] |= nAlphaOr;
			c[1] |= nAlphaOr;

			if (nColor0 > nColor1)
			{
				c[2] = PValue_Set(nAlpha8, (aC0[1] * 2 + aC1[1]) / 3,
				                           (aC0[2] * 2 + aC1[2]) / 3,
				                           (aC0[3] * 2 + aC1[3]) / 3);
				c[3] = PValue_Set(nAlpha8, (aC0[1] + aC1[1] * 2) / 3,
				                           (aC0[2] + aC1[2] * 2) / 3,
				                           (aC0[3] + aC1[3] * 2) / 3);
			}
			else
			{
				c[3] = 0;
				c[2] = PValue_Set(nAlpha8, (aC0[1] + aC1[1]) / 2,
				                           (aC0[2] + aC1[2]) / 2,
				                           (aC0[3] + aC1[3]) / 2);
			}

			aColors[0] = convert.Convert16(&c[0]);
			aColors[1] = convert.Convert16(&c[1]);
			aColors[2] = convert.Convert16(&c[2]);
			aColors[3] = convert.Convert16(&c[3]);

			nBits = *(uint32*)(pBlock + 4);

			pDest = (uint16*)(pRequest->m_pDstData
			                  + (yb << 2) * pRequest->m_nDstPitch + xb * 8);

			pDest[0] = aColors[ nBits        & 3];
			pDest[1] = aColors[(nBits >>  2) & 3];
			pDest[2] = aColors[(nBits >>  4) & 3];
			pDest[3] = aColors[(nBits >>  6) & 3];
			pDest = (uint16*)((uint8*)pDest + pRequest->m_nDstPitch);

			pDest[0] = aColors[(nBits >>  8) & 3];
			pDest[1] = aColors[(nBits >> 10) & 3];
			pDest[2] = aColors[(nBits >> 12) & 3];
			pDest[3] = aColors[(nBits >> 14) & 3];
			pDest = (uint16*)((uint8*)pDest + pRequest->m_nDstPitch);

			pDest[0] = aColors[(nBits >> 16) & 3];
			pDest[1] = aColors[(nBits >> 18) & 3];
			pDest[2] = aColors[(nBits >> 20) & 3];
			pDest[3] = aColors[(nBits >> 22) & 3];
			pDest = (uint16*)((uint8*)pDest + pRequest->m_nDstPitch);

			pDest[0] = aColors[(nBits >> 24) & 3];
			pDest[1] = aColors[(nBits >> 26) & 3];
			pDest[2] = aColors[(nBits >> 28) & 3];
			pDest[3] = aColors[(nBits >> 30) & 3];

			if (bHasAlphaBlock)
			{
				uint8*  pABlock;
				uint16* pADest;

				pADest = (uint16*)(pRequest->m_pDstData
				                   + (yb << 2) * pRequest->m_nDstPitch + xb * 8);

				pABlock = pRequest->m_pSrcData + (xb << nBlockShift)
				          + ((yb * nBlocksX) << nBlockShift);

				if (bInterpAlpha)
				{
					int nA, nB;

					aAlpha[0] = pABlock[0];
					aAlpha[1] = pABlock[1];

					nA = aAlpha[0];
					nB = aAlpha[1];

					if (nA > nB)
					{
						aAlpha[2] = (uint16)((nA * 6 + nB    ) / 7);
						aAlpha[3] = (uint16)((nA * 5 + nB * 2) / 7);
						aAlpha[4] = (uint16)((nA * 4 + nB * 3) / 7);
						aAlpha[5] = (uint16)((nA * 3 + nB * 4) / 7);
						aAlpha[6] = (uint16)((nA * 2 + nB * 5) / 7);
						aAlpha[7] = (uint16)((nA     + nB * 6) / 7);
					}
					else
					{
						aAlpha[2] = (uint16)((nA * 4 + nB    ) / 5);
						aAlpha[3] = (uint16)((nA * 3 + nB * 2) / 5);
						aAlpha[4] = (uint16)((nA * 2 + nB * 3) / 5);
						aAlpha[5] = (uint16)((nA     + nB * 4) / 5);
						aAlpha[6] = 0;
						aAlpha[7] = 255;
					}

					aAlpha[0] = (uint16)(pAlphaToTable[aAlpha[0]] << nAlphaShift);
					aAlpha[1] = (uint16)(pAlphaToTable[aAlpha[1]] << nAlphaShift);
					aAlpha[2] = (uint16)(pAlphaToTable[aAlpha[2]] << nAlphaShift);
					aAlpha[3] = (uint16)(pAlphaToTable[aAlpha[3]] << nAlphaShift);
					aAlpha[4] = (uint16)(pAlphaToTable[aAlpha[4]] << nAlphaShift);
					aAlpha[5] = (uint16)(pAlphaToTable[aAlpha[5]] << nAlphaShift);
					aAlpha[6] = (uint16)(pAlphaToTable[aAlpha[6]] << nAlphaShift);
					aAlpha[7] = (uint16)(pAlphaToTable[aAlpha[7]] << nAlphaShift);

					aBits[0] = *(uint32*)(pABlock + 2);
					aBits[1] = *(uint16*)(pABlock + 6);

					pADest[0] |= aAlpha[ aBits[0]        & 7];
					pADest[1] |= aAlpha[(aBits[0] >>  3) & 7];
					pADest[2] |= aAlpha[(aBits[0] >>  6) & 7];
					pADest[3] |= aAlpha[(aBits[0] >>  9) & 7];
					pADest = (uint16*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] |= aAlpha[(aBits[0] >> 12) & 7];
					pADest[1] |= aAlpha[(aBits[0] >> 15) & 7];
					pADest[2] |= aAlpha[(aBits[0] >> 18) & 7];
					pADest[3] |= aAlpha[(aBits[0] >> 21) & 7];
					pADest = (uint16*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] |= aAlpha[(aBits[0] >> 24) & 7];
					pADest[1] |= aAlpha[(aBits[0] >> 27) & 7];
					pADest[2] |= aAlpha[(aBits[0] >> 30) | ((aBits[1] & 1) << 2)];
					pADest[3] |= aAlpha[(aBits[1] >>  1) & 7];
					pADest = (uint16*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] |= aAlpha[(aBits[1] >>  4) & 7];
					pADest[1] |= aAlpha[(aBits[1] >>  7) & 7];
					pADest[2] |= aAlpha[(aBits[1] >> 10) & 7];
					pADest[3] |= aAlpha[(aBits[1] >> 13) & 7];
				}
				else
				{
					uint32 nLo, nHi;

					nLo = *(uint32*)pABlock;

					pADest[0] &= (uint16)nAlphaMaskInv; pADest[0] |= aAlpha[ nLo        & 0xf];
					pADest[1] &= (uint16)nAlphaMaskInv; pADest[1] |= aAlpha[(nLo >>  4) & 0xf];
					pADest[2] &= (uint16)nAlphaMaskInv; pADest[2] |= aAlpha[(nLo >>  8) & 0xf];
					pADest[3] &= (uint16)nAlphaMaskInv; pADest[3] |= aAlpha[(nLo >> 12) & 0xf];
					pADest = (uint16*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] &= (uint16)nAlphaMaskInv; pADest[0] |= aAlpha[(nLo >> 16) & 0xf];
					pADest[1] &= (uint16)nAlphaMaskInv; pADest[1] |= aAlpha[(nLo >> 20) & 0xf];
					pADest[2] &= (uint16)nAlphaMaskInv; pADest[2] |= aAlpha[(nLo >> 24) & 0xf];
					pADest[3] &= (uint16)nAlphaMaskInv; pADest[3] |= aAlpha[(nLo >> 28) & 0xf];
					pADest = (uint16*)((uint8*)pADest + pRequest->m_nDstPitch);

					nHi = *(uint32*)(pABlock + 4);

					pADest[0] &= (uint16)nAlphaMaskInv; pADest[0] |= aAlpha[ nHi        & 0xf];
					pADest[1] &= (uint16)nAlphaMaskInv; pADest[1] |= aAlpha[(nHi >>  4) & 0xf];
					pADest[2] &= (uint16)nAlphaMaskInv; pADest[2] |= aAlpha[(nHi >>  8) & 0xf];
					pADest[3] &= (uint16)nAlphaMaskInv; pADest[3] |= aAlpha[(nHi >> 12) & 0xf];
					pADest = (uint16*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] &= (uint16)nAlphaMaskInv; pADest[0] |= aAlpha[(nHi >> 16) & 0xf];
					pADest[1] &= (uint16)nAlphaMaskInv; pADest[1] |= aAlpha[(nHi >> 20) & 0xf];
					pADest[2] &= (uint16)nAlphaMaskInv; pADest[2] |= aAlpha[(nHi >> 24) & 0xf];
					pADest[3] &= (uint16)nAlphaMaskInv; pADest[3] |= aAlpha[(nHi >> 28) & 0xf];
				}
			}
		}
	}

	return 0;
}


uint32 sub_10078910(FormatMgr* pMgr, ConvertRequest* pRequest)
{
	FmtRead    read;
	FmtConvert convert;
	PValue     aColors[4];
	PValue     aAlpha[16];
	uint32     aC0[4];
	uint32     aC1[4];
	PValue     c[4];
	uint16     nColor0, nColor1;
	uint32     aBits[2];
	uint32     nBlocksX, nBlocksY, xb, yb;
	uint32     nBlockShift, nColorOffset;
	uint32     nAlphaOr, nAlpha8, nAlphaShift;
	uint8      bHasAlphaBlock, bInterpAlpha;
	uint32     nAlphaMaskInv;
	uint8*     pAlphaToTable;
	uint32     nType;

	nAlphaOr       = PVALUE_ALPHAMASK;
	bInterpAlpha   = 0;
	nAlpha8        = 0xff;
	bHasAlphaBlock = 0;

	nType = pRequest->m_pSrcFormat->m_nType;

	if (nType == FMTYPE_DXT3)
	{
		bHasAlphaBlock = 1;
	}
	else if (nType == FMTYPE_DXT5)
	{
		pAlphaToTable  = (uint8*)((uint32)pMgr->m_ToTables + (pRequest->m_pDstFormat->m_nBits[CP_ALPHA] << 8));
		nAlphaOr       = 0;
		nAlpha8        = 0;
		nAlphaShift    = pRequest->m_pDstFormat->m_nShifts[CP_ALPHA];
		bInterpAlpha   = 1;
		bHasAlphaBlock = 1;
	}

	nAlphaMaskInv = ~pRequest->m_pDstFormat->m_Masks[CP_ALPHA];
	read.Init(pMgr, &pMgr->m_Format565);
	convert.Init(pMgr, pRequest->m_pDstFormat);

	if (bHasAlphaBlock)
	{
		uint32 i;

		nColorOffset = 8;
		nBlockShift  = 4;

		for (i = 0; i < 16; i++)
			aAlpha[i] = convert.Convert32(&g_AlphaExpand4to8[i]);
	}
	else
	{
		nColorOffset = 0;
		nBlockShift  = 3;
	}

	nBlocksY = pRequest->m_nHeight >> 2;
	nBlocksX = pRequest->m_nWidth  >> 2;

	for (yb = 0; yb < nBlocksY; yb++)
	{
		for (xb = 0; xb < nBlocksX; xb++)
		{
			uint8*  pBlock;
			PValue* pDest;
			uint32  nBits;

			pBlock = pRequest->m_pSrcData + (xb << nBlockShift)
			         + ((yb * nBlocksX) << nBlockShift) + nColorOffset;

			nColor0 = ((uint16*)pBlock)[0];
			nColor1 = ((uint16*)pBlock)[1];

			c[0] = read.Read16(&nColor0);
			c[1] = read.Read16(&nColor1);

			PValue_Get(c[0], aC0[0], aC0[1], aC0[2], aC0[3]);
			PValue_Get(c[1], aC1[0], aC1[1], aC1[2], aC1[3]);

			c[0] |= nAlphaOr;
			c[1] |= nAlphaOr;

			if (nColor0 > nColor1)
			{
				c[2] = PValue_Set(nAlpha8, (aC0[1] * 2 + aC1[1]) / 3,
				                           (aC0[2] * 2 + aC1[2]) / 3,
				                           (aC0[3] * 2 + aC1[3]) / 3);
				c[3] = PValue_Set(nAlpha8, (aC0[1] + aC1[1] * 2) / 3,
				                           (aC0[2] + aC1[2] * 2) / 3,
				                           (aC0[3] + aC1[3] * 2) / 3);
			}
			else
			{
				c[3] = 0;
				c[2] = PValue_Set(nAlpha8, (aC0[1] + aC1[1]) / 2,
				                           (aC0[2] + aC1[2]) / 2,
				                           (aC0[3] + aC1[3]) / 2);
			}

			aColors[0] = convert.Convert32(&c[0]);
			aColors[1] = convert.Convert32(&c[1]);
			aColors[2] = convert.Convert32(&c[2]);
			aColors[3] = convert.Convert32(&c[3]);

			nBits = *(uint32*)(pBlock + 4);

			pDest = (PValue*)(pRequest->m_pDstData
			                  + (yb << 2) * pRequest->m_nDstPitch + xb * 16);

			pDest[0] = aColors[ nBits        & 3];
			pDest[1] = aColors[(nBits >>  2) & 3];
			pDest[2] = aColors[(nBits >>  4) & 3];
			pDest[3] = aColors[(nBits >>  6) & 3];
			pDest = (PValue*)((uint8*)pDest + pRequest->m_nDstPitch);

			pDest[0] = aColors[(nBits >>  8) & 3];
			pDest[1] = aColors[(nBits >> 10) & 3];
			pDest[2] = aColors[(nBits >> 12) & 3];
			pDest[3] = aColors[(nBits >> 14) & 3];
			pDest = (PValue*)((uint8*)pDest + pRequest->m_nDstPitch);

			pDest[0] = aColors[(nBits >> 16) & 3];
			pDest[1] = aColors[(nBits >> 18) & 3];
			pDest[2] = aColors[(nBits >> 20) & 3];
			pDest[3] = aColors[(nBits >> 22) & 3];
			pDest = (PValue*)((uint8*)pDest + pRequest->m_nDstPitch);

			pDest[0] = aColors[(nBits >> 24) & 3];
			pDest[1] = aColors[(nBits >> 26) & 3];
			pDest[2] = aColors[(nBits >> 28) & 3];
			pDest[3] = aColors[(nBits >> 30) & 3];

			if (bHasAlphaBlock)
			{
				uint8*  pABlock;
				PValue* pADest;

				pADest = (PValue*)(pRequest->m_pDstData
				                   + (yb << 2) * pRequest->m_nDstPitch + xb * 16);

				pABlock = pRequest->m_pSrcData + (xb << nBlockShift)
				          + ((yb * nBlocksX) << nBlockShift);

				if (bInterpAlpha)
				{
					uint32 nA, nB;

					aAlpha[0] = pABlock[0];
					aAlpha[1] = pABlock[1];

					nA = aAlpha[0];
					nB = aAlpha[1];

					if (nA > nB)
					{
						aAlpha[2] = ((nA * 6 + nB    ) / 7);
						aAlpha[3] = ((nA * 5 + nB * 2) / 7);
						aAlpha[4] = ((nA * 4 + nB * 3) / 7);
						aAlpha[5] = ((nA * 3 + nB * 4) / 7);
						aAlpha[6] = ((nA * 2 + nB * 5) / 7);
						aAlpha[7] = ((nA     + nB * 6) / 7);
					}
					else
					{
						aAlpha[2] = ((nA * 4 + nB    ) / 5);
						aAlpha[3] = ((nA * 3 + nB * 2) / 5);
						aAlpha[4] = ((nA * 2 + nB * 3) / 5);
						aAlpha[5] = ((nA     + nB * 4) / 5);
						aAlpha[6] = 0;
						aAlpha[7] = 255;
					}

					aAlpha[0] = (pAlphaToTable[aAlpha[0]] << nAlphaShift);
					aAlpha[1] = (pAlphaToTable[aAlpha[1]] << nAlphaShift);
					aAlpha[2] = (pAlphaToTable[aAlpha[2]] << nAlphaShift);
					aAlpha[3] = (pAlphaToTable[aAlpha[3]] << nAlphaShift);
					aAlpha[4] = (pAlphaToTable[aAlpha[4]] << nAlphaShift);
					aAlpha[5] = (pAlphaToTable[aAlpha[5]] << nAlphaShift);
					aAlpha[6] = (pAlphaToTable[aAlpha[6]] << nAlphaShift);
					aAlpha[7] = (pAlphaToTable[aAlpha[7]] << nAlphaShift);

					aBits[0] = *(uint32*)(pABlock + 2);
					aBits[1] = *(uint16*)(pABlock + 6);

					pADest[0] |= aAlpha[ aBits[0]        & 7];
					pADest[1] |= aAlpha[(aBits[0] >>  3) & 7];
					pADest[2] |= aAlpha[(aBits[0] >>  6) & 7];
					pADest[3] |= aAlpha[(aBits[0] >>  9) & 7];
					pADest = (PValue*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] |= aAlpha[(aBits[0] >> 12) & 7];
					pADest[1] |= aAlpha[(aBits[0] >> 15) & 7];
					pADest[2] |= aAlpha[(aBits[0] >> 18) & 7];
					pADest[3] |= aAlpha[(aBits[0] >> 21) & 7];
					pADest = (PValue*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] |= aAlpha[(aBits[0] >> 24) & 7];
					pADest[1] |= aAlpha[(aBits[0] >> 27) & 7];
					pADest[2] |= aAlpha[(aBits[0] >> 30) | ((aBits[1] & 1) << 2)];
					pADest[3] |= aAlpha[(aBits[1] >>  1) & 7];
					pADest = (PValue*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] |= aAlpha[(aBits[1] >>  4) & 7];
					pADest[1] |= aAlpha[(aBits[1] >>  7) & 7];
					pADest[2] |= aAlpha[(aBits[1] >> 10) & 7];
					pADest[3] |= aAlpha[(aBits[1] >> 13) & 7];
				}
				else
				{
					uint32 nLo, nHi;

					nLo = *(uint32*)pABlock;

					pADest[0] &= nAlphaMaskInv; pADest[0] |= aAlpha[ nLo        & 0xf];
					pADest[1] &= nAlphaMaskInv; pADest[1] |= aAlpha[(nLo >>  4) & 0xf];
					pADest[2] &= nAlphaMaskInv; pADest[2] |= aAlpha[(nLo >>  8) & 0xf];
					pADest[3] &= nAlphaMaskInv; pADest[3] |= aAlpha[(nLo >> 12) & 0xf];
					pADest = (PValue*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] &= nAlphaMaskInv; pADest[0] |= aAlpha[(nLo >> 16) & 0xf];
					pADest[1] &= nAlphaMaskInv; pADest[1] |= aAlpha[(nLo >> 20) & 0xf];
					pADest[2] &= nAlphaMaskInv; pADest[2] |= aAlpha[(nLo >> 24) & 0xf];
					pADest[3] &= nAlphaMaskInv; pADest[3] |= aAlpha[(nLo >> 28) & 0xf];
					pADest = (PValue*)((uint8*)pADest + pRequest->m_nDstPitch);

					nHi = *(uint32*)(pABlock + 4);

					pADest[0] &= nAlphaMaskInv; pADest[0] |= aAlpha[ nHi        & 0xf];
					pADest[1] &= nAlphaMaskInv; pADest[1] |= aAlpha[(nHi >>  4) & 0xf];
					pADest[2] &= nAlphaMaskInv; pADest[2] |= aAlpha[(nHi >>  8) & 0xf];
					pADest[3] &= nAlphaMaskInv; pADest[3] |= aAlpha[(nHi >> 12) & 0xf];
					pADest = (PValue*)((uint8*)pADest + pRequest->m_nDstPitch);

					pADest[0] &= nAlphaMaskInv; pADest[0] |= aAlpha[(nHi >> 16) & 0xf];
					pADest[1] &= nAlphaMaskInv; pADest[1] |= aAlpha[(nHi >> 20) & 0xf];
					pADest[2] &= nAlphaMaskInv; pADest[2] |= aAlpha[(nHi >> 24) & 0xf];
					pADest[3] &= nAlphaMaskInv; pADest[3] |= aAlpha[(nHi >> 28) & 0xf];
				}
			}
		}
	}

	return 0;
}


// ======================================================================= //
//
//  (was w22_100799b0_os.cpp) d3d.ren functions reached only through the
//  pixel-conversion dispatch table 0x100899b0 (d3dr2c_os.cpp's 0x1007a180
//  calls entry [pReq->m_nType] as fn(pMgr, pFmt, &nPixel, pOut)), written
//  for the whole-DLL link.  /Os group (w<N>_*_os.cpp, see build/engine.sh).
//
//      [1] 0x100799b0  pack a 32-bit ARGB pixel into an 8-bit format   NEAR
//      [2] 0x10079aa0  the same into a 16-bit format                   NEAR
//      [3] 0x10079b90  the same into a 32-bit format (d3dr5c.cpp)
//      [9] 0x10079c70, [10] 0x10079d70  unpack 8/16-bit to 8888 (d3dhigh.cpp)
//     [11] 0x10079e70  unpack a 32-bit pixel to 8888                  IDENTICAL
//
//  Why /Os: all three end in the /Os padding (`mov %esi,%esi` or four
//  0x90s), and 0x10079e70 is byte-identical at /Os (at /O2 it is 192
//  bytes, a `lea` nop run in the body) -- and here, at /Os /Oi- /Qip.
//
//  WHAT THE FAMILY SHARES, and what the siblings' decodes could not
//  explain -- both now reproduced by 0x10079e70:
//    * the `and $0xffffffff` on the source and destination pointers is
//      icl keeping a no-op mask of a pointer cast to an integer:
//      `(T*)((uint32)p & 0xffffffff)` compiles to exactly that (probed:
//      `& ~0UL`, `& (uint32)-1` and a 64-bit mask all keep the `and`);
//    * the "dead spills" are ARRAYS: the four table pointers are an array
//      of locals and the four channel values another, stored because icl
//      does no dead-store elimination through memory, then used from the
//      registers they were computed in.
//
// ======================================================================= //


// ======================================================================= //
//  0x100799b0 / 0x10079aa0 / 0x10079b90 -- one PValue packed into an 8-,
//  16- or 32-bit pixel: slots [1], [2], [3] of the 0x100899b0 table,
//  `void (FormatMgr*, PFormat*, void* pIn, void* pOut)`, cdecl
//  (lithtech's fm_WritePixel8/16/32: a converter built on the stack, used
//  once).  The tables are the integer sum `(bits << 8) + &m_ToTables`,
//  which recomputes `lea 0x70(pMgr)` for each of the four (the shipped
//  shape; a pointer sum or `m_ToTables + ...` CSEs it into one register).
//  The pixel pointers go through `& 0xffffffff`, and the channel bytes are
//  PValue_GetA/R/G/B: the alpha index then takes the table as the base
//  register, as shipped.
//  NEAR: the red, green and blue lookups come out `(index, table)` where
//  the shipped ones are `(table, index)` -- 42 / 38 bytes for 0x10079aa0 /
//  0x10079b90; 0x100799b0 is 41 with the same source in a probe TU and 128
//  here (the alpha lookup swaps too: which register gets the base depends
//  on the rest of the TU -- an unused struct type with one inline member
//  moves it).  Probed: a named index temp (uint32 or uint8), `& 0xff`,
//  `(uint8)` casts, `idx[table]` / `*(idx + table)`, the tables as a local
//  array, the member Init of the converter (fixes the lookups, loses the
//  table sums' operand order), the cast chain.
//  With 0x10079f30's switch body the TU state is the other
//  one: 0x100799b0 41, 0x10079aa0 42, 0x10079b90 116 (its alpha lookup
//  swapped as well); in BOTH states every R/G/B lookup is `(index,
//  table)`.  No effect on any byte: `(int)`, `(uint8)` and
//  `(int32)(uint8)` index casts, the pixel as PValue, the lookup written
//  `*(table + idx)` or as an integer sum `*(uint8*)((uint32)table + idx)`
//  (icl orders the address operands itself), and the file renamed to
//  stems of 20..50 characters.  convert.Init in place of pk_Init (now) is
//  81 / 169 / 156 and does not fix the lookups; with Init's sums
//  reordered `(bits << 8) + base` it flips the state back (128 / 42 / 38)
//  and costs 0x10077110 / 0x100773e0.
// ======================================================================= //
struct PackFmt100799b0
{
	uint8	m_pad_00[0x18];
	uint32	m_nTab[4];                                  // +0x18
	uint32	m_nShift[4];                                // +0x28
};

inline void pk_Init(FmtConvert* pConvert, uint8* pMgr, PackFmt100799b0* pFmt)
{
	pConvert->m_pFormat = (PFormat*)pFmt;
	pConvert->m_pTables[0] = (uint8*)((pFmt->m_nTab[0] << 8) + (uint32)((FormatMgr*)pMgr)->m_ToTables);
	pConvert->m_pTables[1] = (uint8*)((pFmt->m_nTab[1] << 8) + (uint32)((FormatMgr*)pMgr)->m_ToTables);
	pConvert->m_pTables[2] = (uint8*)((pFmt->m_nTab[2] << 8) + (uint32)((FormatMgr*)pMgr)->m_ToTables);
	pConvert->m_pTables[3] = (uint8*)((pFmt->m_nTab[3] << 8) + (uint32)((FormatMgr*)pMgr)->m_ToTables);
}

void sub_100799b0(uint8* pMgr, PackFmt100799b0* pFmt, void* pIn, void* pOut)
{
	FmtConvert convert;
	uint32* pSrc;
	uint8* pDst;
	uint32 nPixel;
	uint8 nOut;

	pk_Init(&convert, pMgr, pFmt);
	pSrc = (uint32*)((uint32)pIn & 0xffffffff);
	pDst = (uint8*)((uint32)pOut & 0xffffffff);

	nPixel = *pSrc;
	nOut  = convert.m_pTables[0][PValue_GetA(nPixel)] << pFmt->m_nShift[0];
	nOut |= convert.m_pTables[1][PValue_GetR(nPixel)] << pFmt->m_nShift[1];
	nOut |= convert.m_pTables[2][PValue_GetG(nPixel)] << pFmt->m_nShift[2];
	nOut |= convert.m_pTables[3][PValue_GetB(nPixel)] << pFmt->m_nShift[3];
	*pDst = nOut;
}

void sub_10079aa0(uint8* pMgr, PackFmt100799b0* pFmt, void* pIn, void* pOut)
{
	FmtConvert convert;
	uint32* pSrc;
	uint16* pDst;
	uint32 nPixel;
	uint16 nOut;

	pk_Init(&convert, pMgr, pFmt);
	pSrc = (uint32*)((uint32)pIn & 0xffffffff);
	pDst = (uint16*)((uint32)pOut & 0xffffffff);

	nPixel = *pSrc;
	nOut  = convert.m_pTables[0][PValue_GetA(nPixel)] << pFmt->m_nShift[0];
	nOut |= convert.m_pTables[1][PValue_GetR(nPixel)] << pFmt->m_nShift[1];
	nOut |= convert.m_pTables[2][PValue_GetG(nPixel)] << pFmt->m_nShift[2];
	nOut |= convert.m_pTables[3][PValue_GetB(nPixel)] << pFmt->m_nShift[3];
	*pDst = nOut;
}

// d3dr5c.cpp's spelling of the format (the symbol keeps its name).
struct Fmt10079b90
{
	uint8	m_pad_00[0x18];
	uint32	m_nTab0;                                    // +0x18
	uint32	m_nTab1;                                    // +0x1c
	uint32	m_nTab2;                                    // +0x20
	uint32	m_nTab3;                                    // +0x24
	uint32	m_nShift0;                                  // +0x28
	uint32	m_nShift1;                                  // +0x2c
	uint32	m_nShift2;                                  // +0x30
	uint32	m_nShift3;                                  // +0x34
};

void sub_10079b90(uint8* pTables, Fmt10079b90* pFmt, uint32* pSrcIn, uint32* pDstOut)
{
	FmtConvert convert;
	PackFmt100799b0* pFormat = (PackFmt100799b0*)pFmt;
	uint32* pSrc;
	uint32* pDst;
	uint32 nPixel;
	uint32 nOut;

	pk_Init(&convert, pTables, pFormat);
	pSrc = (uint32*)((uint32)pSrcIn & 0xffffffff);
	pDst = (uint32*)((uint32)pDstOut & 0xffffffff);

	nPixel = *pSrc;
	nOut  = convert.m_pTables[0][PValue_GetA(nPixel)] << pFormat->m_nShift[0];
	nOut |= convert.m_pTables[1][PValue_GetR(nPixel)] << pFormat->m_nShift[1];
	nOut |= convert.m_pTables[2][PValue_GetG(nPixel)] << pFormat->m_nShift[2];
	nOut |= convert.m_pTables[3][PValue_GetB(nPixel)] << pFormat->m_nShift[3];
	*pDst = nOut;
}


// ======================================================================= //
//  0x10079c70 / 0x10079d70 / 0x10079e70 -- one 8-, 16- or 32-bit pixel
//  unpacked to a PValue: slots [9], [10], [11] of the same table (moved
//  from d3dhigh.cpp and w22_100799b0_os.cpp; identical in both places).
// ======================================================================= //
struct PixelChannelFormat
{
	uint32  m_nPad00;                                   // +0x00
	uint32  m_nPad04;                                   // +0x04
	uint32  m_nMask[4];                                 // +0x08
	uint32  m_nRamp[4];                                 // +0x18
	uint32  m_nShift[4];                                // +0x28
};

struct PixelRampTables
{
	uint8   m_Pad000[0x970];                            // +0x000
	uint8*  m_pRamps[1];                                // +0x970
};

void sub_10079c70(PixelRampTables* pTables,
                  const PixelChannelFormat* pFormat,
                  const uint8* pIn, uint32* pOut)
{
	// The ramp pointers and channel values live in arrays and the two
	// pixel pointers go through `& 0xffffffff`: icl then keeps them in the
	// stack slots and emits the no-op `and $0xffffffff` the shipped body
	// has (w22_100799b0_os.cpp's 0x10079e70, the same shape).
	uint8*		pRamp[5];
	uint32		c[4];
	const uint8*	pSrc;
	uint32*		pDest;

	pRamp[1] = pTables->m_pRamps[pFormat->m_nRamp[0]];
	pRamp[2] = pTables->m_pRamps[pFormat->m_nRamp[1]];
	pRamp[3] = pTables->m_pRamps[pFormat->m_nRamp[2]];
	pRamp[4] = pTables->m_pRamps[pFormat->m_nRamp[3]];

	pSrc = (const uint8*)((uint32)pIn & 0xffffffff);
	pDest = (uint32*)((uint32)pOut & 0xffffffff);

	c[0] = pRamp[1][(*pSrc & pFormat->m_nMask[0]) >> pFormat->m_nShift[0]];
	c[1] = pRamp[2][(*pSrc & pFormat->m_nMask[1]) >> pFormat->m_nShift[1]];
	c[2] = pRamp[3][(*pSrc & pFormat->m_nMask[2]) >> pFormat->m_nShift[2]];
	c[3] = pRamp[4][(*pSrc & pFormat->m_nMask[3]) >> pFormat->m_nShift[3]];

	*pDest = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}


// 0x10079d70 -- the uint16 instantiation. Four bytes differ from
// 0x10079c70 and they are all the source load width.
void sub_10079d70(PixelRampTables* pTables,
                  const PixelChannelFormat* pFormat,
                  const uint16* pIn, uint32* pOut)
{
	// The ramp pointers and channel values live in arrays and the two
	// pixel pointers go through `& 0xffffffff`: icl then keeps them in the
	// stack slots and emits the no-op `and $0xffffffff` the shipped body
	// has (w22_100799b0_os.cpp's 0x10079e70, the same shape).
	uint8*		pRamp[5];
	uint32		c[4];
	const uint16*	pSrc;
	uint32*		pDest;

	pRamp[1] = pTables->m_pRamps[pFormat->m_nRamp[0]];
	pRamp[2] = pTables->m_pRamps[pFormat->m_nRamp[1]];
	pRamp[3] = pTables->m_pRamps[pFormat->m_nRamp[2]];
	pRamp[4] = pTables->m_pRamps[pFormat->m_nRamp[3]];

	pSrc = (const uint16*)((uint32)pIn & 0xffffffff);
	pDest = (uint32*)((uint32)pOut & 0xffffffff);

	c[0] = pRamp[1][(*pSrc & pFormat->m_nMask[0]) >> pFormat->m_nShift[0]];
	c[1] = pRamp[2][(*pSrc & pFormat->m_nMask[1]) >> pFormat->m_nShift[1]];
	c[2] = pRamp[3][(*pSrc & pFormat->m_nMask[2]) >> pFormat->m_nShift[2]];
	c[3] = pRamp[4][(*pSrc & pFormat->m_nMask[3]) >> pFormat->m_nShift[3]];

	*pDest = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}


struct UnpackFmt10079e70
{
	uint32	m_nPad00;
	uint32	m_nPad04;
	uint32	m_nMask[4];                                 // +0x08
	uint32	m_nRamp[4];                                 // +0x18
	uint32	m_nShift[4];                                // +0x28
};

struct UnpackTables10079e70
{
	uint8	m_Pad000[0x970];
	uint8*	m_pRamps[1];                                // +0x970
};

void sub_10079e70(UnpackTables10079e70* pTables,
                  const UnpackFmt10079e70* pFormat, void* pIn, void* pOut)
{
	uint8*	pRamp[5];
	uint32	c[4];
	uint32*	pSrc;
	uint32*	pDest;

	pRamp[1] = pTables->m_pRamps[pFormat->m_nRamp[0]];
	pRamp[2] = pTables->m_pRamps[pFormat->m_nRamp[1]];
	pRamp[3] = pTables->m_pRamps[pFormat->m_nRamp[2]];
	pRamp[4] = pTables->m_pRamps[pFormat->m_nRamp[3]];

	pSrc = (uint32*)((uint32)pIn & 0xffffffff);
	pDest = (uint32*)((uint32)pOut & 0xffffffff);

	c[0] = (uint8)(*pSrc >> pFormat->m_nShift[0]);
	c[1] = (uint8)(*pSrc >> pFormat->m_nShift[1]);
	c[2] = (uint8)(*pSrc >> pFormat->m_nShift[2]);
	c[3] = (uint8)(*pSrc >> pFormat->m_nShift[3]);

	*pDest = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}


// ---------------------------------------------------------------------- //
//  0x10079f30 (the row says 0x10079f3c) -- SIZE OF A SURFACE IN BYTES.
//
//  The row is the BODY. The FUNCTION starts twelve bytes earlier
//  at 0x10079f30 with the unknown compiler's calling-convention shim:
//
//      8b 44 24 04    mov 0x4(%esp),%eax     format
//      8b 54 24 08    mov 0x8(%esp),%edx     width
//      8b 4c 24 0c    mov 0xc(%esp),%ecx     height
//
//  -- exactly the three-argument shim already seen at
//  0x10039bd0. Its interior is jumped to directly from 0x10073cc7 (the
//  "1 caller" the row reports is a caller of the BODY, past the shim,
//  from code that already has the arguments in registers). The shim is a
//  separate 4-byte-aligned entry so external stack-ABI callers work.
//
//  The table it indexes is at 0x10089890 and reads
//
//      [0]=1  [1]=1  [2]=2  [3]=4  [4]=0  [5]=0  [6]=0  [7]=1
//
//  and formats 4, 5 and 6 -- exactly the three whose table entries are
//  ZERO -- are the ones the code special-cases before it ever reads the
//  table. That agreement is what pins the decode: the table is
//  bytes-per-pixel, the three holes are the compressed formats, 4 is the
//  half-byte one and 5 and 6 are the one-byte ones. DXT1 / DXT3 / DXT5 in
//  that order, but the arithmetic stands whatever they are called.
//
//  Control flow, read off the bytes: the `cmp $4` is done TWICE -- once in
//  the three-way membership test and again inside it to separate 4 from
//  5 and 6 -- which is what a `case 4: ... case 5: case 6:` switch turned
//  into a comparison chain looks like, and is why it is written as nested
//  ifs below rather than as a switch (a switch on 0..7 gives cl a jump
//  table here).
//
//  THE SHIM is icl's /Qip stack entry: identical here, in the copier's TU
//  (was d3dhigh.cpp).  The body is a NEAR, 17 of 47 bytes: shipped hosts
//  the `== 4` arm (`imul; shr; jmp`) AFTER the uncompressed return, ours
//  before it.  What the bytes pinned: the redundant second `cmp $4` is an
//  inline membership test (IsComp4; over a macro or written out icl
//  threads it away), and `!= 4` first puts the w*h arm in line with the
//  `>> 1` arm jumping back to its `mov`.  Probed without moving the
//  uncompressed return: early `!IsComp4` return, if/else, a result
//  variable, a `>>= 1` on one product, switch (either order), ternaries,
//  a Half() / Plain() inline, lithtech's fm_GetImageDataSize spelling, and
//  /Os, /O2 /Qip, /Os /Qip /Qip_no_inlining, /O2 /Qip /Qip_no_inlining
//  (16-17 at best).
//
//  IDENTICAL: the switch-on-comparison sink lever
//  (w7_10040cc0.cpp's 0x10040cc0, w26_10031e10.cpp): `switch (nFormat ==
//  4)` with the `>> 1` arm as `default` -- icl sinks the default arm to
//  the end, and with both arms returning it is the shipped `imul; shr;
//  jmp` back to the shared `mov; ret`.  A result variable assigned in the
//  switch is 10 bytes off; `switch (nFormat != 4)` with the arms swapped
//  is 23.  Side effect (any edit to this body does it, as seen with
//  an int parameter): the TU's IPO state flips the single-pixel packers --
//  0x100799b0 128 -> 41, 0x10079b90 38 -> 116 -- and 0x10074350 681 ->
//  687, 0x10074ea0 1469 -> 1482, 0x10078910 3952 -> 3902; no identical
//  in the file moves.
// ---------------------------------------------------------------------- //

// 0x10089890 -- bytes per pixel, indexed by surface format.
extern uint32 g_nFormatBytesPerPixel[8];

inline int IsComp4(uint32 n) { return n == 4 || n == 5 || n == 6; }

uint32 sub_10079f30(uint32 nFormat, uint32 nWidth, uint32 nHeight)
{
	if (IsComp4(nFormat))
	{
		switch (nFormat == 4)
		{
			case 0:  return nWidth * nHeight;
			default: return (nWidth * nHeight) >> 1;
		}
	}

	return g_nFormatBytesPerPixel[nFormat] * nHeight * nWidth;
}


// ======================================================================= //
//  0x10079f80 / 0x10079f84 -- PFormat::IsSame (was d3dbatcha.cpp), 4 + 68
//  bytes, 8 callers, all converters above
//
//  A /Qip pair: the shim `mov 0x4(%esp),%eax` at 0x10079f80, the body at
//  0x10079f84, which the converters enter (`push other; mov this,%ecx;
//  call 0x10079f84`).  Both identical here.  __thiscall, one argument,
//  answers 0 or 1:
//
//      mov 0x4(%ecx),%edx / cmp 0x4(%eax),%edx / jne false
//      +0x08, +0x0c, +0x10, +0x14 the same way
//      xor %eax,%eax / inc %eax / jmp end
//    false: xor %eax,%eax
//    end:  add $0x4,%esp / pop %ebx / ret $0x4
//
//  WHAT PINS IT: five dwords compared, at +0x04 through +0x14, and +0x00 is
//  NOT compared -- so the object is at least 0x18 wide and its first dword
//  is not part of its identity.  All five `jne`s share ONE `xor %eax,%eax`
//  placed after the true arm, and the true arm is `xor %eax,%eax; inc %eax`
//  and comes FIRST: that is the layout of a single `&&` chain, not of five
//  early `return LTFALSE`s (which would put the true arm last).
// ======================================================================= //

unsigned int C10079f84::sub_10079f80(C10079f84* pOther)
{
	return (m_nType == pOther->m_nType &&
	        m_Masks[0] == pOther->m_Masks[0] &&
	        m_Masks[1] == pOther->m_Masks[1] &&
	        m_Masks[2] == pOther->m_Masks[2] &&
	        m_Masks[3] == pOther->m_Masks[3]);
}
