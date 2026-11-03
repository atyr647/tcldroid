# Status (what is true today)

Last updated: 2026-11-03

| Milestone | State |
| --- | --- |
| M0 reproducible builds | **Built, not yet run.** `tools/build-tcl-android.sh` takes a clean checkout to `tclsh9.0` + `libtcl9.0.so` for **arm64-v8a** and **x86_64** with **zero patches to upstream Tcl 9.0.4** (NDK r29, API 26). |
| M1 headless Tcl + core tests | **Not started.** The binaries have never been executed: this dev container has no KVM/emulator and cannot run Bionic binaries. Next step needs an environment with an emulator or device. |
| M2–M6 | not started |

## What was verified (by inspecting the build output)
- Both `tclsh9.0` are ELF PIE executables for the right architecture, using `/system/bin/linker64`, linking only Bionic libraries (`libc`, `libm`, `libdl`, `libz`).
- Every `PT_LOAD` segment is 16 KB aligned (`0x4000`).
- zipfs is enabled and the Tcl script library is embedded (`//zipfs:/lib/tcl/tcl_library`).
- Bundled packages built: `itcl`, `thread`, `sqlite3`, `tdbc` and its sqlite driver. The `tdbcmysql`/`tdbcodbc`/`tdbcpostgres` drivers also compiled but need external client libraries that do not exist on Android, so they are useless there (candidates to exclude).

## Not verified
- Anything at runtime: startup, `init.tcl` from zipfs, threads, sockets, `load`, clock/timezone, Unicode filenames.
- The compiler emitted ~30 warnings in the x86_64 build; not yet reviewed.

## Known issues / next steps
1. Run `tclsh9.0` on an emulator or arm64 device (smoke test, then Tcl's own test suite) — the gate for M1.
2. Exclude the three useless tdbc drivers from the Android build.
3. Strip symbols for release builds (current outputs are unstripped).
4. Add CI (GitHub Actions has KVM-capable runners for an x86_64 emulator) once step 1 is scripted.
5. Scripts are only tested on Linux x86_64.
