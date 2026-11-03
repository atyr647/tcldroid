#!/usr/bin/env bash
# Run Tcl's OWN test suite (tests/all.tcl of the pinned Tcl) with the `tcltest`
# executable (the shell plus Tcl's test-only commands).
#   tests/tcltest.sh android [extra all.tcl args]   # Android x86_64 build on real Bionic (root, chroot)
#   tests/tcltest.sh host    [extra all.tcl args]   # same suite on the host build: the BASELINE
# Compare the two: failures present only under `android` are Android-specific;
# failures in both come from the environment (running as root, containers, ...).
# Output: build/tcltest-<mode>.out.   Extra args example:  -file 'string*.test'
. "$(dirname "$0")/../tools/env.sh"
mode="${1:?usage: tests/tcltest.sh android|host [all.tcl args]}"; shift || true
"$TCLDROID_ROOT/tools/fetch-upstream.sh"
OUT="$BUILD_DIR/tcltest-$mode.out"
SRC_TESTS="$BUILD_DIR/src/tcl$TCL_VERSION/tests"

case "$mode" in
host)
  W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
  B="$BUILD_DIR/host/tcl"
  [ -x "$B/tcltest" ] || make -C "$B" tcltest >/dev/null
  cp -a "$SRC_TESTS" "$W/tests"; mkdir -p "$W/home" "$W/tmp"
  cd "$W/tests"
  env -i PATH=/usr/bin:/bin LANG=C.UTF-8 LD_LIBRARY_PATH="$B" HOME="$W/home" TMPDIR="$W/tmp" \
      "$B/tcltest" all.tcl "$@" > "$OUT" 2>&1
  ;;
android)
  [ "$(id -u)" = 0 ] || die "android mode needs root (chroot, mount)"
  ROOT="$BUILD_DIR/bionic-root"; INST="$BUILD_DIR/install/android-x86_64"
  B="$BUILD_DIR/android-x86_64/tcl"
  [ -f "$INST/lib/libtcl9.0.so" ] || "$TCLDROID_ROOT/tools/build-tcl-android.sh" x86_64
  [ -e "$ROOT/apex/com.android.runtime/bin/linker64" ] || "$TCLDROID_ROOT/tools/bionic-root.sh"
  [ -x "$B/tcltest" ] || make -C "$B" tcltest >/dev/null
  umount_all() { for m in dev proc; do mountpoint -q "$ROOT/$m" && umount "$ROOT/$m"; done; return 0; }
  trap umount_all EXIT
  mount -t proc proc "$ROOT/proc"; mount --bind /dev "$ROOT/dev"
  TCL="$ROOT/data/local/tcl"; rm -rf "$TCL" "$ROOT/data/local/tmp"
  mkdir -p "$TCL" "$ROOT/data/local/tmp/home" "$ROOT/data/local/tmp/tmp"
  cp -a "$INST/bin" "$INST/lib" "$TCL/"; cp "$B/tcltest" "$TCL/bin/"
  # load.test/unload.test/pkgMkIndex.test look for dltest/ next to the interpreter.
  [ -d "$B/dltest" ] && cp -a "$B/dltest" "$TCL/bin/dltest"
  cp -a "$SRC_TESTS" "$TCL/tests"
  timeout "${TCLTEST_TIMEOUT:-3000}" env -i PATH=/system/bin LD_LIBRARY_PATH=/data/local/tcl/lib \
      HOME=/data/local/tmp/home TMPDIR=/data/local/tmp/tmp TCLLIBPATH=/data/local/tcl/lib \
      "$(command -v chroot)" "$ROOT" /system/bin/sh -c \
      'cd /data/local/tcl/tests && exec /data/local/tcl/bin/tcltest all.tcl "$@"' sh "$@" \
      2>&1 | grep -v "linkerconfig\|linker config" > "$OUT"
  ;;
*) die "unknown mode: $mode" ;;
esac
echo "output: $OUT"; grep -E "^all\.tcl.*Total|^Sourced [0-9]+ Test Files|Files with failing tests|Number of tests skipped" "$OUT" | tail -5
