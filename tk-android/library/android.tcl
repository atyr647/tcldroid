# android.tcl --
#
#	Touch policy for Tk on Android: what a finger means to the classic widgets.
#	Sourced at the end of tk.tcl (see upstream/patches/0003) when the Android
#	platform layer is present. Everything here is ordinary Tk bindings and
#	procedures, so an application can change or switch any of it off.
#
#	  * Dragging on a text, listbox or treeview scrolls it (with momentum), and
#	    a tap clicks. Set ::tkdroid::touchScroll 0 for the classic mouse feel.
#	  * Long press on a text-entry widget selects the word under the finger,
#	    keeps extending the selection while the finger drags, and shows the
#	    system's Cut/Copy/Paste/Select-all toolbar when it lifts.
#	  * Long press elsewhere generates the virtual event <<LongPress>>.
#	  * A two-finger pinch generates <<PinchStart>>, <<Pinch>> and <<PinchEnd>>;
#	    %d is the cumulative scale factor (1.0 = unchanged) and %x %y the focal point.
#
#	  * The Activity lifecycle arrives as <<AndroidPause>>, <<AndroidSaveState>>,
#	    <<AndroidResume>> and <<AndroidLowMemory>> on ".".
#
#	The native side calls ::tkdroid::native; nothing here runs unless the
#	platform reports a gesture.

namespace eval ::tkdroid {
    variable touchScroll 1	;# 0: leave Text/Listbox/Treeview mouse bindings alone
    variable T			;# the touch in progress
    array set T {mode idle w {} x0 0 y0 0 ly 0 lx 0 t 0 vy 0 vx 0 acc 0 axis y
	menu {} fling {}}
    variable orig		;# class bindings that the wrappers below call
    array set orig {}
    variable textClasses {Text Entry Spinbox TEntry TCombobox TSpinbox}
}

# ---- helpers -------------------------------------------------------------

proc ::tkdroid::slop {} {
    return [expr {max(4, int([winfo pixels . 0.05i]))}]	;# ~8 dp
}

# Run a saved class binding as if Tk had fired it.
proc ::tkdroid::run {key w x y} {
    variable orig
    if {![info exists orig($key)] || $orig($key) eq ""} return
    set X [expr {[winfo rootx $w] + $x}]
    set Y [expr {[winfo rooty $w] + $y}]
    uplevel #0 [string map [list %% % %W $w %x $x %y $y %X $X %Y $Y %b 1 %s 256] $orig($key)]
}

# Pixels per scroll unit: 1 for text (it scrolls by pixels), a row for lists.
proc ::tkdroid::unit {w} {
    if {[winfo class $w] eq "Text"} {return 1}
    foreach y {4 20 60 120} {
	catch {
	    set i [expr {[winfo class $w] eq "Listbox" ? [$w nearest $y] : [$w identify item 20 $y]}]
	    set bb [$w bbox $i]
	    if {[llength $bb] == 4 && [lindex $bb 3] > 0} {return [lindex $bb 3]}
	}
    }
    return [expr {int([font metrics TkDefaultFont -linespace] * 1.5)}]
}

# Scroll by dpx pixels (+ = show further down/right); keeps the fractional part.
proc ::tkdroid::scrollBy {w axis dpx} {
    variable T
    set u [unit $w]
    set T(acc) [expr {$T(acc) + $dpx}]
    set n [expr {int($T(acc) / $u)}]
    if {$n == 0} {return 0}
    set T(acc) [expr {$T(acc) - $n * $u}]
    set view [expr {$axis eq "x" ? "xview" : "yview"}]
    set before [$w $view]
    if {$u == 1} {
	$w $view scroll $n pixels
    } else {
	$w $view scroll $n units
    }
    return [expr {$before ne [$w $view]}]
}

# ---- one-finger drag: scroll instead of select ------------------------------------

proc ::tkdroid::Down {w x y} {
    variable T
    variable touchScroll
    catch {after cancel $T(fling)}
    set T(fling) {}
    array set T [list w $w x0 $x y0 $y lx $x ly $y t [clock microseconds] vy 0.0 vx 0.0 \
	    acc 0 axis y menu {}]
    if {$touchScroll} {
	set T(mode) pending	;# decided by the first movement, or by the lift (a tap)
    } else {
	set T(mode) direct
	run [winfo class $w],<Button-1> $w $x $y
    }
}

