# Starkit support: a zip-based kit is unpacked by tools/unkit.py, and its main.tcl runs unchanged
# against the starkit shim.
load $::env(TKLIB) Tk
set fails 0
proc check {name cond} {
    global fails
    if {[uplevel 1 [list expr $cond]]} {puts "PASS $name"} else {puts "FAIL $name"; incr fails}
}
set root [file normalize [file join [file dirname [info script]] ../..]]
set tmp [file join [expr {[info exists ::env(TMPDIR)] ? $::env(TMPDIR) : "/tmp"}] tcldroid-kit-test-[pid]]
file delete -force $tmp
file mkdir $tmp/src/lib/app-hello
set f [open $tmp/src/main.tcl w]
puts $f {package require starkit
starkit::startup
package require app-hello
set ::result [list $::starkit::mode [file tail $::starkit::topdir] [hello::greet] [file exists [file join $::starkit::topdir main.tcl]]]}
close $f
set f [open $tmp/src/lib/app-hello/pkgIndex.tcl w]
puts $f {package ifneeded app-hello 1.0 [list source [file join $dir hello.tcl]]}
close $f
set f [open $tmp/src/lib/app-hello/hello.tcl w]
puts $f {package provide app-hello 1.0; namespace eval hello {proc greet {} {return hi}}}
close $f

# a Tcl 9 zipkit: a script stub, then a zip
zipfs mkzip $tmp/body.zip $tmp/src $tmp/src
set out [open $tmp/app.kit wb]
puts -nonewline $out "#!/bin/sh\n# \\\nexec tclsh \"\$0\" \"\$@\"\n"
set in [open $tmp/body.zip rb]; fcopy $in $out; close $in; close $out

check unkit-zip {![catch {exec python3 -I $root/tools/unkit.py $tmp/app.kit $tmp/out} msg]}
check unkit-files {[file exists $tmp/out/main.tcl] && [file exists $tmp/out/lib/app-hello/hello.tcl]}

namespace eval ::tkdroid {variable appdir}
set ::tkdroid::appdir $tmp/out
lappend ::auto_path $tmp/out
check starkit-main {![catch {source $tmp/out/main.tcl} e]}
check starkit-result {$::result eq [list starkit out hi 1]}
check starkit-libpath {[file join $tmp/out lib] in $::auto_path}

# Metakit kits are refused, with the way out
set f [open $tmp/mk.kit wb]; puts -nonewline $f "#!/bin/sh\n# \\\nexec tclkit \"\$0\"\npackage require mk4vfs\n"; close $f
check metakit-refused {[catch {exec python3 -I $root/tools/unkit.py $tmp/mk.kit $tmp/mkout} m] && [string match "*sdx unwrap*" $m]}

# an entry that climbs out of the folder is refused
exec python3 -I -c "
import zipfile
z=zipfile.ZipFile('$tmp/evil.kit','w'); z.writestr('main.tcl','x'); z.writestr('../escape.txt','x'); z.close()"
check path-traversal-refused {[catch {exec python3 -I $root/tools/unkit.py $tmp/evil.kit $tmp/evilout} m] && ![file exists $tmp/escape.txt]}

file delete -force $tmp
puts "[expr {$fails ? "FAILED: $fails" : "all passed"}]"
exit [expr {$fails ? 1 : 0}]
