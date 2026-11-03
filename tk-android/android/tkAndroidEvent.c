/*
 * tkAndroidEvent.c --
 *
 *	Input and events. Tk's Tcl event source feeds Tk from a thread-safe queue
 *	that the UI thread fills (touch, keys, back); touches become pointer
 *	events through Tk's in-process pointer machinery (generic/tkPointer.c,
 *	as on Windows and macOS); expose events are produced by the window server;
 *	focus changes are reflected back as FocusIn/FocusOut. Also the keysym
 *	tables, the present (screen flush) hook, and the `tkdroid::` test hooks.
 */

#include "tkAndroidPriv.h"
#include <pthread.h>
#include <stdio.h>

/* ---- the UI-thread -> Tcl-thread queue ------------------------------------------- */

enum { REC_TOUCH = 1, REC_KEY = 2, REC_BACK = 3, REC_RESIZE = 4 };

typedef struct Rec {
    int kind;
    int a, b, c, d;
} Rec;

#define QSIZE 512
static Rec queue[QSIZE];
static int qHead, qTail;
static pthread_mutex_t qLock = PTHREAD_MUTEX_INITIALIZER;
static int sourceCreated, presentScheduled;

static void
Push(Rec r)
{
    pthread_mutex_lock(&qLock);
    if ((qTail + 1) % QSIZE != qHead) {
	queue[qTail] = r;
	qTail = (qTail + 1) % QSIZE;
    }
    pthread_mutex_unlock(&qLock);
    if (tkA.tclThread != NULL) {
	Tcl_ThreadAlert(tkA.tclThread);
    }
}

static int
Pop(Rec *r)
{
    int ok = 0;

    pthread_mutex_lock(&qLock);
    if (qHead != qTail) {
	*r = queue[qHead];
	qHead = (qHead + 1) % QSIZE;
	ok = 1;
    }
    pthread_mutex_unlock(&qLock);
    return ok;
}

static int
QueueEmpty(void)
{
    int e;

    pthread_mutex_lock(&qLock);
    e = qHead == qTail;
    pthread_mutex_unlock(&qLock);
    return e;
}

/* action: 0 down, 1 up, 2 move, 3 cancel (android.view.MotionEvent) */
void
TkAQueueTouch(int action, int x, int y)
{
    Rec r = {REC_TOUCH, action, x, y, 0};

    Push(r);
}

static KeySym
KeysymForChar(int cp)
{
    return cp < 0x100 ? (KeySym) cp : (KeySym) (0x01000000 | cp);
}

/* A character key (unicode) or, if unicode == 0, a special key by keysym. */
void
TkAQueueKey(int down, int keysym, int unicode, int state)
{
    Rec r = {REC_KEY, down, unicode ? (int) KeysymForChar(unicode) : keysym, state, 0};

    Push(r);
}

void
TkAQueueBack(void)
{
    Rec r = {REC_BACK, 0, 0, 0, 0};

    Push(r);
}

void
TkAQueueResize(int w, int h, int dpi)
{
    Rec r = {REC_RESIZE, w, h, dpi, 0};

    Push(r);
}

/* ---- X events ---------------------------------------------------------------------- */

static unsigned long serial = 2;

unsigned long
TkANextSerial(void)
{
    LastKnownRequestProcessed(tkA.display) = ++serial;
    return serial;
}

void
TkAGenerateExpose(TkADrawable *d, TkARect r)
{
    XEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = Expose;
    ev.xexpose.serial = TkANextSerial();
    ev.xexpose.send_event = False;
    ev.xexpose.display = tkA.display;
    ev.xexpose.window = TKA_ID(d);
    ev.xexpose.x = r.x1;
    ev.xexpose.y = r.y1;
    ev.xexpose.width = r.x2 - r.x1;
    ev.xexpose.height = r.y2 - r.y1;
    ev.xexpose.count = 0;
    Tk_QueueWindowEvent(&ev, TCL_QUEUE_TAIL);
}

static void
FocusEvent(TkWindow *top, int in)
{
    XEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = in ? FocusIn : FocusOut;
    ev.xfocus.serial = TkANextSerial();
    ev.xfocus.send_event = False;
    ev.xfocus.display = tkA.display;
    ev.xfocus.window = top->window;
    ev.xfocus.mode = NotifyNormal;
    ev.xfocus.detail = NotifyNonlinear;
    Tk_QueueWindowEvent(&ev, TCL_QUEUE_TAIL);
}

