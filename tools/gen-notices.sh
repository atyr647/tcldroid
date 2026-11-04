#!/usr/bin/env bash
# Write the third-party licence notices that ship inside every APK (assets/licenses/).
#   usage: tools/gen-notices.sh OUTFILE
. "$(dirname "$0")/env.sh"
out="${1:?usage: gen-notices.sh OUTFILE}"
"$TCLDROID_ROOT/tools/fetch-upstream.sh" >/dev/null
TCL="$BUILD_DIR/src/tcl$TCL_VERSION"; TK="$BUILD_DIR/src/tk$TK_VERSION"
section() { printf '\n==============================================================================\n%s\n==============================================================================\n\n' "$1"; }
{
  echo "THIRD-PARTY NOTICES"
  echo
  echo "This application runs on tcldroid, which contains the software below. Each part is used under"
  echo "its own licence, reproduced here as those licences require."
  section "Tcl $TCL_VERSION  (https://www.tcl-lang.org/)"
  cat "$TCL/license.terms"
  section "Tk $TK_VERSION  (https://www.tcl-lang.org/)"
  cat "$TK/license.terms"
  for p in itcl thread tdbc sqlite3; do
    f=$(ls "$TCL"/pkgs/${p}*/license.terms 2>/dev/null | head -1)
    [ -n "$f" ] && { section "Tcl bundled package: $(basename "$(dirname "$f")")"; cat "$f"; }
  done
  section "zlib (bundled with Tcl)"
  sed -n '/Copyright (C)/,/Mark Adler/p' "$TCL/compat/zlib/zlib.h" | sed 's/^ *\/\* *//; s/ *\*\/ *$//' | head -40
  section "SQLite (bundled with Tcl)"
  echo "The SQLite source code is in the public domain (https://www.sqlite.org/copyright.html)."
  section "tcldroid (the Android platform layer and host)"
  echo "Copyright the tcldroid authors. The licence of tcldroid's own code has not been chosen yet;"
  echo "see the LICENSE file of the source repository once it exists."
} > "$out"
log "wrote $out ($(wc -l < "$out") lines)"
