#include <stdio.h>

#include "err.h"
#include "pkt.h"

int main() {
  pkt *p;
  int status = pkt_create(&p, PktTyMsg, get_default_allocator());
  if (status != 0) {
    fprintf(stderr, "Failed to create packet: %s\n", err_code_2_str(status));
    exit(1);
  }

  pkt_free(&p);

  return 0;
}