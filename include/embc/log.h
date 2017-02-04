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

EMBC_CPP_GUARD_START


/**
 * @ingroup embc
 * @defgroup embc_log Console logging
 *
 * @brief Generic console logging with compile-time levels.
 *
 * To use this module, define log_printf in you application.
 *
 * @{
 */

/**
 * @def LOG_GLOBAL_LEVEL
 *
 * @brief The global logging level.
 *
 * The maximum level to compile regardless of the individual module level.
 */
#ifndef LOG_GLOBAL_LEVEL
#define LOG_GLOBAL_LEVEL LOG_LEVEL_ALL
#endif

/**
 * @def LOG_LEVEL
 *
 * @brief The module logging level.
 *
 * Define LOG_LEVEL before including this file.
 */
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
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
 *     void log_printf(const char * format, ...) {
 *         va_list arg;
 *         va_start(arg, format);
 *         vprintf(format, arg);
 *         va_end(arg);
 *     }
 */
typedef void(*log_printf)(const char * format, ...);

extern log_printf log_printf_;

/**
 * @brief Initialize the logging feature.
 *
 * @param handler The log handler.  Pass NULL or call log_finalize() to
 *      restore the default log handler.
 *
 * @return 0 or error code.
 *
 * The library initializes with a default null log handler so that logging
 * which occurs before log_initialize will not cause a fault.  This function
 * may be safely called at any time, even without finalize.
 */
EMBC_API int log_initialize(log_printf handler);

/**
 * @brief Finalize the logging feature.
 *
 * This is equivalent to calling log_initialize(0).
 */
EMBC_API void log_finalize();

/**
 * @brief The printf function including log formatting.
 */
#define LOG_PRINTF(level, format, ...) \
    log_printf_("%c %s:%d: " format "\n", log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);

/**
 * @brief The available logging levels.
 */
enum log_level_e {
    /** Logging functionality is disabled. */
    LOG_LEVEL_OFF         = -1,
    /** A "panic" condition that may result in significant harm. */
    LOG_LEVEL_EMERGENCY   = 0,
    /** A condition requiring immediate action. */
    LOG_LEVEL_ALERT       = 1,
    /** A critical error which prevents further functions. */
    LOG_LEVEL_CRITICAL    = 2,
    /** An error which prevents the current operation from completing or
     *  will adversely effect future functionality. */
    LOG_LEVEL_ERROR       = 3,
    /** A warning which may adversely affect the current operation or future
     *  operations. */
    LOG_LEVEL_WARN        = 4,
    /** A notification for interesting events. */
    LOG_LEVEL_NOTICE      = 5,
    /** An informative message. */
    LOG_LEVEL_INFO        = 6,
    /** Detailed messages for the software developer. */
    LOG_LEVEL_DEBUG       = 7,
    /** Very detailed messages for the software developer. */
    LOG_LEVEL_DEBUG2      = 8,
    /** Insanely detailed messages for the software developer. */
    LOG_LEVEL_DEBUG3      = 9,
    /** All logging functionality is enabled. */
    LOG_LEVEL_ALL         = 10,
};

/**
 * @brief Map log level to a string name.
 */
extern char const * const log_level_str[LOG_LEVEL_ALL + 1];

/**
 * @brief Map log level to a single character.
 */
extern char const log_level_char[LOG_LEVEL_ALL + 1];

/**
 * @brief Check the current level against the static logging configuration.
 *
 * @param level The level to query.
 * @return True if logging at level is permitted.
 */
#define LOG_CHECK_STATIC(level) ((level <= LOG_GLOBAL_LEVEL) && (level <= LOG_LEVEL) && (level >= 0))

/**
 * @brief Check a log level against a configured level.
 *
 * @param level The level to query.
 * @param cfg_level The configured logging level.
 * @return True if level is permitted given cfg_level.
 */
#define LOG_LEVEL_CHECK(level, cfg_level) (level <= cfg_level)


/*!
 * \brief Macro to log a printf-compatible formatted string.
 *
 * \param level The log_level_e.
 * \param format The printf-compatible formatting string.
 * \param ... The arguments to the formatting string.
 */
#define LOGF(level, format, ...) \
    if (LOG_CHECK_STATIC(level)) { \
        LOG_PRINTF(level, format, __VA_ARGS__); \
    }

/*!
 * \brief Macro to log a string.
 *
 * \param level The log_level_e.
 * \param msg The string to log.
 */
#define LOGS(level, msg) \
    if (LOG_CHECK_STATIC(level)) { \
        LOG_PRINTF(level, "%s", msg); \
    }

/*!
 * \brief Macro to log a character.
 *
 * \param level The log_level_e.
 * \param c The character to log.
 */
#define LOGC(level, c) \
    if (LOG_CHECK_STATIC(level)) { \
        LOG_PRINTF(level, "%c", c); \
    }


/**
 * \brief The logging levels available to the system.
 */

