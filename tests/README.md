# tests

| Script | Needs | What it checks |
| --- | --- | --- |
| `host-embed.sh` | build machine | Android host's Tcl embedding code + `selftest.tcl` against a *host* Tcl (22 checks). Not Bionic/JNI. |
| `bionic-chroot.sh` | Linux x86_64, **root** (chroot/mount), `debugfs` | The **Android x86_64 Tcl build on real Bionic** (Android's libc + linker, extracted from the pinned system image): `selftest.tcl` with an empty environment. No emulator/KVM. Not JNI/Activity. |
| `android-smoke.sh` | `adb` + device/emulator | Installs the APK, runs the self-test inside the real app, expects `TCLDROID_SMOKE: PASS`. **Never run.** |

| `tcltest.sh android\|host` | root for `android` | Tcl's **own** test suite (`all.tcl`) with the `tcltest` executable, on the Android build under real Bionic or on the host (baseline). Compare the two; see docs/status.md for results and known environmental failures (`httpProxy` needs DNS, `exec-19.1` needs `/bin/sh`). |
