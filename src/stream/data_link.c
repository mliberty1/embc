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

#define EMBC_LOG_LEVEL EMBC_LOG_LEVEL_INFO
#include "embc/stream/data_link.h"
#include "embc/stream/msg_ring_buffer.h"
#include "embc/stream/ring_buffer_u64.h"
#include "embc/cdef.h"
#include "embc/ec.h"
#include "embc/log.h"
#include "embc/platform.h"
#include <inttypes.h>

#define SEND_COUNT_MAX (25)


enum tx_state_e {
    TX_ST_DISCONNECTED,
    TX_ST_CONNECTED,
};

enum tx_frame_state_e {
    TX_FRAME_ST_IDLE,
    TX_FRAME_ST_SEND,
    TX_FRAME_ST_SENT,
    TX_FRAME_ST_ACK,
};

enum rx_frame_state_e {
    RX_FRAME_ST_IDLE,
    RX_FRAME_ST_ACK,
    RX_FRAME_ST_NACK,
};

struct tx_frame_s {
    uint8_t * buf;
    uint32_t last_send_time_ms;
    uint8_t state;
    uint8_t send_count;
};

struct rx_frame_s {
    uint8_t state;
    uint8_t msg_size;  // minus 1
    uint16_t frame_id;
    uint32_t metadata;
    uint8_t msg[EMBC_FRAMER_PAYLOAD_MAX_SIZE];
};

struct embc_dl_s {
    struct embc_dl_ll_s ll_instance;
    struct embc_dl_api_s ul_instance;

    uint16_t tx_frame_last_id; // the last frame that has not yet be ACKed
    uint16_t tx_frame_next_id; // the next frame id for sending.
    uint16_t rx_next_frame_id; // the next frame that has not yet been received
    uint16_t rx_max_frame_id;  // the most future stored frame id

    struct embc_mrb_s tx_buf;
    // rx_frames also contain the rx buffers
    struct embc_rbu64_s tx_link_buf;

    struct tx_frame_s * tx_frames;
    uint16_t tx_frame_count;
    uint32_t tx_timeout_ms;
    struct rx_frame_s * rx_frames;
    uint16_t rx_frame_count;

    enum tx_state_e tx_state;
    uint32_t tx_reset_last_ms;

    embc_dl_on_send_fn on_send_cbk;
    void * on_send_user_data;

    embc_dl_lock lock;
    embc_dl_unlock unlock;
    void * lock_user_data;

    struct embc_framer_s rx_framer;
    struct embc_dl_rx_status_s rx_status;
    struct embc_dl_tx_status_s tx_status;
};

static void tx_reset(struct embc_dl_s * self);

static void lock_default(void * user_data) {
    (void) user_data;
}

static void unlock_default(void * user_data) {
    (void) user_data;
}

int32_t embc_dl_send(struct embc_dl_s * self,
                     uint32_t metadata,
                     uint8_t const *msg, uint32_t msg_size) {
    if (self->tx_state != TX_ST_CONNECTED) {
        return EMBC_ERROR_UNAVAILABLE;
    }

    self->lock(self->lock_user_data);
    uint16_t frame_id = self->tx_frame_next_id;
    uint16_t idx = frame_id & (self->tx_frame_count - 1);
    struct tx_frame_s * f = &self->tx_frames[idx];

    if (embc_framer_frame_id_subtract(frame_id, self->tx_frame_last_id) >= self->tx_frame_count) {
        EMBC_LOGD1("embc_dl_send(0x%02" PRIx32 ") too many frames outstanding", metadata);
        self->unlock(self->lock_user_data);
        return EMBC_ERROR_NOT_ENOUGH_MEMORY;
    }

    if (!embc_framer_validate_data(frame_id, metadata, msg_size)) {
        EMBC_LOGW("embc_framer_send invalid parameters");
        self->unlock(self->lock_user_data);
        return EMBC_ERROR_PARAMETER_INVALID;
    }

    uint16_t frame_sz = msg_size + EMBC_FRAMER_OVERHEAD_SIZE;
    uint8_t * b = embc_mrb_alloc(&self->tx_buf, frame_sz);
    if (!b) {
        EMBC_LOGD1("embc_dl_send(0x%06" PRIx32 ") out of buffer space", metadata);
        self->unlock(self->lock_user_data);
        return EMBC_ERROR_NOT_ENOUGH_MEMORY;
    }
    int32_t rv = embc_framer_construct_data(b, frame_id, metadata, msg, msg_size);
    EMBC_ASSERT(0 == rv);  // embc_framer_validate already checked

    // queue transmit frame for send_data()
    f->last_send_time_ms = self->ll_instance.time_get_ms(self->ll_instance.user_data);
    f->send_count = 0;
    f->buf = b;
    f->state = TX_FRAME_ST_SEND;

    self->tx_status.msg_bytes += msg_size;
    self->tx_status.bytes += frame_sz;
    self->tx_frame_next_id = (frame_id + 1) & EMBC_FRAMER_FRAME_ID_MAX;
    // frame queued for send_data()
    self->unlock(self->lock_user_data);

    if (self->on_send_cbk) {
        self->on_send_cbk(self->on_send_user_data);
    }
    return 0;
}

