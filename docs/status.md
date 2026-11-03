# Status (what is true today)

Last updated: 2026-11-03

| Milestone | State |
| --- | --- |
| M0 reproducible builds | **Done.** `tools/build-tcl-android.sh` builds unmodified Tcl 9.0.4 (no source patches) for arm64-v8a and x86_64; `tools/build-android-host.sh` builds a debug APK containing both. |
| M1 headless Tcl on Android + core tests | **Reached.** Tcl's full suite on real Bionic (x86_64 chroot) and the host app self-test **22/22 on a real arm64 phone**. Tcl's own suite: 70,421 tests, 0 attributable to Tcl; 14 failures, all explained (below). |
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

## First run on a real device (user-reported screenshot, host app 0.1.0)
Real arm64 Android phone (Android user `u0_a535`, TZ `America/New_York`): **21/22 self-test checks passed**. Confirmed in a real app: the app loads `libtcl9.0.so` and `libtcldroid.so`, the Tcl thread runs, JNI callbacks and stdout-to-screen work, Tcl mounts its embedded script library by itself (`zipfs-library`, `zipfs-init-tcl`, `zipfs-mounts`), `encoding system` is `utf-8`, Unicode filenames round-trip, bundled tzdata works with `TZ`, `zlib`, event loop, coroutines, and `exec /system/bin/sh` all work.
The one failure, `socket-loopback: couldn't open socket: operation not permitted`, was **my omission**: Android refuses `socket()` (even to localhost) unless the manifest declares `android.permission.INTERNET`. Fixed in host app 0.1.1. **Re-run on the same phone (user-reported screenshot): `TCLDROID_SMOKE: PASS 22/22`**, including `socket-loopback`. The packager (Phase 5) must therefore emit `INTERNET` by default for apps that use sockets.

## NOT verified
- On the device: real DNS and non-loopback networking, the x86_64 app, other Android versions/devices, SELinux/OEM variations. One phone (arm64), one run each of 0.1.0 and 0.1.1; the phone model/Android version are not recorded because the screenshots were scrolled past the device-info header.
- arm64 Tcl's own test suite (only the self-test has run on arm64).
- `tests/android-smoke.sh` (the adb automation) and `.github/workflows/ci.yml` have never been run; the self-test itself has now run once by hand on a device.
- Tk: nothing exists yet.

## Next steps
1. (Optional) Automate the on-device run: `tests/android-smoke.sh` on a KVM-accelerated emulator or an attached phone (`tools/build-android-host.sh && tests/android-smoke.sh`). The manual run already passed.
2. Optionally run the arm64 build under `qemu-user` with an arm64 system image for Bionic coverage on that ABI.
3. Phase 2: Tk backend (ADR 0002). The rendering/text spike needs timings from a real device or accelerated emulator to mean anything.
4. CI on a KVM-capable runner.
