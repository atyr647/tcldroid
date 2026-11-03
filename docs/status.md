# Status (what is true today)

Last updated: 2026-11-03

| Milestone | State |
| --- | --- |
| M0 reproducible builds | **Done for build, not for run.** `tools/build-tcl-android.sh` builds unmodified Tcl 9.0.4 for arm64-v8a and x86_64; `tools/build-android-host.sh` builds a debug APK containing both. Zero patches to upstream. |
| M1 headless Tcl on Android + core tests | **Not reached.** Nothing has ever executed on Android: this container has no KVM/emulator. The self-test and smoke runner exist (below) but the Android half is unrun. |
| M2–M6 | not started |

## Verified (by inspection or by running on the build machine)
- Android binaries: correct ELF type/arch, Bionic-only dependencies, 16 KB `PT_LOAD` alignment, Tcl library zip embedded (`//zipfs:/lib/tcl/tcl_library`, 869 files).
- Debug APK (`android-host/app/build/outputs/apk/debug/app-debug.apk`, ~4 MB):
  - `libtcl9.0.so` is **byte-identical** to the staged build in both ABIs, so packaging/stripping did not damage the appended zip.
  - `libtcldroid.so` has `DT_NEEDED libtcl9.0.so` (name, not path), exports `JNI_OnLoad` and `Java_dev_tcldroid_host_TclRuntime_nativeStart`, and every Tcl symbol it imports is exported by `libtcl9.0.so`.
  - Manifest: minSdk 26, targetSdk 35, `extractNativeLibs=true`; `zipalign -P 16` passes.
  - No compiler warnings with `-Wall -Wextra` in the glue.
- **Embedding logic, on the build machine** (`tests/host-embed.sh`): the JNI-free core (`tcldroid_core.c`) drives the 22-check `selftest.tcl` against a host Tcl with `libtcl9.0.so` loaded from a relocated directory, an empty environment (no `LANG`, no `TCL_LIBRARY`) and stdout/stderr redirected through the custom channels: **22/22 pass**. This covers app-hook zipfs mount, `init.tcl` from zipfs, UTF-8 forcing, bundled tzdata, sockets, event loop.

## NOT verified (needs an emulator or device)
- Anything on Bionic or Android: library loading, JNI callbacks, the thread, whether `dladdr` yields a mountable path inside the app, Bionic's locale behaviour, `zlib` from `/system`, `exec`, sockets under the app sandbox.
- `tests/android-smoke.sh` and `.github/workflows/ci.yml` have never been executed.
- Tcl's own test suite (M1) has not been run anywhere on Android.

## Findings so far
- Tcl 9 removed implicit `~` expansion (`file home` instead) and `tcl_platform(threaded)`; the compatibility policy now says so.
- With no `LANG`, Tcl picks `iso8859-1`, which breaks Unicode filenames. The host sets `LANG` and forces `encoding system utf-8`; the self-test guards this.
- The tdbc mysql/odbc/postgres drivers compile but are useless on Android; the host app does not ship them.

## Next steps
1. **Run `tests/android-smoke.sh` on an emulator or device.** Everything else is blocked on a first real run. (On any machine with the Android SDK + an emulator: `tools/build-android-host.sh && tests/android-smoke.sh`.)
2. Then: Tcl's own test suite on device (M1), CI.
3. Phase 2 spike: text/rendering (ADR 0002).
