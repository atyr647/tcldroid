# tests

| Script | Needs | What it checks |
| --- | --- | --- |
| `host-embed.sh` | build machine only | Android host's Tcl embedding code + `selftest.tcl` against a host Tcl (22 checks). Not Bionic/JNI. |
| `android-smoke.sh` | `adb` + device/emulator | Installs the APK, runs the self-test on Android, expects `TCLDROID_SMOKE: PASS`. **Never run yet.** |

Tcl's own test suite on Android (milestone M1) is not set up yet.
