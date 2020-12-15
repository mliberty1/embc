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

#include "embc/stream/msg_ring_buffer.h"
#include "embc/log.h"
#include "embc/platform.h"

/*
 * The message storage format is:
 *    sz[7:0], sz[15:8], sz[23:16], sz[31:24], msg[0...N]
 * The message size, sz, must be less than 0x80000000.
 * Any message with the bit[31] set is considered a control header,
 * which indicates wrap_around.
 */

void embc_mrb_init(struct embc_mrb_s * self, uint8_t * buffer, uint16_t buffer_size) {
    self->buf = buffer;
    self->buf_size = buffer_size;
    embc_mrb_clear(self);
}

void embc_mrb_clear(struct embc_mrb_s * self) {
    self->head = 0;
    self->tail = 0;
    self->count = 0;
    embc_memset(self->buf, 0, self->buf_size);
}

static inline uint8_t * add_sz(uint8_t * p, uint32_t sz) {
    p[0] = sz & 0xff;
    p[1] = (sz >> 8) & 0xff;
    p[2] = (sz >> 16) & 0xff;
    p[3] = (sz >> 24) & 0xff;
    return (p + 4);
}

uint8_t * embc_mrb_alloc(struct embc_mrb_s * self, uint32_t size) {
    uint8_t *p = self->buf + self->head;
    uint32_t tail = self->tail;

    if (size > 0x80000000U) {
        EMBC_LOGE("embc_mrb_alloc too big");
        return NULL;
    }

    if ((self->head + size + 8) > self->buf_size) {
        // not enough room at end of buffer, must wrap
        if (self->head < self->tail) {
            return NULL;  // but out of room
        } else if ((size + 5) < tail) {
            // fits after wrap
            add_sz(p, 0xffffffffU);
            p = self->buf;
        } else {
            return NULL; // does not fit
        }
    } else if (self->head >= tail) {
        // fits as is
    } else if ((self->head + size + 5) < tail) {
        // fits as is
    } else {
        return NULL; // does not fit.
    }
    p = add_sz(p, size);
    self->head = (p - self->buf) + size;
    if (self->head >= self->buf_size) {
        EMBC_ASSERT(self->head == self->buf_size);
        self->head = 0;
    }
    ++self->count;
    return p;
}

static inline uint32_t get_sz(uint8_t * p) {
    return ((uint32_t) p[0])
            | (((uint32_t) p[1]) << 8)
            | (((uint32_t) p[2]) << 16)
            | (((uint32_t) p[3]) << 24);
}

uint8_t * embc_mrb_peek(struct embc_mrb_s * self, uint32_t * size) {
    uint8_t *p = self->buf + self->tail;
    uint32_t head = self->head;
    uint32_t sz;
    *size = 0;

    if (self->tail == head) {
        return NULL;
    }
    sz = get_sz(p);
    if (sz >= 0x80000000) {
        // rollover
        if (head > self->tail) {
           EMBC_LOGE("buffer overflow"); // should never be possible
            embc_mrb_clear(self);
            return NULL;
        }
        self->tail = 0;
        if (self->tail == head) {
            return NULL;
        }
        p = self->buf;
        sz = get_sz(p);
    }
    *size = sz;
    return (p + 4);
}


uint8_t * embc_mrb_pop(struct embc_mrb_s * self, uint32_t * size) {
    uint8_t *p = embc_mrb_peek(self, size);
    if (p) {
        self->tail += (4 + *size);
        if (self->tail >= self->buf_size) {
            self->tail -= self->buf_size;
        }
        if (self->count) {
            --self->count;
        }
    }
    return p;
}