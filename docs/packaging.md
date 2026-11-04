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

Launch screen: `--splash '#0B0F1A'` sets its colour (white by default); the app icon is centred on it.

## Signing and updates

```
tools/make-keystore.sh my-key.jks                    # once; back the file up
TCLDROID_KS_PASS=... tools/pack-app.sh myapp --keystore my-key.jks --key-alias key0 --code 2
```

Without `--keystore` the APK carries the Android debug key: fine for your own phone, but an app
installs **over** an older one only if both use the same `--id`, the same key, and a higher
`--code`. `--release` builds a non-debuggable APK and `--aab` an Android App Bundle (both need a
keystore). Passwords are read from the environment or asked for, never taken from the command
line. The key that signed an *existing* app (for example the AndroWish-based build) is needed to
update that app in place; without it, ship the new build under a new `--id` and move user data over.

The unpacked app folder is kept between launches and replaced only when a different build of the
app is installed.

## Limits (be aware)

- Release signing and AAB output exist but are not yet verified against a Play Store upload.
- Starkits/Starpacks (Metakit-based `.kit`) are **not supported yet**; ship plain folders or zip
  your own scripts. Tcl 9's `zipfs` works for your own zips.
- Third-party compiled extensions are not supported yet (only pure-Tcl packages plus those in Tcl/Tk).
- Only tested on one phone plus host-side harnesses.
