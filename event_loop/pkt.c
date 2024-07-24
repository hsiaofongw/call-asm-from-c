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
  int rem = x % m;
  int result = x - rem;
  return rem == 0 ? x - rem : x - rem + mask;
}

#define DEFAULT_ALIGN_M 4
int align_default(int x) { return align_to(x, DEFAULT_ALIGN_M); }

void *pkt_default_alloca(const int size, void *closure) { return malloc(size); }

void pkt_default_deleter(void *p, void *closure) { free(p); }

struct pkt_impl *pkt_create(int type,
                            void *(*alloca)(const int size, void *closure),
                            void *alloca_closure,
                            void (*deleter)(void *obj, void *payload),
                            void *deleter_closure) {
  struct pkt_impl *pkt;
  void *(*used_alloc)(const int, void *) =
      alloca == NULL ? pkt_default_alloca : alloca;
  pkt = used_alloc(sizeof(struct pkt_impl), alloca_closure);
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
  return pkt;
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

int pkt_header_get_value(struct pkt_impl *p, int key_idx, char *buf,
                         int length) {
  if (length < MAX_HEADER_VALUE_SIZE) {
    return ErrTooSmallBuffer;
  }

  switch (key_idx) {
    case PktFieldSender:
      memcpy(buf, p->sender, p->sender_length);
      break;
    case PktFieldReceiver:
      memcpy(buf, p->receiver, p->receiver_length);
      break;
    case PktFieldContentLength:
      int *size_out = (void *)buf;
      *size_out = p->body_length;
      break;
    default:
      return ErrNonSupportedField;
  }

  return 0;
}

int pkt_body_buf_upscale_on_demand(struct pkt_impl *p, int length_addend) {
  if (p->body_capacity < p->body_length + length_addend) {
    int new_capacity = align_default(p->body_length + length_addend);
    int new_body_buf = p->alloc(new_capacity, p->alloca_closure);
    if (!new_body_buf) {
      return ErrAllocaFailed;
    }
    if (p->body_length > 0 && p->body != NULL) {
      memcpy(new_body_buf, p->body, p->body_length);
      p->deleter(p->body, p->deleter_closure);
    }
    p->body = new_body_buf;
    p->body_capacity = new_capacity;
  }
  return 0;
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
  return 0;
}

int pkt_body_receive_chunk(struct pkt_impl *p, char *buf, int length,
                           int *chunk_size, int offset) {
  int remain_length = p->body_length - offset;
  *chunk_size = length > remain_length ? remain_length : length;
  if (*chunk_size == 0) {
    return 0;
  }
  char *src = (void *)(p->body);
  memcpy(buf, &src[offset], *chunk_size);
  return 0;
}