#include <unistd.h>

char buf[1024];

int main() {
  while (1) {
    int nbytes_read = read(STDIN_FILENO, buf, sizeof(buf));
    if (nbytes_read <= 0) {
      if (nbytes_read < 0) {
        return nbytes_read;
      }
      return 0;
    }

    int status = write(STDOUT_FILENO, buf, nbytes_read);
    if (status < 0) {
      return status;
    }
  }

  return 0;
}
