#!/usr/bin/env bash
# Run every file of Tcl's test suite on its own and record per-file totals, to
# diff host vs Android:   tests/tcltest-perfile.sh host|android
# Output: build/perfile-<mode>.tsv   (file total passed skipped failed)
. "$(dirname "$0")/../tools/env.sh"
mode="${1:?usage: $0 host|android}"
"$TCLDROID_ROOT/tools/fetch-upstream.sh"
SRC_TESTS="$BUILD_DIR/src/tcl$TCL_VERSION/tests"
OUT="$BUILD_DIR/perfile-$mode.tsv"

# Run one file in its own copy of the tests dir and print one TSV line.
# (Shared by both modes; written to be valid for toybox sh as well.)
RUNNER='
f="$1"; w="$TESTROOT/w.$$"; rm -rf "$w"; mkdir -p "$w"; cp -r "$TESTROOT/tests" "$w/tests"
cd "$w/tests"; r=$("$TCLTEST" "$f" 2>&1 | grep "^$f:" | tail -1 | tr "\t" " ")
echo "$f $r" | sed "s/ Total / /;s/ Passed / /;s/ Skipped / /;s/ Failed / /;s/^\([^ ]*\) [^ ]*: */\1 /"
cd /; rm -rf "$w"'

case "$mode" in
host)
  B="$BUILD_DIR/host/tcl"; [ -x "$B/tcltest" ] || make -C "$B" tcltest >/dev/null
  W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; mkdir -p "$W/home" "$W/tmp"; cp -r "$SRC_TESTS" "$W/tests"
  (cd "$SRC_TESTS" && ls *.test) | \
    xargs -P4 -I{} env -i PATH=/usr/bin:/bin LANG=C.UTF-8 LD_LIBRARY_PATH="$B" HOME="$W/home" TMPDIR="$W/tmp" \
      TESTROOT="$W" TCLTEST="$B/tcltest" sh -c "$RUNNER" sh {} | sort > "$OUT"
  ;;
android)
  [ "$(id -u)" = 0 ] || die "android mode needs root"
  ROOT="$BUILD_DIR/bionic-root"; INST="$BUILD_DIR/install/android-x86_64"; B="$BUILD_DIR/android-x86_64/tcl"
  [ -x "$ROOT/system/bin/linker64" ] || "$TCLDROID_ROOT/tools/bionic-root.sh"
  [ -x "$B/tcltest" ] || make -C "$B" tcltest >/dev/null
  umount_all() { for m in dev proc; do mountpoint -q "$ROOT/$m" && umount "$ROOT/$m"; done; return 0; }
  trap umount_all EXIT
  mount -t proc proc "$ROOT/proc"; mount --bind /dev "$ROOT/dev"
  TCL="$ROOT/data/local/tcl"; rm -rf "$TCL" "$ROOT/data/local/tmp"
  mkdir -p "$TCL" "$ROOT/data/local/tmp/home" "$ROOT/data/local/tmp/tmp" "$ROOT/data/local/tmp/work"
  cp -a "$INST/bin" "$INST/lib" "$TCL/"; cp "$B/tcltest" "$TCL/bin/"; cp -a "$SRC_TESTS" "$ROOT/data/local/tmp/work/tests"
  printf '%s\n' "$RUNNER" > "$ROOT/data/local/tmp/runner.sh"
  (cd "$SRC_TESTS" && ls *.test) > "$ROOT/data/local/tmp/files.txt"
  env -i PATH=/system/bin LD_LIBRARY_PATH=/data/local/tcl/lib HOME=/data/local/tmp/home TMPDIR=/data/local/tmp/tmp \
      TESTROOT=/data/local/tmp/work TCLTEST=/data/local/tcl/bin/tcltest \
      "$(command -v chroot)" "$ROOT" /system/bin/sh -c \
      'while read f; do sh /data/local/tmp/runner.sh "$f"; done < /data/local/tmp/files.txt' | sort > "$OUT"
  ;;
*) die "unknown mode" ;;
esac
echo "output: $OUT ($(wc -l < "$OUT") files)"
