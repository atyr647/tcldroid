# Changelog

tcldroid follows [semantic versioning](https://semver.org/) for what [docs/api-stability.md](docs/api-stability.md)
lists as *stable*. The version lives in the `VERSION` file; the APK's `versionCode` is
`major*10000 + minor*100 + patch`.

## 0.6.0
- Runs Tk's own test suite (97 files, 9,742 tests) against the port: 86.7% pass, every failure triaged,
  0 crashes, 0 AddressSanitizer errors ([docs/tk-test-suite.md](docs/tk-test-suite.md)).
- Fixed what it found: missing `VisibilityNotify`, `tk busy`, use of destroyed windows (new patch 0004),
  cursor validation, pointer warp, `tk caret`, `selection clear -selection CLIPBOARD`, unmapped toplevel sizing.
- Packager: `--orientation`, `--permission`, `--abis`, adaptive icons (`icon-foreground.png`, `--icon-bg`), `--backup`.
- Reproducible APKs (byte-identical rebuilds); third-party licence notices ship in every APK.
- CI workflow rewritten around commands that run locally; the demo logs a verdict for `tests/android-smoke.sh`.

## 0.5.1
- Drawing 3-6x faster on Learn Tcl (clipped row copies, text cache, SIMD fills, touch-move coalescing).
- Launch splash (`--splash`) and a start-up hold: no blank or half-built frames.

## 0.5.0
- Device API: `::tkdroid::vibrate/haptic/sound/device`, `bell`, lifecycle events, fullscreen.
- Signing (`--keystore`, `--release`, `--aab`, `tools/make-keystore.sh`); the app folder is unpacked only when the build changes.

## 0.4.0
- System clipboard, long-press selection with the native Cut/Copy/Paste toolbar, drag-to-scroll with momentum,
  pinch events, correct cancel/slide-off behaviour for touches.

## 0.3.0
- App packager: any Tcl/Tk folder with a `main.tcl` becomes its own APK.

## 0.2.x
- Tk 9.0.4 on a phone: widgets, Android `Paint` text, touch, soft keyboard, insets, crash reporting.
- 0.2.1: fixed a startup crash (`free()` of Tk-allocated memory). 0.2.3-0.2.4: keyboard policy, self-test in the Tk UI.

## 0.1.x
- Headless Tcl 9.0.4 for Android (arm64-v8a, x86_64), verified on real Bionic; host app running a self-test.