static uint16_t tx_buf_frame_sz(struct tx_frame_s * f) {
    return ((uint16_t) f->buf[3]) + 1 + EMBC_FRAMER_OVERHEAD_SIZE;
}

static uint16_t tx_buf_frame_id(struct tx_frame_s * f) {
    return (((uint16_t) f->buf[2] & 0x7) << 8) | f->buf[4];
}

static inline void event_emit(struct embc_dl_s * self, enum embc_dl_event_e event) {
    if (self->ul_instance.event_fn) {
        self->ul_instance.event_fn(self->ul_instance.user_data, event);
    }
}

static void send_data(struct embc_dl_s * self, uint16_t frame_id) {
    uint16_t idx = frame_id & (self->tx_frame_count - 1);
    struct tx_frame_s * f = &self->tx_frames[idx];
    if (TX_FRAME_ST_IDLE == f->state) {
        EMBC_LOGW("send_data(%d) when idle", (int) frame_id);
        return;
    } else if (TX_FRAME_ST_ACK == f->state) {
        EMBC_LOGW("send_data(%d) when already ack", (int) frame_id);  // but do it anyway
    }

    uint16_t frame_sz = tx_buf_frame_sz(f);

    uint32_t send_sz = self->ll_instance.send_available(self->ll_instance.user_data);
    if (send_sz < frame_sz) {
        // todo - consider support partial send, modify process, too
        return;
    }

    f->state = TX_FRAME_ST_SENT;
    if (f->send_count) {
        ++self->tx_status.retransmissions;
    }
    f->send_count += 1;
    if (f->send_count > SEND_COUNT_MAX) {
        EMBC_LOGW("send_data(%d), count=%d", (int) frame_id, (int) f->send_count);
        tx_reset(self);
        event_emit(self, EMBC_DL_EV_TX_DISCONNECTED);
    } else {
        EMBC_LOGD3("send_data(%d) buf->%d, count=%d, last=%d, next=%d",
                   (int) frame_id, (int) tx_buf_frame_id(f), (int) f->send_count,
                   (int) self->tx_frame_last_id, (int) self->tx_frame_next_id);
        f->last_send_time_ms = self->ll_instance.time_get_ms(self->ll_instance.user_data);
        self->ll_instance.send(self->ll_instance.user_data, f->buf, frame_sz);
    }
}

