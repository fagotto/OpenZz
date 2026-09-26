#!/bin/sh
set -eu
if test -z "${PYTHON3:-}" || ! "$PYTHON3" -c 'import sys; sys.exit(sys.version_info < (3, 9))'; then
    echo 'SKIP: Python 3.9+ required for executable reference documentation'
    exit 77
fi
exec "$PYTHON3" "$(dirname "$0")/zz-reference-tests.py"
