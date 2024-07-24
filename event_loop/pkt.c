#include "pkt.h"

struct pkt_impl {
  // PktType
  int type;

  int sender_length;
  char *sender;

  int receiver_length;
  char *receiver;

  int body_length;
  char *body;
};
