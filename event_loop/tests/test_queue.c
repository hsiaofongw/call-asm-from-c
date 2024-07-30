#include <stdio.h>

#include "../queue.h"

void print_queue(queue *q) {
  printf("capacity: %d\n", queue_get_capacity(q));
  printf("size: %d\n", queue_get_size(q));
  printf("is_fullfilled: %d\n", queue_is_fullfilled(q));
}

int main() {
  int a, b, c;
  printf("addr(a) = 0x%016lx\n", (unsigned long)&a);
  printf("addr(b) = 0x%016lx\n", (unsigned long)&b);
  printf("addr(c) = 0x%016lx\n", (unsigned long)&c);

  queue *q = queue_create(3);
  print_queue(q);

  queue_enqueue(q, (void *)&a);
  queue_enqueue(q, (void *)&b);
  print_queue(q);

  queue_enqueue(q, (void *)&c);
  print_queue(q);

  void *elem1, *elem2, *elem3;
  elem1 = queue_dequeue(q);
  printf("Deque called, elem1 = 0x%016lx\n", (unsigned long)elem1);
  print_queue(q);
  elem2 = queue_dequeue(q);
  printf("Deque called, elem2 = 0x%016lx\n", (unsigned long)elem2);
  print_queue(q);
  elem3 = queue_dequeue(q);
  printf("Deque called, elem3 = 0x%016lx\n", (unsigned long)elem3);
  print_queue(q);

  queue_enqueue(q, (void *)&a);
  printf("Enqueue called.\n");
  queue_enqueue(q, (void *)&b);
  printf("Enqueue called.\n");

  elem1 = queue_dequeue(q);
  printf("Deque called, elem1 = 0x%016lx\n", (unsigned long)elem1);
  print_queue(q);

  elem2 = queue_dequeue(q);
  printf("Deque called, elem1 = 0x%016lx\n", (unsigned long)elem2);
  print_queue(q);

  return 0;
}