/*
 * Copyright 2014-2018 Jetperch LLC
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
 * \file
 *
 * \brief strmap collection.
 */

#ifndef EMBC_STRMAP_H_
#define EMBC_STRMAP_H_

#include "embc/cmacro_inc.h"
#include "embc/platform.h"

EMBC_CPP_GUARD_START

/**
 * \ingroup embc
 * \defgroup embc_strmap Map a string keys to an arbitrary objects
 *
 * \brief Store a mapping of keys to values.
 *
 * @{
 */

/**
 * @brief The strmap class (opaque).
 */
struct embc_strmap_s;

struct embc_strmap_s * embc_strmap_new();
void embc_strmap_free(struct embc_strmap_s *self);
embc_size_t embc_strmap_length(struct embc_strmap_s *self);
int embc_strmap_put(struct embc_strmap_s *self, const char *key, void *value, void **old_value);
int embc_strmap_get(struct embc_strmap_s *self, const char *key, void **value);
int embc_strmap_remove(struct embc_strmap_s *self, const char *key, void **old_value);


/** @} */


EMBC_CPP_GUARD_END

#endif /* EMBC_STRMAP_H_ */
