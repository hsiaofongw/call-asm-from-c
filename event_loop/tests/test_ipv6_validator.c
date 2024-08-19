#include <stdio.h>
#include <string.h>

#include "../validate.h"
#include "utils.h"

int main() {
  str_case_t cases[] = {
      {.input = "2001:db8:85a3:0::8a2E:0370:7334", .expect = 1},
      {.input = "2001:0db8:85a3:0:0:8A2E:0370:7334:", .expect = 0},
      {.input = "2001:0db8:85a3:0:0:8A2E:0370:7334", .expect = 1},
      {.input = "192.168.255.1", .expect = 0},
      {.input = "a192", .expect = 0},
      {.input = "1::2", .expect = 1},
      {.input = "1234", .expect = 0},
      {.input = "1", .expect = 0},
      {.input = "", .expect = 0},
      {.input = "::", .expect = 1},
      {.input = "12::", .expect = 1},
      {.input = "1234::", .expect = 1},
      {.input = "12:123:1234::1", .expect = 1},
      {.input = "12:123:1234:12345:1", .expect = 0},
      {.input = "abcd:e", .expect = 0},
      {.input = "abcd::e", .expect = 1},
      {.input = "abcde::", .expect = 0},
      {.input = "::12345", .expect = 0},
      {.input = "::1234", .expect = 1},
      {.input = "::123:abc", .expect = 1},
      {.input = "::12345:ab", .expect = 0}};

  const int nr_cases = sizeof(cases) / sizeof(str_case_t);
  int exit_code = 0;
  for (int i = 0; i < nr_cases; ++i) {
    str_case_t *c = &cases[i];
    size_t len = strlen(c->input);

    int result = is_ipv6_str_valid(c->input, len);
    printf("[%d] \"%s\" (length: %lu) expected: %d actual: %d\n", i, c->input,
           len, c->expect, result);
    exit_code = exit_code || (result != c->expect);
  }

  return exit_code;
}
