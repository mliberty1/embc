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
 * \file
 *
 * \brief hashmap collection.
 */

#ifndef EMBC_COLLECTIONS_HASHMAP_H_
#define EMBC_COLLECTIONS_HASHMAP_H_

#include "embc/cmacro_inc.h"
#include "embc/platform.h"

EMBC_CPP_GUARD_START

/**
 * \ingroup embc
 * \defgroup embc_hashmap Hashmap
 *
 * \brief Store a mapping of keys to values.
 *
 * @{
 */

/**
 * @brief The hashmap class (opaque).
 */
struct embc_hashmap_s;

/**
 * @brief The hashmap iterator class (opaque).
 */
struct embc_hashmap_iterator_s;

/**
 * @brief Hash a value.
 *
 * @param value The value to hash.
 * @return The hash.
 */
typedef embc_size_t (*embc_hashmap_hash)(void * value);

/**
 * @brief Test two values for equivalence.
 *
 * @param self The first value to compare
 * @param other The other value to compare
 * @return True if equivalent, false otherwise.
 */
typedef bool (*embc_hashmap_equiv)(void * self, void * other);


EMBC_API struct embc_hashmap_s * embc_hashmap_new(embc_hashmap_hash hash, embc_hashmap_equiv equiv);
EMBC_API void embc_hashmap_free(struct embc_hashmap_s * self);
EMBC_API embc_size_t embc_hashmap_length(struct embc_hashmap_s * self);
EMBC_API int embc_hashmap_put(struct embc_hashmap_s * self, void * key, void * value, void ** old_value);
EMBC_API int embc_hashmap_get(struct embc_hashmap_s * self, void * key, void ** value);
EMBC_API int embc_hashmap_remove(struct embc_hashmap_s * self, void * key, void ** old_value);

EMBC_API struct embc_hashmap_iterator_s * embc_hashmap_iterator_new(struct embc_hashmap_s * self);
EMBC_API int embc_hashmap_iterator_next(struct embc_hashmap_iterator_s * self, void ** key, void ** value);
EMBC_API void embc_hashmap_iterator_free(struct embc_hashmap_iterator_s * self);

/** @} */


EMBC_CPP_GUARD_END

#endif /* EMBC_COLLECTIONS_HASHMAP_H_ */
