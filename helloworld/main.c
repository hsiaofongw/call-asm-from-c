
extern void hello(char *msg, long len);

// the *eax would be use as the EAX argument to call cpuid,
// cpu_vendor at least 12 chars.
// eax also be written the EAX return value of cpuid instruction.
extern void cpuid0(int *eax, char *cpu_vendor);

char msg[] = "hello, world!\n";

int main() {
  hello(msg, sizeof(msg));
  return 0;
}
