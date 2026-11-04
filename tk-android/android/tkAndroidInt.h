/*
 * tkAndroidInt.h --
 *
 *	Platform-internal declarations for the Android port of Tk, selected by generic code with
 *	"#elif defined(TK_ANDROID) #include "tkAndroidInt.h"" next to tkWinInt.h and tkMacOSXInt.h.
 *
 *	Android has no X server, so the in-process pointer/grab machinery that the Windows and macOS
 *	ports use (generic/tkPointer.c) is needed; its platform hooks are declared below.
 */
#ifndef _TKANDROIDINT
#define _TKANDROIDINT

#ifndef _TKINT
#include "tkInt.h"
#endif

#include "tkIntPlatDecls.h"  /* IWYU pragma: export */

/* Optional Unix modules that Tk's init code may reference. */
MODULE_SCOPE  int       Tktray_Init (Tcl_Interp* interp);
MODULE_SCOPE  int       SysNotify_Init (Tcl_Interp* interp);
MODULE_SCOPE  int       Cups_Init (Tcl_Interp* interp);

/*
 * Platform hooks required by generic/tkPointer.c (in-process mouse capture,
 * as on Windows/macOS). Declared in tkIntPlatDecls.h only for those platforms.
 */
MODULE_SCOPE unsigned long TkpGetMS(void);
MODULE_SCOPE void          TkpSetCapture(TkWindow *winPtr);
MODULE_SCOPE Tk_Window     TkpGetCapture(void);
MODULE_SCOPE void          TkpSetCursor(TkpCursor cursor);

#endif /* _TKANDROIDINT */
