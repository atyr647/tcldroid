/*
 * tkAndroidSystem.c --
 *
 *	Device feedback and facts for scripts: vibration, haptics, sound effects,
 *	`bell`, and ::tkdroid::device. Thin wrappers over host callbacks; with no host
 *	(headless tests) they do nothing, so scripts need no guards.
 *
 *	  ::tkdroid::vibrate ms ?amplitude?
 *	  ::tkdroid::haptic ?tick|click|heavy|confirm|reject?
 *	  ::tkdroid::sound preload path ?path ...?
 *	  ::tkdroid::sound play path ?-volume 0..1? ?-rate 0.5..2?
 *	  ::tkdroid::sound stop
 *	  ::tkdroid::device ?key?
 */

#include "tkAndroidPriv.h"

int
XBell(Display *d, int percent)
{
    (void) d; (void) percent;
    if (tkAHost.bell != NULL) {
	tkAHost.bell();
    }
    return 0;
}

static int
VibrateCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    int ms, amp = 0;

    (void) cd;
    if (objc < 2 || objc > 3 || Tcl_GetIntFromObj(interp, objv[1], &ms) != TCL_OK
	    || (objc == 3 && Tcl_GetIntFromObj(interp, objv[2], &amp) != TCL_OK)) {
	Tcl_WrongNumArgs(interp, 1, objv, "milliseconds ?amplitude?");
	return TCL_ERROR;
    }
    if (ms < 0 || ms > 5000 || amp < 0 || amp > 255) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"milliseconds must be 0..5000 and amplitude 0..255", TCL_AUTO_LENGTH));
	return TCL_ERROR;
    }
    if (ms > 0 && tkAHost.vibrate != NULL) {
	tkAHost.vibrate(ms, amp);
    }
    return TCL_OK;
}

static int
HapticCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    static const char *const kinds[] = {"tick", "click", "heavy", "confirm", "reject", NULL};
    int k = TKA_HAPTIC_CLICK;

    (void) cd;
    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?tick|click|heavy|confirm|reject?");
	return TCL_ERROR;
    }
    if (objc == 2 && Tcl_GetIndexFromObj(interp, objv[1], kinds, "kind", 0, &k) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tkAHost.haptic != NULL) {
	tkAHost.haptic(k);
    }
    return TCL_OK;
}

static int
CheckReadable(Tcl_Interp *interp, const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("couldn't read sound file \"%s\"", path));
	return TCL_ERROR;
    }
    fclose(f);
    return TCL_OK;
}

static int
SoundCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    static const char *const subs[] = {"preload", "play", "stop", NULL};
    int sub, i;

    (void) cd;
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "preload|play|stop ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], subs, "subcommand", 0, &sub) != TCL_OK) {
	return TCL_ERROR;
    }
    switch (sub) {
    case 0:
	for (i = 2; i < objc; i++) {
	    if (CheckReadable(interp, Tcl_GetString(objv[i])) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (tkAHost.soundPreload != NULL) {
		tkAHost.soundPreload(Tcl_GetString(objv[i]));
	    }
	}
	return TCL_OK;
    case 1: {
	double vol = 1.0, rate = 1.0;
	const char *path;

	if (objc < 3 || (objc % 2) == 0) {
	    Tcl_WrongNumArgs(interp, 2, objv, "path ?-volume v? ?-rate r?");
	    return TCL_ERROR;
	}
	path = Tcl_GetString(objv[2]);
	for (i = 3; i < objc; i += 2) {
	    const char *opt = Tcl_GetString(objv[i]);
	    double *dst = strcmp(opt, "-volume") == 0 ? &vol : strcmp(opt, "-rate") == 0 ? &rate : NULL;

	    if (dst == NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad option \"%s\": must be -volume or -rate", opt));
		return TCL_ERROR;
	    }
	    if (Tcl_GetDoubleFromObj(interp, objv[i + 1], dst) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (vol < 0) vol = 0;
	if (vol > 1) vol = 1;
	if (rate < 0.5) rate = 0.5;
	if (rate > 2) rate = 2;
	if (CheckReadable(interp, path) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (tkAHost.soundPlay != NULL) {
	    tkAHost.soundPlay(path, (int) (vol * 1000), (int) (rate * 1000));
	}
	return TCL_OK;
    }
    default:
	if (tkAHost.soundStop != NULL) {
	    tkAHost.soundStop();
	}
	return TCL_OK;
    }
}

/* ::tkdroid::device ?key? -- a dict of device/app facts (see MainActivity.systemInfo). */
static int
InfoCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *d = Tcl_NewDictObj();
    char *text, *p, *line;

    (void) cd;
    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?key?");
	return TCL_ERROR;
    }
    Tcl_DictObjPut(NULL, d, Tcl_NewStringObj("android", TCL_AUTO_LENGTH), Tcl_NewIntObj(1));
    text = tkAHost.systemInfo != NULL ? tkAHost.systemInfo() : NULL;
    for (p = text; p != NULL && *p != '\0'; ) {
	char *tab, *nl;

	line = p;
	nl = strchr(p, '\n');
	if (nl != NULL) {
	    *nl = '\0';
	    p = nl + 1;
	} else {
	    p += strlen(p);
	}
	tab = strchr(line, '\t');
	if (tab != NULL) {
	    *tab = '\0';
	    Tcl_DictObjPut(NULL, d, Tcl_NewStringObj(line, TCL_AUTO_LENGTH),
		    Tcl_NewStringObj(tab + 1, TCL_AUTO_LENGTH));
	}
    }
    free(text);
    if (objc == 2) {
	Tcl_Obj *v = NULL;

	Tcl_DictObjGet(NULL, d, objv[1], &v);
	if (v == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown key \"%s\"", Tcl_GetString(objv[1])));
	    Tcl_DecrRefCount(d);
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, v);
	Tcl_DecrRefCount(d);
	return TCL_OK;
    }
    Tcl_SetObjResult(interp, d);
    return TCL_OK;
}

