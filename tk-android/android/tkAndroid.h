/*
 * tkAndroid.h --
 *
 *	Public interface of the Tk Android platform layer, for the host (the
 *	Android JNI glue, or a test driver). Deliberately independent of Tk's
 *	internal headers so a host can include it without them.
 *
 *	Threading: everything here runs on the Tcl thread, except the functions
 *	marked thread-safe (the TkAQueue* family), which the UI thread calls.
 */
#ifndef TKANDROID_H
#define TKANDROID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- host callbacks ---------------------------------------------------------------- */

/*
 * The host (the Android Activity, or a test driver) receives frames and
 * platform requests through these. All are called on the Tcl thread.
 */
typedef void (*TkAPresentProc)(const uint32_t *pix, int w, int h, int x1, int y1,
	int x2, int y2);

typedef struct TkAHost {
    TkAPresentProc present;		/* screen changed: pixels 0xAARRGGBB */
    void (*setTitle)(const char *utf8);
    void (*setFullscreen)(int on);
    void (*backUnhandled)(void);	/* Back pressed, no WM_DELETE_WINDOW handler */
    void (*moveToBack)(void);		/* `wm iconify .` */
    void (*caret)(int x, int y, int height);	/* text caret moved (IME cursor position) */
    void (*keyboard)(int show);		/* show or hide the soft keyboard */
    /* System clipboard (text only). getClipboard returns malloc'd UTF-8, or NULL if
     * it holds no text; setClipboard(NULL) empties it. Either may be NULL: Tk then keeps
     * a private in-process clipboard (headless tests). */
    char *(*getClipboard)(void);
    void (*setClipboard)(const char *utf8);
    /* Show the native text-editing toolbar (Cut/Copy/Paste/Select all) near (x, y) in
     * screen pixels. flags: TKA_MENU_* bits. */
    void (*textMenu)(int x, int y, int flags);
    /* Device feedback. All optional (NULL = silently nothing). */
    void (*vibrate)(int ms, int amplitude);	/* amplitude 1..255, 0 = device default */
    void (*haptic)(int kind);			/* TKA_HAPTIC_*: the system's own haptic patterns */
    void (*soundPreload)(const char *path);	/* decode a WAV/OGG/MP3 file ahead of time */
    void (*soundPlay)(const char *path, int volume1000, int rate1000);
    void (*soundStop)(void);
    void (*bell)(void);				/* Tk's `bell` */
    /* The script finished handling a TKA_LIFE_* event (see TkAQueueLifecycle). */
    void (*lifecycleDone)(int kind);
    /* Device and app facts as "key<TAB>value<LF>" lines, malloc'd (::tkdroid::device). */
    char *(*systemInfo)(void);
    /* Asynchronous platform service (file picker, share, permissions, ...). The host starts the
     * work and, when it finishes (any thread), calls TkAQueueResult with the same id. kind and
     * args are NUL-terminated UTF-8; args is "key<TAB>value<LF>" lines. */
    void (*request)(int id, const char *kind, const char *args);
    /* Is the Android permission granted? 1 yes, 0 no. */
    int (*checkPermission)(const char *permission);
} TkAHost;

enum { TKA_HAPTIC_TICK, TKA_HAPTIC_CLICK, TKA_HAPTIC_HEAVY, TKA_HAPTIC_CONFIRM, TKA_HAPTIC_REJECT };
enum { TKA_LIFE_PAUSE, TKA_LIFE_RESUME, TKA_LIFE_LOWMEMORY };

#define TKA_MENU_CUT 1
#define TKA_MENU_COPY 2
#define TKA_MENU_PASTE 4
#define TKA_MENU_SELECTALL 8

extern TkAHost tkAHost;
void TkAndroidSetHost(const TkAHost *host);
void TkAndroidSetScreen(int width, int height, double dpi);
void TkAndroidResize(int width, int height, double dpi);	/* Tcl thread */

/* ---- text engine ------------------------------------------------------- */

typedef struct TkAFontInfo {
    int ascent, descent, maxWidth, fixed;
} TkAFontInfo;

typedef struct TkATextEngine {
    /* Open a font; family may be NULL for the default. Returns NULL on failure. */
    void *(*open)(const char *family, int pixelSize, int bold, int italic,
	    TkAFontInfo *info);
    void (*close)(void *font);
    /* Advance width in pixels of a UTF-8 string. */
    int (*measure)(void *font, const char *utf8, int nbytes);
    /* Render a UTF-8 string. Returns a malloc'd alpha mask (*w x *h bytes) whose
     * baseline is *baseline rows from the top; NULL if nothing to draw. */
    uint8_t *(*render)(void *font, const char *utf8, int nbytes, int *w, int *h,
	    int *baseline);
    /* Report available family names. */
    void (*families)(void (*add)(void *ctx, const char *name), void *ctx);
} TkATextEngine;

extern const TkATextEngine *tkAEngine;
void TkAndroidSetTextEngine(const TkATextEngine *engine);
const TkATextEngine *TkAGetBuiltinEngine(void);	/* NULL if none compiled in */

/* ---- thread-safe input from the UI thread -------------------------------------------- */

/* action: 0 down, 1 up, 2 move, 3 cancel (android.view.MotionEvent). */
void TkAQueueTouch(int action, int x, int y);
/* A character key (unicode != 0) or a special key given as an X keysym. */
void TkAQueueKey(int down, int keysym, int unicode, int state);
void TkAQueueBack(void);
void TkAQueueResize(int w, int h, int dpi);
/* The system clipboard changed (in any app, including ours). */
void TkAQueueClipboardChanged(void);
/* The finger has stayed down in place (x, y) long enough: a long press. */
void TkAQueueLongPress(int x, int y);
/* Two-finger pinch. phase: 0 start, 1 update, 2 end; (x, y) the focal point; scale1000 the
 * cumulative scale since the start, times 1000. */
void TkAQueuePinch(int phase, int x, int y, int scale1000);
/* The Activity paused / resumed / is short of memory (TKA_LIFE_*): Tk generates
 * <<AndroidPause>> + <<AndroidSaveState>>, <<AndroidResume>>, <<AndroidLowMemory>> on ".". */
void TkAQueueLifecycle(int kind);
/* Answer to a host->request: status is "ok", "error", "cancelled" or "denied"; payload is text
 * (newline-separated items). Thread-safe; strings are copied. */
void TkAQueueResult(int id, const char *status, const char *payload);
/* A choice from the text toolbar: TKA_MENU_CUT, _COPY, _PASTE or _SELECTALL. */
void TkAQueueTextAction(int action);

#ifdef __cplusplus
}
#endif

#endif /* TKANDROID_H */
