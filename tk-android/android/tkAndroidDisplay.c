/*
 * tkAndroidDisplay.c --
 *
 *	Display connection, initialisation, and the trivial parts of the
 *	in-process "server" (sync, flush, errors, atoms of the application name,
 *	things that have no meaning without an X server).
 */

/* This file builds the Display structure itself, so it needs the full definition. */
#define XLIB_ILLEGAL_ACCESS
#ifndef _GNU_SOURCE
#define _GNU_SOURCE	/* Dl_info / dladdr on glibc */
#endif
#include "tkAndroidPriv.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <dlfcn.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

TkAState tkA;
static int screenSet;

void
TkALog(const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
#ifdef __ANDROID__
    __android_log_write(ANDROID_LOG_INFO, "tkdroid", buf);
#else
    fprintf(stderr, "[tkdroid] %s\n", buf);
#endif
}

void
TkAStubCalled(const char *name)
{
    static int count;

    if (count++ < 200) {
	TkALog("not implemented: %s", name);
    }
}

/* Called by the host before Tk starts (or when the screen changes). */
void
TkAndroidSetScreen(int width, int height, double dpi)
{
    tkA.width = width > 0 ? width : 720;
    tkA.height = height > 0 ? height : 1280;
    tkA.density = dpi > 0 ? dpi : 160.0;
    screenSet = 1;
}

/*
 * ---------------------------------------------------------------------------
 * Display
 * ---------------------------------------------------------------------------
 */

static const char screenName[] = ":0";
static int (*errorHandler)(Display *, XErrorEvent *);

Display *
XkbOpenDisplay(const char *name, int *ev, int *err, int *major, int *minor,
	int *reason)
{
    _XPrivDisplay display = (_XPrivDisplay) calloc(1, sizeof(Display));
    Screen *screen = (Screen *) calloc(1, sizeof(Screen));
    static char vendor[] = "tkdroid";

    (void) name;
    if (!screenSet) {
	int w = 720, h = 1280;
	double dpi = 320.0;
	const char *env = getenv("TKDROID_SCREEN");	/* e.g. 1080x2200@420: for host tests */

	if (env != NULL) {
	    sscanf(env, "%dx%d@%lf", &w, &h, &dpi);
	}
	TkAndroidSetScreen(w, h, dpi);
    }
    if (tkA.root == NULL) {
	tkA.root = (TkADrawable *) calloc(1, sizeof(TkADrawable));
	tkA.root->isWindow = 1;
	tkA.root->depth = 24;
	tkA.root->mapped = 1;
	tkA.root->w = tkA.width;
	tkA.root->h = tkA.height;
	tkA.root->bg = 0x000000;
	tkA.root->pix = (uint32_t *) calloc((size_t) tkA.width * (size_t) tkA.height,
		sizeof(uint32_t));
    }
    display->fd = 0;
    display->qlen = 0;
    display->screens = screen;
    display->nscreens = 1;
    display->default_screen = 0;
    display->display_name = (char *) screenName;
    display->vendor = vendor;
    display->release = 1;
    LastKnownRequestProcessed(display) = 1;

    screen->root = TKA_ID(tkA.root);
    screen->display = (Display *) display;
    screen->black_pixel = 0x00000000;
    screen->white_pixel = 0x00FFFFFF;
    screen->width = tkA.width;
    screen->height = tkA.height;
    screen->mwidth = (int) (tkA.width * 25.4 / tkA.density);
    screen->mheight = (int) (tkA.height * 25.4 / tkA.density);
    screen->root_depth = 24;
    screen->cmap = 1;
    screen->root_visual = (Visual *) calloc(1, sizeof(Visual));
    screen->root_visual->visualid = 1;
    screen->root_visual->c_class = TrueColor;
    screen->root_visual->red_mask = 0x00FF0000;
    screen->root_visual->green_mask = 0x0000FF00;
    screen->root_visual->blue_mask = 0x000000FF;
    /* Tk's shared XGetVisualInfo() (xlib/xutil.c) reports depth = bits_per_rgb, as the macOS port relies on. */
    screen->root_visual->bits_per_rgb = 24;
    screen->root_visual->map_entries = 256;
    if (ev) *ev = 0;
    if (err) *err = 0;
    if (major) *major = 0;
    if (minor) *minor = 0;
    if (reason) *reason = 0;
    return (Display *) display;
}

