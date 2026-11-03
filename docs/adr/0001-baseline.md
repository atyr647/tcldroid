# ADR 0001 — Pinned baseline: versions, Android targets, build system

- Status: **Proposed** (Phase 0). Needs owner sign-off on the two items marked ⚠.
- Date: 2026-11-03

## Decision

| Item | Choice |
| --- | --- |
| Tcl / Tk | **9.0.4** (release tarballs, hashes in `upstream/versions.env`) |
| NDK | **r29** (`29.0.14206865`), SHA-1 checked against Google's manifest |
| Minimum API | **26** (Android 8.0) |
| ABIs (first-class) | **arm64-v8a**, **x86_64** (emulators/ChromeOS). `armeabi-v7a`/`x86` deferred |
| Page size | Link with `-Wl,-z,max-page-size=16384` so libraries load on 16 KB-page devices |
| Tcl/Tk build | Upstream **autoconf** `configure`/`make`, cross-compiled by `tools/*.sh` |
| App host | **Java** Activity + JNI, built with Gradle (Android Gradle Plugin) — Phase 2 |
| Tk backend build | A new `android/` directory in the Tk tree, in the style of `unix/`, `win/`, `macosx/` (overlay kept in `tk-android/`, applied via `upstream/patches/`) |

## Why Tcl/Tk 9.0, not 8.6

- The roadmap's goal of **upstreaming** points at the current development line. 9.x is where new platform work lands; 8.6 is maintenance-only.
- Tcl 9 ships **zipfs** in the core, and `generic/tclZipfs.c` in 9.0.4 *already contains an `#ifdef ANDROID` path* (mounting an APK, which is a zip, and remapping `AndroidManifest.xml`, `resources.arsc`, `classes.dex`). It also aligns appended payloads the way Android's `zipalign` does. That is a large head start for Phase 4/5.
- Tcl 9's `epoll` notifier (`unix/tclEpollNotfy.c`) is a good fit for Bionic.

### ⚠ The cost, stated plainly

Existing Starkits/Starpacks in the wild are overwhelmingly **Tcl 8.x + Metakit (`mk4vfs`)**. A Tcl 9 core:

- cannot read Metakit-based kits without an additional read-only `mk4vfs`/Metakit port, and
- breaks some 8.6 scripts and **all 8.6 binary extensions** (they must be rebuilt for 9.x; `Tcl_Size`, UTF-8 default for `source`, removed commands/options).

The roadmap's **M4 ("existing compatible Starkit payload runs without edits")** is therefore *not free* with this baseline. **Decision gate at the start of Phase 4:** support (a) zip-based kits via zipfs, (b) Metakit kits via a ported read-only reader, and/or (c) fall back to an 8.6 build of the runtime. Phase 0 should survey a handful of real Starkits you care about to size this; if most of them are Metakit/8.6, reconsider the baseline **before** Phase 2 invests in a 9.x-only backend.

## Why these Android choices

- **API 26**: broad coverage of devices still in use, and a floor high enough to avoid most legacy workarounds. NDK r29 itself supports API 21+, so this is a product choice, easily lowered.
- **arm64-v8a + x86_64**: all current phones/tablets are 64-bit; x86_64 is what CI emulators and ChromeOS run. 32-bit adds test burden for little reach.
- **16 KB pages**: newer Android versions can run with 16 KB pages; unaligned ELF segments fail to load. It costs nothing to do from day one.
- **Autoconf for Tcl/Tk, not CMake/ndk-build**: upstreamable patches must fit upstream's build. Gradle is used only for the app wrapper.
- **Java, not Kotlin**: the host is a thin Activity/`SurfaceView`/`InputConnection` shim around JNI; Java adds no plugin and keeps JNI signatures simple. Revisit if the host grows.

## Open items (not decided here)

- ⚠ Project licence. Tcl/Tk are under a BSD-style licence, which is the natural fit for upstream contribution, but this repo has no `LICENSE` file yet because that is your decision to make.
- Physical test devices (need at least one arm64 phone and one tablet) and CI emulator provider.
- Cross-check the SourceForge tarball hashes against hashes published by core.tcl-lang.org before any upstream submission. (Note: the gzip header inside the 9.0.4 tarballs names `…9.0.4rc2-src.tar`, while `tcl.h`/`tk.h` report `9.0.4`; the pin is on the exact bytes we tested.)
