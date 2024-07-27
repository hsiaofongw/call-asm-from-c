#include "pkt.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "blob.h"
#include "err.h"
#include "ringbuf.h"

struct pkt_impl {
  // PktType
  int type;

  int sender_length;
  char *sender;

  int receiver_length;
  char *receiver;

  struct blob_t *body;
  struct alloc_t *mem;
};

char pkt_magic_words[] = {0x1, 0x2, 0x3, 0x4, 0x1, 0x2, 0x3, 0x4};

// align x to multiples of 2^m
int align_to(int x, int m) {
  int mask = (((int)1) << m);
  int rem = x & (mask - 1);
  int result = x - rem;
  return rem == 0 ? x - rem : x - rem + mask;
}

#define DEFAULT_ALIGN_M 4
int align_default(int x) { return align_to(x, DEFAULT_ALIGN_M); }

#define PAGE_SIZE_M 12
#define PAGE_SIZE (((int)0x1) << PAGE_SIZE_M)
int align_page(int x) { return align_to(x, PAGE_SIZE_M); }

int pkt_create(struct pkt_impl **result, int type, struct alloc_t *allocator) {
  struct alloc_t *used_allocator = allocator;
  if (used_allocator == NULL) {
    used_allocator = &default_alloc;
  }

  if (type != PktTyMsg) {
    return ErrNonSupportedMsgType;
  }

  struct pkt_impl *pkt =
      used_allocator->alloc(sizeof(struct pkt_impl), used_allocator->closure);

  pkt->mem = used_allocator;
  pkt->sender_length = 0;
  pkt->receiver_length = 0;
  pkt->body = blob_create(PAGE_SIZE, used_allocator);
  if (!pkt->body || pkt->body == NULL) {
    return ErrAllocaFailed;
  }
  pkt->receiver = NULL;
  pkt->sender = NULL;
  *result = pkt;

  return 0;
}

void pkt_free(struct pkt_impl **p) {
  struct pkt_impl *pkt = *p;
  void (*used_deleter)(void *p, void *closure) = pkt->mem->deleter;
  void *deleter_closure = pkt->mem->closure;

  if (pkt->sender) {
    used_deleter(pkt->sender, deleter_closure);
  }

  if (pkt->receiver) {
    used_deleter(pkt->receiver, deleter_closure);
  }

  if (pkt->body) {
    blob_free(pkt->body);
  }

  used_deleter(pkt, deleter_closure);
  *p = NULL;
}

int pkt_get_type(struct pkt_impl *pkt) { return pkt->type; }

int pkt_set_sender(struct pkt_impl *p, char *buf, int length) {
  if (p->sender) {
    p->mem->deleter(p->sender, p->mem->closure);
  }

  p->sender = p->mem->alloc(length, p->mem->closure);
  if (!p->sender) {
    return ErrAllocaFailed;
  }

  p->sender_length = length;
  memcpy(p->sender, buf, length);
  return 0;
}

int pkt_set_receiver(struct pkt_impl *p, char *buf, int length) {
  if (p->receiver) {
    p->mem->deleter(p->receiver, p->mem->closure);
  }

  p->receiver = p->mem->alloc(length, p->mem->closure);
  if (!p->receiver) {
    return ErrAllocaFailed;
  }

  p->receiver_length = length;
  memcpy(p->receiver, buf, length);
  return 0;
}

int pkt_header_set_value(struct pkt_impl *p, int key_idx, char *buf,
                         int length) {
  if (length > MAX_HEADER_VALUE_SIZE) {
    return ErrSizeTooLarge;
  }

  switch (key_idx) {
    case PktFieldSender:
      return pkt_set_sender(p, buf, length);
    case PktFieldReceiver:
      return pkt_set_receiver(p, buf, length);
    default:
      return ErrNonSupportedField;
  }

  return 0;
}

int pkt_header_get_value(struct pkt_impl *p, int key_idx, char *buf, int length,
                         int *size) {
  if (length < MAX_HEADER_VALUE_SIZE) {
    return ErrTooSmallBuffer;
  }

  switch (key_idx) {
    case PktFieldSender:
      memcpy(buf, p->sender, p->sender_length);
      *size = p->sender_length;
      break;
    case PktFieldReceiver:
      memcpy(buf, p->receiver, p->receiver_length);
      *size = p->receiver_length;
      break;
    case PktFieldContentLength:
      int *size_out = (void *)buf;
      *size_out = p->body->size;
      *size = sizeof(p->body->size);
      break;
    default:
      return ErrNonSupportedField;
  }

  return 0;
}

