/*
 * tkAndroidWindow.c --
 *
 *	The in-process "X server": window tree, geometry, stacking, mapping,
 *	pixmaps and visibility/clipping. There is no X connection; windows are
 *	TkADrawable records and all of them paint into the root's single pixel
 *	buffer. Where Tk relies on X semantics, those are reproduced here:
 *	a window's background is painted before it is exposed, and exposing a
 *	window exposes what it uncovers in its parent.
 */

#include "tkAndroidPriv.h"

/* From tkAndroidDraw.c */
void TkAFillClipped(const TkARegion *clip, TkARect r, uint32_t pixel);
void TkACopyRoot(const TkARegion *clip, TkARect dst, int srcX, int srcY);

/*
 * ---------------------------------------------------------------------------
 * Tree helpers
 * ---------------------------------------------------------------------------
 */

void
TkAWindowOrigin(TkADrawable *d, int *xp, int *yp)
{
    int x = 0, y = 0;

    for (; d != NULL && d->isWindow; d = d->parent) {
	x += d->x;
	y += d->y;
    }
    *xp = x;
    *yp = y;
}

int
TkAIsViewable(TkADrawable *d)
{
    for (; d != NULL; d = d->parent) {
	if (d != tkA.root && !d->mapped) {
	    return 0;
	}
    }
    return 1;
}

static void
Unlink(TkADrawable *d)
{
    TkADrawable *p = d->parent;

    if (p == NULL) {
	return;
    }
    if (d->prev) d->prev->next = d->next; else p->first = d->next;
    if (d->next) d->next->prev = d->prev; else p->last = d->prev;
    d->prev = d->next = NULL;
}

static void
LinkTop(TkADrawable *d, TkADrawable *p)
{
    d->parent = p;
    d->prev = p->last;
    d->next = NULL;
    if (p->last) p->last->next = d; else p->first = d;
    p->last = d;
}

/* Insert d directly above (above!=0) or below `sib` (a sibling). */
static void
LinkRelative(TkADrawable *d, TkADrawable *sib, int above)
{
    TkADrawable *p = sib->parent;

    d->parent = p;
    if (above) {
	d->prev = sib;
	d->next = sib->next;
	if (sib->next) sib->next->prev = d; else p->last = d;
	sib->next = d;
    } else {
	d->next = sib;
	d->prev = sib->prev;
	if (sib->prev) sib->prev->next = d; else p->first = d;
	sib->prev = d;
    }
}

static TkARect
RectOf(TkADrawable *d)
{
    TkARect r;
    int x, y;

    TkAWindowOrigin(d, &x, &y);
    r.x1 = x;
    r.y1 = y;
    r.x2 = x + d->w;
    r.y2 = y + d->h;
    return r;
}

/*
 * The visible part of a window, in root coordinates: its rectangle, clipped by
 * every ancestor, minus mapped children, minus higher-stacked siblings of the
 * window and of each ancestor (which may overlap it).
 */
void
TkAComputeClip(TkADrawable *d, TkARegion *clip)
{
    TkADrawable *a, *s, *c;

    TkARegionClear(clip);
    if (d == NULL || !TkAIsViewable(d)) {
	return;
    }
    TkARegionAddRect(clip, RectOf(d));
    for (a = d; a != NULL && a->parent != NULL; a = a->parent) {
	TkARegionIntersectRect(clip, RectOf(a->parent));
	for (s = a->next; s != NULL; s = s->next) {
	    if (s->mapped) {
		TkARegionSubtractRect(clip, RectOf(s));
	    }
	}
    }
    for (c = d->first; c != NULL; c = c->next) {
	if (c->mapped) {
	    TkARegionSubtractRect(clip, RectOf(c));
	}
    }
}

TkADrawable *
TkAWindowAt(int x, int y)
{
    TkADrawable *d = tkA.root, *c;
    int again = 1;

    while (again) {
	again = 0;
	for (c = d->last; c != NULL; c = c->prev) {
	    TkARect r = RectOf(c);

	    if (c->mapped && x >= r.x1 && x < r.x2 && y >= r.y1 && y < r.y2) {
		d = c;
		again = 1;
		break;
	    }
	}
    }
    return d;
}

