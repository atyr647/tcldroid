/*
 * tcldroid_jni.c --
 *
 *	JNI entry points. Runs tcldroid_run() (tcldroid_core.c) on a dedicated native
 *	thread and connects Tk's Android platform layer (libtk9.0.so, tkAndroid.h) to
 *	Java:
 *
 *	  Tcl thread -> Java : output, completion, frames (pixel copies into a Java
 *	                       int[]), window title, Back unhandled, text caret
 *	  Java -> Tcl thread : touch, keys, Back, resize (thread-safe queues in libtk)
 *	  Text               : a TkATextEngine implemented by dev.tcldroid.host.TextEngine
 *	                       (android.graphics.Paint) through JNI.
 *
 *	See docs/adr/0002-tk-backend-architecture.md for why Tcl owns a thread.
 */

#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcldroid_core.h"
#include "tkAndroid.h"

/* Android's JNIEnv** versus the JDK's void** (the host JVM test builds the same source). */
#ifdef TCLDROID_HOST_JVM
#define ATTACH_ENV(p) ((void **) (p))
#else
#define ATTACH_ENV(p) (p)
#endif

#define TAG "tcldroid"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static JavaVM *gVm;
static jobject gListener;			/* global ref, lives for the process */
static jmethodID gOnOutput, gOnDone, gAllocFrame, gOnFrame, gOnTitle, gOnBack,
	gOnMoveToBack, gOnCaret;
static jintArray gFrame;			/* global ref */
static int gFrameW, gFrameH;

static jclass gEngine;				/* global ref: dev/tcldroid/host/TextEngine */
static jmethodID gEOpen, gEMeasure, gERender, gEFamilies;

typedef struct {
    char *libDir, *filesDir, *cacheDir, *tz, *script;
    jobject listener;
    JNIEnv *env;
    int width, height, dpi;
} Job;

static JNIEnv *
Env(void)
{
    JNIEnv *e = NULL;

    (*gVm)->GetEnv(gVm, (void **) &e, JNI_VERSION_1_6);
    return e;
}

static void
Check(JNIEnv *e)
{
    if ((*e)->ExceptionCheck(e)) {
	(*e)->ExceptionDescribe(e);
	(*e)->ExceptionClear(e);
    }
}

static jbyteArray
ToByteArray(JNIEnv *env, const char *s, jsize n)
{
    jbyteArray a = (*env)->NewByteArray(env, n);

    if (a != NULL && n > 0) {
	(*env)->SetByteArrayRegion(env, a, 0, n, (const jbyte *) s);
    }
    return a;
}

/* ---- output --------------------------------------------------------------------------- */

static void
OnOutput(void *ctx, const char *buf, int len)
{
    Job *job = (Job *) ctx;
    jbyteArray a = ToByteArray(job->env, buf, len);

    if (a != NULL) {
	(*job->env)->CallVoidMethod(job->env, job->listener, gOnOutput, a);
	(*job->env)->DeleteLocalRef(job->env, a);
    }
    Check(job->env);
}

/* ---- Tk host callbacks (all on the Tcl thread) ---------------------------------------- */

static void
Present(const uint32_t *pix, int w, int h, int x1, int y1, int x2, int y2)
{
    JNIEnv *e = Env();
    int y;

    if (gFrame == NULL || gFrameW != w || gFrameH != h) {
	jintArray a = (jintArray) (*e)->CallObjectMethod(e, gListener, gAllocFrame, w, h);

	Check(e);
	if (a == NULL) {
	    return;
	}
	if (gFrame != NULL) {
	    (*e)->DeleteGlobalRef(e, gFrame);
	}
	gFrame = (jintArray) (*e)->NewGlobalRef(e, a);
	(*e)->DeleteLocalRef(e, a);
	gFrameW = w;
	gFrameH = h;
	x1 = 0; y1 = 0; x2 = w; y2 = h;
    }
    for (y = y1; y < y2; y++) {
	(*e)->SetIntArrayRegion(e, gFrame, y * w + x1, x2 - x1,
		(const jint *) (pix + (size_t) y * (size_t) w + x1));
    }
    (*e)->CallVoidMethod(e, gListener, gOnFrame, x1, y1, x2, y2);
    Check(e);
}

