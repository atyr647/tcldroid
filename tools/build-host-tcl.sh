#!/usr/bin/env bash
# Build a native (build-machine) Tcl. Cross-compiling Tcl 9 needs a working
# tclsh on the build machine to generate tclIndex files and the zipfs payload.
. "$(dirname "$0")/env.sh"
"$TCLDROID_ROOT/tools/fetch-upstream.sh"

PREFIX="$BUILD_DIR/install/host"
if [ -x "$PREFIX/bin/tclsh9.0" ]; then log "host Tcl already built: $PREFIX"; exit 0; fi

mkdir -p "$BUILD_DIR/host/tcl"
cd "$BUILD_DIR/host/tcl"
log "configuring host Tcl $TCL_VERSION"
"$BUILD_DIR/src/tcl$TCL_VERSION/unix/configure" --prefix="$PREFIX" >configure.log 2>&1
log "building host Tcl (log: $PWD/make.log)"
# The bundled-package configure step runs the just-built tclsh before it is installed.
LD_LIBRARY_PATH="$PWD" make -j"$(nproc)" >make.log 2>&1
make install >install.log 2>&1
log "host Tcl installed: $PREFIX"
