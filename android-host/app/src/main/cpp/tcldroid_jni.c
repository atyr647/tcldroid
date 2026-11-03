/*
 * tcldroid_jni.c --
 *
 *	Minimal JNI host for an unmodified Tcl 9 core (libtcl9.0.so).
 *
 *	Tcl runs on its own thread with its normal blocking event loop (see
 *	docs/adr/0002-tk-backend-architecture.md). Java only starts the thread
 *	and receives output / completion callbacks. All Android specifics are
 *	handled here, outside Tcl's sources:
 *
 *	  - The process has no usable stdout/stderr, so Tcl's std channels are
 *	    replaced by channels that write to logcat and to the Java listener.
 *	    Scripts keep using plain `puts`.
 *	  - Android sets no LANG and has no /etc/localtime, so HOME, TMPDIR, TZ
 *	    and a UTF-8 locale are provided through the environment before Tcl
 *	    starts.
 *	  - Tcl's script library is a zip appended to libtcl9.0.so; the app hook
 *	    mounts it (Tcl locates the library with dladdr).
 */

#include <android/log.h>
#include <jni.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <tcl.h>

#define TAG "tcldroid"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static JavaVM *gVm;
static jmethodID gOnOutput;	/* void onOutput(byte[]) */
static jmethodID gOnDone;	/* void onDone(boolean, byte[]) */

/* Per-Tcl-thread state used by the output channel. */
static __thread JNIEnv *tEnv;
static __thread jobject tListener;

typedef struct {
    const char *libDir, *filesDir, *cacheDir, *tz;
    char *script;
    jobject listener;		/* global ref */
} Job;

/*
 * ---------------------------------------------------------------------------
 * Java callbacks
 * ---------------------------------------------------------------------------
 */

static jbyteArray
ToByteArray(JNIEnv *env, const char *s, jsize n)
{
    jbyteArray a = (*env)->NewByteArray(env, n);
    if (a != NULL && n > 0) {
	(*env)->SetByteArrayRegion(env, a, 0, n, (const jbyte *) s);
    }
    return a;
}

static void
EmitOutput(const char *s, int n)
{
    jbyteArray a;

    if (tEnv == NULL || tListener == NULL) {
	return;
    }
    a = ToByteArray(tEnv, s, n);
    if (a != NULL) {
	(*tEnv)->CallVoidMethod(tEnv, tListener, gOnOutput, a);
	(*tEnv)->DeleteLocalRef(tEnv, a);
    }
    if ((*tEnv)->ExceptionCheck(tEnv)) {
	(*tEnv)->ExceptionClear(tEnv);
    }
}

/*
 * ---------------------------------------------------------------------------
 * stdout/stderr channels: line-buffered to logcat, raw bytes to Java
 * ---------------------------------------------------------------------------
 */

typedef struct {
    int prio;
    int len;
    char line[1024];
} LogChan;

