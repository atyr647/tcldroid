#!/usr/bin/env bash
# Check that the submission series (upstream/series/*.patch) applied to PRISTINE Tk gives exactly the tree
# this repository builds and tests (pristine + upstream/patches + the tk-android overlay).
. "$(dirname "$0")/../env.sh"
S="$BUILD_DIR/series-verify"; rm -rf "$S"; mkdir -p "$S"
tar -xzf "$CACHE_DIR/tk$TK_VERSION-src.tar.gz" -C "$S"
T="$S/tk$TK_VERSION"
for p in "$TCLDROID_ROOT"/upstream/series/*.patch; do
  patch -s -d "$T" -p1 < "$p" || die "series patch does not apply cleanly: $p"
done
DEV="$BUILD_DIR/src/tk$TK_VERSION"
bad=0
for d in generic unix xlib win macosx library android bitmaps doc tests; do
  [ -d "$T/$d" ] || continue
  diff -rq -x '*.orig' -x '*.rej' -x a11y.tcl -x android.tcl -x starkit.tcl "$T/$d" "$DEV/$d" >"$S/diff.$d" 2>&1 || { bad=1; echo "DIFFERS in $d:"; head -5 "$S/diff.$d"; }
done
for f in android.tcl a11y.tcl starkit.tcl; do cmp -s "$T/library/$f" "$TCLDROID_ROOT/tk-android/library/$f" || { bad=1; echo "DIFFERS: library/$f"; }; done
[ $bad = 0 ] || die "series and the development tree differ"
log "series verified: pristine Tk $TK_VERSION + upstream/series == the tree under test"
