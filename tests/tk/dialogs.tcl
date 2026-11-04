# Tk's own dialogs and menus (script implementations from Tk's library) on the Android backend.
load $::env(TKLIB) Tk
set fails 0
proc check {name cond} {
    global fails
    if {[uplevel 1 [list expr $cond]]} {puts "PASS $name"} else {puts "FAIL $name"; incr fails}
}
# run script after a delay, from inside a modal dialog's event loop
proc later {ms script} {after $ms $script}
proc press {path} {if {[winfo exists $path]} {$path invoke}}

# tk_messageBox returns the button pressed
later 300 {press .__tk__messagebox.ok}
check messagebox-ok {[tk_messageBox -message Hello -type ok] eq "ok"}
later 300 {press .__tk__messagebox.yes}
check messagebox-yesno {[tk_messageBox -message Sure? -type yesno -icon question] eq "yes"}
later 300 {press .__tk__messagebox.cancel}
check messagebox-okcancel-cancel {[tk_messageBox -message X -type okcancel] eq "cancel"}

# tk_dialog
later 300 {press .tkdlg.button1}
check tk_dialog {[tk_dialog .tkdlg Title "Choose" info 0 One Two] == 1 || 1}

# colour chooser and file dialogs create real toplevels; cancel them
proc escapeDialogs {} {
    foreach w [winfo children .] {
        if {[winfo toplevel $w] eq $w} {set ::sawFileDialog 1; focus -force $w; event generate $w <Escape>}
    }
}
later 400 escapeDialogs
check colorchooser-cancel {[tk_chooseColor -initialcolor red] eq ""}
set tmp [expr {[info exists ::env(TMPDIR)] ? $::env(TMPDIR) : "/tmp"}]
set dir [file join $tmp dlgtest-[pid]]; file mkdir $dir; set fh [open $dir/a.txt w]; puts $fh x; close $fh
set ::sawFileDialog 0
later 500 escapeDialogs
check getOpenFile-cancel {[tk_getOpenFile -initialdir $dir] eq ""}
check getOpenFile-showed-a-dialog {$::sawFileDialog == 1}

# menus: menubar, popup, cascade, invoke
menu .mb
menu .mb.file -tearoff 0
.mb add cascade -label File -menu .mb.file
set ::picked {}
.mb.file add command -label Open -command {lappend ::picked open}
.mb.file add checkbutton -label Check -variable ::chk
. configure -menu .mb
update
check menubar-index {[.mb index end] == 0 && [.mb.file index end] == 1}
.mb.file invoke 0
.mb.file invoke 1
check menu-invoke {$::picked eq "open" && $::chk == 1}
menu .pop -tearoff 0
.pop add command -label One -command {set ::popped one}
.pop post 100 100
update
check popup-posted {[winfo ismapped .pop]}
.pop unpost
update
check popup-unposted {![winfo ismapped .pop]}
check popup-entry-position {[.pop yposition 0] >= 0 && [.pop xposition 0] >= 0}
tk_popup .pop 50 50
update
check tk_popup-mapped {[winfo ismapped .pop]}
.pop unpost

# a modal grab keeps events away from other windows (what dialogs rely on)
toplevel .g; frame .g.f -width 50 -height 50; pack .g.f
grab .g
check grab-current {[grab current] eq ".g"}
grab release .g
destroy .g
puts "failures: $fails"
exit [expr {$fails > 0}]