/* Paint the window background over r (window coordinates) and mark it dirty. */
static void
PaintBackground(TkADrawable *d, TkARect r)
{
    TkARegion *clip;
    int ox, oy;
    TkARect rr;

    if (!d->bgSet || !TkAIsViewable(d)) {
	return;
    }
    clip = TkARegionNew();
    TkAComputeClip(d, clip);
    TkAWindowOrigin(d, &ox, &oy);
    rr.x1 = r.x1 + ox;
    rr.y1 = r.y1 + oy;
    rr.x2 = r.x2 + ox;
    rr.y2 = r.y2 + oy;
    TkAFillClipped(clip, rr, (uint32_t) (d->bg & 0xFFFFFF) | TKA_OPAQUE);
    TkARegionFree(clip);
}

/* X semantics: background painted, then an Expose event for the area. */
static void
ExposeWindow(TkADrawable *d, TkARect r)
{
    TkARect full = {0, 0, d->w, d->h};

    if (r.x1 < 0) r.x1 = 0;
    if (r.y1 < 0) r.y1 = 0;
    if (r.x2 > full.x2) r.x2 = full.x2;
    if (r.y2 > full.y2) r.y2 = full.y2;
    if (r.x1 >= r.x2 || r.y1 >= r.y2 || !TkAIsViewable(d)) {
	return;
    }
    PaintBackground(d, r);
    if (d->winPtr != NULL) {
	TkAGenerateExpose(d, r);
    }
}

static void
ExposeTree(TkADrawable *d)
{
    TkADrawable *c;
    TkARect full;

    full.x1 = full.y1 = 0;
    full.x2 = d->w;
    full.y2 = d->h;
    ExposeWindow(d, full);
    for (c = d->first; c != NULL; c = c->next) {
	if (c->mapped) {
	    ExposeTree(c);
	}
    }
}

/* Expose `area` (root coordinates) in every viewable window whose clip meets it. */
static void
ExposeRootArea(TkADrawable *d, TkARect area)
{
    TkADrawable *c;
    int ox, oy;
    TkARect r;

    if (d != tkA.root && !d->mapped) {
	return;
    }
    TkAWindowOrigin(d, &ox, &oy);
    r.x1 = area.x1 - ox;
    r.y1 = area.y1 - oy;
    r.x2 = area.x2 - ox;
    r.y2 = area.y2 - oy;
    if (d != tkA.root) {
	ExposeWindow(d, r);
    }
    for (c = d->first; c != NULL; c = c->next) {
	ExposeRootArea(c, area);
    }
}

/*
 * ---------------------------------------------------------------------------
 * Creation and destruction
 * ---------------------------------------------------------------------------
 */

Window
Tk_MakeWindow(Tk_Window tkwin, Window parent)
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkADrawable *d = (TkADrawable *) calloc(1, sizeof(TkADrawable));
    TkADrawable *p = parent != None ? TKA_DRAWABLE(parent) : tkA.root;

    d->isWindow = 1;
    d->depth = winPtr->depth ? winPtr->depth : 24;
    d->x = winPtr->changes.x;
    d->y = winPtr->changes.y;
    d->w = winPtr->changes.width > 0 ? winPtr->changes.width : 1;
    d->h = winPtr->changes.height > 0 ? winPtr->changes.height : 1;
    d->winPtr = winPtr;
    if (winPtr->dirtyAtts & CWBackPixel) {
	d->bg = winPtr->atts.background_pixel;
	d->bgSet = 1;
    }
    if (winPtr->dirtyAtts & CWEventMask) {
	d->eventMask = winPtr->atts.event_mask;
    }
    LinkTop(d, p);
    return TKA_ID(d);
}

void
TkpMakeTransparentWindowExist(Tk_Window tkwin, Window parent)
{
    TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->window = Tk_MakeWindow(tkwin, parent);
}

static void
Destroy(TkADrawable *d)
{
    while (d->first != NULL) {
	Destroy(d->first);
    }
    Unlink(d);
    free(d->pix);
    free(d);
}

int
XDestroyWindow(Display *display, Window w)
{
    TkADrawable *d = TKA_DRAWABLE(w), *p;
    TkARect area;

    (void) display;
    if (d == NULL || d == tkA.root) {
	return 0;
    }
    p = d->parent;
    area = RectOf(d);
    Destroy(d);
    if (p != NULL) {
	ExposeRootArea(p, area);
    }
    return 1;
}

