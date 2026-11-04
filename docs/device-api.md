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

## Platform services: files, share, links, permissions, notifications

These ask Android to do something, possibly with system UI, and answer **later**: your `-command` script is
called with two more arguments, a **status** (`ok`, `cancelled`, `denied` or `error`) and a **list of results**.
The app keeps running meanwhile.

| Command | Results |
| --- | --- |
| `::tkdroid::pickfile -command cmd ?-types {image/png ...}? ?-multiple 1?` | local file paths. The system picker (any provider: Drive, Downloads, SD card...) is used; each chosen file is copied into the app's cache first, so ordinary `open`/`file` commands work on the results. |
| `::tkdroid::savefile -source file -name name -command cmd ?-type mime?` | the saved name. The user picks the location; your file is copied there. |
| `::tkdroid::share ?-text t? ?-file path? ?-type mime? ?-title t? ?-command cmd?` | none. Opens the system share sheet. (Android does not say whether the user finished.) |
| `::tkdroid::openurl url ?-command cmd?` | none. `http`, `https`, `mailto`, `tel`, `geo`, `sms` only. |
| `::tkdroid::permission request perm ?perm ...? -command cmd` | the granted permissions. The permission must also be declared in the app: `pack-app.sh --permission android.permission.CAMERA`. Status `denied` if any was refused. |
| `::tkdroid::permitted perm` | `1` or `0` right now (synchronous). |
| `::tkdroid::notify -title t -text t ?-command cmd?` | none. On Android 13+ declare and request `android.permission.POST_NOTIFICATIONS` first (status `denied` otherwise). |

```tcl
::tkdroid::pickfile -types {text/plain} -command {apply {{status files} {
    if {$status eq "ok"} {set fh [open [lindex $files 0]]; .t insert end [read $fh]; close $fh}
}}}
```

Tk's own dialogs also work (`tk_messageBox`, `tk_dialog`, `tk_chooseColor`, `tk_getOpenFile` inside the app's
own storage, menus, `tk_popup`, grabs); use `pickfile`/`savefile` when the user should reach files outside the app.
A request that makes no sense (nothing to share, an unreadable source file, an unknown option) is a Tcl error at
once; everything that depends on the user or the system arrives through the callback. With no Android host
(headless tests) every service answers `error`.

## Start-up screen

The app shows its launch screen (`--splash '#RRGGBB'` and the app icon) until the first *complete*
screen is ready. Frames are withheld while `main.tcl` is still building its UI, even if it calls
`update` on the way, and released when the script reaches the event loop (`vwait`, `tkwait`, or the
end of `main.tcl`), when it calls `::tkdroid::ready`, or after 4 seconds, whichever is first.

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
