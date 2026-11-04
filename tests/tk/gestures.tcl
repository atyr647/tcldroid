# Host-side checks of clipboard, touch scrolling, long-press selection and pinch.
# Run with tests/tk-host.sh (a host build of Tk with the Android platform layer).
load $::env(TKLIB) Tk
set fails 0
proc check {name cond} {
    global fails
    if {[uplevel 1 [list expr $cond]]} {puts "PASS $name"} else {puts "FAIL $name"; incr fails}
}
proc tap {x y} {::tkdroid::touch down $x $y; update; ::tkdroid::touch up $x $y; update}
proc drag {x y0 y1 {steps 8}} {
    ::tkdroid::touch down $x $y0; update
    for {set i 1} {$i <= $steps} {incr i} {
	::tkdroid::touch move $x [expr {$y0 + ($y1-$y0)*$i/$steps}]; update
    }
}
proc gesture {args} {::tkdroid::native {*}$args; update}

# ---- clipboard ------------------------------------------------------------------------
entry .e -width 30
pack .e -fill x
.e insert end "alpha beta gamma"
update
clipboard clear
clipboard append "from tk"
check clip-roundtrip {[clipboard get] eq "from tk"}
check clip-system {[::tkdroid::hostclipboard] eq "from tk"}
.e selection range 0 5
event generate .e <<Copy>>
check clip-copy-widget {[::tkdroid::hostclipboard] eq "alpha"}
::tkdroid::hostclipboard "other app text"
update
check clip-external-seen {[clipboard get] eq "other app text"}
check clip-ownership-dropped {![catch {selection own -selection CLIPBOARD} o] && $o eq ""}
.e delete 0 end
.e icursor 0
event generate .e <<Paste>>
check clip-paste-widget {[.e get] eq "other app text"}
clipboard clear
clipboard append "x"
check clip-reclaim {[clipboard get] eq "x" && [::tkdroid::hostclipboard] eq "x"}
selection clear -selection CLIPBOARD
check clip-selection-clear-empties-system {[::tkdroid::hostclipboard] eq "" && [catch {clipboard get}]}
clipboard clear
clipboard append "x"
check clip-get-bad-type {[catch {clipboard get -type NOSUCH}]}
destroy .e

# ---- touch scrolling in a text widget ----------------------------------------------------
text .t -width 30 -height 10 -wrap none
pack .t -fill both -expand 1
for {set i 1} {$i <= 300} {incr i} {.t insert end "line $i some words in a line\n"}
.t mark set insert 1.0
update
set rx [winfo rootx .t]; set ry [winfo rooty .t]
set x [expr {$rx + 50}]; set y0 [expr {$ry + 150}]
drag $x $y0 [expr {$y0 - 120}]
::tkdroid::touch up $x [expr {$y0 - 120}]; update
set v [lindex [.t yview] 0]
check drag-scrolls {$v > 0.0}
check drag-keeps-caret {[.t index insert] eq "1.0"}
check drag-no-selection {[llength [.t tag ranges sel]] == 0}
after 200 {set done 1}; vwait done
set v2 [lindex [.t yview] 0]
check fling-continues {$v2 >= $v}
after 1500 {set done 1}; vwait done	;# let the momentum die out
# a tap places the caret (and focuses)
.t yview moveto 0; update
tap [expr {$rx + 40}] [expr {$ry + 5}]
check tap-places-caret {[lindex [split [.t index insert] .] 0] == 1 && [lindex [split [.t index insert] .] 1] > 0}
check tap-focus {[focus] eq ".t"}
# long press selects a word; dragging extends it
set X [expr {$rx + 10 * 8}]; set Y [expr {$ry + 5}]
::tkdroid::touch down $X $Y; update
gesture longpress $X $Y 0 0
check longpress-selects {[llength [.t tag ranges sel]] == 2}
check longpress-word {[regexp {^\w+$} [.t get sel.first sel.last]]}
::tkdroid::touch up $X $Y; update
# toolbar flags: select all + copy (+ cut since editable)
set ::menu {}
rename ::tkdroid::textmenu ::tkdroid::textmenu_real
proc ::tkdroid::textmenu {x y f} {set ::menu [list $x $y $f]}
::tkdroid::touch down $X $Y; update
gesture longpress $X $Y 0 0
::tkdroid::touch up $X $Y; update
update; update
check menu-shown {[llength $::menu] == 3}
check menu-flags {[lindex $::menu 2] & 8 && [lindex $::menu 2] & 2 && [lindex $::menu 2] & 1}
gesture textaction 2 0 0 0
check action-copy {[::tkdroid::hostclipboard] eq [.t get sel.first sel.last]}
gesture textaction 8 0 0 0
check action-selectall {[.t tag ranges sel] eq "1.0 [.t index end]"}
gesture textaction 1 0 0 0
check action-cut {[.t get 1.0 end] eq "\n"}
check cut-to-clipboard {[string match "line 1 some*" [::tkdroid::hostclipboard]]}
# paste enabled when the clipboard has text
set ::menu {}
.t insert end "hello world"
::tkdroid::touch down $X $Y; update
gesture longpress $X $Y 0 0
::tkdroid::touch up $X $Y; update; update
check menu-paste-flag {[lindex $::menu 2] & 4}
destroy .t

