#!/bin/sh
set -eu
: "${ZZ:?}"
work=$(mktemp -d "${TMPDIR:-/tmp}/openzz-cli.XXXXXX")
trap 'rm -rf "$work"' 0
trap 'exit 1' 1 2 3 15
cd "$work"
printf '/print 42\n' | "$ZZ" -p > output 2>&1
grep '42' output >/dev/null
for length in 249 250 255 256; do
  awk -v n="$length" 'BEGIN { printf "!!"; for(i=2;i<n;i++) printf "x"; printf "\n/print 42\n" }' > input
  status=0
  "$ZZ" < input > output 2>&1 || status=$?
  expected=0
  if [ "$length" -eq 256 ]; then expected=1; fi
  if [ "$status" -ne "$expected" ]; then
    cat output
    echo "Interactive line length $length: status $status, expected $expected"
    exit 1
  fi
  grep '42' output >/dev/null
  if [ "$length" -eq 256 ]; then grep 'too long' output >/dev/null; fi
done
