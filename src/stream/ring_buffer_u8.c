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

#include "embc/stream/ring_buffer_u8.h"
#include "embc/log.h"


void embc_rb8_init(struct embc_rb8_s * self, uint8_t * buffer, uint16_t buffer_size) {
    self->buf = buffer;
    self->buf_size = buffer_size;
    embc_rb8_clear(self);
}

void embc_rb8_clear(struct embc_rb8_s * self) {
    self->head = 0;
    self->tail = 0;
    self->rollover = self->buf_size;
}

uint32_t embc_rb8_size(struct embc_rb8_s * self) {
    if (self->head >= self->tail) {
        return self->head - self->tail;
    } else {
        return ((self->head + self->rollover) - self->tail);
    }
}

uint32_t embc_rb8_empty_size(struct embc_rb8_s * self) {
    return self->rollover - 1 - embc_rb8_size(self);
}

uint32_t embc_rb8_capacity(struct embc_rb8_s * self) {
    return self->rollover - 1;
}

bool embc_rb8_push(struct embc_rb8_s * self, uint32_t sz) {
    uint32_t rb_sz = embc_rb8_empty_size(self);
    if (sz > rb_sz) {
        EMBC_LOGE("ring_buf_push but full");
        return false;
    }
    self->head += sz;
    if (self->head > self->buf_size) {
        self->head -= self->buf_size;
    }
    return true;
}

void embc_rb8_pop(struct embc_rb8_s * self, uint32_t sz) {
    uint32_t rb_sz = embc_rb8_size(self);
    if (sz > rb_sz) {
        sz = rb_sz;
    }
    self->tail += sz;
    if (self->tail >= self->rollover) {
        self->tail -= self->rollover;
        self->rollover = self->buf_size;
    }
    if (self->head == self->tail) {
        embc_rb8_clear(self);
    }
}

uint8_t * embc_rb8_insert(struct embc_rb8_s * self, uint32_t sz) {
    uint32_t empty_sz = embc_rb8_empty_size(self);
    uint32_t rollover_sz = 0;
    if ((self->head + sz) > self->buf_size) {
        rollover_sz = self->buf_size - self->head;
    }
    if (empty_sz < (sz + rollover_sz)) {
        return NULL;
    }
    if (rollover_sz) {
        if (self->head == self->tail) {
            self->head = 0;
            self->tail = 0;
            self->rollover = self->buf_size;
        } else {
            self->rollover = self->head;
            self->head = 0;
        }
    }
    uint8_t * ptr = self->buf + self->head;
    self->head += sz;
    return ptr;
}
