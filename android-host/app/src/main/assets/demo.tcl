# demo.tcl -- an ordinary Tcl/Tk script, run unmodified by the Android host.
# Nothing here is Android-specific except reading ::tkdroid::screen for the info line.

wm title . "Tk on Android"
. configure -background #eceff1

set count 0
set name  "tap here, then type"
set cb 0
set rb 1

label .hdr -text "Tcl/Tk $tk_patchLevel" -font {sans-serif 14 bold} -background #eceff1 -pady 8
lassign [::tkdroid::screen] sw sh dpi
label .info -background #eceff1 -wraplength [expr {$sw - 40}] -justify center \
    -text "screen ${sw}x${sh} @ [expr {int($dpi)}] dpi,  tk scaling [format %.2f [tk scaling]] px/pt"

button .tap -text "Tap me" -pady 14 -command {
    incr count
    .tap configure -text "Tapped $count time[expr {$count == 1 ? {} : {s}}]"
}

entry .e -textvariable name -justify center

frame .opts -background #eceff1
checkbutton .opts.c  -text "Check"  -variable cb   -background #eceff1 -anchor w
radiobutton .opts.r1 -text "One"    -variable rb -value 1 -background #eceff1 -anchor w
radiobutton .opts.r2 -text "Two"    -variable rb -value 2 -background #eceff1 -anchor w
pack .opts.c .opts.r1 .opts.r2 -side left -padx 12

scale .s -from 0 -to 100 -orient horizontal -showvalue 1 -background #eceff1 \
    -sliderlength [expr {int(0.9 * $dpi / 2.54 * 1.2)}]

canvas .cv -height [expr {int($dpi * 0.9)}] -background white -relief sunken -borderwidth 2
set r [expr {int($dpi * 0.18)}]
.cv create oval 60 30 [expr {60+2*$r}] [expr {30+2*$r}] -fill orange -outline black -width 2 -tags drag
.cv create rectangle 260 40 [expr {260+2*$r}] [expr {40+2*$r}] -fill #4fc3f7 -tags drag
.cv create text [expr {$sw/2}] [expr {int($dpi*0.8)}] -text "drag the shapes" -fill #2e7d32
.cv bind drag <ButtonPress-1> {set dx %x; set dy %y}
.cv bind drag <B1-Motion>     {.cv move current [expr {%x-$dx}] [expr {%y-$dy}]; set dx %x; set dy %y}

ttk::progressbar .pb -mode indeterminate
.pb start 25

text .log -height 8 -wrap word -font TkFixedFont -relief sunken -borderwidth 2
.log insert end "Self-test output appears here.\n"

# Run assets/selftest.tcl, showing its output in the text widget above.
proc runSelftest {} {
    global env
    .log delete 1.0 end
    rename ::puts ::tcl_puts
    proc ::puts args {
        .log insert end "[lindex $args end]\n"
        .log see end
        update idletasks
    }
    set code [catch {uplevel #0 [list source [file join $env(HOME) assets selftest.tcl]]} err]
    rename ::puts {}
    rename ::tcl_puts ::puts
    if {$code} { .log insert end "error: $err\n" }
    .log see end
}
# Measure how fast the pipeline redraws on this device (results go to the text box).
proc benchmark {} {
    .log delete 1.0 end
    .log insert end "benchmark running...\n"
    update
    set results {}
    tkdroid::stats reset
    set t0 [clock milliseconds]
    for {set i 0} {$i < 60} {incr i} { .cv move drag 4 0; update }
    set ms [expr {([clock milliseconds] - $t0) / 60.0}]
    lappend results [format "canvas: move 2 shapes   %6.1f ms/frame" $ms] [format "  draw %.1f  present %.1f  (avg ms, area %d px)" \
        [dict get [tkdroid::stats] draw_avg] [dict get [tkdroid::stats] present_avg] [dict get [tkdroid::stats] area]]
    for {set i 0} {$i < 60} {incr i} { .cv move drag -4 0; update }
    tkdroid::stats reset
    .log delete 1.0 end
    set t0 [clock milliseconds]
    for {set i 0} {$i < 40} {incr i} { .log insert end "line $i of scrolling text in a text widget\n"; .log see end; update }
    set ms [expr {([clock milliseconds] - $t0) / 40.0}]
    lappend results [format "text: append + scroll  %6.1f ms/frame" $ms] [format "  draw %.1f  present %.1f  (avg ms, area %d px)" \
        [dict get [tkdroid::stats] draw_avg] [dict get [tkdroid::stats] present_avg] [dict get [tkdroid::stats] area]]
    tkdroid::stats reset
    set t0 [clock milliseconds]
    for {set i 0} {$i < 20} {incr i} { .s set [expr {$i * 5}]; update }
    set ms [expr {([clock milliseconds] - $t0) / 20.0}]
    lappend results [format "scale: slide           %6.1f ms/frame" $ms]
    .log delete 1.0 end
    foreach r $results { .log insert end "$r\n" }
    .log insert end "frame = Tk redraw + raster ('draw') then copy to Android ('present')\n"
}
button .bm -text "Benchmark" -command benchmark -pady 8
button .st -text "Run self-test" -command runSelftest -pady 8
button .quit -text "Quit" -command exit -pady 8

pack .hdr .info -fill x
pack .tap -fill x -padx 24 -pady 10
pack .e -fill x -padx 24 -pady 6 -ipady 6
pack .opts -pady 6
pack .s -fill x -padx 24
pack .cv -fill x -padx 24 -pady 8
pack .pb -fill x -padx 24 -pady 4
pack .log -fill both -expand 1 -padx 24 -pady 6
pack .bm .st .quit -fill x -padx 24 -pady 3

# Back navigation: a handler here would run instead of closing the app.
# (Left unset on purpose: with no handler, Back finishes the Activity.)

vwait ::forever
