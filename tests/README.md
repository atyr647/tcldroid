# tests

| Script | Needs | What it checks |
| --- | --- | --- |
| `host-embed.sh` | build machine | Android host's Tcl embedding code + `selftest.tcl` against a *host* Tcl (22 checks). Not Bionic/JNI. |
| `bionic-chroot.sh` | Linux x86_64, **root** (chroot/mount), `debugfs` | The **Android x86_64 Tcl build on real Bionic** (Android's libc + linker, extracted from the pinned system image): `selftest.tcl` with an empty environment. No emulator/KVM. Not JNI/Activity. |
| `tk-host.sh` | build machine | Tk (host build with the Android platform layer) against `tests/tk/*.tcl`: clipboard, touch scrolling, long-press selection, toolbar, pinch, slide-off/cancel, rotation (35 checks). The same script also runs on the Android x86_64 build under Bionic (see docs/status.md). |
| `jvm-jni.sh` | build machine, JDK | The real JNI layer + `TclRuntime.java` on a host JVM with `-Xcheck:jni`: start, frames, touch, keyboard, resize, Back, and (GestureHarness) clipboard, long press, toolbar, pinch. |
| `tk-suite.sh` | build machine | **Tk's own test suite** (97 files, ~9,700 tests) against this port, with an expected-failures list; `ASAN=1` for AddressSanitizer. See [docs/tk-test-suite.md](../docs/tk-test-suite.md). |
| `android-smoke.sh` | `adb` + device/emulator | Installs the APK, runs the self-test inside the real app, expects `TCLDROID_SMOKE: PASS`. **Never run.** |

| `tcltest.sh android\|host` | root for `android` | Tcl's **own** test suite (`all.tcl`) with the `tcltest` executable, on the Android build under real Bionic or on the host (baseline). Compare the two; see docs/status.md for results and known environmental failures (`httpProxy` needs DNS, `exec-19.1` needs `/bin/sh`). |
| `tcltest-perfile.sh host\|android` | as above | Runs every test file separately and records per-file totals (`build/perfile-<mode>.tsv`), to diff host vs Android. |
