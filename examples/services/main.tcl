# Services -- tries every ::tkdroid:: platform service. Pack with:
#   tools/pack-app.sh examples/services --name Services --permission android.permission.POST_NOTIFICATIONS
package require Tk
wm title . Services
. configure -background #eceff1

proc log {msg} {
    .out configure -state normal
    .out insert end "$msg\n"
    .out see end
    .out configure -state disabled
}
proc done {what status results} {
    log "$what -> $status [expr {[llength $results] ? [list $results] : {}}]"
    if {$what eq "pick" && $status eq "ok"} {
        foreach f $results {
            log "  [file tail $f]: [file size $f] bytes"
            if {[string match *.txt $f]} {
                set fh [open $f]; log "  first line: [string range [gets $fh] 0 60]"; close $fh
            }
        }
    }
}

label .t -text "Platform services" -font {TkDefaultFont 16 bold} -background #eceff1
pack .t -fill x -pady 10
foreach {name label cmd} {
    pick   "Pick a file"          {::tkdroid::pickfile -command {done pick}}
    many   "Pick images (several)" {::tkdroid::pickfile -types {image/*} -multiple 1 -command {done many}}
    save   "Save a note"          {saveNote}
    share  "Share some text"      {::tkdroid::share -text "Hello from a Tcl app" -title "Share via" -command {done share}}
    sharef "Share a file"         {shareNote}
    url    "Open a web page"      {::tkdroid::openurl https://www.tcl-lang.org/ -command {done url}}
    perm   "Allow notifications"  {::tkdroid::permission request android.permission.POST_NOTIFICATIONS -command {done perm}}
    notify "Show a notification"  {::tkdroid::notify -title "Tcl says hi" -text "A notification from a Tcl/Tk app" -command {done notify}}
} {
    button .b$name -text $label -pady 8 -command $cmd
    pack .b$name -fill x -padx 16 -pady 3
}
proc noteFile {} {
    set f [file join $::env(TMPDIR) note.txt]
    set fh [open $f w]; puts $fh "A note written by a Tcl app at [clock format [clock seconds]]"; close $fh
    return $f
}
proc saveNote {} {::tkdroid::savefile -source [noteFile] -name "tcl-note.txt" -type text/plain -command {done save}}
proc shareNote {} {::tkdroid::share -file [noteFile] -title "Share the note" -command {done sharef}}

text .out -height 10 -state disabled -wrap word -background white
pack .out -fill both -expand 1 -padx 16 -pady 10
log "granted: POST_NOTIFICATIONS=[::tkdroid::permitted android.permission.POST_NOTIFICATIONS]"
wm protocol . WM_DELETE_WINDOW exit
vwait ::forever