Pixmap
Tk_GetPixmap(Display *display, Drawable drawable, int width, int height, int depth)
{
    TkADrawable *d = (TkADrawable *) calloc(1, sizeof(TkADrawable));

    (void) display;
    (void) drawable;
    d->depth = depth;
    d->w = width > 0 ? width : 1;
    d->h = height > 0 ? height : 1;
    d->pix = (uint32_t *) calloc((size_t) d->w * (size_t) d->h, sizeof(uint32_t));
    return (Pixmap) TKA_ID(d);
}

void
Tk_FreePixmap(Display *display, Pixmap pixmap)
{
    TkADrawable *d = TKA_DRAWABLE(pixmap);

    (void) display;
    if (d != NULL) {
	free(d->pix);
	free(d);
    }
}

/*
 * ---------------------------------------------------------------------------
 * Mapping
 * ---------------------------------------------------------------------------
 */

int
XMapWindow(Display *display, Window w)
{
    TkADrawable *d = TKA_DRAWABLE(w);

    (void) display;
    if (d != NULL && !d->mapped) {
	d->mapped = 1;
	ExposeTree(d);
    }
    return 1;
}

int
XUnmapWindow(Display *display, Window w)
{
    TkADrawable *d = TKA_DRAWABLE(w);

    (void) display;
    if (d != NULL && d->mapped) {
	TkARect area = RectOf(d);

	d->mapped = 0;
	if (d->parent != NULL) {
	    ExposeRootArea(d->parent, area);
	}
    }
    return 1;
}

Status
XIconifyWindow(Display *display, Window w, int screen)
{
    (void) screen;
    return XUnmapWindow(display, w);
}

Status
XWithdrawWindow(Display *display, Window w, int screen)
{
    (void) screen;
    return XUnmapWindow(display, w);
}

/*
 * ---------------------------------------------------------------------------
 * Geometry and stacking
 * ---------------------------------------------------------------------------
 */

static void
Restack(TkADrawable *d, TkADrawable *sibling, int mode)
{
    TkADrawable *p = d->parent;

    if (p == NULL) {
	return;
    }
    if (sibling != NULL && sibling->parent != p) {
	sibling = NULL;
    }
    Unlink(d);
    switch (mode) {
    case Above:
	if (sibling != NULL) LinkRelative(d, sibling, 1); else LinkTop(d, p);
	break;
    case Below:
	if (sibling != NULL) LinkRelative(d, sibling, 0);
	else {
	    /* lowest */
	    d->parent = p;
	    d->next = p->first;
	    d->prev = NULL;
	    if (p->first) p->first->prev = d; else p->last = d;
	    p->first = d;
	}
	break;
    default:
	LinkTop(d, p);
    }
    if (TkAIsViewable(d)) {
	/* Restacking changes what is visible: repaint this window and the rest of the level. */
	ExposeRootArea(p, RectOf(d));
    }
}

int
XConfigureWindow(Display *display, Window w, unsigned int mask, XWindowChanges *c)
{
    TkADrawable *d = TKA_DRAWABLE(w);
    TkARect before, after;
    int resized = 0, moved = 0;

    (void) display;
    if (d == NULL || d == tkA.root) {
	return 0;
    }
    before = RectOf(d);
    if ((mask & CWX) && c->x != d->x) { d->x = c->x; moved = 1; }
    if ((mask & CWY) && c->y != d->y) { d->y = c->y; moved = 1; }
    if ((mask & CWWidth) && c->width > 0 && c->width != d->w) { d->w = c->width; resized = 1; }
    if ((mask & CWHeight) && c->height > 0 && c->height != d->h) { d->h = c->height; resized = 1; }
    if (mask & CWStackMode) {
	Restack(d, (mask & CWSibling) ? TKA_DRAWABLE(c->sibling) : NULL, c->stack_mode);
    }
    after = RectOf(d);
    if ((moved || resized) && TkAIsViewable(d)) {
	/* What it vacated belongs to the parent (and siblings); what it now covers is new. */
	if (d->parent != NULL) {
	    ExposeRootArea(d->parent, before);
	}
	ExposeRootArea(d, after);
	ExposeWindow(d, (TkARect) {0, 0, d->w, d->h});
    }
    return 1;
}

