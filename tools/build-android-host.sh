#!/usr/bin/env bash
# Build the Android host app (debug APK) with the pinned NDK/SDK, packaging the
# Tcl libraries built by tools/build-tcl-android.sh.
#   usage: tools/build-android-host.sh [gradle task ...]     (default: assembleDebug)
. "$(dirname "$0")/env.sh"

"$TCLDROID_ROOT/tools/build-tcl-android.sh"       # idempotent enough: reuses finished trees
SDK="$("$TCLDROID_ROOT/tools/fetch-android-sdk.sh" | tail -1)"
command -v cmake >/dev/null && command -v ninja >/dev/null || die "cmake and ninja are required"

# Stage only what the app ships from the Tcl build: the core library
# (script library is appended to it as a zip; see android-host/app/build.gradle).
JNILIBS="$BUILD_DIR/jniLibs"
for abi in $ANDROID_ABIS; do
  mkdir -p "$JNILIBS/$abi"
  cp "$BUILD_DIR/install/android-$abi/lib/libtcl9.0.so" "$JNILIBS/$abi/"
done

cat > "$TCLDROID_ROOT/android-host/local.properties" <<PROPS
sdk.dir=$SDK
cmake.dir=$(dirname "$(dirname "$(command -v cmake)")")
PROPS

tasks=("$@"); [ ${#tasks[@]} -gt 0 ] || tasks=(assembleDebug)
cd "$TCLDROID_ROOT/android-host"
./gradlew --no-daemon "${tasks[@]}" \
  -Ptcldroid.ndk="$NDK_HOME" \
  -Ptcldroid.jniLibs="$JNILIBS" \
  -Ptcldroid.tclRoot="$BUILD_DIR/install"
log "APK: $TCLDROID_ROOT/android-host/app/build/outputs/apk/debug/app-debug.apk"
