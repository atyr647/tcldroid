# Accessibility snapshot (tk-android/library/a11y.tcl): what a screen reader is told about real widgets.
load $::env(TKLIB) Tk
package require Ttk
set fails 0
proc check {name cond} {
    global fails
    if {[uplevel 1 [list expr $cond]]} {puts "PASS $name"} else {puts "FAIL $name"; incr fails}
}
proc nodes {} {
    set d {}
    foreach l [split [::tkdroid::a11y::snapshot] \n] {
	lassign [split $l \t] id parent x1 y1 x2 y2 role flags text value
	dict set d $id [dict create id $id parent $parent rect [list $x1 $y1 $x2 $y2] role $role flags $flags text $text value $value]
    }
    return $d
}
proc find {nodes role text} {
    dict for {id n} $nodes {
	if {[dict get $n role] eq $role && [dict get $n text] eq $text} {return $n}
    }
    return {}
}
proc has {n flag} {expr {[string first $flag [dict get $n flags]] >= 0}}

set ::clicks 0
button .b -text "Save file" -command {incr ::clicks}
button .bd -text "Disabled one" -state disabled
label .l -text "Hello label"
set ::cb 1
checkbutton .c -text "Remember" -variable ::cb
set ::rb b
radiobutton .r1 -text "Choice A" -variable ::rb -value a
radiobutton .r2 -text "Choice B" -variable ::rb -value b
entry .e -width 20
.e insert 0 "typed text"
entry .pw -width 10 -show *
.pw insert 0 secret
text .t -width 20 -height 3
.t insert end "multi\nline"
listbox .lb -height 3
.lb insert end alpha beta gamma delta
.lb selection set 1
scale .s -from 0 -to 100 -orient horizontal
.s set 40
ttk::button .tb -text "Themed button"
ttk::progressbar .pb -value 25
canvas .cv -width 200 -height 100
.cv create text 10 10 -text "Canvas heading" -anchor nw
.cv create text 10 60 -text "Tap target" -anchor nw -tags tgt
.cv bind tgt <ButtonRelease-1> {incr ::clicks 10}
.cv create text 10 5000 -text "far below" -anchor nw
.cv configure -scrollregion {0 0 200 6000}
pack .b .bd .l .c .r1 .r2 .e .pw .t .lb .s .tb .pb .cv -fill x
update
set ::tkdroid::a11y::enabled 1
set n [nodes]

check count {[dict size $n] >= 15}
set b [find $n button "Save file"]
check button {$b ne "" && [has $b x] && [has $b e]}
check button-rect-matches-window {[dict get $b rect] eq [list [winfo rootx .b] [winfo rooty .b] [expr {[winfo rootx .b]+[winfo width .b]}] [expr {[winfo rooty .b]+[winfo height .b]}]]}
set bd [find $n button "Disabled one"]
check disabled-button {$bd ne "" && ![has $bd x] && ![has $bd e]}
check label {[find $n text "Hello label"] ne ""}
set c [find $n checkbox Remember]
check checkbox-checked {$c ne "" && [has $c c] && [has $c k] && [dict get $c value] eq "on"}
set ::cb 0
set n [nodes]
check checkbox-unchecked-after-variable-change {![has [find $n checkbox Remember] k]}
check radio-selection {[has [find $n radio "Choice B"] k] && ![has [find $n radio "Choice A"] k]}
set ed ""
dict for {id x} $n {if {[dict get $x role] eq "edit" && [dict get $x value] eq "typed text"} {set ed $x}}
check entry-value {$ed ne "" && [has $ed t]}
set pw ""
dict for {id x} $n {if {[dict get $x role] eq "edit" && [has $x p]} {set pw $x}}
check password-hidden {$pw ne "" && [dict get $pw value] ne "secret" && [string length [dict get $pw value]] == 6}
set tx ""
dict for {id x} $n {if {[dict get $x role] eq "edit" && [has $x m]} {set tx $x}}
check multiline-text-value {$tx ne "" && [dict get $tx value] eq "multi line"}
check listbox-items {[find $n listitem alpha] ne "" && [find $n listitem gamma] ne "" && [find $n listitem delta] eq ""}
check listbox-selected {[has [find $n listitem beta] s] && ![has [find $n listitem alpha] s]}
set lbnode ""; dict for {id x} $n {if {[dict get $x role] eq "list"} {set lbnode $x}}
check listitem-parent-is-list {[dict get [find $n listitem alpha] parent] == [dict get $lbnode id]}
set sl ""; dict for {id x} $n {if {[dict get $x role] eq "slider"} {set sl $x}}
check slider-value {$sl ne "" && [dict get $sl value] == 40}
set pb ""; dict for {id x} $n {if {[dict get $x role] eq "progress"} {set pb $x}}
check progress-value {$pb ne "" && [dict get $pb value] == 25}
check ttk-button {[find $n button "Themed button"] ne ""}
check canvas-text {[find $n text "Canvas heading"] ne ""}
check canvas-bound-text-is-a-button {[has [find $n button "Tap target"] x]}
check canvas-offscreen-item-excluded {[find $n text "far below"] eq "" && [find $n button "far below"] eq ""}
.cv yview moveto 0.83
update
set n [nodes]
check canvas-scrolled-shows-new-item {[find $n text "far below"] ne "" || [find $n button "far below"] ne ""}
check canvas-scrolled-hides-old-item {[find $n text "Canvas heading"] eq ""}
.cv yview moveto 0
update

