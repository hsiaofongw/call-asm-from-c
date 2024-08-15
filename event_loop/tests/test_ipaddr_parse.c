#include <stdio.h>
#include <string.h>

#include "../validate_ipv4.h"
#include "../validate_ipv6.h"

void test_ipv4() {
  char *addresses[] = {"192.168.3.101", "a",   "",   "1234",
                       "1234.1",        "12.", "1.", "192.168.1.1.1",
                       "192.168.1."};
  const int n_addr = sizeof(addresses) / sizeof(char *);
  int result;
  for (int i = 0; i < n_addr; ++i) {
    char *addr = addresses[i];
    int len = strlen(addr);
    result = is_ipv4_str_valid(addr, len);
    printf("addr: %s length: %d result: %d\n", addr, len, result);
  }
}

int main() {
  test_ipv4();
  return 0;
}