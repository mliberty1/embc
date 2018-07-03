/*
 * Copyright 2014-2018 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "embc/collections/strmap.h"

// TODO Hashmap resize
// TODO Iteration


struct entry_s {
    embc_size_t hash;
    void * value;
    struct entry_s * next;
    char key[4]; // MUST be last element, allocate required length
};

struct embc_strmap_s {
    /**
     * @brief The allocated hash table.
     *
     * The size must be a power of two.
     */
    struct entry_s ** hashtable;

    /**
     * @brief The mask to apply to a hash before indexing into hashtable.
     */
    embc_size_t hashtable_mask;
    embc_size_t length;
};

struct embc_strmap_s * embc_strmap_new() {
    struct embc_strmap_s * strmap = embc_alloc_clr(sizeof(struct embc_strmap_s));
    strmap->hashtable_mask = 0x3F;
    embc_size_t sz = (strmap->hashtable_mask + 1) * sizeof(struct entry_s *);
    strmap->hashtable = embc_alloc_clr(sz);
    return strmap;
}

void embc_strmap_free(struct embc_strmap_s *self) {
    if (self) {
        embc_size_t idx;
        struct entry_s * item;
        struct entry_s * next_item;
        for (idx = 0; idx <= self->hashtable_mask; ++idx) {
            item = self->hashtable[idx];
            while (item) {
                next_item = item->next;
                embc_free(item);
                item = next_item;
            }
        }
        embc_free(self->hashtable);
        embc_free(self);
    }
}

embc_size_t embc_strmap_length(struct embc_strmap_s *self) {
    if (self) {
        return self->length;
    } else {
        return 0;
    }
}

// djb2: http://www.cse.yorku.ca/~oz/hash.html
static embc_size_t compute_hash(const char * key) {
    embc_size_t hash = 5381;
    int c;

    while (1) {
        c = *key++;
        if (0 == c) {
            break;
        }
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

static bool is_key_equiv(const char * key1, const char * key2) {
    return (0 == strcmp(key1, key2));
}

int embc_strmap_put(struct embc_strmap_s *self, const char *key, void *value, void **old_value) {
    embc_size_t hash;
    struct entry_s **previous;
    struct entry_s *item;
    hash = compute_hash(key);
    previous = &self->hashtable[hash & self->hashtable_mask];
    if (*previous) {
        item = *previous;
        while (item) {
            if ((item->hash == hash) && is_key_equiv(item->key, key)) {
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

    embc_size_t key_length = strlen(key);
    item = embc_alloc_clr(sizeof(struct entry_s) + key_length);
    item->hash = hash;
    embc_memcpy(item->key, key, key_length);
    item->value = value;
    *previous = item;
    if (old_value) {
        *old_value = 0;
    }
    self->length += 1;
    return 0;
}

int embc_strmap_get(struct embc_strmap_s * self, char const * key, void ** value) {
    embc_size_t hash;
    struct entry_s *item;
    hash = compute_hash(key);
    item = self->hashtable[hash & self->hashtable_mask];
    while (item) {
        if ((item->hash == hash) && is_key_equiv(item->key, key)) {
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
    return 1;
}

int embc_strmap_remove(struct embc_strmap_s * self, char const * key, void ** value) {
    embc_size_t hash;
    struct entry_s **previous;
    struct entry_s * item;
    hash = compute_hash(key);
    previous = &self->hashtable[hash & self->hashtable_mask];
    item = *previous;
    while (item) {
        if ((item->hash == hash) && is_key_equiv(item->key, key)) {
            *previous = item->next;
            if (value) {
                *value = item->value;
            }
            embc_free(item);
            --self->length;
            return 0;
        }
        previous = &item->next;
        item = item->next;
    }
    return 1;
}
