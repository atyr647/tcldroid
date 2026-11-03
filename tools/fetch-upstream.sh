#!/usr/bin/env bash
# Download, verify and unpack the pinned Tcl and Tk sources into build/src/.
. "$(dirname "$0")/env.sh"

fetch "$TCL_URL" "$CACHE_DIR/tcl$TCL_VERSION-src.tar.gz" sha256 "$TCL_SHA256"
fetch "$TK_URL"  "$CACHE_DIR/tk$TK_VERSION-src.tar.gz"  sha256 "$TK_SHA256"

mkdir -p "$BUILD_DIR/src"
for p in tcl tk; do
  v=$([ $p = tcl ] && echo "$TCL_VERSION" || echo "$TK_VERSION")
  if [ ! -d "$BUILD_DIR/src/$p$v" ]; then
    log "unpacking $p$v"
    tar -xzf "$CACHE_DIR/$p$v-src.tar.gz" -C "$BUILD_DIR/src"
    # Apply our patch series (if any) on top of the pristine tree.
    if [ -f "$TCLDROID_ROOT/upstream/patches/$p.series" ]; then
      while read -r patch; do
        [ -n "$patch" ] && [ "${patch#\#}" = "$patch" ] || continue
        log "applying $patch to $p$v"
        patch -d "$BUILD_DIR/src/$p$v" -p1 < "$TCLDROID_ROOT/upstream/patches/$patch"
      done < "$TCLDROID_ROOT/upstream/patches/$p.series"
    fi
  fi
done
log "sources ready in $BUILD_DIR/src"
