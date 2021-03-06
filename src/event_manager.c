/*
 * Copyright 2017-2018 Jetperch LLC
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
 
#include "embc/event_manager.h"
#include "embc/collections/list.h"
#include "embc/time.h"
#include <stdlib.h>


struct event_s {
    int32_t event_id;
    int64_t timestamp;
    embc_evm_callback cbk_fn;
    void * cbk_user_data;
    struct embc_list_s node;
};

#define EVGET(item) embc_list_entry(item, struct event_s, node)

struct embc_evm_s {
    int32_t event_counter;
    struct embc_list_s events_pending;
    struct embc_list_s events_free;
};

struct embc_evm_s * embc_evm_allocate() {
    struct embc_evm_s * self = calloc(1, sizeof(struct embc_evm_s));
    if (self) {
        embc_list_initialize(&self->events_pending);
        embc_list_initialize(&self->events_free);
    }
    return self;
}

void embc_evm_free(struct embc_evm_s * self) {
    free(self);
}

int32_t embc_evm_schedule(struct embc_evm_s * self, int64_t timestamp,
                          embc_evm_callback cbk_fn, void * cbk_user_data) {
    struct event_s * ev = 0;
    if (embc_list_is_empty(&self->events_free)) {
        ++self->event_counter;
        ev = calloc(1, sizeof(struct event_s));
        ev->event_id = self->event_counter;
        embc_list_initialize(&ev->node);
    } else {
        ev = EVGET(embc_list_remove_head(&self->events_free));
    }
    ev->timestamp = timestamp;
    ev->cbk_fn = cbk_fn;
    ev->cbk_user_data = cbk_user_data;

    struct embc_list_s * node;
    struct event_s * ev_next;
    embc_list_foreach(&self->events_pending, node) {
        ev_next = EVGET(node);
        if (ev->timestamp < ev_next->timestamp) {
            embc_list_insert_before(node, &ev->node);
            return ev->event_id;
        }
    }
    embc_list_add_tail(&self->events_pending, &ev->node);
    return ev->event_id;
}

int32_t embc_evm_cancel(struct embc_evm_s * self, int32_t event_id) {
    struct embc_list_s * node;
    struct event_s * ev;
    embc_list_foreach(&self->events_pending, node) {
        ev = EVGET(node);
        if (ev->event_id == event_id) {
            embc_list_remove(node);
            embc_list_add_tail(&self->events_free, node);
            break;
        }
    }
    return 0;
}

int64_t embc_evm_time_next(struct embc_evm_s * self) {
    if (embc_list_is_empty(&self->events_pending)) {
        return EMBC_TIME_MIN;
    }
    return EVGET(embc_list_peek_head(&self->events_pending))->timestamp;
}

int64_t embc_evm_interval_next(struct embc_evm_s * self, int64_t time_current) {
    if (embc_list_is_empty(&self->events_pending)) {
        return -1;
    }
    struct event_s * ev = EVGET(embc_list_peek_head(&self->events_pending));
    if (ev->timestamp <= time_current) {
        return 0;
    } else {
        return ev->timestamp - time_current;
    }
}

int32_t embc_evm_process(struct embc_evm_s * self, int64_t time_current) {
    struct embc_list_s * node;
    struct event_s * ev;
    int32_t count = 0;
    embc_list_foreach(&self->events_pending, node) {
        ev = EVGET(node);
        if (ev->timestamp > time_current) {
            break;
        }
        embc_list_remove(node);
        ev->cbk_fn(ev->cbk_user_data, ev->event_id);
        embc_list_add_tail(&self->events_free, node);
        ++count;
    }
    return count;
}
