#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Small Python-to-Python experiment whose host and extension grammar runs in ZZ.

No Python parser substitutes for ZZ: this file performs restricted lexing,
module validation, IR -> Python AST lowering, and publication of generated code.
Syntax modules are JSON data, not Python or arbitrary native ZZ actions.
"""
from __future__ import annotations

import argparse
import ast
import copy
from dataclasses import dataclass
import json
import keyword
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

MAX_SOURCE = 32768
MAX_TOKENS = 2048
MAX_RULES = 32
MAX_DEPTH = 24
MAX_MODULE = 32768
MAX_OUTPUT = 2 * 1024 * 1024
MAX_AST_WORK = 16384
IDENT = re.compile(r"[A-Za-z_][A-Za-z_0-9]*\Z")
IMPORT = re.compile(r'import syntax ([A-Za-z_][A-Za-z_0-9]*) version "([A-Za-z_0-9.-]{1,32})"\s*(?:#.*)?\Z')
TOKEN = re.compile(r'''(?P<space> +)|(?P<comment>\#.*)|(?P<string>"(?:[^"\\\n]|\\[^\n])*"|'(?:[^'\\\n]|\\[^\n])*')|(?P<int>[0-9]+)|(?P<name>[A-Za-z_][A-Za-z_0-9]*)|(?P<op>//|<=|>=|==|!=|[+*/%<>=():,\-])''')
OPS = {"+": "ADD", "-": "SUB", "*": "MUL", "/": "DIV", "//": "FLOORDIV", "%": "MOD", "<": "LT", "<=": "LE", ">": "GT", ">=": "GE", "==": "EQ", "!=": "NE", "=": "ASSIGN", ":": "COLON", "(": "LP", ")": "RP", ",": "COMMA"}
BASE_WORDS = {"if", "while", "pass", "and", "or", "not", "True", "False", "None"}
BINARY = {"add": ast.Add, "sub": ast.Sub, "mul": ast.Mult, "div": ast.Div, "floordiv": ast.FloorDiv, "mod": ast.Mod}
COMPARE = {"lt": ast.Lt, "le": ast.LtE, "gt": ast.Gt, "ge": ast.GtE, "eq": ast.Eq, "ne": ast.NotEq}
SIGNATURES = {**{k: ("expr", ["expr", "expr"]) for k in BINARY | COMPARE}, "and": ("expr", ["expr", "expr"]), "or": ("expr", ["expr", "expr"]), "not": ("expr", ["expr"]), "neg": ("expr", ["expr"]), "call": ("expr", ["expr", "args"]), "if": ("stmt", ["expr", "suite"]), "while": ("stmt", ["expr", "suite"]), "assign": ("stmt", ["name", "expr"]), "expr": ("stmt", ["expr"]), "pass": ("stmt", [])}


class TranslationError(Exception):
    """Recoverable translation error; no target program has been executed."""


@dataclass(frozen=True)
class Token:
    text: str
    line: int


@dataclass(frozen=True)
class Translation:
    code: str
    zz_source: str
    zz_source_lines: dict[int, int]
    modules: tuple[str, ...]


def require(condition, message):
    if not condition:
        raise TranslationError(message)


def read_limited(path, limit):
    with Path(path).open("rb") as stream:
        data = stream.read(limit + 1)
    require(len(data) <= limit, f"{path}: size limit exceeded")
    try:
        return data.decode("utf-8")
    except UnicodeError as exc:
        raise TranslationError(f"{path}: expected UTF-8") from exc


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def fits(actual, expected):
    return actual == expected or (actual == "name" and expected == "expr")


def check_template(value, captures, expected="stmt", depth=0):
    require(depth <= MAX_DEPTH, "AST template nesting limit exceeded")
    if isinstance(value, dict):
        if set(value) == {"capture"}:
            name = value["capture"]
            require(isinstance(name, str) and name in captures, "unknown template capture")
            result = captures[name]
        elif set(value) in ({"args"}, {"suite"}):
            result = next(iter(value))
            items = value[result]
            require(isinstance(items, list) and len(items) <= 16, "invalid template sequence")
            require(result != "suite" or bool(items), "empty generated suite")
            for item in items:
                check_template(item, captures, "expr" if result == "args" else "stmt", depth + 1)
        else:
            raise TranslationError("unknown AST template object")
    else:
        require(isinstance(value, list) and value and isinstance(value[0], str), "expected typed AST template")
        kind = value[0]
        if kind == "global":
            require(len(value) == 2 and isinstance(value[1], str) and IDENT.fullmatch(value[1]) and not keyword.iskeyword(value[1]), "invalid global name")
            result = "name"
        elif kind == "constant":
            require(len(value) == 2 and (value[1] is None or type(value[1]) in (str, int, bool)), "unsupported template constant")
            result = "expr"
        else:
            require(kind in SIGNATURES, f"unknown AST constructor: {kind}")
            result, parameters = SIGNATURES[kind]
            require(len(value) == len(parameters) + 1, f"wrong arity for {kind}")
            for item, param in zip(value[1:], parameters):
                check_template(item, captures, param, depth + 1)
    require(fits(result, expected), f"template requires {expected}, got {result}")


def load_module(directory, name, version):
    path = Path(directory) / f"{name}.zzpy.json"
    try:
        module = json.loads(read_limited(path, MAX_MODULE), object_pairs_hook=unique_object)
    except (ValueError, RecursionError) as exc:
        raise TranslationError(f"{path}: malformed module JSON") from exc
    require(isinstance(module, dict) and set(module) == {"format", "name", "version", "rules"}, "invalid module fields")
    require(type(module["format"]) is int and module["format"] == 1, "unsupported module format")
    require(module["name"] == name and module["version"] == version, f"module {name}: name/version mismatch")
    rules = module["rules"]
    require(isinstance(rules, list) and 0 < len(rules) <= MAX_RULES, "invalid module rule count")
    checked = []
    names = set()
    for rule in rules:
        require(isinstance(rule, dict) and set(rule) == {"name", "pattern", "expand"}, "invalid rule fields")
        rname = rule["name"]
        require(isinstance(rname, str) and len(rname) <= 48 and IDENT.fullmatch(rname) and rname not in names, "invalid or duplicate rule name")
        names.add(rname)
        pattern = rule["pattern"]
        require(isinstance(pattern, list) and 2 <= len(pattern) <= 10, "pattern needs 2..10 elements")
        require(isinstance(pattern[0], str) and IDENT.fullmatch(pattern[0]) and len(pattern[0]) <= 48 and not keyword.iskeyword(pattern[0]), "rule must start with a new keyword")
        captures = {}
        for i, part in enumerate(pattern):
            if isinstance(part, str):
                require(len(part) <= 48 and (IDENT.fullmatch(part) or part in OPS), "invalid literal in pattern")
            else:
                require(isinstance(part, dict) and set(part) == {"capture", "type"}, "invalid pattern capture")
                cap, category = part["capture"], part["type"]
                require(isinstance(cap, str) and len(cap) <= 48 and IDENT.fullmatch(cap) and cap not in captures, "invalid/duplicate capture")
                require(category in ("expr", "name", "suite"), "unsupported capture category")
                require(category != "suite" or (i == len(pattern) - 1 and pattern[i - 1] == ":"), "suite must end the pattern after ':'")
                captures[cap] = category
        check_template(rule["expand"], captures)
        checked.append((rule, captures))
    return checked


def zz_rule(rule, captures, rule_id, literals):
    """Compile declarative syntax into an actual legacy ZZ production."""
    beads, args = [], []
    for part in rule["pattern"]:
        if isinstance(part, str):
            beads.append(json.dumps(OPS[part] if part in OPS else literals[part]))
        else:
            parameter = f"z{len(args)}"
            beads.append(f"py_{part['type']}^{parameter}")
            args.append(parameter)
    if not any(category == "suite" for category in captures.values()):
        beads.append('"NL"')
    # Only fixed trusted actions enter ZZ. The module's AST template remains data.
    fragments = [json.dumps(f'["extension","{rule_id}",[')]
    for i, arg in enumerate(args):
        if i:
            fragments.append(json.dumps(","))
        fragments.append(arg)
    fragments.append(json.dumps("]]"))
    return "/py_body -> " + " ".join(beads) + " { /return " + " & ".join(fragments) + " }"


def wrap_zz(text):
    """Keep tokens intact below the historical scanner's physical line length."""
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[^\s"]+', text)
    require(all(len(t) < 110 for t in tokens), "internal ZZ token too long")
    lines, row = [], ""
    for token in tokens:
        if len(row) + len(token) + 1 > 110:
            lines.append(row)
            row = ""
        row += (" " if row else "") + token
    if row:
        lines.append(row)
    return lines


class Frontend:
    def __init__(self, syntax_dir):
        self.syntax_dir = syntax_dir
        self.values = {}
        self.locations = {}
        self.modules = {}
        self.rules = {}
        self.verbs = set()
        # Module literals are mapped to private terminals so they cannot collide
        # with ID/INT/LOC or other internal lexer markers.
        self.literals = {word: word for word in BASE_WORDS}
        self.lines = ["/zlex_set_parse_eol 0;"]
        self.mapping = {}
        base = Path(__file__).with_name("base.zz").read_text(encoding="utf-8")
        for line in base.splitlines():
            if line.strip() and not line.lstrip().startswith("!!"):
                self.lines.extend(wrap_zz(line + ";"))
        self.pending = []
        self.count = 0
        self.ast_work = 0

    def key(self, value, line):
        key = f"k{len(self.values)}"
        self.values[key] = value
        self.locations[key] = line
        return key

    def emit(self, text, line):
        self.count += 1
        require(self.count <= MAX_TOKENS, "token budget exceeded")
        self.pending.append(Token(text, line))

    def flush(self):
        if not self.pending:
            return
        self.lines.append("__ZZPY_BEGIN")
        for token in self.pending:
            self.lines.append(token.text)
            self.mapping[len(self.lines)] = token.line
        self.lines.append("__ZZPY_END ;")
        self.mapping[len(self.lines)] = self.pending[-1].line
        self.pending.clear()

    def install(self, name, version, line):
        if name in self.modules:
            require(self.modules[name] == version, f"line {line}: conflicting module version")
            return
        checked = load_module(self.syntax_dir, name, version)
        require(len(self.rules) + len(checked) <= MAX_RULES, "total rule budget exceeded")
        pending_verbs = set()
        for rule, _ in checked:
            verb = rule["pattern"][0]
            require(verb not in self.verbs | pending_verbs | BASE_WORDS, f"line {line}: syntax keyword conflict: {verb}")
            pending_verbs.add(verb)
        self.flush()  # Definitions are installed only AFTER preceding host statements.
        for rule, captures in checked:
            for part in rule["pattern"]:
                if isinstance(part, str) and part not in OPS and part not in self.literals:
                    self.literals[part] = f"KW{len(self.literals)}"
            rule_id = f"r{len(self.rules)}"
            self.rules[rule_id] = (rule, captures)
            self.lines.extend(wrap_zz(zz_rule(rule, captures, rule_id, self.literals) + ";"))
        self.verbs.update(pending_verbs)
        self.modules[name] = version

    def scan(self, source):
        require(len(source.encode("utf-8")) <= MAX_SOURCE, "source exceeds 32 KiB")
        require("\0" not in source and "\r" not in source, "use LF newlines without NUL bytes")
        indents = [0]
        for number, line in enumerate(source.split("\n"), 1):
            require("\t" not in line, f"line {number}: tabs are not supported")
            stripped = line.lstrip(" ")
            if not stripped or stripped.startswith("#"):
                continue
            indent = len(line) - len(stripped)
            if indent > indents[-1]:
                indents.append(indent)
                require(len(indents) <= MAX_DEPTH, f"line {number}: indentation limit exceeded")
                self.emit("IN", number)
            while indent < indents[-1]:
                indents.pop()
                self.emit("OUT", number)
            require(indent == indents[-1], f"line {number}: inconsistent dedent")
            if stripped.startswith("import syntax "):
                require(indent == 0, f"line {number}: syntax imports must be top-level")
                match = IMPORT.fullmatch(stripped)
                require(match, f"line {number}: invalid syntax import")
                self.install(*match.groups(), number)
                continue
            location = self.key(None, number)
            self.emit(f'LOC "{location}"', number)
            position, parentheses = indent, 0
            while position < len(line):
                match = TOKEN.match(line, position)
                require(match, f"line {number}, column {position + 1}: unsupported token")
                kind, text = match.lastgroup, match.group()
                position = match.end()
                if kind == "comment":
                    break
                if kind == "space":
                    continue
                if kind == "name":
                    if text in self.literals:
                        self.emit(self.literals[text], number)
                    else:
                        require(not keyword.iskeyword(text), f"line {number}: unsupported Python keyword {text}")
                        self.emit(f'ID "{self.key(text, number)}"', number)
                elif kind in ("int", "string"):
                    require(len(text) <= 512, f"line {number}: literal budget exceeded")
                    try:
                        value = ast.literal_eval(text)
                    except (ValueError, SyntaxError) as exc:
                        raise TranslationError(f"line {number}: invalid literal") from exc
                    require(type(value) in (int, str), f"line {number}: unsupported literal")
                    self.emit(f'{"INT" if kind == "int" else "STR"} "{self.key(value, number)}"', number)
                else:
                    if text == "(":
                        parentheses += 1
                    elif text == ")":
                        parentheses -= 1
                    require(0 <= parentheses <= MAX_DEPTH, f"line {number}: unbalanced/deep parentheses")
                    self.emit(OPS[text], number)
            require(parentheses == 0, f"line {number}: multiline expressions are not supported")
            self.emit("NL", number)
        while len(indents) > 1:
            indents.pop()
            self.emit("OUT", len(source.split("\n")))
        self.flush()
        return "\n".join(self.lines) + "\n"

    def spend(self, amount=1):
        self.ast_work += amount
        require(self.ast_work <= MAX_AST_WORK, "AST expansion budget exceeded")

    def lower(self, value, depth=0):
        self.spend()
        require(depth <= 128, "IR nesting limit exceeded")
        kind, *args = value
        if kind == "at":
            return self.lower(args[1], depth + 1)
        if kind == "name":
            return ast.Name(id=self.values[args[0]], ctx=ast.Load())
        if kind == "literal":
            return ast.Constant(value=self.values[args[0]])
        if kind == "bool":
            return ast.Constant(value=args[0])
        if kind == "none":
            return ast.Constant(value=None)
        if kind in ("and_chain", "or_chain"):
            # Python chains are n-ary. Flattening explicit parentheses or nesting
            # a flat chain can change calls to an operand's stateful __bool__.
            return ast.BoolOp(op=ast.And() if kind == "and_chain" else ast.Or(),
                              values=[self.lower(a, depth + 1) for a in args[0]])
        if kind == "extension":
            rule, captures = self.rules[args[0]]
            values = {}
            for (name, category), captured in zip(captures.items(), args[1]):
                values[name] = self.suite(captured, depth + 1) if category == "suite" else self.lower(captured, depth + 1)
            require(len(values) == len(captures) == len(args[1]), "invalid extension capture count")
            return self.expand(rule["expand"], values, depth + 1)
        if kind in ("if", "while"):
            return self.construct(kind, [self.lower(args[0], depth + 1), self.suite(args[1], depth + 1)])
        if kind == "call":
            return self.construct(kind, [self.lower(args[0], depth + 1), [self.lower(a, depth + 1) for a in args[1]]])
        return self.construct(kind, [self.lower(a, depth + 1) for a in args])

    def suite(self, values, depth):
        return [self.lower(value, depth + 1) for value in values]

    def expand(self, value, captures, depth):
        self.spend()
        require(depth <= 128, "expanded AST depth limit exceeded")
        if isinstance(value, dict):
            if "capture" in value:
                captured = captures[value["capture"]]
                roots = captured if isinstance(captured, list) else [captured]
                for root in roots:
                    for _ in ast.walk(root):
                        self.spend()
                return copy.deepcopy(captured)
            return [self.expand(v, captures, depth + 1) for v in next(iter(value.values()))]
        kind, *args = value
        if kind == "global":
            return ast.Name(id=args[0], ctx=ast.Load())
        if kind == "constant":
            return ast.Constant(value=args[0])
        return self.construct(kind, [self.expand(a, captures, depth + 1) for a in args])

    @staticmethod
    def construct(kind, args):
        if kind in BINARY:
            return ast.BinOp(left=args[0], op=BINARY[kind](), right=args[1])
        if kind in COMPARE:
            return ast.Compare(left=args[0], ops=[COMPARE[kind]()], comparators=[args[1]])
        if kind in ("not", "neg"):
            return ast.UnaryOp(op=ast.Not() if kind == "not" else ast.USub(), operand=args[0])
        if kind in ("and", "or"):
            return ast.BoolOp(op=ast.And() if kind == "and" else ast.Or(), values=args)
        if kind == "assign":
            target = copy.deepcopy(args[0])
            require(isinstance(target, ast.Name), "assignment target must be a name")
            target.ctx = ast.Store()
            return ast.Assign(targets=[target], value=args[1])
        if kind == "call":
            return ast.Call(func=args[0], args=args[1], keywords=[])
        if kind in ("if", "while"):
            return (ast.If if kind == "if" else ast.While)(test=args[0], body=args[1], orelse=[])
        if kind == "expr":
            return ast.Expr(value=args[0])
        if kind == "pass":
            return ast.Pass()
        raise TranslationError(f"unsupported IR node: {kind}")


def translate(source, zz, syntax_dir, filename="<zzpy>", timeout=10):
    frontend = Frontend(syntax_dir)
    script = frontend.scan(source)
    # Process isolation is not a security sandbox. Never execute target Python or
    # user-supplied raw ZZ actions during translation. Ignore legacy auto-includes.
    env = os.environ.copy()
    env.pop("ZZ_DEFAULT_INCLUDE_FILE", None)
    env.pop("ZZ_INCLUDES", None)
    with tempfile.TemporaryDirectory(prefix="zzpy-") as work:
        path = Path(work) / "input.zz"
        path.write_text(script, encoding="ascii")
        with (Path(work) / "stdout").open("w+b") as out, (Path(work) / "stderr").open("w+b") as err:
            try:
                run = subprocess.run([str(Path(zz).resolve()), str(path)], cwd=work, env=env, stdout=out, stderr=err, timeout=timeout, check=False)
            except subprocess.TimeoutExpired as exc:
                raise TranslationError("ZZ parser time budget exceeded") from exc
            out.seek(0); err.seek(0)
            output, errors = out.read(MAX_OUTPUT + 1), err.read(MAX_OUTPUT + 1)
        require(len(output) <= MAX_OUTPUT and len(errors) <= MAX_OUTPUT, "ZZ output budget exceeded")
        if run.returncode:
            diagnostic = errors.decode("utf-8", errors="replace")
            match = re.search(r"line (\d+) of", diagnostic)
            line = frontend.mapping.get(int(match.group(1))) if match else None
            where = f"{filename}:{line}" if line else filename
            raise TranslationError(f"{where}: ZZ rejected the program\n{diagnostic[:3000]}")
    statements = []
    try:
        for line in output.decode("ascii").splitlines():
            if line.strip():
                statements.extend(json.loads(line))
        body = frontend.suite(statements, 0)
        tree = ast.Module(body=body, type_ignores=[])
        # Parse diagnostics map back to the input. Full generated-Python source
        # maps are future work; output is independently reparsed below.
        tree = ast.fix_missing_locations(tree)
        compile(tree, filename, "exec")  # Validates; does not execute.
        code = ast.unparse(tree) + "\n"
        require(len(code.encode("utf-8")) <= MAX_OUTPUT, "generated Python size limit exceeded")
        compile(code, filename, "exec")
    except (ValueError, TypeError, SyntaxError, KeyError, RecursionError, UnicodeError) as exc:
        raise TranslationError(f"invalid generated IR/AST: {exc}") from exc
    return Translation(code, script, dict(frontend.mapping), tuple(f"{n}@{v}" for n, v in frontend.modules.items()))


def publish(path, code):
    path = Path(path)
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent, prefix=f".{path.name}.", delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(code)
            stream.close()
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--zz", required=True, type=Path, help="built ozz executable")
    parser.add_argument("--syntax-dir", type=Path, help="defaults to source-directory/syntax")
    parser.add_argument("-o", "--output", type=Path, help="atomically write standard Python instead of stdout")
    args = parser.parse_args(argv)
    try:
        result = translate(read_limited(args.source, MAX_SOURCE), args.zz, args.syntax_dir or args.source.parent / "syntax", str(args.source))
        if args.output:
            require(args.output.resolve() != args.source.resolve(), "output must differ from input")
            publish(args.output, result.code)
        else:
            sys.stdout.write(result.code)
    except (TranslationError, OSError, RecursionError) as exc:
        print(f"zzpy: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
