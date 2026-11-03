# android-host

A minimal Android app that hosts an **unmodified Tcl 9 core** (`libtcl9.0.so`) on its own thread. It is the foundation for the Tk host (Phase 2): Activity lifecycle, JNI bridge, packaging.

Today it runs `app/src/main/assets/selftest.tcl` at startup, shows the output, and logs a verdict line `TCLDROID_SMOKE: PASS|FAIL n/m` to logcat (tag `tcldroid`).

## Build

```sh
tools/build-android-host.sh        # from the repo root; builds Tcl first if needed
# -> android-host/app/build/outputs/apk/debug/app-debug.apk
tests/android-smoke.sh             # needs adb + a device/emulator
```

## Design notes (all verified only by reading/compiling, not yet by running — see docs/status.md)

- `TclRuntime` (Java) → `tcldroid_jni.c` starts one native thread that owns the interpreter. The interpreter is process-lifetime state: `MainActivity` keeps its log in static state and does not restart Tcl on rotation.
- `puts` works unmodified: Tcl's stdout/stderr are replaced by channels that write to logcat and the UI.
- `HOME`=`filesDir`, `TMPDIR`=`cacheDir`, `TZ`=Java default zone and a UTF-8 locale are set before Tcl starts, because Android provides none of `LANG`, `/etc/localtime`, or usable stdio. `encoding system` is also forced to `utf-8`.
- Tcl's script library is a zip appended to `libtcl9.0.so`, mounted by opening the library file found via `dladdr`. So the app **must** use legacy native-library packaging (`extractNativeLibs=true`) and **must not** strip that library. Both are set in `app/build.gradle`. (The alternative, shipping the library zip as an asset and mounting it explicitly, is the fallback if Play/size constraints ever require uncompressed in-APK libraries.)
- `libtcldroid.so` links `libtcl9.0.so` by *name*, not path; `libtcl9.0.so` has no `SONAME`, so a full path would end up in `DT_NEEDED`.
