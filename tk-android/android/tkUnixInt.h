/*
 * tkUnixInt.h (Android overlay) --
 *
 *	Tk's generic code selects a platform-internal header with logic like
 *	"#ifdef _WIN32 / #elif MAC_OSX_TK / #else tkUnixInt.h". Android is
 *	Unix-family (same stub-table section as X11), so generic code includes
 *	"tkUnixInt.h". With `-I android` ahead of `-I unix` that name resolves to
 *	THIS file, which keeps upstream's generic/ sources unpatched (see
 *	docs/adr/0002-tk-backend-architecture.md, "Header selection").
 *
 *	Android has no X server, so the in-process pointer/grab machinery that
 *	the Windows and macOS ports use (generic/tkPointer.c) is needed; its
 *	platform hooks are declared below.
 */
#ifndef _TKUNIXINT
#define _TKUNIXINT

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

#endif /* _TKUNIXINT */
