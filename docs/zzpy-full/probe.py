#!/usr/bin/env python3
"""Exploratory coverage probes, not a claim of Python conformance.
Run with the target CPython; only compile input/output, never execute user code.
"""
import argparse
import ast
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile

CASES = {
    'baseline': 'x = 2 + 3 * 4\nprint(x)\n',
    'nested_blocks': 'x = 2\nwhile x > 0:\n    if x == 1:\n        print(x)\n    x = x - 1\n',
    'soft_names': 'match = 1\ncase = 2\ntype = 3\nprint(match, case, type)\n',
    'dunder_name': '__name__ = "module"\n',
    'unicode_name': 'caffè = 1\n',
    'multiline_expr': 'x = (1 +\n     2)\n',
    'explicit_join': 'x = 1 + \\\n    2\n',
    'tabs': 'if True:\n\tpass\n',
    'triple_string': 'if True:\n    s = """a\nb"""\n',
    'raw_bytes': 'a = r"C:\\temp"\nb = b"abc"\n',
    'adjacent_strings': 's = "a" "b"\n',
    'fstring': 'x = 1\ns = f"{x + 1!r:>10}"\n',
    'tstring': 'x = 1\ns = t"{x}"\n',
    'numeric_forms': 'a = 0xFF\nb = 1_000\nc = 1.5e-3\nd = 2j\n',
    'power': 'a = -2**2\nb = 2**-1\n',
    'tuple': 'a = (1,)\n',
    'dict_set_list': 'a = {"x": 1}\nb = {1, 2}\nc = [1, 2]\n',
    'comprehension': 'a = [x * 2 for x in range(4) if x % 2]\n',
    'generator': 'a = sum(x for x in range(4))\n',
    'call_arguments': 'f(1, x=2, *args, **kw)\n',
    'attribute_slice': 'a = obj.value[1:5:2]\n',
    'assignment': 'a, *rest = values\na = b = 1\na += 1\nx: int = 2\n',
    'chained_comparison': 'a = 0 < f() < 3\n',
    'walrus': 'if (x := f()):\n    pass\n',
    'conditional_lambda': 'f = lambda x: x if x else 0\n',
    'inline_suite': 'if True: x = 1; print(x)\n',
    'if_else': 'if True:\n    pass\nelif False:\n    pass\nelse:\n    pass\n',
    'loop_else': 'for x in values:\n    if x:\n        break\nelse:\n    pass\n',
    'decorator_def': '@decorate\ndef f(a, /, b=1, *, c=2, **kw):\n    return a + b\n',
    'class': 'class C(Base, metaclass=Meta):\n    pass\n',
    'async': 'async def f():\n    async with resource() as r:\n        await r.read()\n',
    'try_except_star': 'try:\n    f()\nexcept* ValueError:\n    pass\nfinally:\n    pass\n',
    'match': 'match value:\n    case {"x": x} if x > 0:\n        pass\n    case _:\n        pass\n',
    'type_parameters': 'type Pair[T] = tuple[T, T]\ndef identity[T](x: T) -> T:\n    return x\n',
    'ordinary_import': 'import os\nfrom sys import version as v\n',
    'syntax_module_import': 'import syntax\n',
    'context_invalid_return': 'return 1\n',
    'context_invalid_break': 'break\n',
    'context_invalid_await': 'await f()\n',
    'invalid_comparison': 'x = 1 < < 2\n',
    'long_literal_300': 's = "' + 'a' * 300 + '"\n',
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--zzpy', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    rows = []
    with tempfile.TemporaryDirectory(prefix='zzpy-full-probes-') as work:
        p = Path(work)/'probe.zzpy'
        for name, source in CASES.items():
            row = dict(name=name, source=source)
            try:
                compile(source, '<probe>', 'exec')
                row['cpython_accepts'] = True
            except SyntaxError as e:
                row.update(cpython_accepts=False, cpython_error=str(e))
            p.write_text(source)
            result = subprocess.run([str(args.zzpy.resolve()), str(p)], cwd=work,
                                    capture_output=True, timeout=10)
            row.update(zzpy_exit=result.returncode,
                       diagnostic=result.stderr.decode(errors='replace').replace(str(p), 'probe.zzpy')[:1200])
            if result.returncode == 0:
                generated = result.stdout.decode('utf-8')
                row['generated'] = generated
                try:
                    compile(generated, '<generated>', 'exec')
                    row['generated_compiles'] = True
                    row['ast_equal'] = ast.dump(ast.parse(source)) == ast.dump(ast.parse(generated))
                except SyntaxError as e:
                    row.update(generated_compiles=False, generated_error=str(e))
            rows.append(row)
    # Demonstrate the current line-by-line indentation strategy on a multiline literal.
    fragment = 's = """a\nb"""\n'
    naive = 'if True:\n' + ''.join('    '+line for line in fragment.splitlines(True))
    correct = 'if True:\n    s = """a\nb"""\n'
    constants = lambda s: [node.value for node in ast.walk(ast.parse(s)) if isinstance(node, ast.Constant) and isinstance(node.value, str)]
    report = dict(python=platform.python_version(), cases=rows,
                  indentation_probe=dict(naive=naive, correct=correct,
                                         naive_constants=constants(naive), correct_constants=constants(correct)))
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n')
    valid = [row for row in rows if row['cpython_accepts']]
    passed = [row for row in valid if row.get('ast_equal') and row.get('generated_compiles')]
    print(json.dumps(dict(python=report['python'], probes=len(rows), valid_python=len(valid),
                          valid_equivalent=len(passed), rejected_valid=sum(row['zzpy_exit']!=0 for row in valid))))


if __name__ == '__main__':
    main()
