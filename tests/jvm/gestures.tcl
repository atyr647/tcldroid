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
# platform services: requests go to the host, answers come back later into the -command
proc cb {name status results} {puts "RESULT $name $status [llength $results] [join $results |]"}
::tkdroid::pickfile -types {image/png image/jpeg} -multiple 1 -command {cb pick}
::tkdroid::openurl https://example.org/x -command {cb url}
::tkdroid::permission request android.permission.CAMERA android.permission.VIBRATE -command {cb perm}
::tkdroid::share -text "hello there" -title Send -command {cb share}
set src [file join $::env(TMPDIR) save-src.txt]
set fh [open $src w]; puts $fh notes; close $fh
::tkdroid::savefile -source $src -name "my notes.txt" -type text/plain -command {cb save}
puts "MODE [::tkdroid::inputmode] [::tkdroid::inputmode text] [::tkdroid::inputmode code] [catch {::tkdroid::inputmode bogus}]"
puts "PERMITTED [::tkdroid::permitted android.permission.VIBRATE] [::tkdroid::permitted android.permission.CAMERA]"
puts "ERR [catch {::tkdroid::pickfile} m] $m"
puts "ERR [catch {::tkdroid::share} m] $m"
puts "ERR [catch {::tkdroid::pickfile -bogus 1 -command x} m] $m"
# device feedback, facts, fullscreen, lifecycle
::tkdroid::vibrate 30 128
::tkdroid::haptic confirm
set wav [file join $::env(TMPDIR) beep.wav]
set fh [open $wav wb]; puts -nonewline $fh RIFF; close $fh
::tkdroid::sound preload $wav
::tkdroid::sound play $wav -volume 0.5 -rate 1.5
::tkdroid::sound stop
bell
puts "INFO sdk=[::tkdroid::device sdk] model=[dict get [::tkdroid::device] model] files=[::tkdroid::device filesDir] android=[dict get [::tkdroid::device] android]"
puts "ERR [catch {::tkdroid::sound play /no/such/file.wav} m] $m"
puts "ERR [catch {::tkdroid::vibrate 99999} m] $m"
wm attributes . -fullscreen 1
bind . <<AndroidPause>> {puts "LIFE pause"}
bind . <<AndroidSaveState>> {puts "LIFE save"}
bind . <<AndroidResume>> {puts "LIFE resume"}
bind . <<AndroidLowMemory>> {puts "LIFE lowmem"}
puts READY
vwait forever
