# Tk's own test suite on the Android backend

The strongest evidence that a Tk port is faithful is that **upstream's own tests pass**. This port runs
all 97 test files of Tk 9.0.4 (`tests/*.test`, ~9,700 tests) on the host build of the same C code, one
file per process:

```
tests/tk-suite.sh [file.test ...]        # JOBS=6, OUTDIR=..., ASAN=1 for AddressSanitizer
```

It builds a *test* variant of libtk (Tk's `tkTest.c`/`tkSquare.c` test commands, started with
`::tkdroid::tktestinit`), runs each file with a virtual 1024x768 display, writes the full output of
each file to `build/tksuite/<file>.out` and a summary to `build/tksuite/summary.tsv`, then compares the
failures against [`tests/tk/expected-failures.txt`](../tests/tk/expected-failures.txt). The run fails
only for a failure that is **not** on that list.

## Result (Tk 9.0.4, host build, this repository)

| | |
| --- | --- |
| test files | 97 |
| tests run | 9,742 |
| passed | 8,445 (86.7%) |
| skipped (constraints) | 963 |
| failed | 334, **all triaged** (see below) |
| crashes | 0 |
| AddressSanitizer errors | **0** across the whole suite (`ASAN=1`) |
| timeouts | 1 (`wm.test`: X11 window-manager semantics) |

Of the 334 accepted failures, about 230 are tests of X11 behaviour that has no Android equivalent by
design (the `unix*` test files, embedding, the X selection protocol with a second Tk process, X window
manager properties; category `na-x11`), a few depend on the harness or build (`na-env`), ~40 are known
differences (`deviation`) and 14 are not yet understood (`untriaged`: the list to burn down).

## How to read it, and what the harness changes

- **Desktop window manager mode** (`TKDROID_DESKTOP_WM=1`): on a device `.` always fills the screen
  (compatibility policy). The tests assume an ordinary toplevel that gets its requested size, so the
  harness turns that policy off. Everything else is the code that ships.
- **Touch policy off** (`::tkdroid::touchScroll 0`): the tests drive widgets with synthetic mouse events;
  the Android touch layer (drag scrolls, tap acts on release) is tested separately
  (`tests/tk/gestures.tcl`).
- `testmenubar`, `testsend`, `testwrapper` (X11-only test commands) are not built.
- The text engine on the host is FreeType with DejaVu fonts, not Android's `Paint`; a few layout tests
  depend on exact font metrics (`deviation`).

## What running it found and fixed (all in this repository)

| Found by | Bug |
| --- | --- |
| tkwait visibility (3,000 tests could not run) | no `VisibilityNotify` events |
| bind.test | null-interpreter crash in `TkpScanWindowId` |
| busy.test | `tk busy` hooks were stubs (crash and non-functional) |
| window.test + ASan | use of a destroyed window's record: Tk skips `XDestroyWindow` for the children of a destroyed window on platforms other than Windows/macOS, leaving dead windows in the window server and the pointer module (now patch 0004, plus a live-drawable registry that makes stale ids harmless) |
| cursor/button/spinbox/paned tests | invalid `-cursor` values were accepted silently |
| event.test | `event generate -warp` did nothing; no Enter/Leave when the pointer's window disappears |
| tk.test | `tk caret` never reported the caret |
| clipboard.test | `selection clear -selection CLIPBOARD` left the old text on the system clipboard |
| pack.test | an iconified/unmapped toplevel ignored size requests, so geometry managers inside it froze |

## Not covered

This runs on the host, not on a device or in an Android emulator; it exercises the platform layer's
logic and Tk's widget code, not the Java host, JNI, `Paint` text, or the GPU path (see
`tests/jvm-jni.sh` for the JNI layer and the device checks in `docs/status.md`).