# focus is reported
focus -force .e
update
set n [nodes]
set foc {}
dict for {id x} $n {if {[has $x f]} {lappend foc [dict get $x role]}}
check focus-flag {$foc eq "edit"}

# a dialog is a window node with its title; its content is a child of it
toplevel .dlg
wm title .dlg "Confirm delete"
button .dlg.ok -text "Delete it"
pack .dlg.ok
update
set n [nodes]
set win [find $n window "Confirm delete"]
check dialog-window {$win ne ""}
set okb [find $n button "Delete it"]
check dialog-child {$okb ne "" && [dict get $okb parent] == [dict get $win id]}
destroy .dlg
update
check dialog-gone {[find [nodes] window "Confirm delete"] eq ""}

# ids stay stable while a widget lives, and are never reused
set id1 [dict get [find [nodes] button "Save file"] id]
update
check ids-stable {[dict get [find [nodes] button "Save file"] id] == $id1}

# actions: a click is a tap at the node centre; set text; scroll
set bid [dict get [find [nodes] button "Save file"] id]
::tkdroid::a11y::snapshot
set ::tkdroid::a11y::last [::tkdroid::a11y::snapshot]
::tkdroid::a11y::action $bid 16 ""
after 300 {set ::go 1}; vwait ::go
check action-click-invokes-button {$::clicks == 1}
set tid [dict get [find [nodes] button "Tap target"] id]
set ::tkdroid::a11y::last [::tkdroid::a11y::snapshot]
::tkdroid::a11y::action $tid 16 ""
after 300 {set ::go 2}; vwait ::go
check action-click-canvas-item-fires-its-binding {$::clicks == 11}
set eid ""; dict for {id x} [nodes] {if {[dict get $x role] eq "edit" && [dict get $x value] eq "typed text"} {set eid $id}}
set ::tkdroid::a11y::last [::tkdroid::a11y::snapshot]
::tkdroid::a11y::action $eid 2097152 "replaced"
check action-set-text {[.e get] eq "replaced"}
::tkdroid::a11y::action $eid 1 ""
update
check action-focus {[focus] eq ".e"}
# the app's own words
button .icon -text "X"
pack .icon
::tkdroid::accessible .icon -name "Close"
label .deco -text "decoration"
pack .deco
::tkdroid::accessible .deco -hidden 1
frame .custom -width 50 -height 30 -background red
pack .custom
::tkdroid::accessible .custom -name "Colour swatch" -role button
update
set n [nodes]
check override-name {[find $n button "Close"] ne "" && [find $n button "X"] eq ""}
check override-hidden {[find $n text "decoration"] eq ""}
check override-role-on-plain-frame {[find $n button "Colour swatch"] ne ""}
check override-bad-option {[catch {::tkdroid::accessible .icon -bogus 1}]}
# a read-only text box is text, not an edit field
text .ro -width 20 -height 2 -state normal
.ro insert end "read only words"
.ro configure -state disabled
pack .ro
update
check readonly-text-is-text {[find [nodes] text "read only words"] ne ""}
# a click that carries its own coordinates works even for an id Tcl no longer knows
set before $::clicks
::tkdroid::a11y::action 999999 16 "[expr {[winfo rootx .b] + 5}] [expr {[winfo rooty .b] + 5}]"
after 300 {set ::go 3}; vwait ::go
check click-with-coordinates-needs-no-bookkeeping {$::clicks == $before + 1}
puts "failures: $fails"
exit [expr {$fails > 0}]
