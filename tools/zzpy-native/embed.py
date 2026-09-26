#!/usr/bin/env python3
"""Developer-only regeneration; builds use the committed embedded grammar."""
import json
from pathlib import Path
root = Path(__file__).resolve().parents[2]
source = (root/'tools/zzpy-native/base.zz').read_text()
(root/'src/zzpy_base.h').write_text(
    '/* Generated from tools/zzpy-native/base.zz; checked by tests. */\n'
    'static const char zzpy_base[] =\n' +
    ''.join(json.dumps(line)+'\n' for line in source.splitlines(True)
            if not line.lstrip().startswith('!!')) + ';\n')
