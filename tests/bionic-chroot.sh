#!/usr/bin/env bash
# Run the Android x86_64 Tcl build under REAL Bionic (Android's libc + linker)
# on a plain Linux x86_64 host: no emulator, no KVM. Needs root (chroot, mount).
#
#   tests/bionic-chroot.sh                 # selftest.tcl with an empty environment
#   tests/bionic-chroot.sh script.tcl      # run any script with the Android tclsh
#
# Covers Tcl on Bionic (loader, libc, locale, sockets, zlib, exec, zipfs mount).
# Does NOT cover JNI, the Activity, app namespaces or the Android framework.
. "$(dirname "$0")/../tools/env.sh"
[ "$(id -u)" = 0 ] || die "needs root (chroot, mount)"

ROOT="$BUILD_DIR/bionic-root"
INST="$BUILD_DIR/install/android-x86_64"
[ -f "$INST/lib/libtcl9.0.so" ] || "$TCLDROID_ROOT/tools/build-tcl-android.sh" x86_64
[ -e "$ROOT/apex/com.android.runtime/bin/linker64" ] || "$TCLDROID_ROOT/tools/bionic-root.sh"

umount_all() {
  for m in dev proc; do mountpoint -q "$ROOT/$m" && umount "$ROOT/$m"; done
  return 0
}
trap umount_all EXIT
mount -t proc proc "$ROOT/proc"
mount --bind /dev "$ROOT/dev"

TCL="$ROOT/data/local/tcl"
rm -rf "$TCL"; mkdir -p "$TCL" "$ROOT/data/local/tmp/files" "$ROOT/data/local/tmp/cache"
cp -a "$INST/bin" "$INST/lib" "$TCL/"
# Extra files for the script, visible in the chroot as /data/local/tmp/files/<name>.
[ -z "${BIONIC_FILES:-}" ] || cp $BIONIC_FILES "$ROOT/data/local/tmp/files/"
SCRIPT="${1:-$TCLDROID_ROOT/android-host/app/src/main/assets/selftest.tcl}"
cp "$SCRIPT" "$TCL/script.tcl"

# Empty environment, like an app process: no LANG, no TCL_LIBRARY, no PATH.
# (The linker's "linkerconfig" warnings are expected: there is no Android init here.)
set +e
env -i LD_LIBRARY_PATH=/data/local/tcl/lib \
    HOME=/data/local/tmp/files TMPDIR=/data/local/tmp/cache TZ="${TZ_NAME:-Europe/Berlin}" \
    "$(command -v chroot)" "$ROOT" /data/local/tcl/bin/tclsh9.0 /data/local/tcl/script.tcl \
    2>&1 | grep -v "linkerconfig\|linker config" | tee "$BUILD_DIR/bionic-chroot.out"
status=${PIPESTATUS[0]}
[ -n "${1:-}" ] || grep -q "TCLDROID_SMOKE: PASS" "$BUILD_DIR/bionic-chroot.out" || status=1
exit "$status"
