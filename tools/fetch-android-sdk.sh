#!/usr/bin/env bash
# Install just the Android SDK pieces Gradle needs into .cache/android-sdk
# (platform + build-tools). Skipped if ANDROID_HOME already provides them.
. "$(dirname "$0")/env.sh"

SDK="${ANDROID_HOME:-$CACHE_DIR/android-sdk}"
if [ ! -d "$SDK/platforms/android-$SDK_PLATFORM_API" ]; then
  fetch "$SDK_PLATFORM_URL" "$CACHE_DIR/$(basename "$SDK_PLATFORM_URL")" sha1 "$SDK_PLATFORM_SHA1"
  tmp="$(mktemp -d)"; unzip -q "$CACHE_DIR/$(basename "$SDK_PLATFORM_URL")" -d "$tmp"
  mkdir -p "$SDK/platforms"; mv "$tmp"/* "$SDK/platforms/android-$SDK_PLATFORM_API"; rmdir "$tmp"
fi
if [ ! -d "$SDK/build-tools/$SDK_BUILD_TOOLS" ]; then
  fetch "$SDK_BUILD_TOOLS_URL" "$CACHE_DIR/$(basename "$SDK_BUILD_TOOLS_URL")" sha1 "$SDK_BUILD_TOOLS_SHA1"
  tmp="$(mktemp -d)"; unzip -q "$CACHE_DIR/$(basename "$SDK_BUILD_TOOLS_URL")" -d "$tmp"
  mkdir -p "$SDK/build-tools"; mv "$tmp"/* "$SDK/build-tools/$SDK_BUILD_TOOLS"; rmdir "$tmp"
fi
log "Android SDK ready: $SDK"
echo "$SDK"
