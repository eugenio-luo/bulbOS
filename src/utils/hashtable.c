#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/hashtable.h>
#include <kernel/vmm.h>

#define HASH_OFFSET   14695981039346656037UL
#define HASH_PRIME    1099511628211UL

static size_t prime_numbers[] = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43,
        47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103,
        107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163,
        167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227,
        229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
        283, 293, 307, 311,
};
static size_t pnum_size = sizeof(prime_numbers) / sizeof(uint32_t);

size_t
ht_find_prime(size_t n)
{
        for (size_t i = 0; i < pnum_size; ++i)
                if (n < prime_numbers[i])
                        return prime_numbers[i];

        printf("[HASH] hash table size become too big\n");
        abort();
        return -1;
}

hash_table_t*
ht_create(size_t max_size, uint32_t max_load, int flag)
{
        hash_table_t *table = kmalloc(sizeof(hash_table_t));
        table->size = 0;
        table->capacity = ht_find_prime(max_size * 100 / max_load);
        table->max_load = max_load;
        table->flag = flag;
        
        table->entries = kmalloc(sizeof(hash_entry_t) * table->capacity);
        for (size_t i = 0; i < table->capacity; ++i) 
                table->entries[i] = (hash_entry_t) {
                        .key64 = 0,
                        .value = 0,
                        .state = HT_EMPTY,
                };
        
        return table;
}

void
ht_free(hash_table_t *table)
{
        kfree(table->entries);
        kfree(table);
}        

static uint64_t
ht_key64(uint64_t key)
{
        uint64_t hash = HASH_OFFSET;

        for (size_t i = 0; i < sizeof(uint64_t); ++i) {
                hash ^= key;
                hash *= HASH_PRIME;
        }

        return hash;
}

static uint64_t
ht_key32(char *key)
{
        uint64_t hash = HASH_OFFSET;

        for (; *key; ++key) {
                hash ^= *key;
                hash *= HASH_PRIME;
        }

        return hash;
}

static uint64_t
ht_key(hash_table_t *table, hash_key_t key)
{
        return (table->flag & HT_PTRKEY) ? ht_key32(key.key32) : ht_key64(key.key64); 
}

static int
ht_compare(hash_table_t *table, hash_entry_t *entry, hash_key_t key)
{
        if (table->flag & HT_PTRKEY) 
                return !strcmp(entry->key32, key.key32);
        else
                return entry->key64 == key.key64;
}

static void
ht_assign(hash_table_t *table, hash_entry_t *entry, hash_key_t key)
{
        if (table->flag & HT_PTRKEY)
                entry->key32 = key.key32;
        else
                entry->key64 = key.key64;
}

void*
ht_get(hash_table_t *table, hash_key_t key)
{
        size_t index = ht_key(table, key) % table->capacity;
        hash_entry_t *entry = &table->entries[index];

        while (entry->state != HT_EMPTY) {
                if (entry->state == HT_VALID && ht_compare(table, entry, key))
                        return entry->value;

                entry = &table->entries[++index % table->capacity];
        }

        return NULL;
}

void
ht_resize(hash_table_t *table)
{
        size_t old_cap = table->capacity;
        hash_entry_t *old_entries = table->entries;

        table->size = 0;
        table->capacity = ht_find_prime(table->capacity * 2);
        table->entries = kmalloc(sizeof(hash_entry_t) * table->capacity);

        for (size_t i = 0; i < old_cap; ++i)
                if (old_entries[i].state == HT_VALID) {
                        hash_key_t key = {old_entries[i].key64};
                        ht_set(table, key, old_entries[i].value);
                }
                        
        kfree(old_entries);
}

int
ht_set(hash_table_t *table, hash_key_t key, void *value)
{
        if (table->size * 100 > table->capacity * table->max_load) {
                if (table->flag & HT_RESIZE)
                        ht_resize(table);
                else
                        return -1;
        }

        size_t index = ht_key(table, key) % table->capacity;
        hash_entry_t *entry = &table->entries[index];
        
        while (entry->state == HT_VALID) {
                if (ht_compare(table, entry, key)) {
                        entry->value = value;
                        return 0;
                }
                        
                entry = &table->entries[++index % table->capacity];
        }

        ht_assign(table, entry, key);
        entry->value = value;

        if (entry->state == HT_EMPTY)
                ++table->size;
        
        entry->state = HT_VALID;
        return 0;
}

int
ht_remove(hash_table_t *table, hash_key_t key)
{
        uint64_t hash = ht_key(table, key);
        size_t index = hash % table->capacity;
        hash_entry_t *entry = &table->entries[index];

        while (entry->state != HT_EMPTY) {
                if (entry->state != HT_VALID || !ht_compare(table, entry, key)) {
                        entry = &table->entries[++index % table->capacity];
                        continue;
                }

                ht_assign(table, entry, (hash_key_t){0});

                hash_entry_t *next = &table->entries[++index % table->capacity];
                if (next->state == HT_EMPTY) {
                        --table->size;
                        entry->state = HT_EMPTY;
                } else {
                        entry->state = HT_TOMBSTONE;
                }

                return 0;
        }
        
        printf("[HT] removing invalid key\n");
        return -1;
}
