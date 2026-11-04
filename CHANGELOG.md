# Changelog

tcldroid follows [semantic versioning](https://semver.org/) for what [docs/api-stability.md](docs/api-stability.md)
lists as *stable*. The version lives in the `VERSION` file; the APK's `versionCode` is
`major*10000 + minor*100 + patch`.

## 0.9.4
- Starkit/Starpack support: `pack-app.sh MyApp.kit` unpacks a zip-based kit (Tcl 9 zipkit/zipfs starpack) and packs
  its files; `package require starkit` works (`topdir`, `mode`, `infoscript`, `startup`); Metakit kits are refused with
  the `sdx unwrap` way out; entries that climb out of the folder are refused. [docs/starkit.md](docs/starkit.md).

## 0.9.3
- Accessibility diagnostics: an `a11y-debug` file in the app folder turns on on-screen messages for each step of a
  screen-reader action (see docs/accessibility.md); a general `toast` request kind.

## 0.9.2
- Upstream preparation: the Tk changes are now a 5-patch reviewable series (`upstream/series/`) with explicit
  `TK_ANDROID` platform-header cases in generic code (no more shadowed header names), generated and verified
  against pristine Tk 9.0.4 (`tools/upstream/`); `android/README.md`; [docs/upstream-submission.md](docs/upstream-submission.md).

## 0.9.1
- Accessibility: a screen-reader click now carries the node's own coordinates (no dependence on id bookkeeping on
  the Tcl side); the view itself is no longer a focus/click target for TalkBack, and a click on it activates the
  node that has screen-reader focus; the click is announced; API 26/27 guards.

## 0.9.0
- **Accessibility**: TalkBack and other screen readers can read and operate Tk apps (roles, names, values,
  states, rectangles, click/focus/set-text/scroll actions, focus announcements) through an
  `AccessibilityNodeProvider`; `::tkdroid::accessible` lets an app name, re-role or hide widgets
  ([docs/accessibility.md](docs/accessibility.md)).

## 0.8.0
- `::tkdroid::inputmode`: code / text / number / email / url / password keyboards; **IME composition** (swipe typing,
  suggestions, CJK) inline in `text` mode.
- Hardware keyboard: modifier state, Ctrl+C/V/X/A/Z, Shift+Tab, F1-F12, Ctrl+arrows.
- Non-ASCII text is measured as a whole run (consistent with drawing; right for joined and combining scripts).

## 0.7.0
- Platform services for Tcl scripts: `::tkdroid::pickfile`, `savefile`, `share`, `openurl`, `permission`,
  `permitted`, `notify` ([device-api.md](docs/device-api.md)); a content provider shares files safely.
- Tk's own dialogs, menus, `tk_popup` and grabs are tested on the backend (`tests/tk/dialogs.tcl`).
- New example app `examples/services` that tries every service.

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
