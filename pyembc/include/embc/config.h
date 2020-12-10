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
 * @brief EMBC configuration.
 */

#ifndef EMBC_CONFIG_H_
#define EMBC_CONFIG_H_

/**
 * @ingroup embc
 * @defgroup embc_config Configuration
 *
 * @brief EMBC configuration.
 *
 * @{
 */


/* Set global log level */
#define EMBC_LOG_GLOBAL_LEVEL EMBC_LOG_LEVEL_ALL

/* Override the log format */
/*
#define EMBC_LOG_PRINTF(level, format, ...) \
   embc_log_printf_("%c %s:%d: " format "\n", embc_log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);
#endif
*/


#define EMBC_PLATFORM_STDLIB 1

#define EMBC_VERSION_MAJOR   0
#define EMBC_VERSION_MINOR   2
#define EMBC_VERSION_PATCH   1
#define EMBC_VERSION_U32     ((uint32_t) ( \
    ((EMBC_VERSION_MAJOR & 0xff) << 24) | \
    ((EMBC_VERSION_MINOR & 0xff) << 16) | \
    (EMBC_VERSION_PATCH & 0xffff) ))
#define EMBC_VERSION_STR     "0.2.1"

#define EMBC_CSTR_FLOAT_ENABLE 0

/** @} */

#endif /* EMBC_CONFIG_H_ */

