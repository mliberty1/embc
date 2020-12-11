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

/*
TODO:
 - port 0 implementation
 - enqueue send messages & defer send_done until actually done
 - add send priority support
 */

/*
 * Known issues:
 *
 * - 1: Susceptible to corrupted length:
 *   If the receiver receives a long partial message, then the message will
 *   stay in the buffer until future messages finally reach the full length.
 *   This scenario can block the receiver for an unbounded amount of time,
 *   especially if the link is mostly idle.
 *   Potential workarounds:
 *   - send a full-frame worth of SOF when transmitter is idle.
 *   - flush frame if not completed with a timeout duration.
 */

#define EMBC_LOG_LEVEL EMBC_LOG_LEVEL_ALL
#include "embc/stream/data_link.h"
#include "embc/collections/list.h"
#include "embc/ec.h"
#include "embc/bbuf.h"
#include "embc/crc.h"
#include "embc/log.h"
#include "embc/platform.h"

#define FRAME_ID_MASK (EMBC_FRAMER_COUNT - 1)  // for incrementing

// The following must all be powers of 2
#define EMBC_FRAMER_TX_RING_BUFFER_SIZE (1 << 13)
#define EMBC_FRAMER_RX_RING_BUFFER_SIZE (1 << 13)
#define EMBC_FRAMER_LINK_BUFFER_SIZE (1 << 8)
#define EMBC_FRAMER_TX_WINDOW_SIZE (1 << 6)
#define EMBC_FRAMER_RX_WINDOW_SIZE (1 << 6)


enum tx_frame_state_e {
    TX_FRAME_ST_IDLE,
    TX_FRAME_ST_SENT,
    TX_FRAME_ST_ACK,
};

enum rx_frame_state_e {
    RX_FRAME_ST_IDLE,
    RX_FRAME_ST_ACK,
    RX_FRAME_ST_NACK,
};

struct tx_frame_s {
    uint32_t last_send_time;
    uint16_t tx_buf_offset;
    uint8_t state;
    uint8_t send_count;
};

struct rx_frame_s {
    uint16_t tx_buf_offset;
    uint8_t state;
};

struct embc_dl_s {
    struct embc_dl_config_s config;
    struct embc_dl_ll_s ll_instance;

    uint16_t tx_frame_id; // the last frame that has not yet be ACKed
    uint16_t rx_frame_id; // the next frame that has not yet been received

    uint8_t tx_buf[EMBC_FRAMER_TX_RING_BUFFER_SIZE];
    uint16_t tx_buf_head;
    uint16_t tx_buf_tail;

    uint8_t rx_buf[EMBC_FRAMER_RX_RING_BUFFER_SIZE];
    uint16_t rx_buf_head;
    uint16_t rx_buf_tail;

    uint8_t link_buf[EMBC_FRAMER_LINK_BUFFER_SIZE];
    uint16_t link_buf_head;
    uint16_t link_buf_tail;

    struct tx_frame_s tx_frames[EMBC_FRAMER_TX_WINDOW_SIZE];
    struct rx_frame_s rx_frames[EMBC_FRAMER_RX_WINDOW_SIZE];

    struct embc_framer_s rx_framer;
    struct embc_dl_status_s status;
};


int32_t embc_dl_send(struct embc_dl_s * self,
                     uint8_t port_id, uint16_t message_id,
                     uint8_t const *msg_buffer, uint32_t msg_size) {
    uint8_t b[EMBC_FRAMER_MAX_SIZE];
    uint16_t frame_id = self->tx_frame_id;
    int32_t rv = embc_framer_construct_data(b, frame_id, port_id, message_id, msg_buffer, msg_size);
    if (rv) {
        EMBC_LOGW("embc_framer_send construct failed");
        return rv;
    }

    uint16_t frame_sz = msg_size + EMBC_FRAMER_OVERHEAD_SIZE;
    self->status.tx.bytes += frame_sz;
    self->ll_instance.send(self->ll_instance.user_data, b, frame_sz);

    return 0;
}

