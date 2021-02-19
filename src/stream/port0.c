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

#include "embc/stream/port0.h"
#include "embc/stream/transport.h"
#include "embc/stream/pubsub.h"
#include "embc/cstr.h"
#include "embc/log.h"
#include "embc/ec.h"
#include "embc/platform.h"
#include "embc/time.h"
#include <inttypes.h>
#include <string.h>


const char EMBC_PORT0_META[] = "{\"type\":\"oam\"}";
static const char EV_TOPIC[] = "port/0/ev";
static const char TX_TOPIC[] = "port/0/tx";
static const char REMOTE_STATUS_TOPIC[] = "port/0/rstat";
static const char ECHO_ENABLE_META_TOPIC[] = "port/0/echo/enable";
static const char ECHO_OUTSTANDING_META_TOPIC[] = "port/0/echo/window";
static const char ECHO_LENGTH_META_TOPIC[] = "port/0/echo/length";

#define META_OUTSTANDING_MAX (4)


static const char TX_META[] =
    "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Data link TX state.\","
    "\"default\": 0,"
    "\"options\": [[0, \"disconnected\"], [1, \"connected\"]],"
    "\"flags\": [\"read_only\"],"
    "\"retain\": 1"
    "}";

static const char EV_META[] =
    "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Data link event.\","
    "\"default\": 256,"
    "\"options\": [[0, \"unknown\"], [1, \"rx_reset\"], [2, \"tx_disconnected\"], [3, \"tx_connected\"]],"
    "\"flags\": [\"read_only\"]"
    "}";

static const char ECHO_ENABLE_META[] =
    "{"
    "\"dtype\": \"bool\","
    "\"brief\": \"Enable echo\","
    "\"default\": 0,"
    "\"retain\": 1"
    "}";

static const char ECHO_WINDOW_META[] =
    "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Number of outstanding echo frames\","
    "\"default\": 8,"
    "\"range\": [1, 64],"  // inclusive
    "\"retain\": 1"
    "}";

static const char ECHO_LENGTH_META[] =
    "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Length of each frame in bytes\","
    "\"default\": 256,"
    "\"range\": [8, 256],"  // inclusive
    "\"retain\": 1"
    "}";

enum port0_state_e {
    ST_INIT = 0,
    ST_META,
    ST_DISCONNECTED,
    ST_CONNECTED,
};

struct embc_port0_s {
    enum embc_port0_mode_e mode;
    struct embc_dl_s * dl;
    struct embc_transport_s * transport;
    struct embc_pubsub_s * pubsub;
    char topic_prefix[EMBC_PUBSUB_TOPIC_LENGTH_MAX];
    embc_transport_send_fn send_fn;
    uint8_t meta_tx_port_id;
    uint8_t meta_rx_port_id;
    uint8_t state;
    uint8_t topic_prefix_length;

    uint8_t echo_enable;
    uint8_t echo_window;
    uint16_t echo_length;

    int64_t echo_rx_frame_id;
    int64_t echo_tx_frame_id;
    int64_t echo_buffer[EMBC_FRAMER_PAYLOAD_MAX_SIZE / sizeof(int64_t)];
};

