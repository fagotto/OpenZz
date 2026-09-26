/* Regression checks for sparse lists and lexer boundaries. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zz.h"
#include "zlex.h"
#include "list.h"
#include "source.h"
#include <fcntl.h>
#include <unistd.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main(void)
{
  struct s_content a, b, item, *joined, token;
  struct s_list *sparse;
  char *input, *cursor;
  int errors;
  zz_set_output_stream(stdout);
  zz_init();
  CHECK(source_pipe());
  CHECK(pop_source());
  CHECK(fcntl(STDIN_FILENO, F_GETFD) != -1);

  input = malloc(301);
  CHECK(input != NULL);
  memset(input, 'a', 300);
  input[300] = '\0';
  CHECK(!zz_parse_file(input));
  input[0] = '.';
  input[100] = '\0';
  CHECK(!zz_parse_file(input));
  free(input);
  create_list(&a, 1);
  create_list(&b, 2);
  item.tag = tag_int;
  s_content_ivalue(item) = 42;
  append_to_list(&a, &item);
  append_to_list(&b, &item);
  append_to_list(&b, &item);
  sparse = s_content_pvalue(b);
  sparse->array[0].tag = tag_none;
  joined = s_concat_list(&a, &b);
  CHECK(get_list_size(joined) == 2);
  delete_list(joined);
  free(joined);
  merge_list(&a, &b);
  CHECK(get_list_size(&a) == 2);
  delete_list(&a);
  delete_list(&b);

  /* More exponent digits than the lexer's stack buffer. */
  input = malloc(4096);
  CHECK(input != NULL);
  strcpy(input, "1.0e+");
  memset(input + 5, '1', 4090);
  input[4095] = '\0';
  cursor = input;
  errors = zz_get_error_number();
  zlex(&cursor, &token);
  CHECK(cursor == input + 4095);
  CHECK(zz_get_error_number() > errors);
  free(input);

  /* Long mantissa plus decimal point used to overflow before the assertion. */
  input = malloc(259);
  CHECK(input != NULL);
  memset(input, '1', 255);
  strcpy(input + 255, ".0");
  cursor = input;
  errors = zz_get_error_number();
  zlex(&cursor, &token);
  CHECK(*cursor == '\0');
  CHECK(zz_get_error_number() > errors);
  free(input);

  /* Exact-size allocation makes a read past the terminal NUL visible to ASan. */
  input = malloc(3);
  CHECK(input != NULL);
  input[0] = '"'; input[1] = '\\'; input[2] = '\0';
  cursor = input;
  errors = zz_get_error_number();
  zlex(&cursor, &token);
  CHECK(cursor == input + 2);
  CHECK(zz_get_error_number() > errors);
  free(s_content_svalue(token));
  free(input);
  return 0;
}
