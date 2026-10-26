// p3_ttf.cpp -- the menus' text from a TrueType font (MODERN.md, M-P3-24).
// See p3_ttf.h.

#include "stdafx.h"
#include "VarTrack.h"
#include "ltguimgr.h"
#include "cshell/p3_ui.h"
#include "cshell/p3_ttf.h"

#define P3_TTF_SHEET_WIDTH  2048        // within every renderer's texture size
#define P3_TTF_PAD          2           // black columns and rows around a glyph
#define P3_TTF_FIRST        33          // '!' .. '~', the bitmap fonts' characters
#define P3_TTF_COUNT        94

#ifndef FR_PRIVATE
#define FR_PRIVATE          0x10
#endif
#ifndef ANTIALIASED_QUALITY
#define ANTIALIASED_QUALITY 4
#endif

typedef int (WINAPI *P3AddFontResourceEx_t)(LPCSTR, DWORD, PVOID);

// The fonts: each file, added for this process only (it is not installed),
// and the face it holds.
enum { P3_TTF_MENU, P3_TTF_CONSOLE, P3_TTF_FACES };
static const char *s_aszFile[P3_TTF_FACES] = { "fonts\\BowlbyOneSC-Regular.ttf", "fonts\\NotoSansMono-Regular.ttf" };
static const char *s_aszFace[P3_TTF_FACES] = { "Bowlby One SC", "Noto Sans Mono" };

VarTrack g_vtP3MenuFont;

static int P3_TTF_Load(int nFace)
{
	static int s_anLoaded[P3_TTF_FACES] = { -1, -1 };
	if (s_anLoaded[nFace] >= 0)
		return s_anLoaded[nFace];

	s_anLoaded[nFace] = 0;
	HMODULE hGdi = GetModuleHandleA("gdi32.dll");
	P3AddFontResourceEx_t pAdd = hGdi ? (P3AddFontResourceEx_t)GetProcAddress(hGdi, "AddFontResourceExA") : NULL;
	if (pAdd && pAdd(s_aszFile[nFace], FR_PRIVATE, NULL) > 0)
		s_anLoaded[nFace] = 1;
	else
		g_pLTClient->CPrint("MenuFont: could not load %s; the bitmap fonts are used instead", s_aszFile[nFace]);
	return s_anLoaded[nFace];
}

static int P3_TTF_Wanted()
{
	if (!g_vtP3MenuFont.IsInitted())
		g_vtP3MenuFont.Init(g_pLTClient, "MenuFont", LTNULL, 1.0f);
	return g_vtP3MenuFont.GetFloat(1.0f) != 0.0f;
}

int P3_TTF_Enabled()
{
	return P3_TTF_Wanted() && P3_TTF_Load(P3_TTF_MENU);
}

static bool P3_TTF_MakeFace(CLTGUIFont *pFont, int nFace, int nHeight, const char *szRefName);

bool P3_TTF_MakeFont(CLTGUIFont *pFont, int nRetailHeight, float fScale, const char *szRefName)
{
	if (!pFont || !P3_TTF_Enabled())
		return false;
	return P3_TTF_MakeFace(pFont, P3_TTF_MENU, (int)((float)nRetailHeight * fScale + 0.5f), szRefName);
}

CLTGUIFont *P3_TTF_ConsoleFont(int nScreenHeight)
{
	static CLTGUIFont *s_pFont = NULL;
	static int s_nHeight = 0;			// the height last asked for
	static bool s_bMade = false;		// and whether it was made

	if (!P3_TTF_Wanted() || !P3_TTF_Load(P3_TTF_CONSOLE))
		return NULL;

	// About 22 lines in the console's half of a 1080-line screen; never
	// smaller than the retail small font.
	int nHeight = nScreenHeight / 48;
	if (nHeight < 14)
		nHeight = 14;
	if (nHeight != s_nHeight)
	{
		if (!s_pFont)
			s_pFont = new CLTGUIFont;
		s_nHeight = nHeight;
		s_bMade = s_pFont && P3_TTF_MakeFace(s_pFont, P3_TTF_CONSOLE, nHeight, "P3TTF_CONSOLE");
	}
	return s_bMade ? s_pFont : NULL;
}

