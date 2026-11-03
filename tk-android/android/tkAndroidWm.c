/*
 * tkAndroidWm.c --
 *
 *	The window manager. Policy (docs/compatibility-policy.md):
 *
 *	  - "." (the application's main window) fills the screen;
 *	  - every other toplevel is a *layer* inside the same surface, sized from
 *	    its requested geometry, clamped to the screen, centred unless
 *	    `wm geometry` positions it;
 *	  - the Back button runs WM_DELETE_WINDOW of the topmost toplevel that has
 *	    a handler, else asks the host to finish the Activity;
 *	  - subcommands without an Android meaning are accepted: the value is
 *	    stored and returned by queries, with no visible effect.
 */

#include "tkAndroidPriv.h"
#include <stdio.h>

typedef struct WmInfo {
    TkWindow *winPtr;
    struct WmInfo *nextPtr;
    int withdrawn, neverMapped, iconic;
    int hasPos, x, y;
    int width, height;		/* from `wm geometry`; -1 = not set */
    Tcl_Obj *title;
    Tcl_HashTable protocols;	/* name -> Tcl_Obj command */
    Tcl_HashTable misc;		/* accepted-only options: name -> Tcl_Obj */
    TkWindow *transientFor;
    int overrideRedirect;
} WmInfo;

static WmInfo *wmList;

TkAHost tkAHost;

void
TkAndroidSetHost(const TkAHost *host)
{
    tkAHost = *host;
}

static int
IsMain(WmInfo *w)
{
    return w->winPtr->mainPtr != NULL && w->winPtr == w->winPtr->mainPtr->winPtr;
}

/* ---- sizing --------------------------------------------------------------------------- */

static void
Apply(WmInfo *w)
{
    TkWindow *wp = w->winPtr;
    int x, y, wd, ht;

    if (IsMain(w)) {
	x = y = 0;
	wd = tkA.width;
	ht = tkA.height;
    } else {
	wd = w->width > 0 ? w->width : Tk_ReqWidth(wp);
	ht = w->height > 0 ? w->height : Tk_ReqHeight(wp);
	if (wd < 1) wd = 1;
	if (ht < 1) ht = 1;
	if (wd > tkA.width) wd = tkA.width;
	if (ht > tkA.height) ht = tkA.height;
	x = w->hasPos ? w->x : (tkA.width - wd) / 2;
	y = w->hasPos ? w->y : (tkA.height - ht) / 2;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x + wd > tkA.width) x = tkA.width - wd;
	if (y + ht > tkA.height) y = tkA.height - ht;
    }
    if (wp->changes.x != x || wp->changes.y != y || wp->changes.width != wd
	    || wp->changes.height != ht) {
	Tk_MoveResizeWindow((Tk_Window) wp, x, y, wd, ht);
    }
}

static void
RequestProc(void *cd, Tk_Window tkwin)
{
    WmInfo *w = ((TkWindow *) tkwin)->wmInfoPtr;

    (void) cd;
    if (w != NULL && (w->winPtr->flags & TK_MAPPED)) {
	Apply(w);
    }
}

static void
LostSlaveProc(void *cd, Tk_Window tkwin)
{
    (void) cd;
    (void) tkwin;
}

static const Tk_GeomMgr wmMgrType = {"wm", RequestProc, LostSlaveProc};

void
TkAWmScreenChanged(void)
{
    WmInfo *w;

    for (w = wmList; w != NULL; w = w->nextPtr) {
	if (w->winPtr->window != None) {
	    Apply(w);
	}
    }
}

/* ---- lifecycle -------------------------------------------------------------------------- */

void
TkWmNewWindow(TkWindow *winPtr)
{
    WmInfo *w = (WmInfo *) calloc(1, sizeof(WmInfo));

    w->winPtr = winPtr;
    w->neverMapped = 1;
    w->width = w->height = -1;
    Tcl_InitHashTable(&w->protocols, TCL_STRING_KEYS);
    Tcl_InitHashTable(&w->misc, TCL_STRING_KEYS);
    w->nextPtr = wmList;
    wmList = w;
    winPtr->wmInfoPtr = (struct TkWmInfo *) w;
    Tk_ManageGeometry((Tk_Window) winPtr, &wmMgrType, NULL);
}