static void send_link(struct embc_dl_s * self, enum embc_framer_type_e frame_type, uint16_t frame_id) {
    uint32_t sz = (self->link_buf_tail - self->link_buf_head) & (EMBC_FRAMER_LINK_BUFFER_SIZE - 1);
    uint32_t send_sz;
    if (sz < EMBC_FRAMER_LINK_SIZE) {
        EMBC_LOG_WARN("link buffer full");
        return;
    }
    uint8_t * b = self->link_buf + self->link_buf_head;
    int rv = embc_framer_construct_link(b, frame_type, frame_id);
    if (rv) {
        EMBC_LOGW("send_link construct failed");
        return;
    }
    self->link_buf_head = (self->link_buf_head + 1) & (EMBC_FRAMER_LINK_BUFFER_SIZE - 1);

    sz = (self->link_buf_tail - self->link_buf_head) & (EMBC_FRAMER_LINK_BUFFER_SIZE - 1);
    send_sz = self->ll_instance.send_available(self->ll_instance.user_data);
    if (sz > send_sz) {
        sz = (send_sz / EMBC_FRAMER_LINK_SIZE) * EMBC_FRAMER_LINK_SIZE;
    }
    if (sz < EMBC_FRAMER_LINK_SIZE) {
        return;
    }
    self->status.tx.bytes += sz;
    if ((self->link_buf_tail + sz) > EMBC_FRAMER_LINK_BUFFER_SIZE) {
        // wrap around, send in two parts
        send_sz = EMBC_FRAMER_LINK_BUFFER_SIZE - self->link_buf_tail;
        self->ll_instance.send(self->ll_instance.user_data,
                               self->link_buf + self->link_buf_tail,
                               send_sz);
        self->link_buf_tail = 0;
        sz -= send_sz;
        self->ll_instance.send(self->ll_instance.user_data,
                               self->link_buf + self->link_buf_tail,
                               sz);
        self->link_buf_tail += sz;
    } else {
        self->ll_instance.send(self->ll_instance.user_data,
                               self->link_buf + self->link_buf_tail,
                               sz);
    }
}



void embc_dl_data_cbk(void * user_data, uint16_t frame_id,
                      uint8_t port_id, uint16_t message_id,
                      uint8_t const *msg_buffer, uint32_t msg_size) {
    struct embc_dl_s * self = (struct embc_dl_s *) user_data;
    ++self->status.rx.data_frames;
    // todo
    (void) frame_id;
    (void) port_id;
    (void) message_id;
    (void) msg_buffer;
    (void) msg_size;

    (void) send_link;

}

static void handle_ack_all(struct embc_dl_s * self, uint16_t frame_id) {
    (void) frame_id;
    ++self->status.tx.data_frames;
    // todo
}

static void handle_ack_one(struct embc_dl_s * self, uint16_t frame_id) {
    (void) self;
    (void) frame_id;
    // todo
}

static void handle_nack_frame_id(struct embc_dl_s * self, uint16_t frame_id) {
    (void) self;
    (void) frame_id;
    // todo
}

static void handle_nack_framing_error(struct embc_dl_s * self, uint16_t frame_id) {
    (void) self;
    (void) frame_id;
    // todo
}

static void handle_reset(struct embc_dl_s * self, uint16_t frame_id) {
    (void) self;
    (void) frame_id;
    // todo
}

void embc_dl_link_cbk(void * user_data, enum embc_framer_type_e frame_type, uint16_t frame_id) {
    struct embc_dl_s * self = (struct embc_dl_s *) user_data;
    switch (frame_type) {
        case EMBC_FRAMER_FT_ACK_ALL: handle_ack_all(self, frame_id); break;
        case EMBC_FRAMER_FT_ACK_ONE: handle_ack_one(self, frame_id); break;
        case EMBC_FRAMER_FT_NACK_FRAME_ID: handle_nack_frame_id(self, frame_id); break;
        case EMBC_FRAMER_FT_NACK_FRAMING_ERROR: handle_nack_framing_error(self, frame_id); break;
        case EMBC_FRAMER_FT_RESET: handle_reset(self, frame_id); break;
        default: break;
    }
}

void embc_dl_framing_error_cbk(void * user_data) {
    struct embc_dl_s * self = (struct embc_dl_s *) user_data;
    (void) self;
    // todo
}

uint32_t embc_dl_service_interval_ms(struct embc_dl_s * self) {
    (void) self;
    return 0xffffffffU;
    // todo
}

void embc_dl_process(struct embc_dl_s * self) {
    (void) self;
    // todo
}

struct embc_dl_s * embc_dl_initialize(
        struct embc_dl_config_s const * config,
        struct embc_dl_ll_s * ll_instance) {
    if (!config || !ll_instance) {
        EMBC_LOGE("invalid arguments");
        return 0;
    }

    struct embc_dl_s * self = (struct embc_dl_s *) embc_alloc_clr(sizeof(struct embc_dl_s));
    if (!self) {
        EMBC_LOGE("alloc failed");
        return 0;
    }
    EMBC_LOGI("initialize");

    self->config = *config;
    self->ll_instance = *ll_instance;
    self->status.version = 1;
    embc_framer_reset(&self->rx_framer);
    return self;
}

int32_t embc_framer_finalize(struct embc_framer_s * self) {
    EMBC_LOGD("finalize");
    if (self) {
        embc_free(self);
    }
    return 0;
}

int32_t embc_framer_status_get(
        struct embc_framer_s * self,
        struct embc_framer_status_s * status) {
    (void) self;
    if (!status) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    *status = self->status;
    return 0;
}

