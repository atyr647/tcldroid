#!/usr/bin/env bash
# Exercise the Android host's Tcl embedding on the build machine (no device).
#
# Builds android-host/.../tcldroid_core.c against the *host* Tcl and runs
# selftest.tcl through it, with libtcl9.0.so loaded from a relocated directory
# (as an app loads it from nativeLibraryDir) and with no LANG/TCL_LIBRARY set.
# This validates the embedding logic (app hook + zipfs mount, std channels,
# environment, encoding) but NOT Bionic, JNI, or the Activity.
. "$(dirname "$0")/../tools/env.sh"
"$TCLDROID_ROOT/tools/build-host-tcl.sh"

CPP="$TCLDROID_ROOT/android-host/app/src/main/cpp"
HOST="$BUILD_DIR/install/host"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
mkdir -p "$T/lib" "$T/files" "$T/cache"
cp "$HOST/lib/libtcl9.0.so" "$T/lib/"

gcc -std=gnu11 -Wall -Wextra -I"$HOST/include" -I"$CPP" \
    "$TCLDROID_ROOT/tests/host/embed_test.c" "$CPP/tcldroid_core.c" \
    -L"$T/lib" -ltcl9.0 -Wl,-rpath,"$T/lib" -o "$T/embed_test"

env -i PATH="$PATH" "$T/embed_test" "$T/lib" "$T/files" "$T/cache" "${TZ_NAME:-Europe/Berlin}" \
    "$TCLDROID_ROOT/android-host/app/src/main/assets/selftest.tcl"
