# android-host

A minimal Android app that hosts an **unmodified Tcl 9 core** (`libtcl9.0.so`) on its own thread. It is the foundation for the Tk host (Phase 2): Activity lifecycle, JNI bridge, packaging.

Today it runs `app/src/main/assets/selftest.tcl` at startup, shows the output, and logs a verdict line `TCLDROID_SMOKE: PASS|FAIL n/m` to logcat (tag `tcldroid`).

## Build

```sh
tools/build-android-host.sh        # from the repo root; builds Tcl first if needed
# -> android-host/app/build/outputs/apk/debug/app-debug.apk
tests/android-smoke.sh             # needs adb + a device/emulator
tests/host-embed.sh                # no device: runs the same Tcl embedding code + self-test on the build machine
```

## Design notes (compiled and host-tested, **never run on Android** — see docs/status.md)

- `TclRuntime` (Java) → `tcldroid_jni.c` starts one native thread that owns the interpreter and calls `tcldroid_core.c`, the JNI-free core that also builds on the host (`tests/host-embed.sh`). The interpreter is process-lifetime state: `MainActivity` keeps its log in static state and does not restart Tcl on rotation.
- `puts` works unmodified: Tcl's stdout/stderr are replaced by channels that write to logcat and the UI.
- `HOME`=`filesDir`, `TMPDIR`=`cacheDir` and `TZ`=Java default zone are set before Tcl starts: Android's process environment has none of them, and Tcl's `clock` needs `TZ` because there is no `/etc/localtime`. `LANG=en_US.UTF-8` is also set and `encoding system` forced to `utf-8` as a *defensive* measure: on Bionic (API 34 image) Tcl already selects `utf-8` with an empty environment (tested in `tests/bionic-chroot.sh`), so this is belt and braces, not a fix.
- Tcl's script library is a zip appended to `libtcl9.0.so`, mounted by opening the library file found via `dladdr`. So the app **must** use legacy native-library packaging (`extractNativeLibs=true`) and **must not** strip that library. Both are set in `app/build.gradle`. (The alternative, shipping the library zip as an asset and mounting it explicitly, is the fallback if Play/size constraints ever require uncompressed in-APK libraries.)
- `libtcldroid.so` links `libtcl9.0.so` by *name*, not path; `libtcl9.0.so` has no `SONAME`, so a full path would end up in `DT_NEEDED`.
