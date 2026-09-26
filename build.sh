#!/bin/sh
set -eu
# Compatibility entry point; use one build system on all supported platforms.
srcdir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
"$srcdir/configure" "$@"
exec make all
