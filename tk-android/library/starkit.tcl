# starkit.tcl --
#
#	The part of the Starkit contract that makes sense on Android. A Starkit's main.tcl
#	normally opens with
#
#	    package require starkit
#	    starkit::startup
#
#	On tcldroid the kit's files were unpacked by the packager (tools/pack-app.sh MyApp.kit), so
#	"the kit" is the app folder. This package answers the questions kits ask about themselves:
#
#	  ::starkit::topdir      the app folder (where main.tcl and lib/ are)
#	  ::starkit::mode        "starkit"
#	  ::starkit::infoscript  the path of main.tcl
#	  ::starkit::startup     puts topdir/lib on auto_path (idempotent); returns
#	  ::starkit::autoextend  adds a directory of packages to auto_path
#
#	It does not include Metakit (mk4vfs) or the vfs package: a kit is read when you pack it,
#	not mounted when it runs. See docs/starkit.md.

package provide starkit 1.3.3

namespace eval ::starkit {
    variable topdir [expr {[info exists ::tkdroid::appdir] ? $::tkdroid::appdir : [pwd]}]
    variable mode starkit
    variable infoscript [file join $topdir main.tcl]

    proc autoextend {dir} {
	if {[file isdirectory $dir] && $dir ni $::auto_path} {
	    lappend ::auto_path $dir
	}
	return
    }

    proc startup {} {
	variable topdir
	autoextend $topdir
	autoextend [file join $topdir lib]
	return
    }
}
