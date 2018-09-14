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

/*!
 * \file
 *
 * \brief Trivial logging support.
 */

#ifndef EMBC_LOG_H_
#define EMBC_LOG_H_

#include "cmacro_inc.h"
#include "embc/config.h"

EMBC_CPP_GUARD_START


/**
 * @ingroup embc
 * @defgroup embc_log Console logging
 *
 * @brief Generic console logging with compile-time levels.
 *
 * To use this module, call embc_log_initialize() with the appropriate
 * handler for your application.
 *
 * @{
 */

/**
 * @def EMBC_LOG_GLOBAL_LEVEL
 *
 * @brief The global logging level.
 *
 * The maximum level to compile regardless of the individual module level.
 * This value should be defined in the project CMake (makefile).
 */
#ifndef EMBC_LOG_GLOBAL_LEVEL
#define EMBC_LOG_GLOBAL_LEVEL EMBC_LOG_LEVEL_ALL
#endif

/**
 * @def EMBC_LOG_LEVEL
 *
 * @brief The module logging level.
 *
 * Typical usage 1:  (not MISRA C compliant, but safe)
 *
 *      #define EMBC_LOG_LEVEL EMBC_LOG_LEVEL_WARNING
 *      #include "log.h"
 */
#ifndef EMBC_LOG_LEVEL
#define EMBC_LOG_LEVEL EMBC_LOG_LEVEL_INFO
#endif

/**
 * @def __FILENAME__
 *
 * @brief The filename to display for logging.
 *
 * When compiling C and C++ code, the __FILE__ define may contain a long path
 * that just confuses the log output.  The build tools, such as make and cmake,
 * can define __FILENAME__ to produce more meaningful results.
 *
 * A good Makefile usage includes:
 *
 */
#ifndef __FILENAME__
#define __FILENAME__ __FILE__
#endif

#ifdef __GNUC__
/* https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Function-Attributes.html */
#define EMBC_LOG_PRINTF_FORMAT __attribute__((format (printf, 1, 2)))
#else
#define EMBC_LOG_PRINTF_FORMAT
#endif

/**
 * @brief The printf-style function.
 *
 * @param format The print-style formatting string.
 * The remaining parameters are arguments for the formatting string.
 * @return The number of characters printed.
 *
 * For PC-based applications, a common implementation is::
 *
 *     #include <stdarg.h>
 *     #include <stdio.h>
 *
 *     void embc_log_printf(const char * format, ...) {
 *         va_list arg;
 *         va_start(arg, format);
 *         vprintf(format, arg);
 *         va_end(arg);
 *     }
 *
 * If your application calls the LOG* macros from multiple threads, then
 * the embc_log_printf implementation must be thread-safe and reentrant.
 */
typedef void(*embc_log_printf)(const char * format, ...) EMBC_LOG_PRINTF_FORMAT;

extern volatile embc_log_printf embc_log_printf_;

/**
 * @brief Initialize the logging feature.
 *
 * @param handler The log handler.  Pass NULL or call embc_log_finalize() to
 *      restore the default log handler.
 *
 * @return 0 or error code.
 *
 * The library initializes with a default null log handler so that logging
 * which occurs before embc_log_initialize will not cause a fault.  This function
 * may be safely called at any time, even without finalize.
 */
EMBC_API int embc_log_initialize(embc_log_printf handler);

/**
 * @brief Finalize the logging feature.
 *
 * This is equivalent to calling embc_log_initialize(0).
 */
EMBC_API void embc_log_finalize();

/**
 * @def EMBC_LOG_PRINTF
 * @brief The printf function including log formatting.
 *
 * @param level The level for this log message
 * @param format The formatting string
 * @param ... The arguments for the formatting string
 */
#ifndef EMBC_LOG_PRINTF
#define EMBC_LOG_PRINTF(level, format, ...) \
    embc_log_printf_("%c %s:%d: " format "\n", embc_log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);
#endif

/**
 * @brief The available logging levels.
 */
enum embc_log_level_e {
    /** Logging functionality is disabled. */
    EMBC_LOG_LEVEL_OFF         = -1,
    /** A "panic" condition that may result in significant harm. */
    EMBC_LOG_LEVEL_EMERGENCY   = 0,
    /** A condition requiring immediate action. */
    EMBC_LOG_LEVEL_ALERT       = 1,
    /** A critical error which prevents further functions. */
    EMBC_LOG_LEVEL_CRITICAL    = 2,
    /** An error which prevents the current operation from completing or
     *  will adversely effect future functionality. */
    EMBC_LOG_LEVEL_ERROR       = 3,
    /** A warning which may adversely affect the current operation or future
     *  operations. */
    EMBC_LOG_LEVEL_WARNING     = 4,
    /** A notification for interesting events. */
    EMBC_LOG_LEVEL_NOTICE      = 5,
    /** An informative message. */
    EMBC_LOG_LEVEL_INFO        = 6,
    /** Detailed messages for the software developer. */
    EMBC_LOG_LEVEL_DEBUG1      = 7,
    /** Very detailed messages for the software developer. */
    EMBC_LOG_LEVEL_DEBUG2      = 8,
    /** Insanely detailed messages for the software developer. */
    EMBC_LOG_LEVEL_DEBUG3      = 9,
    /** All logging functionality is enabled. */
    EMBC_LOG_LEVEL_ALL         = 10,
};

