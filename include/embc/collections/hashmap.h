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

#ifndef EMBC_HASHMAP_H_
#define EMBC_HASHMAP_H_

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
struct hashmap_s;

/**
 * @brief The hashmap iterator class (opaque).
 */
struct hashmap_iterator_s;

/**
 * @brief Hash a value.
 *
 * @param value The value to hash.
 * @return The hash.
 */
typedef embc_size_t (*hashmap_hash)(void * value);

/**
 * @brief Test two values for equivalence.
 *
 * @param self The first value to compare
 * @param other The other value to compare
 * @return True if equivalent, false otherwise.
 */
typedef bool (*hashmap_equiv)(void * self, void * other);


struct hashmap_s * hashmap_new(hashmap_hash hash, hashmap_equiv equiv);
void hashmap_free(struct hashmap_s * self);
embc_size_t hashmap_length(struct hashmap_s * self);
int hashmap_put(struct hashmap_s * self, void * key, void * value, void ** old_value);
int hashmap_get(struct hashmap_s * self, void * key, void ** value);
int hashmap_remove(struct hashmap_s * self, void * key, void ** old_value);

struct hashmap_iterator_s * hashmap_iterator_new(struct hashmap_s * self);
int hashmap_iterator_next(struct hashmap_iterator_s * self, void ** key, void ** value);
void hashmap_iterator_free(struct hashmap_iterator_s * self);

/** @} */


EMBC_CPP_GUARD_END

#endif /* EMBC_HASHMAP_H_ */
