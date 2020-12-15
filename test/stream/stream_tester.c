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

#include "embc/stream/data_link.h"
#include "embc/stream/framer.h"
#include "embc/collections/list.h"
#include "embc/log.h"
#include "embc/ec.h"
#include <stdio.h>
#include <stdarg.h>


struct msg_s {
    uint32_t metadata;
    uint8_t msg_buffer[EMBC_FRAMER_MAX_SIZE];  // hold messages and frames
    uint32_t msg_size;
    struct embc_list_s item;
};

struct stream_tester_s;

struct host_s {
    char name;
    struct embc_dl_s * dl;
    struct embc_list_s recv_expect;
    struct embc_list_s send_queue;
    struct stream_tester_s * stream_tester;
    struct host_s * target;
    uint32_t metadata;
};

struct stream_tester_s {
    uint64_t byte_drop_rate;
    uint64_t byte_insert_rate;
    uint64_t bit_error_rate;
    uint64_t timeout_rate;
    uint32_t time_ms;
    struct embc_list_s msg_free;
    struct host_s a;
    struct host_s b;
};

struct stream_tester_s s_;

static uint64_t rand_u60() {
    return (((uint64_t) rand() & 0x7fff) << 45)
        | (((uint64_t) rand() & 0x7fff) << 30)
        | (((uint64_t) rand() & 0x7fff) << 15)
        | (((uint64_t) rand() & 0x7fff) << 0);
}

/*
static uint32_t rand_u30() {
    return (((uint32_t) rand() & 0x7fff) << 15)
           | (((uint32_t) rand() & 0x7fff) << 0);
}
*/

static inline uint16_t rand_u15() {
    return ((uint16_t) rand() & 0x7fff);
}

void embc_fatal(char const * file, int line, char const * msg) {
    struct embc_dl_status_s a_status;
    struct embc_dl_status_s b_status;
    embc_dl_status_get(s_.a.dl, &a_status);
    embc_dl_status_get(s_.b.dl, &b_status);
    printf("FATAL: %s:%d: %s\n", file, line, msg);
    fflush(stdout);
    exit(1);
}

static void * hal_alloc(embc_size_t size_bytes) {
    return malloc((size_t) size_bytes);
}

static void hal_free(void * ptr) {
    free(ptr);
}