/** Detailed messages for the software developer. */
#define EMBC_LOG_LEVEL_DEBUG EMBC_LOG_LEVEL_DEBUG1

/**
 * @brief Map log level to a string name.
 */
extern char const * const embc_log_level_str[EMBC_LOG_LEVEL_ALL + 1];

/**
 * @brief Map log level to a single character.
 */
extern char const embc_log_level_char[EMBC_LOG_LEVEL_ALL + 1];

/**
 * @brief Check the current level against the static logging configuration.
 *
 * @param level The level to query.
 * @return True if logging at level is permitted.
 */
#define EMBC_LOG_CHECK_STATIC(level) ((level <= EMBC_LOG_GLOBAL_LEVEL) && (level <= EMBC_LOG_LEVEL) && (level >= 0))

/**
 * @brief Check a log level against a configured level.
 *
 * @param level The level to query.
 * @param cfg_level The configured logging level.
 * @return True if level is permitted given cfg_level.
 */
#define EMBC_LOG_LEVEL_CHECK(level, cfg_level) (level <= cfg_level)

/*!
 * \brief Macro to log a printf-compatible formatted string.
 *
 * \param level The embc_log_level_e.
 * \param format The printf-compatible formatting string.
 * \param ... The arguments to the formatting string.
 */
#define EMBC_LOG(level, format, ...) \
    do { \
        if (EMBC_LOG_CHECK_STATIC(level)) { \
            EMBC_LOG_PRINTF(level, format, __VA_ARGS__); \
        } \
    } while (0)


// zero length variadic arguments are not allowed for macros
// this hack ensures that LOG(message) and LOG(format, args...) are both supported.
// https://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick
#define _EMBC_LOG_SELECT(PREFIX, _9, _8, _7, _6, _5, _4, _3, _2, _1, SUFFIX, ...) PREFIX ## _ ## SUFFIX
#define _EMBC_LOG_1(level, message) EMBC_LOG(level, "%s", message)
#define _EMBC_LOG_N(level, format, ...) EMBC_LOG(level, format, __VA_ARGS__)
#define _EMBC_LOG_DISPATCH(level, ...)  _EMBC_LOG_SELECT(_EMBC_LOG, __VA_ARGS__, N, N, N, N, N, N, N, N, 1, 0)(level, __VA_ARGS__)


/** Log a emergency using printf-style arguments. */
#define EMBC_LOG_EMERGENCY(...)  _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_EMERGENCY, __VA_ARGS__)
/** Log a alert using printf-style arguments. */
#define EMBC_LOG_ALERT(...)  _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_ALERT, format, __VA_ARGS__)
/** Log a critical failure using printf-style arguments. */
#define EMBC_LOG_CRITICAL(...)  _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_CRITICAL, __VA_ARGS__)
/** Log an error using printf-style arguments. */
#define EMBC_LOG_ERROR(...)     _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_ERROR, __VA_ARGS__)
/** Log an error using printf-style arguments.  Alias for EMBC_LOG_ERROR. */
#define EMBC_LOG_ERR EMBC_LOG_ERROR
/** Log a warning using printf-style arguments. */
#define EMBC_LOG_WARNING(...)      _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_WARNING, __VA_ARGS__)
/** Log a warning using printf-style arguments.  Alias for EMBC_LOG_WARNING. */
#define EMBC_LOG_WARN EMBC_LOG_WARNING
/** Log a notice using printf-style arguments. */
#define EMBC_LOG_NOTICE(...)    _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_NOTICE,   __VA_ARGS__)
/** Log an informative message using printf-style arguments. */
#define EMBC_LOG_INFO(...)      _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_INFO,     __VA_ARGS__)
/** Log a detailed debug message using printf-style arguments. */
#define EMBC_LOG_DEBUG1(...)    _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_DEBUG1,    __VA_ARGS__)
/** Log a detailed debug message using printf-style arguments.  Alias for EMBC_LOG_DEBUG1. */
#define EMBC_LOG_DEBUG EMBC_LOG_DEBUG1
/** Log a detailed debug message using printf-style arguments.  Alias for EMBC_LOG_DEBUG1. */
#define EMBC_LOG_DBG EMBC_LOG_DEBUG1
/** Log a very detailed debug message using printf-style arguments. */
#define EMBC_LOG_DEBUG2(...)    _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_DEBUG2,  __VA_ARGS__)
/** Log an insanely detailed debug message using printf-style arguments. */
#define EMBC_LOG_DEBUG3(...)    _EMBC_LOG_DISPATCH(EMBC_LOG_LEVEL_DEBUG3,  __VA_ARGS__)


#define EMBC_LOGE EMBC_LOG_ERROR
#define EMBC_LOGW EMBC_LOG_WARNING
#define EMBC_LOGN EMBC_LOG_NOTICE
#define EMBC_LOGI EMBC_LOG_INFO
#define EMBC_LOGD EMBC_LOG_DEBUG1
#define EMBC_LOGD1 EMBC_LOG_DEBUG1
#define EMBC_LOGD2 EMBC_LOG_DEBUG2
#define EMBC_LOGD3 EMBC_LOG_DEBUG3

/** @} */

EMBC_CPP_GUARD_END

#endif /* EMBC_LOG_H_ */
