// p3_ttf.h -- the menus' text from a TrueType font (MODERN.md, M-P3-24).
// Compiled only into the modern build.
//
// The game's menu fonts are bitmaps drawn for a 640x480 screen; scaled up
// (M-P3-23) they are soft and blocky. With MenuFont 1 the menu fonts (small,
// medium, large, help and title) are drawn instead from fonts\BowlbyOneSC-Regular.ttf
// (SIL Open Font License, fonts\BowlbyOneSC-OFL.txt), at the size the screen needs:
// Windows draws its glyphs, white on black and antialiased, onto a sheet the
// game's bitmap-font code then draws from as it does from its own images.

#ifndef P3_TTF_H
#define P3_TTF_H

class CLTGUIFont;

// The menu font in use (MenuFont, and the font file loaded): nonzero for
// the TrueType font.
int         P3_TTF_Enabled();

// The font remade from the TrueType font, its glyphs nRetailHeight (the
// retail bitmap font's height) times fScale pixels high. False when the
// TrueType font is not in use or could not be drawn; the font is then as
// it was.
bool        P3_TTF_MakeFont(CLTGUIFont *pFont, int nRetailHeight, float fScale, const char *szRefName);

// M-P3-25: the developer console's font (M-P2-01), Noto Sans Mono
// (fonts\NotoSansMono-Regular.ttf) at a size for nScreenHeight, remade
// when that changes; NULL when the TrueType fonts are not in use (MenuFont
// 0) or it could not be drawn, and the console then uses the small font.
CLTGUIFont *P3_TTF_ConsoleFont(int nScreenHeight);

#endif // P3_TTF_H
