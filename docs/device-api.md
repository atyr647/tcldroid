# Device API for Tcl/Tk apps

Everything lives in the `::tkdroid::` namespace and exists only on Android, so
`[llength [info commands ::tkdroid::vibrate]]` is the test for "am I running on tcldroid"
(`tk windowingsystem` says `x11`, `$tcl_platform(platform)` says `unix`: both are deliberately
what Tk's unchanged widget code expects). Outside a device, e.g. in the headless host tests, the
feedback commands do nothing instead of failing, so scripts need no guards.

## Feedback

| Command | Effect |
| --- | --- |
| `::tkdroid::vibrate ms ?amplitude?` | Vibrate for `ms` (0–5000). `amplitude` 1–255 on devices that support it. Needs no permission prompt. |
| `::tkdroid::haptic ?tick\|click\|heavy\|confirm\|reject?` | The system's own haptic patterns (respect the user's "touch feedback" setting). Default `click`. |
| `::tkdroid::sound preload path ?path…?` | Decode sound files ahead of time (WAV, OGG, MP3). |
| `::tkdroid::sound play path ?-volume 0..1? ?-rate 0.5..2?` | Play a short effect now (low latency, up to 8 at once). Loads the file on first use; a play request made while it is still decoding plays the moment it is ready. |
| `::tkdroid::sound stop` | Stop everything. |
| `bell` | Tk's own command: a click sound. |

Sound is for effects (a few seconds), not music streaming. A script that synthesises its sounds as
WAV data (like Learn Tcl) writes them to `$env(TMPDIR)` once and plays the file.

## Facts: `::tkdroid::device ?key?`

A dict (or one value): `android 1`, `sdk`, `release`, `manufacturer`, `model`, `abi`, `appId`,
`appVersionName`, `appVersionCode`, `versionName` (tcldroid's own), `filesDir`, `cacheDir`, `appDir`,
`dpi`, `locale`, `darkMode`.

## Full screen

`wm attributes . -fullscreen 1` hides the status and navigation bars (swipe from the edge shows
them briefly); `0` brings them back. The window follows the new size like any resize.

## Lifecycle events

Generated on `.` (so `bind . …` or `bind all …`):

| Event | When |
| --- | --- |
| `<<AndroidPause>>`, `<<AndroidSaveState>>` | The app leaves the foreground (Home, another app, screen off). **Save now**: the Activity waits up to half a second for your handlers, after which Android may kill the process at any time. |
| `<<AndroidResume>>` | Back in the foreground. |
| `<<AndroidLowMemory>>` | The system wants memory back: drop caches. |

## Back button

Back runs your `wm protocol . WM_DELETE_WINDOW` handler. To go back inside the app and only leave
from the home screen:

```tcl
wm protocol . WM_DELETE_WINDOW {if {[atHome]} exit else goBack}
```

With no handler the app closes. `exit` ends the Tcl interpreter and the Activity.

## Where files go

| Path | Use |
| --- | --- |
| `$env(HOME)` (= `::tkdroid::device filesDir`) | Your data: kept across launches **and updates**. |
| `$env(TMPDIR)` (= `cacheDir`) | Caches; Android may clear it when space is short. |
| the current directory / `$::tkdroid::appdir` | The app's own files, read-only by convention: replaced when a different build is installed (not on every launch). |

## Typing

The keyboard asks for plain text without suggestions or autocorrect, so code typed in a field
arrives as typed. Composition (CJK, swipe typing with corrections) is not supported yet.
