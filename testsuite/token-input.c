#include "zz_tokens.h"
#include "zzbind.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"failed line %d: %s\n",__LINE__,#x); return 1; } } while (0)
struct input { struct s_content *items; size_t count, pos; int fail; };
static int read_token(void *user, struct zz_token *out)
{
  struct input *in = user;
  if (in->pos == in->count) return in->fail ? ZZ_TOKEN_ERROR : ZZ_TOKEN_END;
  out->value = in->items[in->pos++];
  out->span.line = 7; out->span.column = in->pos;
  out->span.byte_start = in->pos - 1; out->span.byte_end = in->pos;
  return ZZ_TOKEN_OK;
}
static int calls;
static char received[2048];
static int capture(int argc, struct s_content *argv, struct s_content *ret)
{
  (void)ret;
  if (argc != 1) return 0;
  snprintf(received, sizeof(received), "%s", argv[0].val.svalue);
  ++calls;
  return 1;
}
static void bind_capture(const char *name, const char *tag)
{
  zz_bind_open("stat"); zz_bind_keyword(name); zz_bind_match(tag);
  zz_bind_call_exe_no_tag(capture); zz_bind_close();
}
int main(void)
{
  struct s_content tokens[3] = {{0}};
  struct input in = {tokens, 3, 0, 0};
  char long_text[1024];
  zz_set_output_stream(stdout); zz_init();
  bind_capture("take", "ident"); bind_capture("text", "qstring");
  CHECK(zz_parse_string("/host := 42;"));
  tokens[0].tag = tag_ident; tokens[0].val.svalue = "take";
  tokens[1].tag = tag_ident; tokens[1].val.svalue = "host";
  tokens[2].tag = tag_eol;
  CHECK(zz_parse_tokens("opaque-name", read_token, &in));
  CHECK(calls == 1 && !strcmp(received,"host"));
  memset(long_text, 'x', sizeof(long_text)-1); long_text[1023] = 0;
  tokens[0].val.svalue = "text"; tokens[1].tag = tag_qstring;
  tokens[1].val.svalue = long_text; in.pos = 0;
  CHECK(zz_parse_tokens("long-literal", read_token, &in));
  CHECK(calls == 2 && !strcmp(received,long_text));
  /* Definitions inside a committed action are immediately usable. */
  CHECK(zz_parse_string("/stat -> launch { /stat -> fresh { text \"composed\" }; fresh };"));
  tokens[0].val.svalue = "launch"; tokens[1].tag = tag_eol;
  in.count = 2; in.pos = 0;
  CHECK(zz_parse_tokens("native-composition", read_token, &in));
  CHECK(calls == 3 && !strcmp(received,"composed"));
  CHECK(zz_parse_string("/stat -> forward ident^x { take x };"));
  tokens[0].val.svalue = "forward";
  tokens[1].tag = tag_ident; tokens[1].val.svalue = "host";
  in.count = 3; in.pos = 0;
  CHECK(zz_parse_tokens("native-capture", read_token, &in));
  CHECK(calls == 4 && !strcmp(received,"host"));
  in.count = 0; in.pos = 0; in.fail = 1;
  CHECK(!zz_parse_tokens("reader-failure", read_token, &in));
  in.fail = 0;
  CHECK(zz_parse_tokens("empty", read_token, &in));
  CHECK(!zz_parse_tokens("null-reader", NULL, NULL));
  tokens[0].tag = NULL; in.count = 1;
  CHECK(!zz_parse_tokens("invalid-token", read_token, &in));

  tokens[0].tag = tag_ident; tokens[0].val.svalue = "text";
  tokens[1].tag = tag_qstring; tokens[1].val.svalue = "after failure";
  in.count = 3; in.pos = 0;
  CHECK(zz_parse_tokens("after-failure", read_token, &in));
  CHECK(calls == 5 && !strcmp(received,"after failure"));
  return 0;
}
