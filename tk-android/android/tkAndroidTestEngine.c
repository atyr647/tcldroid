/*
 * tkAndroidTestEngine.c --
 *
 *	FreeType-based text engine used ONLY for host-side tests (it is not
 *	compiled into Android builds, which use android.graphics.Paint). It makes
 *	test screenshots look like real Tk text.
 */

#include "tkAndroidPriv.h"

#ifdef TKA_TEST_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library lib;

typedef struct TFont { FT_Face face; int px; } TFont;

static const char *
PathFor(const char *family, int bold, int italic)
{
    int mono = family && (strstr(family, "mono") || strstr(family, "Courier") || strstr(family, "Mono"));
    int serif = family && (strstr(family, "serif") && !strstr(family, "sans")) ;
    static char path[256];
    const char *dir = "/usr/share/fonts/truetype/dejavu/";
    const char *base = mono ? "DejaVuSansMono" : serif ? "DejaVuSerif" : "DejaVuSans";

    snprintf(path, sizeof(path), "%s%s%s%s.ttf", dir, base, bold ? "-Bold" : "",
	    italic && !bold ? (mono ? "-Oblique" : "-Oblique") : "");
    if (italic && !bold) {
	/* DejaVuSans-Oblique.ttf exists; serif has no oblique */
	if (serif) snprintf(path, sizeof(path), "%sDejaVuSerif.ttf", dir);
    }
    return path;
}

static void *
Open(const char *family, int px, int bold, int italic, TkAFontInfo *info)
{
    TFont *f = (TFont *) calloc(1, sizeof(TFont));

    if (lib == NULL && FT_Init_FreeType(&lib)) {
	free(f);
	return NULL;
    }
    if (FT_New_Face(lib, PathFor(family, bold, italic), 0, &f->face)) {
	free(f);
	return NULL;
    }
    FT_Set_Pixel_Sizes(f->face, 0, (FT_UInt) px);
    f->px = px;
    info->ascent = (int) ((f->face->size->metrics.ascender + 63) >> 6);
    info->descent = (int) ((-f->face->size->metrics.descender + 63) >> 6);
    info->maxWidth = (int) ((f->face->size->metrics.max_advance + 63) >> 6);
    info->fixed = FT_IS_FIXED_WIDTH(f->face) ? 1 : 0;
    return f;
}

static void Close(void *font) { TFont *f = (TFont *) font; FT_Done_Face(f->face); free(f); }

static int
Measure(void *font, const char *s, int n)
{
    TFont *f = (TFont *) font;
    int w = 0, i = 0;

    while (i < n) {
	int cp, len = (int) Tcl_UtfToUniChar(s + i, &cp);

	if (len < 1) len = 1;
	if (FT_Load_Char(f->face, (FT_ULong) cp, FT_LOAD_DEFAULT) == 0) {
	    w += (int) ((f->face->glyph->advance.x + 32) >> 6);
	}
	i += len;
    }
    return w;
}

static uint8_t *
Render(void *font, const char *s, int n, int *ow, int *oh, int *base)
{
    TFont *f = (TFont *) font;
    int asc = (int) ((f->face->size->metrics.ascender + 63) >> 6);
    int desc = (int) ((-f->face->size->metrics.descender + 63) >> 6);
    int total = Measure(font, s, n) + 4, h = asc + desc, pen = 0, i = 0;
    uint8_t *m;

    if (total <= 4 || h <= 0) {
	return NULL;
    }
    m = (uint8_t *) calloc((size_t) total * (size_t) h, 1);
    while (i < n) {
	int cp, len = (int) Tcl_UtfToUniChar(s + i, &cp), r, c;
	FT_GlyphSlot g;

	if (len < 1) len = 1;
	if (FT_Load_Char(f->face, (FT_ULong) cp, FT_LOAD_RENDER) == 0) {
	    g = f->face->glyph;
	    for (r = 0; r < (int) g->bitmap.rows; r++) {
		int y = asc - g->bitmap_top + r;

		for (c = 0; c < (int) g->bitmap.width; c++) {
		    int x = pen + g->bitmap_left + c;

		    if (x >= 0 && x < total && y >= 0 && y < h) {
			int a = g->bitmap.buffer[r * g->bitmap.pitch + c] + m[y * total + x];

			m[y * total + x] = (uint8_t) (a > 255 ? 255 : a);
		    }
		}
	    }
	    pen += (int) ((g->advance.x + 32) >> 6);
	}
	i += len;
    }
    *ow = total;
    *oh = h;
    *base = asc;
    return m;
}

static void
Families(void (*add)(void *, const char *), void *ctx)
{
    add(ctx, "sans-serif");
    add(ctx, "serif");
    add(ctx, "monospace");
}

static const TkATextEngine engine = {Open, Close, Measure, Render, Families};
const TkATextEngine *TkAGetBuiltinEngine(void) { return &engine; }

#else
/*
 * Fallback for builds without FreeType (the Android libraries): glyphs are drawn
 * as hollow boxes. The Android app installs a real engine (android.graphics.Paint)
 * before Tk starts; this exists so Tk can run, and be tested, without one
 * (for example under Bionic in a chroot).
 */
typedef struct BoxFont { int px; } BoxFont;

static void *
BoxOpen(const char *family, int px, int bold, int italic, TkAFontInfo *info)
{
    BoxFont *f = (BoxFont *) calloc(1, sizeof(BoxFont));

    (void) family; (void) bold; (void) italic;
    f->px = px;
    info->ascent = px * 4 / 5;
    info->descent = px - info->ascent;
    info->maxWidth = px * 3 / 5;
    info->fixed = 1;
    return f;
}

static void BoxClose(void *font) { free(font); }

static int
BoxMeasure(void *font, const char *s, int n)
{
    int count = 0, i = 0, cp;

    while (i < n) {
	int len = (int) Tcl_UtfToUniChar(s + i, &cp);

	i += len < 1 ? 1 : len;
	count++;
    }
    return count * ((BoxFont *) font)->px * 3 / 5;
}

static uint8_t *
BoxRender(void *font, const char *s, int n, int *ow, int *oh, int *base)
{
    BoxFont *f = (BoxFont *) font;
    int adv = f->px * 3 / 5, asc = f->px * 4 / 5, w = BoxMeasure(font, s, n), h = f->px;
    uint8_t *m;
    int i = 0, k = 0, x, y, cp;

    if (w <= 0) {
	return NULL;
    }
    m = (uint8_t *) calloc((size_t) w * (size_t) h, 1);
    while (i < n) {
	int len = (int) Tcl_UtfToUniChar(s + i, &cp);

	i += len < 1 ? 1 : len;
	if (cp != ' ') {
	    for (y = asc / 3; y < asc; y++) {
		for (x = 1; x < adv - 1; x++) {
		    int edge = (y == asc / 3 || y == asc - 1 || x == 1 || x == adv - 2);

		    if (edge) m[y * w + k * adv + x] = 255;
		}
	    }
	}
	k++;
    }
    *ow = w;
    *oh = h;
    *base = asc;
    return m;
}

static void
BoxFamilies(void (*add)(void *, const char *), void *ctx)
{
    add(ctx, "sans-serif");
    add(ctx, "serif");
    add(ctx, "monospace");
}

static const TkATextEngine boxEngine = {BoxOpen, BoxClose, BoxMeasure, BoxRender, BoxFamilies};
const TkATextEngine *TkAGetBuiltinEngine(void) { return &boxEngine; }
#endif
