#!/bin/sh
set -eu
for scenario in name literal-wins no-fallback reverse-order other-context \
  same-prefix same-prefix-second dynamic-context dynamic-fallback reduce-conflict distinct-follow late-distinction factored early-effect complete-effect \
  split-operator atomic-assignment atomic-equality operator-tags typed-name typed-keyword \
  typed-incomplete
do
  output=$(./parser-decisions "$scenario" 2>&1) || { printf '%s\n' "$output"; exit 1; }
  printf '%s\n' "$output"
  case "$scenario" in
    reduce-conflict|late-distinction|split-operator)
      case "$output" in
        *"Ambiguous syntax (2)"*) ;;
        *) echo "missing ambiguity diagnostic: $scenario"; exit 1 ;;
      esac ;;
    no-fallback|reverse-order|early-effect|typed-incomplete|dynamic-fallback)
      case "$output" in
        *"SYNTAX ERROR"*) ;;
        *) echo "missing syntax diagnostic: $scenario"; exit 1 ;;
      esac ;;
  esac
done
