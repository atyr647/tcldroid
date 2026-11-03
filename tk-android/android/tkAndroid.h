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
} TkAHost;

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

#ifdef __cplusplus
}
#endif

#endif /* TKANDROID_H */
