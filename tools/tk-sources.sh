#!/usr/bin/env bash
# Print the absolute paths of the Tk sources the Android port compiles: the macOS
# port's shared object lists (read from unix/Makefile.in), the reused Unix widget
# files, and our own tk-android/android/*.c. Assumes sources are fetched and patched.
#   usage: tools/tk-sources.sh [--with-android]    (--with-android adds our platform files)
. "$(dirname "$0")/env.sh"
TK="$BUILD_DIR/src/tk$TK_VERSION"
objs_of() { sed -n "/^$1[[:space:]]*=/,/^\$/p" "$TK/unix/Makefile.in" | tr -d '\\' | grep -o '[A-Za-z0-9_]*\.o' | sed 's/\.o$//'; }
LIST=$( { for v in WIDG_OBJS CANV_OBJS IMAGE_OBJS TEXT_OBJS FONT_OBJS GENERIC_OBJS TTK_OBJS STUB_OBJS; do objs_of $v; done
          objs_of AQUA_OBJS | grep -v -i 'MacOSX'
          for r in ${REUSE_UNIX-tkUnixButton tkUnixMenubu tkUnixScrlbr tkUnixMenu}; do echo "$r"; done; } | sort -u )
for name in $LIST; do
  for d in generic generic/ttk unix xlib; do
    [ -f "$TK/$d/$name.c" ] && { echo "$TK/$d/$name.c"; break; }
  done
done
if [ "${1:-}" = "--with-android" ]; then
  for f in "$TCLDROID_ROOT"/tk-android/android/tkAndroid*.c; do echo "$f"; done
fi
