# a11y.tcl --
#
#	Accessibility for Tk on Android. Tk 9.0 has no accessibility API of its own, so this walks the
#	widget tree and describes what a person using TalkBack (or another screen reader) needs: for every
#	visible, meaningful widget its role, name, value, state, screen rectangle and what can be done to it.
#	The description is a flat text snapshot sent to the Java side (AccessibilityNodeProvider), which turns
#	it into Android accessibility nodes. Actions come back through ::tkdroid::native a11yaction.
#
#	Nothing runs unless a screen reader (or another accessibility service) is active.
#
#	Snapshot format, one node per line, tab separated:
#	  id  parent  x1  y1  x2  y2  role  flags  text  value
#	roles: button checkbox radio edit text list listitem slider progress window view
#	flags: e enabled  f focused  c checkable  k checked  s selected  t editable text  x clickable
#	       l scrollable  p password  m multi-line

namespace eval ::tkdroid::a11y {
    variable enabled 0
    variable ids; array set ids {}		;# key (window path, or path#item) -> id
    variable keys; array set keys {}		;# id -> key
    variable nextId 1
    variable seen {}				;# keys used by the snapshot being built
    variable lines {}
    variable last ""
    variable lastFrames -1
    variable job ""
    variable maxText 400
    variable override; array set override {}	;# window -> dict (name role hidden), set by ::tkdroid::accessible
}

proc ::tkdroid::a11y::idFor {key} {
    variable ids; variable keys; variable nextId; variable seen
    if {![info exists ids($key)]} {
	set ids($key) $nextId
	set keys($nextId) $key
	incr nextId
    }
    dict set seen $key 1
    return $ids($key)
}

proc ::tkdroid::a11y::clean {s} {
    variable maxText
    set s [string map [list \t " " \n " " \r " "] $s]
    if {[string length $s] > $maxText} {set s "[string range $s 0 $maxText-1]..."}
    return $s
}

proc ::tkdroid::a11y::intersect {r clip} {
    lassign $r x1 y1 x2 y2
    lassign $clip cx1 cy1 cx2 cy2
    set x1 [expr {max($x1, $cx1)}]; set y1 [expr {max($y1, $cy1)}]
    set x2 [expr {min($x2, $cx2)}]; set y2 [expr {min($y2, $cy2)}]
    if {$x1 >= $x2 || $y1 >= $y2} {return {}}
    return [list $x1 $y1 $x2 $y2]
}

proc ::tkdroid::a11y::emit {key parent rect role flags text value} {
    variable lines
    set id [idFor $key]
    lassign $rect x1 y1 x2 y2
    lappend lines [join [list $id $parent $x1 $y1 $x2 $y2 $role $flags [clean $text] [clean $value]] \t]
    return $id
}

