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
#include "embc/stream/framer.h"
#include "embc/stream/framer_rx.h"
#include "embc/collections/list.h"
#include "embc/ec.h"
#include "embc/bbuf.h"
#include "embc/crc.h"
#include "embc/log.h"
#include "embc/platform.h"

#define FRAME_ID_MASK (EMBC_FRAMER_COUNT - 1)  // for incrementing

struct tx_msg_s {
    uint8_t buf[EMBC_FRAMER_FRAME_MAX_SIZE];  // Dynamically allocated message buffer
    uint32_t buf_sz;            // Actual data size in buf.
    struct embc_list_s item;    // priority queue or inflight queue
};

struct embc_framer_s {
    struct embc_framer_config_s config;
    struct embc_framer_hal_s hal;
    struct embc_framer_ll_s ll_instance;
    struct embc_framer_ul_s ul_instance;  // callbacks for ll_instance
    struct embc_framer_rx_s rx;

    uint16_t send_frame_id;
    uint16_t recv_frame_id;

    struct embc_framer_port_s * ports; // array, one for each port
    struct embc_list_s send_free;      // List of free tx_msg_s
    struct embc_list_s send[3];        // List of pending tx_msg_s, one for each priority
    struct embc_list_s send_inflight;  // List of inflight tx_msg_s
    struct embc_list_s send_retransmit; // List of NACK'ed tx_msg_s for retransmission
    struct tx_msg_s * sending;         // The message currently being sent
    int8_t ack_inflight;               // Count of ack/nack buffers inflight
    struct tx_msg_s * ack_construct;   // Ack/nack buffer being constructed

    struct embc_framer_status_s status;
};

static inline bool is_port_id_valid(struct embc_framer_s * self, uint8_t port_id) {
    return port_id < self->config.ports;
}

