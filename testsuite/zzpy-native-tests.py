"""End-to-end tests: the translator is a C binary; Python validates its output."""
import ast
import contextlib
import io
import itertools
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ['ZZPY']).resolve()
GRAMMAR = (ROOT / 'examples/zzpy-native/grammar.zz').read_text()

class Native(unittest.TestCase):
    def translate(self, source, grammar=GRAMMAR, flags=(), success=True, extra=None):
        with tempfile.TemporaryDirectory() as temp:
            cwd = Path(temp)
            (cwd/'grammar.zz').write_text(grammar)
            (cwd/'input.zzpy').write_text(source)
            for name, text in (extra or {}).items():
                (cwd/name).write_text(text)
            env = dict(os.environ, ZZ_DEFAULT_INCLUDE_FILE='/nonexistent/autoload.zz', ZZ_INCLUDES='/nonexistent')
            result = subprocess.run([str(BINARY), *flags, 'input.zzpy'], cwd=cwd,
                                    env=env, text=True, capture_output=True, timeout=10)
            if success:
                self.assertEqual(result.returncode, 0, result.stderr)
                ast.parse(result.stdout)
            else:
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, '')
            return result

    def execute(self, code, ns=None):
        scope = {} if ns is None else dict(ns)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            exec(compile(code, '<generated>', 'exec'), scope)
        return out.getvalue(), scope

    def test_composed_extensions(self):
        source=(ROOT/'examples/zzpy-native/dynamic.zzpy').read_text()
        out=self.translate(source).stdout
        self.assertIn('while not (i == 0):', out)
        _, ns=self.execute(out)
        self.assertEqual(ns['i'], 0)
        self.assertEqual(ns['v'], {i:i for i in range(10)})

    def test_explicit_import(self):
        r=self.translate('import syntax "custom.zz"\nx = 2\ntozero x:\n    print(x)\n',
                         extra={'custom.zz': GRAMMAR})
        self.assertEqual(self.execute(r.stdout)[0], '1\n0\n')

    def test_command_line_grammar(self):
        r=self.translate('x = 2\ntozero x:\n    print(x)\n', flags=('--grammar','grammar.zz'))
        self.assertEqual(self.execute(r.stdout)[0], '1\n0\n')

    def test_inline(self):
        r=self.translate('syntax zz {\n'+GRAMMAR+'}\nx = 2\ntozero x:\n    print(x)\n')
        self.assertEqual(self.execute(r.stdout)[0], '1\n0\n')

    def test_no_implicit_default_load(self):
        self.translate('x = 1\ntozero x:\n    print(x)\n', success=False)

    def test_import_is_ordered(self):
        self.translate('x = 1\ntozero x:\n    print(x)\nimport syntax\n', success=False)

    def test_missing_import(self):
        self.translate('import syntax "missing.zz"\n', success=False)

    def test_nested_blocks(self):
        code='import syntax\ni = 3\nv = {}\ntozero i:\n    j = 2\n    tozero j:\n        v[i * 10 + j] = i + j\n'
        _, ns=self.execute(self.translate(code).stdout)
        self.assertEqual(ns['v'],{i*10+j:i+j for i in range(3) for j in range(2)})

    def test_new_user_construct(self):
        grammar=GRAMMAR+'''/p_stmt -> "when" p_expr^c ":" p_suite^b {
    pylower if ( c ) : b
}
/p_stmt -> "announce" p_expr^e "__NL" {
    pylower print ( e ) __NL
}
'''
        r=self.translate('import syntax\nwhen True:\n    announce "hello"\n', grammar)
        self.assertEqual(self.execute(r.stdout)[0], 'hello\n')

    def test_late_binding(self):
        source='''syntax zz {
/p_stmt -> "outer" ":" p_suite^b { pylower inner : b }
/p_stmt -> "inner" ":" p_suite^b { pylower if True : b }
}
outer:
    print("first")
syntax zz {
/p_stmt -> "inner" ":" p_suite^b { pylower if False : b }
}
outer:
    print("second")
'''
        self.assertEqual(self.execute(self.translate(source).stdout)[0], 'first\n')

    def test_standard_python_equivalence(self):
        source='''a = 2 + 3 * 4
b = (2 + 3) * 4
c = a // 3
v = {}
if a > 0 and b > 0 or False:
    while c > 0:
        c = c - 1
        v[c] = -c % 3
print(a, b, c, v, "hello", 'world')
'''
        code=self.translate(source).stdout
        self.assertEqual(ast.dump(ast.parse(source)),ast.dump(ast.parse(code)))
        self.assertEqual(self.execute(source)[0],self.execute(code)[0])

    def test_boolean_truth_calls(self):
        for expr in ('a and b and c','(a and b) and c','a or b or c','a and (b or c)'):
            code=self.translate('answer = '+expr+'\n').stdout
            for bits in itertools.product((False,True), repeat=3):
                traces=[]
                for program in ('answer = '+expr, code):
                    trace=[]
                    class Probe:
                        def __init__(self, name, truth): self.name,self.truth=name,truth
                        def __bool__(self): trace.append(self.name); return self.truth
                    ns={name:Probe(name,truth) for name,truth in zip('abc',bits)}
                    _, out=self.execute(program,ns)
                    traces.append((trace,out['answer'].name))
                self.assertEqual(*traces)

    def test_no_execution_during_translation(self):
        r=self.translate('explode()\n')
        self.assertEqual(r.stdout,'explode()\n')

    def test_literals_and_large_ints(self):
        source='x = 123456789012345678901234567890\nprint(x, "é", "a\\\\b", "quote\\\"")\n'
        self.assertEqual(self.execute(source)[0],self.execute(self.translate(source).stdout)[0])

    def test_rejections(self):
        for source in ('if True:\n', 'a = 1\n  b = 2\n', 'if True:\n\tpass\n',
                       'a = (1\n + 2)\n', 'a = 01\n', '1 = 2\n',
                       'a + b = 2\n', 'x = 1.2\n', 'syntax zz {\n',
                       'if True:\n    import syntax\n', 'x = "unterminated\n',
                       '__emit = 2\n', 'x = 1\x00\n', 'True = 1\n',
                       'return\n', 'lambda = 2\n', 'x = "\\xZZ"\n'):
            with self.subTest(source=source): self.translate(source, success=False)

    def test_output_atomicity(self):
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp); (p/'out.py').write_text('preserved\n')
            (p/'in.zzpy').write_text('x = 1\nif True:\n')
            r=subprocess.run([str(BINARY),'-o','out.py','in.zzpy'],cwd=p,capture_output=True,timeout=10)
            self.assertNotEqual(r.returncode,0)
            self.assertEqual((p/'out.py').read_text(),'preserved\n')
            (p/'in.zzpy').write_text('x = 1\n')
            r=subprocess.run([str(BINARY),'-o','out.py','in.zzpy'],cwd=p,capture_output=True,timeout=10)
            self.assertEqual(r.returncode,0,r.stderr)
            self.assertEqual((p/'out.py').read_text(),'x = 1\n')
            os.link(p/'in.zzpy',p/'alias.zzpy')
            r=subprocess.run([str(BINARY),'-o','alias.zzpy','in.zzpy'],cwd=p,capture_output=True,timeout=10)
            self.assertNotEqual(r.returncode,0)
            self.assertEqual((p/'in.zzpy').read_text(),'x = 1\n')

    def test_repeated_import_and_fresh_process(self):
        source='import syntax\nimport syntax\nx = 1\ntozero x:\n    pass\n'
        self.assertEqual(self.execute(self.translate(source).stdout)[1]['x'],0)
        self.translate('x = 1\ntozero x:\n    pass\n',success=False)

    def test_source_limits(self):
        for source in ('#' * 32769, 'x = "'+'a'*401+'"\n',
                       'x = '+ '('*33 + '1'+ ')'*33+'\n',
                       'x = '+ ' + '.join(['1']*2100)+'\n'):
            self.translate(source,success=False)

    def test_string_is_not_native_code(self):
        source='print("!! /include evil.zz ; { } #")\n'
        self.assertEqual(self.execute(self.translate(source).stdout)[0], '!! /include evil.zz ; { } #\n')

    def test_header_matches_grammar(self):
        source=(ROOT/'tools/zzpy-native/base.zz').read_text()
        expected='/* Generated from tools/zzpy-native/base.zz; checked by tests. */\nstatic const char zzpy_base[] =\n'
        expected+=''.join(json.dumps(line)+'\n' for line in source.splitlines(True) if not line.lstrip().startswith('!!'))+';\n'
        self.assertEqual((ROOT/'src/zzpy_base.h').read_text(),expected)

if __name__=='__main__': unittest.main()
