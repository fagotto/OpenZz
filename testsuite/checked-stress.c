/* Deterministic malformed-input and arithmetic differential checks.
 * Not a claim of exhaustive fuzzing; reproducible on every CI platform. */
#include "zz_checked.h"
#include <stdint.h>
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
static uint64_t rng = UINT64_C(0x12345678);
static uint64_t random_bits(void) {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
}
int main(void) {
    const char *seed = "/patch begin \"p\"\n/symbol total : i64 mutable\n/slot r : Symbol<i64> "
                       "from visible_symbols\n/rule inc : Stmt -> \"bump\" r^x \"by\" expr<i64>^n "
                       "=> Assign(x, CheckedAdd(Load(x), n))\n/patch check\n/patch commit\ntotal = "
                       "2 + 3 * 4\nbump total by 5\nprint(total)\n";
    size_t i, j, len = strlen(seed);
    char buf[1024];
    zz_context *c = zz_context_new();
    CHECK(c);
    for (i = 0; i < 3000; i++) {
        zz_context *trial = zz_context_clone(c);
        zz_status status;
        zz_diagnostic d;
        size_t n = random_bits() % (len + 1);
        CHECK(trial);
        memcpy(buf, seed, n);
        buf[n] = 0;
        for (j = 0; j < 4 && n; j++)
            buf[random_bits() % n] = (char)(1 + random_bits() % 255);
        status = zz_context_apply(trial, buf);
        CHECK(status >= ZZ_OK && status <= ZZ_CONFLICT);
        if (status != ZZ_OK)
            CHECK(zz_context_revision(trial) == 0 && zz_context_symbol_count(trial) == 0);
        (void)zz_context_probe(trial, buf, &d);
        zz_context_free(trial);
    }
    for (i = 0; i < 100000; i++) {
        int64_t a, b, got, expected;
        uint64_t ua = random_bits(), ub = random_bits();
        int overflow, ok;
        memcpy(&a, &ua, sizeof(a));
        memcpy(&b, &ub, sizeof(b));
        overflow = __builtin_add_overflow(a, b, &expected);
        ok = zz_i64_add(a, b, &got);
        CHECK(ok == !overflow && (!ok || got == expected));
        overflow = __builtin_sub_overflow(a, b, &expected);
        ok = zz_i64_sub(a, b, &got);
        CHECK(ok == !overflow && (!ok || got == expected));
        overflow = __builtin_mul_overflow(a, b, &expected);
        ok = zz_i64_mul(a, b, &got);
        CHECK(ok == !overflow && (!ok || got == expected));
    }
    zz_context_free(c);
    return 0;
}
