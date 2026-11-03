# tcldroid — native Tcl/Tk and Starpack packaging for Android

Goal: run ordinary Tcl/Tk applications (and package Starkits/Starpacks as installable APK/AABs) on Android using a **real native port** of Tcl and Tk — no PRoot, Termux, X11/SDL shim or replacement toolkit.

The full plan is in [`docs/roadmap.txt`](docs/roadmap.txt). **Current state: Phase 0 (baseline & design), in progress.** See [`docs/status.md`](docs/status.md) for exactly what works today.

## Layout

| Dir | Purpose | Phase |
| --- | --- | --- |
| [`upstream/`](upstream/) | Pinned Tcl/Tk (and tool) versions + checksums, our patch series | 0 |
| [`tools/`](tools/) | Reproducible fetch/build scripts | 0 |
| [`runtime/`](runtime/) | Android Tcl + Tclkit integration | 1, 4 |
| [`tk-android/`](tk-android/) | The new `android/` platform backend for Tk | 2 |
| [`android-host/`](android-host/) | Activity, JNI bridge, Gradle project | 2–3 |
| [`packager/`](packager/) | Starkit/Starpack → APK/AAB | 5 |
| [`extensions/`](extensions/) | Android builds of native Tcl packages | 4 |
| [`tests/`](tests/) | Regression + device tests | 1+ |
| [`examples/`](examples/) | Unmodified Tcl/Tk demo apps | 2+ |
| [`docs/`](docs/) | ADRs, compatibility policy/matrix, dependency inventory | 0+ |

## Build (headless Tcl for Android — Milestone M0)

Requirements on the build machine (Linux x86_64): `bash`, `curl`, `unzip`, `tar`, `make`, a native C compiler, `sha1sum`/`sha256sum`. Nothing else: the script downloads and verifies the pinned NDK.

```sh
tools/build-tcl-android.sh            # arm64-v8a and x86_64
tools/build-tcl-android.sh arm64-v8a  # one ABI
```

Output lands in `build/install/android-<abi>/` (gitignored). Set `ANDROID_NDK_HOME` to reuse an existing NDK; set `TCL_CONFIGURE_EXTRA` to pass extra `configure` flags.

## Decisions

- [ADR 0001 — baseline versions & Android targets](docs/adr/0001-baseline.md)
- [ADR 0002 — Tk backend architecture](docs/adr/0002-tk-backend-architecture.md)
- [Compatibility policy](docs/compatibility-policy.md) · [matrix](docs/compatibility-matrix.md)
- [Dependency inventory](docs/dependency-inventory.md)

Two decisions are waiting on the project owner; they are marked ⚠ in the ADRs.
