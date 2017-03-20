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
 * @brief Asynchronous stream sink
 */

#ifndef EMBC_STREAM_ASYNC_SINK_H_
#define EMBC_STREAM_ASYNC_SINK_H_

#include <stdint.h>
#include "embc/cmacro_inc.h"
#include "embc/stream/async.h"

EMBC_CPP_GUARD_START

/**
 * @ingroup embc
 * @defgroup embc_stream_async_sink Asynchronous stream sink
 *
 * @brief Asynchronous stream sink reference implementation.
 *
 * @{
 */

/**
 * @brief Function called for stream data.
 *
 * @param user_data The arbitrary user data.
 * @param buffer The completed buffer.
 * @param length The length of buffer in bytes.
 */
typedef void (*embc_stream_sink_data_fn)(
        void * user_data,
        uint8_t const * buffer,
        uint32_t length);

/**
 * @brief The stream sink instance.
 */
struct embc_stream_sink_s {
    struct embc_stream_consumer_s consumer;
    struct embc_stream_producer_s * producer;
    uint8_t * transaction_buffer;
    uint16_t transaction_length;
    uint8_t mode;
    uint8_t * dst_buffer;
    uint32_t dst_length;
    uint32_t offset;
    embc_stream_sink_data_fn write_fn;
    void * write_user_data;
    embc_stream_sink_data_fn done_fn;
    void * done_user_data;
};

/**
 * @brief Initialize the stream sink.
 *
 * @param self The instance to initialize.
 * @param transaction_buffer The producer buffer to use when transmitting
 *      data.  This only allows a single transaction outstanding at a time.
 * @param transaction_length The length of transaction_buffer in bytes.
 */
EMBC_API void embc_stream_sink_initialize(
        struct embc_stream_sink_s * self,
        uint8_t * transaction_buffer,
        uint16_t transaction_length);

EMBC_API struct embc_stream_consumer_s * embc_stream_sink_get_consumer(
        struct embc_stream_sink_s * self);

EMBC_API void embc_stream_sink_set_producer(
        struct embc_stream_sink_s * self,
        struct embc_stream_producer_s * producer);

/**
 * @brief Set the optional function called for each write.
 *
 * @param self The stream sink instance.
 * @param fn The function to call on each write transaction.
 *      Provide 0 to unset the callback.
 * @param user_data The data to provide to fn.
 */
EMBC_API void embc_stream_sink_set_write_fn(
        struct embc_stream_sink_s * self,
        embc_stream_sink_data_fn fn,
        void * user_data);

EMBC_API void embc_stream_sink_receive(
        struct embc_stream_sink_s * self,
        uint8_t * dst_buffer,
        uint32_t dst_length,
        embc_stream_sink_data_fn done_fn,
        void * done_user_data);

/** @} */

EMBC_CPP_GUARD_END

#endif /* EMBC_STREAM_ASYNC_SINK_H_ */
