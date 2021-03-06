/*
 * Copyright 2014-2017 Jetperch LLC
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

#include "embc/collections/intmap.h"
#include "embc.h"
#include "embc/inttypes.h"
#include "utlist.h"

struct entry_s {
    embc_size_t key;
    void * value;
    struct entry_s * next;
};

struct embc_intmap_s {
    /**
     * @brief The allocated hash table.
     *
     * The size must be a power of two.
     */
    struct entry_s ** bins;

    /**
     * @brief The mask to apply to a hash before indexing into hashtable.
     */
    embc_size_t hash_mask;

    /**
     * @brief The total number of items in this container.
     */
    embc_size_t length;

    struct embc_intmap_iterator_s * iterators;
};

struct embc_intmap_iterator_s {
    struct embc_intmap_s * intmap;
    embc_size_t current_bin;
    struct entry_s * next_item;
    struct embc_intmap_iterator_s * next;
    struct embc_intmap_iterator_s * prev;
};

static void itermap_iterator_next_advance(struct embc_intmap_iterator_s * self);

struct embc_intmap_s * embc_intmap_new() {
    struct embc_intmap_s * intmap = embc_alloc_clr(sizeof(struct embc_intmap_s));
    intmap->hash_mask = 0x7;
    embc_size_t sz = (intmap->hash_mask + 1) * sizeof(struct entry_s *);
    intmap->bins = embc_alloc_clr(sz);
    return intmap;
}

void embc_intmap_free(struct embc_intmap_s * self) {
    embc_size_t idx = 0;
    struct entry_s * item = 0;
    struct entry_s * next_item = 0;
    if (self) {
        for (idx = 0; idx <= self->hash_mask; ++idx) {
            item = self->bins[idx];
            while (item) {
                next_item = item->next;
                embc_free(item);
                item = next_item;
            }
        }
        embc_free(self->bins);
        embc_free(self);
    }
}

embc_size_t embc_intmap_length(struct embc_intmap_s * self) {
    return self ? self->length : 0;
}

static inline embc_size_t compute_bin(struct embc_intmap_s * self, embc_size_t x) {
    return (x & self->hash_mask);
}

static void resize(struct embc_intmap_s * self) {
    struct entry_s * item;
    struct entry_s **previous;
    struct entry_s * next;
    struct entry_s ** bins_old = self->bins;
    embc_size_t idx = 0;
    embc_size_t mask_old = self->hash_mask;
    embc_size_t length = (self->hash_mask + 1) << 2;
    self->bins = embc_alloc_clr(length * sizeof(struct entry_s *));
    self->hash_mask = length - 1;
    EMBC_LOGD("intmap.resize -> %d", (int) length);
    for (idx = 0; idx <= mask_old; ++idx) {
        next = bins_old[idx];
        while (next) {
            item = next;
            next = item->next;
            item->next = 0;
            previous = &self->bins[compute_bin(self, item->key)];
            while (*previous) {
                *previous = (*previous)->next;
            }
            *previous = item;
        }
    }
}

int embc_intmap_put(struct embc_intmap_s * self, embc_size_t key, void * value, void ** old_value) {
    struct entry_s **previous;
    struct entry_s *item;
    EMBC_DBC_NOT_NULL(self);
    if ((!self->iterators) && (self->length >= (2 * self->hash_mask))) {
        resize(self);
    }

    previous = &self->bins[compute_bin(self, key)];
    if (*previous) {
        item = *previous;
        while (item) {
            if (item->key == key) {
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

    item = embc_alloc_clr(sizeof(struct entry_s));
    item->key = key;
    item->value = value;
    *previous = item;
    if (old_value) {
        *old_value = 0;
    }
    self->length += 1;
    return 0;
}

int embc_intmap_get(struct embc_intmap_s * self, embc_size_t key, void ** value) {
    struct entry_s *item;
    EMBC_DBC_NOT_NULL(self);
    item = self->bins[compute_bin(self, key)];
    while (item) {
        if (item->key == key) {
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
    return EMBC_ERROR_NOT_FOUND;
}

int embc_intmap_remove(struct embc_intmap_s * self, embc_size_t key, void ** old_value) {
    struct entry_s **previous;
    struct entry_s *item;
    struct embc_intmap_iterator_s * iter;
    EMBC_DBC_NOT_NULL(self);
    previous = &self->bins[compute_bin(self, key)];
    item = *previous;
    while (item) {
        if (item->key == key) {
            *previous = item->next;
            if (old_value) {
                *old_value = item->value;
            }
            self->length -= 1;
            DL_FOREACH(self->iterators, iter) {
                if (iter->next_item == item) {
                    itermap_iterator_next_advance(iter);
                }
            }
            item->next = 0;
            item->value = 0;
            return 0;
        }
        previous = &(item->next);
        item = item->next;
    }
    return EMBC_ERROR_NOT_FOUND;
}

static void itermap_iterator_next_advance(struct embc_intmap_iterator_s * self) {
    if (self->next_item) {
        if (self->next_item->next) {
            self->next_item = self->next_item->next;
            return;
        } else {
            self->current_bin += 1;
        }
    }
    while (self->current_bin <= self->intmap->hash_mask) {
        self->next_item = self->intmap->bins[self->current_bin];
        if (self->next_item) {
            break;
        }
        self->current_bin += 1;
    }
    return;
}

struct embc_intmap_iterator_s * embc_intmap_iterator_new(struct embc_intmap_s * self) {
    EMBC_DBC_NOT_NULL(self);
    struct embc_intmap_iterator_s * iter = embc_alloc_clr(sizeof(struct embc_intmap_iterator_s));
    iter->intmap = self;
    iter->next_item = 0;
    DL_APPEND(self->iterators, iter);
    itermap_iterator_next_advance(iter);
    return iter;
}

int embc_intmap_iterator_next(struct embc_intmap_iterator_s * self, embc_size_t * key, void ** value) {
    EMBC_DBC_NOT_NULL(self);
    EMBC_DBC_NOT_NULL(key);
    EMBC_DBC_NOT_NULL(value);
    if (!self->next_item) {
        return EMBC_ERROR_NOT_FOUND;
    }
    *key = self->next_item->key;
    *value = self->next_item->value;
    itermap_iterator_next_advance(self);
    return 0;
}

void embc_intmap_iterator_free(struct embc_intmap_iterator_s * self) {
    if (self) {
        DL_DELETE(self->intmap->iterators, self);
        embc_free(self);
    }
}
