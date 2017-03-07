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
 * @brief Standard embc status and error codes.
 */

#ifndef EMBC_EC_H_
#define EMBC_EC_H_

#include "embc/cmacro_inc.h"

/**
 * @ingroup embc
 * @defgroup embc_ec Error codes
 *
 * @brief Standardize error code definitions.
 *
 * See <a href="http://www.cplusplus.com/reference/system_error/errc/">errc</a>
 *
 * @{
 */


/**
 * @brief The list of error codes for use by X macros.
 *
 * @see https://en.wikipedia.org/wiki/X_Macro
 * @see http://www.drdobbs.com/cpp/the-x-macro/228700289
 */
#define EMBC_ERROR_CODES(X) \
    X(SUCCESS,                  "Success (no error)") \
    X(UNSPECIFIED,              "Unspecified error") \
    X(NOT_ENOUGH_MEMORY,        "Insufficient memory to complete the operation") \
    X(NOT_SUPPORTED,            "Operation is not supported") \
    X(IO,                       "Input/output error") \
    X(PARAMETER_INVALID,        "The parameter value is invalid") \
    X(INVALID_RETURN_CONDITION, "The function return condition is invalid") \
    X(INVALID_CONTEXT,          "The context is invalid") \
    X(INVALID_MESSAGE_LENGTH,   "The message length in invalid") \
    X(MESSAGE_INTEGRITY,        "The message integrity check failed") \
    X(SYNTAX_ERROR,             "A syntax error was detected") \
    X(TIMED_OUT,                "The operation did not complete in time") \
    X(FULL,                     "The target of the operation is full") \
    X(EMPTY,                    "The target of the operation is empty") \
    X(TOO_SMALL,                "The target of the operation is too small") \
    X(TOO_BIG,                  "The target of the operation is too big") \
    X(NOT_FOUND,                "The requested resource was not found") \
    X(ALREADY_EXISTS,           "The requested resource already exists") \
    X(PERMISSIONS,              "Insufficient permissions to perform the operation.") \
    X(BUSY,                     "The requested resource is currently busy.") \
    X(UNAVAILABLE,              "The requested resource is currently unavailable.") \
    X(IN_USE,                   "The requested resource is currently in use.") \
    X(CLOSED,                   "The requested resource is currently closed.") \
    X(SEQUENCE,                 "The requested operation was out of sequence.") \
    X(ABORTED,                  "The requested operation was previously aborted.") \
    X(SYNCHRONIZATION,          "The target is not synchronized with the originator.") \


#define EMBC_ERROR_ENUM(NAME, TEXT) EMBC_ERROR_ ## NAME,

/**
 * @brief The list of error codes.
 */
enum embc_error_code_e {
    EMBC_ERROR_CODES(EMBC_ERROR_ENUM)
    EMBC_ERROR_CODE_COUNT
};

#define EMBC_SUCCESS EMBC_ERROR_SUCCESS


EMBC_CPP_GUARD_START

/**
 * @brief Convert an error code into its short name.
 *
 * @param[in] ec The error code (embc_error_code_e).
 * @return The short string name for the error code.
 */
EMBC_API const char * embc_error_code_name(int ec);

/**
 * @brief Convert an error code into its description.
 *
 * @param[in] ec The error code (embc_error_code_e).
 * @return The user-meaningful description of the error.
 */
EMBC_API const char * embc_error_code_description(int ec);


EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_EC_H_ */
