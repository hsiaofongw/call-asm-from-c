#include "pkt.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

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

int pkt_create(struct pkt_impl **result, int type, struct alloc_t *allocator) {
  struct alloc_t *used_allocator = allocator;

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

int pkt_set_type(struct pkt_impl *pkt, int type) {
  if (type != PktTyMsg) {
    return ErrNonSupportedMsgType;
  }
  pkt->type = type;
  return 0;
}

char *pkt_type_2_str(int type) {
  switch ((enum PktType)type) {
    case PktTyMsg:
      return "PktTyMsg";
    default:
      return "Unknown";
  }
}

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
      ((int *)buf)[0] = p->body->size;
      size[0] = sizeof(p->body->size);
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
  int fullfilled;
  int read_offset;
};

struct serialize_ctx_impl *serialize_ctx_create(struct alloc_t *allocator) {
  struct serialize_ctx_impl *c =
      allocator->alloc(sizeof(struct serialize_ctx_impl), allocator->closure);
  c->buf = blob_create(PAGE_SIZE, allocator);
  c->mem = allocator;
  c->fullfilled = 0;
  c->read_offset = 0;
  return c;
}

void serialize_ctx_free(struct serialize_ctx_impl *s_ctx) {
  void (*deleter)(void *addr, void *closure) = s_ctx->mem->deleter;
  void *closure = s_ctx->mem->closure;
  blob_free(s_ctx->buf);
  deleter(s_ctx, closure);
}

int serialize_ctx_send_pkt(struct serialize_ctx_impl *s_ctx, pkt *p) {
  if (s_ctx->fullfilled) {
    return ErrInternalBufferFullFilled;
  }

  int status;
  char *temp;

  // magic words
  blob_send_chunk(s_ctx->buf, pkt_magic_words, sizeof(pkt_magic_words));

  // type
  int type = htonl(pkt_get_type(p));
  blob_send_chunk(s_ctx->buf, (char *)&type, sizeof(type));

  // senderlen, sender, receiverlen, receiver.
  int sender_size, receiver_size;
  char sender[MAX_HEADER_VALUE_SIZE];
  char receiver[MAX_HEADER_VALUE_SIZE];
  pkt_header_get_value(p, PktFieldSender, sender, sizeof(sender), &sender_size);
  pkt_header_get_value(p, PktFieldReceiver, receiver, sizeof(receiver),
                       &receiver_size);

  int sender_size_nw_byteorder = htonl(sender_size);
  int receiver_size_nw_byteorder = htonl(receiver_size);

  int size_of_name_length = sizeof(p->sender_length);
  status = blob_pre_allocate_buffer(
      s_ctx->buf, 2 * size_of_name_length + sender_size + receiver_size, &temp);
  if (status != 0) {
    return status;
  }

  memcpy(temp, &sender_size_nw_byteorder, size_of_name_length);
  memcpy(&temp[size_of_name_length], sender, sender_size);
  memcpy(&temp[size_of_name_length + sender_size], &receiver_size_nw_byteorder,
         size_of_name_length);
  memcpy(&temp[size_of_name_length + sender_size + size_of_name_length],
         receiver, receiver_size);

  status = blob_deem_buf_written(
      s_ctx->buf, 2 * size_of_name_length + sender_size + receiver_size);
  if (status != 0) {
    return status;
  }

  // content-length
  int *content_length_ptr;
  status = blob_pre_allocate_buffer(s_ctx->buf, MAX_HEADER_VALUE_SIZE,
                                    (void *)&content_length_ptr);
  if (status != 0) {
    return status;
  }
  int content_length;
  int sizeof_contentlength_field;
  status =
      pkt_header_get_value(p, PktFieldContentLength, (char *)&content_length,
                           MAX_HEADER_VALUE_SIZE, &sizeof_contentlength_field);
  if (status != 0) {
    return status;
  }
  *content_length_ptr = htonl(content_length);
  blob_deem_buf_written(s_ctx->buf, sizeof_contentlength_field);

  status = blob_pre_allocate_buffer(s_ctx->buf, content_length, &temp);
  if (status != 0) {
    return status;
  }

  int offset = 0;
  int remain_cap = content_length;
  while (1) {
    int chunk_size;
    status = pkt_body_receive_chunk(p, temp, remain_cap, &chunk_size, offset);
    if (status != 0) {
      return status;
    }

    if (chunk_size > 0) {
      blob_deem_buf_written(s_ctx->buf, chunk_size);
    }
    if (chunk_size == 0) {
      // eof
      break;
    }
    offset += chunk_size;
    remain_cap -= chunk_size;
  }

  s_ctx->fullfilled = 1;

  return 0;
}