TkDisplay *
TkpOpenDisplay(const char *displayName)
{
    static TkDisplay *opened;

    if (opened != NULL) {
	return strcmp(DisplayString(opened->display), displayName) == 0 || displayName[0] == '\0'
		? opened : NULL;
    }
    tkA.display = XkbOpenDisplay(displayName, NULL, NULL, NULL, NULL, NULL);
    opened = (TkDisplay *) ckalloc(sizeof(TkDisplay));
    memset(opened, 0, sizeof(TkDisplay));
    opened->display = tkA.display;
    tkA.dispPtr = opened;
    TkpInitKeymapInfo(opened);
    TkCreateXEventSource();
    return opened;
}

void
TkpCloseDisplay(TkDisplay *dispPtr)
{
    (void) dispPtr;
}

const char *
TkGetDefaultScreenName(Tcl_Interp *interp, const char *name)
{
    (void) interp;
    return (name != NULL && name[0] != '\0') ? name : screenName;
}

void
TkGetServerInfo(Tcl_Interp *interp, Tk_Window tkwin)
{
    (void) tkwin;
    Tcl_SetObjResult(interp, Tcl_NewStringObj("Android", TCL_AUTO_LENGTH));
}

/*
 * Tk's script library (tk.tcl, widgets' bindings, ...) is a zip appended to this
 * shared library, like Tcl's own. Mount it by opening the library file itself,
 * which is found with dladdr(); then tcl_findLibrary() locates tk.tcl there.
 * (With TK_LIBRARY set, as in host tests, the mount is simply unused.)
 */
static void
MountLibrary(Tcl_Interp *interp)
{
    Dl_info info;
    Tcl_Obj *cmd;

    if (!dladdr((void *) TkpInit, &info) || info.dli_fname == NULL) {
	return;
    }
    cmd = Tcl_ObjPrintf("catch {zipfs mount {%s} //zipfs:/lib/tk}", info.dli_fname);
    Tcl_IncrRefCount(cmd);
    Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(cmd);
    if (Tcl_EvalEx(interp, "file exists //zipfs:/lib/tk/tk.tcl", TCL_INDEX_NONE, TCL_EVAL_GLOBAL) == TCL_OK
	    && strcmp(Tcl_GetStringResult(interp), "1") == 0) {
	Tcl_SetVar2(interp, "tk_library", NULL, "//zipfs:/lib/tk", TCL_GLOBAL_ONLY);
    }
    Tcl_ResetResult(interp);
}

int
TkpInit(Tcl_Interp *interp)
{
    tkA.tclThread = Tcl_GetCurrentThread();
    MountLibrary(interp);
    if (tkAEngine == NULL) {
	tkAEngine = TkAGetBuiltinEngine();
    }
    return TkAInitCommands(interp);
}

void
TkpGetAppName(Tcl_Interp *interp, Tcl_DString *namePtr)
{
    const char *name = Tcl_GetVar2(interp, "argv0", NULL, TCL_GLOBAL_ONLY);
    const char *p;

    if (name == NULL || name[0] == '\0') {
	name = "tk";
    }
    p = strrchr(name, '/');
    Tcl_DStringAppend(namePtr, p != NULL ? p + 1 : name, TCL_INDEX_NONE);
}

/* There is no `send` registry on Android, so the name is taken as given. */
const char *
Tk_SetAppName(Tk_Window tkwin, const char *name)
{
    (void) tkwin;
    return name;
}

void
TkpDisplayWarning(const char *msg, const char *title)
{
    TkALog("%s: %s", title, msg);
}