static void
FlushLine(LogChan *c)
{
    if (c->len > 0) {
	c->line[c->len] = '\0';
	__android_log_write(c->prio, TAG, c->line);
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
    EmitOutput(buf, toWrite);
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

static LogChan outChan = { ANDROID_LOG_INFO, 0, {0} };
static LogChan errChan = { ANDROID_LOG_ERROR, 0, {0} };

static void
InstallStdChannel(const char *name, LogChan *state, int which)
{
    Tcl_Channel ch = Tcl_CreateChannel(&logChanType, name, state,
	    TCL_WRITABLE);

    Tcl_SetChannelOption(NULL, ch, "-encoding", "utf-8");
    Tcl_SetChannelOption(NULL, ch, "-buffering", "line");
    Tcl_SetStdChannel(ch, which);
}

/*
 * ---------------------------------------------------------------------------
 * The Tcl thread
 * ---------------------------------------------------------------------------
 */

static void
Finish(Job *job, int ok, const char *result, Tcl_Size len)
{
    jbyteArray a = ToByteArray(tEnv, result, (jsize) len);

    (*tEnv)->CallVoidMethod(tEnv, job->listener, gOnDone,
	    ok ? JNI_TRUE : JNI_FALSE, a);
    if ((*tEnv)->ExceptionCheck(tEnv)) {
	(*tEnv)->ExceptionClear(tEnv);
    }
    if (a != NULL) {
	(*tEnv)->DeleteLocalRef(tEnv, a);
    }
}

static void *
TclThread(void *arg)
{
    Job *job = (Job *) arg;
    JNIEnv *env = NULL;
    char arg0[PATH_MAX];
    char *argvStorage[2];
    char **argv = argvStorage;
    int argc = 1;
    Tcl_Interp *interp;
    int code;
    Tcl_Size len;
    const char *msg;

    (*gVm)->AttachCurrentThread(gVm, &env, NULL);
    tEnv = env;
    tListener = job->listener;

    setenv("HOME", job->filesDir, 1);
    setenv("TMPDIR", job->cacheDir, 1);
    setenv("TZ", job->tz, 1);
    setenv("LANG", "en_US.UTF-8", 1);

    /*
     * The "executable" is nominal; what matters is that Tcl_FindExecutable()
     * runs, and that the app hook then mounts the zip attached to the shared
     * library (path discovered by dladdr, i.e. nativeLibraryDir/libtcl9.0.so).
     */
    snprintf(arg0, sizeof(arg0), "%s/libtcldroid.so", job->libDir);
    argvStorage[0] = arg0;
    argvStorage[1] = NULL;
    TclZipfs_AppHook(&argc, &argv);

    InstallStdChannel("stdout", &outChan, TCL_STDOUT);
    InstallStdChannel("stderr", &errChan, TCL_STDERR);

    interp = Tcl_CreateInterp();
    if (Tcl_Init(interp) != TCL_OK) {
	msg = Tcl_GetStringFromObj(Tcl_GetObjResult(interp), &len);
	LOGE("Tcl_Init failed: %s", msg);
	Finish(job, 0, msg, len);
	goto out;
    }
    Tcl_SetSystemEncoding(interp, "utf-8");

    code = Tcl_EvalEx(interp, job->script, TCL_INDEX_NONE, TCL_EVAL_GLOBAL);
    msg = Tcl_GetStringFromObj(Tcl_GetObjResult(interp), &len);
    if (code != TCL_OK) {
	LOGE("script error: %s", msg);
    }
    FlushLine(&outChan);
    FlushLine(&errChan);
    Finish(job, code == TCL_OK, msg, len);
    Tcl_DeleteInterp(interp);

  out:
    Tcl_FinalizeThread();
    tListener = NULL;
    (*env)->DeleteGlobalRef(env, job->listener);
    free((void *) job->libDir);
    free((void *) job->filesDir);
    free((void *) job->cacheDir);
    free((void *) job->tz);
    free(job->script);
    free(job);
    (*gVm)->DetachCurrentThread(gVm);
    return NULL;
}

/*
 * ---------------------------------------------------------------------------
 * JNI entry points
 * ---------------------------------------------------------------------------
 */

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{
    (void) reserved;
    gVm = vm;
    return JNI_VERSION_1_6;
}

/* Copy a Java byte[] (UTF-8) into a NUL-terminated malloc'd string. */
static char *
DupBytes(JNIEnv *env, jbyteArray a)
{
    jsize n = (*env)->GetArrayLength(env, a);
    char *s = malloc((size_t) n + 1);

    if (s != NULL) {
	(*env)->GetByteArrayRegion(env, a, 0, n, (jbyte *) s);
	s[n] = '\0';
    }
    return s;
}

JNIEXPORT jboolean JNICALL
Java_dev_tcldroid_host_TclRuntime_nativeStart(JNIEnv *env, jclass clazz,
	jbyteArray libDir, jbyteArray filesDir, jbyteArray cacheDir,
	jbyteArray tz, jbyteArray script, jobject listener)
{
    Job *job = calloc(1, sizeof(Job));
    jclass lc;
    pthread_t th;
    pthread_attr_t attr;
    int rc;

    (void) clazz;
    if (job == NULL) {
	return JNI_FALSE;
    }
    lc = (*env)->GetObjectClass(env, listener);
    gOnOutput = (*env)->GetMethodID(env, lc, "onOutput", "([B)V");
    gOnDone = (*env)->GetMethodID(env, lc, "onDone", "(Z[B)V");
    if (gOnOutput == NULL || gOnDone == NULL) {
	free(job);
	return JNI_FALSE;	/* NoSuchMethodError is pending */
    }

    job->libDir = DupBytes(env, libDir);
    job->filesDir = DupBytes(env, filesDir);
    job->cacheDir = DupBytes(env, cacheDir);
    job->tz = DupBytes(env, tz);
    job->script = DupBytes(env, script);
    job->listener = (*env)->NewGlobalRef(env, listener);

    /* Tcl recurses deeply; the 1 MB default thread stack is too small. */
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8 * 1024 * 1024);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    rc = pthread_create(&th, &attr, TclThread, job);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
	LOGE("pthread_create failed: %d", rc);
	return JNI_FALSE;
    }
    return JNI_TRUE;
}
