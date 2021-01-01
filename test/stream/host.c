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

// #include "embc/stream/transport.h"
#include "embc/host/uart_data_link.h"
#include "embc/stream/ring_buffer_u8.h"
#include "embc/collections/list.h"
#include "embc/cstr.h"
#include "embc/ec.h"
#include "embc/log.h"
// #include "embc/host/uart.h"
#include <stdio.h>
#include <stdarg.h>
#include <windows.h>


#define COM_PORT_PREFIX         "\\\\.\\"
// NOTE: reduce latency timer to 1 milliseconds for FTDI chips.


enum host_type_e {
    HOST_TYPE_CONTROLLER,
    HOST_TYPE_REPEATER,
    HOST_TYPE_RECEIVER,
};


static const char * HOST_TYPES[] = {
    "controller",  // Connected to repeater, or UART TX->RX.
    "repeater",    // Connect to controller or another repeater.
    "receiver",    // Receive and discard.
};

struct msg_s {
    uint32_t metadata;
    uint8_t msg_buf[EMBC_FRAMER_PAYLOAD_MAX_SIZE];  // hold messages and frames
    uint32_t msg_size;
    struct embc_list_s item;
};

struct host_s {
    // struct embc_transport_s transport;
    int host_type;
    uint32_t metadata;
    struct embc_udl_s * dl;
    struct embc_list_s msg_free;
    struct embc_list_s msg_expect;
    struct embc_list_s msg_write;
    struct embc_list_s msg_read;

    uint32_t tx_window_size;
    uint32_t time_last_ms;
    struct embc_dl_status_s dl_status;

    HANDLE ctrl_event;
};

struct host_s h_;


