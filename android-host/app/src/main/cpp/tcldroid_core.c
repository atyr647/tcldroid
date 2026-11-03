/*
 * tcldroid_core.c -- see tcldroid_core.h.
 *
 * Android specifics handled here, outside Tcl's sources:
 *
 *   - The process has no usable stdout/stderr, so Tcl's std channels are
 *     replaced by channels that write to logcat and to a callback. Scripts
 *     keep using plain `puts`.
 *   - Android sets no LANG and has no /etc/localtime, so HOME, TMPDIR, TZ and
 *     a UTF-8 locale are provided before Tcl starts, and the system encoding
 *     is forced to utf-8 afterwards.
 *   - Tcl's script library is a zip appended to libtcl9.0.so; the app hook
 *     mounts it (Tcl locates the library with dladdr).
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tcl.h>

#include "tcldroid_core.h"

#ifdef __ANDROID__
#include <android/log.h>
#define TD_LOG_INFO  ANDROID_LOG_INFO
#define TD_LOG_ERROR ANDROID_LOG_ERROR
#define TD_LOGW(prio, s) __android_log_write((prio), "tcldroid", (s))
#else
#define TD_LOG_INFO  0
#define TD_LOG_ERROR 1
#define TD_LOGW(prio, s) fprintf(stderr, "[tcldroid] %s\n", (s))
#endif

typedef struct {
    int prio;
    int len;
    char line[1024];
    const TclDroidConfig *cfg;
} LogChan;

static void
FlushLine(LogChan *c)
{
    if (c->len > 0) {
	c->line[c->len] = '\0';
	TD_LOGW(c->prio, c->line);
	c->len = 0;
    }
}

static int
LogOutput(void *instanceData, const char *buf, int toWrite, int *errorCodePtr)
{
    LogChan *c = (LogChan *) instanceData;
    int i;

    (void) errorCodePtr;
    for (i = 0; i < toWrite; i++) {
	if (buf[i] == '\n') {
	    FlushLine(c);
	} else {
	    c->line[c->len++] = buf[i];
	    if (c->len == (int) sizeof(c->line) - 1) {
		FlushLine(c);
	    }
	}
    }
    if (c->cfg->onOutput != NULL) {
	c->cfg->onOutput(c->cfg->ctx, buf, toWrite);
    }
    return toWrite;
}

static void
LogWatch(void *instanceData, int mask)
{
    (void) instanceData;
    (void) mask;
}

static int
LogClose2(void *instanceData, Tcl_Interp *interp, int flags)
{
    (void) interp;
    (void) flags;
    FlushLine((LogChan *) instanceData);
    return 0;
}

static const Tcl_ChannelType logChanType = {
    .typeName = "androidlog",
    .version = TCL_CHANNEL_VERSION_5,
    .outputProc = LogOutput,
    .watchProc = LogWatch,
    .close2Proc = LogClose2,
};

static LogChan *
InstallStdChannel(const char *name, int prio, int which,
	const TclDroidConfig *cfg)
{
    LogChan *state = calloc(1, sizeof(LogChan));
    Tcl_Channel ch;

    state->prio = prio;
    state->cfg = cfg;
    ch = Tcl_CreateChannel(&logChanType, name, state, TCL_WRITABLE);
    Tcl_SetChannelOption(NULL, ch, "-encoding", "utf-8");
    Tcl_SetChannelOption(NULL, ch, "-buffering", "line");
    Tcl_SetStdChannel(ch, which);
    return state;
}

static char *
DupN(const char *s, Tcl_Size n)
{
    char *r = malloc((size_t) n + 1);

    memcpy(r, s, (size_t) n);
    r[n] = '\0';
    return r;
}

int
tcldroid_run(const TclDroidConfig *cfg, char **result, int *resultLen)
{
    char arg0[PATH_MAX];
    char *argvStorage[2];
    char **argv = argvStorage;
    int argc = 1;
    int ok = 0;
    Tcl_Interp *interp;
    Tcl_Size len;
    const char *msg;
    LogChan *out, *err;

    setenv("HOME", cfg->filesDir, 1);
    setenv("TMPDIR", cfg->cacheDir, 1);
    setenv("TZ", cfg->tz, 1);
    setenv("LANG", "en_US.UTF-8", 1);

    /*
     * The "executable" is nominal; what matters is that Tcl_FindExecutable()
     * runs, and that the app hook then mounts the zip attached to the shared
     * library (path discovered by dladdr, i.e. nativeLibraryDir/libtcl9.0.so).
     */
    snprintf(arg0, sizeof(arg0), "%s/libtcldroid.so", cfg->libDir);
    argvStorage[0] = arg0;
    argvStorage[1] = NULL;
    TclZipfs_AppHook(&argc, &argv);

    out = InstallStdChannel("stdout", TD_LOG_INFO, TCL_STDOUT, cfg);
    err = InstallStdChannel("stderr", TD_LOG_ERROR, TCL_STDERR, cfg);

    interp = Tcl_CreateInterp();
    if (Tcl_Init(interp) != TCL_OK) {
	msg = Tcl_GetStringFromObj(Tcl_GetObjResult(interp), &len);
	TD_LOGW(TD_LOG_ERROR, msg);
	*result = DupN(msg, len);
	*resultLen = (int) len;
    } else {
	int code;

	Tcl_SetSystemEncoding(interp, "utf-8");
	if (cfg->onInit != NULL) {
	    code = cfg->onInit(interp, cfg->ctx);
	} else {
	    code = TCL_OK;
	}
	if (code == TCL_OK) {
	    code = Tcl_EvalEx(interp, cfg->script, TCL_INDEX_NONE,
		    TCL_EVAL_GLOBAL);
	}
	msg = Tcl_GetStringFromObj(Tcl_GetObjResult(interp), &len);
	if (code != TCL_OK) {
	    TD_LOGW(TD_LOG_ERROR, msg);
	}
	*result = DupN(msg, len);
	*resultLen = (int) len;
	ok = (code == TCL_OK);
    }
    FlushLine(out);
    FlushLine(err);
    Tcl_DeleteInterp(interp);
    Tcl_FinalizeThread();	/* closes the std channels (uses out/err) */
    free(out);
    free(err);
    return ok;
}
