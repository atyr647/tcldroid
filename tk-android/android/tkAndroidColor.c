/*
 * tkAndroidColor.c --
 *
 *	Colours (a TrueColor visual: pixel = 0x00RRGGBB), cursors (accepted and
 *	ignored: touch devices have no pointer shape), and the selection and
 *	clipboard hooks.
 */

#include "tkAndroidPriv.h"
#include "tkColor.h"
#include "tkSelect.h"

static unsigned long
PixelOf(const XColor *c)
{
    return ((unsigned long) (c->red >> 8) << 16) | ((unsigned long) (c->green >> 8) << 8)
	    | (unsigned long) (c->blue >> 8);
}

TkColor *
TkpGetColor(Tk_Window tkwin, const char *name)
{
    XColor c;
    TkColor *t;

    memset(&c, 0, sizeof(c));
    if (!XParseColor(Tk_Display(tkwin), 1, name, &c)) {
	return NULL;
    }
    t = (TkColor *) ckalloc(sizeof(TkColor));
    memset(t, 0, sizeof(TkColor));
    t->color = c;
    t->color.pixel = PixelOf(&c);
    return t;
}

TkColor *
TkpGetColorByValue(Tk_Window tkwin, XColor *colorPtr)
{
    TkColor *t = (TkColor *) ckalloc(sizeof(TkColor));

    (void) tkwin;
    memset(t, 0, sizeof(TkColor));
    t->color.red = colorPtr->red;
    t->color.green = colorPtr->green;
    t->color.blue = colorPtr->blue;
    t->color.pixel = PixelOf(colorPtr);
    return t;
}

Status
XAllocColor(Display *d, Colormap c, XColor *xp)
{
    (void) d; (void) c;
    xp->pixel = PixelOf(xp);
    return 1;
}

int
XQueryColors(Display *d, Colormap c, XColor *colors, int n)
{
    int i;

    (void) d; (void) c;
    for (i = 0; i < n; i++) {
	unsigned long p = colors[i].pixel;

	colors[i].red = (unsigned short) (((p >> 16) & 0xFF) * 257);
	colors[i].green = (unsigned short) (((p >> 8) & 0xFF) * 257);
	colors[i].blue = (unsigned short) ((p & 0xFF) * 257);
    }
    return 1;
}

Status
XLookupColor(Display *d, Colormap c, const char *name, XColor *exact, XColor *screen)
{
    if (!XParseColor(d, c, name, exact)) {
	return 0;
    }
    exact->pixel = PixelOf(exact);
    *screen = *exact;
    return 1;
}

int
XFreeColors(Display *d, Colormap c, unsigned long *pixels, int n, unsigned long planes)
{
    (void) d; (void) c; (void) pixels; (void) n; (void) planes;
    return 1;
}

/* ---- cursors ------------------------------------------------------------------------- */

static TkCursor *
NewCursor(void)
{
    static unsigned long nextId = 1;	/* Tk keeps a table keyed by id: each must be unique */
    TkCursor *c = (TkCursor *) ckalloc(sizeof(TkCursor));

    memset(c, 0, sizeof(TkCursor));
    c->cursor = (Tk_Cursor) nextId++;
    return c;
}

TkCursor *
TkGetCursorByName(Tcl_Interp *interp, Tk_Window tkwin, const char *string)
{
    (void) interp; (void) tkwin; (void) string;
    return NewCursor();
}

TkCursor *
TkCreateCursorFromData(Tk_Window tkwin, const char *source, const char *mask,
	int width, int height, int xHot, int yHot, XColor fg, XColor bg)
{
    (void) tkwin; (void) source; (void) mask; (void) width; (void) height;
    (void) xHot; (void) yHot; (void) fg; (void) bg;
    return NewCursor();
}

/* Platform resources only: generic/tkCursor.c frees the TkCursor record itself. */
void TkpFreeCursor(TkCursor *cursorPtr) { (void) cursorPtr; }
int XFreeCursor(Display *d, Cursor c) { (void) d; (void) c; return 1; }

/* ---- selection owner and window properties (the clipboard itself is tkAndroidSelect.c) -------------------------- */

int XSetSelectionOwner(Display *d, Atom a, Window w, Time t) { (void) d; (void) a; (void) w; (void) t; return 1; }
int XChangeProperty(Display *d, Window w, Atom a1, Atom a2, int i1, int i2, const unsigned char *c, int i3)
{ (void) d; (void) w; (void) a1; (void) a2; (void) i1; (void) i2; (void) c; (void) i3; return 1; }
int XDeleteProperty(Display *d, Window w, Atom a) { (void) d; (void) w; (void) a; return 1; }
int XGetWindowProperty(Display *d, Window w, Atom a1, long l1, long l2, Bool b, Atom a2, Atom *ap,
	int *ip, unsigned long *ul1, unsigned long *ul2, unsigned char **cpp)
{
    (void) d; (void) w; (void) a1; (void) l1; (void) l2; (void) b; (void) a2;
    *ap = None; *ip = 0; *ul1 = 0; *ul2 = 0; *cpp = NULL;
    return 1;
}
char *XGetAtomName(Display *d, Atom a) { (void) d; (void) a; return NULL; }