/** Log a critical failure using printf-style arguments. */
#define LOGF_CRITICAL(format, ...)  LOGF(LOG_LEVEL_CRITICAL, format, __VA_ARGS__)
/** Log an error using printf-style arguments.  Same as LOGF_ERR. */
#define LOGF_ERR(format, ...)       LOGF(LOG_LEVEL_ERROR,    format, __VA_ARGS__)
/** Log an error using printf-style arguments. */
#define LOGF_ERROR(format, ...)     LOGF(LOG_LEVEL_ERROR,    format, __VA_ARGS__)
/** Log an warning using printf-style arguments. */
#define LOGF_WARN(format, ...)      LOGF(LOG_LEVEL_WARN,     format, __VA_ARGS__)
/** Log an warning using printf-style arguments.  Same as LOGF_WARN. */
#define LOGF_WARNING(format, ...)   LOGF(LOG_LEVEL_WARN,     format, __VA_ARGS__)
/** Log a notice using printf-style arguments. */
#define LOGF_NOTICE(format, ...)    LOGF(LOG_LEVEL_NOTICE,   format, __VA_ARGS__)
/** Log an informative message using printf-style arguments. */
#define LOGF_INFO(format, ...)      LOGF(LOG_LEVEL_INFO,     format, __VA_ARGS__)
/** Log a detailed debug message using printf-style arguments.  Same as LOGF_DEBUG. */
#define LOGF_DBG(format, ...)       LOGF(LOG_LEVEL_DEBUG,    format, __VA_ARGS__)
/** Log a detailed debug message using printf-style arguments. */
#define LOGF_DEBUG(format, ...)     LOGF(LOG_LEVEL_DEBUG,    format, __VA_ARGS__)
/** Log a very detailed debug message using printf-style arguments. */
#define LOGF_DEBUG2(format, ...)     LOGF(LOG_LEVEL_DEBUG2,  format, __VA_ARGS__)
/** Log an insanely detailed debug message using printf-style arguments. */
#define LOGF_DEBUG3(format, ...)     LOGF(LOG_LEVEL_DEBUG3,  format, __VA_ARGS__)

/** Log a critical failure string. */
#define LOGS_CRITICAL(msg)  LOGS(LOG_LEVEL_CRITICAL, msg)
/** Log an error string.  Same as LOGS_ERR. */
#define LOGS_ERR(msg)       LOGS(LOG_LEVEL_ERROR,    msg)
/** Log an error string. */
#define LOGS_ERROR(msg)     LOGS(LOG_LEVEL_ERROR,    msg)
/** Log an warning string. */
#define LOGS_WARN(msg)      LOGS(LOG_LEVEL_WARN,     msg)
/** Log an warning string.  Same as LOGS_WARN. */
#define LOGS_WARNING(msg)   LOGS(LOG_LEVEL_WARN,     msg)
/** Log a notice string. */
#define LOGS_NOTICE(msg)    LOGS(LOG_LEVEL_NOTICE,   msg)
/** Log an informative message string. */
#define LOGS_INFO(msg)      LOGS(LOG_LEVEL_INFO,     msg)
/** Log a detailed debug message string.  Same as LOGS_DEBUG. */
#define LOGS_DBG(msg)       LOGS(LOG_LEVEL_DEBUG,    msg)
/** Log a detailed debug message string. */
#define LOGS_DEBUG(msg)     LOGS(LOG_LEVEL_DEBUG,    msg)
/** Log a very detailed debug message string. */
#define LOGS_DEBUG2(msg)     LOGS(LOG_LEVEL_DEBUG2,  msg)
/** Log an insanely detailed debug message string. */
#define LOGS_DEBUG3(msg)     LOGS(LOG_LEVEL_DEBUG3,  msg)

/** Log a critical failure character. */
#define LOGC_CRITICAL(c)    LOGC(LOG_LEVEL_CRITICAL, c)
/** Log an error character.  Same as LOGC_ERR. */
#define LOGC_ERR(c)         LOGC(LOG_LEVEL_ERROR,    c)
/** Log an error character. */
#define LOGC_ERROR(c)       LOGC(LOG_LEVEL_ERROR,    c)
/** Log a warning character. */
#define LOGC_WARN(c)        LOGC(LOG_LEVEL_WARN,     c)
/** Log a warning character.  Same as LOGC_WARN. */
#define LOGC_WARNING(c)     LOGC(LOG_LEVEL_WARN,     c)
/** Log a notice character. */
#define LOGC_NOTICE(c)      LOGC(LOG_LEVEL_NOTICE,   c)
/** Log an informative character. */
#define LOGC_INFO(c)        LOGC(LOG_LEVEL_INFO,     c)
/** Log a debug character.  Same as LOGC_DEBUG. */
#define LOGC_DBG(c)         LOGC(LOG_LEVEL_DEBUG,    c)
/** Log a debug character. */
#define LOGC_DEBUG(c)       LOGC(LOG_LEVEL_DEBUG,    c)
/** Log a very detailed debug character. */
#define LOGC_DEBUG2(c)       LOGC(LOG_LEVEL_DEBUG2,  c)
/** Log an insanely detailed debug character. */
#define LOGC_DEBUG3(c)       LOGC(LOG_LEVEL_DEBUG3,  c)

/** @} */

EMBC_CPP_GUARD_END

#endif /* EMBC_LOG_H_ */
