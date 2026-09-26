"""Execute the reference manual examples against the native ZZ engine."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ZZ = Path(os.environ['ZZ']).resolve()
BUILD = Path(os.environ['TEST_BUILDDIR']).resolve()
CASES = json.loads((ROOT/'docs/zz-reference/examples.json').read_text())
MANUAL = (ROOT/'docs/ZZ_MANUALE_RIFERIMENTO.md').read_text()


def inventory():
    lines = []
    for filename in ('kernel.c', 'zkernel.c'):
        source = (ROOT/'src'/filename).read_text()
        source = re.sub(r'/\*.*?\*/', '', source, flags=re.S)
        source = re.sub(r'//[^\n]*', '', source)
        for match in re.finditer(r'OPEN\((stat|int)\).*?\bEND\b', source, re.S):
            keyword = re.search(r'M\("((?:\\.|[^"\\])*)"\)', match.group())
            if keyword and keyword[1].startswith('/'):
                lines.append(filename+': '+' '.join(match.group().split()))
    return '\n'.join(lines)+'\n'


def main():
    assert inventory() == (ROOT/'docs/zz-reference/native-rules.txt').read_text(), 'Update native command inventory and manual'
    assert len({case['id'] for case in CASES}) == len(CASES)
    plugin = None
    descriptor = BUILD/'libtagdtor.la'
    if descriptor.exists():
        match = re.search(r"^dlname='([^']+)'", descriptor.read_text(), re.M)
        if match:
            plugin = BUILD/'.libs'/match[1]
    env = dict(os.environ)
    env.pop('ZZ_DEFAULT_INCLUDE_FILE', None)
    env.pop('ZZ_INCLUDES', None)
    skipped = 0
    for case in CASES:
        marker = '<!-- example:'+case['id']+' -->\n```zz\n'+case['code']+'```'
        assert marker in MANUAL, f"Manual differs from tested example: {case['id']}"
        for name, content in case['files'].items():
            assert 'File ausiliario `'+name+'`:\n\n```zz\n'+content+'```' in MANUAL
        if case['plugin'] and not plugin:
            print('SKIP load: static build has no dynamic test module')
            skipped += 1
            continue
        with tempfile.TemporaryDirectory(prefix='zz-reference-') as work:
            directory = Path(work)
            for name, content in case['files'].items():
                file = directory/name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text(content)
            if case['plugin']:
                shutil.copyfile(plugin, directory/'modulo.so')
            (directory/'main.zz').write_text(case['code'])
            result = subprocess.run([str(ZZ), 'main.zz'], cwd=directory, env=env,
                                    capture_output=True, timeout=10)
            stdout = result.stdout.decode('utf-8', errors='replace')
            stderr = result.stderr.decode('utf-8', errors='replace')
            output = stdout+stderr
            assert result.returncode == case['status'], (case['id'], result.returncode, output)
            if case['expected'] is not None:
                assert [line.rstrip() for line in stdout.splitlines()] == case['expected'], (case['id'], output)
            if case['contains'] is not None:
                assert case['contains'] in output, (case['id'], output)
            if case['id'] == 'write-rules':
                assert 'ciao' in (directory/'regole.zz').read_text()
            print('PASS', case['id'])
    print(f'{len(CASES)-skipped} examples passed; {skipped} skipped; inventory and manual synchronized')


if __name__ == '__main__':
    main()
