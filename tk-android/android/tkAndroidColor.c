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

/* The names of Tk's cursors: X11's cursor font (77) and Tk's own extras. There is no pointer on a
 * touch screen, so no cursor is ever shown, but a script's cursor spec is still validated exactly as
 * upstream does (unix/tkUnixCursor.c), so a typo is an error here as on every other platform. */
static const char *const cursorNames[] = {
    "X_cursor",
    "arrow",
    "based_arrow_down",
    "based_arrow_up",
    "boat",
    "bogosity",
    "bottom_left_corner",
    "bottom_right_corner",
    "bottom_side",
    "bottom_tee",
    "box_spiral",
    "center_ptr",
    "circle",
    "clock",
    "coffee_mug",
    "cross",
    "cross_reverse",
    "crosshair",
    "diamond_cross",
    "dot",
    "dotbox",
    "double_arrow",
    "draft_large",
    "draft_small",
    "draped_box",
    "exchange",
    "fleur",
    "gobbler",
    "gumby",
    "hand1",
    "hand2",
    "heart",
    "icon",
    "iron_cross",
    "left_ptr",
    "left_side",
    "left_tee",
    "leftbutton",
    "ll_angle",
    "lr_angle",
    "man",
    "middlebutton",
    "mouse",
    "pencil",
    "pirate",
    "plus",
    "question_arrow",
    "right_ptr",
    "right_side",
    "right_tee",
    "rightbutton",
    "rtl_logo",
    "sailboat",
    "sb_down_arrow",
    "sb_h_double_arrow",
    "sb_left_arrow",
    "sb_right_arrow",
    "sb_up_arrow",
    "sb_v_double_arrow",
    "shuttle",
    "sizing",
    "spider",
    "spraycan",
    "star",
    "target",
    "tcross",
    "top_left_arrow",
    "top_left_corner",
    "top_right_corner",
    "top_side",
    "top_tee",
    "trek",
    "ul_angle",
    "umbrella",
    "ur_angle",
    "watch",
    "xterm",
    "none",
    "myarrow",
    NULL
};

TkCursor *
TkGetCursorByName(Tcl_Interp *interp, Tk_Window tkwin, const char *string)
{
    Tcl_Size argc;
    const char **argv = NULL;
    TkCursor *result = NULL;
    XColor color;
    int known = 0, i;

    if (Tcl_SplitList(interp, string, &argc, &argv) != TCL_OK) {
	return NULL;
    }
    if (argc == 0) {
	goto bad;
    }
    if (argv[0][0] == '@') {
	/* a cursor from bitmap files: {src fg} or {src mask fg bg}; files are not read here */
	if (Tcl_IsSafe(interp)) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "cannot get cursor from a file in a safe interpreter", TCL_AUTO_LENGTH));
	    Tcl_SetErrorCode(interp, "TK", "SAFE", "CURSOR_FILE", (char *) NULL);
	    goto done;
	}
	if (argc != 2 && argc != 4) {
	    goto bad;
	}
    } else {
	if (argc > 3) {
	    goto bad;
	}
	for (i = 0; cursorNames[i] != NULL; i++) {
	    if (strcmp(cursorNames[i], argv[0]) == 0) {
		known = 1;
		break;
	    }
	}
	if (!known) {
	    goto bad;
	}
    }
    for (i = (argv[0][0] == '@' ? (int) argc - 2 : 1); i < (int) argc; i++) {
	if (argv[0][0] == '@' && i < (int) argc - 2) {
	    continue;
	}
	if (TkParseColor(Tk_Display(tkwin), Tk_Colormap(tkwin), argv[i], &color) == 0) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("invalid color name \"%s\"", argv[i]));
	    Tcl_SetErrorCode(interp, "TK", "CURSOR", "COLOR", (char *) NULL);
	    goto done;
	}
    }
    result = NewCursor();
    goto done;

  bad:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad cursor spec \"%s\"", string));
    Tcl_SetErrorCode(interp, "TK", "VALUE", "CURSOR", (char *) NULL);
  done:
    if (argv != NULL) {
	ckfree(argv);
    }
    return result;
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

/* Tk gave up an owner of CLIPBOARD (selection clear -selection CLIPBOARD): empty the system one too,
 * or `clipboard get` would still find the old text there. */
int
XSetSelectionOwner(Display *d, Atom a, Window w, Time t)
{
    (void) d; (void) t;
    if (w == None && tkA.dispPtr != NULL && a == tkA.dispPtr->clipboardAtom) {
	TkAClipboardDisowned();
    }
    return 1;
}
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