unsigned long
TkpGetMS(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long) (ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

long
Tk_GetUserInactiveTime(Display *dpy)
{
    (void) dpy;
    return -1;		/* not available */
}

void
Tk_ResetUserInactiveTime(Display *dpy)
{
    (void) dpy;
}

Tcl_Obj *
Tk_GetSystemDefault(Tk_Window tkwin, const char *dbName, const char *className)
{
    (void) tkwin;
    (void) dbName;
    (void) className;
    return NULL;
}

TkWindow *
TkpGetWrapperWindow(TkWindow *winPtr)
{
    return winPtr;	/* there is no wrapper window: the toplevel is its own */
}

/* ---- things that have no meaning without an X server ------------------------ */

int XSync(Display *d, Bool discard) { (void) d; (void) discard; return 0; }
int XFlush(Display *d) { (void) d; return 0; }
int XNoOp(Display *d) { (void) d; return 0; }
void TkpSync(Display *d) { (void) d; }
XAfterFunction XSynchronize(Display *d, Bool onoff) { (void) d; (void) onoff; return NULL; }
int XGrabServer(Display *d) { (void) d; return 0; }
int XUngrabServer(Display *d) { (void) d; return 0; }
int XForceScreenSaver(Display *d, int mode) { (void) d; (void) mode; return 0; }
/* Tk frees X-API results (XGetVisualInfo, XQueryTree, ...) with XFree: they are all ckalloc'd. */
int XFree(void *data) { ckfree(data); return 0; }
XHostAddress *XListHosts(Display *d, int *n, Bool *state) { (void) d; *n = 0; *state = False; return NULL; }
int TkpCmapStressed(Tk_Window tkwin, Colormap cmap) { (void) tkwin; (void) cmap; return 0; }
VisualID XVisualIDFromVisual(Visual *v) { return v ? v->visualid : 0; }

XErrorHandler
XSetErrorHandler(XErrorHandler handler)
{
    XErrorHandler old = (XErrorHandler) errorHandler;

    errorHandler = handler;
    return old;
}

Colormap XCreateColormap(Display *d, Window w, Visual *v, int alloc) { (void) d; (void) w; (void) v; (void) alloc; return 1; }
int XFreeColormap(Display *d, Colormap c) { (void) d; (void) c; return 1; }
Status XGetWMColormapWindows(Display *d, Window w, Window **wp, int *n) { (void) d; (void) w; (void) wp; *n = 0; return 0; }
void TkWmAddToColormapWindows(TkWindow *winPtr) { (void) winPtr; }
void TkWmRemoveFromColormapWindows(TkWindow *winPtr) { (void) winPtr; }

/* ---- unsupported by policy: embedding and `send` --------------------------------- */

int
Tk_UseWindow(Tcl_Interp *interp, Tk_Window tkwin, const char *string)
{
    (void) tkwin;
    (void) string;
    Tcl_SetObjResult(interp, Tcl_NewStringObj("embedding is not supported on Android", TCL_AUTO_LENGTH));
    return TCL_ERROR;
}

void Tk_MakeContainer(Tk_Window tkwin) { (void) tkwin; }
Tk_Window Tk_GetOtherWindow(Tk_Window tkwin) { (void) tkwin; return NULL; }
Window TkUnixContainerId(TkWindow *winPtr) { (void) winPtr; return None; }
void TkSendCleanup(TkDisplay *dispPtr) { (void) dispPtr; }

int
TkGetInterpNames(Tcl_Interp *interp, Tk_Window tkwin)
{
    (void) tkwin;
    Tcl_SetObjResult(interp, Tcl_NewObj());
    return TCL_OK;
}

static int
Unsupported(Tcl_Interp *interp, const char *what)
{
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s is not supported on Android", what));
    return TCL_ERROR;
}

int
TkpTestembedCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    (void) cd; (void) objc; (void) objv;
    return Unsupported(interp, "testembed");
}

int
TkpTestsendCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    (void) cd; (void) objc; (void) objv;
    return Unsupported(interp, "testsend");
}

int XSetCommand(Display *d, Window w, char **argv, int argc) { (void) d; (void) w; (void) argv; (void) argc; return 0; }
void XSetWMClientMachine(Display *d, Window w, XTextProperty *p) { (void) d; (void) w; (void) p; }
Status XStringListToTextProperty(char **list, int count, XTextProperty *p) { (void) list; (void) count; (void) p; return 0; }

/* ---- busy windows (`tk busy`): not yet implemented ------------------------------------ */

void
TkpCreateBusy(Tk_FakeWin *winPtr, Tk_Window tkRef, Window *parentPtr,
	Tk_Window tkParent, TkBusy busy)
{
    (void) winPtr; (void) tkRef; (void) parentPtr; (void) tkParent; (void) busy;
    TkAStubCalled("TkpCreateBusy");
}

void TkpHideBusyWindow(TkBusy busy) { (void) busy; }
void TkpShowBusyWindow(TkBusy busy) { (void) busy; }
