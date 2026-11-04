/*
 * tkAndroidFont.c --
 *
 *	Tk's font hooks on top of a TkATextEngine (FreeType in tests, Android's
 *	android.graphics.Paint on a device). Tk asks for fonts by attributes;
 *	the engine reports metrics, advance widths and renders glyph runs.
 */

#include "tkAndroidPriv.h"
#include "tkFont.h"

const TkATextEngine *tkAEngine;

void
TkAndroidSetTextEngine(const TkATextEngine *engine)
{
    tkAEngine = engine;
}

typedef struct AFont {
    TkFont font;		/* must be first */
    void *eng;
    int ascii[128];		/* advance cache, -1 = unknown */
} AFont;

static void
InitFont(AFont *f, Tk_Window tkwin, const TkFontAttributes *fa)
{
    TkAFontInfo info;
    const char *family = fa->family != NULL ? fa->family : "sans-serif";
    int px = (int) (TkFontGetPixels(tkwin, fa->size) + 0.5);
    int i;

    if (px < 6) px = 6;
    f->eng = tkAEngine->open(family, px, fa->weight == TK_FW_BOLD, fa->slant == TK_FS_ITALIC, &info);
    if (f->eng == NULL) {
	f->eng = tkAEngine->open(NULL, px, fa->weight == TK_FW_BOLD, fa->slant == TK_FS_ITALIC, &info);
    }
    f->font.fid = (Font) (uintptr_t) f;
    f->font.fa = *fa;
    f->font.fa.family = Tk_GetUid(family);
    f->font.fm.ascent = info.ascent;
    f->font.fm.descent = info.descent;
    f->font.fm.maxWidth = info.maxWidth;
    f->font.fm.fixed = info.fixed;
    for (i = 0; i < 128; i++) {
	f->ascii[i] = -1;
    }
}

TkFont *
TkpGetFontFromAttributes(TkFont *tkFontPtr, Tk_Window tkwin, const TkFontAttributes *faPtr)
{
    AFont *f;

    if (tkFontPtr == NULL) {
	f = (AFont *) ckalloc(sizeof(AFont));
	memset(f, 0, sizeof(AFont));
    } else {
	f = (AFont *) tkFontPtr;
	TkpDeleteFont(tkFontPtr);
    }
    InitFont(f, tkwin, faPtr);
    return &f->font;
}

TkFont *TkpGetNativeFont(Tk_Window tkwin, const char *name) { (void) tkwin; (void) name; return NULL; }

void
TkpDeleteFont(TkFont *tkFontPtr)
{
    AFont *f = (AFont *) tkFontPtr;

    if (f->eng != NULL) {
	TkAFlushTextCache();	/* cached masks are keyed by the engine's font handle */
	tkAEngine->close(f->eng);
	f->eng = NULL;
    }
}

/*
 * Named system fonts. Sizes are in points and scale with the screen density
 * (tk scaling = dpi/72): 8 pt is about 2.8 mm tall, comfortable on a phone.
 */
void
TkpFontPkgInit(TkMainInfo *mainPtr)
{
    static const struct { const char *name, *family; int size; int bold; } fonts[] = {
	{"TkDefaultFont", "sans-serif", 8, 0},
	{"TkTextFont", "sans-serif", 8, 0},
	{"TkFixedFont", "monospace", 8, 0},
	{"TkMenuFont", "sans-serif", 8, 0},
	{"TkHeadingFont", "sans-serif", 8, 1},
	{"TkCaptionFont", "sans-serif", 8, 1},
	{"TkSmallCaptionFont", "sans-serif", 7, 0},
	{"TkIconFont", "sans-serif", 8, 0},
	{"TkTooltipFont", "sans-serif", 7, 0},
	{NULL, NULL, 0, 0}
    };
    Tk_Window tkwin = (Tk_Window) mainPtr->winPtr;
    int i;

    /* Called before the main window is fully linked to its TkMainInfo. */
    if (mainPtr->winPtr->mainPtr == NULL) {
	mainPtr->winPtr->mainPtr = mainPtr;
    }
    for (i = 0; fonts[i].name != NULL; i++) {
	TkFontAttributes fa;

	TkInitFontAttributes(&fa);
	fa.family = Tk_GetUid(fonts[i].family);
	fa.size = fonts[i].size;
	fa.weight = fonts[i].bold ? TK_FW_BOLD : TK_FW_NORMAL;
	fa.slant = TK_FS_ROMAN;
	if (TkCreateNamedFont(mainPtr->interp, tkwin, fonts[i].name, &fa) != TCL_OK) {
	    TkALog("named font %s failed: %s", fonts[i].name, Tcl_GetStringResult(mainPtr->interp));
	}
    }
}

static void
AddFamily(void *ctx, const char *name)
{
    Tcl_ListObjAppendElement(NULL, (Tcl_Obj *) ctx, Tcl_NewStringObj(name, TCL_AUTO_LENGTH));
}

