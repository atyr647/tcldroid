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
#include "tkAndroid.h"
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
unsigned long TkANextSerial(void);
void TkAFocusLost(TkWindow *winPtr);
TkWindow *TkAActiveTop(void);
void TkAHandleBack(void);
void TkAWmScreenChanged(void);

/* Declared in tkIntPlatDecls.h only for the Windows/macOS sections. */
void TkCreateXEventSource(void);
unsigned long TkpGetMS(void);

/* ---- misc ---------------------------------------------------------------------- */

void TkAStubCalled(const char *name);
void TkALog(const char *fmt, ...);
int TkAInitCommands(Tcl_Interp *interp);

#endif /* TKANDROIDPRIV_H */
