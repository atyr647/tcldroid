/*
 * tcldroid_core.h --
 *
 *	JNI-free core of the Android host: starts an unmodified Tcl 9 core in
 *	the calling thread, runs one script, and reports output. It builds for
 *	Android (logcat) and for the build machine (stderr), so the embedding
 *	logic can be tested without a device (tests/host-embed.sh).
 */
#ifndef TCLDROID_CORE_H
#define TCLDROID_CORE_H

#include <tcl.h>

typedef void (TclDroidOutputProc)(void *ctx, const char *buf, int len);

typedef struct {
    const char *libDir;		/* directory containing libtcl9.0.so */
    const char *filesDir;	/* becomes HOME */
    const char *cacheDir;	/* becomes TMPDIR */
    const char *tz;		/* becomes TZ (Olson name) */
    const char *script;		/* UTF-8 Tcl source */
    TclDroidOutputProc *onOutput; /* raw stdout/stderr bytes; may be NULL */
    /* Called after Tcl_Init() and before the script (e.g. to load Tk); may be NULL.
     * A non-TCL_OK return aborts the run with the interpreter result as the error. */
    int (*onInit)(Tcl_Interp *interp, void *ctx);
    void *ctx;
} TclDroidConfig;

/*
 * Runs on the calling thread, which must be dedicated to Tcl (the Tcl thread
 * state is finalized before returning). Returns 1 if the script finished with
 * TCL_OK. *result receives the malloc'd UTF-8 interpreter result or error
 * message (never NULL on return); the caller frees it.
 */
int tcldroid_run(const TclDroidConfig *cfg, char **result, int *resultLen);

#endif
