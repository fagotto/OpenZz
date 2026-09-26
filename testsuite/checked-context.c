#include "zz_checked.h"
#include <limits.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void)
{
    zz_context *a = zz_context_new(), *b;
    int64_t n;
    CHECK(a); b = zz_context_clone(a); CHECK(b && b != a);
    zz_context_free(a); CHECK(zz_context_revision(b) == 0); zz_context_free(b);
    CHECK(!zz_i64_add(INT64_MAX,1,&n)); CHECK(!zz_i64_sub(INT64_MIN,1,&n));
    CHECK(!zz_i64_mul(INT64_MIN,-1,&n)); CHECK(!zz_i64_mul(-1,INT64_MIN,&n));
    CHECK(zz_i64_mul(INT64_MIN,1,&n) && n == INT64_MIN);
    CHECK(zz_i64_mul(-4,-5,&n) && n == 20);
    CHECK(zz_i64_mul(0,INT64_MIN,&n) && n == 0);
    return 0;
}
