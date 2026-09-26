#!/usr/bin/env python3
"""End-to-end tests: ZZ recognition, AST lowering, and ordinary Python runtime."""
import ast
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "zzpy"))
from zzpy import TranslationError, translate

ZZ = Path(os.environ["ZZ"]).resolve()
SYNTAX = ROOT / "examples" / "zzpy" / "syntax"
IMPORT = 'import syntax control version "1"\n'


def execute(code, extra=None):
    stream = io.StringIO()
    environment = dict(extra or {})
    with contextlib.redirect_stdout(stream):
        exec(compile(code, "generated.py", "exec"), environment)
    return stream.getvalue(), environment


class ZZPyTests(unittest.TestCase):
    def trans(self, source, syntax=SYNTAX, **kwargs):
        return translate(source, ZZ, syntax, filename="test.zzpy", **kwargs)

    def reject(self, source, text=None, syntax=SYNTAX):
        with self.assertRaises(TranslationError) as caught:
            self.trans(source, syntax)
        if text:
            self.assertIn(text, str(caught.exception))

    def test_demo_and_real_dynamic_rules(self):
        result = self.trans((ROOT / "examples/zzpy/dynamic.zzpy").read_text())
        self.assertEqual(execute(result.code)[0], "initial 14\n15\n17\ndone 17\n")
        self.assertEqual(result.modules, ("control@1",))
        self.assertIn('/py_body -> "KW', result.zz_source)
        first_program = result.zz_source.index("\n__ZZPY_BEGIN\n")
        dynamic_rule = result.zz_source.index('/py_body -> "KW')
        self.assertLess(first_program, dynamic_rule)
        self.assertNotIn("unless", result.code)
        self.assertNotIn("until", result.code)

    def test_plain_subset_matches_python(self):
        source = '''x = 2 + 3 * 4
print(x, -x, x // 3, x % 3, x / 2)
while x > 10:
    x = x - 1
if not x == 0 and (x < 20 or False):
    print("value", x, True, False, None)
pass
'''
        self.assertEqual(execute(self.trans(source).code)[0], execute(source)[0])

    def test_before_import_and_no_leak_between_calls(self):
        bad = "unless False:\n    print(1)\n"
        self.reject(bad + IMPORT, "ZZ rejected")
        self.trans(IMPORT + bad)
        self.reject(bad, "ZZ rejected")

    def test_import_after_block(self):
        source = 'if True:\n    print(1)\n' + IMPORT + 'unless False:\n    print(2)\n'
        self.assertEqual(execute(self.trans(source).code)[0], "1\n2\n")

    def test_empty_and_comment_only(self):
        self.assertEqual(execute(self.trans("").code)[0], "")
        self.assertEqual(execute(self.trans("# comment\n").code)[0], "")

    def test_repeated_import_idempotent(self):
        result = self.trans(IMPORT + IMPORT + "unless False:\n    print(3)\n")
        self.assertEqual(result.modules, ("control@1",))
        self.assertEqual(execute(result.code)[0], "3\n")

    def test_no_target_execution_and_single_condition_evaluation(self):
        calls = []
        def tick():
            calls.append("tick")
            return False
        result = self.trans(IMPORT + 'unless tick():\n    print("once")\n')
        self.assertEqual(calls, [])
        self.assertEqual(execute(result.code, {"tick": tick})[0], "once\n")
        self.assertEqual(calls, ["tick"])

    def test_short_circuit_and_native_python_integers(self):
        source = "x = 9223372036854775807 + 1\nprint(x)\nprint(False and missing())\nprint(True or missing())\n"
        self.assertEqual(execute(self.trans(source).code)[0], execute(source)[0])

    def test_boolean_chains_preserve_stateful_truth_tests(self):
        class Value:
            def __init__(self, name, truth, events):
                self.name, self.truth, self.events = name, truth, events
            def __bool__(self):
                self.events.append(self.name)
                return self.truth
        for source in ("x = a and b and c\n", "x = a or b or c\n",
                       "x = (a and b) and c\n", "x = a and (b and c)\n",
                       "x = (a or b) or c\n", "if a and b and c:\n    pass\n"):
            generated = self.trans(source).code
            self.assertEqual(ast.dump(ast.parse(source)), ast.dump(ast.parse(generated)))
            for mask in range(8):
                observed = []
                for code in (source, generated):
                    events = []
                    env = {name: Value(name, bool(mask & (1 << i)), events)
                           for i, name in enumerate("abc")}
                    _, result = execute(code, env)
                    observed.append((events, result["x"].name if "x" in result else None))
                self.assertEqual(observed[0], observed[1], (source, mask))

    def test_string_payload_cannot_inject_zz(self):
        text = '\"; /print 999; /include "evil"\nü # unless until'
        source = "print(" + repr(text) + ")\n"
        result = self.trans(source)
        self.assertNotIn("evil", result.zz_source)
        self.assertEqual(execute(result.code)[0], text + "\n")

    def test_auto_include_environment_is_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            include = Path(directory) / "sideeffect.zz"
            include.write_text('/print "INJECTED"\n')
            old = os.environ.get("ZZ_DEFAULT_INCLUDE_FILE")
            os.environ["ZZ_DEFAULT_INCLUDE_FILE"] = str(include)
            try:
                self.assertEqual(execute(self.trans("print(1)\n").code)[0], "1\n")
            finally:
                if old is None:
                    os.environ.pop("ZZ_DEFAULT_INCLUDE_FILE")
                else:
                    os.environ["ZZ_DEFAULT_INCLUDE_FILE"] = old

    def test_version_and_import_placement(self):
        self.reject('import syntax control version "2"\n', "version mismatch")
        self.reject('if True:\n    ' + IMPORT, "top-level")
        self.reject(IMPORT + 'import syntax control version "2"\n', "conflicting module version")

    def test_lexical_and_subset_boundaries(self):
        for source in ("x = 1.5\n", "x = 0x20\n", "x = 012\n", "def f():\n    pass\n", "x = [1]\n", 'print(f"x")\n', "print(1 < 2 < 3)\n", "if True:\n\tpass\n", "print(\n1)\n", "x = 1\r\n", "x = 1\0\n"):
            with self.subTest(source=source):
                self.reject(source)

    def test_indentation_validation(self):
        for source in ("if True:\nprint(1)\n", "x = 1\n    print(1)\n", "if True:\n    pass\n  pass\n"):
            with self.subTest(source=source):
                self.reject(source)

    def test_original_line_in_zz_error(self):
        self.reject("x = 1\nprint() +\n", "test.zzpy:2")

    def test_budget_failures(self):
        self.reject("#" * 32769, "32 KiB")
        self.reject("x = " + "1 + " * 1200 + "1\n", "token budget")
        self.reject("print(" + "(" * 30 + "1" + ")" * 30 + ")\n", "deep parentheses")

    def test_independent_user_module_without_frontend_changes(self):
        module = {"format": 1, "name": "custom", "version": "1", "rules": [
            {"name": "when", "pattern": ["when", {"capture": "cond", "type": "expr"}, ":", {"capture": "body", "type": "suite"}], "expand": ["if", {"capture": "cond"}, {"capture": "body"}]},
            {"name": "display", "pattern": ["display", {"capture": "value", "type": "expr"}], "expand": ["expr", ["call", ["global", "print"], {"args": [{"capture": "value"}]}]]}
        ]}
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "custom.zzpy.json").write_text(json.dumps(module))
            result = self.trans('import syntax custom version "1"\nwhen True:\n    display 2 + 3 * 4\n', Path(directory))
            self.assertEqual(execute(result.code)[0], "14\n")
            self.assertNotIn("display", result.code)

    def test_module_conflicts_bad_types_and_code_injection(self):
        original = json.loads((SYNTAX / "control.zzpy.json").read_text())
        variants = []
        for edit in (lambda m: m["rules"][1].update(pattern=m["rules"][0]["pattern"]),
                     lambda m: m["rules"][0].update(expand=["if", {"capture": "body"}, {"capture": "body"}]),
                     lambda m: m["rules"][0].update(expand=["exec", ["constant", "evil"]]),
                     lambda m: m["rules"][0].update(pattern=['bad" /print 99', {"capture": "x", "type": "expr"}])):
            module = json.loads(json.dumps(original)); edit(module); variants.append(module)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "control.zzpy.json")
            for module in variants:
                with self.subTest(module=module):
                    path.write_text(json.dumps(module))
                    self.reject(IMPORT, syntax=Path(directory))
            path.write_text('{"name":"control","name":"other"}')
            self.reject(IMPORT, "duplicate JSON key", Path(directory))

    def test_name_captures_and_reused_binding(self):
        module = {"format": 1, "name": "update", "version": "1", "rules": [
            {"name": "increase", "pattern": ["increase", {"capture": "target", "type": "name"}, "by", {"capture": "amount", "type": "expr"}],
             "expand": ["assign", {"capture": "target"}, ["add", {"capture": "target"}, {"capture": "amount"}]]}
        ]}
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "update.zzpy.json").write_text(json.dumps(module))
            code = 'x = 2\nimport syntax update version "1"\nincrease x by 3 * 4\nprint(x)\n'
            self.assertEqual(execute(self.trans(code, Path(directory)).code)[0], "14\n")

    def test_template_expansion_budget(self):
        body = {"capture": "body"}
        module = {"format": 1, "name": "twice", "version": "1", "rules": [
            {"name": "duplicate", "pattern": ["duplicate", ":", {"capture": "body", "type": "suite"}],
             "expand": ["if", ["constant", True], {"suite": [
                 ["if", ["constant", True], body], ["if", ["constant", True], body]]}]}
        ]}
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "twice.zzpy.json").write_text(json.dumps(module))
            code = 'import syntax twice version "1"\n'
            code += "".join("    " * n + "duplicate:\n" for n in range(12))
            code += "    " * 12 + "pass\n"
            self.reject(code, "AST expansion budget", Path(directory))

    def test_real_zz_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory, "reject")
            executable.write_text("#!/bin/sh\nexit 7\n")
            executable.chmod(0o700)
            with self.assertRaisesRegex(TranslationError, "ZZ rejected"):
                translate("print(1)\n", executable, SYNTAX)

    def test_no_partial_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory, "program.zzpy")
            output = Path(directory, "program.py")
            output.write_text("previous valid output\n")
            source.write_text('print(1)\n' + IMPORT + 'unless:\n    pass\n')
            result = subprocess.run([sys.executable, str(ROOT / "tools/zzpy/zzpy.py"), "--zz", str(ZZ), "--syntax-dir", str(SYNTAX), "-o", str(output), str(source)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stdout, "")
            self.assertEqual(output.read_text(), "previous valid output\n")

    def test_module_is_data_not_executed_python(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "control.zzpy.json").write_text('import os; os.abort()')
            self.reject(IMPORT, "malformed module", Path(directory))


if __name__ == "__main__":
    unittest.main(verbosity=2)
