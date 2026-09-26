#!/bin/sh
set -eu
cd "$(dirname "$0")"
if command -v glibtoolize >/dev/null 2>&1; then
  LIBTOOLIZE=${LIBTOOLIZE:-glibtoolize}
  export LIBTOOLIZE
fi
mkdir -p m4
exec autoreconf --force --install --verbose