int
XMoveWindow(Display *display, Window w, int x, int y)
{
    XWindowChanges c;

    c.x = x;
    c.y = y;
    return XConfigureWindow(display, w, CWX | CWY, &c);
}

int
XResizeWindow(Display *display, Window w, unsigned int width, unsigned int height)
{
    XWindowChanges c;

    c.width = (int) width;
    c.height = (int) height;
    return XConfigureWindow(display, w, CWWidth | CWHeight, &c);
}

int
XMoveResizeWindow(Display *display, Window w, int x, int y, unsigned int width,
	unsigned int height)
{
    XWindowChanges c;

    c.x = x;
    c.y = y;
    c.width = (int) width;
    c.height = (int) height;
    return XConfigureWindow(display, w, CWX | CWY | CWWidth | CWHeight, &c);
}

int
XRaiseWindow(Display *display, Window w)
{
    XWindowChanges c;

    c.stack_mode = Above;
    return XConfigureWindow(display, w, CWStackMode, &c);
}

int
XLowerWindow(Display *display, Window w)
{
    XWindowChanges c;

    c.stack_mode = Below;
    return XConfigureWindow(display, w, CWStackMode, &c);
}

/*
 * ---------------------------------------------------------------------------
 * Attributes
 * ---------------------------------------------------------------------------
 */

int
XChangeWindowAttributes(Display *display, Window w, unsigned long mask,
	XSetWindowAttributes *a)
{
    TkADrawable *d = TKA_DRAWABLE(w);

    (void) display;
    if (d == NULL) {
	return 0;
    }
    if (mask & CWBackPixel) {
	d->bg = a->background_pixel;
	d->bgSet = 1;
    }
    if (mask & CWEventMask) {
	d->eventMask = a->event_mask;
    }
    return 1;
}

int
XSetWindowBackground(Display *display, Window w, unsigned long pixel)
{
    XSetWindowAttributes a;

    a.background_pixel = pixel;
    return XChangeWindowAttributes(display, w, CWBackPixel, &a);
}

/* Background/border pixmaps, border width and colormaps are not supported. */
int XSetWindowBackgroundPixmap(Display *d, Window w, Pixmap p) { (void) d; (void) w; (void) p; return 1; }
int XSetWindowBorder(Display *d, Window w, unsigned long p) { (void) d; (void) w; (void) p; return 1; }
int XSetWindowBorderPixmap(Display *d, Window w, Pixmap p) { (void) d; (void) w; (void) p; return 1; }
int XSetWindowBorderWidth(Display *d, Window w, unsigned int b) { (void) d; (void) w; (void) b; return 1; }
int XSetWindowColormap(Display *d, Window w, Colormap c) { (void) d; (void) w; (void) c; return 1; }

int
XSelectInput(Display *display, Window w, long mask)
{
    TkADrawable *d = TKA_DRAWABLE(w);

    (void) display;
    if (d != NULL) {
	d->eventMask = mask;
    }
    return 1;
}

int
XClearWindow(Display *display, Window w)
{
    TkADrawable *d = TKA_DRAWABLE(w);

    (void) display;
    if (d != NULL && d->isWindow) {
	PaintBackground(d, (TkARect) {0, 0, d->w, d->h});
    }
    return 1;
}

Status
XGetGeometry(Display *display, Drawable dr, Window *root, int *x, int *y,
	unsigned int *width, unsigned int *height, unsigned int *border,
	unsigned int *depth)
{
    TkADrawable *d = TKA_DRAWABLE(dr);

    (void) display;
    if (d == NULL) {
	return 0;
    }
    if (root) *root = TKA_ID(tkA.root);
    if (x) *x = d->isWindow ? d->x : 0;
    if (y) *y = d->isWindow ? d->y : 0;
    if (width) *width = (unsigned) d->w;
    if (height) *height = (unsigned) d->h;
    if (border) *border = 0;
    if (depth) *depth = (unsigned) d->depth;
    return 1;
}

Status
XGetWindowAttributes(Display *display, Window w, XWindowAttributes *a)
{
    TkADrawable *d = TKA_DRAWABLE(w);

    memset(a, 0, sizeof(*a));
    if (d == NULL) {
	return 0;
    }
    a->x = d->x;
    a->y = d->y;
    a->width = d->w;
    a->height = d->h;
    a->depth = d->depth;
    a->root = TKA_ID(tkA.root);
    a->screen = ScreenOfDisplay(display, 0);
    a->visual = DefaultVisual(display, 0);
    a->map_state = d->mapped ? IsViewable : IsUnmapped;
    a->your_event_mask = d->eventMask;
    a->override_redirect = 0;
    return 1;
}

