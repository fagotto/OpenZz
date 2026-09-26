/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ZZ_CHECKED_INTERNAL_H
#define ZZ_CHECKED_INTERNAL_H
#include "zz_checked.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#define ZC_NAME 64
#define ZC_TOKEN 256
#define ZC_MAX_DEPTH 128
#define ZC_DEFAULT_BUDGET 4096
#define ZC_MAX_INPUT (1024 * 1024)
typedef struct { char name[ZC_NAME]; int mutable; } zc_symbol;
typedef struct { char name[ZC_NAME]; } zc_slot;
typedef struct {
    char name[ZC_NAME], verb[ZC_NAME], separator[ZC_NAME], slot[ZC_NAME];
    char module[ZC_NAME];
    unsigned exported;
} zc_rule;
typedef struct { char token; unsigned precedence; int right; } zc_operator;
typedef enum { ZC_CONST, ZC_LOAD, ZC_ADD, ZC_SUB, ZC_MUL, ZC_ASSIGN, ZC_PRINT } zc_kind;
typedef struct { zc_kind kind; size_t a, b; int64_t value; } zc_node;
typedef struct {
    uint64_t revision;
    size_t budget;
    zc_symbol *symbols; size_t nsymbols;
    zc_slot *slots; size_t nslots;
    zc_rule *rules; size_t nrules;
    zc_node *nodes; size_t nnodes;
    size_t *statements; size_t nstatements;
    zc_operator ops[3];
} zc_state;
struct zz_context {
    zc_state *live, *draft;
    zz_diagnostic error;
    char patch[ZC_NAME];
    int checked;
};
zc_state *zc_state_new(void);
zc_state *zc_state_clone(const zc_state *s);
void zc_state_free(zc_state *s);
zz_status zc_error(zz_context *c, zz_status code, size_t line, size_t col, const char *fmt, ...);
int zc_append(zz_context *c, void **items, size_t *count, size_t width, const void *item);
#endif