static void send_link_pending(struct embc_dl_s * self) {
    uint32_t pending_sz = embc_rbu64_size(&self->tx_link_buf);
    uint32_t send_sz = self->ll_instance.send_available(self->ll_instance.user_data) / EMBC_FRAMER_LINK_SIZE;
    if (pending_sz <= send_sz) {
        send_sz = pending_sz;
    }
    if (!send_sz) {
        return;
    }

    self->tx_status.bytes += send_sz * EMBC_FRAMER_LINK_SIZE;
    if ((self->tx_link_buf.tail + send_sz) > self->tx_link_buf.buf_size) {
        // wrap around, send in two parts
        uint32_t sz = self->tx_link_buf.buf_size - self->tx_link_buf.tail;
        self->ll_instance.send(self->ll_instance.user_data,
                               (uint8_t *) embc_rbu64_tail(&self->tx_link_buf),
                               sz * EMBC_FRAMER_LINK_SIZE);
        embc_rbu64_discard(&self->tx_link_buf, sz);
        send_sz -= sz;
        self->ll_instance.send(self->ll_instance.user_data,
                               (uint8_t *) self->tx_link_buf.buf,
                               send_sz * EMBC_FRAMER_LINK_SIZE);
        embc_rbu64_discard(&self->tx_link_buf, send_sz);
    } else {
        self->ll_instance.send(self->ll_instance.user_data,
                               (uint8_t *) embc_rbu64_tail(&self->tx_link_buf),
                               send_sz * EMBC_FRAMER_LINK_SIZE);
        embc_rbu64_discard(&self->tx_link_buf, send_sz);
    }
}

static void send_link(struct embc_dl_s * self, enum embc_framer_type_e frame_type, uint16_t frame_id) {
    if (!embc_framer_validate_link(frame_type, frame_id)) {
        return;
    }
    uint64_t b;
    int32_t rv = embc_framer_construct_link((uint8_t *) &b, frame_type, frame_id);
    if (rv) {
        EMBC_LOGW("send_link error: %d", (int) rv);
        return;
    } else if (!embc_rbu64_push(&self->tx_link_buf, b)) {
        EMBC_LOGW("link buffer full");
    }
}

static inline uint32_t reset_timeout_duration_ms(struct embc_dl_s * self) {
    return self->tx_timeout_ms * 16;
}

static void send_reset_request(struct embc_dl_s * self) {
    self->tx_state = TX_ST_DISCONNECTED;
    uint32_t time_ms = self->ll_instance.time_get_ms(self->ll_instance.user_data);
    self->tx_reset_last_ms = time_ms;
    send_link(self, EMBC_FRAMER_FT_RESET, 0);
}

static void tx_reset(struct embc_dl_s * self) {
    EMBC_LOGD1("tx_reset");
    self->tx_state = TX_ST_DISCONNECTED;
    self->tx_frame_last_id = 0;
    self->tx_frame_next_id = 0;
    embc_mrb_clear(&self->tx_buf);
    for (uint16_t f = 0; f < self->tx_frame_count; ++f) {
        self->tx_frames[f].state = TX_FRAME_ST_IDLE;
    }
    send_reset_request(self);
}

static void rx_reset(struct embc_dl_s * self) {
    EMBC_LOGD1("rx_reset");
    self->rx_next_frame_id = 0;
    self->rx_max_frame_id = 0;
    embc_rbu64_clear(&self->tx_link_buf);
    for (uint16_t f = 0; f < self->rx_frame_count; ++f) {
        self->rx_frames[f].state = RX_FRAME_ST_IDLE;
    }
}

static void on_recv_msg_done(struct embc_dl_s * self, uint32_t metadata, uint8_t *msg, uint32_t msg_size) {
    if (self->ul_instance.recv_fn) {
        self->ul_instance.recv_fn(self->ul_instance.user_data, metadata, msg, msg_size);
    }
    self->rx_status.msg_bytes += msg_size;
    ++self->rx_status.data_frames;
}

