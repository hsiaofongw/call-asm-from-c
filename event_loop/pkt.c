#include "pkt.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "blob.h"
#include "err.h"

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