#!/usr/bin/env bash
# Phase 2 survey: compile every platform-INDEPENDENT Tk source (generic/, generic/ttk/,
# xlib/) for one Android ABI against tk-android/android/tkAndroidPort.h, then compute
# which symbols are referenced but defined nowhere (not by Tk itself, not by Bionic).
# That residue is, by construction, what the Android platform layer must provide.
#   usage: tools/survey-tk-android.sh [abi]     (default arm64-v8a)
# Output: build/survey/<abi>/{compile.log,undefined.txt,by-prefix.txt,referrers.txt}
. "$(dirname "$0")/env.sh"
abi="${1:-arm64-v8a}"
"$TCLDROID_ROOT/tools/fetch-ndk.sh" >/dev/null
rm -rf "$BUILD_DIR/src/tk$TK_VERSION"        # re-apply patches + overlay every time
"$TCLDROID_ROOT/tools/fetch-upstream.sh"
[ -f "$BUILD_DIR/install/android-$abi/include/tcl.h" ] || "$TCLDROID_ROOT/tools/build-tcl-android.sh" "$abi"

TK="$BUILD_DIR/src/tk$TK_VERSION"
TCLSRC="$BUILD_DIR/src/tcl$TCL_VERSION"
TOOL="$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin"
triple="$(abi_triple "$abi")"
CC="$TOOL/${triple}${ANDROID_MIN_API}-clang"
OUT="$BUILD_DIR/survey/$abi"; rm -rf "$OUT"; mkdir -p "$OUT/obj"

# Tk is built as a shared library that talks to Tcl through stubs (USE_TCL_STUBS).
FLAGS=(-DANDROID -DBUILD_tk -DUSE_TCL_STUBS -DTK_LIBRARY=\"//zipfs:/lib/tk\" -DHAVE_LIMITS_H=1
       -fPIC -O1 -w
       -I"$TK/android" -I"$TK/generic" -I"$TK/generic/ttk" -I"$TK/xlib" -I"$TK/bitmaps" -I"$TK/unix"
       -I"$BUILD_DIR/install/android-$abi/include" -I"$TCLSRC/generic")

# The compile set mirrors what the macOS port (the closest model: Unix-like, no X
# server) builds, read from unix/Makefile.in: the shared widget/generic/ttk object
# lists, plus the platform-neutral members of AQUA_OBJS (tkPointer, tkMacWinMenu,
# tkFileFilter, tkUnix3d, tkUnixScale, xlib/*). The tkMacOSX*.c files are exactly
# what the Android port must replace.
objs_of() { sed -n "/^$1[[:space:]]*=/,/^\$/p" "$TK/unix/Makefile.in" | tr -d '\\' | grep -o '[A-Za-z0-9_]*\.o' | sed 's/\.o$//'; }
LIST=$( { for v in WIDG_OBJS CANV_OBJS IMAGE_OBJS TEXT_OBJS FONT_OBJS GENERIC_OBJS TTK_OBJS STUB_OBJS; do objs_of $v; done
          objs_of AQUA_OBJS | grep -v -i 'MacOSX'; } | sort -u )
# Optionally also reuse Unix-port files that draw with the Xlib-shaped API only
# (Tk draws buttons/scrollbars/menus itself there, instead of using native controls).
# Default reuses the four widget files that compile unchanged; override with REUSE_UNIX="..." (empty = none).
LIST=$( { echo "$LIST"; for r in ${REUSE_UNIX-tkUnixButton tkUnixMenubu tkUnixScrlbr tkUnixMenu}; do echo "$r"; done; } | sort -u )
echo "$LIST" > "$OUT/compile-set.txt"
# tkUuid.h is generated at build time from manifest.uuid (see unix/Makefile.in).
{ echo '#define TK_VERSION_UUID \\'; cat "$TK/manifest.uuid"; echo; } > "$OUT/tkUuid.h"
FLAGS+=(-I"$OUT")
ok=0; bad=0
: > "$OUT/compile.log"
for name in $LIST; do
  src=""
  for d in generic generic/ttk unix xlib; do [ -f "$TK/$d/$name.c" ] && { src="$TK/$d/$name.c"; break; }; done
  [ -n "$src" ] || { echo "NO SOURCE for $name" >> "$OUT/compile.log"; bad=$((bad+1)); continue; }
  o="$OUT/obj/$name.o"
  if "$CC" "${FLAGS[@]}" -c "$src" -o "$o" 2>>"$OUT/compile.log"; then ok=$((ok+1)); else bad=$((bad+1)); echo "FAILED: $src" >> "$OUT/compile.log"; fi
done
log "compiled $ok files, $bad failed (see $OUT/compile.log)"

NM="$TOOL/llvm-nm"
# Symbols defined by Tk's own objects, and by Bionic (NDK sysroot stubs).
"$NM" --defined-only -g "$OUT"/obj/*.o 2>/dev/null | awk 'NF==3{print $3}' | sort -u > "$OUT/defined-tk.txt"
SYSROOT="$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/$triple/$ANDROID_MIN_API"
# (Bionic exports carry version suffixes such as malloc@@LIBC; strip them.)
for l in libc.so libm.so libdl.so; do "$NM" -D --defined-only "$SYSROOT/$l" 2>/dev/null | awk 'NF==3{print $3}'; done | sed 's/@.*//' | sort -u > "$OUT/defined-bionic.txt"
# Tcl's stub library is linked into Tk and provides Tcl_InitStubs / tclStubsPtr.
"$NM" --defined-only -g "$BUILD_DIR/install/android-$abi/lib/libtclstub.a" 2>/dev/null | awk 'NF==3{print $3}' | sort -u > "$OUT/defined-tclstub.txt"
# Tcl stub-table entry points are resolved at runtime through the stubs pointer.
"$NM" -u -g "$OUT"/obj/*.o 2>/dev/null | awk 'NF==2{print $2}' | sort -u > "$OUT/referenced.txt"
comm -23 "$OUT/referenced.txt" <(sort -u "$OUT/defined-tk.txt" "$OUT/defined-bionic.txt" "$OUT/defined-tclstub.txt") > "$OUT/undefined.txt"

# Which object files reference each missing symbol.
for o in "$OUT"/obj/*.o; do
  "$NM" -u -g "$o" 2>/dev/null | awk -v f="$(basename "$o" .o)" 'NF==2{print $2, f}'
done | sort > "$OUT/refs-all.txt"
join <(sort "$OUT/undefined.txt") "$OUT/refs-all.txt" | awk '{a[$1]=a[$1]" "$2} END{for(k in a)print k":"a[k]}' | sort > "$OUT/referrers.txt"

# Group by naming convention.
awk '{
  s=$1
  if (s ~ /^Tkp/) c="Tkp* (Tk platform hooks)"; else if (s ~ /^TkWm|^Wm/) c="window manager";
  else if (s ~ /^Tk[A-Z]/) c="Tk* (internal)"; else if (s ~ /^Ttk/) c="Ttk*";
  else if (s ~ /^X[A-Z]/) c="X* (Xlib-shaped drawing/window API)"; else if (s ~ /^Tcl_|^Tcl[A-Z]/) c="Tcl* (not stubbed)";
  else c="other"; n[c]++ } END{for(k in n) printf "%4d  %s\n", n[k], k}' "$OUT/undefined.txt" | sort -rn > "$OUT/by-prefix.txt"
log "missing symbols: $(wc -l < "$OUT/undefined.txt")"; cat "$OUT/by-prefix.txt"