void
TkWmDeadWindow(TkWindow *winPtr)
{
    WmInfo *w = (WmInfo *) winPtr->wmInfoPtr, **pp;
    Tcl_HashSearch s;
    Tcl_HashEntry *e;

    if (w == NULL) {
	return;
    }
    for (pp = &wmList; *pp != NULL; pp = &(*pp)->nextPtr) {
	if (*pp == w) {
	    *pp = w->nextPtr;
	    break;
	}
    }
    for (e = Tcl_FirstHashEntry(&w->protocols, &s); e != NULL; e = Tcl_NextHashEntry(&s)) {
	Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(e));
    }
    for (e = Tcl_FirstHashEntry(&w->misc, &s); e != NULL; e = Tcl_NextHashEntry(&s)) {
	Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(e));
    }
    Tcl_DeleteHashTable(&w->protocols);
    Tcl_DeleteHashTable(&w->misc);
    if (w->title) Tcl_DecrRefCount(w->title);
    TkAFocusLost(winPtr);
    free(w);
    winPtr->wmInfoPtr = NULL;
}

static void
SendEvent(TkWindow *winPtr, int type)
{
    XEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.xmap.serial = TkANextSerial();
    ev.xmap.display = winPtr->display;
    ev.xmap.event = winPtr->window;
    ev.xmap.window = winPtr->window;
    Tk_HandleEvent(&ev);
}

void
TkWmMapWindow(TkWindow *winPtr)
{
    WmInfo *w = (WmInfo *) winPtr->wmInfoPtr;

    if (w->withdrawn) {
	return;
    }
    w->neverMapped = 0;
    w->iconic = 0;
    Apply(w);
    winPtr->flags |= TK_MAPPED;
    XMapWindow(winPtr->display, winPtr->window);
    SendEvent(winPtr, MapNotify);
    if (TkAActiveTop() == NULL || !IsMain(w)) {
	TkpChangeFocus(winPtr, 0);
    }
}

void
TkWmUnmapWindow(TkWindow *winPtr)
{
    if (!(winPtr->flags & TK_MAPPED)) {
	return;
    }
    winPtr->flags &= ~TK_MAPPED;
    XUnmapWindow(winPtr->display, winPtr->window);
    SendEvent(winPtr, UnmapNotify);
}

int
TkpWmSetState(TkWindow *winPtr, int state)
{
    WmInfo *w = (WmInfo *) winPtr->wmInfoPtr;

    if (state == WithdrawnState) {
	w->withdrawn = 1;
	TkWmUnmapWindow(winPtr);
    } else if (state == IconicState) {
	w->iconic = 1;
	TkWmUnmapWindow(winPtr);
	if (IsMain(w) && tkAHost.moveToBack) {
	    tkAHost.moveToBack();
	}
    } else {
	w->withdrawn = 0;
	w->iconic = 0;
	Tk_MapWindow((Tk_Window) winPtr);
    }
    return 1;
}

void TkWmSetClass(TkWindow *winPtr) { (void) winPtr; }
int XSetIconName(Display *d, Window w, const char *name) { (void) d; (void) w; (void) name; return 1; }
void TkWmProtocolEventProc(TkWindow *winPtr, XEvent *ev) { (void) winPtr; (void) ev; }
void TkWmCleanup(TkDisplay *dispPtr) { (void) dispPtr; }
void Tk_SetGrid(Tk_Window tkwin, int rw, int rh, int gw, int gh) { (void) tkwin; (void) rw; (void) rh; (void) gw; (void) gh; }
void Tk_UnsetGrid(Tk_Window tkwin) { (void) tkwin; }
void TkUnixSetMenubar(Tk_Window tkwin, Tk_Window menubar) { (void) tkwin; (void) menubar; }
void TkpMakeMenuWindow(Tk_Window tkwin, int transient) { (void) tkwin; (void) transient; }

void
Tk_MoveToplevelWindow(Tk_Window tkwin, int x, int y)
{
    WmInfo *w = ((TkWindow *) tkwin)->wmInfoPtr ? (WmInfo *) ((TkWindow *) tkwin)->wmInfoPtr : NULL;

    if (w != NULL) {
	w->hasPos = 1;
	w->x = x;
	w->y = y;
	if (w->winPtr->flags & TK_MAPPED) {
	    Apply(w);
	}
    }
}

void
TkWmRestackToplevel(TkWindow *winPtr, int aboveBelow, TkWindow *otherPtr)
{
    XWindowChanges c;
    unsigned mask = CWStackMode;

    c.stack_mode = aboveBelow;
    if (otherPtr != NULL && otherPtr->window != None) {
	c.sibling = otherPtr->window;
	mask |= CWSibling;
    }
    XConfigureWindow(winPtr->display, winPtr->window, mask, &c);
}