static void
SetTitle(const char *utf8)
{
    JNIEnv *e = Env();
    jbyteArray a = ToByteArray(e, utf8, (jsize) strlen(utf8));

    (*e)->CallVoidMethod(e, gListener, gOnTitle, a);
    (*e)->DeleteLocalRef(e, a);
    Check(e);
}

static void
BackUnhandled(void)
{
    JNIEnv *e = Env();

    (*e)->CallVoidMethod(e, gListener, gOnBack);
    Check(e);
}

static void
MoveToBack(void)
{
    JNIEnv *e = Env();

    (*e)->CallVoidMethod(e, gListener, gOnMoveToBack);
    Check(e);
}

static void
Caret(int x, int y, int height)
{
    JNIEnv *e = Env();

    (*e)->CallVoidMethod(e, gListener, gOnCaret, x, y, height);
    Check(e);
}

static const TkAHost host = {Present, SetTitle, NULL, BackUnhandled, MoveToBack, Caret};

/* ---- text engine: android.graphics.Paint through JNI ----------------------------------- */

static void *
EOpen(const char *family, int px, int bold, int italic, TkAFontInfo *info)
{
    JNIEnv *e = Env();
    jstring fam = family != NULL ? (*e)->NewStringUTF(e, family) : NULL;
    jintArray m = (*e)->NewIntArray(e, 4);
    jint v[4];
    jlong h;

    h = (*e)->CallStaticLongMethod(e, gEngine, gEOpen, fam, px, bold ? JNI_TRUE : JNI_FALSE,
	    italic ? JNI_TRUE : JNI_FALSE, m);
    Check(e);
    (*e)->GetIntArrayRegion(e, m, 0, 4, v);
    if (fam != NULL) (*e)->DeleteLocalRef(e, fam);
    (*e)->DeleteLocalRef(e, m);
    if (h == 0) {
	return NULL;
    }
    info->ascent = v[0];
    info->descent = v[1];
    info->maxWidth = v[2];
    info->fixed = v[3];
    return (void *) (intptr_t) h;
}

static void
EClose(void *font)
{
    (void) font;	/* Paint objects live as long as the process: Tk caches fonts */
}

static int
EMeasure(void *font, const char *s, int n)
{
    JNIEnv *e = Env();
    jbyteArray b = ToByteArray(e, s, n);
    jint w = (*e)->CallStaticIntMethod(e, gEngine, gEMeasure, (jlong) (intptr_t) font, b);

    (*e)->DeleteLocalRef(e, b);
    Check(e);
    return (int) w;
}

/* Java returns [w, h, baseline] as three little-endian ints, then w*h alpha bytes. */
static uint8_t *
ERender(void *font, const char *s, int n, int *ow, int *oh, int *base)
{
    JNIEnv *e = Env();
    jbyteArray b = ToByteArray(e, s, n);
    jbyteArray r = (jbyteArray) (*e)->CallStaticObjectMethod(e, gEngine, gERender,
	    (jlong) (intptr_t) font, b);
    uint8_t *out = NULL;

    (*e)->DeleteLocalRef(e, b);
    Check(e);
    if (r != NULL) {
	jsize len = (*e)->GetArrayLength(e, r);

	if (len > 12) {
	    jbyte hdr[12];
	    int w, h;

	    (*e)->GetByteArrayRegion(e, r, 0, 12, hdr);
	    w = (hdr[0] & 0xFF) | (hdr[1] & 0xFF) << 8 | (hdr[2] & 0xFF) << 16 | (hdr[3] & 0xFF) << 24;
	    h = (hdr[4] & 0xFF) | (hdr[5] & 0xFF) << 8 | (hdr[6] & 0xFF) << 16 | (hdr[7] & 0xFF) << 24;
	    *base = (hdr[8] & 0xFF) | (hdr[9] & 0xFF) << 8 | (hdr[10] & 0xFF) << 16 | (hdr[11] & 0xFF) << 24;
	    if (w > 0 && h > 0 && (jsize) (12 + (size_t) w * (size_t) h) <= len) {
		out = (uint8_t *) malloc((size_t) w * (size_t) h);
		(*e)->GetByteArrayRegion(e, r, 12, w * h, (jbyte *) out);
		*ow = w;
		*oh = h;
	    }
	}
	(*e)->DeleteLocalRef(e, r);
    }
    return out;
}

