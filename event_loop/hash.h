#include "alloc.h"

#ifndef MY_HASH
#define MY_HASH

#define HASHTAB_INIT_CAPACITY (((int)1) << 16)

struct hashtab_impl;
typedef struct hashtab_impl *hashtab;

hashtab *hashtab_create(int (*hash_func)(char *buf, int size),
                        struct alloc_t *allocator);

void hashtab_free(hashtab *);

// returns NULL if the entry dosen't exist.
void *hashtab_get(hashtab *, char *key_buf, int key_size);

// returns 1 if collided.
int hashtab_set(hashtab *, char *key_buf, int key_size, void *data);

#endif