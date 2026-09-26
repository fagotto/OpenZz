#!/bin/sh
set -eu
if test -z "${PYTHON3:-}" || ! "$PYTHON3" -c 'import sys; sys.exit(sys.version_info < (3, 9))'; then
    echo 'SKIP: Python 3.9+ required to validate the native translator output'
    exit 77
fi
exec "$PYTHON3" "$(dirname "$0")/zzpy-native-tests.py"