static void
EFamilies(void (*add)(void *, const char *), void *ctx)
{
    JNIEnv *e = Env();
    jobjectArray arr = (jobjectArray) (*e)->CallStaticObjectMethod(e, gEngine, gEFamilies);
    jsize i, n;

    Check(e);
    if (arr == NULL) {
	return;
    }
    n = (*e)->GetArrayLength(e, arr);
    for (i = 0; i < n; i++) {
	jstring js = (jstring) (*e)->GetObjectArrayElement(e, arr, i);
	const char *c = (*e)->GetStringUTFChars(e, js, NULL);

	add(ctx, c);
	(*e)->ReleaseStringUTFChars(e, js, c);
	(*e)->DeleteLocalRef(e, js);
    }
    (*e)->DeleteLocalRef(e, arr);
}

static const TkATextEngine jniEngine = {EOpen, EClose, EMeasure, ERender, EFamilies};

/* ---- the Tcl thread ---------------------------------------------------------------------- */

/* After Tcl_Init and before the script: configure the platform layer and load Tk. */
static int
OnInit(Tcl_Interp *interp, void *ctx)
{
    Job *job = (Job *) ctx;
    char cmd[1200];

    TkAndroidSetScreen(job->width, job->height, (double) job->dpi);
    TkAndroidSetTextEngine(&jniEngine);
    TkAndroidSetHost(&host);
    snprintf(cmd, sizeof(cmd), "load {%s/libtk9.0.so} Tk", job->libDir);
    return Tcl_EvalEx(interp, cmd, TCL_INDEX_NONE, TCL_EVAL_GLOBAL);
}

static void
FreeJob(JNIEnv *env, Job *job)
{
    (*env)->DeleteGlobalRef(env, job->listener);
    free(job->libDir);
    free(job->filesDir);
    free(job->cacheDir);
    free(job->tz);
    free(job->script);
    free(job);
}

static void *
TclThread(void *arg)
{
    Job *job = (Job *) arg;
    JNIEnv *env = NULL;
    TclDroidConfig cfg;
    char *result = NULL;
    int resultLen = 0, ok;
    jbyteArray a;

    (*gVm)->AttachCurrentThread(gVm, ATTACH_ENV(&env), NULL);
    job->env = env;

    cfg.libDir = job->libDir;
    cfg.filesDir = job->filesDir;
    cfg.cacheDir = job->cacheDir;
    cfg.tz = job->tz;
    cfg.script = job->script;
    cfg.onOutput = OnOutput;
    cfg.onInit = OnInit;
    cfg.ctx = job;
    ok = tcldroid_run(&cfg, &result, &resultLen);

    a = ToByteArray(env, result, resultLen);
    (*env)->CallVoidMethod(env, job->listener, gOnDone, ok ? JNI_TRUE : JNI_FALSE, a);
    Check(env);
    if (a != NULL) {
	(*env)->DeleteLocalRef(env, a);
    }
    free(result);
    FreeJob(env, job);
    (*gVm)->DetachCurrentThread(gVm);
    return NULL;
}

