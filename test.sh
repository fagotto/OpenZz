#!/bin/sh
set -eu
# Run from the configured build directory (source or out-of-tree).
exec make check "$@"
