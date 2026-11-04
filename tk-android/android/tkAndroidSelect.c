/*
 * tkAndroidSelect.c --
 *
 *	Selection and clipboard.
 *
 *	PRIMARY (text selected in a widget) lives entirely inside Tk, as on every
 *	platform: generic/tkSelect.c serves it directly while the owner is in this
 *	process, and Android has no second application to hand it to.
 *
 *	CLIPBOARD is the Android system clipboard (ClipboardManager, via the host):
 *	- `clipboard append` renders Tk's buffers to text and puts them on the system
 *	  clipboard;
 *	- `clipboard get` / `selection get -selection CLIPBOARD` read the system
 *	  clipboard unless Tk itself still owns it;
 *	- when another app (or the user) replaces the clipboard, Tk's ownership is dropped
 *	  (TkAClipboardChanged), so Tk never serves stale text.
 *	Only text is supported. Without host callbacks (headless tests) a private
 *	in-process clipboard stands in, so scripts behave the same.
 */

#include "tkAndroidPriv.h"
#include "tkSelect.h"

static char *ourText;		/* what Tk last put on the clipboard (UTF-8), or NULL */
static char *privateClip;	/* stand-in clipboard when the host has none */

static char *
Dup(const char *s)
{
    size_t n = strlen(s);
    char *d = (char *) malloc(n + 1);

    if (d != NULL) {
	memcpy(d, s, n + 1);
    }
    return d;
}

/* The current clipboard text, malloc'd, or NULL if there is none. */
static char *
HostGet(void)
{
    if (tkAHost.getClipboard != NULL) {
	return tkAHost.getClipboard();
    }
    return privateClip != NULL ? Dup(privateClip) : NULL;
}

static void
HostSet(const char *text)
{
    if (tkAHost.setClipboard != NULL) {
	tkAHost.setClipboard(text);
	return;
    }
    free(privateClip);
    privateClip = text != NULL ? Dup(text) : NULL;
}

/* All buffers of the STRING (or UTF8_STRING) target, joined. Caller frees. */
static char *
RenderBuffers(TkDisplay *dispPtr)
{
    TkClipboardTarget *t, *use = NULL;
    TkClipboardBuffer *b;
    size_t len = 0;
    char *out, *p;

    for (t = dispPtr->clipTargetPtr; t != NULL; t = t->nextPtr) {
	if (t->type == XA_STRING || t->type == dispPtr->utf8Atom) {
	    use = t;
	    if (t->type == dispPtr->utf8Atom) {
		break;
	    }
	}
    }
    if (use != NULL) {
	for (b = use->firstBufferPtr; b != NULL; b = b->nextPtr) {
	    len += (size_t) b->length;
	}
    }
    out = (char *) malloc(len + 1);
    if (out == NULL) {
	return NULL;
    }
    p = out;
    if (use != NULL) {
	for (b = use->firstBufferPtr; b != NULL; b = b->nextPtr) {
	    memcpy(p, b->buffer, (size_t) b->length);
	    p += b->length;
	}
    }
    *p = '\0';
    return out;
}

void
TkSelUpdateClipboard(TkWindow *winPtr, clipboardOption opt)
{
    TkDisplay *dispPtr = winPtr->dispPtr;
    char *text;

    if (opt == CLIPBOARD_APPEND) {
	text = RenderBuffers(dispPtr);
	if (text == NULL) {
	    return;
	}
	free(ourText);
	ourText = text;
	HostSet(ourText);
    } else if (opt == CLIPBOARD_CLEAR) {
	free(ourText);
	ourText = Dup("");
	HostSet(NULL);
    }
}

/*
 * Tcl thread: the system clipboard changed. If it no longer holds what Tk put there,
 * Tk is no longer the owner.
 */
void
TkAClipboardChanged(void)
{
    TkDisplay *dispPtr = tkA.dispPtr;
    char *cur;
    int same;

    if (dispPtr == NULL || dispPtr->clipWindow == NULL || !dispPtr->clipboardActive) {
	return;
    }
    cur = HostGet();
    same = strcmp(cur != NULL ? cur : "", ourText != NULL ? ourText : "") == 0;
    free(cur);
    if (!same) {
	Tk_ClearSelection(dispPtr->clipWindow, dispPtr->clipboardAtom);
    }
}

/* Selection owned by "another application": only CLIPBOARD, only text. */
int
TkSelGetSelection(Tcl_Interp *interp, Tk_Window tkwin, Atom selection, Atom target,
	Tk_GetSelProc *proc, void *clientData)
{
    TkDisplay *dispPtr = ((TkWindow *) tkwin)->dispPtr;
    char *text;
    int result;

    if (selection == dispPtr->clipboardAtom
	    && (target == XA_STRING || target == dispPtr->utf8Atom)
	    && (text = HostGet()) != NULL) {
	/* Android line ends are already \n. */
	result = proc(clientData, interp, text);
	free(text);
	return result;
    }
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "%s selection doesn't exist or form \"%s\" not defined",
	    Tk_GetAtomName(tkwin, selection), Tk_GetAtomName(tkwin, target)));
    Tcl_SetErrorCode(interp, "TK", "SELECTION", "EXISTS", (char *) NULL);
    return TCL_ERROR;
}

void
TkClipCleanup(TkDisplay *dispPtr)
{
    (void) dispPtr;
    free(ourText);
    ourText = NULL;
}

void TkSelEventProc(Tk_Window tkwin, XEvent *ev) { (void) tkwin; (void) ev; }
void TkSelPropProc(XEvent *ev) { (void) ev; }

/*
 * ::tkdroid::hostclipboard ?text?  --  test hook. With an argument, replace the system
 * clipboard the way another application would (and notify Tk); without one, return
 * the system clipboard's text ("" if none).
 */
int
TkAClipboardCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    (void) cd;
    if (objc == 2) {
	HostSet(Tcl_GetString(objv[1]));
	TkAQueueClipboardChanged();
    } else if (objc == 1) {
	char *t = HostGet();

	Tcl_SetObjResult(interp, Tcl_NewStringObj(t != NULL ? t : "", TCL_AUTO_LENGTH));
	free(t);
    } else {
	Tcl_WrongNumArgs(interp, 1, objv, "?text?");
	return TCL_ERROR;
    }
    return TCL_OK;
}