Window
XRootWindow(Display *display, int screen)
{
    (void) display;
    (void) screen;
    return TKA_ID(tkA.root);
}

Status
XQueryTree(Display *display, Window w, Window *root, Window *parent,
	Window **children, unsigned int *n)
{
    TkADrawable *d = TKA_DRAWABLE(w), *c;
    unsigned int count = 0, i = 0;

    (void) display;
    if (d == NULL) {
	return 0;
    }
    for (c = d->first; c != NULL; c = c->next) count++;
    *root = TKA_ID(tkA.root);
    *parent = d->parent ? TKA_ID(d->parent) : None;
    *children = count ? (Window *) ckalloc(count * sizeof(Window)) : NULL;   /* freed by Tk with XFree */
    for (c = d->first; c != NULL; c = c->next) (*children)[i++] = TKA_ID(c);
    *n = count;
    return 1;
}

Bool
XTranslateCoordinates(Display *display, Window src, Window dst, int sx, int sy,
	int *dx, int *dy, Window *child)
{
    int ox = 0, oy = 0, px = 0, py = 0;

    (void) display;
    TkAWindowOrigin(TKA_DRAWABLE(src), &ox, &oy);
    TkAWindowOrigin(TKA_DRAWABLE(dst), &px, &py);
    *dx = sx + ox - px;
    *dy = sy + oy - py;
    if (child) *child = None;
    return 1;
}

/*
 * ---------------------------------------------------------------------------
 * Tk-level geometry queries
 * ---------------------------------------------------------------------------
 */

void
Tk_GetRootCoords(Tk_Window tkwin, int *xp, int *yp)
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    int x = 0, y = 0;

    if (winPtr->window != None) {
	TkAWindowOrigin(TKA_DRAWABLE(winPtr->window), &x, &y);
    } else {
	for (; winPtr != NULL; winPtr = winPtr->parentPtr) {
	    x += winPtr->changes.x;
	    y += winPtr->changes.y;
	    if (winPtr->flags & TK_TOP_HIERARCHY) {
		break;
	    }
	}
    }
    *xp = x;
    *yp = y;
}

Tk_Window
Tk_CoordsToWindow(int rootX, int rootY, Tk_Window tkwin)
{
    TkADrawable *d = TkAWindowAt(rootX, rootY);
    TkWindow *me = (TkWindow *) tkwin;

    while (d != NULL && (d->winPtr == NULL || d->winPtr->mainPtr != me->mainPtr)) {
	d = d->parent;
    }
    return d ? (Tk_Window) d->winPtr : NULL;
}

void
Tk_GetVRootGeometry(Tk_Window tkwin, int *xp, int *yp, int *wp, int *hp)
{
    (void) tkwin;
    *xp = 0;
    *yp = 0;
    *wp = tkA.width;
    *hp = tkA.height;
}

int
TkpScanWindowId(Tcl_Interp *interp, const char *string, Window *idPtr)
{
    unsigned long long v;
    char *end;

    v = strtoull(string, &end, 0);
    if (end == string || *end != '\0' || v == 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected integer but got \"%s\"", string));
	return TCL_ERROR;
    }
    *idPtr = (Window) v;
    return TCL_OK;
}

/*
 * Scrolling: copy the rectangle and report what must be redrawn (damage).
 */
int
TkScrollWindow(Tk_Window tkwin, GC gc, int x, int y, int width, int height,
	int dx, int dy, TkRegion damageRgn)
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkARegion *damage = (TkARegion *) damageRgn;
    TkARect rect = {x, y, x + width, y + height};
    TkARect moved = {x + dx, y + dy, x + width + dx, y + height + dy};

    XCopyArea(winPtr->display, winPtr->window, winPtr->window, gc, x, y,
	    (unsigned) width, (unsigned) height, x + dx, y + dy);
    TkARegionClear(damage);
    TkARegionAddRect(damage, rect);
    TkARegionSubtractRect(damage, moved);
    return damage->n > 0;
}
