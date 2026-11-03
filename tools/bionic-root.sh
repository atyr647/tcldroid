#!/usr/bin/env bash
# Assemble a minimal Android userland (real Bionic libc, linker64, libz, ...) in
# build/bionic-root from the pinned x86_64 system image, so Android x86_64
# binaries can run under chroot on a plain Linux x86_64 host -- no emulator, no
# KVM. Covers Bionic/linker/dladdr/locale behaviour of Tcl; NOT the Android
# framework (no JNI/Activity). Needs root (chroot) and e2fsprogs (debugfs).
. "$(dirname "$0")/env.sh"
command -v debugfs >/dev/null || die "debugfs (e2fsprogs) is required"

SYSIMG="$CACHE_DIR/android-sdk/system-images/android-$SYSIMG_API/default/x86_64/system.img"
if [ ! -f "$SYSIMG" ]; then
  # Same pinned download run-emulator.sh uses; only the image is needed here.
  fetch "$SYSIMG_URL" "$CACHE_DIR/$(basename "$SYSIMG_URL")" sha1 "$SYSIMG_SHA1"
  mkdir -p "$(dirname "$SYSIMG")"; tmp="$(mktemp -d)"
  unzip -q "$CACHE_DIR/$(basename "$SYSIMG_URL")" -d "$tmp"; mv "$tmp"/*/* "$(dirname "$SYSIMG")"/ 2>/dev/null || mv "$tmp"/* "$(dirname "$SYSIMG")"/
fi
# Byte offset of the `system` logical partition inside the `super` partition of
# the pinned image (x86_64-34_r04); found by parsing the dynamic-partition (LP) metadata.
SYSTEM_OFFSET=3145728
SYS="$SYSIMG?offset=$SYSTEM_OFFSET"

ROOT="$BUILD_DIR/bionic-root"
# SAFETY: never rm -rf a tree that still has /dev or /proc bind-mounted into it.
if grep -q " $ROOT/" /proc/mounts; then die "unmount everything under $ROOT first (see tests/bionic-chroot.sh)"; fi
rm -rf --one-file-system "$ROOT"; mkdir -p "$ROOT"/{system/bin,system/lib64,apex,dev,proc,tmp,data/local/tcl}
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT

# Runtime APEX: Bionic libc/libm/libdl and linker64 live here on modern Android.
debugfs -R "dump /system/apex/com.android.runtime.apex $T/runtime.apex" "$SYS" 2>/dev/null
unzip -q "$T/runtime.apex" apex_payload.img -d "$T"
mkdir -p "$ROOT/apex/com.android.runtime"
for d in bin lib64; do debugfs -R "rdump /$d $ROOT/apex/com.android.runtime" "$T/apex_payload.img" 2>/dev/null; done

# Platform libs a Tcl build links against, plus a shell for `exec` tests.
for f in lib64/libz.so lib64/liblog.so bin/sh; do
  debugfs -R "dump /system/$f $ROOT/system/$f" "$SYS" 2>/dev/null
done
chmod +x "$ROOT/system/bin/sh"
# Same symlinks the real image has.
ln -s /apex/com.android.runtime/bin/linker64 "$ROOT/system/bin/linker64"
for l in libc libm libdl; do ln -s /apex/com.android.runtime/lib64/bionic/$l.so "$ROOT/system/lib64/$l.so"; done
ln -s /apex/com.android.runtime/lib64/libc++.so "$ROOT/system/lib64/libc++.so"
log "Bionic root ready: $ROOT ($(du -sh "$ROOT" | cut -f1))"
