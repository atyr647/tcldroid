# ADR 0002 — Native Tk backend architecture

- Status: **Accepted** 2026-11-03 (owner delegated the judgement calls), including the reading of "no X11 emulation" below and the dedicated-Tcl-thread design.
- Date: 2026-11-03

## Audit findings (Tk 9.0.4)

Platform sizes (lines of C/ObjC): `unix/` 28k, `win/` 44k, `macosx/` 40k, shared `xlib/` 1.7k.

1. **Windows and macOS do not run X11.** They implement Tk's *internal Xlib-shaped API* (`XDrawLine`, `XFillRectangle`, `XCreateGC`, `XCopyArea`, `Drawable`, `GC`, `XEvent`…) natively on top of GDI / CoreGraphics. The shared `xlib/` directory holds small generic helpers (`xcolors.c`, `xdraw.c`, `xgc.c`, `ximage.c`, `xutil.c`) that those ports reuse.
2. The widget code in `generic/` talks only to that internal API plus a small set of platform hooks (`TkpXxx` functions: fonts, cursors, menus, regions, busy windows, window manager, etc.).
3. Tcl already has an `ANDROID` branch in zipfs and an epoll notifier. Whether the rest of Tcl needs Android-specific changes is unverified until the M0 cross-build and M1 tests (see docs/status.md).

## Decision

**Add `android/` to Tk, modelled on `win/` and `macosx/`:** implement the platform hooks and the Xlib-shaped drawing subset directly on Android platform APIs. Reuse all of `generic/`, and reuse `xlib/` helpers only where they are pure logic (colours, GC bookkeeping), never a software pretend-X-server.

### ⚠ Interpretation of the "no X11 emulation" principle

The roadmap says *"do not emulate X11, Linux, or desktop operating systems"* and *"no SDL/X11 compatibility shim"*. This ADR reads that as: **no X server, no X protocol, no libX11/XCB, no SDL window layer.** It does **not** forbid implementing Tk's own internal API vocabulary (`Drawable`, `GC`, `XEvent` type names), because that vocabulary *is* Tk's portability contract and is exactly how the Windows and macOS ports work. Rewriting every widget to a new API would violate the other principle (preserve upstream widget implementations) and make upstreaming impossible. Please confirm this reading.

### Threading and event loop

Run the Tcl interpreter on **its own dedicated thread** with a normal blocking `Tcl_DoOneEvent` loop (upstream epoll notifier, unmodified). The Android UI thread forwards touch/key/lifecycle events with `Tcl_ThreadQueueEvent` + `Tcl_ThreadAlert`, which is upstream's supported cross-thread mechanism.

- No notifier surgery, and Tcl timers/`after`/sockets keep their normal semantics.
- Android's main thread is never blocked by script code (no ANRs from a long `update`).
- Costs: UI operations that must be synchronous (IME, clipboard, dialogs, insets) do a blocking JNI hand-off to the UI thread; drawing must be allowed from a non-UI thread (a `SurfaceView` surface permits this).

This deliberately replaces the roadmap's "connect Android Looper to the Tcl notifier" wording with a simpler design. The Looper-on-UI-thread alternative stays documented as a fallback if the hand-off latency proves unacceptable.

### Windows model

Follow the macOS model: one native surface per Tk *toplevel*, with Tk's child windows drawn by Tk inside it. See `docs/compatibility-policy.md` for how toplevels, dialogs, menus and `wm` map.

### Rendering and text (the main technical risk)

Principle: use Android's real graphics/text APIs. Two candidates, to be decided by a **Phase 2 spike with measurements** (text-heavy `text` widget scrolling, `canvas` with 5k items, resize):

| | A. JNI to `android.graphics` (`Canvas`/`Paint`) | B. Native: `ANativeWindow` + bundled FreeType/HarfBuzz |
| --- | --- | --- |
| Text shaping/fallback/IME-consistent fonts | Platform-correct, free | Must bundle fonts/shaping; diverges from system |
| Draw-call cost | JNI per call unless batched | None |
| Accessibility/i18n fidelity | High | Lower |
| Dependencies | None | FreeType, HarfBuzz (licences to audit) |

Default plan is **A with batching** (record draw ops in native code, replay in one JNI call per frame) because it best satisfies "use Android's actual platform APIs for text". B is the fallback if A cannot meet latency targets.

### Package layout and build

- `tk-android/` holds the new `android/` tree (C, with Java kept in `android-host/`); `upstream/patches/tk.series` lists the small changes needed to `generic/`/`configure` so the diff stays reviewable for upstream.
- `android-host/` holds the Activity, `SurfaceView`, `InputConnection`, JNI glue and Gradle project.
- Headless Tcl (`runtime/`) must build and test without any Tk or Java code.

## Decisions from the first compile survey (2026-11-03)

Compiling Tk's platform-independent code for Android (`tools/survey-tk-android.sh`; results in [tk-android-work-list.md](../tk-android-work-list.md)) forced three concrete choices. All 119 files in the compile set build with zero failures; **195 platform functions** remain to be implemented, in 12 families.

1. **Header selection.** Generic code picks its platform headers with ~10 irregular `#ifdef _WIN32 / MAC_OSX_TK / else` patterns across 11 files. Rather than patch all of them, Android is treated as Unix-family: `tk-android/android/tkUnixInt.h` and `tkUnixDefault.h` *shadow* the Unix headers of the same name through include-path order (`-I android` before `-I unix`), and the only upstream patches are two one-hunk changes (`upstream/patches/0001`, `0002`: `tkPort.h` and `tkPointer.c`). Cost: the names are slightly misleading; the upstream-quality form (renaming to `tkAndroidInt.h` and patching the 11 sites) can be done at submission time.
2. **Stub tables (PROVISIONAL).** Tk's own Xlib-shaped declarations (`tkIntXlibDecls.h`, ~290 functions) exist only in the Windows and macOS sections of `generic/tk.decls`. Android selects the macOS section by defining `MAC_OSX_TCL` at the top of `tkPort.h` (before `tk.h` pulls in Xlib). This is a prototype shortcut; the correct form is a dedicated ANDROID section in `tk.decls`, which also fixes stub-slot numbering for extensions. **Must be resolved before any upstream submission.**
3. **Reuse Unix widget drawing, not native controls.** The macOS port uses native Cocoa controls for buttons, scrollbars and menus. Unix Tk draws them itself with the Xlib-shaped API, which is what Android's backend will implement. `tkUnixButton`, `tkUnixMenubu`, `tkUnixScrlbr`, `tkUnixMenu` (plus `tkUnix3d`, `tkUnixScale`) compile against the Android port unchanged and remove 31 functions from the work list. This keeps upstream widget code and appearance (a native-looking Android theme remains a ttk-level option, Phase 3). Other Unix files (`tkUnixWm`, `tkUnixFont`, `tkUnixKey`, ...) depend on X11 behaviour and are *not* reused.

Also confirmed by the survey: the Windows/macOS-style in-process pointer/grab code (`generic/tkPointer.c`) is required on Android (no X server), and `tkUnixWm.c`-style X window-manager logic is not.

## Consequences

- Largest single work item is the Xlib-shaped drawing subset + font backend; the roadmap's Phase 2 sizing should be read as "comparable to a new `win/`-sized port, minus what `generic/` and `xlib/` already give us".
- The two non-obvious Tk areas with no Android equivalent — multiple top-level OS windows and application-wide menus/`send` — are handled by policy, not emulation (see compatibility policy).
