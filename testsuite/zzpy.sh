#!/bin/sh
set -eu
if test -z "${PYTHON3:-}"; then
    echo 'SKIP: Python 3.9+ is required for the optional ZZPy prototype'
    exit 77
fi
if ! "$PYTHON3" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 9) else 1)'; then
    echo 'SKIP: Python 3.9+ is required for the optional ZZPy prototype'
    exit 77
fi
exec "$PYTHON3" "$(dirname "$0")/zzpy-tests.py"
