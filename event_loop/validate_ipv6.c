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

      parsed_val = strtol(head, &endptr, 10);
      if (endptr != head) {
        if (parsed_val < 0 || parsed_val > UCHAR_MAX) {
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
  if (n_tokens == 0) {
    return 0;
  }

  int n_octets = 0, n_cols = 0, n_wildcards = 0, n_decs = 0, n_dots = 0;
  for (int i = 0; i < n_tokens; ++i) {
    switch (tokens[i]) {
      case OCTETS:
        ++n_octets;
        break;
      case COL:
        ++n_cols;
        break;
      case WILDCARD:
        ++n_wildcards;
        break;
      case DEC:
        ++n_decs;
        break;
      case DOT:
        ++n_dots;
        break;
      default:
        return 0;
    }
  }

  if (!(n_wildcards == 0 || n_wildcards == 1)) {
    return 0;
  }

  if (!(n_dots == 0 || n_dots == 3)) {
    return 0;
  }

  if (!(n_decs == 4 || n_decs == 0)) {
    return 0;
  }

  while (head < end) {
    ipstr_token_t *next = &head[1];
    if (head->token_type == OCTETS) {
      if (next == end) {
        break;
      }

      if ((next->token_type == COL || next->token_type == WILDCARD)) {
        head = &head[2];
        continue;
      }

      return 0;
    } else if (head->token_type == DEC) {
      if (next == end) {
        break;
      }

      if (next->token_type == DOT) {
        head = &head[2];
        continue;
      }

      return 0;
    } else if (head == tokens && head->token_type == WILDCARD) {
      if (next == end) {
        return 1;
      } else if (next->token_type == OCTETS || next->token_type == DEC) {
        head = &head[1];
        continue;
      } else {
        return 0;
      }
    } else {
      return 0;
    }
  }

  if (n_wildcards == 0 && n_dots == 0) {
    return n_octets == 8;
  } else if (n_wildcards == 0 && n_dots == 3) {
    return n_octets == 6 && n_decs == 4;
  } else if (n_wildcards == 1 && n_dots == 0) {
    return n_octets <= 7;
  } else if (n_wildcards == 1 && n_dots == 3) {
    return n_octets <= 5;
  } else {
    return 0;
  }
}