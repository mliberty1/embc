/*
 * Copyright 2017 Jetperch LLC
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
 * \brief A doubly-linked mutable circular list.
 */

#ifndef EMBC_LIST_H_
#define EMBC_LIST_H_

#include "embc/cmacro_inc.h"
#include "embc/cdef.h"
#include "embc/platform.h"

EMBC_CPP_GUARD_START

/**
 * \defgroup list list
 *
 * \brief A doubly-linked mutable circular list.
 *
 * This module defines a double-linked circular list.  Each list item contains
 * two pointers: one for the next item and one for the previous item.  The
 * list itself is defined by a single item (two pointers) which is a sentry
 * to denote the start and end of the list.
 *
 * The implementation uses a structure that is intended to be added to the
 * containing structure.  This design choice allows for a single entry to
 * participate in multiple lists.
 *
 * The doubly-linked list has the excellent property of O(1) head insertion,
 * tail insertion, relative insertion and deletion.  A singly linked list,
 * while requiring only a single pointer, has O(n) tail insertion and deletion.
 * For more information, see Wikipedia
 * (https://en.wikipedia.org/wiki/Doubly_linked_list).
 *
 * The terminology used in this module is:
 *
 *    - list: The top-level list which contains pointers to the first and last
 *      items.  A list uses the same struct as an item.
 *    - item: A single list element which contains pointers to the next
 *      and previous items.
 *    - entry: The struct containing meaningful data and one or more
 *      item structs which can participate in a list.
 *
 * Although this list implementation is a circular list, the list item is
 * really a sentry that does not participate in the list.  Item deletion is
 * the main reason for having a circular list.  With a circular list, an item
 * may be safely removed from a list any knowledge of which list it is in.
 * Items may be safely passed to embc_list_remove() at any time, even when they
 * are not in a list, as long as they were previously in a list or initialized
 * with embc_list_initialize(item).
 *
 * @{
 */

/**
 * @brief A linked list or linked list item.
 */
struct embc_list_s {
    /**
     * @brief The pointer to the next item.
     *
     * The last item in the list points back to the list.
     */
    struct embc_list_s * next;

    /**
     * @brief The pointer to the previous item.
     *
     * The first item in the list points back to the list.
     */
    struct embc_list_s * prev;
};

/**
 * @brief Initialize a list or an item.
 *
 * @param list The list pointer or item pointer to initialize.
 */
static inline void embc_list_initialize(struct embc_list_s * list) {
    list->next = list;
    list->prev = list;
}

/**
 * @brief Check if the list is empty.
 *
 * @param list The list pointer to check.
 * @return true if empty, false if contains one or more items.
 */
static inline bool embc_list_is_empty(struct embc_list_s * list) {
    return (list->next == list);
}

/**
 * @brief Get the entry containing the list item.
 *
 * @param item The list item pointer.
 * @param type The type of the entry container that has the list.
 * @param member The name of the list member in the structure.
 * @return The pointer to the container.
 */
#define embc_list_entry(item, type, member) \
    EMBC_CONTAINER_OF(item, type, member)

/**
 * @brief Add an item to the front of the list.
 *
 * @param list The list pointer.
 * @param item The pointer to the item to add.
 */
static inline void embc_list_add_head(struct embc_list_s * list, struct embc_list_s * item) {
    item->next = list->next;
    item->prev = list;
    item->next->prev = item;
    list->next = item;
}

/**
 * @brief Add an item to the end of the list.
 *
 * @param list The list pointer.
 * @param item The pointer to the item to add.
 */
static inline void embc_list_add_tail(struct embc_list_s * list, struct embc_list_s * item) {
    item->next = list;
    item->prev = list->prev;
    item->prev->next = item;
    list->prev = item;
}

/**
 * @brief Get the first item in the list without modifying the list.
 *
 * @param list The list pointer.
 * @return The pointer to the first item in the list or 0 if empty.
 */
static inline struct embc_list_s * embc_list_peek_head(struct embc_list_s * list) {
    if ((0 != list) && (list->next != list)) {
        return list->next;
    }
    return 0;
}

/**
 * @brief Get the last item in the list without modifying the list.
 *
 * @param list The list pointer.
 * @return The pointer ot the last item in the list or 0 if empty.
 */
static inline struct embc_list_s * embc_list_peek_tail(struct embc_list_s * list) {
    if ((0 != list) && (list->prev != list)) {
        return list->prev;
    }
    return 0;
}

/**
 * @brief Get the first item in the list and remove it from the list.
 *
 * @param list The list pointer.
 * @return The pointer to the first item in the list or 0 if empty.
 */
static inline struct embc_list_s * embc_list_remove_head(struct embc_list_s * list) {
    if ((0 != list) && (list->next != list)) {
        struct embc_list_s * item = list->next;
        item->next->prev = list;
        list->next = item->next;
        item->next = item;
        item->prev = item;
        return item;
    }
    return 0;
}

/**
 * @brief Get the last item in the list and remove it from the list.
 *
 * @param list The list pointer.
 * @return The pointer to the last item in the list or 0 if empty.
 */
