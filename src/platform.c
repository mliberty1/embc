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

#include "embc/platform.h"
#include "embc/assert.h"


static void * alloc_default(embc_size_t size_bytes) {
    (void) size_bytes;
    EMBC_FATAL("no alloc");
    return 0;
}

static void free_default(void * ptr) {
    (void) ptr;
    EMBC_FATAL("no free");
}

static embc_alloc_fn alloc_ = alloc_default;
static embc_free_fn free_ = free_default;

void embc_allocator_set(embc_alloc_fn alloc, embc_free_fn free) {
    alloc_ = (0 != alloc) ? alloc : alloc_default;
    free_ = (0 != free) ? free : free_default;;
}

void * embc_alloc(embc_size_t size_bytes) {
    return alloc_(size_bytes);
}

void embc_free(void * ptr) {
    free_(ptr);
}