static TkWindow *activeTop;

static TkWindow *
TopOf(TkWindow *w)
{
    while (w != NULL && !(w->flags & TK_TOP_HIERARCHY)) {
	w = w->parentPtr;
    }
    return w;
}

int
TkpChangeFocus(TkWindow *winPtr, int force)
{
    TkWindow *top;

    (void) force;
    if (winPtr == NULL || (winPtr->flags & TK_ALREADY_DEAD) || !Tk_IsMapped(winPtr)) {
	return 0;
    }
    top = TopOf(winPtr);
    if (top != NULL && top != activeTop) {
	if (activeTop != NULL && !(activeTop->flags & TK_ALREADY_DEAD)) {
	    FocusEvent(activeTop, 0);
	}
	FocusEvent(top, 1);
	activeTop = top;
    }
    return (int) TkANextSerial();
}

/* Called by the window manager when a toplevel dies, so focus does not dangle. */
void
TkAFocusLost(TkWindow *winPtr)
{
    if (activeTop == winPtr) {
	activeTop = NULL;
    }
}

TkWindow *
TkAActiveTop(void)
{
    return activeTop;
}

void TkpClaimFocus(TkWindow *top, int force) { (void) top; (void) force; }

TkWindow *
TkWmFocusToplevel(TkWindow *winPtr)
{
    return TopOf(winPtr);
}

int
XSetInputFocus(Display *d, Window w, int revert, Time t)
{
    (void) d; (void) w; (void) revert; (void) t;
    return 1;
}

int
XGetInputFocus(Display *d, Window *w, int *revert)
{
    (void) d;
    *w = activeTop != NULL ? activeTop->window : None;
    *revert = RevertToParent;
    return 1;
}

/* ---- pointer ----------------------------------------------------------------------------- */

static TkWindow *captureWin;

void
TkpSetCapture(TkWindow *winPtr)
{
    captureWin = winPtr;
}

Tk_Window
TkpGetCapture(void)
{
    return (Tk_Window) captureWin;
}

void
TkGetPointerCoords(Tk_Window tkwin, int *xp, int *yp)
{
    (void) tkwin;
    *xp = tkA.ptrX;
    *yp = tkA.ptrY;
}

Bool
XQueryPointer(Display *d, Window w, Window *root, Window *child, int *rx, int *ry,
	int *wx, int *wy, unsigned int *mask)
{
    int ox = 0, oy = 0;

    (void) d;
    if (w != None && TKA_DRAWABLE(w) != NULL) {
	TkAWindowOrigin(TKA_DRAWABLE(w), &ox, &oy);
    }
    if (root) *root = TKA_ID(tkA.root);
    if (child) *child = None;
    if (rx) *rx = tkA.ptrX;
    if (ry) *ry = tkA.ptrY;
    if (wx) *wx = tkA.ptrX - ox;
    if (wy) *wy = tkA.ptrY - oy;
    if (mask) *mask = tkA.ptrState;
    return True;
}

int XWarpPointer(Display *d, Window s, Window dw, int sx, int sy, unsigned w, unsigned h, int dx, int dy)
{ (void) d; (void) s; (void) dw; (void) sx; (void) sy; (void) w; (void) h; (void) dx; (void) dy; return 1; }
void TkpWarpPointer(TkDisplay *dispPtr) { (void) dispPtr; }
void TkpSetCursor(TkpCursor cursor) { (void) cursor; }
void Tk_SetCaretPos(Tk_Window tkwin, int x, int y, int height) { (void) tkwin; (void) x; (void) y; (void) height; }

static void
ProcessTouch(const Rec *r)
{
    TkADrawable *d = TkAWindowAt(r->b, r->c);
    TkWindow *w = captureWin;
    int state;

    while (w == NULL && d != NULL) {
	w = d->winPtr;
	d = d->parent;
    }
    if (w == NULL) {
	return;
    }
    tkA.ptrX = r->b;
    tkA.ptrY = r->c;
    state = (r->a == 0 || r->a == 2) ? Button1Mask : 0;
    tkA.ptrState = state;
    Tk_UpdatePointer((Tk_Window) w, r->b, r->c, state);
}

