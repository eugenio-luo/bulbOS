#ifndef _UTILS_HASHTABLE_H
#define _UTILS_HASHTABLE_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
        HT_EMPTY = 0,
        HT_VALID,
        HT_TOMBSTONE,
} hash_state_t;

typedef enum {
        HT_RESIZE = (1 << 0),
        HT_PTRKEY = (1 << 1),
} hash_flag_t;

typedef union {
        uint64_t key64;
        char *key32;
} hash_key_t;

typedef struct {
        union {
                uint64_t key64;
                char *key32;
        };
        void *value;
        int state;
} hash_entry_t;

typedef struct {
        hash_entry_t *entries;
        size_t capacity;
        size_t size;
        uint32_t max_load; /* in percentage */
        int flag;
} hash_table_t;

hash_table_t* ht_create(size_t max_size, uint32_t max_load, int flag);
void ht_free(hash_table_t *table);
int ht_set(hash_table_t *table, hash_key_t key, void *value);
void* ht_get(hash_table_t *table, hash_key_t key);
int ht_remove(hash_table_t *table, hash_key_t key);

#endif
