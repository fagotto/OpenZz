#!/bin/sh
# Standalone checks deliberately exclude the legacy global runtime.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ozz-sanitizers.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
for test in checked-context checked-grammar checked-stress; do
    ${CC:-cc} -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O1 \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -I"$root/src" "$root/src/zz_context.c" \
        "$root/src/zz_checked_parse.c" "$root/src/zz_checked_eval.c" \
        "$root/testsuite/$test.c" -o "$work/$test"
    "$work/$test"
done
