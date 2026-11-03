#!/usr/bin/env bash
# Run the REAL JNI layer (tcldroid_jni.c, tcldroid_core.c) and the real TclRuntime.java on a
# host JVM with -Xcheck:jni, using Java stand-ins for the Android classes. It exercises
# the Tcl thread attach, listener callbacks, frame transfer, the text-engine bridge, touch,
# keys, resize and Back. Also saves the very first frame (build/jvm-test/first.ppm), but NOT Android's Bitmap/View/IME. Catches JNI misuse and native crashes.
#   usage: tests/jvm-jni.sh [script.tcl]     (default: the demo)   output: build/jvm-test/frame.ppm
. "$(dirname "$0")/../tools/env.sh"
"$TCLDROID_ROOT/tools/build-tk.sh" host >/dev/null
JH="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")}"
W="$BUILD_DIR/jvm-test"; rm -rf "$W"; mkdir -p "$W/classes" "$W/files/assets" "$W/lib"
HOST="$BUILD_DIR/install/host"; CPP="$TCLDROID_ROOT/android-host/app/src/main/cpp"
cp "$HOST/lib/libtcl9.0.so" "$BUILD_DIR/tk-host/libtk9.0.so" "$W/lib/"
gcc -shared -fPIC -g -Wall -Wextra -DTCLDROID_HOST_JVM -I"$JH/include" -I"$JH/include/linux" -I"$TCLDROID_ROOT/tests/jvm/shim" \
    -I"$HOST/include" -I"$TCLDROID_ROOT/tk-android/android" "$CPP/tcldroid_jni.c" "$CPP/tcldroid_core.c" \
    -L"$W/lib" -ltk9.0 -ltcl9.0 -lpthread -Wl,-rpath,"$W/lib" -o "$W/lib/libtcldroid.so"
J="$TCLDROID_ROOT/android-host/app/src/main/java/dev/tcldroid/host"
"$JH/bin/javac" -nowarn -d "$W/classes" "$TCLDROID_ROOT"/tests/jvm/stubs/android/content/Context.java \
    "$TCLDROID_ROOT"/tests/jvm/stubs/android/content/pm/ApplicationInfo.java \
    "$J/TclRuntime.java" "$TCLDROID_ROOT"/tests/jvm/src/dev/tcldroid/host/*.java 2>&1 | grep -v "^Note:" || true
cp "$TCLDROID_ROOT/android-host/app/src/main/assets/selftest.tcl" "$W/files/assets/"
SCRIPT="${1:-$TCLDROID_ROOT/android-host/app/src/main/assets/demo.tcl}"
cd "$W"
LD_LIBRARY_PATH="$W/lib" TZ=UTC timeout 60 "$JH/bin/java" -Xcheck:jni -Dfirst.ppm="$W/first.ppm" -Djava.awt.headless=true -Djava.library.path="$W/lib" \
    -cp classes dev.tcldroid.host.Harness "$W/lib" "$W/files" "$SCRIPT" "$W/frame.ppm"
echo "exit status: $?"
