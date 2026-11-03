/*
 * tkAndroidPort.h --
 *
 *	Platform header for the Android port of Tk, modelled on
 *	macosx/tkMacOSXPort.h: a Unix-like system that takes Tk's *own* bundled
 *	Xlib-shaped types from tk/xlib/X11 (no system X11, no X server, no X
 *	protocol). See docs/adr/0002-tk-backend-architecture.md.
 *
 *	Decisions marked PROVISIONAL are to be settled by the Phase 2 rendering
 *	spike and recorded in the ADR.
 */

#ifndef _TKANDROIDPORT
#define _TKANDROIDPORT

/*
 * NOTE: the macro MAC_OSX_TCL is defined at the top of generic/tkPort.h (patch
 * 0001), because tk.h pulls in the Xlib declarations before this header is read.
 * See that patch and ADR 0002 ("Stub tables").
 */

#include <stdio.h>
#include <pwd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/stat.h>
#ifndef _TCL
#   include <tcl.h>
#endif
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>
#if defined(__GNUC__) && !defined(__cplusplus)
#   pragma GCC diagnostic ignored "-Wc++-compat"
#endif
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xfuncproto.h>
#include <X11/Xutil.h>

#define SELECT_MASK fd_set


#ifndef MODULE_SCOPE
#   ifdef __cplusplus
#	define MODULE_SCOPE extern "C"
#   else
#	define MODULE_SCOPE extern
#   endif
#endif

#ifndef NBBY
#   define NBBY 8
#endif

/* Use Tk's internal keysym hash table instead of a system XStringToKeysym. */
#define REDO_KEYSYM_LOOKUP

/*
 * Not meaningful on Android (no colormaps). TkpCmapStressed() and TkpSync() are
 * real functions in the X11 section of tkIntPlatDecls.h, so they are provided by
 * the platform layer (trivially), not as macros.
 */
#define TkpFreeColor(tkColPtr)
#define TkSetPixmapColormap(p,c) {}

/* Needed by the reused Unix button code (as in tkUnixPort.h): no native controls. */
#define TkpButtonSetDefaults() {}
#define TkpDestroyButton(butPtr) {}

#define TkpPrintWindowId(buf,w) \
	snprintf((buf), TCL_INTEGER_SPACE, "0x%lx", (unsigned long) (w))

/*
 * Pixel format: Tk pixel values are 0x00RRGGBB (TrueColor, red_mask 0x00FF0000).
 * The screen buffer holds 0xFFRRGGBB, which Java's Bitmap.setPixels() takes as is.
 */
#define TkpGetPixel(p) \
	((((unsigned long) ((p)->red >> 8) & 0xff) << 16) \
	| (((unsigned long) ((p)->green >> 8) & 0xff) << 8) \
	| ((unsigned long) ((p)->blue >> 8) & 0xff))

/*
 * PROVISIONAL: Tk's own double buffering is left ON (TK_NO_DOUBLE_BUFFERING is
 * not defined) until the spike shows whether the backing bitmap makes it
 * redundant, as it is on macOS.
 */

#include "tkIntXlibDecls.h"  /* IWYU pragma: export */

/* Entry point hooks used by the application init code. */
#define USE_CUSTOM_EXIT_PROC
EXTERN int TkpWantsExitProc(void);
EXTERN TCL_NORETURN void TkpExitProc(void *);

#endif /* _TKANDROIDPORT */
