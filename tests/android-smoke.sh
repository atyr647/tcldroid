#!/usr/bin/env bash
# Install the host APK on the connected device/emulator, launch it, and wait for
# the self-test verdict printed by android-host/app/src/main/assets/selftest.tcl.
#   usage: tests/android-smoke.sh [path/to/app-debug.apk]
# Requires `adb` and exactly one connected device. Exit status: 0 = PASS.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APK="${1:-$ROOT/android-host/app/build/outputs/apk/debug/app-debug.apk}"
PKG=dev.tcldroid.host
TIMEOUT="${SMOKE_TIMEOUT:-120}"

[ -f "$APK" ] || { echo "APK not found: $APK" >&2; exit 2; }
adb get-state >/dev/null

adb install -r "$APK" >/dev/null
adb shell pm clear "$PKG" >/dev/null 2>&1 || true
adb logcat -c
adb shell am start -n "$PKG/.MainActivity" >/dev/null

deadline=$((SECONDS + TIMEOUT))
verdict=""
while [ $SECONDS -lt $deadline ]; do
  verdict="$(adb logcat -d -s tcldroid:V | grep -o 'TCLDROID_SMOKE: .*' | tail -1 || true)"
  [ -n "$verdict" ] && break
  sleep 2
done

echo "---- tcldroid log ----"
adb logcat -d -s tcldroid:V AndroidRuntime:E DEBUG:F
echo "----------------------"
case "$verdict" in
  "TCLDROID_SMOKE: PASS"*) echo "$verdict"; exit 0 ;;
  "") echo "no verdict within ${TIMEOUT}s (crash? check the log above)" >&2; exit 1 ;;
  *)  echo "$verdict" >&2; exit 1 ;;
esac