# ---- sliding off a button before lifting must not click it ---------------------------------------
set clicks 0
button .c -text Click -command {incr clicks}
pack .c -pady 40
update
set cx [expr {[winfo rootx .c] + 10}]; set cy [expr {[winfo rooty .c] + 10}]
tap $cx $cy
check button-tap-clicks {$clicks == 1}
::tkdroid::touch down $cx $cy; update
::tkdroid::touch move $cx [expr {$cy + 400}]; update
::tkdroid::touch up $cx [expr {$cy + 400}]; update
check button-slide-off-no-click {$clicks == 1}
::tkdroid::touch down $cx $cy; update
::tkdroid::touch move [expr {$cx + 3}] $cy; update
::tkdroid::touch up [expr {$cx + 3}] $cy; update
check button-slide-within-clicks {$clicks == 2}
::tkdroid::touch down $cx $cy; update
::tkdroid::touch cancel $cx $cy; update
check button-cancel-no-click {$clicks == 2}
destroy .c

# ---- long press on a button: virtual event, no word selection --------------------------------
button .b -text Hello
pack .b
update
set got {}
bind .b <<LongPress>> {set got "%x,%y"}
set bx [expr {[winfo rootx .b] + 10}]; set by [expr {[winfo rooty .b] + 10}]
gesture longpress $bx $by 0 0
check longpress-virtual {$got eq "10,10"}
destroy .b

# ---- listbox scrolls by rows --------------------------------------------------------------------
listbox .l -height 8
pack .l
for {set i 0} {$i < 100} {incr i} {.l insert end "item $i"}
update
set lx [expr {[winfo rootx .l] + 20}]; set ly [expr {[winfo rooty .l] + 200}]
drag $lx $ly [expr {$ly - 150}]
::tkdroid::touch up $lx [expr {$ly - 150}]; update
check listbox-drag {[.l nearest 0] > 0}
check listbox-no-select {[.l curselection] eq ""}
tap $lx [expr {[winfo rooty .l] + 30}]
check listbox-tap-selects {[.l curselection] ne ""}
destroy .l

# ---- pinch ------------------------------------------------------------------------------------------
frame .f -width 200 -height 200
pack .f
update
set log {}
bind .f <<PinchStart>> {lappend log "start %d"}
bind .f <<Pinch>> {lappend log "pinch %d"}
bind .f <<PinchEnd>> {lappend log "end %d"}
set px [expr {[winfo rootx .f] + 50}]; set py [expr {[winfo rooty .f] + 50}]
gesture pinch 0 $px $py 1000
gesture pinch 1 $px $py 1500
gesture pinch 2 $px $py 1750
check pinch-events {$log eq {{start 1.000} {pinch 1.500} {end 1.750}}}
# touchScroll off restores classic behaviour
destroy .f
set ::tkdroid::touchScroll 0
text .t2 -width 30 -height 5
pack .t2
.t2 insert end "one two three\nfour five six\n"
update
set cx [expr {[winfo rootx .t2] + 5}]; set cy [expr {[winfo rooty .t2] + 5}]
::tkdroid::touch down $cx $cy; update
::tkdroid::touch move [expr {$cx + 60}] $cy; update
::tkdroid::touch up [expr {$cx + 60}] $cy; update
check classic-drag-selects {[llength [.t2 tag ranges sel]] == 2}
# ---- rotation: the screen changes shape, the layout follows ------------------------------------------
frame .rot -background red
pack .rot -fill both -expand 1
update
::tkdroid::resize 2200 1080; update
check rotate-landscape {[winfo width .] == 2200 && [winfo height .] == 1080 && [winfo width .rot] == 2200}
::tkdroid::resize 1080 2200; update
check rotate-portrait {[winfo width .] == 1080 && [winfo height .] == 2200}
destroy .rot
# ---- platform services without a host: an immediate, clean error through the callback ------------------
set ::svc {}
::tkdroid::pickfile -command {lappend ::svc}
::tkdroid::openurl https://example.org -command {lappend ::svc}
vwait ::svc
update
check svc-headless-error {[lindex $::svc 0 0] eq "error"}
check svc-permitted-headless {[::tkdroid::permitted android.permission.CAMERA] == 0}
check svc-bad-options {[catch {::tkdroid::savefile -name x -command y}] && [catch {::tkdroid::openurl}]}

