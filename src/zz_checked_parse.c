/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "zz_checked_internal.h"
#include <errno.h>
#include <inttypes.h>
/* A deliberately restricted, deterministic grammar profile. No legacy globals
 * or callbacks are reachable here. All semantic actions only construct IR. */
enum { T_END, T_WORD, T_NUMBER, T_STRING, T_PUNCT, T_EOL };
typedef struct {
    zz_context *c;
    zc_state *s;
    const char *input, *cur;
    int kind, prefix;
    char text[ZC_TOKEN];
    size_t line, col, tline, tcol;
    unsigned depth;
} parser;
static int alpha(unsigned char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}
static int digit(unsigned char ch) { return ch >= '0' && ch <= '9'; }
static int fail(parser *p, zz_status code, const char *why) {
    if (p->c->error.code == ZZ_OK)
        zc_error(p->c, code, p->tline, p->tcol, "%s", why);
    return 0;
}
static void next(parser *p) {
    size_t n = 0;
    const char *start;
    if (p->c->error.code != ZZ_OK)
        return;
    while (*p->cur == ' ' || *p->cur == '\t' || *p->cur == '\r') {
        p->cur++;
        p->col++;
    }
    if (*p->cur == '#' || (p->cur[0] == '!' && p->cur[1] == '!'))
        while (*p->cur && *p->cur != '\n') {
            p->cur++;
            p->col++;
        }
    p->tline = p->line;
    p->tcol = p->col;
    start = p->cur;
    p->text[0] = 0;
    if (!*start) {
        p->kind = T_END;
        return;
    }
    if (*start == '\n' || *start == ';') {
        p->kind = T_EOL;
        p->cur++;
        p->col++;
        if (*start == '\n') {
            p->line++;
            p->col = 1;
        }
        return;
    }
    if (alpha((unsigned char)*start)) {
        p->kind = T_WORD;
        while (alpha((unsigned char)*p->cur) || digit((unsigned char)*p->cur))
            p->cur++;
    } else if (digit((unsigned char)*start)) {
        p->kind = T_NUMBER;
        while (digit((unsigned char)*p->cur))
            p->cur++;
    } else if (*start == '"') {
        if (p->prefix) {
            fail(p, ZZ_INVALID, "strings are not allowed in checked statements");
            return;
        }
        p->kind = T_STRING;
        start = ++p->cur;
        p->col++;
        while (*p->cur && *p->cur != '"') {
            if (*p->cur == '\\' || *p->cur == '\n' || (unsigned char)*p->cur >= 128) {
                fail(p, ZZ_INVALID, "quoted strings use plain ASCII without escapes");
                return;
            }
            p->cur++;
        }
        if (!*p->cur) {
            fail(p, p->prefix ? ZZ_INCOMPLETE : ZZ_INVALID, "unterminated string");
            return;
        }
    } else {
        if ((unsigned char)*start >= 128) {
            fail(p, ZZ_INVALID, "checked profile requires ASCII input");
            return;
        }
        p->kind = T_PUNCT;
        p->cur++;
        if ((start[0] == '-' && start[1] == '>') || (start[0] == '=' && start[1] == '>'))
            p->cur++;
    }
    n = (size_t)(p->cur - start);
    if (n >= sizeof(p->text)) {
        fail(p, ZZ_LIMIT, "token exceeds 255 bytes");
        return;
    }
    memcpy(p->text, start, n);
    p->text[n] = 0;
    p->col += n;
    if (p->kind == T_STRING) {
        p->cur++;
        p->col++;
    }
}
static int is(parser *p, const char *text) {
    return (p->kind == T_WORD || p->kind == T_PUNCT) && !strcmp(p->text, text);
}
static int take(parser *p, const char *text) {
    if (!is(p, text))
        return 0;
    next(p);
    return 1;
}
static int want(parser *p, const char *text) {
    char msg[256];
    if (take(p, text))
        return p->c->error.code == ZZ_OK;
    if (p->prefix && (p->kind == T_END || (!*p->cur && !strncmp(text, p->text, strlen(p->text)))))
        return fail(p, ZZ_INCOMPLETE, "incomplete statement");
    snprintf(msg, sizeof(msg), "expected '%s', found '%.120s'", text,
             p->kind == T_END ? "end of input" : p->text);
    return fail(p, ZZ_INVALID, msg);
}
static int name(parser *p, char out[ZC_NAME]) {
    if (p->kind != T_WORD && p->kind != T_STRING)
        return fail(p, ZZ_INVALID, "expected name");
    if (strlen(p->text) >= ZC_NAME)
        return fail(p, ZZ_LIMIT, "name exceeds 63 bytes");
    strcpy(out, p->text);
    next(p);
    return p->c->error.code == ZZ_OK;
}
static int identifier(parser *p, char out[ZC_NAME]) {
    if (p->kind != T_WORD)
        return fail(p, ZZ_INVALID, "expected ASCII identifier");
    return name(p, out);
}
static size_t symbol(parser *p, const char *text) {
    size_t i = p->s->nsymbols;
    while (i--)
        if (p->s->symbols[i].visible && !strcmp(p->s->symbols[i].name, text))
            return i;
    return SIZE_MAX;
}
static int partial_symbol(parser *p, int mutable_only) {
    size_t i;
    if (!p->prefix || *p->cur || (p->kind != T_WORD && p->kind != T_END))
        return 0;
    for (i = 0; i < p->s->nsymbols; i++)
        if (p->s->symbols[i].visible && (!mutable_only || p->s->symbols[i].mutable) &&
            symbol(p, p->s->symbols[i].name) == i &&
            !strncmp(p->s->symbols[i].name, p->text, strlen(p->text)))
            return 1;
    return 0;
}
#define PUSH(p, field, count, value)                                                               \
    do {                                                                                           \
        void *tmp = (p)->s->field;                                                                 \
        if (!zc_append((p)->c, &tmp, &(p)->s->count, sizeof(*(p)->s->field), &(value)))            \
            return 0;                                                                              \
        (p)->s->field = tmp;                                                                       \
    } while (0)
