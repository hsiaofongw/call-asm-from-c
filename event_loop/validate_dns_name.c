#include <ctype.h>

#include "limitations.h"

int is_dns_name_valid(char *base, int len) {
  if (len <= 0 || len > MAX_HOSTNAME_ALLOWED) {
    return 0;
  }

  char *head = base, *end = &base[len];
  int label_len = 0;
  while (head < end) {
    if (isalnum(*head)) {
      label_len = 1;
      char *next = &head[1];
      if (next == end) {
        return 1;
      } else if (*next == '.') {
        head = &next[1];
        continue;
      } else if (isalnum(*next) || *next == '-') {
        char *w_begin = head;
        while (head < end && (isalnum(*head) || *head == '-')) {
          ++label_len;
          ++head;
        }

        if ((head < end && *head == '.') || head == end) {
          if (head > w_begin && head[-1] == '-') {
            return 0;
          }

          if (label_len > MAX_DNS_LABEL_LEN) {
            return 0;
          }

          head = &head[1];
          continue;
        } else {
          return 0;
        }
      } else {
        return 0;
      }
    } else {
      return 0;
    }
  }

  return end[-1] != '.';
}
