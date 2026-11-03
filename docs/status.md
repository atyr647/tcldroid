# Status (what is true today)

Last updated: 2026-11-03

| Milestone | State |
| --- | --- |
| M0 reproducible build skeleton | **Partly verified.** Pinned inputs download and checksum-verify from a clean state (NDK r29 SHA-1 matches Google's manifest; Tcl/Tk 9.0.4 hashes recorded). The native host Tcl build and the Android cross-build via `tools/build-tcl-android.sh` had **not finished** when this was written, so **no Android binary has been produced or run yet**. |
| M1 headless Tcl + core tests | not started. There is no emulator/KVM in the dev container, so on-device tests need another environment. |
| M2–M6 | not started |

## Known issues in the scripts
- Written and run only on Linux x86_64.
- Not yet verified that the bundled packages (`itcl`, `tdbc`, `thread`, `sqlite3`) cross-compile; if they do not, set `TCL_CONFIGURE_EXTRA` (or add a flag to the script) to skip them.
- No CI workflow yet (nothing to run it against until the build is confirmed).