static void
ProcessKey(const Rec *r)
{
    XEvent ev;
    TkWindow *target = tkA.dispPtr != NULL ? tkA.dispPtr->focusPtr : NULL;

    if (target == NULL) {
	target = activeTop;
    }
    if (target == NULL || target->window == None) {
	return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.type = r->a ? KeyPress : KeyRelease;
    ev.xkey.serial = TkANextSerial();
    ev.xkey.display = tkA.display;
    ev.xkey.window = target->window;
    ev.xkey.root = TKA_ID(tkA.root);
    ev.xkey.subwindow = None;
    ev.xkey.time = TkpGetMS();
    ev.xkey.state = (unsigned) r->c;
    ev.xkey.keycode = (unsigned) r->b;	/* keycode == keysym on this platform */
    ev.xkey.same_screen = True;
    Tk_QueueWindowEvent(&ev, TCL_QUEUE_TAIL);
}

/* ---- the Tcl event source ------------------------------------------------------------ */

static void PresentIdle(void *cd);

static void
SetupProc(void *cd, int flags)
{
    (void) cd;
    if (!(flags & TCL_WINDOW_EVENTS)) {
	return;
    }
    if (!QueueEmpty() || (tkA.dirty && !presentScheduled && tkAHost.present != NULL)) {
	Tcl_Time t = {0, 0};

	Tcl_SetMaxBlockTime(&t);
    }
}

static void
CheckProc(void *cd, int flags)
{
    Rec r;

    (void) cd;
    if (!(flags & TCL_WINDOW_EVENTS)) {
	return;
    }
    while (Pop(&r)) {
	switch (r.kind) {
	case REC_TOUCH:
	    ProcessTouch(&r);
	    break;
	case REC_KEY:
	    ProcessKey(&r);
	    break;
	case REC_BACK:
	    TkAHandleBack();
	    break;
	case REC_RESIZE:
	    TkAndroidResize(r.a, r.b, (double) r.c);
	    break;
	}
    }
    if (tkA.dirty && !presentScheduled && tkAHost.present != NULL) {
	presentScheduled = 1;
	Tcl_DoWhenIdle(PresentIdle, NULL);
    }
}

/* Push whatever changed since the last frame to the host. */
static void
PresentIdle(void *cd)
{
    TkARect r = tkA.dirtyRect;

    (void) cd;
    presentScheduled = 0;
    if (!tkA.dirty) {
	return;
    }
    tkA.dirty = 0;
    if (r.x1 < 0) r.x1 = 0;
    if (r.y1 < 0) r.y1 = 0;
    if (r.x2 > tkA.width) r.x2 = tkA.width;
    if (r.y2 > tkA.height) r.y2 = tkA.height;
    if (tkAHost.present != NULL && r.x1 < r.x2 && r.y1 < r.y2) {
	tkAHost.present(tkA.root->pix, tkA.width, tkA.height, r.x1, r.y1, r.x2, r.y2);
    }
}

void
TkCreateXEventSource(void)
{
    if (!sourceCreated) {
	sourceCreated = 1;
	Tcl_CreateEventSource(SetupProc, CheckProc, NULL);
    }
}

int TkUnixDoOneXEvent(Tcl_Time *timePtr) { (void) timePtr; return 0; }

int
XNextEvent(Display *d, XEvent *ev)
{
    (void) d;
    memset(ev, 0, sizeof(*ev));
    return 0;
}

int
XPutBackEvent(Display *d, XEvent *ev)
{
    (void) d;
    Tk_QueueWindowEvent(ev, TCL_QUEUE_HEAD);
    return 1;
}

Status
XSendEvent(Display *d, Window w, Bool propagate, long mask, XEvent *ev)
{
    XEvent copy = *ev;

    (void) d; (void) propagate; (void) mask;
    if (copy.xany.window == None) {
	copy.xany.window = w;
    }
    Tk_QueueWindowEvent(&copy, TCL_QUEUE_TAIL);
    return 1;
}

int XWindowEvent(Display *d, Window w, long mask, XEvent *ev) { (void) d; (void) w; (void) mask; memset(ev, 0, sizeof(*ev)); return 0; }
Bool XFilterEvent(XEvent *ev, Window w) { (void) ev; (void) w; return False; }

/* ---- keyboard ------------------------------------------------------------------------------ */

typedef struct { const char *name; KeySym value; } KeyName;
static const KeyName keyNames[] = {
#include "ks_names.h"
    {NULL, 0}
};

KeySym
XStringToKeysym(const char *s)
{
    const KeyName *k;
    unsigned cp;

    for (k = keyNames; k->name != NULL; k++) {
	if (strcmp(k->name, s) == 0) {
	    return k->value;
	}
    }
    if ((s[0] == 'U' || s[0] == 'u') && sscanf(s + 1, "%x", &cp) == 1) {
	return KeysymForChar((int) cp);
    }
    return NoSymbol;
}

char *
XKeysymToString(KeySym ks)
{
    const KeyName *k;
    static char buf[16];

    for (k = keyNames; k->name != NULL; k++) {
	if (k->value == ks) {
	    return (char *) k->name;
	}
    }
    if ((ks & 0xFF000000) == 0x01000000) {
	snprintf(buf, sizeof(buf), "U%04lX", (unsigned long) (ks & 0xFFFFFF));
	return buf;
    }
    return NULL;
}

KeySym XKeycodeToKeysym(Display *d, unsigned int k, int i) { (void) d; return i == 0 ? (KeySym) k : NoSymbol; }
KeySym XkbKeycodeToKeysym(Display *d, unsigned int k, int g, int i) { (void) d; (void) g; return i == 0 ? (KeySym) k : NoSymbol; }
KeyCode XKeysymToKeycode(Display *d, KeySym k) { (void) d; return (KeyCode) k; }

XModifierKeymap *
XGetModifierMapping(Display *d)
{
    XModifierKeymap *m = (XModifierKeymap *) calloc(1, sizeof(XModifierKeymap));

    (void) d;
    m->max_keypermod = 0;
    m->modifiermap = NULL;
    return m;
}

int XFreeModifiermap(XModifierKeymap *m) { free(m); return 1; }
int XRefreshKeyboardMapping(XMappingEvent *e) { (void) e; return 1; }
int XGrabKeyboard(Display *d, Window w, Bool b, int p, int k, Time t) { (void) d; (void) w; (void) b; (void) p; (void) k; (void) t; return GrabSuccess; }
int XUngrabKeyboard(Display *d, Time t) { (void) d; (void) t; return 1; }
XIC XCreateIC(XIM xim, ...) { (void) xim; return NULL; }
int XmbLookupString(XIC ic, XKeyPressedEvent *e, char *buf, int n, KeySym *ks, Status *st)
{ (void) ic; (void) e; (void) buf; (void) n; (void) ks; (void) st; return 0; }

void
TkpInitKeymapInfo(TkDisplay *dispPtr)
{
    dispPtr->bindInfoStale = 0;
    dispPtr->metaModMask = Mod1Mask;
    dispPtr->altModMask = Mod1Mask;
    dispPtr->modeModMask = 0;
    dispPtr->lockUsage = LU_IGNORE;
}

KeySym
TkpGetKeySym(TkDisplay *dispPtr, XEvent *eventPtr)
{
    (void) dispPtr;
    return (KeySym) eventPtr->xkey.keycode;
}

void
TkpSetKeycodeAndState(Tk_Window tkwin, KeySym keySym, XEvent *eventPtr)
{
    (void) tkwin;
    eventPtr->xkey.keycode = (unsigned) keySym;
}

void TkpRedirectKeyEvent(TkWindow *winPtr, XEvent *eventPtr) { (void) winPtr; (void) eventPtr; }

const char *
TkpGetString(TkWindow *winPtr, XEvent *eventPtr, Tcl_DString *dsPtr)
{
    KeySym ks = (KeySym) eventPtr->xkey.keycode;
    int cp = -1;
    char buf[8];

    (void) winPtr;
    Tcl_DStringInit(dsPtr);
    switch (ks) {
    case XK_BackSpace: cp = 8; break;
    case XK_Tab: cp = 9; break;
    case XK_Return: case XK_KP_Enter: cp = 13; break;
    case XK_Escape: cp = 27; break;
    case XK_Delete: cp = 127; break;
    default:
	if (ks >= 0x20 && ks < 0x7F) cp = (int) ks;
	else if (ks >= 0xA0 && ks <= 0xFF) cp = (int) ks;
	else if ((ks & 0xFF000000) == 0x01000000) cp = (int) (ks & 0xFFFFFF);
    }
    if (cp >= 0 && eventPtr->type == KeyPress) {
	int n = Tcl_UniCharToUtf(cp, buf);

	Tcl_DStringAppend(dsPtr, buf, n);
    }
    return Tcl_DStringValue(dsPtr);
}

/* ---- screen changes ----------------------------------------------------------------------------- */

void
TkAndroidResize(int w, int h, double dpi)
{
    uint32_t *pix;

    if (w <= 0 || h <= 0 || tkA.root == NULL) {
	return;
    }
    if (dpi > 0) {
	tkA.density = dpi;
    }
    if (w != tkA.width || h != tkA.height) {
	pix = (uint32_t *) calloc((size_t) w * (size_t) h, sizeof(uint32_t));
	free(tkA.root->pix);
	tkA.root->pix = pix;
	tkA.width = tkA.root->w = w;
	tkA.height = tkA.root->h = h;
	if (tkA.display != NULL) {
	    Screen *s = ScreenOfDisplay(tkA.display, 0);

	    s->width = w;
	    s->height = h;
	    s->mwidth = (int) (w * 25.4 / tkA.density);
	    s->mheight = (int) (h * 25.4 / tkA.density);
	}
    }
    TkAWmScreenChanged();
    TkAMarkDirty((TkARect) {0, 0, w, h});
}

/* ---- test hooks (also handy on a device) -------------------------------------------------------- */

static int
SnapshotCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    FILE *f;
    int x, y;

    (void) cd;
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "path");
	return TCL_ERROR;
    }
    if (tkA.root == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("no display", TCL_AUTO_LENGTH));
	return TCL_ERROR;
    }
    f = fopen(Tcl_GetString(objv[1]), "wb");
    if (f == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("cannot open file", TCL_AUTO_LENGTH));
	return TCL_ERROR;
    }
    fprintf(f, "P6\n%d %d\n255\n", tkA.width, tkA.height);
    for (y = 0; y < tkA.height; y++) {
	for (x = 0; x < tkA.width; x++) {
	    uint32_t p = tkA.root->pix[(size_t) y * tkA.width + x];

	    fputc((p >> 16) & 0xFF, f);
	    fputc((p >> 8) & 0xFF, f);
	    fputc(p & 0xFF, f);
	}
    }
    fclose(f);
    return TCL_OK;
}

