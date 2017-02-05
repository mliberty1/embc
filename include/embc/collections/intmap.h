/* Copyright 2015-2016 Jetperch LLC */

/**
 * \file
 *
 * \brief Provide a map from integers to void *.
 */

#ifndef INTMAP_H
#define INTMAP_H

#include "common/cdef.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

CPP_GUARD_START

/**
 * \defgroup intmap intmap
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
struct intmap_s;

/**
 * @brief The intmap iterator class (opaque).
 */
struct intmap_iterator_s;

/**
 * @brief Create a new intmap.
 *
 * @return The new instance or 0 on error.  Use intmap_free() when done with
 *      this instance.
 */
struct intmap_s * intmap_new();

/**
 * @brief Free an intmap instance.
 *
 * @param self The intmap instance to free.
 */
void intmap_free(struct intmap_s * self);

/**
 * @brief Get the number of items in the container.
 *
 * @param self The container.
 * @return The number of items in the container.
 */
size_t intmap_length(struct intmap_s * self);

/**
 * @brief Put an item into the container
 *
 * @param[in] self The container.
 * @param[in] key The key for the item.
 * @param[in] value The value for the item.  The value must remain valid until
 *      removed from the container, either by intmap_put() with the same key,
 *      intmap_remove() with the same key or intmap_freee().
 * @param[out] old_value The previous value for the item, if this key already
 *      existed.  The caller is responsible for managing this pointer.
 * @return 0 or error code.
 */
int intmap_put(struct intmap_s * self, size_t key, void * value, void ** old_value);

/**
 * @brief Get an item from the container
 *
 * @param[in] self The container.
 * @param[in] key The key for the item.
 * @param[out] value The value for the item.
 * @return 0, JETLEX_ERROR_NOT_FOUND or error code.
 */
int intmap_get(struct intmap_s * self, size_t key, void ** value);

/**
 * @brief Remove an item from the container
 *
 * @param[in] self The container.
 * @param[in] key The key for the item.
 * @param[out] old_value The existing value for the item.  The caller is
 *      responsible for managing this pointer.
 * @return 0, JETLEX_ERROR_NOT_FOUND or error code.
 */
int intmap_remove(struct intmap_s * self, size_t key, void ** old_value);

/**
 * @brief Create an iterator the traverses all items in the container.
 *
 * @param[in] self The container.
 * @return The iterator which must be freed using intmap_iterator_free().
 *
 * Both intmap_put and intmap_remove operations are allowed on the container
 * while an iterator is active.  Items added using intmap_put may or may not
 * be included in the traversal.
 */
struct intmap_iterator_s * intmap_iterator_new(struct intmap_s * self);

/**
 * @brief Get the next item from the container iterator.
 *
 * @param[in] self The iterator.
 * @param[out] key The key for the next item.
 * @param[out] value The value for the next item.
 * @return 0, JETLEX_ERROR_NOT_FOUND at end or an error.
 */
int intmap_iterator_next(struct intmap_iterator_s * self, size_t * key, void ** value);

/**
 * @brief Free an iterator.
 *
 * @param[in] self The iterator.
 */
void intmap_iterator_free(struct intmap_iterator_s * self);

/**
 * @brief Declare an custom intmap with a specified value type.
 *
 * @param name The name for the intmap.
 * @param type The value type.
 */
#define INTMAP_DECLARE(name, type) \
    struct name##_s; \
    struct name##_iterator_s; \
    struct name##_s * name##_new(); \
    void name##_free(struct name##_s * self); \
    size_t name##_length(struct name##_s * self); \
    int name##_put(struct name##_s * self, size_t key, type value, type * old_value); \
    int name##_get(struct name##_s * self, size_t key, type * value); \
    int name##_remove(struct name##_s * self, size_t key, type * old_value); \
    struct name##_iterator_s * name##_iterator_new(struct name##_s * self); \
    int name##_iterator_next(struct name##_iterator_s * self, size_t * key, type * value); \
    void name##_iterator_free(struct name##_iterator_s * self);

/**
 * @brief Define the default structure.
 *
 * @param name The name for the intmap.
 *
 * If this macro is not used, ensure to include struct intmap_s * intmap
 * in your structure definition.
 */
#define INTMAP_DEFINE_STRUCT(name) \
    struct name##_s { struct intmap_s * intmap; };

/**
 * @brief Define an custom intmap with a specified value type.
 *
 * @param name The name for the intmap.
 * @param type The value type.
 */
#define INTMAP_DEFINE(name, type) \
    struct name##_iterator_s { struct intmap_iterator_s * iter; }; \
    struct name##_s * name##_new() { \
        struct name##_s * self = calloc(1, sizeof(struct name##_s)); \
        self->intmap = intmap_new(); \
        return self; \
    } \
    void name##_free(struct name##_s * self) { intmap_free(self->intmap); } \
    size_t name##_length(struct name##_s * self) { return intmap_length(self->intmap); } \
    int name##_put(struct name##_s * self, size_t key, type value, type * old_value) { \
        return intmap_put(self->intmap, key, (void *) value, (void **) old_value); \
    } \
    int name##_get(struct name##_s * self, size_t key, type * value) { \
        return intmap_get(self->intmap, key, (void **) value); \
    } \
    int name##_remove(struct name##_s * self, size_t key, type * old_value) { \
        return intmap_remove(self->intmap, key, (void **) old_value); \
    } \
    struct name##_iterator_s * name##_iterator_new(struct name##_s * self) { \
        struct name##_iterator_s * iter = calloc(1, sizeof(struct name##_iterator_s)); \
        iter->iter = intmap_iterator_new(self->intmap); \
        return iter; \
    } \
    int name##_iterator_next(struct name##_iterator_s * self, size_t * key, type * value) { \
        return intmap_iterator_next(self->iter, key, (void *) value); \
    } \
    void name##_iterator_free(struct name##_iterator_s * self) { \
        intmap_iterator_free(self->iter); \
    }

/** @} */


CPP_GUARD_END

#endif /* INTMAP_H */