proc ::tkdroid::Move {w x y} {
    variable T
    set c [winfo class $w]
    switch -- $T(mode) {
	pending {
	    if {hypot($x - $T(x0), $y - $T(y0)) <= [slop]} return
	    set T(mode) scroll
	    set T(axis) [expr {$c eq "Text" && abs($x - $T(x0)) > abs($y - $T(y0)) ? "x" : "y"}]
	    set T(lx) $x; set T(ly) $y
	    # fall through to scroll
	    scroll $w $x $y
	}
	scroll { scroll $w $x $y }
	default { run $c,<B1-Motion> $w $x $y }
    }
}

proc ::tkdroid::scroll {w x y} {
    variable T
    set now [clock microseconds]
    set dt [expr {max(1.0, ($now - $T(t)) / 1000.0)}]
    if {$T(axis) eq "x"} {set d [expr {$T(lx) - $x}]} else {set d [expr {$T(ly) - $y}]}
    set T(t) $now; set T(lx) $x; set T(ly) $y
    # smoothed velocity in pixels per millisecond
    set T(vy) [expr {0.7 * $T(vy) + 0.3 * ($d / $dt)}]
    scrollBy $w $T(axis) $d
}

proc ::tkdroid::Up {w x y} {
    variable T
    set c [winfo class $w]
    switch -- $T(mode) {
	pending {		;# a tap
	    run $c,<Button-1> $w $T(x0) $T(y0)
	    run $c,<ButtonRelease-1> $w $x $y
	}
	scroll {
	    # momentum only if the finger was still moving when it lifted
	    if {([clock microseconds] - $T(t)) < 80000 && abs($T(vy)) > 0.15} {
		set T(fling) [after 16 [list ::tkdroid::Fling $w]]
	    }
	}
	default { run $c,<ButtonRelease-1> $w $x $y }
    }
    set T(mode) idle
}

proc ::tkdroid::Leave {w x y} {
    variable T
    if {$T(mode) in {pending scroll}} return	;# no auto-scan while the finger scrolls
    run [winfo class $w],<B1-Leave> $w $x $y
}

proc ::tkdroid::Fling {w} {
    variable T
    set T(fling) {}
    if {![winfo exists $w]} return
    set d [expr {$T(vy) * 16}]
    if {abs($d) < 0.4} return
    if {![scrollBy $w $T(axis) $d] && abs($d) >= [unit $w]} return	;# hit an end
    set T(vy) [expr {$T(vy) * 0.95}]
    set T(fling) [after 16 [list ::tkdroid::Fling $w]]
}

# ---- long press and the text toolbar --------------------------------------------------

proc ::tkdroid::LongPress {X Y} {
    variable T
    variable textClasses
    set w [winfo containing $X $Y]
    if {$w eq ""} return
    set x [expr {$X - [winfo rootx $w]}]
    set y [expr {$Y - [winfo rooty $w]}]
    set c [winfo class $w]
    if {$c ni $textClasses} {
	catch {event generate $w <<LongPress>> -x $x -y $y}
	return
    }
    if {$T(mode) eq "pending" && $T(w) eq $w} {
	run $c,<Button-1> $w $T(x0) $T(y0)	;# the press we were still holding back
    }
    set T(mode) select
    if {[$w cget -state] eq "disabled"} {
	set T(mode) idle
	return
    }
    focus $w
    run $c,<Double-Button-1> $w $x $y	;# select the word, exactly as a double click would
    set T(menu) [list $X $Y]
}

# bound to <ButtonRelease-1> on "all": the lift that ends a long press shows the toolbar
proc ::tkdroid::AnyUp {} {
    variable T
    if {$T(menu) eq ""} return
    lassign $T(menu) X Y
    set T(menu) {}
    after idle [list ::tkdroid::ShowMenu $X $Y]
}

proc ::tkdroid::ShowMenu {X Y} {
    set w [focus]
    if {$w eq "" || ![winfo exists $w]} return
    set flags 8				;# select all
    if {[winfo class $w] eq "Text"} {
	set has [llength [$w tag ranges sel]]
	set editable [expr {[$w cget -state] eq "normal"}]
    } else {
	set has [$w selection present]
	set editable [expr {[$w cget -state] eq "normal"}]
    }
    if {$has} {
	set flags [expr {$flags | 2 | ($editable ? 1 : 0)}]
    }
    if {$editable && ![catch {::clipboard get} clip] && $clip ne ""} {
	set flags [expr {$flags | 4}]
    }
    textmenu $X $Y $flags
}

