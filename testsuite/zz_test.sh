#!/bin/sh
# One test per process: Automake collects failures and supports parallel runs.
set -eu
if [ "$#" -eq 0 ]; then
  exec make check
fi
testfile=$1
: "${ZZ:?ZZ must name the built interpreter}"
: "${TEST_BUILDDIR:?TEST_BUILDDIR must name the build test directory}"
case "$testfile" in /*) ;; *) testfile="$PWD/$testfile" ;; esac
work=$(mktemp -d "${TMPDIR:-/tmp}/openzz-test.XXXXXX")
trap 'rm -rf "$work"' 0
trap 'exit 1' 1 2 3 15
if [ "$(basename "$testfile")" = tagdtor.zz ]; then
  dlname=
  . "$TEST_BUILDDIR/libtagdtor.la"
  if [ -z "$dlname" ]; then
    echo 'Dynamic loading test requires shared libraries'
    exit 77
  fi
  # Libtool supplies the actual platform suffix; copy beside the isolated input.
  cp "$TEST_BUILDDIR/.libs/$dlname" "$work/$dlname"
  sed "s|/aa=/load_lib .*|/aa=/load_lib \"./$dlname\"|" "$testfile" > "$work/input.zz"
else
  cp "$testfile" "$work/input.zz"
fi
awk '/^!!/ { sub(/^!!/, ""); if (toupper($0) ~ /END.OUT/) exit; if (active) {sub(/[ \t]*$/, ""); print}; if (toupper($0) ~ /REFERENCE.OUT/) active=1 }' "$testfile" > "$work/expected.raw"
cd "$work"
status=0
"$ZZ" input.zz > actual.raw 2>&1 || status=$?
# Ignore only the historical nondeterministic diagnostics/address lines.
normalize() {
  awk '{ sub(/[ \t]*$/, ""); if ($0 !~ /^\| +line/ && $0 !~ /^listed/ && $0 !~ /reg_var:[A-Z0-9]*$/) print }' "$1"
}
normalize actual.raw > actual
normalize expected.raw > expected
expected_status=$(sed -n 's/^!! EXPECT_EXIT: //p' "$testfile")
expected_status=${expected_status:-0}
if [ "$status" -ne "$expected_status" ]; then
  cat actual.raw
  echo "Interpreter exited with status $status (expected $expected_status)"
  exit 1
fi
diff -u expected actual
