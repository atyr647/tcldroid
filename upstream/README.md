# upstream

- `versions.env` — the single source of truth for pinned Tcl, Tk and NDK inputs and their checksums.
- `patches/` — our patch series against pristine upstream. `tools/fetch-upstream.sh` applies `tcl.series` / `tk.series` (one patch filename per line, `#` comments allowed) after unpacking. Keep patches small, one concern each, in the shape an upstream reviewer would accept.

There are currently **no patches**. Whether unmodified Tcl 9.0.4 cross-builds for Android is tracked in docs/status.md.
