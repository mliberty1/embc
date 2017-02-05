/* Copyright 2016 Jetperch LLC */

#include "hashmap.h"
#include "common/ec.h"
#include "common/dbc.h"
#include <stdlib.h>

struct entry_s {
    void * key;
    size_t hash;
    void * value;
    struct entry_s * next;
};

struct hashmap_s {
    hashmap_hash hash;
    hashmap_equiv equiv;
    /**
     * @brief The allocated hash table.
     *
     * The size must be a power of two.
     */
    struct entry_s ** hashtable;

    /**
     * @brief The mask to apply to a hash before indexing into hashtable.
     */
    size_t hashtable_mask;
    size_t length;
};

struct hashmap_iterator_s {
    struct hashmap_s * hashmap;
    struct entry_s ** previous;
    struct entry_s * next;
};

struct hashmap_s * hashmap_new(hashmap_hash hash, hashmap_equiv equiv) {
    if (!hash || !equiv) {
        return 0;
    }
    struct hashmap_s * hashmap = calloc(1, sizeof(struct hashmap_s));
    if (!hashmap) {
        return 0;
    }
    hashmap->hash = hash;
    hashmap->equiv = equiv;
    hashmap->hashtable_mask = 0x7;
    hashmap->hashtable = calloc(hashmap->hashtable_mask + 1, sizeof(struct entry_s *));
    return hashmap;
}

void hashmap_free(struct hashmap_s * self) {
    if (self) {
        // free everything
        free(self);
    }
}

size_t hashmap_length(struct hashmap_s * self) {
    if (self) {
        return self->length;
    } else {
        return 0;
    }
}

int hashmap_put(struct hashmap_s * self, void * key, void * value, void ** old_value) {
    size_t hash;
    struct entry_s **previous;
    struct entry_s *item;
    DBC_ARG_NOT_NULL(self);
    hash = self->hash(key);
    previous = &self->hashtable[hash & self->hashtable_mask];
    if (*previous) {
        item = *previous;
        while (item) {
            if ((item->hash == hash) && (self->equiv(item->key, key))) {
                if (old_value) {
                    *old_value = item->value;
                }
                item->value = value;
                return 0;
            }
            previous = &(item->next);
            item = item->next;
        }
    }

    item = calloc(1, sizeof(struct entry_s));
    if (!item) {
        return JETLEX_ERROR_NOT_ENOUGH_MEMORY;
    }
    item->hash = hash;
    item->key = key;
    item->value = value;
    *previous = item;
    if (old_value) {
        *old_value = 0;
    }
    self->length += 1;
    return 0;
}

int hashmap_get(struct hashmap_s * self, void * key, void ** value) {
    size_t hash;
    struct entry_s *item;
    DBC_ARG_NOT_NULL(self);
    hash = self->hash(key);
    item = self->hashtable[hash & self->hashtable_mask];
    while (item) {
        if ((item->hash == hash) && (self->equiv(item->key, key))) {
            if (value) {
                *value = item->value;
            }
            return 0;
        }
        item = item->next;
    }
    if (value) {
        *value = 0;
    }
    return JETLEX_ERROR_NOT_FOUND;
}

#if 0
int hashmap_remove(struct hashmap_s * self, void * key);
size_t hashmap_length(struct hashmap_s * self);

struct hashmap_iterator_s * hashmap_iterator_new(struct hashmap_s * self);
int hashmap_iterator_next(struct hashmap_iterator_s * self, void ** key, void ** value);
void hashmap_iterator_free(struct hashmap_iterator_s * self);
#endif
