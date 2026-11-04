#!/usr/bin/env bash
# Run Tcl test scripts against a host build of Tk with the Android platform layer
# (headless: no device, private clipboard, FreeType/box text). Default: all tests/tk/*.tcl.
#   usage: tests/tk-host.sh [script.tcl ...]
. "$(dirname "$0")/../tools/env.sh"
"$TCLDROID_ROOT/tools/build-tk.sh" host >/dev/null
export LD_LIBRARY_PATH="$BUILD_DIR/install/host/lib" TKLIB="$BUILD_DIR/tk-host/libtk9.0.so" \
       TKDROID_SCREEN="${TKDROID_SCREEN:-1080x2200@420}" LANG=C.UTF-8
scripts=("$@"); [ ${#scripts[@]} -gt 0 ] || scripts=("$TCLDROID_ROOT"/tests/tk/[!s]*.tcl)	# (suite-driver.tcl belongs to tk-suite.sh)
rc=0
for s in "${scripts[@]}"; do
  echo "== $s"
  timeout 120 "$BUILD_DIR/install/host/bin/tclsh9.0" "$s" </dev/null || rc=1
done
exit $rc
