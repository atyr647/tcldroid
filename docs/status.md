# Status (what is true today)

Last updated: 2026-11-03

| Milestone | State |
| --- | --- |
| M0 reproducible builds | **Done.** `tools/build-tcl-android.sh` builds unmodified Tcl 9.0.4 (no source patches) for arm64-v8a and x86_64; `tools/build-android-host.sh` builds a debug APK containing both. |
| M1 headless Tcl on Android + core tests | **Reached for x86_64 on real Bionic; not yet on a device/emulator or arm64.** Tcl's own suite: 70,421 tests, 0 attributable to Tcl; 14 failures, all explained (below). |
| M2–M6 | not started |

## What "on Bionic" means here
No emulator was usable (this container has no KVM; Android 14 under pure software emulation never finished booting in ~90 minutes). Instead `tools/bionic-root.sh` extracts Android 14's **own** `linker64`, `libc`, `libm`, `libdl`, `libz`, `toybox` userland and `/system/etc/hosts` from the pinned system image into a chroot, and the **Android x86_64 build** of Tcl runs inside it with an empty environment. That is real Bionic and the real Android linker, but **not** the Android framework: no JNI, no Activity, no app sandbox/SELinux/seccomp, no arm64.

## Tcl's own test suite (`tests/tcltest.sh`)
Same suite, same 168 files, same harness conventions, host build as baseline:

| | Total | Passed | Skipped | Failed |
| --- | --- | --- | --- | --- |
| host (glibc, baseline) | 70,421 | 62,618 | 7,803 | 0 |
| Android x86_64 on Bionic | 70,421 | 62,539 | 7,868 | 14 |

The 14 failures, all understood:

| Tests | Cause | Kind |
| --- | --- | --- |
| `httpProxy-2.1.0` … `5.1.2` (12) | fetch `www.google.com`; the chroot has no DNS | test environment |
| `exec-19.1` | hard-codes `/bin/sh`; Android only has `/system/bin/sh` | platform difference |
| `unixInit-3.2` | sets `LANG=japanese` and expects `euc-jp`/`shiftjis`; Bionic only supports UTF-8 locales so Tcl correctly reports `utf-8` | platform difference |

Extra skips on Android vs host (65): `icu` 58 (platform ICU not available, see policy), and a few tool-related constraints (`filetime64bit`, `umask`, `foundGroup`, `haveBash`, `testWithLimit`: 2/2/2/1/1) that I did not analyse individually; they involve toybox vs GNU coreutils behaviour, not Tcl.

`load.test`, `unload.test`, `pkgMkIndex.test` (native shared-library loading on Bionic): 84/84 pass.

### How we got here (so the numbers can be trusted)
- First full Android run showed 132 failures; **119 were defects in my test root** (no `/linkerconfig/ld.config.txt` made every child process print a linker warning that `exec` treats as an error; no `/system/etc/hosts` so `localhost` failed). Fixed; the root now mirrors a real device for these.
- The first host baseline reported 53,353 tests vs Android's 69,753. Cause: my host harness ran in the C locale, `cmdAH.test` crashed at test 24.9 writing non-ASCII to stdout, and its ~17,000 tests never ran. With `LANG=C.UTF-8` the host runs all 70,421. (Bionic is UTF-8 by default, so Android was unaffected.)

## Bug found and fixed in the Android build
`package require sqlite3` (and `cookiejar`) failed on Bionic with `cannot locate symbol "trunc"`: the package libraries did not list `libm.so` as a dependency. glibc tolerates that, Bionic does not. Fixed by passing `LIBS=-lm` (`tools/build-tcl-android.sh`); verified on x86_64 (sqlite3 3.53.0, cookiejar, tdbc, Thread, itcl all load) and `DT_NEEDED libm.so` confirmed in the arm64 libraries too.

## Other verified facts
- Android binaries: correct ELF type/arch, Bionic-only dependencies, 16 KB `PT_LOAD` alignment, Tcl library zip embedded (`//zipfs:/lib/tcl/tcl_library`).
- Debug APK builds for both ABIs. In it, `libtcl9.0.so` is byte-identical to the staged build (appended zip intact), `libtcldroid.so` links `libtcl9.0.so` by name, exports the JNI entry points, and every imported Tcl symbol is exported by `libtcl9.0.so`. Manifest `extractNativeLibs=true`, `zipalign -P 16` passes.
- `tests/host-embed.sh`: the JNI-free embedding core (`tcldroid_core.c`) runs the 22-check `selftest.tcl` on the host 22/22 with a relocated `libtcl9.0.so` and an empty environment. The same `selftest.tcl` also passes 22/22 on Bionic (`tests/bionic-chroot.sh`).
- Bionic chooses `utf-8` as the system encoding with no `LANG`; the host forcing of UTF-8 is defensive only.

## NOT verified
- Anything on a real Android device or emulator: the app, JNI callbacks, the Tcl thread under the Android runtime, `dladdr` path inside the app's linker namespace, app sandbox/SELinux effects, real DNS and sockets from an app.
- **arm64 has never been executed**, only built and inspected.
- `tests/android-smoke.sh` and `.github/workflows/ci.yml` have never been run.
- Tk: nothing exists yet.

## Next steps
1. Run `tests/android-smoke.sh` on a KVM-accelerated emulator or a real device (JNI/Activity layer). Any machine with the Android SDK + emulator + KVM: `tools/build-android-host.sh && tests/android-smoke.sh`.
2. Optionally run the arm64 build under `qemu-user` with an arm64 system image for Bionic coverage on that ABI.
3. Phase 2: Tk backend (ADR 0002). The rendering/text spike needs timings from a real device or accelerated emulator to mean anything.
4. CI on a KVM-capable runner.
