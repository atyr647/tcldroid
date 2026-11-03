# tk-android

The Tk `android/` platform tree (docs/adr/0002-tk-backend-architecture.md). Copied into the Tk source tree by `tools/fetch-upstream.sh` / `tools/build-tk.sh`.

| File | Role |
| --- | --- |
| `tkAndroid.h` | Public host interface: callbacks, text-engine interface, thread-safe input queues. No Tk internals. |
| `tkAndroidPort.h`, `tkUnixInt.h`, `tkUnixDefault.h` | Platform headers (shadowing the Unix names so generic code stays unpatched). |
| `tkAndroidWindow.c` | The in-process "X server": window tree, stacking, mapping, exposure, pixmaps, clipping. |
| `tkAndroidDraw.c` | Software rasteriser behind the Xlib-shaped drawing API; images; text blit. |
| `tkAndroidRegion.c` | Regions as rectangle lists. |
| `tkAndroidEvent.c` | Tcl event source, touch/key/back queues, focus, pointer, keysyms, `tkdroid::` test hooks. |
| `tkAndroidWm.c` | Window manager (`.` fills the screen, other toplevels are layers; Back; `wm` subcommands). |
| `tkAndroidFont.c`, `tkAndroidTestEngine.c` | Fonts on a pluggable text engine; FreeType (host tests) and a box-glyph fallback. |
| `tkAndroidColor.c`, `tkAndroidDisplay.c` | Colours, cursors, selection stubs; display, init, library mount, trivia. |

Build: `tools/build-tk.sh host` (fast loop: FreeType text, `TKDROID_SCREEN=1080x2200@420`) or `tools/build-tk.sh arm64-v8a|x86_64`.
Test hooks (Tcl): `::tkdroid::snapshot file.ppm`, `touch down|up|move x y`, `key down|up name`, `screen`, `resize w h`.
Not implemented yet: selection/clipboard, menus' platform side, `tk busy`, rotated text, IME composition, multi-touch. See docs/tk-android-work-list.md for the original work list (all 195 functions are now implemented; some, like selection, are placeholders).
