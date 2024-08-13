
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

typedef struct ipstr_token_ {
  // See IPStrSegType
  int token_type;

  // buffer to store characters in this segment
  char buf[4];

  // number of characters that is already stored in the buffer
  int buflen;

} ipstr_token_t;

int is_hex(char c) {
  char hex_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a',
                       'b', 'c', 'd', 'e', 'f', 'A', 'B', 'C', 'D', 'E', 'F'};

  for (int i = 0; i < sizeof(hex_digits); ++i) {
    if (hex_digits[i] == c) {
      return 1;
    }
  }

  return 0;
}

int is_decimal(char *buf, int len) {
  for (int i = 0; i < len; ++i) {
    if (!isdigit(buf[i])) {
      return 0;
    }
  }
  return 1;
}

void try_convert_octets_to_dec(ipstr_token_t *token) {
  if (token->token_type == OCTETS && is_decimal(token->buf, token->buflen)) {
    token->token_type = DEC;
  }
}

// 对 ipv6 string 进行 tokenize，结果写入 tokens，当成功时返回 tokens
// 个数，失败时返回 0。 base 指向被 tokenize 字符串的基地址，len 表示被 tokenize
// 字符串的长度（不包括末尾的 0）。
int tokenize_ipv6_str(ipstr_token_t *tokens, int max_n_segs, char *base,
                      int len) {
  int n_segs = 0;
  const int max_bufsize = sizeof(tokens[0].buf);
  char *head = base, *end = &base[len];
  while (head < end) {
    if (n_segs >= max_n_segs) {
      return 0;
    }

    if (head + 1 < end && strncmp(head, "::", 2) == 0) {
      memcpy(tokens[n_segs].buf, "::", 2);
      tokens[n_segs].buflen = 2;
      tokens[n_segs].token_type = WILDCARD;
      ++n_segs;
      head = &head[2];
    } else if (*head == ':') {
      tokens[n_segs].buf[0] = ':';
      tokens[n_segs].buflen = 1;
      tokens[n_segs].token_type = COL;
      ++n_segs;
      ++head;
    } else if (is_hex(*head)) {
      char *buf = tokens[n_segs].buf;
      int *buflen = &(tokens[n_segs].buflen);
      *buflen = 0;
      while (head < end && is_hex(*head) && *buflen < max_bufsize) {
        buf[*buflen] = *head;
        ++(*buflen);
        ++head;
      }
      tokens[n_segs].token_type = OCTETS;
      ++n_segs;
    } else if (*head == '.') {
      if (n_segs > 0) {
        try_convert_octets_to_dec(&tokens[n_segs - 1]);
      }

      tokens[n_segs].buf[0] = '.';
      tokens[n_segs].buflen = 1;
      tokens[n_segs].token_type = DOT;
      ++n_segs;
    } else {
      return 0;
    }
  }

  if (n_segs > 0) {
    try_convert_octets_to_dec(&tokens[n_segs - 1]);
  }

  return n_segs;
}

int is_ipv6_str_valid(char *base, int len) {
  ipstr_token_t tokens[20];
  const int max_n_segs = sizeof(tokens) / sizeof(tokens[0]);
  int n_tokens = tokenize_ipv6_str(tokens, max_n_segs, base, len);
  if (n_tokens == 0) {
    return 0;
  }

  int n_octets = 0, n_cols = 0, n_wildcards = 0, n_decs = 0, n_dots = 0;
  for (int i = 0; i < n_tokens; ++i) {
    switch (tokens[i].token_type) {
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

  ipstr_token_t *head = tokens, *end = &tokens[n_tokens];
  if (head == end) {
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