int pkt_body_send_chunk(struct pkt_impl *p, char *buf, int length) {
  if (p->body->size + length > MAX_BODY_SIZE) {
    return ErrBodyTooLarge;
  }

  return blob_send_chunk(p->body, buf, length);
}

int pkt_body_receive_chunk(struct pkt_impl *p, char *buf, int length,
                           int *chunk_size, int offset) {
  return blob_receive_chunk(buf, length, chunk_size, p->body, offset);
}

struct serialize_ctx_impl {
  struct blob_t *buf;
  struct alloc_t *mem;
};

struct serialize_ctx_impl *serialize_ctx_create(struct alloc_t *allocator) {
  struct serialize_ctx_impl *c =
      allocator->alloc(sizeof(struct serialize_ctx_impl), allocator->closure);
  c->buf = blob_create(PAGE_SIZE, allocator);
  c->mem = allocator;
  return c;
}

void serialize_ctx_free(struct serialize_ctx_impl *s_ctx) {
  void (*deleter)(void *addr, void *closure) = s_ctx->mem->deleter;
  void *closure = s_ctx->mem->closure;
  blob_free(s_ctx->buf);
  deleter(s_ctx, closure);
}

int serialze_ctx_send_pkt(struct serialize_ctx_impl *s_ctx, pkt *p) {
  int status;
  char *temp;

  // magic words
  blob_send_chunk(s_ctx->buf, pkt_magic_words, sizeof(pkt_magic_words));

  // type
  int type = htonl(pkt_get_type(p));
  blob_send_chunk(s_ctx->buf, &(type), sizeof(type));

  // sender
  status = blob_pre_allocate_buffer(s_ctx->buf, MAX_HEADER_VALUE_SIZE, &temp);
  if (status != 0) {
    return status;
  }
  int sender_size;
  pkt_header_get_value(p, PktFieldSender, temp, MAX_HEADER_VALUE_SIZE,
                       &sender_size);
  blob_deem_buf_written(s_ctx->buf, sender_size);

  // receiver
  status = blob_pre_allocate_buffer(s_ctx->buf, MAX_HEADER_VALUE_SIZE, &temp);
  if (status != 0) {
    return status;
  }
  int receiver_size;
  pkt_header_get_value(p, PktFieldReceiver, temp, MAX_HEADER_VALUE_SIZE,
                       &receiver_size);
  blob_deem_buf_written(s_ctx->buf, receiver_size);

  // content-length
  int *content_length_ptr;
  status = blob_pre_allocate_buffer(s_ctx->buf, MAX_HEADER_VALUE_SIZE,
                                    (void *)&content_length_ptr);
  if (status != 0) {
    return status;
  }
  int sizeof_contentlength_field;
  pkt_header_get_value(p, PktFieldContentLength, (void *)content_length_ptr,
                       MAX_HEADER_VALUE_SIZE, &sizeof_contentlength_field);
  blob_deem_buf_written(s_ctx->buf, sizeof_contentlength_field);

  status = blob_pre_allocate_buffer(s_ctx->buf, *content_length_ptr, &temp);
  if (status != 0) {
    return status;
  }

  int offset = 0;
  int remain_cap = *content_length_ptr;
  while (1) {
    int chunk_size = 0;
    status = pkt_body_receive_chunk(p, temp, remain_cap, &chunk_size, offset);
    if (chunk_size > 0) {
      blob_deem_buf_written(s_ctx->buf, chunk_size);
    }
    if (status == 0) {
      // eof
      break;
    }
    if (status < 0) {
      // error
      return status;
    }
    offset += chunk_size;
    remain_cap -= chunk_size;
  }
}

enum PktParseState {
  EXPECT_MAGICWORDS,
  EXPECT_TYPE,
  EXPECT_SENDER_LENGTH,
  EXPECT_SENDER,
  EXPECT_RECEIVER_LENGTH,
  EXPECT_RECEIVER,
  EXPECT_CONTENT_LENGTH,
  EXPECT_BODY
};

struct parse_ctx_impl {
  struct alloc_t *mem;
  enum PktParseState state;
  ringbuf *buf;
  pkt *p;
  int parsed;
  int sender_len;
  int receiver_len;
  int content_len;
};

