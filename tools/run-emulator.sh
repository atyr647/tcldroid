#!/usr/bin/env bash
# Fetch the pinned emulator + x86_64 system image + adb, create an AVD, boot it
# headless, and wait until Android is ready. Works WITHOUT KVM (software
# emulation: boot takes many minutes) as well as with it.
#   usage: tools/run-emulator.sh            # boots, prints the adb serial, leaves it running
# Env: EMULATOR_ACCEL=off|auto (default: auto -> off when /dev/kvm is absent)
. "$(dirname "$0")/env.sh"

SDK="$("$TCLDROID_ROOT/tools/fetch-android-sdk.sh" | tail -1)"
unpack() {  # unpack URL SHA1 DESTDIR  (zip's single top-level dir becomes DESTDIR)
  [ -d "$3" ] && return 0
  fetch "$1" "$CACHE_DIR/$(basename "$1")" sha1 "$2"
  local tmp; tmp="$(mktemp -d)"; unzip -q "$CACHE_DIR/$(basename "$1")" -d "$tmp"
  mkdir -p "$(dirname "$3")"; mv "$tmp"/* "$3"; rmdir "$tmp"
}
unpack "$EMULATOR_URL"       "$EMULATOR_SHA1"       "$SDK/emulator"
unpack "$PLATFORM_TOOLS_URL" "$PLATFORM_TOOLS_SHA1" "$SDK/platform-tools"
unpack "$SYSIMG_URL"         "$SYSIMG_SHA1"         "$SDK/system-images/android-$SYSIMG_API/default/x86_64"

export ANDROID_HOME="$SDK" ANDROID_SDK_ROOT="$SDK"
export ANDROID_AVD_HOME="$BUILD_DIR/avd" ANDROID_USER_HOME="$BUILD_DIR/android-user"
AVD="$ANDROID_AVD_HOME/tcldroid.avd"
if [ ! -d "$AVD" ]; then
  mkdir -p "$AVD"
  printf 'avd.ini.encoding=UTF-8\npath=%s\ntarget=android-%s\n' "$AVD" "$SYSIMG_API" > "$ANDROID_AVD_HOME/tcldroid.ini"
  cat > "$AVD/config.ini" <<CFG
AvdId=tcldroid
avd.ini.encoding=UTF-8
abi.type=x86_64
hw.cpu.arch=x86_64
hw.cpu.ncore=4
hw.ramSize=2048
hw.lcd.width=720
hw.lcd.height=1280
hw.lcd.density=320
hw.keyboard=yes
hw.gpu.enabled=yes
hw.gpu.mode=swiftshader_indirect
disk.dataPartition.size=3G
image.sysdir.1=system-images/android-$SYSIMG_API/default/x86_64/
tag.id=default
tag.display=Default Android System Image
PlayStore.enabled=false
CFG
fi

ACCEL="${EMULATOR_ACCEL:-auto}"
[ "$ACCEL" = auto ] && { [ -w /dev/kvm ] && ACCEL=on || ACCEL=off; }
log "booting emulator (accel=$ACCEL; software emulation is slow, be patient)"
mkdir -p "$BUILD_DIR/emulator-logs"
# Without hardware acceleration the emulator refuses to start unless explicitly told software emulation is wanted.
[ "$ACCEL" = off ] && export ANDROID_I_WANT_MY_TCG=yes
nohup "$SDK/emulator/emulator" -avd tcldroid -no-window -no-audio -no-boot-anim \
    -no-snapshot -no-metrics -accel "$ACCEL" -gpu swiftshader_indirect \
    > "$BUILD_DIR/emulator-logs/emulator.log" 2>&1 &
echo $! > "$BUILD_DIR/emulator.pid"

ADB="$SDK/platform-tools/adb"
"$ADB" start-server >/dev/null
EMU_PID="$(cat "$BUILD_DIR/emulator.pid")"
until [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ]; do
  kill -0 "$EMU_PID" 2>/dev/null || { tail -20 "$BUILD_DIR/emulator-logs/emulator.log" >&2; die "emulator exited before boot completed"; }
  sleep 10
done
log "Android is up: $("$ADB" shell getprop ro.build.version.release | tr -d '\r') (API $("$ADB" shell getprop ro.build.version.sdk | tr -d '\r')), $("$ADB" shell uname -m | tr -d '\r')"
echo "$ADB"
