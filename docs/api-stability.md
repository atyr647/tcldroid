# API stability

What an application may rely on, and what may change. Versioning is [semantic](https://semver.org/):
**stable** items do not break within a major version (a change is announced one minor version ahead and
kept working until the next major); **provisional** items may change in a minor version; **internal** items
exist for tests and may change at any time.

## Stable

| Item | Where documented |
| --- | --- |
| Tcl 9.0 and Tk 9.0 commands and widgets, as upstream defines them (with the Android policies in [compatibility-policy.md](compatibility-policy.md)) | upstream manuals |
| `tools/pack-app.sh` options and the app folder layout (`main.tcl`, `lib/`, `icon.png`, `icon-foreground.png`) | [packaging.md](packaging.md) |
| `::tkdroid::vibrate`, `haptic`, `sound`, `device`, `ready`, `pickfile`, `savefile`, `share`, `openurl`, `permission`, `permitted`, `notify` | [device-api.md](device-api.md) |
| Virtual events `<<AndroidPause>>`, `<<AndroidSaveState>>`, `<<AndroidResume>>`, `<<AndroidLowMemory>>`, `<<LongPress>>`, `<<PinchStart>>`, `<<Pinch>>`, `<<PinchEnd>>` | device-api.md, [touch-and-clipboard.md](touch-and-clipboard.md) |
| `::tkdroid::touchScroll` | touch-and-clipboard.md |
| `$env(HOME)`, `$env(TMPDIR)`, `$::tkdroid::appdir` meanings | device-api.md |

## Provisional

`::tkdroid::textmenu`; the exact `::tkdroid::device` keys beyond those documented; the list of classes
(Text, Listbox, Treeview) the touch policy wraps; the behaviour of the 4-second start-up hold.

## Internal (tests only)

`::tkdroid::request` (the primitive under the services), `::tkdroid::snapshot`, `touch`, `key`, `screen`, `resize`, `stats`, `keyboard`, `hostclipboard`,
`tktestinit`, `::tkdroid::native`; the `TKDROID_SCREEN` and `TKDROID_DESKTOP_WM` environment variables;
everything in `tkAndroid.h` (the host interface) until it is documented as a porting API.
