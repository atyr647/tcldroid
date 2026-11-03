# Dependency inventory

**Verified** = I read the licence file in the pinned artefact. **To verify** = not yet checked; do not rely on it. All pins live in [`upstream/versions.env`](../upstream/versions.env).

## Build inputs (pinned)

| Dependency | Version | Used for | Licence | Verified |
| --- | --- | --- | --- | --- |
| Tcl | 9.0.4 | runtime core | BSD-style (`license.terms`) | ✅ |
| Tk | 9.0.4 | GUI toolkit; basis for the Android backend | BSD-style (`license.terms`; differs from Tcl's by a notice naming Apple Inc.) | ✅ |
| Android NDK | r29 (29.0.14206865) | clang/Bionic cross toolchain | Apache-2.0 + LLVM + Bionic (BSD) components | ⬜ to verify (Google's NDK licence page) |
| JDK + Gradle + Android Gradle Plugin | JDK 17+, Gradle 8.14 available in the dev container; AGP version TBD | APK/AAB, Phase 2+ | Apache-2.0 | ⬜ |

## Bundled inside the Tcl 9.0.4 tarball

| Component | Version | Notes |
| --- | --- | --- |
| zlib | 1.3.2 (`compat/zlib`) | used by zipfs; zlib licence |
| sqlite3 | 3.53.0 (`pkgs/`) | public domain |
| itcl | 4.3.8 | Tcl licence |
| tdbc (+ sqlite3 / mysql / odbc / postgres drivers) | 1.1.13 | the mysql/odbc/postgres drivers need external client libraries; **will not be built for Android** unless asked |
| thread | 3.0.6 | Tcl licence |

## Reference projects to audit in Phase 0 (not yet reviewed)

| Project | Why it matters | State |
| --- | --- | --- |
| **AndroWish** (Christian Werner) | The only prior Android Tk effort; upstream `tclZipfs.c` already contains an `ANDROID` branch that came from this lineage. Must be audited for design lessons and *licence compatibility* before any code is reused. It is SDL-based, which the roadmap rules out as an architecture, so expect lessons, not code. | ⬜ **not audited**; I could not fetch it in this session |
| **Tclkit / kitgen** | Bootstrap and VFS layout | ⬜ |
| **SDX** | Starkit/Starpack wrap/unwrap | ⬜ |
| **TclVFS / `vfs::zip`** | zip VFS for pre-9 kits | ⬜ |
| **Metakit / `mk4vfs`** | Format of most existing Starkits (see ADR 0001 decision gate) | ⬜ licence and portability to Bionic |

## Android platform APIs expected (Phase 2+)

`SurfaceView`/`Surface`, `android.graphics.Canvas`/`Paint`/`Bitmap` (via JNI), `Choreographer`, `InputConnection`/`InputMethodManager`, `ClipboardManager`, Storage Access Framework, `AccessibilityNodeProvider`. No third-party UI or graphics libraries are planned (FreeType/HarfBuzz only if ADR 0002's fallback B is taken).
