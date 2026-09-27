/* External token input for the native ZZ parser. LGPL-2.1-or-later. */
#ifndef OPENZZ_TOKENS_H
#define OPENZZ_TOKENS_H
#include <stddef.h>
#include "zlex.h"
#ifdef __cplusplus
extern "C" {
#endif

struct zz_source_span {
  size_t byte_start, byte_end; /* half-open byte offsets */
  size_t line, column;        /* one-based; zero means unknown */
};
struct zz_token {
  struct s_content value;     /* registered ZZ tag and its payload */
  struct zz_source_span span;
};
enum { ZZ_TOKEN_ERROR = -1, ZZ_TOKEN_END = 0, ZZ_TOKEN_OK = 1 };
/* Fill one token; return OK, END or ERROR. No parser calls from the reader.
 * Payloads are borrowed and must outlive all grammar/parameter references.
 * The reader must not mutate grammar or ZZ state. */
typedef int (*zz_token_reader)(void *user, struct zz_token *token);
/* Call zz_init() first. Synchronous, native root grammar. Returns 1 on success, 0 on error.
 * Names never undergo automatic ZZ parameter substitution. Native actions
 * still execute normally. No rollback, thread safety or parser isolation.
 * name is borrowed for this call. EOF is reported by END, not an EOF token. */
int zz_parse_tokens(const char *name, zz_token_reader reader, void *user);
#ifdef __cplusplus
}
#endif
#endif