TkWindow **
TkWmStackorderToplevel(TkWindow *parentPtr)
{
    TkADrawable *c;
    int n = 0;
    TkWindow **list;

    for (c = tkA.root->first; c != NULL; c = c->next) {
	if (c->winPtr != NULL && c->winPtr->mainPtr == parentPtr->mainPtr) n++;
    }
    list = (TkWindow **) ckalloc((size_t) (n + 1) * sizeof(TkWindow *));
    n = 0;
    for (c = tkA.root->first; c != NULL; c = c->next) {
	if (c->winPtr != NULL && c->winPtr->mainPtr == parentPtr->mainPtr) list[n++] = c->winPtr;
    }
    list[n] = NULL;
    return list;
}

/* ---- Back navigation ---------------------------------------------------------------------------- */

static void
RunProtocol(WmInfo *w, Tcl_Obj *cmd)
{
    Tcl_Interp *interp = w->winPtr->mainPtr->interp;

    Tcl_Preserve(interp);
    Tcl_IncrRefCount(cmd);
    if (Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL) != TCL_OK) {
	Tcl_BackgroundException(interp, TCL_ERROR);
    }
    Tcl_DecrRefCount(cmd);
    Tcl_Release(interp);
}

void
TkAHandleBack(void)
{
    TkADrawable *c;

    /* topmost mapped toplevel first */
    for (c = tkA.root->last; c != NULL; c = c->prev) {
	WmInfo *w;
	Tcl_HashEntry *e;

	if (c->winPtr == NULL || !c->mapped) {
	    continue;
	}
	w = (WmInfo *) c->winPtr->wmInfoPtr;
	if (w == NULL) {
	    continue;
	}
	e = Tcl_FindHashEntry(&w->protocols, "WM_DELETE_WINDOW");
	if (e != NULL) {
	    RunProtocol(w, (Tcl_Obj *) Tcl_GetHashValue(e));
	    return;
	}
	if (!IsMain(w)) {
	    /* a layer without a handler: just dismiss it */
	    TkpWmSetState(w->winPtr, WithdrawnState);
	    return;
	}
    }
    if (tkAHost.backUnhandled) {
	tkAHost.backUnhandled();
    }
}

/* ---- the `wm` command -------------------------------------------------------------------------- */

static Tcl_Obj *
MiscGet(WmInfo *w, const char *key, const char *dflt)
{
    Tcl_HashEntry *e = Tcl_FindHashEntry(&w->misc, key);

    return e ? (Tcl_Obj *) Tcl_GetHashValue(e) : Tcl_NewStringObj(dflt, TCL_AUTO_LENGTH);
}

static void
MiscSet(WmInfo *w, const char *key, Tcl_Obj *val)
{
    int isNew;
    Tcl_HashEntry *e = Tcl_CreateHashEntry(&w->misc, key, &isNew);

    if (!isNew) {
	Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(e));
    }
    Tcl_IncrRefCount(val);
    Tcl_SetHashValue(e, val);
}

static int
ParseGeometry(const char *s, int *w, int *h, int *x, int *y, int *hasPos)
{
    int a, b, c, d;
    char sx, sy;

    *w = *h = -1;
    *hasPos = 0;
    if (*s == '=') s++;
    if (sscanf(s, "%dx%d%c%d%c%d", &a, &b, &sx, &c, &sy, &d) == 6) {
	*w = a; *h = b; *x = sx == '-' ? tkA.width - a - c : c; *y = sy == '-' ? tkA.height - b - d : d;
	*hasPos = 1;
	return 1;
    }
    if (sscanf(s, "%dx%d", &a, &b) == 2) {
	*w = a; *h = b;
	return 1;
    }
    if (sscanf(s, "%c%d%c%d", &sx, &c, &sy, &d) == 4 && (sx == '+' || sx == '-')) {
	*x = sx == '-' ? -c : c; *y = sy == '-' ? -d : d;
	*hasPos = 1;
	return 1;
    }
    return 0;
}