/*
 * ::tkdroid::request kind ?key value ...?  -> id
 * The primitive under the asynchronous services (file picker, share, permissions...): hands the
 * request to the host and returns an id; the answer arrives later through ::tkdroid::native result
 * and is routed to the -command that the Tcl wrappers in android.tcl registered. With no host
 * (headless tests) the answer is an immediate "error".
 */
static int
RequestCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    static int nextId = 1;
    Tcl_DString args;
    Tcl_Size i;
    int id = nextId++;

    (void) cd;
    if (objc < 2 || (objc % 2) != 0) {
	Tcl_WrongNumArgs(interp, 1, objv, "kind ?key value ...?");
	return TCL_ERROR;
    }
    Tcl_DStringInit(&args);
    for (i = 2; i < objc; i += 2) {
	const char *k = Tcl_GetString(objv[i]), *v = Tcl_GetString(objv[i + 1]);

	Tcl_DStringAppend(&args, k, TCL_INDEX_NONE);
	Tcl_DStringAppend(&args, "\t", 1);
	for (; *v != '\0'; v++) {		/* tabs and newlines would break the framing */
	    Tcl_DStringAppend(&args, (*v == '\t' || *v == '\n') ? " " : v, 1);
	}
	Tcl_DStringAppend(&args, "\n", 1);
    }
    if (tkAHost.request != NULL) {
	tkAHost.request(id, Tcl_GetString(objv[1]), Tcl_DStringValue(&args));
    } else {
	TkAQueueResult(id, "error", "no Android host");
    }
    Tcl_DStringFree(&args);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(id));
    return TCL_OK;
}

/* ::tkdroid::permitted permission -> 1/0 (is the Android permission granted?) */
static int
PermittedCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    (void) cd;
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "permission");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(tkAHost.checkPermission != NULL
	    ? tkAHost.checkPermission(Tcl_GetString(objv[1])) : 0));
    return TCL_OK;
}

/* ::tkdroid::inputmode ?code|text|number|email|url|password?  -- what the soft keyboard offers. */
static int
InputModeCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    static const char *const modes[] = {"code", "text", "number", "email", "url", "password", NULL};
    static int current;
    int m;

    (void) cd;
    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?code|text|number|email|url|password?");
	return TCL_ERROR;
    }
    if (objc == 2) {
	if (Tcl_GetIndexFromObj(interp, objv[1], modes, "mode", 0, &m) != TCL_OK) {
	    return TCL_ERROR;
	}
	current = m;
	if (tkAHost.inputMode != NULL) {
	    tkAHost.inputMode(m);
	}
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(modes[current], TCL_AUTO_LENGTH));
    return TCL_OK;
}

int
TkAInitSystemCommands(Tcl_Interp *interp)
{
    Tcl_CreateObjCommand2(interp, "::tkdroid::inputmode", InputModeCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::request", RequestCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::permitted", PermittedCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::vibrate", VibrateCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::haptic", HapticCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::sound", SoundCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tkdroid::device", InfoCmd, NULL, NULL);
    return TCL_OK;
}
