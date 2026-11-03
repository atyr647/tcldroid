#!/usr/bin/env bash
# Cross-compile upstream Tcl for one Android ABI with the NDK (no source patches
# unless listed in upstream/patches/tcl.series).
#   usage: tools/build-tcl-android.sh [abi ...]      (default: all ANDROID_ABIS)
. "$(dirname "$0")/env.sh"

"$TCLDROID_ROOT/tools/fetch-ndk.sh"
"$TCLDROID_ROOT/tools/build-host-tcl.sh"

TOOLCHAIN="$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64"
[ -d "$TOOLCHAIN" ] || die "NDK toolchain not found at $TOOLCHAIN"
TCLSH_NATIVE="$BUILD_DIR/install/host/bin/tclsh9.0"

abis=("$@"); [ ${#abis[@]} -gt 0 ] || read -r -a abis <<<"$ANDROID_ABIS"

for abi in "${abis[@]}"; do
  triple="$(abi_triple "$abi")"
  prefix="$BUILD_DIR/install/android-$abi"
  work="$BUILD_DIR/android-$abi/tcl"
  mkdir -p "$work"; cd "$work"

  export CC="$TOOLCHAIN/bin/${triple}${ANDROID_MIN_API}-clang"
  export AR="$TOOLCHAIN/bin/llvm-ar" RANLIB="$TOOLCHAIN/bin/llvm-ranlib"
  export STRIP="$TOOLCHAIN/bin/llvm-strip" NM="$TOOLCHAIN/bin/llvm-nm"
  export TCLSH_NATIVE

  # Bionic does not let a dlopen()ed library borrow symbols (trunc, cos, ...) from the
  # executable's libm: package libraries must list -lm themselves (LIBS=-lm below), or
  # e.g. `package require sqlite3` fails with 'cannot locate symbol "trunc"'.
  rm -rf "$work/pkgs"    # packages are configured at first `make`; force it to see LIBS
  log "[$abi] configuring Tcl $TCL_VERSION (minSdk $ANDROID_MIN_API)"
  # Android defines __ANDROID__ itself; ANDROID is what tclZipfs.c tests for.
  "$BUILD_DIR/src/tcl$TCL_VERSION/unix/configure" \
      --host="$triple" --prefix="$prefix" \
      --enable-shared --disable-rpath \
      CFLAGS="-DANDROID -fPIC" LDFLAGS="-Wl,-z,max-page-size=16384" \
      LIBS="-lm" \
      ${TCL_CONFIGURE_EXTRA:-} >configure.log 2>&1 \
    || { tail -30 configure.log; die "[$abi] configure failed (see $work/configure.log)"; }

  log "[$abi] building (log: $work/make.log)"
  make -j"$(nproc)" >make.log 2>&1 || { tail -40 make.log; die "[$abi] make failed"; }
  make install >install.log 2>&1 || { tail -20 install.log; die "[$abi] install failed"; }
  log "[$abi] installed to $prefix"
done
