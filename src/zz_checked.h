/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ZZ_CHECKED_H
#define ZZ_CHECKED_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Experimental, isolated i64/AST profile. Does not call legacy actions. */
typedef struct zz_context zz_context;
typedef enum { ZZ_OK, ZZ_INVALID, ZZ_INCOMPLETE, ZZ_LIMIT, ZZ_NOMEM, ZZ_CONFLICT } zz_status;
typedef struct {
    zz_status code;
    size_t line, column;
    char message[256];
} zz_diagnostic;
zz_context *zz_context_new(void);
zz_context *zz_context_clone(const zz_context *ctx);
void zz_context_free(zz_context *ctx);
const zz_diagnostic *zz_context_error(const zz_context *ctx);
uint64_t zz_context_revision(const zz_context *ctx);
/* Each apply is atomic, including directives, AST and an open candidate patch.
 * No output or native actions occur. A failed call preserves the prior state. */
zz_status zz_context_apply(zz_context *ctx, const char *source);
int zz_context_patch_open(const zz_context *ctx);
/* Pure recognition of ONE statement. Probe never modifies ctx. */
zz_status zz_context_probe(const zz_context *ctx, const char *prefix, zz_diagnostic *diagnostic);
/* Context clones are checkpoints. Restore explicitly by replacing the owned context. */
typedef struct {
    size_t id;
    const char *name; /* borrowed until next successful apply/free */
    unsigned scope;
    int is_mutable;
    uint64_t revision;
} zz_symbol_info;
/* Committed visible bindings, including provenance usable by /explain clients. */
int zz_context_lookup_symbol(const zz_context *ctx, const char *name, zz_symbol_info *info);
size_t zz_context_symbol_count(const zz_context *ctx);
const char *zz_context_symbol(const zz_context *ctx, size_t index);
/* Explicit effects: execute the accepted AST or emit standalone C11.
 * Interpreter buffers all numeric output until evaluation succeeds. */
zz_status zz_context_run(zz_context *ctx, FILE *output);
zz_status zz_context_emit_c(zz_context *ctx, FILE *output);
/* Checked arithmetic, shared by interpreter and generated C contract. */
int zz_i64_add(int64_t a, int64_t b, int64_t *out);
int zz_i64_sub(int64_t a, int64_t b, int64_t *out);
int zz_i64_mul(int64_t a, int64_t b, int64_t *out);
#ifdef __cplusplus
}
#endif
#endif
