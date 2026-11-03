/*
 * tcldroid_jni.c --
 *
 *	JNI entry point: runs tcldroid_run() (tcldroid_core.c) on a dedicated
 *	native thread and forwards output / completion to a Java listener.
 *	See docs/adr/0002-tk-backend-architecture.md for why Tcl owns a thread.
 */

#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <stdlib.h>

#include "tcldroid_core.h"

#define TAG "tcldroid"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static JavaVM *gVm;
static jmethodID gOnOutput;	/* void onOutput(byte[]) */
static jmethodID gOnDone;	/* void onDone(boolean, byte[]) */

typedef struct {
    char *libDir, *filesDir, *cacheDir, *tz, *script;
    jobject listener;		/* global ref */
    JNIEnv *env;		/* valid on the Tcl thread only */
} Job;

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
OnOutput(void *ctx, const char *buf, int len)
{
    Job *job = (Job *) ctx;
    jbyteArray a = ToByteArray(job->env, buf, len);

    if (a != NULL) {
	(*job->env)->CallVoidMethod(job->env, job->listener, gOnOutput, a);
	(*job->env)->DeleteLocalRef(job->env, a);
    }
    if ((*job->env)->ExceptionCheck(job->env)) {
	(*job->env)->ExceptionClear(job->env);
    }
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

    (*gVm)->AttachCurrentThread(gVm, &env, NULL);
    job->env = env;

    cfg.libDir = job->libDir;
    cfg.filesDir = job->filesDir;
    cfg.cacheDir = job->cacheDir;
    cfg.tz = job->tz;
    cfg.script = job->script;
    cfg.onOutput = OnOutput;
    cfg.ctx = job;
    ok = tcldroid_run(&cfg, &result, &resultLen);

    a = ToByteArray(env, result, resultLen);
    (*env)->CallVoidMethod(env, job->listener, gOnDone,
	    ok ? JNI_TRUE : JNI_FALSE, a);
    if ((*env)->ExceptionCheck(env)) {
	(*env)->ExceptionClear(env);
    }
    if (a != NULL) {
	(*env)->DeleteLocalRef(env, a);
    }
    free(result);
    FreeJob(env, job);
    (*gVm)->DetachCurrentThread(gVm);
    return NULL;
}

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
    Job *job;
    jclass lc;
    pthread_t th;
    pthread_attr_t attr;
    int rc;

    (void) clazz;
    lc = (*env)->GetObjectClass(env, listener);
    gOnOutput = (*env)->GetMethodID(env, lc, "onOutput", "([B)V");
    gOnDone = (*env)->GetMethodID(env, lc, "onDone", "(Z[B)V");
    if (gOnOutput == NULL || gOnDone == NULL) {
	return JNI_FALSE;	/* NoSuchMethodError is pending */
    }

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
