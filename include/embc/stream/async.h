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
 * @brief The available IOCTL request identifiers.
 */
enum embc_stream_ioctl_e {
    /**
     * @brief An IOCTL request that does nothing.
     *
     * This request is provided for debug and testing purposes.  The
     * associated buffer and length are ignored.  Always returns 0.
     */
    EMBC_STREAM_IOCTL_NOOP,

    /**
     * @brief Transmit any buffered data to the receiver.
     *
     * The associated buffer and length are ignored.
     * Although this method is provided for completeness, individual
     * implementations have no obligation to implement any buffering.
     */
    EMBC_STREAM_IOCTL_FLUSH,

    /**
     * @brief Get the number of available bytes remaining for write.
     *
     * This request provides a simple flow control mechanism for byte-oriented
     * streams.  Length is ignored, and u32 is modified with the value.
     */
    EMBC_STREAM_IOCTL_AVAILABLE_BYTES,

    /**
     * @brief Get the number available write transactions currently remaining.
     *
     * This request provides a simple flow control mechanism for
     * message-oriented streams.  Length is ignored and u32 is modified with
     * the value.
     */
    EMBC_STREAM_IOCTL_AVAILABLE_TRANSACTIONS,

    /** The first implementation-specific IOCTL value. */
    EMBC_STREAM_IOCTL_CUSTOM = 256
};

/**
 * @brief The IOCTL transaction structure.
 */
struct embc_stream_ioctl_s {
    /** The embc_stream_ioctl_e IOCTL request identifier. */
    int16_t request;

    /** The length of data.ptr in bytes, if necessary. */
    uint16_t length;

    /**
     * The data which may be either IN, OUT or INOUT depending upon the
     * specific IOCTL request.  Each IOCTL must document the use of data.
     */
    union {
        /** A 32-bit unsigned integer. */
        uint32_t u32;
        /** A 32-bit signed integer. */
        int32_t i32;
        /** A pointer to a memory structure. */
        void * ptr;
    } data;
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
     * @brief Exchange arbitrary data with the stream.
     *
     * @param self The stream handle.
     * @param[INOUT] transaction The ioctl transaction.
     * @return 0 or error code.
     *
     * This method provides a generic mechanism for communicating arbitrary
     * out-of-band data with the stream implementation.  The default IOCTLs
     * provide a simple flow control mechanism.
     */
    int (*ioctl)(struct embc_stream_handle_s * self,
                 struct embc_stream_ioctl_s * transaction);

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

/**
 * @brief Convenience method to perform stream flush.
 *
 * @param self The stream handle.
 */
static inline void embc_stream_flush(struct embc_stream_handle_s * self) {
    struct embc_stream_ioctl_s transaction = {
            .request = EMBC_STREAM_IOCTL_FLUSH,
            .length = 0,
            .data.u32 = 0
    };
    self->ioctl(self, &transaction);
}

/**
 * @brief Perform an IOCTL that returns a u32 value.
 *
 * @param self The stream handle.
 * @param request The IOCTL request.
 * @return The 32-bit unsigned integer value.
 */
static inline uint32_t embc_stream_ioctl_u32_get(struct embc_stream_handle_s * self, int16_t request) {
    struct embc_stream_ioctl_s transaction = {
            .request = request,
            .length = 0,
            .data.u32 = 0
    };
    self->ioctl(self, &transaction);
    return transaction.data.u32;
}

/** @} */

EMBC_CPP_GUARD_END

#endif /* EMBC_STREAM_ASYNC_H_ */