# text shown by a widget: -text, or the value of its -textvariable
proc ::tkdroid::a11y::textOf {w} {
    set v ""
    if {![catch {$w cget -textvariable} var] && $var ne "" && [uplevel #0 [list info exists $var]]} {
	return [uplevel #0 [list set $var]]
    }
    catch {$w cget -text} v
    return $v
}

proc ::tkdroid::a11y::isDisabled {w} {
    if {[catch {$w cget -state} st]} {
	return [expr {![catch {$w instate disabled} d] && $d}]
    }
    return [expr {$st eq "disabled"}]
}

proc ::tkdroid::a11y::flagsFor {w base} {
    set f $base
    if {[isDisabled $w]} {set f [string map {x {}} $f]} else {append f e}
    if {[focus] eq $w} {append f f}
    return $f
}

# role, flags, text and value of one widget; role {} = a container that gets no node of its own
proc ::tkdroid::a11y::describe {w cls} {
    switch -- $cls {
	Button - TButton - Menubutton - TMenubutton {
	    return [list button [flagsFor $w x] [textOf $w] ""]
	}
	Label - TLabel - Message {
	    set t [textOf $w]
	    if {$t eq ""} {return {}}
	    return [list text [flagsFor $w ""] $t ""]
	}
	Checkbutton - TCheckbutton {
	    set on 0
	    catch {
		set var [$w cget -variable]
		set val [uplevel #0 [list set $var]]
		set on [expr {$val eq [expr {$cls eq "TCheckbutton" ? [$w cget -onvalue] : [$w cget -onvalue]}]}]
	    }
	    return [list checkbox [flagsFor $w xc[expr {$on ? "k" : ""}]] [textOf $w] [expr {$on ? "on" : "off"}]]
	}
	Radiobutton - TRadiobutton {
	    set on 0
	    catch {
		set var [$w cget -variable]
		set on [expr {[uplevel #0 [list set $var]] eq [$w cget -value]}]
	    }
	    return [list radio [flagsFor $w xc[expr {$on ? "k" : ""}]] [textOf $w] [expr {$on ? "selected" : ""}]]
	}
	Entry - TEntry - Spinbox - TSpinbox - TCombobox {
	    set fl t
	    if {![catch {$w cget -show} sh] && $sh ne ""} {append fl p}
	    set v [expr {[string match *p $fl] || [string match *p* $fl] ? [string repeat "•" [string length [$w get]]] : [$w get]}]
	    return [list edit [flagsFor $w $fl] "" $v]
	}
	Text {
	    # an editable text box is an edit field; a read-only one is just text to read
	    if {![catch {$w cget -state} st] && $st eq "normal"} {
		return [list edit [flagsFor $w mlt] "" [$w get 1.0 end-1c]]
	    }
	    return [list text [flagsFor $w "ml"] [$w get 1.0 end-1c] ""]
	}
	Listbox {return [list list [flagsFor $w l] "" ""]}
	Treeview {return [list list [flagsFor $w l] "" ""]}
	Scale - TScale {
	    set v ""; catch {set v [$w get]}
	    return [list slider [flagsFor $w l] "" $v]
	}
	TProgressbar - Progressbar {
	    set v ""; catch {set v [$w cget -value]}
	    return [list progress [flagsFor $w ""] "" $v]
	}
	default {return {}}
    }
}

# children that are not windows
proc ::tkdroid::a11y::items {w cls parent rect} {
    set ox [winfo rootx $w]; set oy [winfo rooty $w]
    switch -- $cls {
	Listbox {
	    set n [$w size]
	    set sel [$w curselection]
	    for {set i [$w nearest 0]} {$i < $n} {incr i} {
		lassign [$w bbox $i] bx by bw bh
		if {$bw eq ""} break
		set r [intersect [list [expr {$ox+$bx}] [expr {$oy+$by}] [expr {$ox+$bx+$bw}] [expr {$oy+$by+$bh}]] $rect]
		if {$r eq ""} {if {$by > [winfo height $w]} break; continue}
		emit "$w#$i" $parent $r listitem [expr {$i in $sel ? "xes" : "xe"}] [$w get $i] ""
	    }
	}
	Treeview {
	    set sel [$w selection]
	    set todo [$w children {}]
	    set count 0
	    while {[llength $todo] && [incr count] < 300} {
		set it [lindex $todo 0]; set todo [lrange $todo 1 end]
		if {[$w item $it -open]} {set todo [concat [$w children $it] $todo]}
		set bb [$w bbox $it]
		if {![llength $bb]} continue
		lassign $bb bx by bw bh
		set r [intersect [list [expr {$ox+$bx}] [expr {$oy+$by}] [expr {$ox+$bx+$bw}] [expr {$oy+$by+$bh}]] $rect]
		if {$r eq ""} continue
		emit "$w#$it" $parent $r listitem [expr {$it in $sel ? "xes" : "xe"}] [$w item $it -text] [join [$w item $it -values] ", "]
	    }
	}
	Canvas {
	    set cx [$w canvasx 0]; set cy [$w canvasy 0]
	    set widgetClickable [expr {[bind $w <ButtonRelease-1>] ne "" || [bind $w <Button-1>] ne ""}]
	    set count 0
	    foreach it [$w find all] {
		if {[$w type $it] ne "text"} continue
		if {[incr count] > 400} break
		set bb [$w bbox $it]
		if {![llength $bb]} continue
		lassign $bb bx1 by1 bx2 by2
		set r [intersect [list [expr {$ox + int($bx1 - $cx)}] [expr {$oy + int($by1 - $cy)}] [expr {$ox + int($bx2 - $cx)}] [expr {$oy + int($by2 - $cy)}]] $rect]
		if {$r eq ""} continue
		set txt [$w itemcget $it -text]
		if {$txt eq ""} continue
		set clickable $widgetClickable
		foreach tg [$w gettags $it] {if {[$w bind $tg] ne ""} {set clickable 1}}
		if {[$w bind $it] ne ""} {set clickable 1}
		emit "$w#$it" $parent $r [expr {$clickable ? "button" : "text"}] [expr {$clickable ? "xe" : "e"}] $txt ""
	    }
	}
    }
}

proc ::tkdroid::a11y::walk {w parent clip} {
    if {![winfo exists $w] || ![winfo ismapped $w]} return
    set x [winfo rootx $w]; set y [winfo rooty $w]
    set rect [intersect [list $x $y [expr {$x + [winfo width $w]}] [expr {$y + [winfo height $w]}]] $clip]
    if {$rect eq ""} return
    set cls [winfo class $w]
    variable override
    if {[info exists override($w)] && [dict exists $override($w) hidden] && [dict get $override($w) hidden]} return
    if {[catch {describe $w $cls} d]} {set d {}}
    if {[info exists override($w)] && ([dict exists $override($w) name] || [dict exists $override($w) role])} {
	# the app's own words win: an icon-only button, a custom-drawn widget
	set o $override($w)
	lassign [expr {[llength $d] ? $d : {view e "" ""}}] orole oflags otext ovalue
	set d [list [expr {[dict exists $o role] ? [dict get $o role] : $orole}] $oflags \
		[expr {[dict exists $o name] ? [dict get $o name] : $otext}] $ovalue]
    }
    if {[llength $d]} {
	lassign $d role flags text value
	set parent [emit $w $parent $rect $role $flags $text $value]
    } elseif {[winfo toplevel $w] eq $w && $w ne "."} {
	set title ""; catch {set title [wm title $w]}
	set parent [emit $w $parent $rect window e $title ""]
    }
    catch {items $w $cls $parent $rect}
    foreach k [winfo children $w] {walk $k $parent $rect}
}

# ::tkdroid::accessible window ?-name text? ?-role role? ?-hidden bool?
#   Say what a widget is, in the screen reader's words: an icon-only button gets a name; a custom-drawn
#   widget gets a role; decorative widgets can be hidden. Roles: button checkbox radio edit text list
#   slider progress view.
proc ::tkdroid::accessible {w args} {
    variable ::tkdroid::a11y::override
    set d [dict create]
    foreach {k v} $args {
	if {$k ni {-name -role -hidden}} {error "bad option \"$k\": must be -name, -role or -hidden"}
	dict set d [string range $k 1 end] $v
    }
    set override($w) $d
    return
}

# the whole screen as snapshot text
proc ::tkdroid::a11y::snapshot {} {
    variable lines; variable seen; variable ids; variable keys
    set lines {}
    set seen {}
    set screen [::tkdroid::screen]
    lassign $screen sw sh
    walk . 0 [list 0 0 $sw $sh]
    foreach k [array names ids] {
	if {![dict exists $seen $k]} {unset keys($ids($k)); unset ids($k)}
    }
    return [join $lines \n]
}

proc ::tkdroid::a11y::enable {on} {
    variable enabled; variable job; variable last
    set enabled $on
    catch {after cancel $job}
    set job ""
    set last ""
    if {$on} {tick}
}

# while a screen reader is running: re-describe the screen when something was drawn
proc ::tkdroid::a11y::tick {} {
    variable enabled; variable job; variable last; variable lastFrames
    set job ""
    if {!$enabled} return
    set f [dict get [::tkdroid::stats] frames]
    if {$f != $lastFrames || $last eq ""} {
	set lastFrames $f
	if {![catch snapshot s] && $s ne $last} {
	    set last $s
	    ::tkdroid::a11ysend $s
	}
    }
    set job [after 250 ::tkdroid::a11y::tick]
}

# ---- actions from the screen reader (the numbers are android.view.accessibility.AccessibilityNodeInfo's)
proc ::tkdroid::a11y::findRect {id} {
    variable keys
    if {![info exists keys($id)]} {return {}}
    set w [lindex [split $keys($id) #] 0]
    return $w
}

# diagnostics: when the app folder holds a file a11y-debug, say what happens (an on-screen message)
proc ::tkdroid::a11y::dbg {msg} {
    variable debug
    if {![info exists debug]} {
	set debug [expr {[info exists ::tkdroid::appdir] && [file exists [file join $::tkdroid::appdir a11y-debug]]}]
    }
    if {$debug} {catch {::tkdroid::request toast text $msg}}
}

proc ::tkdroid::a11y::tapAt {cx cy} {
    set under [winfo containing $cx $cy]
    dbg "Tcl: tap at $cx,$cy over [expr {$under eq "" ? "nothing" : "$under ([winfo class $under])"}]"
    ::tkdroid::touch down $cx $cy
    after 60 [list ::tkdroid::touch up $cx $cy]
}

proc ::tkdroid::a11y::action {id action text} {
    variable keys; variable last
    dbg "Tcl: action $action id $id text '$text'"
    # A click arrives with the node's centre (the host's own snapshot): tap there. No bookkeeping needed.
    if {$action == 16 && [regexp {^(-?\d+) (-?\d+)$} $text -> cx cy]} {
	tapAt $cx $cy
	return
    }
    if {![info exists keys($id)]} return
    set key $keys($id)
    set rect {}
    foreach l [split $last \n] {
	set f [split $l \t]
	if {[lindex $f 0] == $id} {set rect [lrange $f 2 5]; break}
    }
    set w [lindex [split $key #] 0]
    if {![winfo exists $w]} return
    switch -- $action {
	16 {	;# click without coordinates: a tap at the centre of the node
	    if {$rect eq ""} return
	    lassign $rect x1 y1 x2 y2
	    tapAt [expr {($x1 + $x2) / 2}] [expr {($y1 + $y2) / 2}]
	}
	1 {catch {focus -force $w}}	;# focus
	2097152 {			;# set text
	    if {[winfo class $w] in {Entry TEntry Spinbox TSpinbox TCombobox}} {
		$w delete 0 end; $w insert 0 $text
	    } elseif {[winfo class $w] eq "Text"} {
		$w delete 1.0 end; $w insert 1.0 $text
	    }
	}
	4096 - 8192 {			;# scroll forward / backward
	    set dir [expr {$action == 4096 ? 1 : -1}]
	    catch {$w yview scroll $dir pages}
	    if {[winfo class $w] in {Scale TScale}} {
		catch {$w set [expr {[$w get] + $dir * max(1, [$w cget -resolution])}]}
	    }
	}
    }
}
