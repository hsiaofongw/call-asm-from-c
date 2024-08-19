#include <stdio.h>
#include <string.h>

#include "../validate_dns_name.h"

typedef struct case_t {
  char *name;
  int result;
} case_t;

int main() {
  case_t cases[] = {{.name = "www.qq.com", .result = 1}};
  const int nr_cases = sizeof(cases) / sizeof(case_t);
  for (int i = 0; i < nr_cases; ++i) {
    printf("[%d] \"%s\" (length: %lu) expected: %d actual: %d\n", i,
           cases[i].name, strlen(cases[i].name), cases[i].result, 0);
  }

  return 0;
}
