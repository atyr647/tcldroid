# Porting a Tcl 8.6 app to tcldroid (Tcl/Tk 9.0)

tcldroid runs **Tcl 9.0.4 and Tk 9.0.4**, so an app written for 8.6 or for AndroWish meets two
kinds of difference. This list comes from porting a real 3,000-line app (Learn Tcl, 78 lessons,
770 exercises) and running its whole test suite on Tcl 9; it is what that app needed, not a
complete list of Tcl 9 changes (see Tcl's own `changes` and the TIP list for those).

## Tcl 8.6 → 9.0 differences that bit

1. **Relative qualified variable names no longer fall back to the global namespace.**
   In 8.6, `$store::dir` inside `proc app::init` found `::store::dir` when `::app::store::dir`
   did not exist. In 9 it does not, and the failure is silent for writes: `set ui::fscale 1.2`
   creates `::app::ui::fscale`. Write `$::store::dir`, `set ::ui::fscale …`, `-textvariable
   ::ns::var`. The absolute form works in 8.6 too, so the fix is safe for both. (Commands still
   resolve as before.) A regexp over `\$name::name` and `set|incr|lappend|append|unset|array … name::name`
   finds almost every site.
2. **`string bytelength` is gone.** Use `string length [encoding convertto utf-8 $s]`.
3. **`package require Tcl 8.6` refuses 9.0.** A bare `8.6` means "8.6.x only". Write
   `package require Tcl 8.6-` ("8.6 or newer"); the same for `Tk`.

Everything else in Learn Tcl (770 exercises, every code sample in every lesson, `coroutine`, TclOO,
`dict`, `chan`, sockets, `binary`, `encoding`, `clock`) ran unchanged on 9.0.4.

## From AndroWish / `borg`

| AndroWish | tcldroid |
| --- | --- |
| `[llength [info commands borg]]` to detect Android | `[llength [info commands ::tkdroid::vibrate]]` |
| `borg vibrate ms` | `::tkdroid::vibrate ms` (and `::tkdroid::haptic`) |
| `borg beep`, Snack, Muzic | `::tkdroid::sound play file.wav`, `bell` |
| `wm attributes . -fullscreen 1` | the same; hides the system bars |
| `sdltk textinput 1`, `sdltk touchtranslate` | not needed (the soft keyboard follows focus; touches are already plain left-button events) — wrap in `catch` and they are harmless |
| `$env(INTERNAL_STORAGE)`, `$env(HOME)` | `$env(HOME)` is the app's private files directory |
| Back key | `wm protocol . WM_DELETE_WINDOW {…}` |

See [device-api.md](device-api.md) for the full list and
a patch to Learn Tcl (kept in the Learn Tcl repo, not here) for a worked example
(Learn Tcl: the three fixes above, a tcldroid sound/haptic backend, Back and save-on-pause).
