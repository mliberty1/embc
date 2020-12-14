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
 * @brief Message ring buffer to support the data_link implementation.
 */

#ifndef EMBC_STREAM_MESSAGE_RING_BUFFER_H__
#define EMBC_STREAM_MESSAGE_RING_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

struct embc_mrb_s {
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    uint8_t * buf;
    uint32_t buf_size;  // Size of buf in bytes
};

void embc_mrb_init(struct embc_mrb_s * self, uint8_t * buffer, uint16_t buffer_size);
void embc_mrb_clear(struct embc_mrb_s * self);

/**
 * @brief Allocate a message on the ring buffer.
 *
 * @param self The ring buffer instance.
 * @param size The desired size of the buffer.
 * @return The buffer or NULL on out of space.
 */
uint8_t * embc_mrb_alloc(struct embc_mrb_s * self, uint32_t size);

/**
 * @brief Peek at the next message from the buffer.
 *
 * @param self The message ring buffer instance.
 * @param size[out] The size of buffer in bytes.
 * @return The buffer or NULL on empty.
 */
uint8_t * embc_mrb_peek(struct embc_mrb_s * self, uint32_t * size);

/**
 * @brief Pop the next message from the buffer.
 *
 * @param self The message ring buffer instance.
 * @param size[out] The size of buffer in bytes.
 * @return The buffer or NULL on empty.
 */
uint8_t * embc_mrb_pop(struct embc_mrb_s * self, uint32_t * size);


#ifdef __cplusplus
}
#endif

#endif  /* EMBC_STREAM_MESSAGE_RING_BUFFER_H__ */