static void on_recv_data(void * user_data, uint16_t frame_id, uint32_t metadata,
                         uint8_t *msg, uint32_t msg_size) {
    struct embc_dl_s * self = (struct embc_dl_s *) user_data;
    uint16_t this_idx = frame_id & (self->rx_frame_count - 1U);
    uint16_t window_end = (self->rx_next_frame_id + self->rx_frame_count) & EMBC_FRAMER_FRAME_ID_MAX;

    if (frame_id != (frame_id & EMBC_FRAMER_FRAME_ID_MAX)) {
        EMBC_LOGE("on_recv_data(%d) invalid frame_id", (int) frame_id);
    } else if ((0 == msg_size) || (msg_size > EMBC_FRAMER_PAYLOAD_MAX_SIZE)) {
        // should never happen, but check to be safe
        EMBC_LOGE("on_recv_data(%d) invalid msg_size %d", (int) frame_id, (int) msg_size);
        send_link(self, EMBC_FRAMER_FT_NACK_FRAME_ID, frame_id);
        return;
    } else if (self->rx_next_frame_id == frame_id) {
        // next expected frame, recv immediately without putting into ring buffer.
        self->rx_frames[this_idx].state = RX_FRAME_ST_IDLE;
        on_recv_msg_done(self, metadata, msg, msg_size);
        self->rx_next_frame_id = (self->rx_next_frame_id + 1) & EMBC_FRAMER_FRAME_ID_MAX;
        if (self->rx_max_frame_id == frame_id) {
            EMBC_LOGD2("on_recv_data(%d), ACK_ALL", (int) frame_id);
            self->rx_max_frame_id = self->rx_next_frame_id;
            send_link(self, EMBC_FRAMER_FT_ACK_ALL, frame_id);
        } else {
            while (1) {
                this_idx = self->rx_next_frame_id & (self->rx_frame_count - 1U);
                struct rx_frame_s * f = &self->rx_frames[this_idx];
                if (f->state != RX_FRAME_ST_ACK) {
                    break;
                }
                EMBC_LOGD2("on_recv_data(%d), catch up", (int) self->rx_next_frame_id);
                f->state = RX_FRAME_ST_IDLE;
                on_recv_msg_done(self, f->metadata, f->msg, 1 + (uint16_t) f->msg_size);
                self->rx_next_frame_id = (self->rx_next_frame_id + 1) & EMBC_FRAMER_FRAME_ID_MAX;
            }
            frame_id = (self->rx_next_frame_id - 1U) & EMBC_FRAMER_FRAME_ID_MAX;
            send_link(self, EMBC_FRAMER_FT_ACK_ALL, frame_id);
        }
    } else if (embc_framer_frame_id_subtract(frame_id, self->rx_next_frame_id) < 0) {
        // we already have this frame.
        // ack with most recent successfully received frame_id
        EMBC_LOGD3("on_recv_data(%d) old frame next=%d", (int) frame_id, (int) self->rx_next_frame_id);
        send_link(self, EMBC_FRAMER_FT_ACK_ALL, (self->rx_next_frame_id - 1) & EMBC_FRAMER_FRAME_ID_MAX);
    } else if (embc_framer_frame_id_subtract(window_end, frame_id) <= 0) {
        EMBC_LOGI("on_recv_data(%d) frame too far into the future: next=%d, end=%d",
                  (int) frame_id, (int) self->rx_next_frame_id, (int) window_end);
        send_link(self, EMBC_FRAMER_FT_NACK_FRAME_ID, frame_id);
    } else {
        EMBC_LOGD3("on_recv_data(%d) future frame: next=%d, end=%d",
                  (int) frame_id, (int) self->rx_next_frame_id, (int) window_end);
        // future frame
        if (embc_framer_frame_id_subtract(frame_id, self->rx_max_frame_id) > 0) {
            self->rx_max_frame_id = frame_id;
        }

        // nack missing frames not already NACK'ed
        uint16_t next_frame_id = self->rx_next_frame_id;
        while (1) {
            if (next_frame_id == frame_id) {
                break;
            }
            uint16_t next_idx = next_frame_id & (self->rx_frame_count - 1U);
            if (self->rx_frames[next_idx].state == RX_FRAME_ST_IDLE) {
                self->rx_frames[next_idx].state = RX_FRAME_ST_NACK;
                send_link(self, EMBC_FRAMER_FT_NACK_FRAME_ID, next_frame_id);
            }
            next_frame_id = (next_frame_id + 1) & EMBC_FRAMER_FRAME_ID_MAX;
        }

        // store
        self->rx_frames[this_idx].state = RX_FRAME_ST_ACK;
        self->rx_frames[this_idx].msg_size = msg_size - 1;
        self->rx_frames[this_idx].frame_id = frame_id;
        self->rx_frames[this_idx].metadata = metadata;
        embc_memcpy(self->rx_frames[this_idx].msg, msg, msg_size);
        send_link(self, EMBC_FRAMER_FT_ACK_ONE, frame_id);
    }
}

