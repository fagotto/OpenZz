#!/usr/bin/env python3
"""Characterize current ZZ conflicts. These are research probes, not conformance tests."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--zz', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
env = dict(os.environ)
env.pop('ZZ_INCLUDES', None)
env.pop('ZZ_DEFAULT_INCLUDE_FILE', None)
rows = []
for source in sorted((Path(__file__).parent/'parser-probes').glob('*.zz')):
    with tempfile.TemporaryDirectory() as work:
        p = Path(work)/'probe.zz'
        p.write_text(source.read_text())
        result = subprocess.run([str(args.zz.resolve()), str(p)], cwd=work, env=env,
                                capture_output=True, text=True, timeout=10)
        rows.append(dict(name=source.name, exit=result.returncode,
                         stdout=result.stdout.replace(work, '<temporary>'),
                         stderr=result.stderr.replace(work, '<temporary>')))
args.output.write_text(json.dumps(rows, indent=2)+'\n')
for row in rows:
    print(row['name'], row['exit'])
