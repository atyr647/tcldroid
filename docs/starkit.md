# Starkits and Starpacks on tcldroid

A Starkit is a Tcl app in one file. tcldroid packs them the same way it packs a folder:

    tools/pack-app.sh MyApp.kit --name "My App" --id com.example.myapp

The kit is **unpacked when you pack it**, and its files become the app folder (`main.tcl`,
`lib/…`). Nothing is mounted at run time, so a kit costs no start-up time and there is no
virtual file system to go wrong on a phone.

| What you have | Works? | How |
|---|---|---|
| Tcl 9 zipkit (a zip, with or without a `#!/bin/sh` stub) | yes | `pack-app.sh MyApp.kit` |
| Starpack made with Tcl 9 `zipfs mkimg` (executable + zip) | yes | the same; the executable stub is dropped |
| A `MyApp.vfs` folder (what `sdx` wraps) | yes | `pack-app.sh MyApp.vfs` — it is just a folder |
| A zip whose only top-level folder holds `main.tcl` | yes | that folder becomes the root |
| Metakit starkit (`mk4vfs`, what `sdx wrap` made by default up to Tcl 8.6) | **no** | convert once, below |

## Converting a Metakit kit

On any desktop with a Tcl 8.6 tclkit: `sdx unwrap MyApp.kit` makes `MyApp.vfs/`. Pass that folder
to `pack-app.sh`. The packager recognises a Metakit file and says so rather than failing
obscurely. Reading Metakit at pack time (no desktop step) is feasible, because it would be a build-machine
script and not something that runs on the phone. It is **not built**: I could not obtain a real
Metakit kit or the format's source in the build environment, and a reader for a binary format
written from memory without real files to test against would not be trustworthy. It needs one or
two real `.kit` files (for example `sdx.kit`, plus one of your own) as the test oracle; the reader
would then be checked against `sdx unwrap` output and would refuse, never guess, on anything it
cannot verify (every file's recorded size is checked).

## What a kit's main.tcl sees

`package require starkit` works and gives:

- `::starkit::topdir` — the app folder; `::starkit::infoscript` — its `main.tcl`;
- `::starkit::mode` — `starkit`;
- `starkit::startup` — puts `topdir` and `topdir/lib` on `auto_path`;
- `starkit::autoextend dir`.

Packages under `lib/` (including `lib/app-name/` with a `pkgIndex.tcl`) load as usual.

## Not provided

- The `vfs`, `vfs::zip` and `mk4vfs` packages: a kit that mounts other archives itself must use
  Tcl 9's own `zipfs mount` instead.
- Starpack executables for other systems: the stub is thrown away, only the payload is used.
- Compiled extensions inside a kit (`.so`/`.dll`): see the limits in packaging.md.
- Kits that write into their own folder: the app folder is replaced when a new build is installed;
  keep data under `$env(HOME)`.

## Checked

`tests/tk/kit.tcl` builds a zipkit, unpacks it, runs its `main.tcl` against the shim, and checks
that a Metakit file and a zip with an entry climbing out of the folder (`../x`) are refused.
`pack-app.sh MyApp.kit` was run end to end and the resulting APK holds the kit's files.