static bool is_in_tx_window(struct embc_dl_s * self, uint16_t frame_id) {
    int32_t frame_delta = embc_framer_frame_id_subtract(frame_id, self->tx_frame_last_id);
    if (frame_delta < 0) {
        return false;  // frame_id is from the past, ignore
    } else if (frame_delta > self->tx_frame_count) {
        EMBC_LOGI("ack_all too far into the future: %d", (int) frame_delta);
        return false;
    }
    uint16_t frame_id_end = (self->tx_frame_next_id - 1) & EMBC_FRAMER_FRAME_ID_MAX;
    frame_delta = embc_framer_frame_id_subtract(frame_id, frame_id_end);
    if (frame_delta > 0) {
        EMBC_LOGI("ack_all out of window range: %d : recv=%d, last=%d, next=%d",
                  (int) frame_delta, (int) frame_id,
                  (int) self->tx_frame_last_id, (int) self->tx_frame_next_id);
        return false;
    }
    return true;
}

static struct tx_frame_s * tx_frame_get(struct embc_dl_s * self, uint16_t frame_id) {
    if (is_in_tx_window(self, frame_id)) {
        uint16_t idx = frame_id & (self->tx_frame_count - 1);
        struct tx_frame_s * f = &self->tx_frames[idx];
        return f;
    } else {
        return 0;
    }
}

static bool retire_tx_frame(struct embc_dl_s * self) {
    struct tx_frame_s * f = tx_frame_get(self, self->tx_frame_last_id);
    if (f && (f->state != TX_FRAME_ST_IDLE)) {
        self->tx_frame_last_id = (self->tx_frame_last_id + 1) & EMBC_FRAMER_FRAME_ID_MAX;
        ++self->tx_status.data_frames;
        f->state = TX_FRAME_ST_IDLE;
        uint32_t frame_sz = 0;
        uint8_t * frame = embc_mrb_pop(&self->tx_buf, &frame_sz);
        if (!frame) {
            EMBC_LOGE("tx buffer lost sync: empty");
        } else if (frame != f->buf) {
            EMBC_LOGE("tx buffer lost sync: mismatch");
        } else if (tx_buf_frame_sz(f) != frame_sz) {
            EMBC_LOGE("tx buffer lost sync: size mismatch");
        } else {
            // success
        }
        return true;
    }
    return false;
}

static void handle_ack_all(struct embc_dl_s * self, uint16_t frame_id) {
    int32_t frame_delta = embc_framer_frame_id_subtract(frame_id, self->tx_frame_last_id);
    if (frame_delta < 0) {
        return;  // frame_id is from the past, ignore
    } else if (frame_delta > self->tx_frame_count) {
        EMBC_LOGI("ack_all too far into the future: %d", (int) frame_delta);
        return;
    }
    uint16_t frame_id_end = (self->tx_frame_next_id - 1) & EMBC_FRAMER_FRAME_ID_MAX;
    frame_delta = embc_framer_frame_id_subtract(frame_id, frame_id_end);
    if (frame_delta > 0) {
        EMBC_LOGI("ack_all out of window range: %d", (int) frame_delta);
        frame_id = frame_id_end; // only process what we have
    }

    while (embc_framer_frame_id_subtract(frame_id, self->tx_frame_last_id) >= 0) {
        retire_tx_frame(self);
    }
}

static void handle_ack_one(struct embc_dl_s * self, uint16_t frame_id) {
    struct tx_frame_s * f = tx_frame_get(self, frame_id);
    if (f && ((f->state == TX_FRAME_ST_SEND) || (f->state == TX_FRAME_ST_SENT))) {
        f->state = TX_FRAME_ST_ACK;
    }
}

