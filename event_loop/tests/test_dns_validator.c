#include <stdio.h>
#include <string.h>

#include "../validate_dns_name.h"

typedef struct case_t {
  char *name;
  int result;
} case_t;

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

  case_t cases[] = {{.name = "www.qq.com", .result = 1},
                    {.name = "www.qq-", .result = 0},
                    {.name = "123", .result = 1},
                    {.name = "-123.0", .result = 0},
                    {.name = "123-.0", .result = 0},
                    {.name = "123-567.o", .result = 1},
                    {.name = "hello-123.world", .result = 1},
                    {.name = "192.168.1.101.", .result = 0},
                    {.name = "1.1.1.1", .result = 1},
                    {.name = long_label, .result = 0},
                    {.name = edge_case_label, .result = 1}};

  const int nr_cases = sizeof(cases) / sizeof(case_t);
  for (int i = 0; i < nr_cases; ++i) {
    case_t *c = &cases[i];
    size_t len = strlen(c->name);

    int result = is_dns_name_valid(c->name, len);
    printf("[%d] \"%s\" (length: %lu) expected: %d actual: %d\n", i, c->name,
           len, c->result, result);
  }

  return 0;
}
