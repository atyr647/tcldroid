# Tk for Android: the `android/` platform directory

A native Android backend for Tk, modelled on `win/` and `macosx/`. It implements Tk's own Xlib-shaped
API (`Drawable`, `GC`, `XEvent`, `XDrawLine`, ...) in-process, with no X server, no X protocol and no
X11 library, and lets all of `generic/` run unchanged. This is the same relationship the Windows and macOS
ports have to Xlib. Widgets keep their upstream drawing code (the Unix button/scrollbar/menu files are reused
as they are).

## What is here

| File | Role |
| --- | --- |
| `tkAndroid.h` | The host interface: callbacks (present a frame, clipboard, keyboard, sounds, ...), the text-engine interface, thread-safe input queues. Contains no Tk internals, so a host (an Activity, or a test driver) can include it alone. |
| `tkAndroidPort.h`, `tkAndroidInt.h`, `tkAndroidDefault.h`, `tkAndroidPriv.h` | Platform headers (public port header, internal declarations, default options, port-private types). |
| `tkAndroidWindow.c` | The in-process "X server": window tree, stacking, mapping, exposure, pixmaps, clipping. |
| `tkAndroidDraw.c` | Software rasteriser behind the Xlib drawing calls; images; text blits with a rendered-text cache. |
| `tkAndroidRegion.c` | Regions as rectangle lists. |
| `tkAndroidEvent.c` | Tcl event source, touch/key/back queues, focus, pointer, key symbols, start-up frame hold. |
| `tkAndroidWm.c` | Window manager policy for a single-surface device: `.` fills the screen, other toplevels are layers inside it, Back maps to `WM_DELETE_WINDOW`, all `wm` subcommands accepted. |
| `tkAndroidFont.c`, `tkAndroidTestEngine.c` | Fonts on a pluggable text engine (on a device: `android.graphics.Paint` through the host; for desktop tests: FreeType). |
| `tkAndroidColor.c`, `tkAndroidDisplay.c` | Colours, cursors (validated, never shown), display and initialisation, library mounting from the shared object (zipfs). |
| `tkAndroidSelect.c` | Selection and clipboard: PRIMARY inside Tk, CLIPBOARD on the system clipboard through the host. |
| `tkAndroidSystem.c` | Feedback and services hooks: vibration, haptics, sound, `bell`, device facts, asynchronous requests (file picker, share, permissions), input mode, accessibility hand-off. |
| `../library/android.tcl`, `../library/a11y.tcl` | Touch policy (scrolling, long-press selection, gestures) and the accessibility description of the widget tree, in Tcl. |

## Building and testing

Tk's own build system is not used by the Android port yet; the port's repository (tcldroid) builds the
sources listed in `unix/Makefile.in` plus `android/*.c` with the NDK, and builds the same code with a
desktop compiler (`-DTK_ANDROID_PORT`) to run Tk's own test suite (9,742 tests; 86.7% pass, every
remaining failure triaged; 0 AddressSanitizer errors). Differences from upstream behaviour that the
suite shows are listed with reasons in that repository (`tests/tk/expected-failures.txt`).

## Known differences from desktop Tk

`tk windowingsystem` returns `x11` (existing scripts select styles with it; this port reuses Unix widget
drawing). Multiple top-level windows are layers in one surface; `wm geometry` is recorded, not applied, for
`.`. No `send`, no embedding (`-container`/`-use`), no X selection protocol between processes.
Shapes are not anti-aliased (classic Tk look); text is shaped and anti-aliased by the platform text engine.

See `docs/` in the tcldroid repository for the architecture decisions and the compatibility policy.
