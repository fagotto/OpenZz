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
typedef enum {
    ZZ_OK, ZZ_INVALID, ZZ_INCOMPLETE, ZZ_LIMIT, ZZ_NOMEM, ZZ_CONFLICT
} zz_status;
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
/* Checked arithmetic, shared by interpreter and generated C contract. */
int zz_i64_add(int64_t a, int64_t b, int64_t *out);
int zz_i64_sub(int64_t a, int64_t b, int64_t *out);
int zz_i64_mul(int64_t a, int64_t b, int64_t *out);
#ifdef __cplusplus
}
#endif
#endif
