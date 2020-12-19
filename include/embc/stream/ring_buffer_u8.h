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

#ifndef EMBC_STREAM_RING_BUFFER_U8_H__
#define EMBC_STREAM_RING_BUFFER_U8_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup embc
 * @defgroup embc_rbu8 Ring buffer for u8 data values.
 *
 * @brief Provide a simple, fast u8 FIFO buffer.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/// The ring buffer containing unsigned 8-bit integers.
struct embc_rb8_s {
    uint32_t head;
    uint32_t tail;
    uint8_t * buf;
    uint32_t buf_size;  // Size of buf in u8, capacity = buf_size - 1.
};

static inline void embc_rb8_clear(struct embc_rb8_s * self) {
    self->head = 0;
    self->tail = 0;
}

static inline void embc_rb8_init(struct embc_rb8_s * self, uint8_t * buffer, uint32_t buffer_size) {
    self->buf = buffer;
    self->buf_size = buffer_size;
    embc_rb8_clear(self);
}

static inline uint32_t embc_rb8_size(struct embc_rb8_s * self) {
    if (self->head >= self->tail) {
        return (self->head - self->tail);
    } else {
        return ((self->head + self->buf_size) - self->tail);
    }
}

static inline uint32_t embc_rb8_empty_size(struct embc_rb8_s * self) {
    return self->buf_size - 1 - embc_rb8_size(self);
}

static inline uint32_t embc_rb8_capacity(struct embc_rb8_s * self) {
    return (self->buf_size - 1);
}

static inline uint8_t * embc_rb8_head(struct embc_rb8_s * self) {
    return (self->buf + self->head);
}

static inline uint8_t * embc_rb8_tail(struct embc_rb8_s * self) {
    return (self->buf + self->tail);
}

static inline uint32_t embc_rb8_offset_incr(struct embc_rb8_s * self, uint32_t offset) {
    uint32_t next_offset = offset + 1;
    if (next_offset >= self->buf_size) {
        next_offset = 0;
    }
    return next_offset;
}

static inline bool embc_rb8_push(struct embc_rb8_s * self, uint8_t value) {
    uint32_t next_head = embc_rb8_offset_incr(self, self->head);
    if (next_head == self->tail) {  // full
        return false;
    }
    self->buf[self->head] = value;
    self->head = next_head;
    return true;
}

static inline bool embc_rb8_pop(struct embc_rb8_s * self, uint8_t * value) {
    if (self->head == self->tail) {  // empty
        return false;
    }
    *value = self->buf[self->tail];
    self->tail = embc_rb8_offset_incr(self, self->tail);
    return true;
}

static inline bool embc_rb8_discard(struct embc_rb8_s * self, uint32_t count) {
    if (count > embc_rb8_size(self)) {
        embc_rb8_clear(self);
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

/** @} */

#endif  /* EMBC_STREAM_RING_BUFFER_U8_H__ */
