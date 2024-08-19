#include <ctype.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

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
        --label_len;
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

int is_hex(char c) {
  if (isdigit(c)) {
    return 1;
  }
  char hex_chars[] = "abcdefABCDEF";
  for (int i = 0; i < sizeof(hex_chars); ++i) {
    if (hex_chars[i] == c) {
      return 1;
    }
  }
  return 0;
}

int is_ipv6_str_valid(char *base, int len) {
  char test_buf[INET6_ADDRSTRLEN];
  if (len <= 0 || len > sizeof(test_buf) - 1) {
    return 0;
  }

  memcpy(test_buf, base, len);
  test_buf[len] = 0;

  char *head = test_buf, *end = &test_buf[len];
  int n_words = 0, n_wildcard = 0, n_decs = 0;
  while (head < end) {
    char *next = &head[1];
    if (head == test_buf && *head == ':' && next < end && *next == ':') {
      head = &next[1];
      ++n_wildcard;
      continue;
    } else if (is_hex(*head)) {
      int is_prev_v4_decimal = head > test_buf && head[-1] == '.';
      char *num_begin = head;
      int is_all_decimal = 1;
      int seg_len = 0;
      while (head < end && is_hex(*head)) {
        is_all_decimal = is_all_decimal && isdigit(*head);
        ++head;
        ++seg_len;
      }

      int next_is_wilcard = &head[1] < end && head[0] == ':' && head[1] == ':';
      int next_is_dot = &head[1] < end && head[0] == '.';
      int next_is_col = &head[1] < end && head[0] == ':';
      int next_is_eof = head == end;

      if (!(next_is_wilcard || next_is_dot || next_is_col || next_is_eof)) {
        return 0;
      }

      int is_v4_decimal = (next_is_eof && is_prev_v4_decimal) || next_is_dot;
      int is_v6_octets = (next_is_eof && !is_prev_v4_decimal) || next_is_col ||
                         next_is_wilcard;
      if (is_v4_decimal) {
        if (!is_all_decimal) {
          return 0;
        }
        if (seg_len > 1 && *num_begin == '0') {
          return 0;
        }
        long val = strtol(num_begin, NULL, 10);
        if (val < 0 || val > 255) {
          return 0;
        }
        ++n_decs;
        ++head;

      } else if (is_v6_octets) {
        if (seg_len > 4) {
          return 0;
        }
        ++n_words;
        ++head;
        if (head < end && *head == ':') {
          ++head;
          ++n_wildcard;
        }
        continue;
      } else {
        return 0;
      }
    } else {
      return 0;
    }
  }

  // According to RFC4291 section 2.2
  if (n_wildcard == 1) {
    // Clause 2, the "compressed" form.
    if (n_decs == 4) {
      // Clause 3, IPv6-in-IPv4.
      return n_words <= 5;
    } else if (n_decs == 0) {
      // Compressed, but no IPv4 nested in.
      return n_words <= 7;
    } else {
      // Invalid.
      return 0;
    }
  } else if (n_wildcard == 0) {
    if (n_decs == 0) {
      // Clause 1, no-compressed, no-ipv4. exactly 8 hex groups.
      return n_words == 8;
    } else if (n_decs == 4) {
      // Clause 3, IPv4-in-IPv6, no-compressed. exactly 6 hex groups and 4
      // decimals.
      return n_words == 6;
    } else {
      // Invalid.
      return 0;
    }
  } else {
    // Invalid.
    return 0;
  }
}

int is_username_valid(char *base, int len) {
  char *end = &base[len];
  while (base < end) {
    if (!isalnum(*base++)) {
      return 0;
    }
  }
  return 1;
}

int is_port_str_valid(char *base, int len) {
  if (len <= 0 || len > 5) {
    return 0;
  }
  char test_buf[6];
  memcpy(test_buf, base, len);
  test_buf[len] = 0;
  char *start = test_buf;
  char *end = &test_buf[len];
  if (isdigit(*start) && *start != '0') {
    if (&start[1] < end && start[1] == 'x') {
      return 0;
    }
    char *endptr;
    long val = strtol(start, &endptr, 10);
    if (*endptr != 0 || endptr == start) {
      return 0;
    }
    if (val < 0 || val > 65535) {
      return 0;
    }
  }

  return 0;
}