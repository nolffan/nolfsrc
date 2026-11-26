// *********************************************************************** //
//
//  w12_3b3d0_os.cpp -- 0x1003b3d0 (695 bytes): d3d_AddDirtyRect(pRect), the
//  renderer's dirty-rectangle list (count 0x1008e730, d3dneutral.cpp's
//  g_1008e730; entries 0x1008e734, d3dmid.cpp's g_DirtyRects[100], 16-byte
//  signed RECTs). d3dblit.cpp calls it after a screen lock and a blit.
//
//  Read off the shipped body:
//    * NULL: the whole window -- entry 0 = { 0, 0, width (0x1008cbb4),
//      height (0x1008cbb0) } and the count 1;
//    * an empty list: append;
//    * 99 or more entries: collapse to the whole window (a recursive
//      call with NULL);
//    * already inside an entry: nothing to do;
//    * otherwise the entry whose union with the new rectangle saves the
//      most area (union smaller than the two areas summed; the biggest
//      saving wins, the first on a tie) is replaced by that union, and if
//      no union saves anything the rectangle is appended.
//  Each union is built in a zeroed local and copied (`rep movsl`): the
//  zeroing is the REAL LTRect's constructor (src/LT2/sdk/inc/ltrect.h,
//  defaults 0,0,0,0) -- the local is constructed where it is declared,
//  after the empty-list append, which is where the shipped zero stores sit.
//
//  Measured as differing lines against the shipped body:
//  hand-copied LTRect with explicit zeroing 239; the real header 152;
//  `merged`/iBest declared after the early cases 148; if/else instead of
//  early returns (only the NULL case returns) 138; containment as an
//  inline pointer helper 114; the areas as an inline RectArea(const
//  LTRect*) 10 (695 bytes, size-exact); NULL case's `return` 9.
//  IDENTICAL: the 99-entry recursive call is placed out of line, after
//  the first-pick arm.  As an `if` icl keeps it on the fall-through into
//  the epilogue (3 bytes over); written as a `switch` on the comparison
//  with the call in the `default` arm (the lever of w26_100262f0.cpp and
//  w59_1007dd24_osoi.cpp) icl sinks it as shipped.  Calling the collapse
//  by a name holding one of mcpcom.exe's cold words (`d3d_InvalidateRect`)
//  also sinks it, but the real name is not known.  Measured as an `if`
//  with no effect: `> 98`, `< 99` with the arms swapped, `LTNULL`, a
//  `return` after the call, else-if chains; as a switch on `< 99` with the
//  call in `case 0` it stays inline (700 bytes).

//  References: d3dblit.cpp's d3d_AddDirtyRect(LTRect*), aliased onto this
//  definition in link/aliases.rsp.
//
// *********************************************************************** //

#include "ltbasedefs.h"

extern uint32 g_1008e730;                               // 0x1008e730 dirty-rect count
extern LTRect g_DirtyRects[100];                        // 0x1008e734
extern uint32 g_nWindowWidth;                           // 0x1008cbb4
extern uint32 g_nWindowHeight;                          // 0x1008cbb0

static inline LTRect UnionRect3b3d0(const LTRect* a, const LTRect* b)
{
	LTRect r;

	r.left   = (a->left   < b->left)   ? a->left   : b->left;
	r.top    = (a->top    < b->top)    ? a->top    : b->top;
	r.right  = (a->right  > b->right)  ? a->right  : b->right;
	r.bottom = (a->bottom > b->bottom) ? a->bottom : b->bottom;
	return r;
}

static inline int RectInside3b3d0(const LTRect* pOuter, const LTRect* pInner)
{
	return pOuter->left <= pInner->left && pOuter->top <= pInner->top &&
	       pOuter->right >= pInner->right && pOuter->bottom >= pInner->bottom;
}

static inline int32 RectArea3b3d0(const LTRect* pRect)
{
	return (pRect->bottom - pRect->top) * (pRect->right - pRect->left);
}

void sub_1003b3d0(LTRect* pRect)
{
	uint32 i;
	int32  bestSaving, areaSum, areaMerged;

	if (!pRect)
	{
		g_DirtyRects[0].left   = 0;
		g_DirtyRects[0].top    = 0;
		g_DirtyRects[0].right  = g_nWindowWidth;
		g_DirtyRects[0].bottom = g_nWindowHeight;
		g_1008e730 = 1;
		return;
	}

	if (!g_1008e730)
	{
		g_DirtyRects[g_1008e730] = *pRect;
		g_1008e730++;
	}
	else
	{
		LTRect merged;
		int32  iBest = -1;

		switch (g_1008e730 >= 99)
		{
		default:
			sub_1003b3d0(0);
			break;
		case 0:
			for (i = 0; i < g_1008e730; i++)
			{
				if (RectInside3b3d0(&g_DirtyRects[i], pRect))
				{
					return;
				}
			}

			for (i = 0; i < g_1008e730; i++)
			{
				merged = UnionRect3b3d0(&g_DirtyRects[i], pRect);

				areaSum = RectArea3b3d0(&g_DirtyRects[i]) + RectArea3b3d0(pRect);
				areaMerged = RectArea3b3d0(&merged);

				if (areaMerged < areaSum)
				{
					if (iBest < 0 || (areaSum - areaMerged) > bestSaving)
					{
						bestSaving = areaSum - areaMerged;
						iBest = i;
					}
				}
			}

			if (iBest < 0)
			{
				g_DirtyRects[g_1008e730++] = *pRect;
			}
			else
			{
				g_DirtyRects[iBest] = UnionRect3b3d0(&g_DirtyRects[iBest], pRect);
			}
			break;
		}
	}
}
