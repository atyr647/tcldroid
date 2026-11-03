# Status (what is true today)

Last updated: 2026-11-03

| Milestone | State |
| --- | --- |
| M0 reproducible builds | **Done for build, not for run.** `tools/build-tcl-android.sh` builds unmodified Tcl 9.0.4 for arm64-v8a and x86_64; `tools/build-android-host.sh` builds a debug APK containing both. Zero patches to upstream. |
| M1 headless Tcl on Android + core tests | **Partly reached.** The Android x86_64 `tclsh` runs and passes our 22-check self-test on **real Bionic** (see below). **Tcl's own test suite** was run on it (below): no failure attributable to Tcl or Bionic remains in the files that were re-run. Nothing has run on a real Android device/emulator or arm64. |
| M2–M6 | not started |

## Verified (by inspection or by running on the build machine)
- Android binaries: correct ELF type/arch, Bionic-only dependencies, 16 KB `PT_LOAD` alignment, Tcl library zip embedded (`//zipfs:/lib/tcl/tcl_library`, 869 files).
- Debug APK (`android-host/app/build/outputs/apk/debug/app-debug.apk`, ~4 MB):
  - `libtcl9.0.so` is **byte-identical** to the staged build in both ABIs, so packaging/stripping did not damage the appended zip.
  - `libtcldroid.so` has `DT_NEEDED libtcl9.0.so` (name, not path), exports `JNI_OnLoad` and `Java_dev_tcldroid_host_TclRuntime_nativeStart`, and every Tcl symbol it imports is exported by `libtcl9.0.so`.
  - Manifest: minSdk 26, targetSdk 35, `extractNativeLibs=true`; `zipalign -P 16` passes.
  - No compiler warnings with `-Wall -Wextra` in the glue.
- **Embedding logic, on the build machine** (`tests/host-embed.sh`): the JNI-free core (`tcldroid_core.c`) drives the 22-check `selftest.tcl` against a host Tcl with `libtcl9.0.so` loaded from a relocated directory, an empty environment (no `LANG`, no `TCL_LIBRARY`) and stdout/stderr redirected through the custom channels: **22/22 pass**. This covers app-hook zipfs mount, `init.tcl` from zipfs, UTF-8 forcing, bundled tzdata, sockets, event loop.

- **Android build on real Bionic** (`tests/bionic-chroot.sh`): the Android x86_64 `tclsh9.0` + `libtcl9.0.so` run under Android 14's own `linker64`/`libc`/`libm`/`libdl`/`libz` (extracted from the pinned system image, in a chroot, empty environment): **22/22 pass**, including zipfs library mount, Unicode filenames, bundled tzdata, loopback sockets, event loop, `zlib` (system libz), and `exec /system/bin/sh`. Bionic reports UTF-8 by itself, so Tcl chooses `utf-8` with no `LANG`.
- **Tcl's own test suite on real Bionic** (`tests/tcltest.sh android`; the same suite on the host is the baseline, `tests/tcltest.sh host`). First full run: 168 files, 132 failures, all in 21 files; the same 21 files on the host: 4,207 tests, 0 failures. Triage showed **119 of the 132 were defects in my test root, not Tcl**: missing `/linkerconfig/ld.config.txt` (every child process wrote a linker warning to stderr, which `exec` treats as an error) and missing `/system/etc/hosts` (`localhost` did not resolve). After fixing the root, re-running those 21 files: 4,274 tests, **13 failures, all environmental**: 12 `httpProxy` tests need real DNS for `www.google.com` (the chroot has no resolver) and `exec-19.1` hardcodes `/bin/sh`, which Android does not have. On the 12 largest test files (incl. `clock.test`, 17,177 tests) host and Android totals are identical with 0 failures.
  - **Open:** the full host run once reported 53,353 tests vs 69,753 on Android for the same 168 files; I have not yet explained that gap (a re-run of the full host suite is the next check) and the full Android suite has not been re-run since the root fixes. Treat "Tcl's test suite passes on Android" as *likely but not yet established*.
  - These runs use the x86_64 build in a chroot with real Bionic, not an emulator or an arm64 device.
- Booting the emulator **without KVM does not work**: Android 14 under pure software emulation never completed boot in ~90 minutes (system_server watchdog restart loop). `tools/run-emulator.sh` needs KVM (or a much lighter image) to be useful.

## NOT verified
- JNI, the Java Activity, the Tcl thread under Android's app runtime, `dladdr` path inside the app's linker namespace, arm64 execution (only x86_64 was run), Tk.
- On a real device/emulator: library loading by the app, JNI callbacks, the thread, whether `dladdr` yields a mountable path inside the app, sockets under the app sandbox, SELinux/seccomp effects.
- `tests/android-smoke.sh` and `.github/workflows/ci.yml` have never been executed.
- Tcl's own test suite (M1) has not been run on Bionic or anywhere on Android yet.

## Findings so far
- Tcl 9 removed implicit `~` expansion (`file home` instead) and `tcl_platform(threaded)`; the compatibility policy now says so.
- With no `LANG`, Tcl picks `iso8859-1`, which breaks Unicode filenames. The host sets `LANG` and forces `encoding system utf-8`; the self-test guards this.
- The tdbc mysql/odbc/postgres drivers compile but are useless on Android; the host app does not ship them.

## Next steps
1. Run Tcl's own test suite under Bionic (`tests/bionic-chroot.sh` infrastructure) — the M1 gate.
2. Run `tests/android-smoke.sh` on a KVM-accelerated emulator or a real device (JNI/Activity layer): `tools/build-android-host.sh && tests/android-smoke.sh`.
3. Phase 2 spike: text/rendering (ADR 0002).