static void handle_nack_frame_id(struct embc_dl_s * self, uint16_t frame_id) {
    struct tx_frame_s * f = tx_frame_get(self, frame_id);
    if (f && (f->state != TX_FRAME_ST_IDLE)) {
        EMBC_LOGD1("handle_nack_frame_id(%d)", (int) frame_id);
        f->state = TX_FRAME_ST_SEND;
    }
}

static void handle_nack_framing_error(struct embc_dl_s * self, uint16_t frame_id) {
    struct tx_frame_s * f = tx_frame_get(self, frame_id);
    if (f && (f->state != TX_FRAME_ST_IDLE)) {
        EMBC_LOGD1("handle_nack_framing_error(%d)", (int) frame_id);
        f->state = TX_FRAME_ST_SEND;
    }
}

static void handle_reset(struct embc_dl_s * self, uint16_t frame_id) {
    EMBC_LOGD1("received reset %d from remote host", (int) frame_id);
    switch (frame_id) {
        case 0:  // reset request
            rx_reset(self);
            send_link(self, EMBC_FRAMER_FT_RESET, 1);
            event_emit(self, EMBC_DL_EV_RX_RESET_REQUEST);
            break;
        case 1:  // reset response
            if (self->tx_state == TX_ST_DISCONNECTED) {
                self->tx_state = TX_ST_CONNECTED;
                event_emit(self, EMBC_DL_EV_TX_CONNECTED);
            } else {
                EMBC_LOGW("ignore reset rsp since already connected");
            }
            break;
        default:
            EMBC_LOGW("unsupported reset %d", (int) frame_id);
            break;
    }
}

