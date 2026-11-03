# Compatibility policy

Goal: an unmodified Tcl/Tk script should *run and behave sensibly*. Where Android has no equivalent for a desktop concept, the default is to **accept the command, record its state, and return plausible values**, not to throw an error. Errors are reserved for features that cannot be faked without lying to the script.

## Classes

Every command/option/`wm` subcommand in [compatibility-matrix.md](compatibility-matrix.md) has exactly one class:

| Class | Meaning |
| --- | --- |
| **Native** | Upstream behaviour unchanged (all `generic/` widget logic, geometry managers, bindings, options). |
| **Mapped** | Implemented with an Android equivalent; behaviour may differ in documented ways. |
| **Accepted** | Command/option accepted and its value stored and returned by queries, but it has no visible effect on Android. |
| **Unsupported** | Raises a clear Tcl error (`… is not supported on Android`). Used sparingly. |

## Product target: apps that run well on touch

Unmodified scripts must *run*; the platform makes them *usable* by default, without editing them:

- **Density scaling:** `tk scaling` is set from the display density at startup, so point-sized fonts and `-width`/`-padx` in screen units come out physically sensible. Pixel-sized layouts are not rewritten.
- **Touch targets:** no automatic resizing of widgets. An opt-in `android::touchpad`-style helper package (Phase 3) provides larger hit slop and scrolling-by-drag for `canvas`, `text`, `listbox` and `ttk::treeview`.
- **Soft keyboard:** when the IME is shown the surface is resized (`adjustResize`); the focused widget is scrolled into view by the host. Scripts see an ordinary `<Configure>`.
- **Input sources:** touch, stylus, mouse and hardware keyboard all work; hover-only UI degrades gracefully, never required.

## Things Android forbids or changes

- **Executables:** since Android 10 an app cannot execute files from its writable storage. Binary Tcl extensions load only from the APK's native-library directory, so they must be bundled at packaging time; there is no on-device binary package install. `exec` and `open |cmd` can run only system binaries that exist and are permitted (e.g. `/system/bin/sh`); do not rely on them.
- **Process death:** Android may kill the process at any time. The Tcl interpreter lives in a process-level singleton, not in the Activity, so rotation/recreation does not restart the script. Scripts that need to survive process death handle `<<AndroidSaveState>>` and restore on start; nothing else is persisted for them.
- **Multi-window/desktop mode:** supported as resizing of the single surface (per-Activity); true multi-OS-window Tk toplevels are not planned.

## Window-manager model

Android has one app surface per Activity, not a desktop of free windows.

- **`.` (the root toplevel)** fills the Activity content area, inside system insets. `wm geometry .` sizes/positions are *recorded* but the reported window size is the real surface size; resizing events (rotation, split-screen) arrive as ordinary `<Configure>` events.
- **Other toplevels, dialogs and tear-offs** are drawn as in-app **layers** stacked above `.` inside the same Activity. They are sized from their requested geometry, clamped to the surface, and positioned per `wm geometry` if it fits. `wm transient` makes a layer modal-ish relative to its master.
- **Menus / `wm overrideredirect 1` windows** are popup layers (no decoration, dismissed by outside touch).
- **`wm withdraw / deiconify / iconify`** hide/show the layer. `wm iconify .` sends the task to the back (`moveTaskToBack`).
- **`wm title .`** sets the Activity title / task description; **`wm iconphoto`** sets the task icon.
- **`wm attributes . -fullscreen`** maps to immersive mode.
- **`wm protocol . WM_DELETE_WINDOW`** is invoked by the system Back navigation; if no handler is installed Back finishes the Activity. Android lifecycle events (`pause`, `resume`, `low memory`, `save state`) are delivered as virtual events (e.g. `<<AndroidPause>>`) rather than by overloading `wm protocol`.
- **Pointer vs touch:** a single touch is delivered as `<Button-1>`/`<Motion>`/`<ButtonRelease-1>`; long-press → `<Button-3>`; two-finger scroll → `<MouseWheel>`; a real mouse/trackpad uses real buttons. Hover (`<Enter>`/`<Leave>`) only fires for hover-capable input.

## `send`, selection, clipboard, dialogs

- `send` is **Unsupported** (no cross-process Tk registry on Android).
- `clipboard`/`selection` map to the Android clipboard (`CLIPBOARD` only; `PRIMARY` is emulated in-process).
- `tk_getOpenFile`/`tk_getSaveFile`/`tk_chooseDirectory` map to the Storage Access Framework; results are `content://` URIs exposed through a Tcl channel API, since plain filesystem paths are not available for shared storage on modern Android.
- `tk_messageBox` maps to a native dialog; `tk_chooseColor` and `tk_dialog` use Tk's own implementations.

## Storage

- `//zipfs:/app` — read-only bundled application payload (mounted from the APK).
- App-private writable directories: `HOME` is set to `filesDir` and `TMPDIR` to `cacheDir` before Tcl starts, so `file home` and `file tempdir`-style code work unmodified. (Tcl 9 no longer expands a leading `~` implicitly; scripts use `file home`.) A richer `android` Tcl package comes in Phase 3.
- Shared storage and permissions are explicit opt-in APIs, never implicit.

## Extensions

Pure-Tcl packages should work unchanged. Binary extensions must be rebuilt for each supported ABI against Tcl/Tk 9 stubs; loading is via `load` from the APK's native library directory.

## Revision

This is a **design policy**, not a statement of what works today. Statuses live in the matrix. Disagreements found during implementation should change this document in the same commit as the code.
