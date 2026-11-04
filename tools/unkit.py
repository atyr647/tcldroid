#!/usr/bin/env python3
"""Unpack a zip-based Starkit/Starpack (.kit, .exe, .zip) into a folder for tools/pack-app.sh.

usage: unkit.py KITFILE OUTDIR

A zip-based kit is a zip archive, possibly after a script or executable stub; Python finds the
archive from the end of the file. The kit must hold main.tcl at its top (a Starkit does), or in a
single top-level folder (a "MyApp.vfs" style zip), which is then used as the root.
A Metakit kit (what sdx wrap makes with the default writer) is detected and refused with the way out.
Exit status: 0 unpacked, 1 not usable (the reason is printed), 2 usage.
"""
import os, shutil, sys, zipfile

def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    kit, out = sys.argv[1:]
    with open(kit, 'rb') as f:
        head = f.read(8192)
    try:
        z = zipfile.ZipFile(kit)
    except zipfile.BadZipFile:
        if b'mk4vfs' in head or b'Mk4tcl' in head or head.startswith(b'#!/bin/sh'):
            print(f"{kit} looks like a Metakit starkit (mk4vfs), which tcldroid does not read.\n"
                  "Convert it on a desktop, once, with either:\n"
                  "  sdx unwrap " + os.path.basename(kit) + "      # then pack the MyApp.vfs folder it makes\n"
                  "  tclkit sdx.kit unwrap ...                       (same thing)\n"
                  "and pass that folder to pack-app.sh. See docs/starkit.md.", file=sys.stderr)
        else:
            print(f"{kit}: not a zip archive and not a recognised starkit.", file=sys.stderr)
        return 1
    names = [n for n in z.namelist() if not n.endswith('/')]
    prefix = ''
    if 'main.tcl' not in names:
        tops = {n.split('/', 1)[0] for n in names if '/' in n}
        cand = [t for t in tops if (t + '/main.tcl') in names]
        if len(cand) == 1 and all(n.startswith(cand[0] + '/') for n in names):
            prefix = cand[0] + '/'
        else:
            print(f"{kit}: no main.tcl at the top of the archive; not a Starkit.", file=sys.stderr)
            return 1
    os.makedirs(out, exist_ok=True)
    root = os.path.realpath(out)
    count = 0
    for info in z.infolist():
        if info.is_dir() or not info.filename.startswith(prefix):
            continue
        rel = info.filename[len(prefix):]
        dest = os.path.realpath(os.path.join(root, rel))
        if not dest.startswith(root + os.sep):
            print(f"{kit}: refusing entry outside the folder: {info.filename}", file=sys.stderr)
            return 1
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with z.open(info) as src, open(dest, 'wb') as dst:
            shutil.copyfileobj(src, dst)
        count += 1
    print(f"unpacked {count} files from {os.path.basename(kit)}")
    return 0

sys.exit(main())
