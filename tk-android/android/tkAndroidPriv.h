/*
 * tkAndroidPriv.h --
 *
 *	Private declarations shared by the files of the Tk Android platform
 *	layer. Tk's generic code never includes this.
 *
 *	Architecture (docs/adr/0002-tk-backend-architecture.md): there is no X
 *	server. Windows and pixmaps are in-process records (TkADrawable). All
 *	windows of the application share ONE screen pixel buffer owned by the
 *	root drawable; every X-shaped drawing call is rasterised into it in C.
 *	Text is the only thing delegated to the platform: a TkATextEngine measures
 *	and renders glyph runs (FreeType on the build machine for tests, Android's
 *	android.graphics.Paint through JNI on a device).
 */
#ifndef TKANDROIDPRIV_H
#define TKANDROIDPRIV_H

#include "tkInt.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Pixels are 0xAARRGGBB; Tk "pixel" values are 0x00RRGGBB. */
#define TKA_OPAQUE 0xFF000000u

typedef struct TkARect { int x1, y1, x2, y2; } TkARect;	/* half-open */

typedef struct TkARegion {
    int n, cap;
    TkARect *r;			/* disjoint rectangles */
} TkARegion;

typedef struct TkADrawable {
    int isWindow;		/* else pixmap */
    int depth;			/* 24, or 1 for bitmaps */
    struct TkADrawable *parent;
    struct TkADrawable *first, *last;	/* children, lowest first */
    struct TkADrawable *next, *prev;	/* siblings in stacking order */
    int x, y;			/* position within parent (windows) */
    int w, h;
    int mapped;
    unsigned long bg;		/* background pixel */
    int bgSet;
    TkWindow *winPtr;		/* owner (NULL for the root) */
    long eventMask;
    uint32_t *pix;		/* pixel storage (pixmaps, and the root) */
} TkADrawable;

#define TKA_DRAWABLE(id)	((TkADrawable *)(uintptr_t)(id))
#define TKA_ID(d)		((Window)(uintptr_t)(d))

typedef struct TkAState {
    Display *display;
    TkDisplay *dispPtr;
    TkADrawable *root;
    int width, height;		/* screen size in pixels */
    double density;		/* pixels per inch */
    int dirty;			/* screen buffer changed since last present */
    TkARect dirtyRect;
    Tcl_ThreadId tclThread;
    int ptrX, ptrY;		/* last pointer position, root coordinates */
    unsigned int ptrState;	/* button mask */
} TkAState;

extern TkAState tkA;

/* ---- host callbacks ---------------------------------------------------------------- */

/*
 * The host (the Android Activity, or a test driver) receives frames and
 * platform requests through these. All are called on the Tcl thread.
 */
typedef void (*TkAPresentProc)(const uint32_t *pix, int w, int h, int x1, int y1,
	int x2, int y2);

typedef struct TkAHost {
    TkAPresentProc present;		/* screen changed: pixels 0xAARRGGBB */
    void (*setTitle)(const char *utf8);
    void (*setFullscreen)(int on);
    void (*backUnhandled)(void);	/* Back pressed, no WM_DELETE_WINDOW handler */
    void (*moveToBack)(void);		/* `wm iconify .` */
    void (*caret)(int x, int y, int height);	/* text caret moved (soft keyboard) */
} TkAHost;

extern TkAHost tkAHost;
void TkAndroidSetHost(const TkAHost *host);
void TkAndroidSetScreen(int width, int height, double dpi);
void TkAndroidResize(int width, int height, double dpi);	/* Tcl thread */

/* ---- text engine ------------------------------------------------------- */

typedef struct TkAFontInfo {
    int ascent, descent, maxWidth, fixed;
} TkAFontInfo;

typedef struct TkATextEngine {
    /* Open a font; family may be NULL for the default. Returns NULL on failure. */
    void *(*open)(const char *family, int pixelSize, int bold, int italic,
	    TkAFontInfo *info);
    void (*close)(void *font);
    /* Advance width in pixels of a UTF-8 string. */
    int (*measure)(void *font, const char *utf8, int nbytes);
    /* Render a UTF-8 string. Returns a malloc'd alpha mask (*w x *h bytes) whose
     * baseline is *baseline rows from the top; NULL if nothing to draw. */
    uint8_t *(*render)(void *font, const char *utf8, int nbytes, int *w, int *h,
	    int *baseline);
    /* Report available family names. */
    void (*families)(void (*add)(void *ctx, const char *name), void *ctx);
} TkATextEngine;

extern const TkATextEngine *tkAEngine;
void TkAndroidSetTextEngine(const TkATextEngine *engine);
const TkATextEngine *TkAGetBuiltinEngine(void);	/* NULL if none compiled in */

/* ---- regions ---------------------------------------------------------------- */

TkARegion *TkARegionNew(void);
void TkARegionFree(TkARegion *r);
void TkARegionClear(TkARegion *r);
void TkARegionAddRect(TkARegion *r, TkARect rect);	/* union */
void TkARegionIntersectRect(TkARegion *r, TkARect rect);
void TkARegionSubtractRect(TkARegion *r, TkARect rect);
void TkARegionCopy(TkARegion *dst, const TkARegion *src);

/* ---- windows ---------------------------------------------------------------- */

void TkAWindowOrigin(TkADrawable *d, int *x, int *y);	/* in root coords */
TkADrawable *TkAWindowAt(int rootX, int rootY);	/* deepest mapped window */
void TkAComputeClip(TkADrawable *d, TkARegion *clip);	/* visible region, root coords */
int TkAIsViewable(TkADrawable *d);

/* ---- drawing ---------------------------------------------------------------- */

void TkAMarkDirty(TkARect r);
void TkADrawText(Display *display, Drawable drawable, GC gc, void *platformFont,
	const char *utf8, int nbytes, int x, int y);

/* ---- events ------------------------------------------------------------------ */

void TkAGenerateExpose(TkADrawable *d, TkARect r);	/* r in window coords */
void TkAQueueTouch(int action, int x, int y);		/* thread-safe */
void TkAQueueKey(int down, int keysym, int unicode, int state);
void TkAQueueBack(void);
void TkAQueueResize(int w, int h, int dpi);
unsigned long TkANextSerial(void);
void TkAFocusLost(TkWindow *winPtr);
TkWindow *TkAActiveTop(void);
void TkAHandleBack(void);
void TkAWmScreenChanged(void);

/* ---- misc ---------------------------------------------------------------------- */

void TkAStubCalled(const char *name);
void TkALog(const char *fmt, ...);
int TkAInitCommands(Tcl_Interp *interp);

#endif /* TKANDROIDPRIV_H */
