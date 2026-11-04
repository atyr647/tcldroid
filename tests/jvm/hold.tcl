# The first frame the host receives must be the finished UI, not the blank window that exists while
# the app is still starting (see the start-up hold in tkAndroidEvent.c).
package require Tk
. configure -background red
update                                  ;# an app doing this must NOT flash a red window
set t [expr {[clock milliseconds] + 700}]
while {[clock milliseconds] < $t} {}    ;# "building the first screen" takes a while
frame .f -background green
pack .f -fill both -expand 1
update
vwait forever                           ;# entering the event loop ends start-up: now the frame is shown