int parse_ctx_send_chunk(struct parse_ctx_impl *p_ctx, char *buf,
                         const int buf_size, int *size_accepted,
                         int *need_more) {
  *size_accepted = 0;

  if (p_ctx->parsed) {
    return ErrExtractParsedPacketFirst;
  }

  if (buf_size + ringbuf_get_size(p_ctx->buf) == 0) {
    return ErrNoDataToParse;
  }

  char *end = &buf[buf_size];
  char header_buf[MAX_HEADER_VALUE_SIZE];

  while (1) {
    int remain_cap = ringbuf_get_remaining_capacity(p_ctx->buf);
    int remain_buf = end - buf;
    if (remain_cap > 0 && remain_buf > 0) {
      int ingest_size = remain_buf < remain_cap ? remain_buf : remain_cap;
      ringbuf_send_chunk(p_ctx->buf, buf, ingest_size);
      buf = &buf[ingest_size];
      *size_accepted += ingest_size;
    }

    if (ringbuf_is_empty(p_ctx->buf)) {
      break;
    }

    struct pkt_impl *dummy_pkt = NULL;

    switch (p_ctx->state) {
      case EXPECT_MAGICWORDS:
        const int size_of_magicwords = sizeof(pkt_magic_words);
        const int intern_buf_size = ringbuf_get_size(p_ctx->buf);
        *need_more = size_of_magicwords - intern_buf_size;
        if (*need_more > 0) {
          return ErrNeedMore;
        }

        ringbuf_receive_chunk(header_buf, size_of_magicwords, p_ctx->buf);
        if (memcmp(pkt_magic_words, header_buf, size_of_magicwords) == 0) {
          p_ctx->state = EXPECT_TYPE;
          continue;
        }
        return ErrMagicWordsMisMatch;
      case EXPECT_TYPE:
        const int size_of_type = sizeof(dummy_pkt->type);
        const int intern_buf_size = ringbuf_get_size(p_ctx->buf);
        *need_more = size_of_type - intern_buf_size;
        if (*need_more > 0) {
          return ErrNeedMore;
        }

        ringbuf_receive_chunk(header_buf, size_of_type, p_ctx->buf);
        int *received_type = (void *)header_buf;
        *received_type = ntohl(*received_type);

        int valid_type = PktTyMsg;
        if (*received_type != valid_type) {
          return ErrNonSupportedMsgType;
        }
        pkt_set_type(p_ctx->p, *received_type);
        p_ctx->state = EXPECT_SENDER_LENGTH;
        continue;
      case EXPECT_SENDER_LENGTH:
        *need_more =
            sizeof(dummy_pkt->sender_length) - ringbuf_get_size(p_ctx->buf);
        if (*need_more > 0) {
          return ErrNeedMore;
        }

        ringbuf_receive_chunk(header_buf, sizeof(dummy_pkt->sender_length),
                              p_ctx->buf);
        int *sender_len = (void *)header_buf;
        *sender_len = ntohl(*sender_len);
        if (*sender_len > MAX_HEADER_VALUE_SIZE) {
          return ErrInvalidHeaderValue;
        }
        p_ctx->sender_len = *sender_len;
        p_ctx->state = EXPECT_SENDER;
        continue;
      case EXPECT_SENDER:
        *need_more = p_ctx->sender_len - ringbuf_get_size(p_ctx->buf);
        if (*need_more > 0) {
          return ErrNeedMore;
        }

        ringbuf_receive_chunk(header_buf, p_ctx->sender_len, p_ctx->buf);
        int status = pkt_header_set_value(p_ctx->p, PktFieldSender, header_buf,
                                          p_ctx->sender_len);
        if (status != 0) {
          return status;
        }
        p_ctx->state = EXPECT_RECEIVER_LENGTH;
        continue;
      case EXPECT_RECEIVER_LENGTH:
        *need_more =
            sizeof(dummy_pkt->receiver_length) - ringbuf_get_size(p_ctx->buf);
        if (*need_more > 0) {
          return ErrNeedMore;
        }

        ringbuf_receive_chunk(header_buf, sizeof(dummy_pkt->receiver_length),
                              p_ctx->buf);
        int receiver_len = ntohl(*((int *)header_buf));
        if (receiver_len > MAX_HEADER_VALUE_SIZE) {
          return ErrInvalidHeaderValue;
        }
        p_ctx->receiver_len = receiver_len;
        p_ctx->state = EXPECT_SENDER;
        continue;
    }
  }
}