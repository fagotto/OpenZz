# Checked dynamic grammar profile (experimental)

This is an **opt-in, deliberately restricted profile**, implemented in C inside
libozz and selected by `ozz --checked`. Existing ZZ scripts, callbacks and the
legacy parser continue to use their existing entry points. New directives are
NOT recognized by the legacy interpreter. They cannot be mixed with legacy
`/stat` rules. This is the first implementation of the numerical DSL recommended
in `GRAMMATICHE_DINAMICHE_ESTENSIONI_ZZ.md`, not a general replacement parser.

## Try it

From an out-of-tree build directory:

```sh
src/ozz --checked ../OpenZz/examples/checked/increment.zz
src/ozz --checked --emit-c ../OpenZz/examples/checked/increment.zz > increment.c
cc -std=c11 -Wall -Wextra -Werror increment.c -o increment
./increment
```

Adjust the source path for your checkout. Both executions print `19` then `15`.
The generated program is standalone C11, without a dependency on libozz.
`examples/checked/modules.zz` demonstrates an exported rule and versioned import.

## Architecture and compatibility boundary

- `zz_context.c`: owned states, deep snapshots, bounded allocations, diagnostics,
  portable checked arithmetic. No process exit on library errors.
- `zz_checked_parse.c`: ASCII scanner, deterministic statement grammar, Pratt
  expression parser, transactions, symbol identities, syntax modules, pure IR.
- `zz_checked_eval.c`: interpreter and standalone C emitter. Both check i64
  addition, subtraction and multiplication, including `INT64_MIN * -1`.
- `zz_checked_cli.c`: explicit profile selection and file I/O.

The new profile never calls `zz_init`, arbitrary C actions, dynamic libraries,
legacy grammar hooks or the old parser. Independent contexts share no mutable
state. Concurrent access to the **same** context still requires caller locking.
The historical engine remains global and has its documented residual risks.

AST nodes refer to stable symbol indices, not names. Generated variables and
intermediate values use indices, preventing source names from capturing backend
identifiers. This is hygiene for this fixed IR; arbitrary macro templates and
user-defined binders are not implemented.

## Language contract

Statements are separated by newline or `;`. Comments start with `#` or `!!`.
Identifiers use ASCII letters/underscore followed by letters, digits/underscore.
Quoted directive strings are plain ASCII, without escapes or embedded newlines.

```text
/symbol total : i64 mutable
/slot int_ref : Symbol<i64> from visible_symbols

total = 2 + 3 * 4
print(total)
```

Symbols start at zero when the accepted program is explicitly run. Omitting
`mutable` creates a read-only zero binding. This is not definite-assignment
analysis. Each run evaluates the whole accumulated AST from zero; it is not a
stateful REPL execution step. `/scope push` and `/scope pop` change name visibility;
shadowing has a new identity, while previously built AST retains its bindings.

Expressions support i64 decimal literals, unary minus, visible symbol loads,
parentheses and `+`, `-`, `*`. Default precedences are 40 for addition/subtraction
and 50 for multiplication, left associative. Literal and runtime overflow fail.

### Atomic patches

```text
/patch begin "increment" base "g0"
/symbol total : i64 mutable
/slot int_ref : Symbol<i64> from visible_symbols
/rule increment : Stmt -> "bump" int_ref^x "by" expr<i64>^n
  => Assign(x, CheckedAdd(Load(x), n))
/test accepts "bump total by 3"
/test rejects "bump missing by 3"
/patch check
/patch commit
```

- `begin` clones the committed state. `base` is optional; when supplied it must
  match the current committed revision. Nested patches are rejected.
- Candidate changes are private. Public probes and symbol queries see only the
  committed state. Ordinary program statements are forbidden in an open patch.
- `check` rejects known profile conflicts and reruns **all tests registered in
  that patch against its current grammar**. No general CFG ambiguity claim.
- Any further candidate mutation requires another `check` before `commit`.
- `commit` swaps the candidate into place and increments the revision; `abort`
  discards it. Candidate tests are patch-local and are discarded on commit.
- A failed `zz_context_apply` rolls back the entire call, including all statements,
  completed commits and candidate changes within that call. A candidate created
  by a *previous successful call* survives a later failed call, unchanged.
- Outside patches, declaration directives are permitted and advance the revision.
  Execution/AST statements do not change the grammar revision.
- An unclosed patch or module cannot be run or emitted. Nothing executes while
  grammar tests, prefix probes or declarations are being parsed.

### Supported dynamic rule shape

The first profile accepts this deterministic family:

```text
/rule NAME : Stmt -> "VERB" SLOT^target "SEPARATOR" expr<i64>^amount
  => Assign(target, CheckedAdd(Load(target), amount))
```

