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
 * @brief Message framer utilities for byte streams.
 */

#ifndef EMBC_STREAM_FRAMER_UTIL_H_
#define EMBC_STREAM_FRAMER_UTIL_H_

#include "embc/cmacro_inc.h"
#include <stdint.h>

/**
 * @ingroup embc
 * @defgroup embc_framer_util Message framing utilities
 *
 * @brief Support the message framing module.
 *
 * @{
 */

EMBC_CPP_GUARD_START

/**
 * @brief Validate a frame.
 *
 * @param buffer The buffer containing the frame.
 * @param buffer_length The size of buffer in bytes.
 * @param frame_length The size of the validated frame.
 * @return 0, EMBC_ERROR_TOO_SMALL, EMBC_ERROR_IO,
 *      EMBC_ERROR_MESSAGE_INTEGRITY.
 */
EMBC_API int32_t embc_framer_validate(uint8_t const * buffer,
                                      uint16_t buffer_length,
                                      uint16_t * frame_length);


EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_STREAM_FRAMER_H_ */