static int
TouchCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    static const char *const actions[] = {"down", "up", "move", "cancel", NULL};
    int act, x, y;

    (void) cd;
    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "down|up|move|cancel x y");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], actions, "action", 0, &act) != TCL_OK
	    || Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK
	    || Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK) {
	return TCL_ERROR;
    }
    TkAQueueTouch(act, x, y);
    return TCL_OK;
}

static int
KeyCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    int down, cp;
    const char *name;
    KeySym ks;

    (void) cd;
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "down|up keysymName-or-char");
	return TCL_ERROR;
    }
    down = strcmp(Tcl_GetString(objv[1]), "down") == 0;
    name = Tcl_GetString(objv[2]);
    ks = XStringToKeysym(name);
    if (ks != NoSymbol) {
	TkAQueueKey(down, (int) ks, 0, 0);
    } else if (Tcl_UtfToUniChar(name, &cp) > 0) {
	TkAQueueKey(down, 0, cp, 0);
    }
    return TCL_OK;
}

static int
ScreenCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    (void) cd; (void) objc; (void) objv;
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("%d %d %g", tkA.width, tkA.height, tkA.density));
    return TCL_OK;
}

static int
ResizeCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    int w, h;

    (void) cd;
    if (objc != 3 || Tcl_GetIntFromObj(interp, objv[1], &w) != TCL_OK
	    || Tcl_GetIntFromObj(interp, objv[2], &h) != TCL_OK) {
	Tcl_WrongNumArgs(interp, 1, objv, "width height");
	return TCL_ERROR;
    }
    TkAndroidResize(w, h, 0);
    return TCL_OK;
}

int
TkAInitCommands(Tcl_Interp *interp)
{
    Tcl_CreateObjCommand2(interp, "::tkdroid::snapshot", SnapshotCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::touch", TouchCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::key", KeyCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::screen", ScreenCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::resize", ResizeCmd, NULL, NULL);
    return TCL_OK;
}
