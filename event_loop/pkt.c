#include "pkt.h"

#include <stdlib.h>
#include <string.h>

struct pkt_impl {
  // PktType
  int type;

  int sender_length;
  char *sender;

  int receiver_length;
  char *receiver;

  char *body;
  int body_length;
  int body_capacity;

  void *(*alloc)(const int, void *);
  void *alloca_closure;

  void (*deleter)(void *, void *);
  void *deleter_closure;
};

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

void *pkt_default_alloca(const int size, void *closure) { return malloc(size); }

void pkt_default_deleter(void *p, void *closure) { free(p); }

int pkt_create(struct pkt_impl **result, int type,
               void *(*alloca)(const int size, void *closure),
               void *alloca_closure, void (*deleter)(void *obj, void *payload),
               void *deleter_closure) {
  struct pkt_impl *pkt;
  void *(*used_alloc)(const int, void *) =
      alloca == NULL ? pkt_default_alloca : alloca;
  pkt = used_alloc(sizeof(struct pkt_impl), alloca_closure);

  if (type != PktTyMsg) {
    return ErrNonSupportedMsgType;
  }

  pkt->alloc = used_alloc;
  pkt->alloca_closure = alloca_closure;
  pkt->sender_length = 0;
  pkt->receiver_length = 0;
  pkt->body = NULL;
  pkt->body_length = 0;
  pkt->body_capacity = 0;
  pkt->receiver = NULL;
  pkt->sender = NULL;
  pkt->deleter = deleter;
  if (!pkt->deleter) {
    pkt->deleter = pkt_default_deleter;
  }
  pkt->deleter_closure = deleter_closure;
  *result = pkt;

  return 0;
}

void pkt_free(struct pkt_impl **p) {
  struct pkt_impl *pkt = *p;
  void (*used_deleter)(void *p, void *closure) = pkt->deleter;
  void *deleter_closure = pkt->deleter_closure;

  if (pkt->sender) {
    used_deleter(pkt->sender, deleter_closure);
  }

  if (pkt->receiver) {
    used_deleter(pkt->receiver, deleter_closure);
  }

  if (pkt->body) {
    used_deleter(pkt->body, deleter_closure);
  }

  used_deleter(pkt, deleter_closure);
  *p = NULL;
}

int pkt_set_sender(struct pkt_impl *p, char *buf, int length) {
  if (p->sender) {
    p->deleter(p->sender, p->deleter_closure);
  }

  p->sender = p->alloc(length, p->alloca_closure);
  if (!p->sender) {
    return ErrAllocaFailed;
  }

  p->sender_length = length;
  memcpy(p->sender, buf, length);
  return 0;
}

int pkt_set_receiver(struct pkt_impl *p, char *buf, int length) {
  if (p->receiver) {
    p->deleter(p->receiver, p->deleter_closure);
  }

  p->receiver = p->alloc(length, p->alloca_closure);
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
      *size_out = p->body_length;
      *size = sizeof(p->body_length);
      break;
    default:
      return ErrNonSupportedField;
  }

  return 0;
}

int blob_upscale_on_demand(char **buf, int *capacity, int size, int addend,
                           int (*align)(int),
                           void *(*alloc)(const int, void *closure),
                           void *alloc_closure,
                           void (*deleter)(void *addr, void *closure),
                           void *deleter_closure) {
  if (size + addend <= *capacity) {
    return 0;
  }

  int new_capacity = align(size + addend);
  int new_buf = alloc(new_capacity, alloc_closure);
  if (!new_buf) {
    return ErrAllocaFailed;
  }

  if (size > 0 && *buf != NULL) {
    memcpy(new_buf, *buf, size);
    deleter(*buf, deleter_closure);
  }
  *buf = new_buf;
  *capacity = new_capacity;
  return 0;
}

int pkt_body_buf_upscale_on_demand(struct pkt_impl *p, int length_addend) {
  return blob_upscale_on_demand(&(p->body), &(p->body_capacity), p->body_length,
                                length_addend, align_default, p->alloc,
                                p->alloca_closure, p->deleter,
                                p->deleter_closure);
}

int pkt_body_send_chunk(struct pkt_impl *p, char *buf, int length) {
  if (p->body_length + length > MAX_BODY_SIZE) {
    return ErrBodyTooLarge;
  }

  int status = pkt_body_buf_upscale_on_demand(p, length);
  if (status != 0) {
    return status;
  }

  memcpy(&p->body[p->body_length], buf, length);
  p->body_length += length;
  return 0;
}

int blob_receive_chunk(char *dst, int dst_len, int *chunk_size, char *src,
                       int src_size, int src_offset) {
  int remain_src_size = src_size - src_offset;
  *chunk_size = dst_len > remain_src_size ? remain_src_size : dst_len;
  if (*chunk_size <= 0) {
    *chunk_size = 0;
    return 0;
  }

  memcpy(dst, &src[src_offset], *chunk_size);
  return 0;
}

int pkt_body_receive_chunk(struct pkt_impl *p, char *buf, int length,
                           int *chunk_size, int offset) {
  return blob_receive_chunk(buf, length, chunk_size, p->body, p->body_length,
                            offset);
}

struct serialize_ctx_impl {
  char *buf;
  int size;
  int capacity;
};

struct serialize_ctx_impl *serialize_ctx_create() {
  struct serialize_ctx_impl *c = malloc(sizeof(struct serialize_ctx_impl));
  c->capacity = PAGE_SIZE;
  c->buf = malloc(c->capacity);
  c->size = 0;
  return c;
}

int serialze_ctx_send_pkt(struct serialize_ctx_impl *s_ctx, pkt *p) {
  char temp[MAX_HEADER_VALUE_SIZE];
}