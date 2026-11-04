# Run ONE file of Tk's own test suite against the Android backend (host build with Tk's test commands).
# Called by tests/tk-suite.sh:  tclsh suite-driver.tcl file.test
load $::env(TKLIB) Tk
::tkdroid::tktestinit
# Tk's tests drive widgets with synthetic mouse events; the touch policy (drag scrolls, tap acts on
# release) is the Android layer's own and is tested separately (tests/tk/gestures.tcl).
set ::tkdroid::touchScroll 0
cd $::env(TKTESTS)
package require tcltest 2.2
set f [lindex $argv 0]
set argv {}
tcltest::configure -singleproc 1 -verbose {body error} -tmpdir $::env(TMPDIR) -notfile {}
source $f
