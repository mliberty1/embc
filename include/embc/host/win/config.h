/*
 * Copyright 2021 Jetperch LLC
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
 * @brief Default windows host configuration.
 */

#ifndef EMBC_HOST_WIN_CONFIG_H__
#define EMBC_HOST_WIN_CONFIG_H__

#include "embc/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMBC_PLATFORM_STDLIB 1
#define EMBC_CSTR_FLOAT_ENABLE 0
typedef void * embc_os_mutex_t;

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_HOST_WIN_CONFIG_H__ */


