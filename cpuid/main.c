#include <stdio.h>

// the *eax would be use as the EAX argument to call cpuid,
// cpu_vendor at least 12 chars.
// eax also be written the EAX return value of cpuid instruction.
extern void cpuid0(int *eax, char *cpu_vendor);

#define CPUID_VENDOR_BUF 1024
char msg[CPUID_VENDOR_BUF];

int main() {
  int eax = 0;
  cpuid0(&eax, msg);
  msg[12] = 0;

  printf("EAX: %d\n", eax);
  printf("CPU Vendor: %s\n", msg);

  return 0;
}
