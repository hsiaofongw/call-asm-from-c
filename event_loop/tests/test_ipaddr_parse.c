#include <stdio.h>
#include <string.h>

#include "../validate_ipv4.h"
#include "../validate_ipv6.h"

int main() {
  char addr[] = "192.168.3.101";
  int result;
  result = is_ipv4_str_valid(addr, strlen(addr));

  printf("addr: %s result: %d\n", addr, result);

  return 0;
}