# A choice in the toolbar (the code is TKA_MENU_*).
proc ::tkdroid::TextAction {code} {
    set w [focus]
    if {$w eq "" || ![winfo exists $w]} return
    switch -- $code {
	1 { event generate $w <<Cut>> }
	2 { event generate $w <<Copy>> }
	4 { event generate $w <<Paste>> }
	8 {
	    event generate $w <<SelectAll>>
	    set X [expr {[winfo rootx $w] + [winfo width $w] / 2}]
	    set Y [expr {[winfo rooty $w] + [winfo height $w] / 2}]
	    after idle [list ::tkdroid::ShowMenu $X $Y]
	}
    }
}

# ---- pinch ---------------------------------------------------------------------------------

proc ::tkdroid::Pinch {phase X Y scale1000} {
    set w [winfo containing $X $Y]
    if {$w eq ""} {set w .}
    set x [expr {$X - [winfo rootx $w]}]
    set y [expr {$Y - [winfo rooty $w]}]
    set ev [lindex {<<PinchStart>> <<Pinch>> <<PinchEnd>>} $phase]
    catch {event generate $w $ev -x $x -y $y -data [format %.3f [expr {$scale1000 / 1000.0}]]}
}

# ---- Activity lifecycle ------------------------------------------------------------------------
#
#	<<AndroidPause>>/<<AndroidSaveState>>  the app is leaving the foreground: save now (Android
#	    may kill the process any time after this; the Activity waits briefly for the handlers)
#	<<AndroidResume>>                      back in the foreground
#	<<AndroidLowMemory>>                   the system wants memory back: drop caches
#	All are generated on ".", so `bind . <<AndroidPause>> {...}` or `bind all` receives them.

proc ::tkdroid::Lifecycle {kind} {
    set events [lindex {{<<AndroidPause>> <<AndroidSaveState>>} {<<AndroidResume>>} {<<AndroidLowMemory>>}} $kind]
    foreach ev $events {
	if {[catch {event generate . $ev} err]} {catch {after idle [list error $err]}}
	update idletasks
    }
}

# ---- platform services (file picker, share, open a link, permissions, notifications) ----------------------
#
# Each asks Android to do something, possibly showing system UI, and reports back LATER: the -command
# script is called with two more arguments, a status (ok, cancelled, denied or error) and a list of
# results. The app keeps running meanwhile. Picked files are copied into the app's cache directory first,
# so the results are ordinary file paths that any Tcl file command can read.
#
#   ::tkdroid::pickfile  -command cmd ?-types {mime ...}? ?-multiple 1?       results: local file paths
#   ::tkdroid::savefile  -source file -name name -command cmd ?-type mime?     results: the saved name
#   ::tkdroid::share     ?-text t? ?-file path? ?-type mime? ?-title t? ?-command cmd?
#   ::tkdroid::openurl   url ?-command cmd?                           (http, https, mailto, tel, geo, sms)
#   ::tkdroid::permission request perm ?perm ...? -command cmd         results: the granted permissions
#   ::tkdroid::notify    -title t -text t ?-command cmd?
#   ::tkdroid::permitted perm                                          1 if granted (synchronous)

namespace eval ::tkdroid { variable pending; array set pending {} }

proc ::tkdroid::async {kind opts cmd} {
    variable pending
    set id [request $kind {*}$opts]
    if {$cmd ne ""} {set pending($id) $cmd}
    return $id
}

proc ::tkdroid::Result {id status payload} {
    variable pending
    if {![info exists pending($id)]} return
    set cmd $pending($id)
    unset pending($id)
    set results [expr {$payload eq "" ? {} : [split $payload \n]}]
    uplevel #0 [list {*}$cmd $status $results]
}

# split "-opt value ..." into a dict, rejecting unknown options
proc ::tkdroid::options {optlist allowed} {
    set d [dict create]
    foreach {k v} $optlist {
	if {$k ni $allowed} {error "bad option \"$k\": must be [join $allowed {, }]"}
	dict set d [string range $k 1 end] $v
    }
    return $d
}

proc ::tkdroid::pickfile {args} {
    set o [options {*}[list $args {-command -types -multiple}]]
    if {![dict exists $o command]} {error "pickfile needs -command"}
    set a {}
    if {[dict exists $o types]} {lappend a types [join [dict get $o types] ,]}
    if {[dict exists $o multiple]} {lappend a multiple [expr {[dict get $o multiple] ? 1 : 0}]}
    async pickfile $a [dict get $o command]
}