static int node(parser *p, zc_kind kind, size_t a, size_t b, int64_t v, size_t *result) {
    zc_node item = {kind, a, b, v};
    *result = p->s->nnodes;
    PUSH(p, nodes, nnodes, item);
    return 1;
}
static int expression(parser *p, unsigned minimum, size_t *result);
static int atom(parser *p, size_t *result) {
    size_t id;
    uint64_t value = 0, limit;
    int negative = take(p, "-");
    if (p->kind == T_NUMBER) {
        const char *q = p->text;
        limit = negative ? (uint64_t)INT64_MAX + 1 : INT64_MAX;
        while (*q) {
            unsigned d = (unsigned)(*q++ - '0');
            if (value > (limit - d) / 10)
                return fail(p, ZZ_INVALID, "i64 literal overflow");
            value = value * 10 + d;
        }
        next(p);
        return node(p, ZC_CONST, 0, 0,
                    negative ? (value == (uint64_t)INT64_MAX + 1 ? INT64_MIN : -(int64_t)value)
                             : (int64_t)value,
                    result);
    }
    if (negative) {
        size_t operand, zero;
        if (++p->depth > ZC_MAX_DEPTH)
            return fail(p, ZZ_LIMIT, "unary nesting limit exceeded");
        if (!atom(p, &operand) || !node(p, ZC_CONST, 0, 0, 0, &zero))
            return 0;
        p->depth--;
        return node(p, ZC_SUB, zero, operand, 0, result);
    }
    if (take(p, "(")) {
        if (!expression(p, 0, result))
            return 0;
        return want(p, ")");
    }
    if (p->kind == T_WORD) {
        id = symbol(p, p->text);
        if (id == SIZE_MAX)
            return fail(p, partial_symbol(p, 0) ? ZZ_INCOMPLETE : ZZ_INVALID, "unknown i64 symbol");
        next(p);
        return node(p, ZC_LOAD, id, 0, 0, result);
    }
    return fail(p, p->prefix && p->kind == T_END ? ZZ_INCOMPLETE : ZZ_INVALID,
                "expected i64 expression");
}
static int expression(parser *p, unsigned minimum, size_t *result) {
    size_t left, right;
    unsigned i, prec;
    zc_kind kind;
    if (++p->depth > ZC_MAX_DEPTH)
        return fail(p, ZZ_LIMIT, "expression nesting limit exceeded");
    if (!atom(p, &left))
        return 0;
    while (p->kind == T_PUNCT && strlen(p->text) == 1) {
        for (i = 0; i < 3; i++)
            if (p->s->ops[i].token == p->text[0])
                break;
        if (i == 3 || (prec = p->s->ops[i].precedence) < minimum)
            break;
        kind = i == 0 ? ZC_ADD : i == 1 ? ZC_SUB : ZC_MUL;
        next(p);
        if (!expression(p, prec + (p->s->ops[i].right ? 0 : 1), &right) ||
            !node(p, kind, left, right, 0, &left))
            return 0;
    }
    p->depth--;
    *result = left;
    return 1;
}
static int rule_active(parser *p, const zc_rule *r) {
    size_t i;
    if (!r->module[0])
        return 1;
    for (i = 0; i < p->s->nmodules; i++)
        if (!strcmp(p->s->modules[i].name, r->module))
            return r->exported && p->s->modules[i].imported;
    return 0;
}
static int statement(parser *p) {
    size_t target, expr, stmt, i, loaded;
    zc_rule rule;
    if (take(p, "print")) {
        if (!want(p, "(") || !expression(p, 0, &expr) || !want(p, ")") ||
            !node(p, ZC_PRINT, expr, 0, 0, &stmt))
            return 0;
    } else {
        for (i = 0; i < p->s->nrules; i++)
            if (rule_active(p, &p->s->rules[i]) && is(p, p->s->rules[i].verb))
                break;
        if (i < p->s->nrules) {
            rule = p->s->rules[i];
            next(p);
            target = p->kind == T_WORD ? symbol(p, p->text) : SIZE_MAX;
            if (target == SIZE_MAX || !p->s->symbols[target].mutable)
                return fail(p, partial_symbol(p, 1) ? ZZ_INCOMPLETE : ZZ_INVALID,
                            "unknown target symbol");
            next(p);
            if (!want(p, rule.separator) || !expression(p, 0, &expr) ||
                !node(p, ZC_LOAD, target, 0, 0, &loaded) ||
                !node(p, (zc_kind)rule.action, loaded, expr, 0, &expr))
                return 0;
        } else {
            target = p->kind == T_WORD ? symbol(p, p->text) : SIZE_MAX;
            if (target == SIZE_MAX || !p->s->symbols[target].mutable) {
                int partial = partial_symbol(p, 1);
                if (p->prefix && !*p->cur && (p->kind == T_WORD || p->kind == T_END)) {
                    if (!strncmp("print", p->text, strlen(p->text)))
                        partial = 1;
                    for (i = 0; i < p->s->nrules; i++)
                        if (rule_active(p, &p->s->rules[i]) &&
                            !strncmp(p->s->rules[i].verb, p->text, strlen(p->text)))
                            partial = 1;
                }
                return fail(p,
                            partial || (p->prefix && p->kind == T_END) ? ZZ_INCOMPLETE : ZZ_INVALID,
                            "unknown statement or symbol");
            }
            next(p);
            if (!want(p, "=") || !expression(p, 0, &expr))
                return 0;
        }
        if (!p->s->symbols[target].mutable)
            return fail(p, ZZ_INVALID, "assignment to immutable symbol");
        if (!node(p, ZC_ASSIGN, target, expr, 0, &stmt))
            return 0;
    }
    if (p->kind != T_EOL && p->kind != T_END)
        return fail(p, ZZ_INVALID, "unexpected token after statement");
    PUSH(p, statements, nstatements, stmt);
    return 1;
}
static int validate(parser *p) {
    size_t i, j;
    for (i = 0; i < p->s->nrules; i++)
        if (rule_active(p, &p->s->rules[i])) {
            const char *verb = p->s->rules[i].verb;
            if (!strcmp(verb, "print") || symbol(p, verb) != SIZE_MAX)
                return fail(p, ZZ_CONFLICT, "rule verb conflicts with statement or symbol");
            for (j = 0; j < i; j++)
                if (rule_active(p, &p->s->rules[j]) && !strcmp(verb, p->s->rules[j].verb))
                    return fail(p, ZZ_CONFLICT, "two active rules have the same verb");
        }
    return 1;
}
static int grammar_test(parser *p, const char *source, int should_accept) {
    zz_context *test = zz_context_clone(p->c);
    parser q;
    int accepted;
    if (!test)
        return fail(p, ZZ_NOMEM, "allocation failed");
    if (test->draft) {
        zc_state_free(test->live);
        test->live = test->draft;
        test->draft = NULL;
    }
    memset(&test->error, 0, sizeof(test->error));
    memset(&q, 0, sizeof(q));
    q.c = test;
    q.s = test->live;
    q.input = q.cur = source;
    q.line = q.col = 1;
    next(&q);
    accepted = statement(&q);
    if (accepted && q.kind != T_END)
        accepted = 0;
    if (test->error.code == ZZ_NOMEM || test->error.code == ZZ_LIMIT) {
        zz_status status = test->error.code;
        zz_context_free(test);
        return fail(p, status, "test exhausted resources");
    }
    zz_context_free(test);
    return accepted == should_accept ? 1 : fail(p, ZZ_INVALID, "grammar test failed");
}
static int directive(parser *p) {
    zz_context *c = p->c;
    char a[ZC_NAME], b[ZC_NAME];
    size_t i;
    uint64_t base;
    if (take(p, "patch")) {
        if (take(p, "begin")) {
            if (c->draft)
                return fail(p, ZZ_CONFLICT, "nested patch is not supported");
            if (!name(p, a))
                return 0;
            if (take(p, "base")) {
                char *end;
                if (p->kind != T_STRING || p->text[0] != 'g' || !digit((unsigned char)p->text[1]))
                    return fail(p, ZZ_INVALID, "base must be a quoted revision, e.g. g0");
                errno = 0;
                base = strtoull(p->text + 1, &end, 10);
                if (errno || *end || base != c->live->revision)
                    return fail(p, ZZ_CONFLICT, "stale grammar revision");
                next(p);
            }
            c->draft = zc_state_clone(c->live);
            if (!c->draft)
                return fail(p, ZZ_NOMEM, "allocation failed");
            strcpy(c->patch, a);
            c->checked = 0;
            p->s = c->draft;
            return 1;
        }
        if (!c->draft)
            return fail(p, ZZ_CONFLICT, "no open patch");
        if (take(p, "check")) {
            if (!validate(p))
                return 0;
            for (i = 0; i < p->s->ntests; i++)
                if (!grammar_test(p, p->s->tests[i].source, p->s->tests[i].accepts))
                    return 0;
            c->checked = 1;
            return 1;
        }
        if (take(p, "abort")) {
            zc_state_free(c->draft);
            c->draft = NULL;
            p->s = c->live;
            c->checked = 0;
            c->patch[0] = 0;
            return 1;
        }
        if (take(p, "commit")) {
            if (!c->checked)
                return fail(p, ZZ_CONFLICT, "patch requires check after its last change");
            if (p->s->module[0])
                return fail(p, ZZ_CONFLICT, "close module before committing");
            if (c->live->revision == UINT64_MAX)
                return fail(p, ZZ_LIMIT, "revision overflow");
            free(c->draft->tests);
            c->draft->tests = NULL;
            c->draft->ntests = 0;
            c->draft->revision = c->live->revision + 1;
            zc_state_free(c->live);
            c->live = c->draft;
            c->draft = NULL;
            p->s = c->live;
            c->checked = 0;
            c->patch[0] = 0;
            return 1;
        }
        return fail(p, ZZ_INVALID, "unknown patch operation");
    }
    if (take(p, "test")) {
        zc_test test;
        memset(&test, 0, sizeof(test));
        if (take(p, "accepts"))
            test.accepts = 1;
        else if (take(p, "rejects"))
            test.accepts = 0;
        else
            return fail(p, ZZ_INVALID, "expected accepts or rejects");
        if (p->kind != T_STRING)
            return fail(p, ZZ_INVALID, "test requires quoted statement");
        strcpy(test.source, p->text);
        if (!grammar_test(p, test.source, test.accepts))
            return 0;
        if (c->draft) {
            PUSH(p, tests, ntests, test);
            c->checked = 0;
        }
        next(p);
        return 1;
    }
    c->checked = 0;
    if (take(p, "symbol")) {
        zc_symbol sym;
        size_t existing;
        memset(&sym, 0, sizeof(sym));
        if (!identifier(p, sym.name) || !want(p, ":") || !want(p, "i64"))
            return 0;
        sym.mutable = take(p, "mutable");
        sym.visible = 1;
        sym.scope = p->s->scope;
        existing = symbol(p, sym.name);
        if (existing != SIZE_MAX && p->s->symbols[existing].scope == sym.scope)
            return fail(p, ZZ_CONFLICT, "duplicate symbol in scope");
        if (!strcmp(sym.name, "print"))
            return fail(p, ZZ_CONFLICT, "reserved symbol name");
        PUSH(p, symbols, nsymbols, sym);
        return validate(p);
    }
    if (take(p, "scope")) {
        if (take(p, "push")) {
            if (p->s->scope >= ZC_MAX_DEPTH)
                return fail(p, ZZ_LIMIT, "scope depth exhausted");
            p->s->scope++;
            return 1;
        }
        if (take(p, "pop")) {
            if (!p->s->scope)
                return fail(p, ZZ_CONFLICT, "cannot pop root scope");
            for (i = 0; i < p->s->nsymbols; i++)
                if (p->s->symbols[i].scope == p->s->scope)
                    p->s->symbols[i].visible = 0;
            p->s->scope--;
            return 1;
        }
        return fail(p, ZZ_INVALID, "expected push or pop");
    }
    if (take(p, "slot")) {
        zc_slot slot;
        if (!identifier(p, slot.name) || !want(p, ":") || !want(p, "Symbol") || !want(p, "<") ||
            !want(p, "i64") || !want(p, ">") || !want(p, "from") || !want(p, "visible_symbols"))
            return 0;
        for (i = 0; i < p->s->nslots; i++)
            if (!strcmp(slot.name, p->s->slots[i].name))
                return fail(p, ZZ_CONFLICT, "duplicate slot");
        PUSH(p, slots, nslots, slot);
        return 1;
    }
    if (take(p, "rule")) {
        zc_rule rule;
        char x[ZC_NAME], n[ZC_NAME];
        memset(&rule, 0, sizeof(rule));
        strcpy(rule.module, p->s->module);
        if (!identifier(p, rule.name) || !want(p, ":") || !want(p, "Stmt") || !want(p, "->"))
            return 0;
        if (p->kind != T_STRING || !name(p, rule.verb))
            return fail(p, ZZ_INVALID, "rule requires quoted verb");
        for (i = 0; rule.verb[i]; i++)
            if (!alpha((unsigned char)rule.verb[i]) && !(i && digit((unsigned char)rule.verb[i])))
                return fail(p, ZZ_INVALID, "verb must be an identifier");
        if (!rule.verb[0])
            return fail(p, ZZ_INVALID, "empty verb");
        if (!identifier(p, rule.slot))
            return 0;
        for (i = 0; i < p->s->nslots; i++)
            if (!strcmp(rule.slot, p->s->slots[i].name))
                break;
        if (i == p->s->nslots)
            return fail(p, ZZ_INVALID, "unknown typed slot");
        if (!want(p, "^") || !identifier(p, x))
            return 0;
        if (p->kind != T_STRING || !name(p, rule.separator))
            return fail(p, ZZ_INVALID, "rule requires quoted separator");
        for (i = 0; rule.separator[i]; i++)
            if (!alpha((unsigned char)rule.separator[i]) &&
                !(i && digit((unsigned char)rule.separator[i])))
                return fail(p, ZZ_INVALID, "separator must be an identifier");
        if (!rule.separator[0])
            return fail(p, ZZ_INVALID, "empty separator");
        if (!want(p, "expr") || !want(p, "<") || !want(p, "i64") || !want(p, ">") ||
            !want(p, "^") || !identifier(p, n))
            return 0;
        if (!strcmp(x, n))
            return fail(p, ZZ_CONFLICT, "duplicate binding");
        while (p->kind == T_EOL)
            next(p);
        if (!want(p, "=>") || !want(p, "Assign") || !want(p, "(") || !want(p, x) || !want(p, ","))
            return 0;
        if (take(p, "CheckedAdd"))
            rule.action = ZC_ADD;
        else if (take(p, "CheckedSub"))
            rule.action = ZC_SUB;
        else if (take(p, "CheckedMul"))
            rule.action = ZC_MUL;
        else
            return fail(p, ZZ_INVALID, "only checked i64 update constructors are supported");
        if (!want(p, "(") || !want(p, "Load") || !want(p, "(") || !want(p, x) || !want(p, ")") ||
            !want(p, ",") || !want(p, n) || !want(p, ")") || !want(p, ")"))
            return 0;
        for (i = 0; i < p->s->nrules; i++)
            if (!strcmp(rule.name, p->s->rules[i].name) &&
                !strcmp(rule.module, p->s->rules[i].module))
                return fail(p, ZZ_CONFLICT, "duplicate rule name");
        PUSH(p, rules, nrules, rule);
        return 1;
    }
    if (take(p, "operator")) {
        unsigned prec;
        int right;
        if (p->kind != T_STRING || strlen(p->text) != 1)
            return fail(p, ZZ_INVALID, "expected quoted +, - or *");
        for (i = 0; i < 3; i++)
            if (p->s->ops[i].token == p->text[0])
                break;
        if (i == 3)
            return fail(p, ZZ_INVALID, "operator has no checked i64 implementation");
        next(p);
        if (!want(p, "infix") || !want(p, "precedence"))
            return 0;
        if (p->kind != T_NUMBER || strlen(p->text) > 3 ||
            (prec = (unsigned)strtoul(p->text, NULL, 10)) > 100)
            return fail(p, ZZ_INVALID, "precedence must be 0..100");
        next(p);
        if (!want(p, "associativity"))
            return 0;
        if (take(p, "left"))
            right = 0;
        else if (take(p, "right"))
            right = 1;
        else
            return fail(p, ZZ_INVALID, "expected left or right");
        p->s->ops[i].precedence = prec;
        p->s->ops[i].right = right;
        return 1;
    }
    if (take(p, "module")) {
        zc_module m;
        memset(&m, 0, sizeof(m));
        if (p->s->module[0])
            return fail(p, ZZ_CONFLICT, "nested module");
        if (!identifier(p, m.name) || !want(p, "version") || !name(p, m.version))
            return 0;
        for (i = 0; i < p->s->nmodules; i++)
            if (!strcmp(m.name, p->s->modules[i].name))
                return fail(p, ZZ_CONFLICT, "duplicate module");
        PUSH(p, modules, nmodules, m);
        strcpy(p->s->module, m.name);
        return 1;
    }
    if (take(p, "end")) {
        if (!want(p, "module"))
            return 0;
        if (!p->s->module[0])
            return fail(p, ZZ_CONFLICT, "no open module");
        p->s->module[0] = 0;
        return 1;
    }
    if (take(p, "export")) {
        if (!p->s->module[0])
            return fail(p, ZZ_CONFLICT, "export outside module");
        if (!want(p, "syntax"))
            return 0;
        do {
            if (!identifier(p, a))
                return 0;
            for (i = 0; i < p->s->nrules; i++)
                if (!strcmp(a, p->s->rules[i].name) && !strcmp(p->s->module, p->s->rules[i].module))
                    break;
            if (i == p->s->nrules)
                return fail(p, ZZ_INVALID, "unknown module rule");
            p->s->rules[i].exported = 1;
        } while (take(p, ","));
        return 1;
    }
    if (take(p, "import")) {
        if (!want(p, "syntax") || !identifier(p, a) || !want(p, "version") || !name(p, b))
            return 0;
        for (i = 0; i < p->s->nmodules; i++)
            if (!strcmp(a, p->s->modules[i].name) && !strcmp(b, p->s->modules[i].version))
                break;
        if (i == p->s->nmodules)
            return fail(p, ZZ_INVALID, "module version not found");
        p->s->modules[i].imported = 1;
        return validate(p);
    }
    if (take(p, "numeric"))
        return want(p, "i64") && want(p, "overflow") && want(p, "error");
    if (take(p, "lexmode"))
        return want(p, "host") && want(p, "identifiers") && want(p, "ascii");
    if (take(p, "layout"))
        return want(p, "lines");
    if (take(p, "budget")) {
        unsigned long n;
        if (!want(p, "nodes"))
            return 0;
        if (p->kind != T_NUMBER || strlen(p->text) > 7 || (n = strtoul(p->text, NULL, 10)) < 16 ||
            n > 100000)
            return fail(p, ZZ_INVALID, "budget must be 16..100000");
        if (n < p->s->ntests || n < p->s->nnodes || n < p->s->nsymbols || n < p->s->nslots ||
            n < p->s->nrules || n < p->s->nmodules || n < p->s->nstatements)
            return fail(p, ZZ_LIMIT, "budget below existing state");
        p->s->budget = (size_t)n;
        next(p);
        return 1;
    }
    return fail(p, ZZ_INVALID, "unsupported checked-profile directive");
}
zz_status zc_parse(zz_context *c, const char *source, int statement_only, int prefix) {
    parser p;
    memset(&p, 0, sizeof(p));
    memset(&c->error, 0, sizeof(c->error));
    p.c = c;
    p.s = c->draft ? c->draft : c->live;
    p.input = p.cur = source;
    p.line = p.col = 1;
    p.prefix = prefix;
    next(&p);
    if (statement_only) {
        if (!statement(&p))
            return c->error.code;
        while (p.kind == T_EOL)
            next(&p);
        if (p.kind != T_END)
            fail(&p, ZZ_INVALID, "expected a single statement");
        return c->error.code;
    }
    while (p.kind != T_END && c->error.code == ZZ_OK) {
        if (p.kind == T_EOL) {
            next(&p);
            continue;
        }
        if (take(&p, "/")) {
            int was_draft = c->draft != NULL;
            uint64_t revision = c->live->revision;
            int mutation = !is(&p, "test") && !is(&p, "patch");
            if (!directive(&p))
                break;
            if (mutation && !was_draft && !c->draft && revision == c->live->revision) {
                if (c->live->revision == UINT64_MAX) {
                    fail(&p, ZZ_LIMIT, "revision overflow");
                    break;
                }
                c->live->revision++;
            }
        } else {
            if (c->draft || p.s->module[0]) {
                fail(&p, ZZ_CONFLICT,
                     "statements are not allowed inside patches or module definitions");
                break;
            }
            if (!statement(&p))
                break;
        }
        if (p.kind != T_EOL && p.kind != T_END) {
            fail(&p, ZZ_INVALID, "unexpected token after directive");
            break;
        }
    }
    if (c->error.code == ZZ_OK && !c->draft && !validate(&p))
        return c->error.code;
    return c->error.code;
}
zz_status zz_context_apply(zz_context *c, const char *source) {
    zz_context *copy;
    zz_context old;
    zz_status status;
    if (!c || !source)
        return ZZ_INVALID;
    if (strlen(source) > ZC_MAX_INPUT)
        return zc_error(c, ZZ_LIMIT, 0, 0, "input exceeds 1 MiB");
    copy = zz_context_clone(c);
    if (!copy)
        return zc_error(c, ZZ_NOMEM, 0, 0, "allocation failed");
    status = zc_parse(copy, source, 0, 0);
    if (status == ZZ_OK) {
        old = *c;
        *c = *copy;
        *copy = old;
        memset(&c->error, 0, sizeof(c->error));
    } else
        c->error = copy->error;
    zz_context_free(copy);
    return status;
}
int zz_context_patch_open(const zz_context *c) { return c && c->draft; }
zz_status zz_context_probe(const zz_context *c, const char *source, zz_diagnostic *diag) {
    zz_context *copy;
    zz_status status;
    if (!c || !source)
        return ZZ_INVALID;
    if (strlen(source) > ZC_MAX_INPUT) {
        if (diag)
            *diag = (zz_diagnostic){ZZ_LIMIT, 0, 0, "input exceeds 1 MiB"};
        return ZZ_LIMIT;
    }
    copy = zz_context_clone(c);
    if (!copy) {
        if (diag)
            *diag = (zz_diagnostic){ZZ_NOMEM, 0, 0, "allocation failed"};
        return ZZ_NOMEM;
    }
    /* Only committed grammar is externally visible. */
    zc_state_free(copy->draft);
    copy->draft = NULL;
    status = zc_parse(copy, source, 1, 1);
    if (diag)
        *diag = copy->error;
    zz_context_free(copy);
    return status;
}
static int visible(const zc_state *s, size_t index) {
    size_t j;
    if (!s->symbols[index].visible)
        return 0;
    for (j = index + 1; j < s->nsymbols; j++)
        if (s->symbols[j].visible && !strcmp(s->symbols[index].name, s->symbols[j].name))
            return 0;
    return 1;
}
int zz_context_lookup_symbol(const zz_context *c, const char *name, zz_symbol_info *info) {
    size_t i;
    if (!c || !name || !info)
        return 0;
    i = c->live->nsymbols;
    while (i--)
        if (c->live->symbols[i].visible && !strcmp(c->live->symbols[i].name, name)) {
            *info = (zz_symbol_info){i, c->live->symbols[i].name, c->live->symbols[i].scope,
                                     c->live->symbols[i].mutable, c->live->revision};
            return 1;
        }
    return 0;
}
size_t zz_context_symbol_count(const zz_context *c) {
    size_t i, n = 0;
    if (!c)
        return 0;
    for (i = 0; i < c->live->nsymbols; i++)
        if (visible(c->live, i))
            n++;
    return n;
}
const char *zz_context_symbol(const zz_context *c, size_t index) {
    size_t i;
    if (!c)
        return NULL;
    for (i = 0; i < c->live->nsymbols; i++)
        if (visible(c->live, i) && index-- == 0)
            return c->live->symbols[i].name;
    return NULL;
}
