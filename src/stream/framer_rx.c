/*
 * Copyright 2014-2020 Jetperch LLC
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

#define EMBC_LOG_LEVEL EMBC_LOG_LEVEL_ALL
#include "embc/stream/framer_rx.h"
#include "embc/crc.h"
#include "embc/bbuf.h"
#include "embc/log.h"
#include <stdbool.h>

// the length needed to properly parse the data frame length
#define HEADER_SIZE_FOR_DATA (4)


enum embc_framer_rx_state_e {
    ST_SEARCH_SOF,
    ST_SEARCH_FRAME_TYPE,
    ST_STORE_HEADER,
    ST_STORE_REMAINDER,
};

static enum embc_framer_frame_type_e parse_frame_type(uint8_t u8) {
    if ((u8 & 0xe0) == 0) {
        return EMBC_FRAMER_FT_DATA;
    } else if ((u8 & 0xf8) == 0x98) {
        return EMBC_FRAMER_FT_ACK;
    } else if ((u8 & 0xf8) == 0xD8) {
        return EMBC_FRAMER_FT_NACK;
    } else {
        EMBC_LOGW("invalid frame_type: 0x%02x", (unsigned int) u8);
        return EMBC_FRAMER_FT_INVALID;
    }
}

static inline uint16_t parse_frame_id(uint8_t const * buffer) {
    return (((uint16_t) (buffer[1] & 0x7)) << 8) | buffer[2];
}

static inline uint8_t parse_port_id(uint8_t const * buffer) {
    return buffer[4] & 0x1f;
}

static inline uint16_t parse_payload_length(uint8_t const * buffer) {
    return 1 + ((uint16_t) buffer[3]);
}

static inline uint8_t parse_message_id(uint8_t const * buffer) {
    return buffer[5];
}

static bool validate_data_crc(uint8_t const * frame, uint16_t frame_sz) {
    uint8_t const * crc_value = frame + frame_sz - EMBC_FRAMER_FOOTER_SIZE;
    uint32_t crc_rx = EMBC_BBUF_DECODE_U32_LE(crc_value);
    uint32_t crc_calc = embc_crc32(0, frame + 1, frame_sz - EMBC_FRAMER_FOOTER_SIZE - 1);
    return (crc_rx == crc_calc);
}

static void handle_framing_error(struct embc_framer_rx_s * self) {
    if (self->is_sync) {
        ++self->status.resync;
        self->is_sync = 0;
        self->api.on_frame_error(self->api.user_data);
    }
}

static uint8_t frame_type_to_initial_size(enum embc_framer_frame_type_e ft) {
    switch (ft) {
        case EMBC_FRAMER_FT_DATA:
            return HEADER_SIZE_FOR_DATA;
        case EMBC_FRAMER_FT_ACK:
            return EMBC_FRAMER_ACK_SIZE;
        case EMBC_FRAMER_FT_NACK:
            return EMBC_FRAMER_NACK_SIZE;
        default:
            return 0;
    }
}

static bool handle_ack(struct embc_framer_rx_s * self) {
    uint16_t frame_id;
    uint32_t crc32 = embc_crc32(0, self->buf + 1, EMBC_FRAMER_ACK_SIZE - 2);
    uint32_t crc8_calc = (uint8_t) (crc32 & 0xff);
    uint32_t crc8_ack = self->buf[EMBC_FRAMER_ACK_SIZE - 1];
    if (crc8_calc == crc8_ack) {
        frame_id = (((uint16_t) (self->buf[1] & 0x7)) << 8) | self->buf[2];
        EMBC_LOGD("ACK: frame_id=%d", (int) frame_id);
        self->is_sync = true;
        self->api.on_ack(self->api.user_data, frame_id);
        return true;
    } else {
        EMBC_LOGW("handle_ack crc mismatch: 0x%02x != 0x%02x", (unsigned int) crc8_calc, (unsigned int) crc8_ack);
        return false;
    }
}

static bool handle_nack(struct embc_framer_rx_s * self) {
    uint16_t frame_id;
    uint8_t cause;
    uint16_t cause_frame_id;
    uint32_t crc32 = embc_crc32(0, self->buf + 1, EMBC_FRAMER_NACK_SIZE - 2);
    uint32_t crc8_calc = (uint8_t) (crc32 & 0xff);
    uint32_t crc8_ack = self->buf[EMBC_FRAMER_NACK_SIZE - 1];
    if (crc8_calc == crc8_ack) {
        frame_id = (((uint16_t) (self->buf[1] & 0x7)) << 8) | self->buf[2];
        cause = (self->buf[3] & 0x80) >> 7;
        cause_frame_id = (((uint16_t) (self->buf[3] & 0x7)) << 8) | self->buf[4];
        EMBC_LOGD("ACK: frame_id=%d, cause=%d, cause_frame_id=%d",
                  (int) frame_id, (int) cause, (int) cause_frame_id);
        self->is_sync = true;
        self->api.on_nack(self->api.user_data, frame_id, cause, cause_frame_id);
        return true;
    } else {
        EMBC_LOGW("handle_nack crc mismatch: 0x%02x != 0x%02x", (unsigned int) crc8_calc, (unsigned int) crc8_ack);
        return false;
    }
}

struct buffer_s {
    uint8_t const * buf;
    uint32_t size;
};

static inline uint8_t buf_advance(struct buffer_s * buf) {
    uint8_t u8 = buf->buf[0];
    ++buf->buf;
    --buf->size;
    return u8;
}

static void reprocess_buffer(struct embc_framer_rx_s * self) {
    handle_framing_error(self);
    self->state = ST_SEARCH_SOF;
    self->length = 0;
    uint32_t reprocess_length = self->buf_offset - 2;
    self->buf_offset = 0;
    embc_framer_rx_recv(self, self->buf + 2, reprocess_length);
    self->status.total_bytes -= reprocess_length;
}

static void consume_frame_and_search_sof(struct embc_framer_rx_s * self) {
    self->state = ST_SEARCH_SOF;
    uint32_t buf_offset = self->buf_offset;
    self->buf_offset = 0;
    uint32_t length = self->length;
    self->length = 0;
    if (buf_offset < length) {
        // something went wrong
    } else if (buf_offset == length) {
        // normal operation: consume all bytes
    } else {
        uint32_t reprocess_length = buf_offset - length;
        embc_framer_rx_recv(self, self->buf + length, reprocess_length);
        self->status.total_bytes -= reprocess_length;
    }
}

void embc_framer_rx_recv(struct embc_framer_rx_s * self, uint8_t const * buffer, uint32_t buffer_size) {
    struct buffer_s buf = {
        .buf = buffer,
        .size = buffer_size
    };
    EMBC_LOGI("received %d bytes", (int) buffer_size);
    self->status.total_bytes += buffer_size;

    while (buf.size) {
        self->buf[self->buf_offset++] = buf_advance(&buf);

        switch (self->state) {
            case ST_SEARCH_SOF:
                self->length = 0;
                if (self->buf[0] == EMBC_FRAMER_SOF) {
                    EMBC_LOGD("SOF");
                    self->state = ST_SEARCH_FRAME_TYPE;
                    break;
                } else {
                    handle_framing_error(self);
                    self->buf_offset = 0;
                }
                break;

            case ST_SEARCH_FRAME_TYPE:
                if (self->buf[1] == EMBC_FRAMER_SOF) {
                    EMBC_LOGW("expected frame_type, got SOF");
                    self->buf_offset = 1;
                    break;
                }
                self->length = frame_type_to_initial_size(parse_frame_type(self->buf[1]));
                if (self->length) {
                    self->state = ST_STORE_HEADER;
                } else {
                    handle_framing_error(self);
                    self->buf_offset = 0;
                    self->state = ST_SEARCH_SOF;
                }
                break;

            case ST_STORE_HEADER:
                if (self->buf_offset >= self->length) {
                    switch (parse_frame_type(self->buf[1])) {
                        case EMBC_FRAMER_FT_DATA:
                            self->state = ST_STORE_REMAINDER;
                            self->length = EMBC_FRAMER_OVERHEAD_SIZE + parse_payload_length(self->buf);
                            break;
                        case EMBC_FRAMER_FT_ACK:
                            if (handle_ack(self)) {
                                consume_frame_and_search_sof(self);
                            } else {
                                reprocess_buffer(self);
                            }
                            break;
                        case EMBC_FRAMER_FT_NACK:
                            if (handle_nack(self)) {
                                consume_frame_and_search_sof(self);
                            } else {
                                reprocess_buffer(self);
                            }
                            break;
                        default:
                            // todo should never happen
                            reprocess_buffer(self);
                            break;
                    }
                }
                break;

            case ST_STORE_REMAINDER:
                if (buf.size && (self->buf_offset < self->length)) {
                    uint32_t len;
                    if ((self->buf_offset + buf.size) >= self->length) {
                        // copy what we need
                        len = self->length - self->buf_offset;
                    } else {
                        // copy remaining
                        len = buf.size;
                    }
                    memcpy(self->buf + self->buf_offset, buf.buf, len);
                    self->buf_offset += len;
                    buf.buf += len;
                    buf.size -= len;
                }
                if (self->buf_offset >= self->length) {
                    if (!validate_data_crc(self->buf, self->length)) {
                        EMBC_LOGW("DATA crc invalid");
                        ++self->status.crc_errors;
                        reprocess_buffer(self);
                        break;
                    } else {
                        EMBC_LOGD("DATA received, payload %d bytes", (int) self->length - EMBC_FRAMER_OVERHEAD_SIZE);
                        ++self->status.data_frames;
                        self->is_sync = true;
                        self->api.on_frame(self->api.user_data, parse_frame_id(self->buf),
                                           (self->buf[1] >> 3) & 0x3,
                                           parse_port_id(self->buf),
                                           parse_message_id(self->buf),
                                           self->buf + EMBC_FRAMER_HEADER_SIZE,
                                           self->length - EMBC_FRAMER_OVERHEAD_SIZE);
                        consume_frame_and_search_sof(self);
                    }
                }
                break;
        }
    }
}

void embc_framer_rx_initialize(struct embc_framer_rx_s * self) {
    self->buf_offset = 0;
    self->state = ST_SEARCH_SOF;
    self->length = 0;
    self->is_sync = 0;
    memset(&self->status, 0, sizeof(self->status));
}
