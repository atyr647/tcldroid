# Status (what is true today)

Last updated: 2026-11-03

| Milestone | State |
| --- | --- |
| M0 reproducible builds | **Done.** `tools/build-tcl-android.sh` builds unmodified Tcl 9.0.4 (no source patches) for arm64-v8a and x86_64; `tools/build-android-host.sh` builds a debug APK containing both. |
| M1 headless Tcl on Android + core tests | **Reached.** Tcl's full suite on real Bionic (x86_64 chroot) and the host app self-test **22/22 on a real arm64 phone**. Tcl's own suite: 70,421 tests, 0 attributable to Tcl; 14 failures, all explained (below). |
| M2 Tk on Android | **Reached: confirmed on a real phone (host app 0.2.1).** Earlier verified on the build machine and on Bionic. All 195 platform functions are real (no stubs). Real Tk widgets render and respond to simulated touch on the host (glibc) and under Android's own libc (x86_64 chroot). |
| M3–M6 | not started |

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

## Tk on Android (new)
Verified on the **build machine (glibc, FreeType test font)** and the **Android x86_64 build under real Bionic** (`tests/bionic-chroot.sh`-style chroot, fallback box glyphs): Tk 9.0.4 loads, mounts its own script library from a zip appended to `libtk9.0.so`, creates and draws label, button, entry, check/radio, scale, listbox, spinbox, text (tags, wrapping), canvas (oval/rect/line/text), ttk widgets and photo images; a simulated touch-down/up runs a button's `-command`. Screenshots are produced by `::tkdroid::snapshot` and inspected as PNGs.
Bugs found by this testing and fixed: Tk's shared `XGetVisualInfo` takes `depth` from `visual->bits_per_rgb` (set to 24, as macOS does); the Tcl 9 argument order is `zipfs mount zipfile mountpoint`; a stale-stub bug in `tools/build-tk.sh`.
Host app 0.2.0 (APK) adds: `TkView` (draws Tk's frame buffer, forwards touch/keys), soft keyboard when a text widget takes the caret, `android.graphics.Paint` as the text engine, Back -> `WM_DELETE_WINDOW`, system-bar and keyboard insets, and `assets/demo.tcl` (an ordinary Tk script). JNI method signatures were checked against the compiled Java; **none of the Java, JNI frame transfer, Paint text engine, touch/IME path has run on a device yet.**

## First phone run of Tk (0.2.0): crash found and fixed (0.2.1)
User report: a flash of an interface with no text, then the app closed. Root cause, reproduced two independent ways: `XFree()` called `free()` on memory that Tk's xlib helper had allocated with `ckalloc` (`XGetVisualInfo`, hit by the first SVG check/radio indicator). That corrupts the heap; Android's allocator (Scudo) aborts at once. Confirmed on Bionic in the chroot: with the bug restored, `Scudo ERROR: corrupted chunk header ... SIGABRT`; with the fix, the demo plus a keyboard-style resize runs clean. Also run clean under AddressSanitizer, and the real JNI layer survives start/tap/key/resize/Back on a host JVM with `-Xcheck:jni` (`tests/jvm-jni.sh`). Fixed alongside: frames were presented before Tk's widgets had redrawn (a visible empty-UI flash; the "no text" in the screenshot), and window reads could exceed the screen buffer during a resize. 0.2.1 adds an in-app dialog (with Copy) describing a previous abnormal exit, so failures need no `adb`.
**Confirmed on a real phone (user screenshot, host app 0.2.1; 1440x2791 @ 600 dpi, `tk scaling` 8.47): Tk 9.0.4 runs.** Real Tk widgets are drawn with Android's own text (Paint), touch taps run button commands ("Tapped 3 times"), a canvas item was dragged, a radio button was selected by touch, the soft keyboard appeared for the entry (caret visible) and the interface re-flowed around it. That is milestone M2 met on a device.
**Still unverified on a device:** performance (not yet measured), text input *through* the keyboard (the screenshot shows it open, not typed into), rotation, other devices/Android versions, the abnormal-exit dialog.

## NOT verified
- Tk on a device or emulator: frame display, Paint text rendering and metrics, touch/keyboard input, rotation/resize, performance (redraw cost is unmeasured), Tk's own test suite against this backend, selection/clipboard (unimplemented), menus, multi-touch.
- On the device: real DNS and non-loopback networking, the x86_64 app, other Android versions/devices, SELinux/OEM variations. One phone (arm64), one run each of 0.1.0 and 0.1.1; the phone model/Android version are not recorded because the screenshots were scrolled past the device-info header.
- arm64 Tcl's own test suite (only the self-test has run on arm64).
- `tests/android-smoke.sh` (the adb automation) and `.github/workflows/ci.yml` have never been run; the self-test itself has now run once by hand on a device.


## Next steps
1. (Optional) Automate the on-device run: `tests/android-smoke.sh` on a KVM-accelerated emulator or an attached phone (`tools/build-android-host.sh && tests/android-smoke.sh`). The manual run already passed.
2. Optionally run the arm64 build under `qemu-user` with an arm64 system image for Bionic coverage on that ABI.
3. Phase 2: implement the Tk platform layer in the order of the work list (start with the in-process 'server' families so a window and a `button` can be created without drawing; then regions, colours, fonts and drawing). The rendering/text spike (ADR 0002) needs timings from a real device to mean anything.
4. CI on a KVM-capable runner.
