# tk-android

The Tk `android/` platform tree (docs/adr/0002-tk-backend-architecture.md). See [`android/README.md`](android/README.md) for the port as a Tk maintainer would read it. Copied into the Tk source tree by `tools/fetch-upstream.sh` / `tools/build-tk.sh`.

| File | Role |
| --- | --- |
| `tkAndroid.h` | Public host interface: callbacks, text-engine interface, thread-safe input queues. No Tk internals. |
| `tkAndroidPort.h`, `tkAndroidInt.h`, `tkAndroidDefault.h`, `tkAndroidPriv.h` | Platform headers, selected by `TK_ANDROID` in generic code (upstream patch series). |
| `tkAndroidWindow.c` | The in-process "X server": window tree, stacking, mapping, exposure, pixmaps, clipping. |
| `tkAndroidDraw.c` | Software rasteriser behind the Xlib-shaped drawing API; images; text blit. |
| `tkAndroidRegion.c` | Regions as rectangle lists. |
| `tkAndroidEvent.c` | Tcl event source, touch/key/back queues, focus, pointer, keysyms, `tkdroid::` test hooks. |
| `tkAndroidWm.c` | Window manager (`.` fills the screen, other toplevels are layers; Back; `wm` subcommands). |
| `tkAndroidFont.c`, `tkAndroidTestEngine.c` | Fonts on a pluggable text engine; FreeType (host tests) and a box-glyph fallback. |
| `tkAndroidColor.c`, `tkAndroidDisplay.c` | Colours, cursors, property stubs; display, init, library mount, trivia. |
| `tkAndroidSelect.c` | Selection and clipboard (system clipboard through the host). |
| `../library/android.tcl` | Touch policy in Tcl: drag-scroll, long-press selection and toolbar, pinch (see docs/touch-and-clipboard.md). |

Build: `tools/build-tk.sh host` (fast loop: FreeType text, `TKDROID_SCREEN=1080x2200@420`) or `tools/build-tk.sh arm64-v8a|x86_64`.
Test hooks (Tcl): `::tkdroid::snapshot file.ppm`, `touch down|up|move x y`, `key down|up name`, `screen`, `resize w h`.
Not implemented yet: menus' platform side, `tk busy`, rotated text, IME composition, selection handles. See docs/tk-android-work-list.md for the original work list (all 195 functions are now implemented; some, like selection, are placeholders).