# ---- hardware keyboard: modifier state reaches Tk (Ctrl+C/V/X/A/Z, Shift+Tab, function keys) --------------
proc hwkey {name {state 0}} {::tkdroid::key down $name $state; update; ::tkdroid::key up $name $state; update}
entry .ke -width 30
pack .ke
update
focus -force .ke
.ke insert end "hello world"
.ke selection range 0 5
hwkey c 4				;# Ctrl+C
check hwkey-ctrl-c {[::tkdroid::hostclipboard] eq "hello"}
.ke selection clear
.ke icursor end
hwkey v 4				;# Ctrl+V
check hwkey-ctrl-v {[.ke get] eq "hello worldhello"}
hwkey a 4				;# Ctrl+A: select all (Tk's X11 binding is <<SelectAll>> on Control-slash and Control-a)
check hwkey-ctrl-a {[.ke selection present] && [selection get] eq "hello worldhello"}
hwkey x 4				;# Ctrl+X cuts the selection
check hwkey-ctrl-x {[.ke get] eq "" && [::tkdroid::hostclipboard] eq "hello worldhello"}
text .kt -width 20 -height 3 -undo 1
pack .kt
update
focus -force .kt
.kt insert end "abc"
.kt edit separator
hwkey d 0
hwkey e 0
check hwkey-plain-typing {[.kt get 1.0 end-1c] eq "abcde"}
hwkey z 4				;# Ctrl+Z undoes
check hwkey-ctrl-z {[.kt get 1.0 end-1c] ne "abcde"}
button .kb1 -text A; button .kb2 -text B
pack .kb1 .kb2
focus -force .kb2
hwkey Tab 1			;# Shift+Tab: previous widget (the real X keysym is ISO_Left_Tab)
hwkey ISO_Left_Tab 1
check hwkey-shift-tab {[focus] ne ".kb2"}
set ::fk {}
bind .kb1 <F5> {lappend ::fk F5}
bind .kb1 <Control-Right> {lappend ::fk C-Right}
focus -force .kb1
hwkey F5
hwkey Right 4
check hwkey-function-and-arrow-with-modifier {$::fk eq "F5 C-Right"}
destroy .ke .kt .kb1 .kb2

# ---- non-ASCII text is measured as a run, consistently with how Tk lays it out -------------------------------------
set f {Helvetica 12}
set txt "h\u00e9llo w\u00f6rld \u4f60\u597d \u0645\u0631\u062d\u0628\u0627"
set whole [font measure $f $txt]
check measure-run-positive {$whole > [font measure $f "h"]}
check measure-prefix-monotonic {[font measure $f "h\u00e9"] > [font measure $f "h"] && [font measure $f "h\u00e9llo"] > [font measure $f "h\u00e9"]}
label .nl -text $txt -font $f -wraplength [expr {$whole / 2}] -justify left
pack .nl
update
check wrap-nonascii-two-or-more-lines {[winfo reqheight .nl] >= 2 * [font metrics $f -linespace]}
check wrap-nonascii-width-bounded {[winfo reqwidth .nl] <= $whole / 2 + 4}
label .nl2 -text $txt -font $f
pack .nl2
update
check nowrap-width-equals-measure {abs([winfo reqwidth .nl2] - $whole) <= 2 * [.nl2 cget -borderwidth] + 2 * [.nl2 cget -padx] + 2 * [.nl2 cget -highlightthickness]}
destroy .nl .nl2

# ---- ::tkdroid:: must not shadow Tcl/Tk commands: scripts inside the namespace would call them ----
foreach c [info commands ::tkdroid::*] {
    set t [namespace tail $c]
    check "no-shadow-$t" {[info commands ::$t] eq ""}
}
puts "failures: $fails"
exit [expr {$fails > 0}]
