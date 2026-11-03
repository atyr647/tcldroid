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
# toybox is Android's coreutils (cat, ls, echo, ... are symlinks to it) and its shell.
for f in lib64/libz.so lib64/liblog.so bin/sh bin/toybox; do
  debugfs -R "dump /system/$f $ROOT/system/$f" "$SYS" 2>/dev/null
done
# Name resolution: Bionic reads /system/etc/hosts (maps localhost); without it
# `socket localhost` fails with "No address associated with hostname".
mkdir -p "$ROOT/system/etc"
debugfs -R "dump /system/etc/hosts $ROOT/system/etc/hosts" "$SYS" 2>/dev/null
chmod +x "$ROOT/system/bin/sh" "$ROOT/system/bin/toybox"
for a in cat cp date dirname echo env expr false grep head id kill ln ls mkdir mktemp mv printf pwd rm rmdir sed sleep sort stat tail tee test touch tr true uname wc which xargs chmod chown cut basename readlink diff cmp od hostname; do
  ln -sf toybox "$ROOT/system/bin/$a"
done
# Same symlinks the real image has.
ln -s /apex/com.android.runtime/bin/linker64 "$ROOT/system/bin/linker64"
for l in libc libm libdl; do ln -s /apex/com.android.runtime/lib64/bionic/$l.so "$ROOT/system/lib64/$l.so"; done
# Resolve toybox's shared-library dependencies recursively from the image
# (/system/lib64, else the Runtime APEX already extracted above).
declare -A seen=([libc.so]=1 [libm.so]=1 [libdl.so]=1)
queue=("$ROOT/system/bin/toybox")
while [ ${#queue[@]} -gt 0 ]; do
  f="${queue[0]}"; queue=("${queue[@]:1}")
  for l in $(readelf -d "$f" 2>/dev/null | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p'); do
    [ -z "${seen[$l]:-}" ] || continue; seen[$l]=1
    if [ -e "$ROOT/system/lib64/$l" ]; then queue+=("$ROOT/system/lib64/$l"); continue; fi
    if [ -e "$ROOT/apex/com.android.runtime/lib64/$l" ]; then
      ln -s "/apex/com.android.runtime/lib64/$l" "$ROOT/system/lib64/$l"
      queue+=("$ROOT/apex/com.android.runtime/lib64/$l"); continue
    fi
    if debugfs -R "dump /system/lib64/$l $ROOT/system/lib64/$l" "$SYS" 2>/dev/null && [ -s "$ROOT/system/lib64/$l" ]; then
      queue+=("$ROOT/system/lib64/$l")
    else
      rm -f "$ROOT/system/lib64/$l"; log "warning: $l not found in image (needed by $(basename "$f"))"
    fi
  done
done
# Android's init generates /linkerconfig/ld.config.txt at boot. Without it every
# process prints a linker warning on stderr, which breaks anything that checks
# stderr (e.g. Tcl's `exec` tests). Minimal, non-isolated config:
mkdir -p "$ROOT/linkerconfig"
cat > "$ROOT/linkerconfig/ld.config.txt" <<'LDCFG'
dir.system = /system/bin/
dir.tcl = /data/local/tcl/

[system]
namespace.default.search.paths = /system/lib64
namespace.default.search.paths += /apex/com.android.runtime/lib64/bionic

[tcl]
namespace.default.search.paths = /data/local/tcl/lib
namespace.default.search.paths += /system/lib64
namespace.default.search.paths += /apex/com.android.runtime/lib64/bionic
LDCFG
log "Bionic root ready: $ROOT ($(du -sh "$ROOT" | cut -f1))"
