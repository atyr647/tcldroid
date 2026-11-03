# tests

| Script | Needs | What it checks |
| --- | --- | --- |
| `host-embed.sh` | build machine | Android host's Tcl embedding code + `selftest.tcl` against a *host* Tcl (22 checks). Not Bionic/JNI. |
| `bionic-chroot.sh` | Linux x86_64, **root** (chroot/mount), `debugfs` | The **Android x86_64 Tcl build on real Bionic** (Android's libc + linker, extracted from the pinned system image): `selftest.tcl` with an empty environment. No emulator/KVM. Not JNI/Activity. |
| `android-smoke.sh` | `adb` + device/emulator | Installs the APK, runs the self-test inside the real app, expects `TCLDROID_SMOKE: PASS`. **Never run.** |

Tcl's own test suite on Android (milestone M1) is not set up yet.
