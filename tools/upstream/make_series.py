#!/usr/bin/env python3
"""Build the reviewable upstream patch series for Tk from the sources in this repository.

   tools/upstream/make_series.py WORKDIR TK_TARBALL OUTDIR

Unpacks pristine Tk, then makes one git commit per concern (so `git format-patch` gives a series a
Tk maintainer can review step by step), and writes the patches to OUTDIR:

   small generic-code patches (these are also what tools/fetch-upstream.sh applies for development)
   + one patch that adds the android/ directory and the Android scripts in library/.
"""
import os, shutil, subprocess, sys, tarfile, re

work, tarball, out = sys.argv[1:4]
root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if os.path.exists(work):
    shutil.rmtree(work)
os.makedirs(work)
with tarfile.open(tarball) as t:
    t.extractall(work, filter="data")
tk = os.path.join(work, [d for d in os.listdir(work) if d.startswith("tk")][0])

def git(*a, **kw):
    return subprocess.run(["git", "-C", tk, "-c", "safe.directory=*", "-c", "user.name=tcldroid", "-c", "user.email=tcldroid@example.invalid", *a],
                          check=True, capture_output=True, text=True, **kw)

def edit(path, old, new, count=1):
    p = os.path.join(tk, path)
    s = open(p, encoding="utf-8").read()
    assert old in s, "pattern not found in %s: %r" % (path, old[:60])
    open(p, "w", encoding="utf-8").write(s.replace(old, new, count))

def commit(msg):
    git("add", "-A")
    git("commit", "-q", "-m", msg)

git("init", "-q")
commit("Tk 9.0.4 (pristine)")

# ---- 1. tkPort.h ---------------------------------------------------------------------------------
edit("generic/tkPort.h", """#ifndef _TKPORT
#define _TKPORT
""", """#ifndef _TKPORT
#define _TKPORT

/*
 * Android. TK_ANDROID selects the Android platform headers wherever generic code chooses between
 * tkWinInt.h, tkMacOSXInt.h and tkUnixInt.h. (TK_ANDROID_PORT lets the platform layer be built and
 * tested on a desktop compiler.)
 *
 * Android has no X server. Like Windows and macOS it implements Tk's own copy of the Xlib API
 * in-process, so it takes the same section of the generated stub declarations
 * (tkIntXlibDecls.h, keyed on MAC_OSX_TCL for historical reasons). The macro must be defined here,
 * before tk.h pulls those declarations in.
 */
#if defined(__ANDROID__) || defined(TK_ANDROID_PORT)
#   define TK_ANDROID 1
#   ifndef MAC_OSX_TCL
#	define MAC_OSX_TCL 1
#   endif
#endif
""")
edit("generic/tkPort.h", """#   if defined(MAC_OSX_TK)
#	include "tkMacOSXPort.h"
#   else""", """#   if defined(MAC_OSX_TK)
#	include "tkMacOSXPort.h"
#   elif defined(TK_ANDROID)
#	include "tkAndroidPort.h"
#   else""")
commit("""tkPort.h: add TK_ANDROID and select the Android platform header

Android is the third platform, after Windows and macOS, that implements Tk's Xlib API in-process
instead of talking to an X server. TK_ANDROID is defined for __ANDROID__ builds (and, for desktop
testing, TK_ANDROID_PORT); it takes the same stub-declaration section as macOS and includes
tkAndroidPort.h.""")

