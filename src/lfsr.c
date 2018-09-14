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

#include "embc/lfsr.h"
#include "embc/dbc.h"
#include "embc.h"

void lfsr_initialize(struct lfsr_s * self) {
    DBC_NOT_NULL(self);
    self->value = LFSR16_INITIAL_VALUE;
    self->error_count = 0;
    self->resync_bit_count = 16;
}

void lfsr_seed_u16(struct lfsr_s * self, uint16_t seed) {
    DBC_NOT_NULL(self);
    if (seed == 0) {
        seed = 1;
    }
    self->value = seed;
}

static inline void value_guard(struct lfsr_s * self) {
    DBC_NOT_NULL(self);
    if (self->value == 0) {
        EMBC_LOG_WARNING("Invalid lfsr value");
        self->value = 1;
    }
}

static inline int lfsr_next_u1_inner(struct lfsr_s * self) {
    int bit = 0;
    uint16_t lfsr = self->value;
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    self->value = (lfsr >> 1) | (bit << 15);
    return bit;
}

int lfsr_next_u1(struct lfsr_s * self) {
    value_guard(self);
    return lfsr_next_u1_inner(self);
}

uint8_t lfsr_next_u8(struct lfsr_s * self) {
    int i = 0;
    value_guard(self);
    for (i = 0; i < 8; ++i) {
        lfsr_next_u1(self);
    }
    return (uint8_t) ((self->value & 0xff00) >> 8);
}

uint16_t lfsr_next_u16(struct lfsr_s * self) {
    int i = 0;
    value_guard(self);
    for (i = 0; i < 16; ++i) {
        lfsr_next_u1(self);
    }

    return self->value;
}

uint32_t lfsr_next_u32(struct lfsr_s * self) {
    uint32_t value = 0;
    value = ((uint32_t) lfsr_next_u16(self)) << 16;
    value |= (uint32_t) lfsr_next_u16(self);
    return value;
}

int lfsr_follow_u8(struct lfsr_s * self, uint8_t data) {
    uint8_t expected;
    DBC_NOT_NULL(self);
    if (self->resync_bit_count) {
        self->value = (self->value >> 8) | (((uint16_t) data) << 8);
        self->resync_bit_count -= 8;
        if (self->resync_bit_count < 0) {
            self->resync_bit_count = 0;
        }
        return 0;
    }
    expected = lfsr_next_u8(self);
    if (data == expected) {
        return 0;
    } else {
        self->value = ((uint16_t) data) << 8;
        self->resync_bit_count = 8;
        ++self->error_count;
        return -1;
    }
}
