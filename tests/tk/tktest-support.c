/*
 * Pieces needed only by the test build of libtk (TKTEST=1, see tools/build-tk.sh).
 * tkTest.c is written as a stub-linked extension: it calls Tk through the stub tables and
 * starts with Tk_InitStubs. Inside the monolithic libtk the tables are Tk's own, so
 * Tk_InitStubs just points the three table pointers at them.
 */
#include "tkInt.h"
#undef Tk_InitStubs

extern const TkStubs tkStubs;
const TkStubs *tkStubsPtr = &tkStubs;
const TkIntStubs *tkIntStubsPtr;
const TkIntXlibStubs *tkIntXlibStubsPtr;

const char *
Tk_InitStubs(Tcl_Interp *interp, const char *version, int exact)
{
    (void) interp; (void) exact;
    tkIntStubsPtr = tkStubs.hooks->tkIntStubs;
    tkIntXlibStubsPtr = tkStubs.hooks->tkIntXlibStubs;
    return version;
}