int32_t embc_framer_port_register(struct embc_framer_s * self,
                                  uint8_t port_id,
                                  struct embc_framer_port_s * port_def) {
    if (!self || !port_def) {
        EMBC_LOGW("embc_framer_port_register invalid");
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    if (!is_port_id_valid(self, port_id)) {
        EMBC_LOGW("embc_framer_port_register bad port_id=%d", (int) port_id);
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    EMBC_LOGD("port_register %d", (int) port_id);
    self->ports[port_id] = *port_def;
    return 0;
}

static void send_tx_buf(struct embc_framer_s * self, struct tx_msg_s * tx_msg) {
    self->sending = tx_msg;
    self->status.tx.bytes += tx_msg->buf_sz;
    // Call send last, which may invoke send_done() & send_next() immediately (reentrant).
    self->ll_instance.send(self->ll_instance.ll_user_data, tx_msg->buf, tx_msg->buf_sz);
}

static void send_next(struct embc_framer_s * self) {
    struct embc_list_s * item;
    struct tx_msg_s * tx_msg;
    uint16_t frame_id;
    uint32_t crc;
    uint8_t * b;

    if (self->sending) {  // already busy
        return;
    }

    if (!self->ack_inflight && self->ack_construct) {
        tx_msg = self->ack_construct;
        self->ack_construct = 0;
        ++self->ack_inflight;
        send_tx_buf(self, tx_msg);
        return;
    }

    if (!embc_list_is_empty(&self->send_retransmit)) {
        // retransmit the frame
        item = embc_list_remove_head(&self->send_retransmit);
        tx_msg = EMBC_CONTAINER_OF(item, struct tx_msg_s, item);
        embc_list_add_tail(&self->send_inflight, &tx_msg->item);
        send_tx_buf(self, tx_msg);
        return;
    }

    for (int i = EMBC_FRAMER_PRIORITY_HIGH; i >= EMBC_FRAMER_PRIORITY_LOW; --i) {
        if (embc_list_is_empty(&self->send[i])) {
            continue;
        }
        item = embc_list_remove_head(&self->send[i]);
        tx_msg = EMBC_CONTAINER_OF(item, struct tx_msg_s, item);
        frame_id = self->send_frame_id;
        self->send_frame_id = (self->send_frame_id + 1) & FRAME_ID_MASK;
        tx_msg->buf[1] |= (frame_id >> 8) & 0x7;
        tx_msg->buf[2] = (uint8_t) (frame_id & 0xff);
        crc = embc_crc32(0, tx_msg->buf + 1, tx_msg->buf_sz - EMBC_FRAMER_FOOTER_SIZE - 1);
        b = tx_msg->buf + tx_msg->buf_sz - EMBC_FRAMER_FOOTER_SIZE;
        b[0] = crc & 0xff;
        b[1] = (crc >> 8) & 0xff;
        b[2] = (crc >> 16) & 0xff;
        b[3] = (crc >> 24) & 0xff;

        embc_list_add_tail(&self->send_inflight, &tx_msg->item);
        send_tx_buf(self, tx_msg);
        return;
    }
}

int32_t embc_framer_send(struct embc_framer_s * self,
                         uint8_t priority, uint8_t port_id, uint8_t message_id,
                         uint8_t const *msg_buffer, uint32_t msg_size) {
    if (priority > EMBC_FRAMER_PRIORITY_HIGH) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    if (!msg_size) {
        return EMBC_ERROR_TOO_SMALL;
    }
    if (!is_port_id_valid(self, port_id)) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }

    struct embc_list_s tx_msg_list;
    struct embc_list_s * item;
    struct tx_msg_s * tx_msg;
    uint8_t * b;
    embc_list_initialize(&tx_msg_list);
    uint8_t is_start = 1;
    uint8_t is_end = 0;
    uint16_t payload_length;

    while (msg_size) {
        if (embc_list_is_empty(&self->send_free)) {
            // abort, free memory.
            embc_list_foreach(&tx_msg_list, item) {
                embc_list_add_tail(&self->send_free, item);
            }
            return EMBC_ERROR_NOT_ENOUGH_MEMORY;
        }
        tx_msg = EMBC_CONTAINER_OF(embc_list_remove_head(&self->send_free), struct tx_msg_s, item);
        b = tx_msg->buf;
        if (msg_size > EMBC_FRAMER_PAYLOAD_MAX_SIZE) {
            payload_length = EMBC_FRAMER_PAYLOAD_MAX_SIZE;
        } else {
            payload_length = msg_size;
            is_end = 1;
        }
        b[0] = EMBC_FRAMER_SOF;
        b[1] = (is_start << 4) | (is_end << 3);
        b[2] = 0; // leave frame_id blank for now.
        b[3] = (uint8_t) (payload_length - 1);
        b[4] = port_id & 0x1f;
        b[5] = message_id;
        memcpy(b + 6, msg_buffer, payload_length);
        msg_buffer += payload_length;
        msg_size -= payload_length;
        // defer CRC32 computation.
        b = tx_msg->buf + EMBC_FRAMER_HEADER_SIZE + payload_length;
        tx_msg->buf_sz = payload_length + EMBC_FRAMER_OVERHEAD_SIZE;
        embc_list_add_tail(&tx_msg_list, &tx_msg->item);
    }

    embc_list_append(&self->send[priority], &tx_msg_list);

    if (embc_list_is_empty(&self->send_retransmit) && embc_list_is_empty(&self->send_inflight)) {
        send_next(self);
    }

    return 0;
}

static void send_done(void *ul_user_data, uint8_t * buffer, uint32_t buffer_size) {
    struct embc_framer_s * self = (struct embc_framer_s *) ul_user_data;

    struct tx_msg_s * tx_msg = self->sending;
    self->sending = 0;
    if (!tx_msg) {
        EMBC_LOGE("tx_msg is null");
        // todo handle error
        return;
    }
    if ((tx_msg->buf != buffer) || (tx_msg->buf_sz != buffer_size)) {
        EMBC_LOGE("buffer mismatch");
        // todo handle error
        return;
    }

    if (0 == (tx_msg->buf[1] & 0xE0)) {
        // data frame.  no action necessary now, wait until ack.
        // tx_msg is already in send_inflight list.
    } else {
        // anything but data frame type
        --self->ack_inflight;
        embc_list_add_tail(&self->send_free, &tx_msg->item);
    }
    send_next(self);
}

static struct tx_msg_s * ack_buffer_get(struct embc_framer_s * self, uint16_t sz) {
    struct embc_list_s * item;
    struct tx_msg_s * msg = 0;
    if (self->ack_construct) {
        if ((self->ack_construct->buf_sz + sz) <= sizeof(self->ack_construct->buf)) {
            msg = self->ack_construct;
        }
    }
    if (!msg) {
        if (embc_list_is_empty(&self->send_free)) {
            EMBC_LOGW("need ack buffer, buf out of buffers, drop");
            return NULL;
        } else {
            item = embc_list_remove_head(&self->send_free);
            msg = EMBC_CONTAINER_OF(item, struct tx_msg_s, item);
            msg->buf_sz = 0;
            self->ack_construct = msg;
        }
    }
    return msg;
}

static void send_ack(struct embc_framer_s * self, uint16_t frame_id) {
    struct tx_msg_s * msg = ack_buffer_get(self, EMBC_FRAMER_ACK_SIZE);
    if (!msg) {
        return;
    }

    uint8_t * b = msg->buf + msg->buf_sz;
    b[0] = EMBC_FRAMER_SOF;
    b[1] = 0x98 | (uint8_t) ((frame_id >> 8) & 0x7);
    b[2] = (uint8_t) (frame_id & 0xff);
    b[3] = embc_crc32(0, b + 1, 2);
    msg->buf_sz += EMBC_FRAMER_ACK_SIZE;
    if (0 == self->ack_inflight) {
        send_next(self);
    }
}

static void send_nack(struct embc_framer_s * self, uint16_t frame_id,
            enum embc_framer_nack_cause_e cause, uint16_t cause_frame_id) {
    struct tx_msg_s * msg = ack_buffer_get(self, EMBC_FRAMER_NACK_SIZE);
    if (!msg) {
        return;
    }

    uint8_t * b = msg->buf + msg->buf_sz;
    b[0] = EMBC_FRAMER_SOF;
    b[1] = 0xD8 | (uint8_t) ((frame_id >> 8) & 0x7);
    b[2] = (uint8_t) (frame_id & 0xff);
    b[3] = (cause & 1) << 7 | (uint8_t) ((cause_frame_id >> 8) & 0x7);
    b[4] = (uint8_t) (cause_frame_id & 0xff);
    b[5] = embc_crc32(0, b + 1, 4);
    msg->buf_sz += EMBC_FRAMER_NACK_SIZE;
    if (0 == self->ack_inflight) {
        send_next(self);
    }
}

static void on_rx_frame_error(void * user_data) {
    struct embc_framer_s * self = (struct embc_framer_s *) user_data;
    EMBC_LOGW("rx_frame_error");
    send_nack(self, self->recv_frame_id, EMBC_FRAMER_NACK_CAUSE_FRAME_ERROR, 0);
}

static inline uint16_t parse_frame_id(uint8_t const * buffer) {
    return (((uint16_t) (buffer[1] & 0x7)) << 8) | buffer[2];
}

static inline uint8_t parse_is_stop(uint8_t const * buffer) {
    return (buffer[1] & 0x08) >> 3;
}

static inline uint8_t parse_port_id(uint8_t const * buffer) {
    return buffer[4] & 0x1f;
}

static inline uint8_t parse_message_id(uint8_t const * buffer) {
    return buffer[5];
}

static int16_t frame_id_subtract(uint16_t a, uint16_t b) {  // a - b
    int32_t d = ((int32_t) a) - ((int32_t) b);
    if (d > EMBC_FRAMER_INFLIGHT_MAX) {
        d -= EMBC_FRAMER_COUNT;
    } else if (d < -EMBC_FRAMER_INFLIGHT_MAX) {
        d += EMBC_FRAMER_COUNT;
    }
    return (int16_t) d;
}

static void on_rx_ack(void * user_data, uint16_t frame_id) {
    struct embc_framer_s * self = (struct embc_framer_s *) user_data;
    struct embc_list_s * item;
    struct tx_msg_s * msg;
    struct embc_framer_port_s * port;
    uint16_t msg_frame_id;
    uint8_t msg_id;
    uint8_t port_id;

    EMBC_LOGI("on_rx_ack(%d)", (int) frame_id);
    while (!embc_list_is_empty(&self->send_inflight)) {
        item = embc_list_peek_head(&self->send_inflight);
        msg = EMBC_CONTAINER_OF(item, struct tx_msg_s, item);
        msg_frame_id = parse_frame_id(msg->buf);
        if (frame_id_subtract(msg_frame_id, frame_id) <= 0) {
            embc_list_remove_head(&self->send_inflight);
            self->status.tx.data_frames += 1;
            if (parse_is_stop(msg->buf)) {
                port_id = parse_port_id(msg->buf);
                msg_id = parse_message_id(msg->buf);
                port = &self->ports[port_id];
                if (port->send_done_cbk) {
                    EMBC_LOGI("on_rx_ack(frame_id=%d, port_id=%d, msg_id=%d)",
                              (int) frame_id, (int) port_id, (int) msg_id);
                    port->send_done_cbk(port->user_data, port_id, msg_id);
                } else {
                    EMBC_LOGW("on_rx_ack(frame_id=%d, port_id=%d, msg_id=%d) but no port handler",
                              (int) frame_id, (int) port_id, (int) msg_id);
                }
            }
            embc_list_add_tail(&self->send_free, item);
        } else {
            break;
        }
    }
}

static void on_rx_nack(void * user_data, uint16_t frame_id, uint8_t cause, uint16_t cause_frame_id) {
    struct embc_framer_s * self = (struct embc_framer_s *) user_data;

    EMBC_LOGW("rx_nack %d, %d, %d", (int) frame_id, (int) cause, (int) cause_frame_id);

    // handle up to frame_id - 1 as ack.
    uint16_t ack_frame_id = (frame_id - 1) & FRAME_ID_MASK;
    on_rx_ack(user_data, ack_frame_id);

    // schedule retransmission for remainder
    embc_list_append(&self->send_retransmit, &self->send_inflight);
    send_next(self);
}

static void on_rx_frame(void * user_data, uint16_t frame_id, enum embc_framer_sequence_e seq,
                        uint8_t port_id, uint8_t message_id, uint8_t * buf, uint16_t buf_size) {
    struct embc_framer_s * self = (struct embc_framer_s *) user_data;
    (void) seq;  // todo

    if (port_id >= self->config.ports) {
        EMBC_LOGW("on_rx_frame: unsupported port_id %d", (int) port_id);
        return;
    }

    if (self->recv_frame_id == frame_id) {
        EMBC_LOGD("on_rx_frame(frame_id=%d, seq=%d, port_id=%d, message_id=%d, len=%d)",
            (int) frame_id, (int) seq, (int) port_id, (int) message_id, (int) buf_size);
        struct embc_framer_port_s * port = &self->ports[port_id];
        self->recv_frame_id = (self->recv_frame_id + 1) & FRAME_ID_MASK;
        if (!port->recv_cbk) {
            EMBC_LOGW("on_rx_frame: port_id %d has no callback", (int) port_id);
        } else {
            port->recv_cbk(port->user_data, port_id, message_id, buf, buf_size);
        }
        send_ack(self, self->recv_frame_id);
    } else {
        // todo if port=0 and EMBC_FRAMER_PORT0_CONNECT, signal connect, reset
        EMBC_LOGD("on_rx_frame received frame_id=%d != expected %d)",
                  (int) frame_id, (int) self->recv_frame_id);
        send_nack(self, self->recv_frame_id, EMBC_FRAMER_NACK_CAUSE_FRAME_ID, frame_id);
    }
}

static void rx_recv(void * user_data, uint8_t const * buffer, uint32_t buffer_size) {
    struct embc_framer_s * self = (struct embc_framer_s *) user_data;
    embc_framer_rx_recv(&self->rx, buffer, buffer_size);
}

struct embc_framer_s * embc_framer_initialize(
        struct embc_framer_config_s const * config,
        struct embc_framer_hal_s * hal,
        struct embc_framer_ll_s * ll_instance) {
    if (!config || !hal || !ll_instance) {
        EMBC_LOGE("invalid arguments");
        return 0;
    }
    if (config->ports > EMBC_FRAMER_PORTS_MAX) {
        EMBC_LOGE("ports too big: %d", config->ports);
        return 0;
    }

    struct embc_framer_s * self = (struct embc_framer_s *) embc_alloc_clr(sizeof(struct embc_framer_s));
    if (!self) {
        EMBC_LOGE("alloc failed: %d", config->ports);
        return 0;
    }
    EMBC_LOGI("initialize");

    self->config = *config;
    self->hal = *hal;
    self->ll_instance = *ll_instance;
    self->ul_instance.ul_user_data = self;
    self->ul_instance.recv = rx_recv;
    self->ul_instance.send_done = send_done;
    self->status.version = 1;

    embc_list_initialize(&self->send_free);
    for (int i = 0; i <= EMBC_FRAMER_PRIORITY_HIGH; ++i) {
        embc_list_initialize(&self->send[i]);
    }
    embc_list_initialize(&self->send_inflight);
    embc_list_initialize(&self->send_retransmit);

    self->rx.api.user_data = self;
    self->rx.api.on_frame_error = on_rx_frame_error;
    self->rx.api.on_ack = on_rx_ack;
    self->rx.api.on_nack = on_rx_nack;
    self->rx.api.on_frame = on_rx_frame;
    embc_framer_rx_initialize(&self->rx);

    self->ports = embc_alloc_clr(config->ports * sizeof(struct embc_framer_port_s));
    if (!self->ports) {
        embc_framer_finalize(self);
        return 0;
    }

    for (uint32_t i = 0; i < config->send_frames; ++i) {
        struct tx_msg_s * tx_msg = embc_alloc_clr(sizeof(struct tx_msg_s));
        if (!tx_msg) {
            embc_framer_finalize(self);
            return 0;
        }
        embc_list_initialize(&tx_msg->item);
        embc_list_add_tail(&self->send_free, &tx_msg->item);
    }

    self->ll_instance.open(self->ll_instance.ll_user_data, &self->ul_instance);

    return self;
}

static void tx_msg_list_free(struct embc_list_s * items) {
    struct embc_list_s * item;
    struct tx_msg_s * tx_msg;
    embc_list_foreach(items, item) {
        tx_msg = EMBC_CONTAINER_OF(item, struct tx_msg_s, item);
        embc_free(tx_msg);
    }
}

int32_t embc_framer_finalize(struct embc_framer_s * self) {
    EMBC_LOGD("finalize");
    if (self) {
        if (self->ll_instance.close) {
            self->ll_instance.close(self->ll_instance.ll_user_data);
        }

        tx_msg_list_free(&self->send_inflight);
        for (int i = 0; i <= EMBC_FRAMER_PRIORITY_HIGH; ++i) {
            tx_msg_list_free(&self->send[i]);
        }
        tx_msg_list_free(&self->send_free);

        if (self->ports) {
            embc_free(self->ports);
        }
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
    status->rx = self->rx.status;
    status->send_buffers_free = embc_list_length(&self->send_free);
    return 0;
}