int
Tk_WmObjCmd(void *clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    static const char *const subs[] = {
	"aspect", "attributes", "client", "colormapwindows", "command", "deiconify",
	"focusmodel", "forget", "frame", "geometry", "grid", "group", "iconbadge",
	"iconbitmap", "iconify", "iconmask", "iconname", "iconphoto", "iconposition",
	"iconwindow", "manage", "maxsize", "minsize", "overrideredirect", "positionfrom",
	"protocol", "resizable", "sizefrom", "stackorder", "state", "title", "transient",
	"withdraw", NULL
    };
    enum { AspectI, AttributesI, ClientI, ColormapwindowsI, CommandI, DeiconifyI,
	FocusmodelI, ForgetI, FrameI, GeometryI, GridI, GroupI, IconbadgeI,
	IconbitmapI, IconifyI, IconmaskI, IconnameI, IconphotoI, IconpositionI,
	IconwindowI, ManageI, MaxsizeI, MinsizeI, OverrideredirectI, PositionfromI,
	ProtocolI, ResizableI, SizefromI, StackorderI, StateI, TitleI, TransientI,
	WithdrawI };
    Tk_Window main = (Tk_Window) clientData, tkwin;
    TkWindow *winPtr;
    WmInfo *w;
    int idx;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option window ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], subs, "option", 0, &idx) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?arg ...?");
	return TCL_ERROR;
    }
    if (idx == ManageI || idx == ForgetI) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("wm %s is not yet supported on Android", subs[idx]));
	return TCL_ERROR;
    }
    tkwin = Tk_NameToWindow(interp, Tcl_GetString(objv[2]), main);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    winPtr = (TkWindow *) tkwin;
    if (!(winPtr->flags & TK_TOP_HIERARCHY) || winPtr->wmInfoPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("window \"%s\" isn't a top-level window",
		Tcl_GetString(objv[2])));
	return TCL_ERROR;
    }
    w = (WmInfo *) winPtr->wmInfoPtr;

    switch (idx) {
    case TitleI:
	if (objc == 4) {
	    if (w->title) Tcl_DecrRefCount(w->title);
	    w->title = objv[3];
	    Tcl_IncrRefCount(w->title);
	    if (IsMain(w) && tkAHost.setTitle) tkAHost.setTitle(Tcl_GetString(w->title));
	} else if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "window ?newTitle?");
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, w->title ? w->title : Tcl_NewStringObj(winPtr->nameUid, TCL_AUTO_LENGTH));
	return TCL_OK;

    case GeometryI:
	if (objc == 4) {
	    const char *s = Tcl_GetString(objv[3]);
	    int gw, gh, gx = 0, gy = 0, hp;

	    if (s[0] == '\0') {
		w->width = w->height = -1;
		w->hasPos = 0;
	    } else if (!ParseGeometry(s, &gw, &gh, &gx, &gy, &hp)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad geometry specifier \"%s\"", s));
		return TCL_ERROR;
	    } else {
		if (gw > 0) { w->width = gw; w->height = gh; }
		if (hp) { w->hasPos = 1; w->x = gx; w->y = gy; }
	    }
	    if (winPtr->flags & TK_MAPPED) Apply(w);
	    return TCL_OK;
	}
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("%dx%d+%d+%d", Tk_Width(tkwin), Tk_Height(tkwin),
		winPtr->changes.x, winPtr->changes.y));
	return TCL_OK;

    case StateI:
	if (objc == 4) {
	    static const char *const st[] = {"normal", "iconic", "withdrawn", "zoomed", NULL};
	    int s;

	    if (Tcl_GetIndexFromObj(interp, objv[3], st, "argument", 0, &s) != TCL_OK) return TCL_ERROR;
	    TkpWmSetState(winPtr, s == 1 ? IconicState : s == 2 ? WithdrawnState : NormalState);
	    return TCL_OK;
	}
	Tcl_SetObjResult(interp, Tcl_NewStringObj(w->withdrawn ? "withdrawn" : w->iconic ? "iconic" : "normal", TCL_AUTO_LENGTH));
	return TCL_OK;

    case WithdrawI:
	TkpWmSetState(winPtr, WithdrawnState);
	return TCL_OK;
    case DeiconifyI:
	TkpWmSetState(winPtr, NormalState);
	return TCL_OK;
    case IconifyI:
	TkpWmSetState(winPtr, IconicState);
	return TCL_OK;

    case ProtocolI: {
	Tcl_HashEntry *e;

	if (objc == 3) {
	    Tcl_Obj *list = Tcl_NewObj();
	    Tcl_HashSearch s;

	    for (e = Tcl_FirstHashEntry(&w->protocols, &s); e; e = Tcl_NextHashEntry(&s)) {
		Tcl_ListObjAppendElement(NULL, list, Tcl_NewStringObj(Tcl_GetHashKey(&w->protocols, e), TCL_AUTO_LENGTH));
	    }
	    Tcl_SetObjResult(interp, list);
	    return TCL_OK;
	}
	e = Tcl_FindHashEntry(&w->protocols, Tcl_GetString(objv[3]));
	if (objc == 4) {
	    if (e) Tcl_SetObjResult(interp, (Tcl_Obj *) Tcl_GetHashValue(e));
	    return TCL_OK;
	}
	if (e) {
	    Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(e));
	    Tcl_DeleteHashEntry(e);
	}
	if (Tcl_GetCharLength(objv[4]) > 0) {
	    int isNew;

	    e = Tcl_CreateHashEntry(&w->protocols, Tcl_GetString(objv[3]), &isNew);
	    Tcl_IncrRefCount(objv[4]);
	    Tcl_SetHashValue(e, objv[4]);
	}
	return TCL_OK;
    }

    case TransientI:
	if (objc == 4) {
	    if (Tcl_GetCharLength(objv[3]) == 0) {
		w->transientFor = NULL;
	    } else {
		Tk_Window m = Tk_NameToWindow(interp, Tcl_GetString(objv[3]), main);

		if (m == NULL) return TCL_ERROR;
		w->transientFor = (TkWindow *) m;
	    }
	    return TCL_OK;
	}
	Tcl_SetObjResult(interp, Tcl_NewStringObj(w->transientFor ? w->transientFor->pathName : "", TCL_AUTO_LENGTH));
	return TCL_OK;

    case OverrideredirectI:
	if (objc == 4) {
	    int b;

	    if (Tcl_GetBooleanFromObj(interp, objv[3], &b) != TCL_OK) return TCL_ERROR;
	    w->overrideRedirect = b;
	    Tk_ChangeWindowAttributes(tkwin, CWOverrideRedirect, &(XSetWindowAttributes) {.override_redirect = b});
	    return TCL_OK;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(w->overrideRedirect));
	return TCL_OK;

    case FrameI:
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("0x%lx", (unsigned long) winPtr->window));
	return TCL_OK;

    case StackorderI: {
	TkWindow **order = TkWmStackorderToplevel(winPtr), **p;
	Tcl_Obj *list = Tcl_NewObj();

	for (p = order; *p != NULL; p++) {
	    Tcl_ListObjAppendElement(NULL, list, Tcl_NewStringObj((*p)->pathName, TCL_AUTO_LENGTH));
	}
	ckfree(order);
	Tcl_SetObjResult(interp, list);
	return TCL_OK;
    }

    case AttributesI: {
	static const char *const known[] = {"-alpha", "-fullscreen", "-topmost", "-type", "-zoomed", NULL};
	static const char *const dflt[] = {"1.0", "0", "0", "", "0"};
	int i;

	if (objc == 3) {
	    Tcl_Obj *list = Tcl_NewObj();

	    for (i = 0; known[i]; i++) {
		Tcl_ListObjAppendElement(NULL, list, Tcl_NewStringObj(known[i], TCL_AUTO_LENGTH));
		Tcl_ListObjAppendElement(NULL, list, MiscGet(w, known[i], dflt[i]));
	    }
	    Tcl_SetObjResult(interp, list);
	    return TCL_OK;
	}
	if (Tcl_GetIndexFromObj(interp, objv[3], known, "attribute", 0, &i) != TCL_OK) return TCL_ERROR;
	if (objc == 4) {
	    Tcl_SetObjResult(interp, MiscGet(w, known[i], dflt[i]));
	    return TCL_OK;
	}
	MiscSet(w, known[i], objv[4]);
	if (i == 1 && IsMain(w) && tkAHost.setFullscreen) {
	    int on;

	    if (Tcl_GetBooleanFromObj(NULL, objv[4], &on) == TCL_OK) tkAHost.setFullscreen(on);
	}
	return TCL_OK;
    }

    default: {
	/* Accepted: store what is set, return it (or a default) when queried. */
	const char *name = subs[idx];
	const char *dflt = "";

	if (idx == FocusmodelI) dflt = "passive";
	else if (idx == ResizableI) dflt = "1 1";
	else if (idx == MinsizeI) dflt = "1 1";
	else if (idx == MaxsizeI) dflt = "";
	if (objc == 3) {
	    Tcl_SetObjResult(interp, MiscGet(w, name, dflt));
	    return TCL_OK;
	}
	if (idx == MaxsizeI && objc == 3) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("%d %d", tkA.width, tkA.height));
	    return TCL_OK;
	}
	MiscSet(w, name, Tcl_NewListObj(objc - 3, objv + 3));
	return TCL_OK;
    }
    }
    return TCL_OK;
}
