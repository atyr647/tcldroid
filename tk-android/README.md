# tk-android

The new Tk `android/` platform tree (see docs/adr/0002-tk-backend-architecture.md).

`android/` is copied into the Tk source tree by `tools/fetch-upstream.sh`:

- `tkAndroidPort.h` — the platform header (modelled on the macOS one).
- `tkUnixInt.h`, `tkUnixDefault.h` — Android versions that *shadow* the Unix headers by include-path order, so upstream's generic code stays unpatched.

**Status:** only headers exist. The platform layer (195 functions, 12 families) is listed in [docs/tk-android-work-list.md](../docs/tk-android-work-list.md); regenerate it with `tools/survey-tk-android.sh && tools/gen-tk-worklist.py`.
