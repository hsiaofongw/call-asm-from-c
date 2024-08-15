#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int is_ipv4_str_valid(char *base, int len) {
  char test_buf[16];

  if (len <= 0 || len > sizeof(test_buf) - 1) {
    return 0;
  }

  memcpy(test_buf, base, len);
  test_buf[len] = 0;

  char *head = test_buf, *end = &test_buf[len], *last_cursor;
  int n_octets = 0;
  while (head < end) {
    char *next = &head[1];
    if (isdigit(*head)) {
      char *dec_begin = head;
      int seg_len = 0;
      while (head < end && isdigit(*head)) {
        ++head;
        ++seg_len;
      }

      if (seg_len > 1 && *dec_begin == '0') {
        return 0;
      }

      int next_is_dot = &head[1] < end && head[0] == '.';
      int next_is_eof = head == end;
      if (!(next_is_dot || next_is_eof)) {
        return 0;
      }

      long val = strtol(dec_begin, NULL, 10);
      if (val < 0 || val > 255) {
        return 0;
      }
      ++n_octets;
      ++head;
    } else {
      return 0;
    }
  }

  return n_octets == 4;
}