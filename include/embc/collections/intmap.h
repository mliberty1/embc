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
 * \brief Provide a map from integers to void *.
 */

#ifndef EMBC_COLLECTIONS_INTMAP_H_
#define EMBC_COLLECTIONS_INTMAP_H_

#include "embc/cmacro_inc.h"
#include "embc/platform.h"

EMBC_CPP_GUARD_START

/**
 * @ingroup embc
 * \defgroup embc_intmap intmap
 *
 * \brief Provide a map from integers to void *.
 *
 * @{
 */

/**
 * @brief The intmap class (opaque).
 *
 * This container class is an optimized hashmap for storing integer keys.
 */
struct embc_intmap_s;

/**
 * @brief The intmap iterator class (opaque).
 */
struct embc_intmap_iterator_s;

/**
 * @brief Create a new intmap.
 *
 * @return The new instance.  This function asserts on out of memory, so
 *      the return value is guaranteed to be valid.  For platforms which
 *      support free, embc_intmap_free() when done with this instance.
 */
EMBC_API struct embc_intmap_s * embc_intmap_new();

/**
 * @brief Free an intmap instance.
 *
 * @param self The intmap instance to free.
 */
EMBC_API void embc_intmap_free(struct embc_intmap_s * self);

/**
 * @brief Get the number of items in the container.
 *
 * @param self The container.
 * @return The number of items in the container.
 */
EMBC_API embc_size_t embc_intmap_length(struct embc_intmap_s * self);

/**
 * @brief Put an item into the container
 *
 * @param[in] self The container.
 * @param[in] key The key for the item.
 * @param[in] value The value for the item.  The value must remain valid until
 *      removed from the container, either by embc_intmap_put() with the same key,
 *      embc_intmap_remove() with the same key or embc_intmap_freee().
 * @param[out] old_value The previous value for the item, if this key already
 *      existed.  The caller is responsible for managing this pointer.
 * @return 0 or error code.
 */
EMBC_API int embc_intmap_put(struct embc_intmap_s * self, embc_size_t key, void * value, void ** old_value);

/**
 * @brief Get an item from the container
 *
 * @param[in] self The container.
 * @param[in] key The key for the item.
 * @param[out] value The value for the item.
 * @return 0, EMBC_ERROR_NOT_FOUND or error code.
 */
EMBC_API int embc_intmap_get(struct embc_intmap_s * self, embc_size_t key, void ** value);

/**
 * @brief Remove an item from the container
 *
 * @param[in] self The container.
 * @param[in] key The key for the item.
 * @param[out] old_value The existing value for the item.  The caller is
 *      responsible for managing this pointer.
 * @return 0, EMBC_ERROR_NOT_FOUND or error code.
 */
EMBC_API int embc_intmap_remove(struct embc_intmap_s * self, embc_size_t key, void ** old_value);

/**
 * @brief Create an iterator the traverses all items in the container.
 *
 * @param[in] self The container.
 * @return The iterator which must be freed using embc_intmap_iterator_free().
 *
 * Both embc_intmap_put and embc_intmap_remove operations are allowed on the container
 * while an iterator is active.  Items added using embc_intmap_put may or may not
 * be included in the traversal.
 */
struct embc_intmap_iterator_s * embc_intmap_iterator_new(struct embc_intmap_s * self);

/**
 * @brief Get the next item from the container iterator.
 *
 * @param[in] self The iterator.
 * @param[out] key The key for the next item.
 * @param[out] value The value for the next item.
 * @return 0, EMBC_ERROR_NOT_FOUND at end or an error.
 */
EMBC_API int embc_intmap_iterator_next(struct embc_intmap_iterator_s * self, embc_size_t * key, void ** value);

/**
 * @brief Free an iterator.
 *
 * @param[in] self The iterator.
 */
EMBC_API void embc_intmap_iterator_free(struct embc_intmap_iterator_s * self);

/**
 * @brief Declare an custom intmap with a specified value type.
 *
 * @param name The name for the intmap.
 * @param type The value type.
 */
#define EMBC_INTMAP_DECLARE(name, type) \
    struct name##_s; \
    struct name##_iterator_s; \
    struct name##_s * name##_new(); \
    void name##_free(struct name##_s * self); \
    embc_size_t name##_length(struct name##_s * self); \
    int name##_put(struct name##_s * self, embc_size_t key, type value, type * old_value); \
    int name##_get(struct name##_s * self, embc_size_t key, type * value); \
    int name##_remove(struct name##_s * self, embc_size_t key, type * old_value); \
    struct name##_iterator_s * name##_iterator_new(struct name##_s * self); \
    int name##_iterator_next(struct name##_iterator_s * self, embc_size_t * key, type * value); \
    void name##_iterator_free(struct name##_iterator_s * self);

/**
 * @brief Define the default structure.
 *
 * @param name The name for the intmap.
 *
 * If this macro is not used, ensure to include struct embc_intmap_s * intmap
 * in your structure definition.
 */
#define EMBC_INTMAP_DEFINE_STRUCT(name) \
    struct name##_s { struct embc_intmap_s * intmap; };

/**
 * @brief Define an custom intmap with a specified value type.
 *
 * @param name The name for the intmap.
 * @param type The value type.
 */
#define EMBC_INTMAP_DEFINE(name, type) \
    struct name##_iterator_s { struct embc_intmap_iterator_s * iter; }; \
    struct name##_s * name##_new() { \
        struct name##_s * self = calloc(1, sizeof(struct name##_s)); \
        self->intmap = embc_intmap_new(); \
        return self; \
    } \
    void name##_free(struct name##_s * self) { embc_intmap_free(self->intmap); } \
    embc_size_t name##_length(struct name##_s * self) { return embc_intmap_length(self->intmap); } \
    int name##_put(struct name##_s * self, embc_size_t key, type value, type * old_value) { \
        return embc_intmap_put(self->intmap, key, (void *) value, (void **) old_value); \
    } \
    int name##_get(struct name##_s * self, embc_size_t key, type * value) { \
        return embc_intmap_get(self->intmap, key, (void **) value); \
    } \
    int name##_remove(struct name##_s * self, embc_size_t key, type * old_value) { \
        return embc_intmap_remove(self->intmap, key, (void **) old_value); \
    } \
    struct name##_iterator_s * name##_iterator_new(struct name##_s * self) { \
        struct name##_iterator_s * iter = calloc(1, sizeof(struct name##_iterator_s)); \
        iter->iter = embc_intmap_iterator_new(self->intmap); \
        return iter; \
    } \
    int name##_iterator_next(struct name##_iterator_s * self, embc_size_t * key, type * value) { \
        return embc_intmap_iterator_next(self->iter, key, (void *) value); \
    } \
    void name##_iterator_free(struct name##_iterator_s * self) { \
        embc_intmap_iterator_free(self->iter); \
    }

/** @} */


EMBC_CPP_GUARD_END

#endif /* EMBC_COLLECTIONS_INTMAP_H_ */
