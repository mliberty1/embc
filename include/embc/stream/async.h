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
 * @brief Asynchronous stream.
 */

#ifndef EMBC_STREAM_ASYNC_H_
#define EMBC_STREAM_ASYNC_H_

#include <stdint.h>
#include "embc/cmacro_inc.h"

EMBC_CPP_GUARD_START

/**
 * @ingroup embc
 * @defgroup embc_stream_async Asynchronous stream.
 *
 * @brief Asynchronous stream.
 *
 * Unlike stdio streams, EMBC asynchronous streams are much simpler and
 * intended for true streams such as UART and on-the-fly data.
 *
 * @{
 */

struct embc_stream_handle_s;

/**
 * @brief An asynchronous stream factory.
 *
 * Factory instances allow other modules to open new streams.
 */
struct embc_stream_factory_s {
    /**
     * @brief Open a new stream.
     *
     * @param self This instance
     * @param meta Arbitrary metadata for the stream factory.  The caller
     *      maintains ownership and the buffer is only valid for the duration
     *      of the function call.
     * @param[out] handle On success, the handle for future stream operations.
     *      The handle remains valid until the call to handle->close().
     * @return 0 or error code.
     */
    int (*open)(struct embc_stream_factory_s * self,
                void const * meta,
                struct embc_stream_handle_s ** handle);
};

/**
 * @brief The stream status.
 */
struct embc_stream_status_s {
    /** The number of available bytes currently remaining. */
    uint32_t available_bytes;
    /** The number of available write transactions currently remaining. */
    uint32_t available_transactions;
};

/**
 * @brief The asynchronous stream handle.
 *
 * The stream handle is used to manage the stream including writing data.
 */
struct embc_stream_handle_s {
    /**
     * @brief Write data to the stream.
     *
     * @param self The stream handle.
     * @param buffer The buffer containing the data to write.  The caller
     *      maintains ownership and the buffer is only valid for the
     *      duration of the function call.
     * @param length The length of buffer in bytes.
     *
     * The stream protocol places no constraints or guarantees on the
     * block sizes.  Stream implementations are free to constrain
     * implementations to fixed block sizes.
     *
     * Stream implementations may choose the appropriate action on overflow,
     * but EMBC_ASSERT is recommended.  Callers wishing to avoid overflow
     * should call self->status() first to ensure that the write will
     * succeed.
     */
    void (*write)(struct embc_stream_handle_s * self,
                  uint8_t const * buffer, uint32_t length);

    /**
     * @brief Get the current stream status.
     *
     * @param self The stream handle.
     * @return The current stream status..
     *
     * This method provides a simple flow control mechanism.
     */
    struct embc_stream_status_s (*status)(struct embc_stream_handle_s * self);

    /**
     * @brief Transmit any buffered data to the receiver.
     *
     * @param self The stream handle.
     *
     * Although this method is provided for completeness, individual
     * implementations have no obligation to implement any buffering.
     */
    void (*flush)(struct embc_stream_handle_s * self);

    /**
     * @brief Close the stream.
     *
     * @param self The stream handle.
     * @param status The status code for the stream operation.
     *      0 is success.  All other values are errors.  The stream API places
     *      no restrictions on the behavior of the data on error. The receiver
     *      may define its own error behavior.
     */
    void (*close)(struct embc_stream_handle_s * self, uint8_t status);
};

/** @} */

EMBC_CPP_GUARD_END

#endif /* EMBC_STREAM_ASYNC_H_ */
