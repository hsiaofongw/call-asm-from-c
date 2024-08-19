#include <stdio.h>
#include <string.h>

#include "../validate.h"
#include "utils.h"

int main() {
  str_case_t cases[] = {{.input = "192.168.1.1.", .expect = 0},
                        {.input = "192.168.3.101", .expect = 1},
                        {.input = "a", .expect = 0},
                        {.input = "", .expect = 0},
                        {.input = "1234", .expect = 0},
                        {.input = "1234.1", .expect = 0},
                        {.input = "12.", .expect = 0},
                        {.input = "12.34", .expect = 0},
                        {.input = "1.", .expect = 0},
                        {.input = ".", .expect = 0},
                        {.input = "192.168.1.1.1", .expect = 0},
                        {.input = "192.168.1.", .expect = 0},
                        {.input = "192.168.1.1", .expect = 1},
                        {.input = "192.01.123.1", .expect = 0},
                        {.input = "192.01.123.123", .expect = 0},
                        {.input = "192.1.123.123", .expect = 1},
                        {.input = "192.618.1.0", .expect = 0},
                        {.input = "255.255.255.255", .expect = 1},
                        {.input = "255.2555.255.25", .expect = 0},
                        {.input = "192.0.0.1", .expect = 1},
                        {.input = "0.0.0.0", .expect = 1},
                        {.input = "1.1.1.1", .expect = 1},
                        {.input = "hello你好", .expect = 0}};

  const int nr_cases = sizeof(cases) / sizeof(str_case_t);
  int exit_code = 0;
  for (int i = 0; i < nr_cases; ++i) {
    str_case_t *c = &cases[i];
    size_t len = strlen(c->input);

    int result = is_ipv4_str_valid(c->input, len);
    printf("[%d] \"%s\" (length: %lu) expected: %d actual: %d\n", i, c->input,
           len, c->expect, result);
    exit_code = exit_code || (result != c->expect);
  }

  return exit_code;
}

void test_ipv6() {
  char *addresses[] = {"2001:db8:85a3:0::8a2E:0370:7334",
                       "2001:0db8:85a3:0:0:8A2E:0370:7334:",
                       "2001:0db8:85a3:0:0:8A2E:0370:7334",
                       "192.168.255.1",
                       "a192",
                       "1::2",
                       "1234",
                       "1",
                       "",
                       "::",
                       "12::",
                       "1234::",
                       "12:123:1234::1",
                       "abcd:e",
                       "hello,world",
                       "hi.hello",
                       "::12345",
                       "::1234",
                       "::123:abc",
                       "::12345:ab"};
  const int n_addr = sizeof(addresses) / sizeof(char *);
  for (int i = 0; i < n_addr; ++i) {
    char *addr = addresses[i];
    int len = strlen(addr);
    int result = is_ipv6_str_valid(addr, len);
    printf("addr: \"%s\" length: %d result: %d\n", addr, len, result);
  }
}