int serialize_ctx_is_fullfilled(struct serialize_ctx_impl *s_ctx) {
  return s_ctx->fullfilled;
}

int serialize_ctx_receive_chunk(char *dst, int max_len, int *chunk_size,
                                struct serialize_ctx_impl *src) {
  if (!src->fullfilled) {
    return ErrNotReadyToExtract;
  }
  int status =
      blob_receive_chunk(dst, max_len, chunk_size, src->buf, src->read_offset);
  if (status != 0) {
    return status;
  }

  src->read_offset += *chunk_size;
  if (*chunk_size == 0) {
    src->fullfilled = 0;
    src->read_offset = 0;
    blob_clear(src->buf);
  }
  return 0;
}

int serialize_ctx_is_ready_to_send_pkt(struct serialize_ctx_impl *s_ctx) {
  return !s_ctx->fullfilled && s_ctx->read_offset == 0;
}

int serialize_ctx_is_ready_to_receive_chunk(struct serialize_ctx_impl *s_ctx) {
  return s_ctx->fullfilled;
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
  int content_len_needed;
};

char *parse_ctx_get_state_str(int state) {
  switch ((enum PktParseState)state) {
    case EXPECT_MAGICWORDS:
      return "EXPECT_MAGICWORDS";
    case EXPECT_TYPE:
      return "EXPECT_TYPE";
    case EXPECT_SENDER_LENGTH:
      return "EXPECT_SENDER_LENGTH";
    case EXPECT_SENDER:
      return "EXPECT_SENDER";
    case EXPECT_RECEIVER_LENGTH:
      return "EXPECT_RECEIVER_LENGTH";
    case EXPECT_RECEIVER:
      return "EXPECT_RECEIVER";
    case EXPECT_CONTENT_LENGTH:
      return "EXPECT_CONTENT_LENGTH";
    case EXPECT_BODY:
      return "EXPECT_BODY";
    default:
      return "(UNKNOWN)";
  }
}

int parse_ctx_create(struct parse_ctx_impl **p_ctx_out,
                     struct alloc_t *allocator) {
  *p_ctx_out =
      allocator->alloc(sizeof(struct parse_ctx_impl), allocator->closure);
  if (*p_ctx_out == NULL) {
    return ErrAllocaFailed;
  }
  struct parse_ctx_impl *p_ctx = *p_ctx_out;
  p_ctx->mem = allocator;
  p_ctx->state = EXPECT_MAGICWORDS;
  p_ctx->buf = ringbuf_create(MAX_PACKET_SIZE);
  if (!p_ctx->buf) {
    return ErrAllocaFailed;
  }

  int status = pkt_create(&(p_ctx->p), PktTyMsg, allocator);
  if (status != 0) {
    return status;
  }

  p_ctx->parsed = 0;
  p_ctx->sender_len = 0;
  p_ctx->receiver_len = 0;
  p_ctx->content_len = 0;
  p_ctx->content_len_needed = 0;

  return 0;
}

void parse_ctx_free(struct parse_ctx_impl **p) {
  if (!*p) {
    return;
  }
  struct parse_ctx_impl *p_ctx = *p;

  struct alloc_t *allocator = p_ctx->mem;
  if (p_ctx->buf) {
    ringbuf_free(p_ctx->buf);
  }

  if (p_ctx->p) {
    pkt_free(&(p_ctx->p));
  }

  allocator->deleter(p_ctx, allocator->closure);
  *p = NULL;
}

