#!/usr/bin/env bash
# Compare the failures in build/tksuite/*.out with tests/tk/expected-failures.txt.
#   usage: tests/tk/check-expected.sh [outdir]     exit status 1 if an UNEXPECTED test failed
. "$(dirname "$0")/../../tools/env.sh"
OUT="${1:-$BUILD_DIR/tksuite}"; EXP="$TCLDROID_ROOT/tests/tk/expected-failures.txt"
declare -A counts; unexpected=0; expected=0
# a summary row that is not numeric (CRASH/TIMEOUT) is a failure of the whole file
while IFS=$'\t' read -r file total rest; do
  case "$total" in
    ''|*[!0-9]*) names="*FILE-$total*";;
    *) names=$( { grep -h "^==== .* FAILED$" "$OUT/$file.out" 2>/dev/null || true; } | sed "s/^==== //; s/ FAILED$//" | awk "{print \$1}" | sort -u);;
  esac
  for t in $names; do
    match=$(awk -v f="$file" -v t="$t" '!/^#/ && NF>=2 && $1==f { pat=$2; gsub(/[.]/,"\\.",pat); gsub(/\*/,".*",pat); if (t ~ "^" pat "$" || $2=="*") {print $3; exit} }' "$EXP")
    if [ -z "$match" ] && [ "${names#\*FILE}" != "$names" ]; then match=$(awk -v f="$file" '!/^#/ && $1==f && $2=="*" {print $3; exit}' "$EXP"); fi
    if [ -n "$match" ]; then expected=$((expected+1)); counts[$match]=$(( ${counts[$match]:-0} + 1 ))
    else unexpected=$((unexpected+1)); echo "UNEXPECTED: $file $t"; fi
  done
done < "$OUT/summary.tsv"
echo "failing tests: $expected expected, $unexpected unexpected"
exit $((unexpected > 0))
