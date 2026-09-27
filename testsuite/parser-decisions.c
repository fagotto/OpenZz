/* Characterization tests: failures listed here are intentional limitations.
 * One process per scenario keeps grammar, counters and diagnostics isolated. */
#include "zz_tokens.h"
#include "zzbind.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct scenario {
  const char *name, *grammar, *input;
  int mode, accepted, first, second, third;
};
static const struct scenario scenarios[] = {
  {"name", "/stat -> word ident^ident { record 1 };", "word select;", 0,1,1,0,0},
  {"literal-wins", "/stat -> word ident^ident { record 1 }; /stat -> word select ident^ident \":\" { record 2 };", "word select x:;",0,1,0,1,0},
  {"no-fallback", "/stat -> word ident^ident { record 1 }; /stat -> word select ident^ident \":\" { record 2 };", "word select;",0,0,0,0,0},
  {"reverse-order", "/stat -> word select ident^ident \":\" { record 2 }; /stat -> word ident^ident { record 1 };", "word select;",0,0,0,0,0},
  {"other-context", "/stat -> word ident^ident { record 1 }; /stat -> command select ident^ident \":\" { record 2 };", "word select;",0,1,1,0,0},
  {"same-prefix", "/stat -> route ident^ident \":\" { record 1 }; /stat -> route ident^ident \";\" { record 2 };", "route x:;",0,1,1,0,0},
  {"same-prefix-second", "/stat -> route ident^x \":\" { record 1 }; /stat -> route ident^x \";\" { record 2 };", "route x;;",0,1,0,1,0},
  {"dynamic-context", "/stat -> word ident^x { record 1 }; /stat -> enable { /stat -> word select ident^x \":\" { record 2 } };", "word select; enable; word select x:;",0,1,1,1,0},
  {"dynamic-fallback", "/stat -> word ident^x { record 1 }; /stat -> enable { /stat -> word select ident^x \":\" { record 2 } };", "word select; enable; word select;",0,0,1,0,0},
  {"reduce-conflict", "/left -> ident^ident { record 1 }; /right -> ident^ident { record 2 }; /stat -> pick left^left \":\" { record 3 }; /stat -> pick right^right \":\" { record 3 };", "pick x:;",0,0,0,0,0},
  {"distinct-follow", "/left -> ident^ident { record 1 }; /right -> ident^ident { record 2 }; /stat -> pick left^left \":\" { record 3 }; /stat -> pick right^right \"+\" { record 3 };", "pick x:;",0,1,1,0,1},
  {"late-distinction", "/left -> ident^x { record 1 }; /right -> ident^x { record 2 }; /stat -> pick left^x \":\" a { record 3 }; /stat -> pick right^x \":\" b { record 3 };", "pick x: a;",0,0,0,0,0},
  {"factored", "/item -> ident^x { record 1 }; /stat -> pick item^x \":\" a { record 2 }; /stat -> pick item^x \":\" b { record 3 };", "pick x: a;",0,1,1,1,0},
  {"early-effect", "/piece -> ident^ident { record 1 }; /stat -> begin piece^piece \":\" int^int { record 2 };", "begin x: wrong;",0,0,1,0,0},
  {"complete-effect", "/piece -> ident^ident { record 1 }; /stat -> begin piece^piece \":\" int^int { record 2 };", "begin x: 7;",0,1,1,1,0},
  {"split-operator", "/atom -> ident^ident :pass; /expr -> atom^atom :pass; /expr -> expr^expr \"==\" atom^atom :pass; /target -> ident^ident :pass; /stat -> go target^target \"=\" expr^expr { record 1 }; /stat -> go expr^expr { record 2 };", "go x = y;",0,0,0,0,0},
  {"atomic-assignment", "/atom -> ident^ident :pass; /expr -> atom^atom :pass; /expr -> expr^expr EQ atom^atom :pass; /target -> ident^ident :pass; /stat -> go target^target ASSIGN expr^expr { record 1 }; /stat -> go expr^expr { record 2 };", "go x = y;",1,1,1,0,0},
  {"atomic-equality", "/atom -> ident^ident :pass; /expr -> atom^atom :pass; /expr -> expr^expr EQ atom^atom :pass; /target -> ident^ident :pass; /stat -> go target^target ASSIGN expr^expr { record 1 }; /stat -> go expr^expr { record 2 };", "go x == y;",1,1,0,1,0},
  {"operator-tags", "/atom -> ident^x :pass; /expr -> atom^x :pass; /expr -> expr^x eqop^op atom^y :pass; /target -> ident^x :pass; /stat -> go target^x assignop^op expr^y { record 1 }; /stat -> go expr^x { record 2 };", "go x == y;",3,1,0,1,0},
  {"typed-name", "/stat -> word ident^ident { record 1 }; /stat -> word selection^selection ident^ident \":\" { record 2 };", "word select;",2,1,1,0,0},
  {"typed-keyword", "/stat -> word ident^ident { record 1 }; /stat -> word selection^selection ident^ident \":\" { record 2 };", "word select x:;",2,1,0,1,0},
  {"typed-incomplete", "/stat -> word ident^ident { record 1 }; /stat -> word selection^selection ident^ident \":\" { record 2 };", "word select x;",2,0,0,0,0}
};
static int counts[4];
static int record(int argc, struct s_content *argv, struct s_content *ret)
{
  int n;
  (void)ret;
  if (argc != 1) abort();
  n = argv[0].val.ivalue;
  if (n < 1 || n > 3) abort();
  ++counts[n];
  return 1;
}
struct input { char *cursor; int mode, reads; };
static int reader(void *user, struct zz_token *out)
{
  struct input *in = user;
  char *p = in->cursor;
  if (++in->reads > 256) return ZZ_TOKEN_ERROR;
  while (*p == ' ' || *p == '\t') ++p;
  if (!*p) return ZZ_TOKEN_END;
  if ((in->mode == 1 || in->mode == 3) && *p == '=') {
    out->value.tag = tag_ident;
    out->value.val.svalue = p[1] == '=' ? "EQ" : "ASSIGN";
    if (in->mode == 3) {
      out->value.tag = find_tag(p[1] == '=' ? "eqop" : "assignop");
      out->value.val.llvalue = 0;
    }
    in->cursor = p + (p[1] == '=' ? 2 : 1);
    return ZZ_TOKEN_OK;
  }
  zlex(&in->cursor, &out->value);
  if (out->value.tag == tag_eof) return ZZ_TOKEN_END;
  if (in->mode == 2 && out->value.tag == tag_ident &&
      !strcmp(out->value.val.svalue, "select")) {
    /* A deliberately bounded lexer policy, not parser contextual fallback:
     * classify 'select' only before IDENT ':' in this toy language. */
    struct s_content next = {0}, after = {0};
    p = in->cursor;
    zlex(&p, &next);
    if (next.tag == tag_ident) {
      zlex(&p, &after);
      if (after.tag == tag_char && !strcmp(after.val.svalue, ":"))
        out->value.tag = find_tag("selection");
    }
  }
  return ZZ_TOKEN_OK;
}
int main(int argc, char **argv)
{
  size_t i;
  int accepted;
  struct input in;
  const struct scenario *s = NULL;
  if (argc != 2) return 2;
  for (i=0; i<sizeof(scenarios)/sizeof(scenarios[0]); ++i)
    if (!strcmp(argv[1], scenarios[i].name)) s = &scenarios[i];
  if (!s) return 2;
  zz_init();
  zz_bind_open("stat"); zz_bind_keyword("record"); zz_bind_match("int");
  zz_bind_call_exe_no_tag(record); zz_bind_close();
  zz_lex_add_new_tag("selection", NULL, NULL, NULL, NULL, NULL);
  zz_lex_add_new_tag("eqop", NULL, NULL, NULL, NULL, NULL);
  zz_lex_add_new_tag("assignop", NULL, NULL, NULL, NULL, NULL);
  if (!zz_parse_string(s->grammar)) return 2;
  in.cursor = (char *)s->input; in.mode = s->mode; in.reads = 0;
  accepted = zz_parse_tokens(s->name, reader, &in);
  printf("%s: accepted=%d actions=%d,%d,%d\n", s->name, accepted,
         counts[1], counts[2], counts[3]);
  return accepted != s->accepted || counts[1] != s->first ||
         counts[2] != s->second || counts[3] != s->third;
}