#define pack_req(op, cmd_meta) \
     ((EMBC_PORT0_OP_##op & 0x07) | (0x00) | (((uint16_t) cmd_meta) << 8))

#define pack_rsp(op, cmd_meta) \
     ((EMBC_PORT0_OP_##op & 0x07) | (0x08) | (((uint16_t) cmd_meta) << 8))

static inline void topic_append(struct embc_port0_s * self, const char * subtopic) {
    embc_cstr_copy(self->topic_prefix + self->topic_prefix_length, subtopic, EMBC_PUBSUB_TOPIC_LENGTH_MAX - self->topic_prefix_length);
}

static inline void topic_reset(struct embc_port0_s * self) {
    self->topic_prefix[self->topic_prefix_length] = 0;
}

static void echo_send(struct embc_port0_s * self) {
    while ((self->state == ST_CONNECTED) && self->echo_enable
            && ((self->echo_tx_frame_id - self->echo_rx_frame_id) < self->echo_window)) {
        self->echo_buffer[0] = self->echo_tx_frame_id++;
        uint16_t port_data = pack_req(ECHO, 0);
        if (self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, port_data,
                          (uint8_t *) self->echo_buffer, self->echo_length)) {
            EMBC_LOGW("echo_send error");
        }
    }
}

static uint8_t on_echo_enable(void * user_data, const char * topic, const struct embc_pubsub_value_s * value) {
    (void) topic;
    struct embc_port0_s * self = (struct embc_port0_s *) user_data;
    if (value->type != EMBC_PUBSUB_DTYPE_U32) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    self->echo_enable = value->value.u32 ? 1 : 0;
    if (self->echo_enable) {
        echo_send(self);
    } else {
        self->echo_rx_frame_id = 0;
        self->echo_tx_frame_id = 0;
    }
    return 0;
}

static uint8_t on_echo_window(void * user_data, const char * topic, const struct embc_pubsub_value_s * value) {
    (void) topic;
    struct embc_port0_s * self = (struct embc_port0_s *) user_data;
    if (value->type != EMBC_PUBSUB_DTYPE_U32) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    uint32_t v = value->value.u32;
    if ((v < 1) || (v > 64)) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    self->echo_window = v;
    echo_send(self);
    return 0;
}

static uint8_t on_echo_length(void * user_data, const char * topic, const struct embc_pubsub_value_s * value) {
    (void) topic;
    struct embc_port0_s * self = (struct embc_port0_s *) user_data;
    if (value->type != EMBC_PUBSUB_DTYPE_U32) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    uint32_t v = value->value.u32;
    if ((v < 8) || (v > 256)) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    self->echo_length = v;
    echo_send(self);
    return 0;
}

static void publish(struct embc_port0_s * self, const char * subtopic, const struct embc_pubsub_value_s * value) {
    topic_append(self, subtopic);
    embc_pubsub_publish(self->pubsub, self->topic_prefix, value, NULL, NULL);
    topic_reset(self);
}

static void meta_scan(struct embc_port0_s * self) {
    uint8_t payload = 0;
    while ((self->meta_tx_port_id <= EMBC_TRANSPORT_PORT_MAX) && ((self->meta_tx_port_id - self->meta_rx_port_id) < META_OUTSTANDING_MAX)) {
        uint16_t port_data = pack_req(META, self->meta_tx_port_id);
        if (self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, port_data, &payload, 1)) {
            EMBC_LOGW("meta_scan send error");
        }
        ++self->meta_tx_port_id;
    }
}

void embc_port0_on_event_cbk(struct embc_port0_s * self, enum embc_dl_event_e event) {
    if ((EMBC_DL_EV_RX_RESET_REQUEST == event) && (self->mode == EMBC_PORT0_MODE_CLIENT)) {
        EMBC_LOGI("port0 rx reset -> tx reset");
        embc_dl_reset_tx_from_event(self->dl);
    }
    publish(self, EV_TOPIC, &embc_pubsub_u32(event));

    switch (event) {
        case EMBC_DL_EV_RX_RESET_REQUEST:
            break;
        case EMBC_DL_EV_TX_DISCONNECTED:
            if (self->state == ST_CONNECTED) {
                self->state = ST_DISCONNECTED;
            } else if (self->state == ST_META) {
                self->meta_rx_port_id = 0;
                self->meta_tx_port_id = 0;
            }
            publish(self, TX_TOPIC, &embc_pubsub_u32_r(0));
            break;
        case EMBC_DL_EV_TX_CONNECTED:
            if (self->state == ST_INIT) {
                self->state = ST_META;
                meta_scan(self);
            } else {
                self->state = ST_CONNECTED;
                publish(self, TX_TOPIC, &embc_pubsub_u32_r(1));
            }
            break;
        default:
            break;
    }
}


typedef void (*dispatch_fn)(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size);

static void op_status_req(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) cmd_meta;
    (void) msg;
    (void) msg_size;
    struct embc_dl_status_s status;
    int32_t rc = embc_dl_status_get(self->dl, &status);
    if (!rc) {
        return;
    }
    self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, pack_rsp(STATUS, 0),
                  (uint8_t *) &status, sizeof(status));
}

