/* Behavioral coverage of the public checked-profile API. */
#include "zz_checked.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
#define APPLY(c, s)                                                                                \
    do {                                                                                           \
        zz_status rc = zz_context_apply(c, s);                                                     \
        if (rc != ZZ_OK) {                                                                         \
            fprintf(stderr, "line %d: %s\n", __LINE__, zz_context_error(c)->message);              \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static const char *setup =
    "/patch begin \"increment\" base \"g0\"\n"
    "/symbol total : i64 mutable\n"
    "/slot ref : Symbol<i64> from visible_symbols\n"
    "/rule inc : Stmt -> \"bump\" ref^x \"by\" expr<i64>^n => Assign(x, CheckedAdd(Load(x), n))\n"
    "/test accepts \"bump total by 1\"\n"
    "/test rejects \"bump missing by 1\"\n"
    "/patch check\n/patch commit\n";
static int output(zz_context *c, const char *expected) {
    char buf[512] = {0};
    FILE *f = tmpfile();
    zz_status status;
    if (!f)
        return 0;
    status = zz_context_run(c, f);
    rewind(f);
    fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    return status == ZZ_OK && !strcmp(buf, expected);
}
int main(void) {
    zz_context *c = zz_context_new(), *other = zz_context_new(), *saved;
    zz_diagnostic d;
    zz_symbol_info outer, inner;
    uint64_t rev;
    FILE *f;
    size_t n;
    char deep[2048];
    CHECK(c && other);
    APPLY(c, setup);
    CHECK(zz_context_revision(c) == 1);
    CHECK(zz_context_symbol_count(c) == 1 && !strcmp(zz_context_symbol(c, 0), "total"));
    CHECK(zz_context_symbol_count(other) == 0);
    CHECK(zz_context_probe(other, "bump total by 1", &d) == ZZ_INVALID);
    APPLY(c, "total = 2 + 3 * 4\nbump total by 5\nprint(total)\nprint(total - 4)\n");
    CHECK(output(c, "19\n15\n"));
    CHECK(output(c, "19\n15\n"));
    rev = zz_context_revision(c);
    saved = zz_context_clone(c);
    CHECK(saved);
    CHECK(zz_context_apply(c, "/symbol ghost : i64 mutable\nmissing = 1\n") == ZZ_INVALID);
    CHECK(zz_context_revision(c) == rev && zz_context_symbol_count(c) == 1);
    CHECK(output(c, "19\n15\n"));
    CHECK(zz_context_apply(c, "/patch begin \"old\" base \"g0\"\n") == ZZ_CONFLICT);
    CHECK(!zz_context_patch_open(c));
    APPLY(c, "/patch begin \"candidate\" base \"g1\"\n/symbol private : i64 mutable\n");
    CHECK(zz_context_patch_open(c));
    CHECK(zz_context_symbol_count(c) == 1);
    CHECK(zz_context_probe(c, "private = 1", &d) == ZZ_INVALID);
    CHECK(zz_context_apply(c, "/patch commit\n") == ZZ_CONFLICT);
    APPLY(c, "/patch check\n/symbol later : i64 mutable\n");
    CHECK(zz_context_apply(c, "/patch commit\n") == ZZ_CONFLICT);
    APPLY(c, "/patch abort\n");
    CHECK(zz_context_revision(c) == rev);
    APPLY(
        c,
        "/patch begin \"tests\"\n/test rejects \"bump added by 1\"\n/symbol added : i64 mutable\n");
    CHECK(zz_context_apply(c, "/patch check\n") == ZZ_INVALID);
    APPLY(c, "/patch abort\n");
    APPLY(c, "/patch begin \"conflict\"\n/rule dup : Stmt -> \"bump\" ref^x \"with\" expr<i64>^n "
             "=> Assign(x, CheckedAdd(Load(x), n))\n");
    CHECK(zz_context_apply(c, "/patch check\n") == ZZ_CONFLICT);
    APPLY(c, "/patch abort\n");
    CHECK(zz_context_probe(c, "", &d) == ZZ_INCOMPLETE);
    CHECK(zz_context_probe(c, "bu", &d) == ZZ_INCOMPLETE);
    CHECK(zz_context_probe(c, "bump to", &d) == ZZ_INCOMPLETE);
    CHECK(zz_context_probe(c, "bump total b", &d) == ZZ_INCOMPLETE);
    CHECK(zz_context_probe(c, "bump total by 3 +", &d) == ZZ_INCOMPLETE);
    CHECK(zz_context_probe(c, "bump total by 3", &d) == ZZ_OK);
    CHECK(zz_context_probe(c, "bump missing by 3", &d) == ZZ_INVALID);
    CHECK(zz_context_probe(c, "print(total)", &d) == ZZ_OK);
    CHECK(zz_context_probe(c, "print(total) junk", &d) == ZZ_INVALID);
    CHECK(zz_context_probe(c, "print(total)\nprint(total)", &d) == ZZ_INVALID);
    CHECK(output(c, "19\n15\n"));
    CHECK(zz_context_lookup_symbol(c, "total", &outer));
    APPLY(c, "/scope push\n/symbol total : i64 mutable\n");
    CHECK(zz_context_symbol_count(c) == 1);
    CHECK(zz_context_lookup_symbol(c, "total", &inner) && inner.id != outer.id && inner.scope == 1);
    APPLY(c, "/scope pop\n");
    CHECK(zz_context_lookup_symbol(c, "total", &inner) && inner.id == outer.id);
    APPLY(c, "/symbol frozen : i64\n");
    CHECK(zz_context_probe(c, "frozen", &d) == ZZ_INVALID);
    CHECK(zz_context_probe(c, "bump frozen", &d) == ZZ_INVALID);
    CHECK(zz_context_probe(c, "bump frozen by", &d) == ZZ_INVALID);
    CHECK(zz_context_probe(c, "print(frozen)", &d) == ZZ_OK);
    CHECK(zz_context_probe(c, "\"", &d) == ZZ_INVALID);
    CHECK(zz_context_apply(c, "\"print\"(1)\n") == ZZ_INVALID);
    {
        const char *valid = "bump total by -12 * (total + 5)";
        char prefix[128];
        size_t k;
        for (k = 0; k <= strlen(valid); k++) {
            zz_status rc;
            memcpy(prefix, valid, k);
            prefix[k] = 0;
            rc = zz_context_probe(c, prefix, &d);
            CHECK(rc == ZZ_OK || rc == ZZ_INCOMPLETE);
        }
    }
    APPLY(c, "/scope push\n/symbol total : i64 mutable\ntotal = 7\nprint(total)\n/scope "
             "pop\nprint(total)\n");
    CHECK(output(c, "19\n15\n7\n19\n"));
    CHECK(output(saved, "19\n15\n"));
    CHECK(zz_context_apply(c, "/scope pop\n") == ZZ_CONFLICT);
    CHECK(zz_context_apply(c, "/symbol total : i64 mutable\n") == ZZ_CONFLICT);
    CHECK(zz_context_apply(c, "/symbol immutable : i64\nimmutable = 1\n") == ZZ_INVALID);
    CHECK(zz_context_apply(c, "/layout indentation\n") == ZZ_INVALID);
    CHECK(zz_context_apply(c, "/lexmode host identifiers unicode\n") == ZZ_INVALID);
    CHECK(zz_context_apply(c, "total = 9223372036854775808\n") == ZZ_INVALID);
    CHECK(zz_context_apply(c, "total = -9223372036854775809\n") == ZZ_INVALID);
    APPLY(other, "/symbol x : i64 mutable\n/operator \"-\" infix precedence 40 associativity "
                 "right\nx = 10 - 3 - 2\nprint(x)\n");
    CHECK(output(other, "9\n"));
    APPLY(other,
          "/operator \"+\" infix precedence 60 associativity left\nx = 2 + 3 * 4\nprint(x)\n");
    CHECK(output(other, "9\n20\n"));
    zz_context_free(other);
    other = zz_context_new();
    CHECK(other);
    APPLY(other, "/symbol x : i64 mutable\n/slot r : Symbol<i64> from visible_symbols\n/module m "
                 "version \"1\"\n/rule credit : Stmt -> \"credit\" r^x \"with\" expr<i64>^n => "
                 "Assign(x, CheckedAdd(Load(x), n))\n/export syntax credit\n/end module\n/test "
                 "rejects \"credit x with 1\"\n");
    CHECK(zz_context_apply(other, "/import syntax m version \"2\"\n") == ZZ_INVALID);
    APPLY(other, "/import syntax m version \"1\"\ncredit x with 8\nprint(x)\n");
    CHECK(output(other, "8\n"));
    APPLY(saved, "total = 9223372036854775807 + 1\n");
    f = tmpfile();
    CHECK(f);
    CHECK(zz_context_run(saved, f) == ZZ_INVALID);
    CHECK(ftell(f) == 0);
    fclose(f);
    memset(deep, '-', sizeof(deep) - 1);
    deep[sizeof(deep) - 1] = 0;
    CHECK(zz_context_probe(c, deep, &d) == ZZ_INVALID);
    memcpy(deep, "total = ", 8);
    CHECK(zz_context_probe(c, deep, &d) == ZZ_LIMIT);
    for (n = 0; n < 300; n++)
        deep[n] = 'a';
    deep[300] = 0;
    CHECK(zz_context_probe(c, deep, &d) == ZZ_LIMIT);
    zz_context_free(c);
    zz_context_free(other);
    zz_context_free(saved);
    return 0;
}