static inline struct embc_list_s * embc_list_remove_tail(struct embc_list_s * list) {
    if ((0 != list) && (list->prev != list)) {
        struct embc_list_s * item = list->prev;
        item->prev->next = list;
        list->prev = item->prev;
        item->next = item;
        item->prev = item;
        return item;
    }
    return 0;
}

/**
 * @brief Remove an item from a list.
 *
 * @param item The pointer to the item to remove from any list it may be in.
 *      The item's prev and next pointers are also reset.
 *
 * Warning: calling this function on an uninitialized item will cause
 * invalid memory accesses.  Either call embc_list_initialize() when the
 * item is initialized or ensure that the item is already in a list.
 */
static inline void embc_list_remove(struct embc_list_s * item) {
    item->prev->next = item->next;
    item->next->prev = item->prev;
    item->next = item;
    item->prev = item;
}

/**
 * @brief Insert an new item before another item.
 *
 * @param position_item The existing item pointer.
 * @param new_item The new item pointer which will be inserted before
 *      position_item.
 */
static inline void embc_list_insert_before(
        struct embc_list_s * position_item,
        struct embc_list_s * new_item) {
    new_item->prev = position_item->prev;
    new_item->next = position_item;
    new_item->prev->next = new_item;
    position_item->prev = new_item;
}

/**
 * @brief Insert an new item after another item.
 *
 * @param position_item The existing item pointer.
 * @param new_item The new item pointer which will be inserted after
 *      position_item.
 */
static inline void embc_list_insert_after(
        struct embc_list_s * position_item,
        struct embc_list_s * new_item) {
    new_item->next = position_item->next;
    new_item->prev = position_item;
    position_item->next = new_item;
    new_item->next->prev = new_item;
}

/**
 * @brief Iterate over each item in the list in forward order (head to tail).
 *
 * @param list The list pointer.
 * @return The current item pointer from the list.
 *
 * Deleting the current item or inserting items after the current
 * item may cause the traversal to fail!  The only upside is a
 * (usually) minor performance improvement.
 */
#define embc_list_foreach_unsafe(list, item) \
    for (item = (list)->next; item != (list); \
         item = item->next)

/**
 * @brief Iterate over each item in the list in forward order (head to tail).
 *
 * @param list The list pointer.
 * @param item The current item pointer from the list.
 *
 * The item may be safely removed from the list while iterating.  If new items
 * are inserted immediately after this item during the traversal, they will
 * be skipped.
 */
#define embc_list_foreach(list, item) \
    item = (list)->next; \
    for (struct embc_list_s * next__ = item->next; item != (list); \
         item = next__, next__ = next__->next)

/**
 * @brief Iterate over each item in the list in reverse order (tail to head).
 *
 * @param list The list pointer.
 * @return The current item pointer from the list.
 *
 * Deleting the current item or inserting items before the current
 * item may cause the traversal to fail!  The only upside is a
 * (usually) minor performance improvement.
 */
#define embc_list_foreach_reverse_unsafe(list, item) \
    for (item = (list)->prevnext; item != (list); \
         item = item->prev)

/**
 * @brief Iterate over each item in the list in reverse order (tail to head).
 *
 * @param list The list pointer.
 * @return The current item pointer from the list.
 *
 * The item may be safely removed from the list while iterating.  If new items
 * are inserted immediately before this item during the traversal, they will
 * be skipped.
 */
#define embc_list_foreach_reverse(list, item) \
    item = (list)->prev; \
    for (struct embc_list_s * next = item->prev; item != (list); \
         item = next, next = next->prev)

/**
 * @brief Get the number of items in the list: O(n).
 *
 * @param list The list pointer.
 * @return The number of items in the list.
 */
static inline embc_size_t embc_list_length(struct embc_list_s * list) {
    embc_size_t sz = 0;
    struct embc_list_s * item;
    embc_list_foreach_unsafe(list, item) {
        ++sz;
    }
    return sz;
}

/**
 * @brief Get the item by its position in the list.
 *
 * @param list The list pointer.
 * @param index The index for the item.
 * @return The pointer to the item at index or 0 if list is not long enough.
 *
 * To iterate over a list, use one of the foreach functions which are O(n).
 * This function is O(n), so iterating using this function is O(n^2)!
 */
EMBC_API struct embc_list_s * embc_list_index(struct embc_list_s * list,
                                              embc_size_t index);

/**
 * @brief Get the index of the item.
 *
 * @param list The list pointer.
 * @param item The pointer to the item to get.
 * @return The index of item or -1 if not found in the list.  The first
 *      item in the list is at index 0.
 *
 * This operation is O(n).
 */
EMBC_API embc_size_t embc_list_index_of(struct embc_list_s * list,
                                             struct embc_list_s * item);

/**
 * @brief Check if a list contains an item.
 *
 * @param list The list pointer.
 * @param item The pointer to the item to check.
 * @return true if the item was found in the list or false if it was
 *      not found in the list.
 *
 * This operation is O(n).
 */
static inline bool embc_list_contains(struct embc_list_s * list,
                                      struct embc_list_s * item) {
    return (embc_list_index_of(list, item) < 0);
}


/** @} */


EMBC_CPP_GUARD_END

#endif /* EMBC_LIST_H_ */
