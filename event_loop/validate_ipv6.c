#include <ctype.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

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

// An RFC4291 IPv6 text representation validator.
// Returns non-zero if the input string [base, base+len) is valid, otherwise
// returns 0.
int is_ipv6_str_valid(char *base, int len) {
  char test_buf[INET6_ADDRSTRLEN];
  if (len <= 0 || len > sizeof(test_buf) - 1) {
    return 0;
  }

  memcpy(test_buf, base, len);
  test_buf[len] = 0;

  char *head = test_buf, *end = &test_buf[len], *last_cursor;
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

      if (head < end && *head != '.' && *head != ':') {
        return 0;
      }

      int is_v4_decimal =
          (head == end && is_prev_v4_decimal) || (head < end && *head == '.');
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
      } else {
        if (seg_len > 4) {
          return 0;
        }
        ++n_words;
        ++head;
        if (head < end && *head == ':') {
          ++head;
          ++n_wildcard;
        }
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