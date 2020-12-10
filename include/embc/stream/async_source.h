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

/**
 * @file
 *
 * @brief Asynchronous stream source
 */

#ifndef EMBC_STREAM_ASYNC_SOURCE_H_
#define EMBC_STREAM_ASYNC_SOURCE_H_

#include <stdint.h>
#include "embc/cmacro_inc.h"
#include "embc/stream/async.h"

EMBC_CPP_GUARD_START

/**
 * @ingroup embc
 * @defgroup embc_stream_async_source Asynchronous stream source
 *
 * @brief Asynchronous stream source reference implementation.
 *
 * @{
 */

/**
 * @brief The stream source instance.
 */
struct embc_stream_source_s {
    struct embc_stream_producer_s producer;
    struct embc_stream_consumer_s * consumer;
    uint8_t file_id;
    uint8_t transaction_id;
    uint8_t * transaction_buffer;
    uint16_t transaction_length;
    uint8_t const * source_buffer;
    uint32_t source_length;
    uint32_t tx_offset;
    uint32_t rx_offset;
    void (*done_fn)(void * user_data, uint8_t status);
    void * done_user_data;
};

/**
 * @brief Initialize the stream source.
 *
 * @param self The instance to initialize.
 * @param transaction_buffer The producer buffer to use when transmitting
 *      data.  This only allows a single transaction outstanding at a time.
 * @param transaction_length The length of transaction_buffer in total_bytes.
 */
EMBC_API void embc_stream_source_initialize(
        struct embc_stream_source_s * self,
        uint8_t * transaction_buffer,
        uint16_t transaction_length);

EMBC_API void embc_stream_source_set_consumer(
        struct embc_stream_source_s * self,
        struct embc_stream_consumer_s * consumer);

EMBC_API struct embc_stream_producer_s * embc_stream_source_get_producer(
        struct embc_stream_source_s * self);

EMBC_API void embc_stream_source_configure(
        struct embc_stream_source_s * self,
        uint8_t const * source_buffer,
        uint32_t source_length,
        void (*done_fn)(void *, uint8_t),
        void * done_user_data);

EMBC_API void embc_stream_source_open(
        struct embc_stream_source_s * self);

EMBC_API void embc_stream_source_open_ex(
        struct embc_stream_source_s * self,
        void * ptr,
        uint16_t length);

/** @} */

EMBC_CPP_GUARD_END

#endif /* EMBC_STREAM_ASYNC_SOURCE_H_ */
