# Driven by GestureHarness: clipboard + long-press + pinch through the real JNI layer.
package require Tk
wm title . gestures
place [entry .e -font {TkDefaultFont 20}] -x 20 -y 100 -width 900 -height 100
place [button .probe -text Probe -command probe] -x 20 -y 400 -width 300 -height 100
.e insert end "hello wonderful world"
proc probe {} {puts "PROBE clip=[::clipboard get] sel=[expr {[.e selection present] ? [selection get] : {}}]"}
bind . <<PinchStart>> {puts "PINCH start %d"}
bind . <<Pinch>> {puts "PINCH update %d"}
bind . <<PinchEnd>> {puts "PINCH end %d"}
clipboard clear
clipboard append "from tcl"
puts READY
vwait forever
