#include <ctype.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

int ptr_diff(void *a, void *b) {
  long addr_a = (long)a;
  long addr_b = (long)b;

  long diff = addr_a - addr_b;
  if (diff < INT_MIN || diff > INT_MAX) {
    exit(1);
  }

  return (int)diff;
}

enum IPStrSegType {
  // column seperator in ipv6 literals, e.g.: ':'
  COL = 1,

  // hex segment in ipv6 literals, e.g.: 'abcd' '192'
  OCTETS,

  // decimal, such as '192', '168', '1', '101'
  DEC,

  // wildcard mark in ipv6 literals, e.g.: '::'
  WILDCARD,

  // dot in ipv4 nested in ipv6 literals, e.g.: '.'
  DOT,
};

int is_hex(char c) {
  if (isdigit(c)) {
    return 1;
  }
  char hex_chars[] = {'a', 'b', 'c', 'd', 'e', 'f',
                      'A', 'B', 'C', 'D', 'E', 'F'};
  for (int i = 0; i < sizeof(hex_chars); ++i) {
    if (hex_chars[i] == c) {
      return 1;
    }
  }
  return 0;
}

// 对 ipv6 string 进行 tokenize，结果写入 tokens，当成功时返回 tokens
// 个数，失败时返回 0。 base 指向被 tokenize 字符串的基地址，len 表示被 tokenize
// 字符串的长度（不包括末尾的 0）。
// 只接受 octets 'abcd09a', decimal '123', column ':', doublecolumn '::', dot
// '.' 这几种 token，其余的会报错。
int tokenize_ipv6_str(int *tokens, int max_n_segs, char *base, int len) {
  char *endptr;
  long parsed_val;
  char test_buf[INET6_ADDRSTRLEN];
  if (len >= INET6_ADDRSTRLEN) {
    return 0;
  }

  memcpy(test_buf, base, len);
  test_buf[len] = 0;

  int *tokens_begin = tokens;
  int *tokens_end = &tokens[max_n_segs];

  char *head = test_buf, *end = &test_buf[len];
  while (head < end) {
    if (tokens >= tokens_end) {
      return 0;
    } else if (*head == ':') {
      *tokens++ = COL;

      char *next = &head[1];
      if (next < end && *next == ':') {
        *tokens++ = WILDCARD;
        ++head;
      }

      ++head;
    } else if (*head == '.') {
      *tokens++ = DOT;
      ++head;
    } else {
      // strtol also left-trims spaces, and it treats '-', '+', '0x' prefix as
      // normal, doing so leads to false-positives (it would treat an illegal
      // octects group or a illeagle decimal as 'legal'), so in here we exclude
      // all such cases before trying to parse (or validate) octets and
      // decimals.
      if (isspace(*head) || *head == '+' || *head == '-') {
        return 0;
      }

      char *next = &head[1];
      if (next < end && *head == '0' && *next == 'x') {
        return 0;
      }

      strtol(head, &endptr, 16);
      if (endptr != head) {
        int buflen = ptr_diff(endptr, head);
        if (buflen > 4) {
          return 0;
        }

        *tokens++ = OCTETS;
        head = endptr;
        continue;
      }

      if (*head == '0') {
        // decimal shall not begins with leading '0'.
        return 0;
      }

      parsed_val = strtol(head, &endptr, 10);
      if (endptr != head) {
        if (ptr_diff(endptr, head) > 3 || parsed_val < 0 ||
            parsed_val > UCHAR_MAX) {
          return 0;
        }

        *tokens++ = DEC;
        head = endptr;
        continue;
      }
      return 0;
    }
  }

  return ptr_diff(tokens, tokens_begin);
}

int is_ipv6_str_valid(char *base, int len) {
  int tokens[20];
  const int max_n_segs = sizeof(tokens) / sizeof(tokens[0]);
  int n_tokens = tokenize_ipv6_str(tokens, max_n_segs, base, len);
  int *head = tokens, *end = &tokens[n_tokens];
  if (head >= end) {
    return 0;
  }
  int n_octets = 0, n_wildcards = 0, n_decs = 0;
  while (head < end) {
    int *next = &head[1];
    if (head == tokens && *head == WILDCARD) {
      if (next >= end) {
        return 1;
      }
      ++n_wildcards;
      if (*next == OCTETS || *head == DEC) {
        ++head;
        continue;
      }
      return 0;
    } else if (*head == OCTETS) {
      ++n_octets;
      if (next >= end) {
        return (n_decs == 0 && n_wildcards == 1 && n_octets <= 7) ||
               (n_decs == 0 && n_wildcards == 0 && n_octets == 8);
      }
      if (*next == COL) {
        head = &head[2];
        continue;
      }
      if (*next == WILDCARD) {
        head = &head[2];
        ++n_wildcards;
        if (n_wildcards > 1) {
          return 0;
        }
        continue;
      }
      return 0;
    } else if (*head == DEC) {
      if (next == end) {
        return (n_decs == 4 && n_wildcards == 1 && n_octets <= 5) ||
               (n_decs == 4 && n_wildcards == 0 && n_octets == 6);
      }
      if (*next == DOT) {
        ++n_decs;
        head = &head[2];
        continue;
      }
      return 0;
    } else {
      return 0;
    }
  }

  return 0;
}