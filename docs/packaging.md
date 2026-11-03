# Packaging a Tcl/Tk app as an Android APK

```
tools/pack-app.sh APPDIR [--name "My App"] [--id com.example.myapp] [--version 1.0] [--code 1]
```

`APPDIR` is an ordinary Tcl/Tk app folder. It must contain `main.tcl`; everything else in the
folder ships too (extra `.tcl` files, data, `lib/` packages). An optional `icon.png` becomes the
launcher icon. The APK appears in `out/<Name>-<version>.apk`.

Try it: `tools/pack-app.sh examples/notes --name Notes` (installs next to the demo host app
because its id is different).

## What the app sees at run time

- The app folder is unpacked to private storage on every launch and made the current directory;
  it and `lib/` are on `auto_path`. `$::tkdroid::appdir` holds the path.
- **Do not write user data into the app folder** (it is replaced each launch). Use
  `$env(HOME)` (the app's private files directory) and `$env(TMPDIR)`.
- `main.tcl` runs like `wish main.tcl`: after it returns, the app stays in the Tk event loop until
  `.` is destroyed or `exit` is called.
- A Tcl error in `main.tcl` shows a dialog with the full stack trace and a Copy button.
- Back runs your `wm protocol . WM_DELETE_WINDOW` handler; with no handler the app closes.
- Keep a distinct `--id` per app and raise `--code` for each update you install over an old one.

## Limits (be aware)

- APKs are **debug-signed** (installable, not Play-Store-ready). Release signing/AAB is not done.
- Starkits/Starpacks (Metakit-based `.kit`) are **not supported yet**; ship plain folders or zip
  your own scripts. Tcl 9's `zipfs` works for your own zips.
- Third-party compiled extensions are not supported yet (only pure-Tcl packages plus those in Tcl/Tk).
- Only tested on one phone plus host-side harnesses.
