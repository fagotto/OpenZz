/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "zz_checked.h"
#include <stdlib.h>
#include <string.h>
/* No legacy initialization occurs in --checked mode. */
int zz_checked_main(int argc, char **argv) {
    FILE *input;
    char *source;
    size_t n;
    zz_context *ctx;
    zz_status status;
    int emit = 0;
    if (argc == 4 && !strcmp(argv[2], "--emit-c"))
        emit = 1;
    else if (argc != 3) {
        fputs("usage: ozz --checked [--emit-c] source.zz\n", stderr);
        return 2;
    }
    input = fopen(argv[emit ? 3 : 2], "rb");
    if (!input) {
        perror("checked input");
        return 1;
    }
    source = malloc(1024 * 1024 + 2);
    if (!source) {
        fclose(input);
        return 1;
    }
    n = fread(source, 1, 1024 * 1024 + 1, input);
    if (ferror(input) || n > 1024 * 1024 || memchr(source, 0, n)) {
        fputs("input unreadable, too large, or contains NUL bytes\n", stderr);
        fclose(input);
        free(source);
        return 1;
    }
    fclose(input);
    source[n] = 0;
    ctx = zz_context_new();
    if (!ctx) {
        free(source);
        return 1;
    }
    status = zz_context_apply(ctx, source);
    free(source);
    if (status == ZZ_OK)
        status = emit ? zz_context_emit_c(ctx, stdout) : zz_context_run(ctx, stdout);
    if (status != ZZ_OK) {
        const zz_diagnostic *d = zz_context_error(ctx);
        fprintf(stderr, "checked:%zu:%zu: %s (status %d)\n", d->line, d->column, d->message,
                (int)status);
    }
    zz_context_free(ctx);
    return status == ZZ_OK ? 0 : 1;
}