void
TkpGetFontFamilies(Tcl_Interp *interp, Tk_Window tkwin)
{
    Tcl_Obj *list = Tcl_NewObj();

    (void) tkwin;
    if (tkAEngine != NULL && tkAEngine->families != NULL) {
	tkAEngine->families(AddFamily, list);
    }
    Tcl_SetObjResult(interp, list);
}

void TkpGetSubFonts(Tcl_Interp *interp, Tk_Font tkfont) { (void) tkfont; Tcl_SetObjResult(interp, Tcl_NewObj()); }

void
TkpGetFontAttrsForChar(Tk_Window tkwin, Tk_Font tkfont, int c, struct TkFontAttributes *faPtr)
{
    (void) tkwin; (void) c;
    *faPtr = ((TkFont *) tkfont)->fa;
}

/* ---- measuring ------------------------------------------------------------------------------------ */

static int
CharAdvance(AFont *f, const char *p, int len, int cp)
{
    int w;

    if (cp >= 0 && cp < 128 && f->ascii[cp] >= 0) {
	return f->ascii[cp];
    }
    w = tkAEngine->measure(f->eng, p, len);
    if (cp >= 0 && cp < 128) {
	f->ascii[cp] = w;
    }
    return w;
}

int
Tk_MeasureChars(Tk_Font tkfont, const char *source, Tcl_Size numBytes, int maxPixels,
	int flags, int *lengthPtr)
{
    AFont *f = (AFont *) tkfont;
    int i = 0, curX = 0, lastBreak = -1, lastBreakX = 0;
    int stoppedEarly = 0;

    while (i < numBytes) {
	int cp, len = (int) Tcl_UtfToUniChar(source + i, &cp), w;

	if (len < 1) len = 1;
	if (i + len > numBytes) len = (int) numBytes - i;
	w = CharAdvance(f, source + i, len, cp);
	if (maxPixels >= 0 && curX + w > maxPixels) {
	    if ((flags & TK_PARTIAL_OK) && curX < maxPixels) {
		curX += w;
		i += len;
	    }
	    stoppedEarly = 1;
	    break;
	}
	curX += w;
	i += len;
	if (cp == ' ' || cp == '\t') {
	    lastBreak = i;
	    lastBreakX = curX;
	}
    }
    if (stoppedEarly && (flags & TK_WHOLE_WORDS) && lastBreak > 0) {
	i = lastBreak;
	curX = lastBreakX;
    }
    if (i == 0 && (flags & TK_AT_LEAST_ONE) && numBytes > 0) {
	int cp, len = (int) Tcl_UtfToUniChar(source, &cp);

	if (len < 1) len = 1;
	i = len;
	curX = CharAdvance(f, source, len, cp);
    }
    *lengthPtr = curX;
    return i;
}

int
Tk_MeasureCharsInContext(Tk_Font tkfont, const char *string, Tcl_Size numBytes,
	Tcl_Size rangeStart, Tcl_Size rangeLength, int maxPixels, int flags, int *lengthPtr)
{
    (void) numBytes;
    return Tk_MeasureChars(tkfont, string + rangeStart, rangeLength, maxPixels, flags, lengthPtr);
}

/* ---- drawing ------------------------------------------------------------------------------------------ */

void
Tk_DrawChars(Display *display, Drawable drawable, GC gc, Tk_Font tkfont,
	const char *source, Tcl_Size numBytes, int x, int y)
{
    AFont *f = (AFont *) tkfont;
    int width = 0;

    TkADrawText(display, drawable, gc, f->eng, source, (int) numBytes, x, y);
    if (f->font.fa.underline || f->font.fa.overstrike) {
	Tk_MeasureChars(tkfont, source, numBytes, -1, 0, &width);
	if (f->font.fa.underline) {
	    XFillRectangle(display, drawable, gc, x, y + f->font.underlinePos, (unsigned) width,
		    (unsigned) f->font.underlineHeight);
	}
	if (f->font.fa.overstrike) {
	    int y0 = y - f->font.fm.ascent / 3;

	    XFillRectangle(display, drawable, gc, x, y0, (unsigned) width,
		    (unsigned) f->font.underlineHeight);
	}
    }
}

void
Tk_DrawCharsInContext(Display *display, Drawable drawable, GC gc, Tk_Font tkfont,
	const char *string, Tcl_Size numBytes, Tcl_Size rangeStart, Tcl_Size rangeLength,
	int x, int y)
{
    (void) numBytes;
    Tk_DrawChars(display, drawable, gc, tkfont, string + rangeStart, rangeLength, x, y);
}

void
TkDrawAngledChars(Display *display, Drawable drawable, GC gc, Tk_Font tkfont,
	const char *source, Tcl_Size numBytes, double x, double y, double angle)
{
    if (angle != 0.0) {
	TkAStubCalled("TkDrawAngledChars (rotated text drawn unrotated)");
    }
    Tk_DrawChars(display, drawable, gc, tkfont, source, numBytes, (int) (x + 0.5), (int) (y + 0.5));
}
