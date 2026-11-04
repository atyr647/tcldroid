# Notes -- a tiny ordinary Tcl/Tk app, packaged with: tools/pack-app.sh examples/notes
# It stores its text in $env(HOME)/notes.txt (the app's private storage).
package require Tk

# Prose, not code: let the keyboard suggest, correct and compose (swipe typing, CJK).
catch {::tkdroid::inputmode text}

set file [file join $::env(HOME) notes.txt]
set dpi  [expr {[winfo pixels . 1i]}]
set status ""

wm title . Notes
. configure -background #fffde7

label .title -text "Notes" -font {TkDefaultFont 16 bold} -background #fffde7 -anchor w
pack .title -side top -fill x -padx 12 -pady {12 4}

frame .btns -background #fffde7
button .btns.save  -text Save  -command save
button .btns.clear -text Clear -command {.t delete 1.0 end; set status "Cleared (not saved yet)"}
label  .btns.msg   -textvariable status -background #fffde7 -foreground #555555 -anchor e
pack .btns.save .btns.clear -side left -padx 4
pack .btns.msg -side right -padx 8 -fill x -expand 1
pack .btns -side bottom -fill x -padx 8 -pady 8

text .t -wrap word -relief sunken -borderwidth 2 -background white -undo 1 \
    -padx 8 -pady 8 -font [list TkTextFont 14]
pack .t -side top -fill both -expand 1 -padx 12 -pady 4

proc save {} {
    global file status
    set fh [open $file w]
    fconfigure $fh -encoding utf-8
    puts -nonewline $fh [.t get 1.0 end-1c]
    close $fh
    set status "Saved [clock format [clock seconds] -format %H:%M:%S]"
}

if {[file exists $file]} {
    set fh [open $file]
    fconfigure $fh -encoding utf-8
    .t insert end [read $fh]
    close $fh
    set status "Loaded"
} else {
    .t insert end "Tap here and type. Your note is kept between launches.\n"
}

# Back button / closing the window saves, then exits.
wm protocol . WM_DELETE_WINDOW {save; exit}
vwait ::forever