int parse_ctx_send_chunk(struct parse_ctx_impl *p_ctx, char *buf,
                         const int buf_size, int *size_accepted,
                         int *need_more) {
  int status;
  *size_accepted = 0;

  if (p_ctx->parsed) {
    return ErrExtractParsedPacketFirst;
  }

  if (buf_size + ringbuf_get_size(p_ctx->buf) == 0) {
    return ErrNoDataToParse;
  }

  struct pkt_impl *dummy_pkt = NULL;
  char *end = &buf[buf_size];
  char header_buf[MAX_HEADER_VALUE_SIZE];
  char body_buf[PAGE_SIZE];
  const int size_of_magicwords = sizeof(pkt_magic_words);
  const int size_of_type = sizeof(dummy_pkt->type);

  while (1) {
    int remain_cap = ringbuf_get_remaining_capacity(p_ctx->buf);
    int remain_buf = end - buf;
    if (remain_cap > 0 && remain_buf > 0) {
      int ingest_size = remain_buf < remain_cap ? remain_buf : remain_cap;
      ringbuf_send_chunk(p_ctx->buf, buf, ingest_size);
      buf = &buf[ingest_size];
      *size_accepted += ingest_size;
    }

    switch (p_ctx->state) {
      case EXPECT_MAGICWORDS:
        *need_more = size_of_magicwords - ringbuf_get_size(p_ctx->buf);
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
        *need_more = size_of_type - ringbuf_get_size(p_ctx->buf);
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
        p_ctx->sender_len = ntohl(*((int *)header_buf));
        if (p_ctx->sender_len > MAX_HEADER_VALUE_SIZE) {
          return ErrInvalidHeaderValue;
        }
        p_ctx->state = EXPECT_SENDER;
        continue;
      case EXPECT_SENDER:
        *need_more = p_ctx->sender_len - ringbuf_get_size(p_ctx->buf);
        if (*need_more > 0) {
          return ErrNeedMore;
        }

        ringbuf_receive_chunk(header_buf, p_ctx->sender_len, p_ctx->buf);
        status = pkt_header_set_value(p_ctx->p, PktFieldSender, header_buf,
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
        p_ctx->receiver_len = ntohl(*((int *)header_buf));
        if (p_ctx->receiver_len > MAX_HEADER_VALUE_SIZE) {
          return ErrInvalidHeaderValue;
        }
        p_ctx->state = EXPECT_RECEIVER;
        continue;
      case EXPECT_RECEIVER:
        *need_more = p_ctx->receiver_len - ringbuf_get_size(p_ctx->buf);
        if (*need_more > 0) {
          return ErrNeedMore;
        }

        ringbuf_receive_chunk(header_buf, p_ctx->receiver_len, p_ctx->buf);
        status = pkt_header_set_value(p_ctx->p, PktFieldReceiver, header_buf,
                                      p_ctx->receiver_len);
        if (status != 0) {
          return status;
        }
        p_ctx->state = EXPECT_CONTENT_LENGTH;
        continue;
      case EXPECT_CONTENT_LENGTH:
        *need_more =
            sizeof(dummy_pkt->body->size) - ringbuf_get_size(p_ctx->buf);
        if (*need_more > 0) {
          return ErrNeedMore;
        }

        ringbuf_receive_chunk(header_buf, sizeof(dummy_pkt->body->size),
                              p_ctx->buf);
        p_ctx->content_len = ntohl(*(int *)header_buf);
        if (p_ctx->content_len > MAX_BODY_SIZE) {
          return ErrBodyTooLarge;
        }
        p_ctx->content_len_needed = p_ctx->content_len;
        p_ctx->state = EXPECT_BODY;
        continue;
      case EXPECT_BODY:
        *need_more = p_ctx->content_len_needed - ringbuf_get_size(p_ctx->buf);
        if (*need_more > 0) {
          int cap = ringbuf_get_capacity(p_ctx->buf);
          if (*need_more > cap) {
            *need_more = cap;
          }
          return ErrNeedMore;
        }

        int body_chunk_size = ringbuf_receive_chunk(
            body_buf, p_ctx->content_len_needed, p_ctx->buf);
        p_ctx->content_len_needed -= body_chunk_size;
        int status = pkt_body_send_chunk(p_ctx->p, body_buf, body_chunk_size);
        if (status != 0) {
          return status;
        }
        if (p_ctx->content_len_needed == 0) {
          p_ctx->state = EXPECT_MAGICWORDS;
          p_ctx->parsed = 1;
          return 0;
        }
        continue;
    }
  }

  return 0;
}

int parse_ctx_receive_pkt(struct parse_ctx_impl *p_ctx, pkt **p) {
  if (p_ctx->p && p_ctx->parsed) {
    *p = p_ctx->p;
    p_ctx->parsed = 0;
    int status = pkt_create(&(p_ctx->p), PktTyMsg, p_ctx->mem);
    if (status != 0) {
      return status;
    }
    return 0;
  }

  return ErrParsingIsIncomplete;
}

int parse_ctx_is_ready_to_send_chunk(struct parse_ctx_impl *p_ctx) {
  return !p_ctx->parsed;
}

int parse_ctx_is_ready_to_extract_packet(struct parse_ctx_impl *p_ctx) {
  return p_ctx->parsed;
}

int parse_ctx_get_state(struct parse_ctx_impl *p_ctx) { return p_ctx->state; }