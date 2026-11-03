# selftest.tcl -- headless Tcl smoke test, run by the Android host on startup
# (and usable unchanged with any tclsh: it needs only HOME, TMPDIR and TZ).
#
# The source is deliberately ASCII-only; non-ASCII data uses \u escapes so the
# test does not depend on how the script file itself is decoded.
#
# Prints one "ok"/"FAIL" line per check and a final machine-readable line:
#   TCLDROID_SMOKE: PASS <n>/<n>      or      TCLDROID_SMOKE: FAIL <p>/<n>

set ::npass 0
set ::nfail 0

proc check {name script {expected ::none}} {
    if {[catch {uplevel #0 $script} got]} {
        incr ::nfail
        puts "FAIL $name: $got"
        return
    }
    if {$expected ne "::none" && $got ne $expected} {
        incr ::nfail
        puts "FAIL $name: expected {$expected}, got {$got}"
        return
    }
    incr ::npass
    puts "ok   $name = [string range $got 0 60]"
}

# Informational only; never affects the verdict.
proc note {name script} {
    catch {uplevel #0 $script} got
    puts "info $name: [string range $got 0 200]"
}

foreach v {HOME TMPDIR TZ} {
    if {![info exists ::env($v)]} {
        puts "FAIL env-$v: not set"
        incr ::nfail
    }
}
if {$::nfail} { puts "TCLDROID_SMOKE: FAIL 0/$::nfail"; return "missing env" }

note platform         {array get tcl_platform}
note default-timezone {list $::env(TZ) [clock format [clock seconds] -format %Z]}
note packages         {lsort [package names]}

# --- core / startup ----------------------------------------------------------
check version         {info patchlevel} 9.0.4
check pointer-size    {set tcl_platform(pointerSize)} 8
check zipfs-library   {string match //zipfs:/* [info library]} 1
check zipfs-init-tcl  {file exists [file join [info library] init.tcl]} 1
check zipfs-mounts    {expr {[llength [tcl::zipfs::list]] > 0}} 1

# --- text / Unicode ------------------------------------------------------------
check encoding-system {encoding system} utf-8
check string-nonbmp   {string length "\U1F600"} 1
set uni "héllo ✓ 日本 \U1F600"
check unicode-file-roundtrip {
    set f [file join $::env(TMPDIR) "tcldroid-é-日.txt"]
    set fd [open $f w]
    fconfigure $fd -encoding utf-8
    puts -nonewline $fd $::uni
    close $fd
    set fd [open $f r]
    fconfigure $fd -encoding utf-8
    set d [read $fd]
    close $fd
    file delete $f
    set d
} $uni

# --- filesystem ------------------------------------------------------------------
check file-home       {expr {[file home] eq $::env(HOME)}} 1
check file-ops {
    set d [file join $::env(TMPDIR) selftest-dir]
    file delete -force $d
    file mkdir $d/a/b
    set fd [open $d/a/b/f.txt w]; puts $fd data; close $fd
    file rename $d/a/b/f.txt $d/a/g.txt
    set r [list [lsort [glob -tails -directory $d/a *]] [file size $d/a/g.txt]]
    file delete -force $d
    set r
} {{b g.txt} 5}

# --- time (Android has no /etc/localtime; tzdata must come from the zipfs lib) ---
check clock-utc       {clock format 0 -format %Y-%m-%dT%H:%M:%S -timezone :UTC} 1970-01-01T00:00:00
check clock-tzdata    {clock scan "2024-03-10 12:00:00" -format "%Y-%m-%d %H:%M:%S" -timezone America/New_York} 1710086400
check clock-env-tz    {clock format 0 -format %H -timezone $::env(TZ); expr 1} 1

# --- language / library ------------------------------------------------------------
check int64           {expr {2**62}} 4611686018427387904
check format          {format %08.3f 3.14159} 0003.142
check regexp          {regexp -inline {(\d+)-(\w+)} "12-ab"} {12-ab 12 ab}
check binary-encode   {binary encode hex abc} 616263
check zlib            {zlib decompress [zlib compress "abc"]} abc
check interp          {interp create i; set r [i eval {expr 1+1}]; interp delete i; set r} 2
check coroutine {
    set x [coroutine co1 apply {{} {yield a; return b}}]
    append x [co1]
} ab

# --- event loop and sockets ------------------------------------------------------------
check after-vwait {
    set ::t 0
    after 50 {set ::t 1}
    set to [after 5000 {set ::t timeout}]
    vwait ::t
    after cancel $to
    set ::t
} 1
proc ::accept {ch addr port} {
    fconfigure $ch -buffering line
    gets $ch l
    puts $ch "echo:$l"
    close $ch
}
check socket-loopback {
    set ::got {}
    set srv [socket -server ::accept -myaddr 127.0.0.1 0]
    set port [lindex [fconfigure $srv -sockname] 2]
    set ::c [socket 127.0.0.1 $port]
    fconfigure $::c -buffering line
    puts $::c hello
    set to [after 5000 {set ::got timeout}]
    fileevent $::c readable {set ::got [gets $::c]}
    vwait ::got
    after cancel $to
    close $::c
    close $srv
    set ::got
} echo:hello

# --- informational: things Android restricts or changes ------------------------------
note exec-sh {exec /system/bin/sh -c {echo hi}}

set total [expr {$::npass + $::nfail}]
puts [expr {$::nfail ? "TCLDROID_SMOKE: FAIL $::npass/$total" : "TCLDROID_SMOKE: PASS $::npass/$total"}]
