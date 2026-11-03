# Compatibility matrix

Seeded from the 89 manual pages (`doc/*.n`) and the 25 `wm` subcommands of **Tk 9.0.4**. *Class* is the **planned** treatment (see [compatibility-policy.md](compatibility-policy.md)); *Status* is what is true **today**.

Status legend: ⬜ not started · 🔨 in progress · ✅ verified on device/emulator · ❌ known broken

| Status today is ⬜ for every Tk row: no Tk backend exists yet. |
| --- |

## Tcl core

| Item | Class | Phase | Status |
| --- | --- | --- | --- |
| Cross-build `libtcl9.0.so` + `tclsh` for arm64-v8a, x86_64 | Native | 1 / M0 | 🔨 builds; not yet run |
| Core test suite on device/emulator | Native | 1 / M1 | ⬜ |
| zipfs mount of the APK at `//zipfs:/app` | Mapped | 4 | ⬜ |
| Sockets, threads, `load`, clocks, Unicode, filesystem | Native | 1 | ⬜ |
| Bundled packages (`itcl`, `tdbc`, `thread`, `sqlite3`) | Native | 1 | ⬜ |

## Widgets (classic)

| Item | Class | Phase | Status |
| --- | --- | --- | --- |
| `button`, `checkbutton`, `radiobutton`, `label`, `labelframe`, `frame`, `message` | Native | 2 | ⬜ |
| `entry`, `spinbox` (needs IME) | Native + Mapped input | 2–3 | ⬜ |
| `text` | Native + Mapped input | 2–3 | ⬜ |
| `canvas` (incl. `-postscript`) | Native | 2 | ⬜ |
| `listbox`, `scale`, `scrollbar`, `panedwindow` | Native | 2 | ⬜ |
| `menu`, `menubutton`, `tk_optionMenu`, `popup` | Mapped (popup layers; menubar as in-window bar) | 2–3 | ⬜ |
| `toplevel` | Mapped (layers) | 2 | ⬜ |

## Widgets (ttk)

`ttk::button`, `checkbutton`, `combobox`, `entry`, `frame`, `label`, `labelframe`, `menubutton`, `notebook`, `panedwindow`, `progressbar`, `radiobutton`, `scale`, `scrollbar`, `separator`, `sizegrip`, `spinbox`, `treeview`, `ttk::style`, `ttk::image` — **Native** (themes from `generic/`/`library/`; an Android-flavoured native theme is a Phase 3 option) — ⬜

## Geometry, binding, focus, options (all Native)

`pack`, `grid`, `place`, `bind`, `bindtags`, `event`, `focus`, `focusNext/Prev`, `grab`, `option`, `tk_setPalette`, `winfo`, `raise`, `lower`, `destroy`, `tkwait`, `update`/`after` interplay, `tk`, `tkerror` — ⬜

## Platform-dependent commands

| Command | Class | Notes | Phase | Status |
| --- | --- | --- | --- | --- |
| `font`, `tk fontchooser` | Mapped / Unsupported (`fontchooser`) | System fonts via Android; measurement/shaping per ADR 0002 | 2 | ⬜ |
| `image` / `photo` / `bitmap` / `nsimage` (`nsimage` is macOS-only) | Native / Unsupported | PNG, GIF, PPM, SVG are in `generic/` | 2 | ⬜ |
| `clipboard`, `selection` | Mapped | | 3 | ⬜ |
| `bell` | Mapped | haptic/sound feedback | 3 | ⬜ |
| `cursors` / `-cursor` | Accepted | pointer icons only with a mouse | 3 | ⬜ |
| `tk_getOpenFile`, `tk_getSaveFile`, `tk_chooseDirectory` | Mapped | Storage Access Framework | 3 | ⬜ |
| `tk_messageBox` | Mapped | native dialog | 3 | ⬜ |
| `tk_chooseColor`, `tk_dialog` | Native | Tk-drawn | 2 | ⬜ |
| `tk sysnotify` | Mapped | notifications (needs permission on API 33+) | 3 | ⬜ |
| `tk systray` | Unsupported | | — | ⬜ |
| `tk print` | Mapped (late) / Unsupported initially | PrintManager | 6 | ⬜ |
| `busy` | Native/Mapped | | 2 | ⬜ |
| `send` | Unsupported | no registry on Android | — | ⬜ |
| `console` / `loadTk` | Unsupported / Native | `console` needs a design | 3 | ⬜ |
| `tk_mac` | Unsupported | macOS only | — | ⬜ |

## `wm` subcommands

| Subcommand | Class | Behaviour |
| --- | --- | --- |
| `title` | Mapped | Activity title / task description |
| `geometry` | Mapped | `.` reports surface size; layers positioned/sized in-Activity |
| `state`, `withdraw`, `deiconify`, `iconify` | Mapped | layer visibility; `iconify .` → `moveTaskToBack` |
| `protocol` | Mapped | `WM_DELETE_WINDOW` ← Back navigation |
| `attributes` | Mapped / Accepted | `-fullscreen` mapped; `-alpha`, `-topmost`, `-zoomed`, `-type`, … accepted |
| `transient`, `overrideredirect`, `manage`, `forget`, `stackorder` | Mapped | layer ownership/popup/stack |
| `minsize`, `maxsize`, `resizable` | Mapped for layers, Accepted for `.` | |
| `iconphoto` | Mapped | task icon |
| `grid`, `aspect`, `sizefrom`, `positionfrom` | Native / Accepted | |
| `client`, `command`, `colormapwindows`, `focusmodel`, `frame`, `group`, `iconbadge`, `iconbitmap`, `iconmask`, `iconname`, `iconposition`, `iconwindow` | Accepted | stored and returned; no effect |

## Extensions / packages

| Item | Class | Phase | Status |
| --- | --- | --- | --- |
| Pure-Tcl packages | Native | 4 | ⬜ |
| Binary extensions rebuilt for Android ABIs | Native | 4 | ⬜ |
| Starkit (zip-based) payloads | Mapped | 4 | ⬜ |
| Starkit (Metakit/`mk4vfs`) payloads | open — see ADR 0001 decision gate | 4 | ⬜ |
| Starpack executable stubs (Windows/macOS/Linux) | Unsupported (payload is extracted and repackaged) | 5 | ⬜ |
