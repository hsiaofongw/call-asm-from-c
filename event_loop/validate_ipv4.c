#include <stdlib.h>
#include <string.h>

int is_ipv4_str_valid(char *base, int len) {
  if (len <= 0) {
    return 0;
  }

  char test_buf[16];
  if (len > sizeof(test_buf) - 1) {
    return 0;
  }

  memset(test_buf, 0, sizeof(test_buf));
  memcpy(test_buf, base, len);

  char *head = test_buf;
  char *last_cursor;

  for (int i = 0; i < 4; ++i) {
    long val = strtol(head, &last_cursor, 10);

    if (last_cursor == head) {
      return 0;
    }

    if (i < 3 && *last_cursor != '.') {
      return 0;
    } else if (*last_cursor != 0) {
      return 0;
    }

    if (val < 0 || val > 255) {
      return 0;
    }
    head = &last_cursor[1];
  }

  return 1;
}