static void on_recv_link(void * user_data, enum embc_framer_type_e frame_type, uint16_t frame_id) {
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

static void on_framing_error(void * user_data) {
    struct embc_dl_s * self = (struct embc_dl_s *) user_data;
    send_link(self, EMBC_FRAMER_FT_NACK_FRAMING_ERROR, self->rx_next_frame_id);
}

void embc_dl_ll_recv(struct embc_dl_s * self,
                     uint8_t const * buffer, uint32_t buffer_size) {
    embc_framer_ll_recv(&self->rx_framer, buffer, buffer_size);
}

uint32_t embc_dl_service_interval_ms(struct embc_dl_s * self) {
    uint32_t rv = 0xffffffffU;
    int32_t frame_count = embc_framer_frame_id_subtract(self->tx_frame_next_id, self->tx_frame_last_id);
    uint32_t now = self->ll_instance.time_get_ms(self->ll_instance.user_data);

    if (self->tx_state == TX_ST_CONNECTED) {
        for (int32_t offset = 0; offset < frame_count; ++offset) {
            uint16_t frame_id = (self->tx_frame_last_id + offset) & EMBC_FRAMER_FRAME_ID_MAX;
            uint16_t idx = (frame_id + offset) & (self->tx_frame_count - 1);
            struct tx_frame_s *f = &self->tx_frames[idx];
            if (f->state == TX_FRAME_ST_SENT) {
                uint32_t delta = now - f->last_send_time_ms;
                if (delta >= self->tx_timeout_ms) {
                    return 0;
                }
                delta = self->tx_timeout_ms - delta;
                if (delta < rv) {
                    rv = delta;
                }
            }
        }
    } else {
        uint32_t delta = now - self->tx_reset_last_ms;
        uint32_t duration = reset_timeout_duration_ms(self);
        if (delta >= duration) {
            return 0;
        } else {
            rv = duration - delta;
        }
    }
    return rv;
}

static void tx_process_disconnected(struct embc_dl_s * self) {
    uint32_t now = self->ll_instance.time_get_ms(self->ll_instance.user_data);
    uint32_t delta = now - self->tx_reset_last_ms;
    uint32_t duration = reset_timeout_duration_ms(self);
    if (delta >= duration) {
        send_reset_request(self);
    }
}

static void tx_timeout(struct embc_dl_s * self) {
    struct tx_frame_s * f;
    int32_t frame_count = embc_framer_frame_id_subtract(self->tx_frame_next_id, self->tx_frame_last_id);
    uint32_t now = self->ll_instance.time_get_ms(self->ll_instance.user_data);
    for (int32_t offset = 0; offset < frame_count; ++offset) {
        uint16_t frame_id = (self->tx_frame_last_id + offset) & EMBC_FRAMER_FRAME_ID_MAX;
        uint16_t idx = frame_id & (self->tx_frame_count - 1);
        f = &self->tx_frames[idx];
        if (f->state == TX_FRAME_ST_SENT) {
            uint32_t delta = now - f->last_send_time_ms;
            if (delta >= self->tx_timeout_ms) {
                EMBC_LOGD1("tx timeout on %d", (int) frame_id);
                f->state = TX_FRAME_ST_SEND;
            }
        }
    }
}

static void tx_transmit(struct embc_dl_s * self) {
    for (uint16_t offset = 0; offset < self->tx_frame_count; ++offset) {
        uint16_t frame_id = (self->tx_frame_last_id + offset) & EMBC_FRAMER_FRAME_ID_MAX;
        uint16_t idx = (self->tx_frame_last_id + offset) & (self->tx_frame_count - 1);
        struct tx_frame_s * f = &self->tx_frames[idx];
        if (f->state == TX_FRAME_ST_SEND) {
            send_data(self, frame_id);
            break;
        }
    }
}

void embc_dl_process(struct embc_dl_s * self) {
    self->lock(self->lock_user_data);
    send_link_pending(self);
    if (self->tx_state == TX_ST_DISCONNECTED) {
        tx_process_disconnected(self);
    } else {
        tx_timeout(self);
        tx_transmit(self);
    }
    self->unlock(self->lock_user_data);
}

static uint32_t to_power_of_two(uint32_t v) {
    if (v == 0) {
        return v;
    }
    if (v > (1U << 31)) {
        // round down, which is ok for us
        return (1U << 31);
    }
    uint32_t k = 1;
    while (v > k) {
        k <<= 1;
    }
    return k;
}

struct embc_dl_s * embc_dl_initialize(
        struct embc_dl_config_s const * config,
        struct embc_dl_ll_s const * ll_instance) {
    EMBC_ASSERT(EMBC_FRAMER_LINK_SIZE == sizeof(uint64_t)); // assumption for ring_buffer_u64
    if (!config || !ll_instance) {
        EMBC_LOGE("invalid arguments");
        return 0;
    }

    uint32_t tx_link_u64_size = config->tx_link_size;
    if (!tx_link_u64_size) {
        tx_link_u64_size = config->rx_window_size;
    }

    uint32_t tx_buffer_size = config->tx_buffer_size;
    if (tx_buffer_size <= EMBC_FRAMER_MAX_SIZE) {
        // must buffer at least one message
        tx_buffer_size = EMBC_FRAMER_MAX_SIZE + 8 + 1;
    }

    uint32_t tx_window_size = to_power_of_two(config->tx_window_size);
    if (tx_window_size < 1) {
        tx_window_size = 1;
    }
    uint32_t rx_window_size = to_power_of_two(config->rx_window_size);

    // Perform single allocation for embc_dl_s and all buffers.
    size_t offset = 0;
    size_t sz;
    sz = sizeof(struct embc_dl_s);
    sz = EMBC_ROUND_UP_TO_MULTIPLE_UNSIGNED(sz, sizeof(uint64_t));
    offset += sz;

    size_t tx_frame_buf_offset = offset;
    sz = sizeof(struct tx_frame_s[2]) / 2 * tx_window_size;
    sz = EMBC_ROUND_UP_TO_MULTIPLE_UNSIGNED(sz, sizeof(uint64_t));
    offset += sz;

    size_t rx_frame_buf_offset = offset;
    sz = sizeof(struct rx_frame_s[2]) / 2 * rx_window_size;
    sz = EMBC_ROUND_UP_TO_MULTIPLE_UNSIGNED(sz, sizeof(uint64_t));
    offset += sz;

    size_t tx_link_buffer_offset = offset;
    EMBC_ASSERT(EMBC_FRAMER_LINK_SIZE == sizeof(uint64_t));
    EMBC_ASSERT((tx_link_buffer_offset & 0x7) == 0);
    sz = tx_link_u64_size * EMBC_FRAMER_LINK_SIZE;
    offset += sz;

    size_t tx_buffer_offset = offset;
    sz = EMBC_ROUND_UP_TO_MULTIPLE_UNSIGNED(sz, tx_buffer_size);
    offset += sz;

    struct embc_dl_s * self = (struct embc_dl_s *) embc_alloc_clr(offset);
    if (!self) {
        EMBC_LOGE("alloc failed");
        return 0;
    }
    EMBC_LOGI("initialize");

    self->lock = lock_default;
    self->unlock = unlock_default;

    uint8_t * mem = (uint8_t *) self;
    self->tx_frame_count = tx_window_size;
    self->tx_frames = (struct tx_frame_s *) (mem + tx_frame_buf_offset);
    self->rx_frame_count = rx_window_size;
    self->rx_frames = (struct rx_frame_s *) (mem + rx_frame_buf_offset);
    embc_rbu64_init(&self->tx_link_buf, (uint64_t *) (mem + tx_link_buffer_offset), tx_link_u64_size);
    embc_mrb_init(&self->tx_buf, (uint8_t *) (mem + tx_buffer_offset), tx_buffer_size);

    self->tx_timeout_ms = config->tx_timeout_ms;
    self->ll_instance = *ll_instance;
    self->rx_framer.api.user_data = self;
    self->rx_framer.api.framing_error_fn = on_framing_error;
    self->rx_framer.api.link_fn = on_recv_link;
    self->rx_framer.api.data_fn = on_recv_data;
    rx_reset(self);
    tx_reset(self);

    return self;
}

void embc_dl_register_upper_layer(struct embc_dl_s * self, struct embc_dl_api_s const * ul) {
    self->lock(self->lock_user_data);
    self->ul_instance = *ul;
    self->unlock(self->lock_user_data);
}

void embc_dl_reset_tx_from_event(struct embc_dl_s * self) {
    tx_reset(self);
}

int32_t embc_dl_finalize(struct embc_dl_s * self) {
    EMBC_LOGI("finalize");
    if (self) {
        embc_dl_unlock unlock = self->unlock;
        void * lock_user_data = self->lock_user_data;
        self->lock(self->lock_user_data);
        embc_free(self);
        unlock(lock_user_data);
    }
    return 0;
}

int32_t embc_dl_status_get(
        struct embc_dl_s * self,
        struct embc_dl_status_s * status) {
    if (!status) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    self->lock(self->lock_user_data);
    status->version = 1;
    status->rx = self->rx_status;
    status->rx_framer = self->rx_framer.status;
    status->tx = self->tx_status;
    self->unlock(self->lock_user_data);
    return 0;
}

void embc_dl_status_clear(struct embc_dl_s * self) {
    self->lock(self->lock_user_data);
    embc_memset(&self->rx_status, 0, sizeof(self->rx_status));
    embc_memset(&self->rx_framer.status, 0, sizeof(self->rx_framer.status));
    embc_memset(&self->tx_status, 0, sizeof(self->tx_status));
    self->unlock(self->lock_user_data);
}

void embc_dl_register_on_send(struct embc_dl_s * self,
                              embc_dl_on_send_fn cbk_fn, void * cbk_user_data) {
    self->on_send_cbk = cbk_fn;
    self->on_send_user_data = cbk_user_data;
}

void embc_dl_register_lock(struct embc_dl_s * self, embc_dl_lock lock, embc_dl_unlock unlock, void * user_data) {
    self->lock = lock ? lock : lock_default;
    self->unlock = unlock ? unlock : unlock_default;
    self->lock_user_data = user_data;
}
