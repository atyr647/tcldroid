#!/usr/bin/env bash
# Build the Android host app (debug APK) with the pinned NDK/SDK, packaging the
# Tcl libraries built by tools/build-tcl-android.sh.
#   usage: tools/build-android-host.sh [gradle task ...]     (default: assembleDebug)
# The Tcl/Tk app inside the APK is data, chosen by environment variables
# (tools/pack-app.sh is the friendly front end):
#   APP_DIR      folder with main.tcl (+ files, optional icon.png)   default examples/demo
#   APP_NAME     launcher label                                      default "Tcl Host"
#   APP_ID       Android applicationId                               default dev.tcldroid.host
#   APP_VERSION  versionName                                         default 0.5.0
#   APP_CODE     versionCode                                         default 10
#   APP_KEYSTORE signing key file (else the Android debug key); with TCLDROID_KS_PASS,
#                TCLDROID_KEY_ALIAS, TCLDROID_KEY_PASS in the environment
#   APP_BUILD    debug (default) | release (not debuggable; needs APP_KEYSTORE)
#   APP_AAB      1: build an Android App Bundle (release) instead of an APK
. "$(dirname "$0")/env.sh"

# Build Tcl only if a library is missing (set FORCE_TCL=1 to rebuild).
need_tcl=${FORCE_TCL:-}
for abi in $ANDROID_ABIS; do [ -f "$BUILD_DIR/install/android-$abi/lib/libtcl9.0.so" ] || need_tcl=1; done
[ -z "$need_tcl" ] || "$TCLDROID_ROOT/tools/build-tcl-android.sh"
"$TCLDROID_ROOT/tools/fetch-ndk.sh"
SDK="$("$TCLDROID_ROOT/tools/fetch-android-sdk.sh" | tail -1)"
command -v cmake >/dev/null && command -v ninja >/dev/null || die "cmake and ninja are required"

# Tk (with its script library appended) for each ABI.
for abi in $ANDROID_ABIS; do "$TCLDROID_ROOT/tools/build-tk.sh" "$abi" >/dev/null; done

# Stage only what the app ships from the Tcl and Tk builds: the core library
# (script library is appended to it as a zip; see android-host/app/build.gradle).
JNILIBS="$BUILD_DIR/jniLibs"
for abi in $ANDROID_ABIS; do
  mkdir -p "$JNILIBS/$abi"
  cp "$BUILD_DIR/install/android-$abi/lib/libtcl9.0.so" "$JNILIBS/$abi/"
  cp "$BUILD_DIR/tk-$abi/libtk9.0.so" "$JNILIBS/$abi/"
done

cat > "$TCLDROID_ROOT/android-host/local.properties" <<PROPS
sdk.dir=$SDK
cmake.dir=$(dirname "$(dirname "$(command -v cmake)")")
PROPS

# Stage the app: its files go to assets/app/, its icon to res/drawable/app_icon.png.
APP_DIR=${APP_DIR:-$TCLDROID_ROOT/examples/demo}
APP_NAME=${APP_NAME:-Tcl Host}
APP_ID=${APP_ID:-dev.tcldroid.host}
APP_VERSION=${APP_VERSION:-0.5.0}
APP_CODE=${APP_CODE:-10}
APP_DIR=$(cd "$APP_DIR" && pwd) || die "APP_DIR not found"
[ -f "$APP_DIR/main.tcl" ] || die "$APP_DIR/main.tcl not found"
STAGE="$BUILD_DIR/app-stage"
rm -rf "$STAGE"; mkdir -p "$STAGE/assets/app" "$STAGE/res/drawable"
cp -a "$APP_DIR/." "$STAGE/assets/app/"
rm -rf "$STAGE/assets/app/.git"
if [ -f "$APP_DIR/icon.png" ]; then cp "$APP_DIR/icon.png" "$STAGE/res/drawable/app_icon.png"
else cp "$TCLDROID_ROOT/android-host/res-default/drawable/app_icon.png" "$STAGE/res/drawable/app_icon.png"; fi

APP_BUILD=${APP_BUILD:-debug}
if [ -n "${APP_KEYSTORE:-}" ]; then
  [ -f "$APP_KEYSTORE" ] || die "keystore not found: $APP_KEYSTORE"
  APP_KEYSTORE=$(cd "$(dirname "$APP_KEYSTORE")" && pwd)/$(basename "$APP_KEYSTORE")
  [ -n "${TCLDROID_KS_PASS:-}" ] && [ -n "${TCLDROID_KEY_ALIAS:-}" ] || die "set TCLDROID_KS_PASS and TCLDROID_KEY_ALIAS for the keystore"
fi
[ "$APP_BUILD" != release ] && [ -z "${APP_AAB:-}" ] || [ -n "${APP_KEYSTORE:-}" ] || die "release builds and bundles need a keystore (tools/make-keystore.sh)"
tasks=("$@")
if [ ${#tasks[@]} -eq 0 ]; then
  if [ -n "${APP_AAB:-}" ]; then tasks=(bundleRelease)
  elif [ "$APP_BUILD" = release ]; then tasks=(assembleRelease)
  else tasks=(assembleDebug); fi
fi
cd "$TCLDROID_ROOT/android-host"
./gradlew --no-daemon "${tasks[@]}" \
  -Ptcldroid.ndk="$NDK_HOME" \
  -Ptcldroid.ndkVersion="$NDK_REVISION" \
  -Ptcldroid.jniLibs="$JNILIBS" \
  -Ptcldroid.tclRoot="$BUILD_DIR/install" \
  -Ptcldroid.tkRoot="$BUILD_DIR" \
  -Ptcldroid.tkAndroid="$TCLDROID_ROOT/tk-android/android" \
  -Ptcldroid.appAssets="$STAGE/assets" \
  -Ptcldroid.appRes="$STAGE/res" \
  -Ptcldroid.appId="$APP_ID" \
  -Ptcldroid.appName="$APP_NAME" \
  -Ptcldroid.appVersion="$APP_VERSION" \
  -Ptcldroid.appCode="$APP_CODE" \
  ${APP_KEYSTORE:+-Ptcldroid.keystore="$APP_KEYSTORE"}
mkdir -p "$TCLDROID_ROOT/out"
safe=$(printf %s "$APP_NAME" | tr -c 'A-Za-z0-9._-' '-' | sed 's/--*/-/g;s/^-//;s/-$//')
if [ -n "${APP_AAB:-}" ]; then
  APK="$TCLDROID_ROOT/android-host/app/build/outputs/bundle/release/app-release.aab"
  OUT="$TCLDROID_ROOT/out/${safe:-app}-$APP_VERSION.aab"
else
  APK="$TCLDROID_ROOT/android-host/app/build/outputs/apk/$APP_BUILD/app-$APP_BUILD.apk"
  OUT="$TCLDROID_ROOT/out/${safe:-app}-$APP_VERSION.apk"
fi
[ ! -f "$APK" ] || cp "$APK" "$OUT"
[ "$APP_ID" != dev.tcldroid.host ] || [ "$APP_BUILD" != debug ] || [ -n "${APP_AAB:-}" ] || cp "$APK" "$TCLDROID_ROOT/out/tcldroid-host-debug.apk"
log "APK: $OUT"
