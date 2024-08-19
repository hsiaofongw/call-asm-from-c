#include <stdio.h>
#include <string.h>

#include "../validate.h"
#include "utils.h"

int main() {
  char long_label[65];
  for (int i = 0; i < sizeof(long_label) - 1; ++i) {
    long_label[i] = '0';
  }
  long_label[sizeof(long_label) - 1] = 0;

  char edge_case_label[64];
  for (int i = 0; i < sizeof(edge_case_label) - 1; ++i) {
    edge_case_label[i] = '0';
  }
  edge_case_label[sizeof(edge_case_label) - 1] = 0;

  str_case_t cases[] = {{.input = "www.qq.com", .expect = 1},
                        {.input = "www.qq-", .expect = 0},
                        {.input = "123", .expect = 1},
                        {.input = "-123.0", .expect = 0},
                        {.input = "123-.0", .expect = 0},
                        {.input = "123-567.o", .expect = 1},
                        {.input = "hello-123.world", .expect = 1},
                        {.input = "192.168.1.101.", .expect = 0},
                        {.input = "1.1.1.1", .expect = 1},
                        {.input = "", .expect = 0},
                        {.input = "1", .expect = 1},
                        {.input = long_label, .expect = 0},
                        {.input = edge_case_label, .expect = 1}};

  const int nr_cases = sizeof(cases) / sizeof(str_case_t);
  int exit_code = 0;
  for (int i = 0; i < nr_cases; ++i) {
    str_case_t *c = &cases[i];
    size_t len = strlen(c->input);

    int result = is_dns_name_valid(c->input, len);
    printf("[%d] \"%s\" (length: %lu) expected: %d actual: %d\n", i, c->input,
           len, c->expect, result);
    exit_code = exit_code || (result != c->expect);
  }

  return exit_code;
}
