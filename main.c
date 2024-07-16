
extern void hello(char *msg, long len);

char msg[] = "hello, world!\n";

int main() {
  hello(msg, sizeof(msg));
  return 0;
}