void embc_fatal(char const * file, int line, char const * msg) {
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
    printf("%d ", embc_time_get_ms());
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

static void on_event(void *user_data, enum embc_dl_event_e event) {
    struct host_s * host = (struct host_s *) user_data;
    (void) host;
    EMBC_LOGE("on_event(%d)\n", (int) event);
    EMBC_FATAL("on_event_fn\n");
}

static void on_recv(void *user_data, uint32_t metadata,
                    uint8_t *msg, uint32_t msg_size) {
    struct host_s * host = (struct host_s *) user_data;
    int32_t rv;

    switch (host->host_type) {
        case HOST_TYPE_CONTROLLER:
            EMBC_ASSERT(!embc_list_is_empty(&host->msg_expect));
            struct embc_list_s * item = embc_list_remove_head(&host->msg_expect);
            struct msg_s * msg_expect = EMBC_CONTAINER_OF(item, struct msg_s, item);
            if (metadata != msg_expect->metadata) {
                EMBC_LOGE("metadata mismatch: 0x%06x != 0x%06x",
                          metadata, msg_expect->metadata);
            }
            EMBC_ASSERT(metadata == msg_expect->metadata);
            EMBC_ASSERT(msg_size == msg_expect->msg_size);
            EMBC_ASSERT(0 == memcmp(msg, msg_expect->msg_buf, msg_size));
            embc_list_add_tail(&host->msg_free, &msg_expect->item);
            break;

        case HOST_TYPE_REPEATER:
            rv = embc_udl_send(host->dl, metadata, msg, msg_size);
            EMBC_ASSERT(0 == rv);
            break;

        case HOST_TYPE_RECEIVER:
            break;  // do nothing (discard)

        default:
            EMBC_FATAL("invalid host_type");
            break;
    }
}

static struct msg_s * msg_alloc(struct host_s * self) {
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

static void generate_and_send(struct host_s * host) {
    struct msg_s * msg = msg_alloc(host);
    msg->msg_size = 256; // 1 + (rand() & 0xff);
    msg->metadata = host->metadata;
    // msg->metadata = rand() & 0x00ffffff;
    for (uint16_t idx = 0; idx < msg->msg_size; ++idx) {
        msg->msg_buf[idx] = rand() & 0xff;
    }
    int32_t rv = embc_udl_send(host->dl, msg->metadata, msg->msg_buf, msg->msg_size);
    if (rv) {
        EMBC_LOGE("embc_dl_send error %d: %s", (int) rv, embc_error_code_description(rv));
        embc_list_add_tail(&host->msg_free, &msg->item);
    } else {
        host->metadata = (host->metadata + 1) & 0x00ffffff;
        embc_list_add_tail(&host->msg_expect, &msg->item);
    }
}

static void on_process(void * user_data) {
    struct host_s * self = (struct host_s *) user_data;

    struct embc_dl_status_s dl_status;

    switch (h_.host_type) {
        case HOST_TYPE_CONTROLLER:
            if ((embc_udl_send_available(h_.dl) >= EMBC_FRAMER_MAX_SIZE) &&
                (embc_list_length(&h_.msg_expect) < (int32_t) self->tx_window_size) ) {
                generate_and_send(&h_);
            }
            break;
        case HOST_TYPE_REPEATER:
            break;
        case HOST_TYPE_RECEIVER:
            break;
    }

    uint32_t time_ms = embc_time_get_ms();
    if ((time_ms - self->time_last_ms) >= 1000) {
        embc_udl_status_get(h_.dl, &dl_status);
                EMBC_LOGI("retry=%lu, resync=%lu, tx=%d, rx=%d",
                          (unsigned long int) dl_status.tx.retransmissions,
                          (unsigned long int) dl_status.rx_framer.resync,
                          (int) (dl_status.tx.msg_bytes - self->dl_status.tx.msg_bytes),
                          (int) (dl_status.rx.msg_bytes - self->dl_status.rx.msg_bytes));
        self->dl_status = dl_status;
        self->time_last_ms = time_ms;
    }
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    (void) fdwCtrlType;
    SetEvent(h_.ctrl_event);
    return TRUE;
}

#define ARG_CONSUME(count)   argc -= count; argv += count

static const char USAGE[] =
"usage: host.exe --port <port> --type <host_type>\n"
"    port: The COM port path, such as COM3\n"
"    type: The host type, one of [controller, repeater, uart]"
"\n";

// usage: --port <name> --type <host_type>

int main(int argc, char * argv[]) {
    char port_name[1024];

    struct embc_dl_config_s config = {
            .tx_window_size = 8,
            .tx_buffer_size = 1 << 12,
            .rx_window_size = 64,
            .tx_timeout_ms = 15,
            .tx_link_size = 64,
    };

    struct embc_dl_api_s ul = {
            .user_data = &h_,
            .event_fn = on_event,
            .recv_fn = on_recv,
    };

    embc_memset(&h_, 0, sizeof(h_));
    snprintf(port_name, sizeof(port_name), "%s", "COM3");
    ARG_CONSUME(1);
    while (argc) {
        if ((argc >= 2) && (0 == strcmpi(argv[0], "--port"))) {
            snprintf(port_name, sizeof(port_name), "%s", argv[1]);
            ARG_CONSUME(2);
        } else if ((argc >= 2) && (0 == strcmpi(argv[0], "--type"))) {
            if (embc_cstr_to_index(argv[1], HOST_TYPES, &h_.host_type)) {
                printf(USAGE);
                printf("Invalid host_type: %s\n", argv[1]);
                exit(1);
            }
            ARG_CONSUME(2);
        } else {
            printf(USAGE);
            exit(1);
        }
    }

    h_.ctrl_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    EMBC_ASSERT_ALLOC(h_.ctrl_event);
    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        EMBC_LOGW("Could not set control handler");
    }

    // printf("RAND_MAX = %ull\n", RAND_MAX);
    embc_allocator_set(hal_alloc, hal_free);
    embc_log_initialize(app_log_printf_);
    srand(2);

    embc_list_initialize(&h_.msg_free);
    embc_list_initialize(&h_.msg_expect);
    embc_list_initialize(&h_.msg_write);
    embc_list_initialize(&h_.msg_read);
    h_.tx_window_size = config.tx_window_size;

    h_.dl = embc_udl_initialize(&config, port_name, 3000000);
    EMBC_ASSERT_ALLOC(h_.dl);
    EMBC_ASSERT(0 == embc_udl_start(h_.dl, &ul, on_process, &h_));

    h_.time_last_ms = embc_time_get_ms();
    embc_udl_status_get(h_.dl, &h_.dl_status);

    WaitForSingleObject(h_.ctrl_event, INFINITE);
    embc_udl_finalize(h_.dl);
    return 0;
}
