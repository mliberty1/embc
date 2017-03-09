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
 * @brief Asynchronous stream API.
 *
 * This module defines an asynchronous stream API that allows a producer to
 * send data to a consumer.  The API provides flow control that allows the
 * consumer to throttle the producer's data rate.  The API also provides
 * efficient memory management for both byte streams and fixed-size block
 * streams.  The API is designed to allow for full decoupling between the
 * producer and consumer so that they can be split across tasks / threads.
 *
 * Streams are conceptually simple, but the details of flow control and
 * memory allocation, especially in resource-constrained microcontrollers,
 * significantly complicates the design.  See
 * Wikipedia's page on the
 * <a href="https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem">"
 * Producer-consumer problem"</a> for additional background.
 *
 * Streams come in different flavors.  Some streams are fully
 * byte-oriented and accept any size of write transaction, up to a maximum
 * size limited by available memory.  Other streams may support fixed block
 * sizes.  Fixed block-size streams often need to add headers before the
 * payload provided by the producer.  To minimize both
 * copying and memory allocation, this API contains a provision for the
 * consumer to optionally provide the memory buffer to the producer.
 *
 * The transaction types are split into IOCTL and EVENT categories.
 * IOCTLs are initiated by the producer and provided to the consumer's send().
 * The consumer responds to IOCTL requests through producer's send().
 * EVENTs are initiated by the consumer and sent to the producer through
 * the producer's send().  The producer does not directly respond to EVENTs.
 *
 * This API does not define how streams are constructed and connected.  Some
 * streams may choose to implement a factory function that returns the
 * consumer instance.  However, streams are often part of a larger instance,
 * and a method call can return the stream.  In general, consumers should
 * have a method that allows the user to connect the producer.  When a new
 * producer is connected, the consumer should send
 * EMBC_STREAM_EVENT_WRITE_REQUEST to allow the producer to send data.
 *
 * @{
 */

/**
 * @brief The available transaction type identifiers.
 */
enum embc_stream_transaction_type_e {
    /**
     * @brief An IOCTL request that does nothing.
     *
     * This request is provided for debug and testing purposes.  The
     * associated buffer and length are ignored.  The consumer must
     * always respond with status 0.
     */
    EMBC_STREAM_IOCTL_NOOP,

    /**
     * @brief Open a new file over the stream.
     *
     * Length is 0 and data is ignored.  Consumers that only support
     * a byte stream should respond with status=0 with file_id=0.
     * Consumers that support files should return a non-zero file_id on
     * success.
     */
    EMBC_STREAM_IOCTL_OPEN,

    /**
     * @brief Transmit data from the producer to the consumer.
     *
     * The associated data.ptr points to the buffer to write, and the
     * length contains the number of bytes to write.  The buffer must
     * remain valid until the corresponding response.
     *
     * The stream protocol places no constraints or guarantees on the
     * block sizes.  Stream implementations are free to constrain
     * implementations to fixed block sizes.
     *
     * Stream implementations may choose the appropriate action on overflow.
     * Streams designed to handle overflow should respond with an error
     * code.  However, raising EMBC_ASSERT is allowed.
     */
    EMBC_STREAM_IOCTL_WRITE,

    /**
     * @brief An IOCTL that closes the stream.
     *
     * The associated data and length are ignored.  The consumer must
     * flush any queued data before closing the stream.  The producer
     * may set "status" if the close is due to an error.  The consumer
     * does NOT provide a response.
     */
    EMBC_STREAM_IOCTL_CLOSE,

    /**
     * @brief Transmit any buffered data to the receiver.
     *
     * The associated data and length are ignored.
     * Although this method is provided for completeness, individual
     * implementations have no obligation to implement any buffering.
     */
    EMBC_STREAM_IOCTL_FLUSH,

    /** The first implementation-specific IOCTL value. */
    EMBC_STREAM_IOCTL_CUSTOM = 64,

    /**
     * @brief An EVENT that does nothing.
     *
     * This request is provided for debug and testing purposes.  The
     * associated buffer and length are ignored.
     */
    EMBC_STREAM_EVENT_NOOP = 128,

    /**
     * @brief Inform the producer that the consumer is ready for write data.
     *
     * This event enables the consumer to apply backpressure (flow control) to
     * the producer.  Producers should not send EMBC_STREAM_IOCTL_WRITE
     * until allowed through this EMBC_STREAM_EVENT_WRITE_REQUEST.
     *
     * The associated data takes two formats.  For streams that support
     * variable write sizes, the length is 0.  data.u16[1] contains the
     * maximum transfer unit (MTU) allowed for write transactions and
     * data.u16[1] contains the maximum number of total, simultaneous,
     * outstanding write transactions.  Note that data.u16[1] is the total,
     * not the remaining, number of transactions to avoid any uncertainty
     * over messages-in-flight.  The producer is free to allocate and manage
     * the buffer memory.
     *
     * For streams that support fixed block sizes, the consumer may choose
     * to provide the buffer in the request.  By providing a buffer, the
     * consumer can minimize memory usage and copying, especially when the
     * consumer must add a header before sending the buffer down a chain of
     * streams.  In this mode, data.ptr contains the buffer to fill and
     * length contains the desired number of bytes.  The producer is obligated
     * to provide length bytes except for the last WRITE transaction in a
     * file.
     */
    EMBC_STREAM_EVENT_WRITE_REQUEST = 129,

    /**
     * @brief The target consumer connection was established.
     *
     * Length is 0 and data is ignored.
     */
    EMBC_STREAM_EVENT_CONNECT = 130,

    /**
     * @brief The target consumer connection was disconnected.
     *
     * Length is 0 and data is ignored.
     */
    EMBC_STREAM_EVENT_DISCONNECT = 131,


    /** The first implementation-specific EVENT value. */
    EMBC_STREAM_EVENT_CUSTOM = 196,

};

/**
 * @brief The stream transaction structure.
 */
struct embc_stream_transaction_s {
    /** The embc_stream_transaction_type_e transaction type identifier. */
    uint8_t type;

    /** The transaction status: 0 or error code. */
    uint8_t status;

    /**
     * @brief The file identifier (optional).
     *
     * For streams that support open and close, this file identifier
     * is used to indicate the target file.  If the stream implementation
     * allows multiple files to be open simultaneously, then this file_id also
     * allows multiplexing of virtual streams over a single stream
     * implementation.
     */
    uint8_t file_id;

    /**
     * @brief The transaction identifier assigned by the producer (optional).
     *
     * The producer may set this value to match IOCTL requests with the
     * consumer's responses.
     */
    uint8_t producer_transaction_id;

    /**
     * @brief The transaction identifier assigned by the consumer (optional).
     *
     * The consumer may set this value to match EMBC_STREAM_EVENT_WRITE_REQUEST
     * with the producer's EMBC_STREAM_IOCTL_WRITE.  Producers respecting
     * EMBC_STREAM_EVENT_WRITE_REQUEST must populate the provided value in the
     * EMBC_STREAM_IOCTL_WRITE transaction.  Producers that do not respect
     * the write request must set this value to 0.  Any consumer implementing
     * block-based buffers should provide a non-zero consumer_transaction_id
     * to distinguish if the producer has respected the request.
     */
    uint8_t consumer_transaction_id;

    /**
     * @brief Reserved byte for future use.
     */
    uint8_t reserved;

    /**
     * @brief The length of data.ptr in bytes.
     *
     * This field is only set when the type requires data.ptr.  The length
     * should be 0 when data is unused or for any immediate data type.
     */
    uint16_t length;

    /**
     * @brief The data associated with the transaction.
     *
     * IOCTLs may define this data may be either IN, OUT or INOUT depending
     * upon the specific IOCTL request.  If data.ptr is provided, it must
     * remain valid and unmodified until the response is received in
     * producer's send() callback.
     *
     * For EVENTs, data is normally either unused or is one of the immediate
     * types.  Only the EMBC_STREAM_EVENT_WRITE_REQUEST EVENT uses data.ptr
     * to provide the block buffer for EMBC_STREAM_IOCTL_WRITE.
     *
     * Each transaction type must document the use of data.
     */
    union {
        /** A 32-bit unsigned integer. */
        uint32_t u32;
        /** Two 16-bit unsigned integers. */
        uint16_t u16[2];
        /** A pointer to a memory structure. */
        void * ptr;
    } data;

};

/**
 * @brief The asynchronous stream producer.
 *
 * The stream producer interface is used by the consumer to send transactions
 * to the producer.
 */
struct embc_stream_producer_s {
    /**
     * @brief Send an IOCTL response or EVENT to the producer.
     *
     * @param self The stream producer.
     * @param transaction The transaction.  This instance is only valid for
     *      the duration of the call.  For IOCTL responses with data.ptr,
     *      this function call indicates that the producer can release or
     *      reuse the associated buffer.
     * @pre self is not NULL.
     * @pre transaction is not NULL.
     *
     * This method provides a generic mechanism for communicating from the
     * consumer to the producer.  See embc_stream_transaction_type_e.
     */
    void (*send)(struct embc_stream_producer_s * self,
                 struct embc_stream_transaction_s * transaction);
};

/**
 * @brief The asynchronous stream consumer.
 *
 * The stream consumer interface is used by the producer to send transactions
 * to the consumer.  The consumer provides responses as necessary through the
 * producer's send() callback.
 */
struct embc_stream_consumer_s {
    /**
     * @brief Send an IOCTL request to the consumer.
     *
     * @param self The stream consumer.
     * @param transaction The transaction.  Ownership of the transaction
     *      instance is retained by the producer and the transaction is only
     *      valid for the duration of the call.  The consumer must copy the
     *      transaction into any queue.  For transactions using data.ptr, the
     *      data.ptr memory must remain valid until the producer receives the
     *      response from the consumer.
     * @pre self is not NULL.
     * @pre transaction is not NULL.
     *
     * This method provides a generic mechanism for communicating from the
     * producer to the consumer.  See embc_stream_transaction_type_e.
     *
     * The consumer may generate the IOCTL response from within this function
     * call, which typically occurs when the producer and consumer are running
     * within the same task / thread.  The producer must be designed to
     * successfully handle either an immediate callback or a deferred callback.
     */
    void (*send)(struct embc_stream_consumer_s * self,
                 struct embc_stream_transaction_s * transaction);
};

/**
 * @brief Convenience function to perform stream close.
 *
 * @param self The stream consumer.
 * @param file_id The file identifier.
 * @param status The close status: 0 for success.
 */
static inline void embc_stream_close(struct embc_stream_consumer_s * self,
                                    uint8_t file_id,
                                    uint8_t status) {
    struct embc_stream_transaction_s transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.type = EMBC_STREAM_IOCTL_CLOSE;
    transaction.file_id = file_id;
    transaction.status = status;
    self->send(self, &transaction);
}


/** @} */

EMBC_CPP_GUARD_END

#endif /* EMBC_STREAM_ASYNC_H_ */