static void op_status_rsp(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) cmd_meta;
    publish(self, REMOTE_STATUS_TOPIC, &embc_pubsub_bin(msg, msg_size));
}

static void op_echo_req(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, pack_rsp(ECHO, cmd_meta), msg, msg_size);
}

static void op_echo_rsp(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) cmd_meta;
    if (msg_size != self->echo_length) {
        EMBC_LOGW("unexpected echo length: %d != %d", (int) msg_size, (int) self->echo_length);
    }
    if (msg_size >= 8) {
        int64_t frame_id;
        memcpy(&frame_id, msg, sizeof(frame_id));
        if (frame_id != self->echo_rx_frame_id) {
            EMBC_LOGW("echo frame_id mismatch: %" PRIi64 " != %" PRIi64, frame_id, self->echo_rx_frame_id);
        }
        self->echo_rx_frame_id = frame_id + 1;
    }
    echo_send(self);
}

static void op_timesync_req(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    int64_t times[4];
    if (msg_size < 8) {
        return;
    }
    memcpy(&times[0], msg, sizeof(times[0]));
    times[1] = embc_time_utc();
    times[2] = times[1];
    times[3] = 0;
    uint16_t port_data = pack_rsp(TIMESYNC, cmd_meta);
    if (self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, port_data,
                      (uint8_t *) times, sizeof(times))) {
        EMBC_LOGW("timestamp reply error");
    }
}

static void op_meta_req(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) msg;  // ignore
    (void) msg_size;
    uint16_t port_data = pack_rsp(META, cmd_meta);
    size_t meta_sz  = 1;
    const char * meta = embc_transport_meta_get(self->transport, cmd_meta);
    if (!meta) {
        meta = "";
    } else {
        meta_sz = strlen(meta) + 1;
    }
    if (meta_sz > EMBC_FRAMER_PAYLOAD_MAX_SIZE) {
        EMBC_LOGW("on_meta_req too big");
        return;
    }
    if ((cmd_meta <= EMBC_TRANSPORT_PORT_MAX) && meta) {
        self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, port_data,
                      (uint8_t *) meta, (uint32_t) meta_sz);
    } else {
        uint8_t empty = 0;
        self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, port_data, &empty, 1);
    }
}

static void op_meta_rsp(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) msg_size;
    char topic[EMBC_PUBSUB_TOPIC_LENGTH_MAX] = "port/";
    char * topic_end = topic + EMBC_PUBSUB_TOPIC_LENGTH_MAX;
    uint8_t port_id = cmd_meta;
    if (port_id != self->meta_rx_port_id) {
        EMBC_LOGW("meta_rsp unexpected port_id %d != %d", (int) port_id, (int) self->meta_rx_port_id);
    }
    char * t = topic + 5;
    if (port_id > 10) {
        *t++ = '0' + (port_id / 10);
    }
    *t++ = '0' + (port_id % 10);
    embc_cstr_copy(t, "/meta", topic_end - t);
    publish(self, topic, &embc_pubsub_json((char *) msg));
    self->topic_prefix[self->topic_prefix_length] = 0;

    self->meta_rx_port_id = (port_id >= self->meta_tx_port_id) ? self->meta_tx_port_id : (port_id + 1);
    if (self->meta_rx_port_id > EMBC_TRANSPORT_PORT_MAX) {
        if (self->state == ST_META) {
            self->state = ST_CONNECTED;
            publish(self, TX_TOPIC, &embc_pubsub_u32_r(1));
        }
    } else {
        meta_scan(self);
    }
}

