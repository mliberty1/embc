/*
 * Copyright 2020 Jetperch LLC
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

/**
 * @file
 *
 * @brief Ring buffer to support the data_link implementation.
 */

#ifndef EMBC_STREAM_RING_BUFFER_U64_H__
#define EMBC_STREAM_RING_BUFFER_U64_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

struct embc_rb64_s {
    uint32_t head;
    uint32_t tail;
    uint64_t * buf;
    uint32_t buf_size;  // Size of buf in u64, capacity = buf_size - 1.
};

static inline void embc_rb64_clear(struct embc_rb64_s * self) {
    self->head = 0;
    self->tail = 0;
}

static inline void embc_rb64_init(struct embc_rb64_s * self, uint64_t * buffer, uint32_t buffer_size) {
    self->buf = buffer;
    self->buf_size = buffer_size;
    embc_rb64_clear(self);
}

static inline uint32_t embc_rb64_size(struct embc_rb64_s * self) {
    if (self->head >= self->tail) {
        return (self->head - self->tail);
    } else {
        return ((self->head + self->buf_size) - self->tail);
    }
}

static inline uint32_t embc_rb64_empty_size(struct embc_rb64_s * self) {
    return self->buf_size - 1 - embc_rb64_size(self);
}

static inline uint32_t embc_rb64_capacity(struct embc_rb64_s * self) {
    return (self->buf_size - 1);
}

static inline uint64_t * embc_rb64_head(struct embc_rb64_s * self) {
    return (self->buf + self->head);
}

static inline uint64_t * embc_rb64_tail(struct embc_rb64_s * self) {
    return (self->buf + self->tail);
}

static inline uint32_t embc_rb64_offset_incr(struct embc_rb64_s * self, uint32_t offset) {
    uint32_t next_offset = offset + 1;
    if (next_offset >= self->buf_size) {
        next_offset = 0;
    }
    return next_offset;
}

static inline bool embc_rb64_push(struct embc_rb64_s * self, uint64_t value) {
    uint32_t next_head = embc_rb64_offset_incr(self, self->head);
    if (next_head == self->tail) {  // full
        return false;
    }
    self->buf[self->head] = value;
    self->head = next_head;
    return true;
}

static inline bool embc_rb64_pop(struct embc_rb64_s * self, uint64_t * value) {
    if (self->head == self->tail) {  // empty
        return false;
    }
    *value = self->buf[self->tail];
    self->tail = embc_rb64_offset_incr(self, self->tail);
    return true;
}

static inline bool embc_rb64_discard(struct embc_rb64_s * self, uint32_t count) {
    if (count > embc_rb64_size(self)) {
        embc_rb64_clear(self);
        return false;
    }
    uint32_t tail = self->tail + count;
    if (tail >= self->buf_size) {
        tail -= self->buf_size;
    }
    self->tail = tail;
    return true;
}


#ifdef __cplusplus
}
#endif

#endif  /* EMBC_STREAM_RING_BUFFER_U64_H__ */
