#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/ozz-checked.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ZZ" --checked "$CHECKED_EXAMPLES/increment.zz" > "$work/interpreted"
printf '19\n15\n' > "$work/expected"
cmp "$work/expected" "$work/interpreted"
"$ZZ" --checked --emit-c "$CHECKED_EXAMPLES/increment.zz" > "$work/out.c"
# CC may contain a launcher or compiler options, as in ordinary Automake rules.
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$work/out.c" -o "$work/out"
"$work/out" > "$work/compiled"
cmp "$work/interpreted" "$work/compiled"
"$ZZ" --checked "$CHECKED_EXAMPLES/modules.zz" > "$work/modules"
printf '17\n' > "$work/expected"
cmp "$work/expected" "$work/modules"
cat > "$work/overflow.zz" <<'PROGRAM'
/symbol value : i64 mutable
print(1)
value = -9223372036854775808 * -1
print(value)
PROGRAM
if "$ZZ" --checked "$work/overflow.zz" > "$work/output" 2> "$work/error"; then exit 1; fi
test ! -s "$work/output"
"$ZZ" --checked --emit-c "$work/overflow.zz" > "$work/overflow.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$work/overflow.c" -o "$work/overflow"
if "$work/overflow" > "$work/output" 2> "$work/error"; then exit 1; fi
test ! -s "$work/output"
printf '/symbol x : i64 mutable\nprint(1)\nunknown = 2\n' > "$work/invalid.zz"
if "$ZZ" --checked --emit-c "$work/invalid.zz" > "$work/output" 2> "$work/error"; then exit 1; fi
test ! -s "$work/output"
printf 'print(0)\000print(1)\n' > "$work/nul.zz"
if "$ZZ" --checked "$work/nul.zz" > "$work/output" 2> "$work/error"; then exit 1; fi
test ! -s "$work/output"
