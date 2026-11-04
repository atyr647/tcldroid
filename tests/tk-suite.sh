#!/usr/bin/env bash
# Tk's OWN test suite (upstream tests/*.test) against this port, one file per process.
#   usage: tests/tk-suite.sh [file.test ...]       (default: all)      env: JOBS=4, TKDROID_SCREEN
# Needs the test build of libtk (made here). Per-file results: build/tksuite/summary.tsv
#   file  total  passed  skipped  failed   (a file that crashes or times out shows total "CRASH"/"TIMEOUT")
# Full output of each file: build/tksuite/<file>.out.  See docs/tk-test-suite.md for how to read it
# and tests/tk/expected-failures.txt for the triaged known differences.
. "$(dirname "$0")/../tools/env.sh"
# ASAN=1: AddressSanitizer build (finds heap corruption, use-after-free); leaks are not reported
# (the Tcl interpreter is never freed at exit).
if [ -n "${ASAN:-}" ]; then
  SANITIZE=1 SUFFIX=-testasan TKTEST=1 "$TCLDROID_ROOT/tools/build-tk.sh" host >/dev/null
  TKSUFFIX=-testasan
  export LD_PRELOAD="$(gcc -print-file-name=libasan.so)" ASAN_OPTIONS="detect_leaks=0:abort_on_error=0:halt_on_error=1"
else
  SUFFIX=-test TKTEST=1 "$TCLDROID_ROOT/tools/build-tk.sh" host >/dev/null
  TKSUFFIX=-test
fi
TK="$BUILD_DIR/src/tk$TK_VERSION"
OUT="${OUTDIR:-$BUILD_DIR/tksuite}"; rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BUILD_DIR/install/host/lib" TKLIB="$BUILD_DIR/tk-host$TKSUFFIX/libtk9.0.so" LANG=C.UTF-8
export TKDROID_SCREEN="${TKDROID_SCREEN:-1024x768@96}" TCLSH="$BUILD_DIR/install/host/bin/tclsh9.0"
export TKDROID_DESKTOP_WM=1
export DRIVER="$TCLDROID_ROOT/tests/tk/suite-driver.tcl" OUT TK
files=("$@"); [ ${#files[@]} -gt 0 ] || files=($(cd "$TK/tests" && ls *.test))
run_one() {
  f="$1"; w="$OUT/w.$f"; mkdir -p "$w/tmp"; cp -r "$TK/tests/." "$w/tests"
  cd "$w/tests"
  TKTESTS="$w/tests" TMPDIR="$w/tmp" HOME="$w" timeout 300 "$TCLSH" "$DRIVER" "$f" > "$OUT/$f.out" 2>&1 < /dev/null
  rc=$?
  line=$(grep "^$f:" "$OUT/$f.out" | tail -1 | tr '\t' ' ')
  if [ -n "$line" ]; then
    printf '%s\t%s\n' "$f" "$(echo "$line" | sed 's/^[^:]*: *//; s/Total *//; s/Passed *//; s/Skipped *//; s/Failed *//' | tr -s ' ' '\t')"
  elif [ $rc = 124 ]; then printf '%s\tTIMEOUT\t\t\t\n' "$f"
  else printf '%s\tCRASH(rc=%s)\t\t\t\n' "$f" "$rc"; fi
  rm -rf "$w"
}
export -f run_one
printf '%s\n' "${files[@]}" | xargs -P"${JOBS:-4}" -I{} bash -c 'run_one {}' | sort > "$OUT/summary.tsv"
awk -F'\t' 'BEGIN{t=p=s=f=0;c=0} {if ($2 ~ /^[0-9]+$/) {t+=$2;p+=$3;s+=$4;f+=$5; if ($5>0) bad++} else c++} END{printf "files %d  tests %d  passed %d  skipped %d  failed %d  (files with failures %d, crashed/timeouts %d)\n", NR,t,p,s,f,bad,c}' "$OUT/summary.tsv" | tee "$OUT/totals.txt"
"$TCLDROID_ROOT/tests/tk/check-expected.sh" "$OUT" || exit 1