# ---- 2. platform header selection -------------------------------------------------------------------
edit("generic/tkEvent.c", """#elif defined(MAC_OSX_TK)
#include "tkMacOSXInt.h"
#else
#include "tkUnixInt.h"
#endif""", """#elif defined(MAC_OSX_TK)
#include "tkMacOSXInt.h"
#elif defined(TK_ANDROID)
#include "tkAndroidInt.h"
#else
#include "tkUnixInt.h"
#endif""")
edit("generic/tkCmds.c", """#elif defined(MAC_OSX_TK)
#include "tkMacOSXInt.h"
#else
#include "tkUnixInt.h"
#endif""", """#elif defined(MAC_OSX_TK)
#include "tkMacOSXInt.h"
#elif defined(TK_ANDROID)
#include "tkAndroidInt.h"
#else
#include "tkUnixInt.h"
#endif""")
edit("generic/tkBind.c", """#elif defined(MAC_OSX_TK)
#include "tkMacOSXInt.h"
#else /* if defined(__unix__) */
#include "tkUnixInt.h"
#endif""", """#elif defined(MAC_OSX_TK)
#include "tkMacOSXInt.h"
#elif defined(TK_ANDROID)
#include "tkAndroidInt.h"
#else /* if defined(__unix__) */
#include "tkUnixInt.h"
#endif""")
edit("generic/tkPointer.c", """#if defined(MAC_OSX_TK)
#include "tkMacOSXInt.h"
#endif
""", """#if defined(MAC_OSX_TK)
#include "tkMacOSXInt.h"
#elif defined(TK_ANDROID)
#include "tkAndroidInt.h"
#endif
""")
edit("generic/default.h", """#   if defined(MAC_OSX_TK)
#	include "tkMacOSXDefault.h"
#   else""", """#   if defined(MAC_OSX_TK)
#	include "tkMacOSXDefault.h"
#   elif defined(TK_ANDROID)
#	include "tkAndroidDefault.h"
#   else""")
edit("generic/tkStubInit.c", """#define UNIX_TK
#include "tkUnixInt.h"
#endif""", """#define UNIX_TK
#ifdef TK_ANDROID
#include "tkAndroidInt.h"
#else
#include "tkUnixInt.h"
#endif
#endif""")
edit("generic/tkWindow.c", """#elif !defined(MAC_OSX_TK)
#include "tkUnixInt.h"
#endif""", """#elif defined(TK_ANDROID)
#include "tkAndroidInt.h"
#elif !defined(MAC_OSX_TK)
#include "tkUnixInt.h"
#endif""")
edit("generic/tkStubLib.c", """#if !(defined(_WIN32) || defined(MAC_OSX_TK))
#include "tkUnixInt.h"
#endif""", """#if defined(TK_ANDROID)
#include "tkAndroidInt.h"
#elif !(defined(_WIN32) || defined(MAC_OSX_TK))
#include "tkUnixInt.h"
#endif""")
edit("generic/tkGrab.c", """#elif !defined(MAC_OSX_TK)
#include "tkUnixInt.h"
#else
#include "tkMacOSXInt.h"
#endif""", """#elif defined(TK_ANDROID)
#include "tkAndroidInt.h"
#elif !defined(MAC_OSX_TK)
#include "tkUnixInt.h"
#else
#include "tkMacOSXInt.h"
#endif""")
commit("""generic: include the Android platform headers

At each place where generic code selects tkWinInt.h / tkMacOSXInt.h / tkUnixInt.h (and default.h's
platform defaults), add the Android case. tkPointer.c now includes the platform header for Android the
way it does for macOS: the in-process pointer and grab code needs TkpSetCapture and friends.""")

# ---- 3. window destruction ----------------------------------------------------------------------------
edit("generic/tkWindow.c", """#if defined(MAC_OSX_TK) || defined(_WIN32)
\tXDestroyWindow(winPtr->display, winPtr->window);""", """#if defined(MAC_OSX_TK) || defined(_WIN32) || defined(TK_ANDROID)
\t/* Android, like Windows and macOS, keeps its own window records (and the pointer
\t * module's references to them), so every window must be destroyed explicitly. */
\tXDestroyWindow(winPtr->display, winPtr->window);""")
commit("""tkWindow.c: destroy every window explicitly on Android

Tk skips XDestroyWindow for the children of a window being destroyed, relying on the X server to
destroy them with the parent. A platform that keeps its own window records, like Windows and macOS
(which are already special-cased here), must see every destroy; otherwise records for dead windows
stay reachable by the pointer and hit-testing code. Found with AddressSanitizer running window.test.""")

# ---- 4. library hook -----------------------------------------------------------------------------------
edit("library/tk.tcl", """# Run the Ttk themed widget set initialization
if {$::ttk::library ne ""} {
    uplevel \\#0 [list source $::ttk::library/ttk.tcl]
}
""", """# Run the Ttk themed widget set initialization
if {$::ttk::library ne ""} {
    uplevel \\#0 [list source $::ttk::library/ttk.tcl]
}

# Android: touch, selection and gesture policy (library/android.tcl).
if {[llength [info commands ::tkdroid::textmenu]]} {
    uplevel \\#0 [list source -encoding utf-8 [file join $::tk_library android.tcl]]
}
""")
commit("""tk.tcl: load the Android touch policy

On Android, once everything else is initialised, source library/android.tcl (touch scrolling, long-press
selection, gestures, accessibility). Does nothing elsewhere.""")

# ---- 5. the android/ directory --------------------------------------------------------------------------
# (kept out of the dev patch set: tools/fetch-upstream.sh overlays tk-android/ directly, so edits show up at once)
dev_patches = subprocess.run(["git", "-C", tk, "-c", "safe.directory=*", "format-patch", "-q", "-o", out, "HEAD~4"], check=True)
shutil.copytree(os.path.join(root, "tk-android", "android"), os.path.join(tk, "android"),
                ignore=shutil.ignore_patterns("*.o", "*.orig"))
for f in os.listdir(os.path.join(root, "tk-android", "library")):
    if f.endswith(".tcl"):
        shutil.copy(os.path.join(root, "tk-android", "library", f), os.path.join(tk, "library", f))
commit("""Add the Android platform: android/ and library/android.tcl, a11y.tcl

A native Android backend for Tk, modelled on win/ and macosx/: Tk's Xlib API implemented in-process
(window server, software rasteriser, text through a pluggable engine), Tcl event source, window
manager policy for a single-surface device, selection/clipboard, touch input and accessibility.
See android/README.md. Widget code in generic/ is unchanged.""")
git("format-patch", "-q", "-o", out, "--start-number", "5", "-1")
print("patches written to", out)