proc ::tkdroid::savefile {args} {
    set o [options {*}[list $args {-command -source -name -type}]]
    foreach need {command source name} {if {![dict exists $o $need]} {error "savefile needs -$need"}}
    if {![file readable [dict get $o source]]} {error "cannot read [dict get $o source]"}
    set a [list source [file normalize [dict get $o source]] name [dict get $o name]]
    if {[dict exists $o type]} {lappend a type [dict get $o type]}
    async savefile $a [dict get $o command]
}

proc ::tkdroid::share {args} {
    set o [options {*}[list $args {-text -file -type -title -command}]]
    if {![dict exists $o text] && ![dict exists $o file]} {error "share needs -text or -file"}
    set a {}
    foreach k {text type title} {if {[dict exists $o $k]} {lappend a $k [dict get $o $k]}}
    if {[dict exists $o file]} {
	if {![file readable [dict get $o file]]} {error "cannot read [dict get $o file]"}
	lappend a file [file normalize [dict get $o file]]
    }
    async share $a [expr {[dict exists $o command] ? [dict get $o command] : ""}]
}

proc ::tkdroid::openurl {url args} {
    set o [options {*}[list $args {-command}]]
    async openurl [list url $url] [expr {[dict exists $o command] ? [dict get $o command] : ""}]
}

proc ::tkdroid::permission {sub args} {
    if {$sub ne "request"} {error "bad subcommand \"$sub\": must be request"}
    set perms {}
    set cmd ""
    while {[llength $args]} {
	set a [lindex $args 0]
	if {$a eq "-command"} {set cmd [lindex $args 1]; set args [lrange $args 2 end]} else {lappend perms $a; set args [lrange $args 1 end]}
    }
    if {![llength $perms] || $cmd eq ""} {error "usage: permission request perm ?perm ...? -command cmd"}
    async permission [list permissions [join $perms ,]] $cmd
}

proc ::tkdroid::notify {args} {
    set o [options {*}[list $args {-title -text -command}]]
    foreach need {title text} {if {![dict exists $o $need]} {error "notify needs -$need"}}
    async notify [list title [dict get $o title] text [dict get $o text]] [expr {[dict exists $o command] ? [dict get $o command] : ""}]
}

# ---- entry point for the platform layer ------------------------------------------------------

proc ::tkdroid::native {kind a b c d} {
    switch -- $kind {
	longpress { LongPress $a $b }
	pinch { Pinch $a $b $c $d }
	textaction { TextAction $a }
	lifecycle { Lifecycle $a }
	result { Result $a $b $c }
	a11yenable { ::tkdroid::a11y::enable $a }
	a11yaction { ::tkdroid::a11y::action $a $b $c }
    }
}

proc ::tkdroid::install {} {
    variable orig
    variable textClasses
    foreach c {Text Listbox Treeview} {
	foreach seq {<Button-1> <B1-Motion> <ButtonRelease-1> <B1-Leave>} {
	    set orig($c,$seq) [bind $c $seq]
	}
	bind $c <Button-1> {::tkdroid::Down %W %x %y}
	bind $c <B1-Motion> {::tkdroid::Move %W %x %y}
	bind $c <ButtonRelease-1> {::tkdroid::Up %W %x %y}
	bind $c <B1-Leave> {::tkdroid::Leave %W %x %y}
    }
    foreach c $textClasses {
	set orig($c,<Double-Button-1>) [bind $c <Double-Button-1>]
    }
    bind all <ButtonRelease-1> {+::tkdroid::AnyUp}
    # Hardware keyboards: Ctrl+A selects all, as on every Android text field (Tk's X11 default is the
    # Emacs "go to line start"; Home does that here).
    catch {event delete <<LineStart>> <Control-Key-a>}
    event add <<SelectAll>> <Control-Key-a>
}

::tkdroid::install

# screen-reader support (loaded here so a script can also call ::tkdroid::a11y::snapshot directly)
source -encoding utf-8 [file join $::tk_library a11y.tcl]

# Starkit-style apps say "package require starkit" (see starkit.tcl)
package ifneeded starkit 1.3.3 [list source -encoding utf-8 [file join $::tk_library starkit.tcl]]
