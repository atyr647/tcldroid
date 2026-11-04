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
    "$J/TclRuntime.java" "$J/ImeBridge.java" "$TCLDROID_ROOT"/tests/jvm/src/dev/tcldroid/host/*.java 2>&1 | grep -v "^Note:" || true
cp "$TCLDROID_ROOT/android-host/app/src/main/assets/selftest.tcl" "$W/files/assets/"
SCRIPT="${1:-$TCLDROID_ROOT/examples/demo/main.tcl}"
cd "$W"
touch "$W/files/run-selftest"	# the demo then runs its Tcl self-test and logs the verdict (as tests/android-smoke.sh does)
LD_LIBRARY_PATH="$W/lib" TZ=UTC timeout 60 "$JH/bin/java" -Xcheck:jni -Dfirst.ppm="$W/first.ppm" -Djava.awt.headless=true -Djava.library.path="$W/lib" \
    -cp classes dev.tcldroid.host.Harness "$W/lib" "$W/files" "$SCRIPT" "$W/frame.ppm" 2>&1 | tee "$W/demo.out" | grep -v "^\[java\] caret"
echo "exit status: ${PIPESTATUS[0]}"
case "$SCRIPT" in */examples/demo/main.tcl)
  grep -q "TCLDROID_SMOKE: PASS" "$W/demo.out" && echo "PASS demo-selftest-hook" || { echo "FAIL demo-selftest-hook"; grep "SMOKE\|FAIL" "$W/demo.out" | head -5; } ;;
esac

# Clipboard, long press, toolbar and pinch through the real JNI layer.
if [ "${GESTURES:-1}" = 1 ]; then
  OUT="$W/gestures.out"
  LD_LIBRARY_PATH="$W/lib" TZ=UTC timeout 60 "$JH/bin/java" -Xcheck:jni -Djava.awt.headless=true -Djava.library.path="$W/lib" \
      -cp classes dev.tcldroid.host.GestureHarness "$W/lib" "$W/files" "$TCLDROID_ROOT/tests/jvm/gestures.tcl" > "$OUT" 2>&1
  grep -v "caret\|JAVA_TOOL\|^\sat \|WARNING in native" "$OUT"
  fail=0
  expect() { grep -q -- "$1" "$OUT" && echo "PASS $2" || { echo "FAIL $2"; fail=1; }; }
  expect '\[java\] clipboard <- from tcl' "clipboard-write-reaches-host"
  expect 'PROBE clip=from tcl' "clipboard-read-own-text"
  expect 'PROBE clip=other app' "clipboard-external-change-seen"
  expect 'TEXTMENU [0-9]*,[0-9]* flags=\(15\|11\|10\|14\|9\|8\)' "long-press-shows-toolbar"
  expect 'clipboard <- hello' "toolbar-copy-puts-word-on-clipboard"
  expect 'PROBE clip=hello sel=hello' "selection-survives-and-clipboard-readable"
  expect 'PINCH start 1.000' "pinch-start"
  expect 'PINCH update 1.500' "pinch-update"
  expect 'PINCH end 2.000' "pinch-end"
  expect '\[java\] REQUEST 1 pickfile | types=image/png,image/jpeg ; multiple=1' "request-pickfile-args"
  expect '\[java\] REQUEST 2 openurl | url=https://example.org/x' "request-openurl"
  expect '\[java\] REQUEST 3 permission | permissions=android.permission.CAMERA,android.permission.VIBRATE' "request-permission"
  expect '\[java\] REQUEST 4 share | text=hello there' "request-share"
  expect '\[java\] REQUEST 5 savefile | source=.*save-src.txt ; name=my notes.txt ; type=text/plain' "request-savefile"
  expect '\[tcl\] RESULT pick ok 2 /cache/picked/1/a.png|/cache/picked/1/b b.png' "result-pickfile-paths"
  expect '\[tcl\] RESULT url error 1 no app can open this link' "result-openurl-error"
  expect '\[tcl\] RESULT perm denied 1 android.permission.VIBRATE' "result-permission-denied"
  expect '\[tcl\] RESULT share ok 0' "result-share"
  expect '\[tcl\] RESULT save cancelled 0' "result-savefile-cancelled"
  expect 'PERMITTED 1 0' "permitted-sync"
  expect 'MODE code text code 1' "inputmode-command"
  expect '\[java\] INPUTMODE 1' "inputmode-reaches-host"
  expect 'ERR 1 pickfile needs -command' "pickfile-requires-command"
  expect 'ERR 1 share needs -text or -file' "share-requires-content"
  expect 'ERR 1 bad option "-bogus"' "bad-option-rejected"
  expect '\[java\] VIBRATE 30 amp=128' "vibrate"
  expect '\[java\] HAPTIC 3' "haptic-confirm"
  expect 'SOUND preload .*beep.wav' "sound-preload"
  expect 'SOUND play .*beep.wav vol=500 rate=1500' "sound-play-volume-rate"
  expect '\[java\] SOUND stop' "sound-stop"
  expect '\[java\] BELL' "bell"
  expect 'INFO sdk=35 model=Harness files=/x y android=1' "info"
  expect 'ERR 1 couldn.t read sound file' "sound-missing-file-error"
  expect 'ERR 1 milliseconds must be' "vibrate-range-error"
  expect '\[java\] FULLSCREEN true' "fullscreen"
  expect '\[tcl\] LIFE pause' "lifecycle-pause-event"
  expect '\[tcl\] LIFE save' "lifecycle-savestate-event"
  expect '\[java\] LIFECYCLE done 0' "lifecycle-pause-acknowledged"
  expect '\[tcl\] LIFE resume' "lifecycle-resume-event"
  expect '\[tcl\] LIFE lowmem' "lifecycle-lowmemory-event"
  [ "$(grep -c '^\[tcl\] PROBE' "$OUT")" = 3 ] && echo "PASS cancel-is-not-a-click" || { echo "FAIL cancel-is-not-a-click"; fail=1; }
  [ $fail = 0 ] || exit 1
fi

# The first presented frame is the finished screen (start-up hold), not an early blank one.
if [ "${GESTURES:-1}" = 1 ]; then
  rm -f "$W/hold-first.ppm"
  LD_LIBRARY_PATH="$W/lib" TZ=UTC timeout 60 "$JH/bin/java" -Djava.awt.headless=true -Djava.library.path="$W/lib" \
      -Dfirst.ppm="$W/hold-first.ppm" -cp "$W/classes" dev.tcldroid.host.Harness "$W/lib" "$W/files" \
      "$TCLDROID_ROOT/tests/jvm/hold.tcl" "$W/hold-last.ppm" > "$W/hold.out" 2>&1
  python3 -I - "$W/hold-first.ppm" <<'PY' && echo "PASS first-frame-is-finished-ui" || { echo "FAIL first-frame-is-finished-ui"; exit 1; }
import sys
d = open(sys.argv[1], 'rb').read()
hdr, w, h, _ = d.split(None, 4)[0:4] if False else (None, None, None, None)
parts = d.split(b'\n', 3)
w, h = map(int, parts[1].split())
px = parts[3]
i = ((h // 2) * w + w // 2) * 3
r, g, b = px[i], px[i + 1], px[i + 2]
sys.exit(0 if (g > 100 and r < 50 and b < 50) else 1)
PY
fi

# Keyboard composition logic (pure Java)
"$JH/bin/java" -cp "$W/classes" dev.tcldroid.host.ImeTest || exit 1