static void app_log_printf_(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

static uint32_t ll_time_get_ms(void * user_data) {
    struct host_s * host = (struct host_s *) user_data;
    return host->stream_tester->time_ms;
}

static struct msg_s * msg_alloc(struct stream_tester_s * self) {
    struct msg_s * msg;
    if (embc_list_is_empty(&self->msg_free)) {
        msg = embc_alloc_clr(sizeof(struct msg_s));
        EMBC_ASSERT_ALLOC(msg);
        embc_list_initialize(&msg->item);
    } else {
        struct embc_list_s * item = embc_list_remove_head(&self->msg_free);
        msg = EMBC_CONTAINER_OF(item, struct msg_s, item);
    }
    return msg;
}

static void ll_send(void * user_data, uint8_t const * buffer, uint32_t buffer_size) {
    struct host_s * host = (struct host_s *) user_data;
    struct msg_s * msg = msg_alloc(host->stream_tester);
    EMBC_ASSERT(buffer_size <= EMBC_FRAMER_MAX_SIZE);
    msg->msg_size = buffer_size;
    memcpy(msg->msg_buffer, buffer, buffer_size);
    embc_list_add_tail(&host->send_queue, &msg->item);
}

static uint32_t ll_send_available(void * user_data) {
    struct host_s * host = (struct host_s *) user_data;
    (void) host;
    return EMBC_FRAMER_MAX_SIZE;  // todo support a fixed length.
}

static void on_reset(void *user_data) {
    struct host_s * host = (struct host_s *) user_data;
    (void) host;
    EMBC_LOGE("on_reset_fn\n");
    EMBC_FATAL("on_reset_fn\n");
}

static void on_recv(void *user_data, uint32_t metadata,
                uint8_t *msg, uint32_t msg_size) {
    struct host_s * host = (struct host_s *) user_data;
    EMBC_ASSERT(!embc_list_is_empty(&host->recv_expect));
    struct embc_list_s * item = embc_list_remove_head(&host->recv_expect);
    struct msg_s * msg_expect = EMBC_CONTAINER_OF(item, struct msg_s, item);
    EMBC_ASSERT(metadata == msg_expect->metadata);
    EMBC_ASSERT(msg_size == msg_expect->msg_size);
    EMBC_ASSERT(0 == memcmp(msg, msg_expect->msg_buffer, msg_size));
    embc_list_add_tail(&host->stream_tester->msg_free, &msg_expect->item);
}

static void host_initialize(struct host_s *host, struct stream_tester_s * parent,
        char name, struct host_s * target,
        struct embc_dl_config_s const * config) {
    host->stream_tester = parent;
    host->name = name;
    host->target = target;
    struct embc_dl_ll_s ll = {
            .user_data = host,
            .time_get_ms = ll_time_get_ms,
            .send = ll_send,
            .send_available = ll_send_available,
    };

    host->dl = embc_dl_initialize(config, &ll);
    EMBC_ASSERT_ALLOC(host->dl);
    embc_list_initialize(&host->recv_expect);
    embc_list_initialize(&host->send_queue);

    struct embc_dl_api_s ul = {
            .user_data = host,
            .reset_fn = on_reset,
            .recv_fn = on_recv,
    };
    embc_dl_register_upper_layer(host->dl, &ul);
}

static void send(struct host_s *host) {
    struct msg_s * msg = msg_alloc(host->stream_tester);
    msg->msg_size = 1 + (rand() & 0xff);
    msg->metadata = host->metadata;
    // msg->metadata = rand() & 0x00ffffff;
    for (uint16_t idx = 0; idx < msg->msg_size; ++idx) {
        msg->msg_buffer[idx] = rand() & 0xff;
    }
    int32_t rv = embc_dl_send(host->dl, msg->metadata, msg->msg_buffer, msg->msg_size);
    if (rv) {
        EMBC_LOGE("embc_dl_send error %d: %s", (int) rv, embc_error_code_description(rv));
    } else {
        host->metadata = (host->metadata + 1) & 0x00ffffff;
        embc_list_add_tail(&host->target->recv_expect, &msg->item);
    }
}

static void action(struct stream_tester_s * self) {
    self->time_ms += 1 ; // rand() & 0x03;
    send(&self->a);
    return;
    uint8_t action = rand() & 3;
    switch (action) {
        case 0:
            // self->time_ms += rand() & 0x03;
            break;
        case 1:
            send(&self->a);
            break;
        case 2:
            send(&self->b);
            break;
        case 3:
            break;
    }
}

static void process_host(struct host_s * host) {
    struct embc_list_s * item;
    struct msg_s * msg;
    if (embc_list_is_empty(&host->send_queue)) {
        return;
    }
    item = embc_list_remove_head(&host->send_queue);
    msg = EMBC_CONTAINER_OF(item, struct msg_s, item);

    // Permute message
    //uint64_t r_byte_ins = rand_u64() % host->stream_tester->byte_insert_rate;
    //uint64_t r_bit_error = rand_u64() % host->stream_tester->bit_error_rate;

    if (host->stream_tester->byte_drop_rate) {
        uint64_t r_byte_drop = rand_u60() % host->stream_tester->byte_drop_rate;
        while (msg->msg_size && (msg->msg_size >= r_byte_drop)) {
            uint16_t idx = rand_u15() % msg->msg_size;
            if ((idx + 1U) == msg->msg_size) {
                --msg->msg_size;
            } else {
                embc_memcpy(msg->msg_buffer + idx, msg->msg_buffer + idx + 1, msg->msg_size - (idx + 1U));
            }
            --r_byte_drop;
        }
    }

    embc_dl_ll_recv(host->target->dl, msg->msg_buffer, msg->msg_size);
    embc_list_add_tail(&host->stream_tester->msg_free, &msg->item);
}

static void process(struct stream_tester_s * self) {
    int do_run = 1;
    while (do_run) {
        bool a_empty = embc_list_is_empty(&self->a.send_queue);
        bool b_empty = embc_list_is_empty(&self->b.send_queue);
        if (a_empty && b_empty) {
            do_run = 0; // nothing to do
        } else if (!a_empty && !b_empty) {
            // pick at random
            if (rand() & 1) {
                process_host(&self->b);
            } else {
                process_host(&self->a);
            }
        } else if (!a_empty) {
            process_host(&self->a);
        } else {
            process_host(&self->b);
        }
        embc_dl_process(self->a.dl);
        embc_dl_process(self->b.dl);
    }
}

int main(void) {
    struct embc_dl_config_s config = {
        .tx_window_size = 64,
        .tx_buffer_size = (1 << 13),
        .rx_window_size = 64,
        .tx_timeout_ms = 10,
        .tx_link_size = 64,
    };

    printf("RAND_MAX = %ull\n", RAND_MAX);
    embc_allocator_set(hal_alloc, hal_free);
    embc_log_initialize(app_log_printf_);
    srand(1);
    embc_memset(&s_, 0, sizeof(s_));
    embc_list_initialize(&s_.msg_free);
    host_initialize(&s_.a, &s_, 'a', &s_.b, &config);
    host_initialize(&s_.b, &s_, 'b', &s_.a, &config);

    s_.byte_drop_rate = 1000;

    while (1) {
        action(&s_);
        process(&s_);
    }
}