void embc_port0_on_recv_cbk(struct embc_port0_s * self,
                uint8_t port_id,
                enum embc_transport_seq_e seq,
                uint16_t port_data,
                uint8_t *msg, uint32_t msg_size) {
    if (port_id != 0) {
        return;
    }
    if (seq != EMBC_TRANSPORT_SEQ_SINGLE) {
        // all messages are single frames only.
        EMBC_LOGW("port0 received segmented message");
        return;
    }
    uint8_t cmd_meta = (port_data >> 8) & 0xff;
    bool req = (port_data & 0x08) == 0;
    uint8_t op = port_data & 0x07;
    dispatch_fn fn = NULL;


    if (req) {
        switch (op) {
            case EMBC_PORT0_OP_STATUS:      fn = op_status_req; break;
            case EMBC_PORT0_OP_ECHO:        fn = op_echo_req; break;
            case EMBC_PORT0_OP_TIMESYNC:    fn = op_timesync_req; break;
            case EMBC_PORT0_OP_META:        fn = op_meta_req; break;
            //case EMBC_PORT0_OP_RAW:         fn = op_raw_req; break;
            default:
                break;
        }
    } else {
        switch (op) {
            case EMBC_PORT0_OP_STATUS:      fn = op_status_rsp; break;
            case EMBC_PORT0_OP_ECHO:        fn = op_echo_rsp; break;
            //case EMBC_PORT0_OP_TIMESYNC:    fn = op_timesync_rsp; break;
            case EMBC_PORT0_OP_META:        fn = op_meta_rsp; break;
            //case EMBC_PORT0_OP_RAW:         fn = op_raw_rsp; break;
            default:
                break;
        }
    }
    if (fn == NULL) {
        EMBC_LOGW("unsupported: mode=%d, req=%d, op=%d", (int) self->mode, (int) req, (int) op);
    } else {
        fn(self, cmd_meta, msg, msg_size);
    }
}

static void pubsub_create(struct embc_port0_s * self, const char * subtopic, const char * meta,
        const struct embc_pubsub_value_s * value, embc_pubsub_subscribe_fn src_fn, void * src_user_data) {
    topic_append(self, subtopic);
    embc_pubsub_meta(self->pubsub, self->topic_prefix, meta);
    if (src_fn) {
        embc_pubsub_subscribe(self->pubsub, self->topic_prefix, src_fn, src_user_data);
    }
    embc_pubsub_publish(self->pubsub, self->topic_prefix, value, src_fn, src_user_data);
    topic_reset(self);
}

struct embc_port0_s * embc_port0_initialize(enum embc_port0_mode_e mode,
        struct embc_dl_s * dl,
        struct embc_transport_s * transport, embc_transport_send_fn send_fn,
        struct embc_pubsub_s * pubsub, const char * topic_prefix) {
    struct embc_port0_s * p = embc_alloc_clr(sizeof(struct embc_port0_s));
    EMBC_ASSERT_ALLOC(p);
    p->mode = mode;
    p->dl = dl;
    p->transport = transport;
    p->pubsub = pubsub;
    embc_cstr_copy(p->topic_prefix, topic_prefix, sizeof(p->topic_prefix));
    p->topic_prefix_length = (uint8_t) strlen(p->topic_prefix);
    p->send_fn = send_fn;
    p->echo_enable = 0;
    p->echo_window = 8;
    p->echo_length = 256;

    topic_append(p, EV_TOPIC);
    embc_pubsub_meta(p->pubsub, p->topic_prefix, EV_META);
    pubsub_create(p, TX_TOPIC, TX_META, &embc_pubsub_u32_r(0), NULL, NULL);

    pubsub_create(p, ECHO_ENABLE_META_TOPIC, ECHO_ENABLE_META, &embc_pubsub_u32_r(p->echo_enable), on_echo_enable, p);
    pubsub_create(p, ECHO_OUTSTANDING_META_TOPIC, ECHO_WINDOW_META, &embc_pubsub_u32_r(p->echo_window), on_echo_window, p);
    pubsub_create(p, ECHO_LENGTH_META_TOPIC, ECHO_LENGTH_META, &embc_pubsub_u32_r(p->echo_length), on_echo_length, p);

    return p;
}

void embc_port0_finalize(struct embc_port0_s * self) {
    if (self) {
        embc_free(self);
    }
}
