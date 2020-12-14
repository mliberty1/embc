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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

struct embc_rb8_s {
    uint32_t head;
    uint32_t tail;
    uint8_t * buf;
    uint32_t buf_size;  // Size of buf in bytes, capacity = buf_size - 1.
    uint32_t rollover;  // buf_size except when contiguous chunks do not fit
};

void embc_rb8_init(struct embc_rb8_s * self, uint8_t * buffer, uint16_t buffer_size);
void embc_rb8_clear(struct embc_rb8_s * self);
uint32_t embc_rb8_size(struct embc_rb8_s * self);
uint32_t embc_rb8_empty_size(struct embc_rb8_s * self);
uint32_t embc_rb8_capacity(struct embc_rb8_s * self);
static inline uint8_t * embc_rb8_head(struct embc_rb8_s * self) {
    return (self->buf + self->head);
}
static inline uint8_t * embc_rb8_tail(struct embc_rb8_s * self) {
    return (self->buf + self->tail);
}

bool embc_rb8_push(struct embc_rb8_s * self, uint32_t sz);
void embc_rb8_pop(struct embc_rb8_s * self, uint32_t sz);

/**
 * @brief Get & push a contiguous buffer.
 *
 * @param self The ring buffer instance.
 * @param sz The size of the buffer to get.
 * @return The contiguous buffer is length sz bytes or NULL.
 *      The caller should modify this buffer with the contents.
 *
 * This contiguous buffer feature makes this ring buffer implementation
 * a little unusual.
 */
uint8_t * embc_rb8_insert(struct embc_rb8_s * self, uint32_t sz);

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_STREAM_RING_BUFFER_U8_H__ */
