#include <stdio.h>
#include <stdlib.h>

int main() {
  char s[] = "012";
  char *endptr;
  long res = strtol(s, &endptr, 10);
  printf("res = %ld\n", res);
  printf("addr(s) = 0x%016ld\n", (unsigned long)s);
  printf("endptr = 0x%016ld\n", (unsigned long)endptr);
  printf("*endptr = %d", (int)(*endptr));
  return 0;
}