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

  int body_length;
  char *body;

  void *(*alloc)(const int, void *);
  void *alloca_closure;
};

void *pkt_default_alloca(const int size, void *closure) { return malloc(size); }

void pkt_default_deleter(void *p, void *closure) { free(p); }

struct pkt_impl *pkt_create(int type, void *(*alloca)(const int, void *),
                            void *alloca_closure) {
  struct pkt_impl *pkt;
  void *(*used_alloc)(const int, void *) =
      alloca == NULL ? pkt_default_alloca : alloca;
  pkt = used_alloc(sizeof(struct pkt_impl), alloca_closure);
  pkt->alloc = used_alloc;
  pkt->alloca_closure = alloca_closure;
  pkt->body_length = 0;
  pkt->sender_length = 0;
  pkt->receiver_length = 0;
  pkt->body = NULL;
  pkt->receiver = NULL;
  pkt->sender = NULL;
  return pkt;
}

void pkt_free(struct pkt_impl **p, void (*deleter)(void *p, void *closure),
              void *deleter_closure) {
  void (*used_deleter)(void *p, void *closure) = deleter;
  if (used_deleter == NULL) {
    used_deleter = pkt_default_deleter;
  }

  struct pkt_impl *pkt = *p;
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
  if (!p->sender) {
    p->sender = p->alloc(length, p->alloca_closure);
    if (!p->sender) {
      return ErrAllocaFailed;
    }
  }
  p->sender_length = length;
  memcpy(p->sender, buf, length);
  return 0;
}

int pkt_set_receiver(struct pkt_impl *p, char *buf, int length) {
  if (!p->receiver) {
    p->receiver = p->alloc(length, p->alloca_closure);
    if (!p->receiver) {
      return ErrAllocaFailed;
    }
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