`CheckedSub` and `CheckedMul` are also supported. VERB and SEPARATOR must be ASCII
identifiers; the two bindings must be distinct. SLOT must be a declared
`Symbol<i64>` slot. The target must resolve to a visible mutable symbol. Different
active rules cannot share a verb; verbs cannot collide with visible symbols or
`print`. Rule names cannot be duplicated within a module. This profile has no
arbitrary RHS productions, recursive user nonterminals or arbitrary AST actions.
Those are rejected, not silently approximated.

### Operators and syntax modules

```text
/operator "+" infix precedence 60 associativity left
/operator "-" infix precedence 40 associativity right

/module finance version "1"
/rule credit : Stmt -> "credit" int_ref^x "with" expr<i64>^n => Assign(x, CheckedAdd(Load(x), n))
/export syntax credit
/end module
/import syntax finance version "1"
```

Operator declarations configure only existing checked `+`, `-`, `*` operations;
precedence is 0..100. They affect subsequent parsing, not already built AST.
Modules are in-memory syntax containers. Only exported rules activate on import,
which checks exact version and conflicts. One version per module name can be
loaded into a context; there is no filesystem/package resolution. Slots and
symbols are context/scope declarations, not private module exports.

### Explicit policy and budgets

```text
/numeric i64 overflow error
/lexmode host identifiers ascii
/layout lines
/budget nodes 4096
```

These are the implemented profiles. Unicode lexing, Python strings and indentation
are rejected. `nodes` bounds each state collection (AST nodes, symbols, slots,
rules, statements, modules, patch tests), default 4096, configurable 16..100000.
It is not a bound on total bytes or wall-clock time. Further hard limits: 1 MiB
input per apply/probe, 255-byte token, 63-byte name, nesting/scope depth 128.
Limits return `ZZ_LIMIT`, never silently truncate or switch to unrestricted input.
Existing collections cannot be shrunk below their current size by a budget change.

## Embedding, prefixes and checkpoints

Include installed `<ozz/zz_checked.h>` and link libozz. API status is `ZZ_OK` for
success, with distinct invalid, incomplete, resource, allocation and conflict
statuses. Diagnostics have source line/column where available; evaluation errors
currently identify AST node indices, not source spans.

```c
zz_context *ctx = zz_context_new();
/* Check allocation and every status in real code. */
zz_context_apply(ctx, declarations);
zz_context *checkpoint = zz_context_clone(ctx);
zz_diagnostic diagnostic;
zz_status status = zz_context_probe(ctx, "bump to", &diagnostic);
/* Restore by freeing ctx and replacing it with checkpoint when needed. */
zz_context_free(checkpoint);
zz_context_free(ctx);
```

`zz_context_probe` recognizes **one statement** against committed grammar:
`ZZ_OK` = complete, `ZZ_INCOMPLETE` = potentially extendible, `ZZ_INVALID` =
rejected. A complete statement can still be extended (e.g. an expression).
Other statuses must be handled as failures/unknown, not accepted continuations.
Prefix input is replayed on a deep copy; this is a correctness baseline, not an
optimized incremental LR continuation or a ready-made LLM token mask. The caller
accumulates bytes and supplies the full current prefix on each probe. Checkpoint
and restore are API ownership operations, not textual directives in this version.

`zz_context_symbol_count`, `zz_context_symbol`, `zz_context_lookup_symbol` expose
committed visible bindings, stable IDs, scope, mutability and grammar revision.
They support symbol completion/explanation; they are not a complete expected-token
set. Borrowed names expire on successful apply/free. Save owned copies if needed.

`zz_context_run(ctx, output)` evaluates first, buffering numerical output until
all arithmetic succeeds. `zz_context_emit_c` emits a validated AST explicitly.
The generated executable also postpones all prints until arithmetic succeeds.
I/O failures can still leave partial external output; no file transaction or
rollback of arbitrary streams is promised.

## Validation and next boundaries

`make check` includes legacy regressions plus checked API, CLI/C equivalence,
negative cases, deterministic input mutations and arithmetic differential tests.
`sh testsuite/checked-sanitizers.sh` compiles only the new profile with strict C11,
ASan and UBSan. On supported Linux hosts set `ASAN_OPTIONS=detect_leaks=1`.

Still outside this implementation: conversion of the legacy global engine into
contexts, general typed CFG rules, custom type systems, effectful action plugins,
arbitrary hygienic macros, Unicode/indentation modes, generalized ambiguous
parsing, repository symbol extraction, optimized incremental state sharing and
LLM-tokenizer adapters. No guarantee of algorithmic correctness follows from
acceptance by the checked grammar. These boundaries preserve a small testable
first prototype rather than claiming all research proposals are already solved.
