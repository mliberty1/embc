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

#include "embc/collections/list.h"

struct embc_list_s * embc_list_index(struct embc_list_s * list, embc_size_t index) {
    struct embc_list_s * item = list->next;
    embc_size_t idx = 0;
    while (item != list) {
        if (idx == index) {
            return item;
        }
        item = item->next;
        ++idx;
    }
    return 0;
}

embc_size_t embc_list_index_of(struct embc_list_s * list, struct embc_list_s * item) {
    struct embc_list_s * i = list->next;
    embc_size_t idx = 0;
    while (i != list) {
        if (i == item) {
            return idx;
        }
        i = i->next;
        ++idx;
    }
    return -1;
}
