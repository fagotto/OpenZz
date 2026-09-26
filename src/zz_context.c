/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "zz_checked_internal.h"
zc_state *zc_state_new(void)
{
    zc_state *s = calloc(1, sizeof(*s));
    if (s) {
        s->budget = ZC_DEFAULT_BUDGET;
        s->ops[0] = (zc_operator){'+', 40, 0};
        s->ops[1] = (zc_operator){'-', 40, 0};
        s->ops[2] = (zc_operator){'*', 50, 0};
    }
    return s;
}
void zc_state_free(zc_state *s)
{
    if (!s) return;
    free(s->symbols); free(s->slots); free(s->rules);
    free(s->nodes); free(s->statements); free(s);
}
zc_state *zc_state_clone(const zc_state *s)
{
    zc_state *n = zc_state_new();
    if (!n) return NULL;
    n->revision = s->revision; n->budget = s->budget;
    memcpy(n->ops, s->ops, sizeof(n->ops));
#define COPY(field, count) do { \
    if (s->count) { \
        n->field = malloc(s->count * sizeof(*s->field)); \
        if (!n->field) goto fail; \
        memcpy(n->field, s->field, s->count * sizeof(*s->field)); \
        n->count = s->count; \
    } \
} while (0)
    COPY(symbols, nsymbols); COPY(slots, nslots); COPY(rules, nrules);
    COPY(nodes, nnodes); COPY(statements, nstatements);
#undef COPY
    return n;
fail:
    zc_state_free(n); return NULL;
}
zz_context *zz_context_new(void)
{
    zz_context *c = calloc(1, sizeof(*c));
    if (c && !(c->live = zc_state_new())) { free(c); return NULL; }
    return c;
}
zz_context *zz_context_clone(const zz_context *c)
{
    zz_context *n;
    if (!c) return NULL;
    n = calloc(1, sizeof(*n));
    if (!n) return NULL;
    n->live = zc_state_clone(c->live);
    if (c->draft) n->draft = zc_state_clone(c->draft);
    if (!n->live || (c->draft && !n->draft)) { zz_context_free(n); return NULL; }
    n->error = c->error; n->checked = c->checked;
    memcpy(n->patch, c->patch, sizeof(n->patch));
    return n;
}
void zz_context_free(zz_context *c)
{
    if (!c) return;
    zc_state_free(c->live); zc_state_free(c->draft); free(c);
}
const zz_diagnostic *zz_context_error(const zz_context *c) { return c ? &c->error : NULL; }
uint64_t zz_context_revision(const zz_context *c) { return c ? c->live->revision : 0; }
zz_status zc_error(zz_context *c, zz_status code, size_t line, size_t col, const char *fmt, ...)
{
    va_list ap;
    c->error.code = code; c->error.line = line; c->error.column = col;
    va_start(ap, fmt); vsnprintf(c->error.message, sizeof(c->error.message), fmt, ap); va_end(ap);
    return code;
}
int zc_append(zz_context *c, void **items, size_t *count, size_t width, const void *item)
{
    zc_state *s = c->draft ? c->draft : c->live;
    void *p;
    if (*count >= s->budget || *count >= SIZE_MAX / width - 1) {
        zc_error(c, ZZ_LIMIT, 0, 0, "resource budget exhausted"); return 0;
    }
    p = realloc(*items, (*count + 1) * width);
    if (!p) { zc_error(c, ZZ_NOMEM, 0, 0, "allocation failed"); return 0; }
    *items = p; memcpy((char *)p + (*count)++ * width, item, width); return 1;
}
int zz_i64_add(int64_t a, int64_t b, int64_t *out)
{
    if (!out || (b > 0 && a > INT64_MAX-b) || (b < 0 && a < INT64_MIN-b)) return 0;
    *out = a+b; return 1;
}
int zz_i64_sub(int64_t a, int64_t b, int64_t *out)
{
    if (!out || (b > 0 && a < INT64_MIN+b) || (b < 0 && a > INT64_MAX+b)) return 0;
    *out = a-b; return 1;
}
int zz_i64_mul(int64_t a, int64_t b, int64_t *out)
{
    if (!out) return 0;
    if (a > 0) {
        if ((b > 0 && a > INT64_MAX/b) || (b < 0 && b < INT64_MIN/a)) return 0;
    } else if (a < 0) {
        if ((b > 0 && a < INT64_MIN/b) || (b < 0 && a < INT64_MAX/b)) return 0;
    }
    *out = a*b; return 1;
}
