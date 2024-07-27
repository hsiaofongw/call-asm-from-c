#include "hash.h"

#include <string.h>

struct ht_entry;
struct ht_entry {
  char *key_buf;
  int key_size;
  void *value;
  struct ht_entry *next;
  struct alloc_t *mem;
};

struct hashtab_impl {
  int (*hash_func)(char *buf, int size);
  struct alloc_t *mem;
  struct ht_entry **entries;
  int capacity;
};

void hashtab_upscale(struct hashtab_impl *htab, int desired_capacity,
                     int init) {
  if (desired_capacity < htab->capacity) {
    return;
  }

  struct ht_entry **new_entries_table = htab->mem->alloc(
      sizeof(struct ht_entry *) * (desired_capacity), htab->mem->closure);
  for (int i = 0; i < htab->capacity; ++i) {
    new_entries_table[i] = htab->entries[i];
  }
  for (int i = htab->capacity; i < desired_capacity; ++i) {
    new_entries_table[i] = NULL;
  }
  if (!init) {
    htab->mem->deleter(htab->entries, htab->mem->closure);
  }
  htab->entries = new_entries_table;
  htab->capacity = desired_capacity;
}

hashtab *hashtab_create(int (*hash_func)(char *buf, int size),
                        struct alloc_t *allocator) {
  struct hashtab_impl *htab =
      allocator->alloc(sizeof(struct hashtab_impl), allocator->closure);
  htab->hash_func = hash_func;
  htab->mem = allocator;
  htab->capacity = 0;
  htab->entries = NULL;
  hashtab_upscale(htab, HASHTAB_INIT_CAPACITY, 1);
  return htab;
}

struct ht_entry *hashtab_get_entry(struct hashtab_impl *htab, char *key_buf,
                                   int key_size) {
  int idx = htab->hash_func(key_buf, key_size);
  if (idx > htab->capacity || idx < 0) {
    return NULL;
  }

  struct ht_entry *entry_first = htab->entries[idx];
  if (entry_first == NULL) {
    return NULL;
  }

  if (entry_first->next == NULL) {
    return entry_first;
  }

  while (entry_first != NULL) {
    if (entry_first->key_size == key_size) {
      if (strcmp(entry_first->key_buf, key_buf) == 0) {
        return entry_first;
      }
    }
    entry_first = entry_first->next;
  }
  return NULL;
}

void *hashtab_get(struct hashtab_impl *htab, char *key_buf, int key_size) {
  struct ht_entry *entry = hashtab_get_entry(htab, key_buf, key_size);
  if (entry == NULL) {
    return NULL;
  }
  return entry->value;
}

struct ht_entry *ht_entry_create(char *key_buf, int key_size, void *data,
                                 struct alloc_t *allocator) {
  struct ht_entry *hte =
      allocator->alloc(sizeof(struct ht_entry), allocator->closure);
  hte->key_buf = allocator->alloc(sizeof(key_size), allocator->closure);
  memcpy(hte->key_buf, key_buf, key_size);
  hte->key_size = key_size;
  hte->next = NULL;
  hte->value = data;
  hte->mem = allocator;

  return hte;
}

void ht_entry_free(struct ht_entry *hte) {
  struct alloc_t *allocator = hte->mem;
  allocator->deleter(hte->key_buf, allocator->closure);
  allocator->deleter(hte, allocator->closure);
}

void ht_entry_list_free(struct ht_entry *hte) {
  while (hte != NULL) {
    struct ht_entry *next = hte->next;
    ht_entry_free(hte);
    hte = next;
  }
}

int hashtab_set(struct hashtab_impl *htab, char *key_buf, int key_size,
                void *data) {
  int idx = htab->hash_func(key_buf, key_size);
  int required_cap = idx + 1;
  if (required_cap < htab->capacity) {
    hashtab_upscale(htab, required_cap, 0);
  }
  struct ht_entry *entry = htab->entries[idx];
  htab->entries[idx] = ht_entry_create(key_buf, key_size, data, htab->mem);
  htab->entries[idx]->next = entry;

  return entry == NULL ? 0 : 1;
}

void hashtab_free(struct hashtab_impl *htab) {
  for (int i = 0; i < htab->capacity; ++i) {
    if (htab->entries[i] == NULL) {
      continue;
    }
    ht_entry_list_free(htab->entries[i]);
  }
  struct alloc_t *allocator = htab->mem;
  allocator->deleter(htab, allocator->closure);
}

int hash_func_int_identity(char *buf, int size) { return *((int *)buf); }