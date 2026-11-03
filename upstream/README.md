# upstream

- `versions.env` — the single source of truth for pinned Tcl, Tk and NDK inputs and their checksums.
- `patches/` — our patch series against pristine upstream. `tools/fetch-upstream.sh` applies `tcl.series` / `tk.series` (one patch filename per line, `#` comments allowed) after unpacking. Keep patches small, one concern each, in the shape an upstream reviewer would accept.

Current patches (applied in order by `tools/fetch-upstream.sh`, listed in `tk.series`):

- `0001-tkPort-select-android-port.patch` — `generic/tkPort.h`: include `tkAndroidPort.h` on Android, and (PROVISIONALLY) select Tk's Xlib-declaration section; see ADR 0002.
- `0002-tkPointer-include-android-platform-header.patch` — `generic/tkPointer.c`: include the platform header on Android.

Tcl itself needs **no patches**: unmodified Tcl 9.0.4 cross-builds and runs on Android (see docs/status.md). New (non-patch) Tk files live in `tk-android/android/` and are copied into the Tk tree by `tools/fetch-upstream.sh`.