/* ---- JNI entry points ---------------------------------------------------------------------- */

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{
    (void) reserved;
    gVm = vm;
    return JNI_VERSION_1_6;
}

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
	jbyteArray libDir, jbyteArray filesDir, jbyteArray cacheDir, jbyteArray tz,
	jbyteArray script, jobject listener, jint width, jint height, jint dpi)
{
    Job *job;
    jclass lc, ec;
    pthread_t th;
    pthread_attr_t attr;
    int rc;

    (void) clazz;
    lc = (*env)->GetObjectClass(env, listener);
    gOnOutput = (*env)->GetMethodID(env, lc, "onOutput", "([B)V");
    gOnDone = (*env)->GetMethodID(env, lc, "onDone", "(Z[B)V");
    gAllocFrame = (*env)->GetMethodID(env, lc, "allocFrame", "(II)[I");
    gOnFrame = (*env)->GetMethodID(env, lc, "onFrame", "(IIII)V");
    gOnTitle = (*env)->GetMethodID(env, lc, "onTitle", "([B)V");
    gOnBack = (*env)->GetMethodID(env, lc, "onBack", "()V");
    gOnMoveToBack = (*env)->GetMethodID(env, lc, "onMoveToBack", "()V");
    gOnCaret = (*env)->GetMethodID(env, lc, "onCaret", "(III)V");
    ec = (*env)->FindClass(env, "dev/tcldroid/host/TextEngine");
    if (ec == NULL) {
	return JNI_FALSE;	/* NoClassDefFoundError is pending */
    }
    gEngine = (jclass) (*env)->NewGlobalRef(env, ec);
    gEOpen = (*env)->GetStaticMethodID(env, ec, "open", "(Ljava/lang/String;IZZ[I)J");
    gEMeasure = (*env)->GetStaticMethodID(env, ec, "measure", "(J[B)I");
    gERender = (*env)->GetStaticMethodID(env, ec, "render", "(J[B)[B");
    gEFamilies = (*env)->GetStaticMethodID(env, ec, "families", "()[Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
	return JNI_FALSE;	/* NoSuchMethodError is pending */
    }
    gListener = (*env)->NewGlobalRef(env, listener);

    job = calloc(1, sizeof(Job));
    if (job == NULL) {
	return JNI_FALSE;
    }
    job->libDir = DupBytes(env, libDir);
    job->filesDir = DupBytes(env, filesDir);
    job->cacheDir = DupBytes(env, cacheDir);
    job->tz = DupBytes(env, tz);
    job->script = DupBytes(env, script);
    job->listener = (*env)->NewGlobalRef(env, listener);
    job->width = width;
    job->height = height;
    job->dpi = dpi;

    /* Tcl recurses deeply; the 1 MB default thread stack is too small. */
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8 * 1024 * 1024);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    rc = pthread_create(&th, &attr, TclThread, job);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
	LOGE("pthread_create failed: %d", rc);
	FreeJob(env, job);
	return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_dev_tcldroid_host_TclRuntime_nativeTouch(JNIEnv *env, jclass clazz, jint action,
	jint x, jint y)
{
    (void) env; (void) clazz;
    TkAQueueTouch(action, x, y);
}

JNIEXPORT void JNICALL
Java_dev_tcldroid_host_TclRuntime_nativeKey(JNIEnv *env, jclass clazz, jboolean down,
	jint keysym, jint unicode, jint state)
{
    (void) env; (void) clazz;
    TkAQueueKey(down ? 1 : 0, keysym, unicode, state);
}

JNIEXPORT void JNICALL
Java_dev_tcldroid_host_TclRuntime_nativeBack(JNIEnv *env, jclass clazz)
{
    (void) env; (void) clazz;
    TkAQueueBack();
}

JNIEXPORT void JNICALL
Java_dev_tcldroid_host_TclRuntime_nativeResize(JNIEnv *env, jclass clazz, jint w, jint h,
	jint dpi)
{
    (void) env; (void) clazz;
    TkAQueueResize(w, h, dpi);
}