// The font remade from face nFace, its cells nHeight pixels high.
static bool P3_TTF_MakeFace(CLTGUIFont *pFont, int nFace, int nHeight, const char *szRefName)
{
	if (nHeight < 6)
		nHeight = 6;

	HDC hDC = CreateCompatibleDC(NULL);
	if (!hDC)
		return false;
	// A positive height is the cell's (ascent and descent), as the bitmap
	// fonts' images are.
	HFONT hFont = CreateFontA(nHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
							  OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
							  DEFAULT_PITCH | FF_DONTCARE, s_aszFace[nFace]);
	if (!hFont)
	{
		DeleteDC(hDC);
		return false;
	}
	HGDIOBJ hOldFont = SelectObject(hDC, hFont);

	TEXTMETRICA tm;
	GetTextMetricsA(hDC, &tm);
	uint32 nCell = (uint32)tm.tmHeight;
	SIZE szSpace;
	uint32 nSpace = GetTextExtentPoint32A(hDC, " ", 1, &szSpace) ? (uint32)szSpace.cx : 0;

	// Each glyph's box: its advance, widened to its ink where that reaches
	// past it, with black around it; boxes flow into rows.
	uint32 aX[P3_TTF_COUNT], aY[P3_TTF_COUNT], aW[P3_TTF_COUNT];
	int    aDrawX[P3_TTF_COUNT];
	uint32 x = 0, y = 0, nWidest = 0;
	int i;
	for (i = 0; i < P3_TTF_COUNT; i++)
	{
		ABC abc;
		int nAdvance, nLeft = 0, nRight;
		if (GetCharABCWidthsA(hDC, P3_TTF_FIRST + i, P3_TTF_FIRST + i, &abc))
		{
			nAdvance = abc.abcA + (int)abc.abcB + abc.abcC;
			if (abc.abcA < 0)
				nLeft = -abc.abcA;      // ink left of the pen
			nRight = abc.abcA + (int)abc.abcB;
			if (nRight < nAdvance)
				nRight = nAdvance;
		}
		else
		{
			SIZE sz;
			char c = (char)(P3_TTF_FIRST + i);
			GetTextExtentPoint32A(hDC, &c, 1, &sz);
			nAdvance = nRight = sz.cx;
		}
		if (nAdvance < 1)
			nAdvance = 1;
		uint32 nBox = (uint32)(nLeft + nRight) + 2 * P3_TTF_PAD;
		if (x + nBox > P3_TTF_SHEET_WIDTH && x > 0)
		{
			x = 0;
			y += nCell + 2 * P3_TTF_PAD;
		}
		// The glyph's source rectangle is its advance from the pen; the ink
		// either side of it is drawn too (the box), but only the advance is
		// copied, as the retail fonts' glyphs are as wide as they step.
		aDrawX[i] = (int)x + P3_TTF_PAD + nLeft;
		aX[i] = (uint32)aDrawX[i];
		aY[i] = y + P3_TTF_PAD;
		aW[i] = (uint32)nAdvance;
		x += nBox;
		if (x > nWidest)
			nWidest = x;
	}
	uint32 nSheetW = nWidest, nSheetH = y + nCell + 2 * P3_TTF_PAD;

	// The glyphs, white on black, onto a 32-bit DIB.
	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biWidth = (LONG)nSheetW;
	bmi.bmiHeader.biHeight = -(LONG)nSheetH;          // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	void *pBits = NULL;
	HBITMAP hBmp = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
	bool bOK = false;
	if (hBmp && pBits)
	{
		HGDIOBJ hOldBmp = SelectObject(hDC, hBmp);
		memset(pBits, 0, nSheetW * nSheetH * 4);
		SetBkMode(hDC, TRANSPARENT);
		SetTextColor(hDC, RGB(255, 255, 255));
		SetTextAlign(hDC, TA_LEFT | TA_TOP);
		for (i = 0; i < P3_TTF_COUNT; i++)
		{
			char c = (char)(P3_TTF_FIRST + i);
			TextOutA(hDC, aDrawX[i], (int)aY[i], &c, 1);
		}
		GdiFlush();

		// Onto an engine surface: black, then the lit pixels.
		HSURFACE hSheet = g_pLTClient->CreateSurface(nSheetW, nSheetH);
		if (hSheet)
		{
			LTRect rcAll(0, 0, (int)nSheetW, (int)nSheetH);
			g_pLTClient->FillRect(hSheet, &rcAll, SETRGB(0, 0, 0));
			const uint8 *p = (const uint8*)pBits;
			for (uint32 py = 0; py < nSheetH; py++)
			{
				for (uint32 px = 0; px < nSheetW; px++, p += 4)
				{
					uint8 v = p[0] > p[1] ? p[0] : p[1];
					if (p[2] > v) v = p[2];
					if (v)
						g_pLTClient->SetPixel(hSheet, px, py, SETRGB(v, v, v));
				}
			}
			bOK = pFont->InitFromGlyphs(g_pLTClient, hSheet, (char*)szRefName, nCell, aX, aY, aW, nSpace) != LTFALSE;
			if (!bOK)
				g_pLTClient->DeleteSurface(hSheet);
		}
		SelectObject(hDC, hOldBmp);
	}
	if (hBmp)
		DeleteObject(hBmp);
	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);
	DeleteDC(hDC);
	return bOK;
}
