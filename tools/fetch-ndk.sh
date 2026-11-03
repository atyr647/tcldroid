#!/usr/bin/env bash
# Install the pinned Android NDK into .cache/ (skipped if ANDROID_NDK_HOME is set).
. "$(dirname "$0")/env.sh"

if [ -n "${ANDROID_NDK_HOME:-}" ]; then
  log "using ANDROID_NDK_HOME=$ANDROID_NDK_HOME"
else
  fetch "$NDK_URL" "$CACHE_DIR/android-ndk-$NDK_RELEASE-linux.zip" sha1 "$NDK_SHA1"
  if [ ! -d "$NDK_HOME" ]; then
    log "unpacking NDK $NDK_RELEASE"
    unzip -q "$CACHE_DIR/android-ndk-$NDK_RELEASE-linux.zip" -d "$CACHE_DIR"
  fi
fi

rev="$(sed -n 's/^Pkg.Revision *= *//p' "$NDK_HOME/source.properties")"
[ "$rev" = "$NDK_REVISION" ] || log "warning: NDK revision is $rev, pinned is $NDK_REVISION"
log "NDK ready: $NDK_HOME ($rev)"
