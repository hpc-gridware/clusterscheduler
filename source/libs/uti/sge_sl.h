#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Thread safe doubly linked list
 */

#include <pthread.h>

#include <cinttypes>

/** @defgroup uti_sl Simple List
 * @brief A simple, thread safe doubly linked list
 *
 * Lists can be created and destroyed, and the number of nodes retrieved, with
 * #sge_sl_create, #sge_sl_destroy and #sge_sl_get_elem_count.
 *
 * The list is doubly linked, so data can be walked forwards and backwards;
 * function arguments decide whether the first or the last element is addressed
 * (#sge_sl_insert, #sge_sl_delete, #sge_sl_data). Elements can also be inserted
 * into a sorted list or found with a caller supplied compare function
 * (#sge_sl_insert_search, #sge_sl_delete_search, #sge_sl_data_search,
 * #sge_sl_sort).
 *
 * @section uti_sl_threads Thread safety
 *
 * The list itself is thread safe: one list can be shared between threads with
 * no further synchronisation.
 *
 * Nodes are **not**. The functions that hand out or manipulate node structures
 * therefore need synchronisation by the caller: #sge_sl_elem_create,
 * #sge_sl_elem_destroy, #sge_sl_elem_data, #sge_sl_elem_next,
 * #sge_sl_elem_search, #sge_sl_dechain, #sge_sl_insert_before and
 * #sge_sl_append_after.
 *
 * #sge_sl_lock and #sge_sl_unlock exist for exactly that: they make it possible
 * to run a sequence of node based calls as one atomic block.
 *
 * @warning Be careful when taking other locks between #sge_sl_lock and
 *          #sge_sl_unlock. Acquiring them in an inconsistent order deadlocks.
 * @{
 */

/** @brief Walk a list under its lock, releasing it when the loop ends
 *
 * Takes the list lock before the first element and drops it after the last, so
 * the whole traversal is atomic. Do not `break` out of this loop - the lock
 * would stay held; see @ref uti_sl_threads.
 */
#define for_each_sl_locked(elem, list) \
   for(sge_sl_lock(list), elem = nullptr, sge_sl_elem_next(list, &elem, SGE_SL_FORWARD); \
       elem != nullptr || (sge_sl_unlock(list), false); \
       sge_sl_elem_next(list, &elem, SGE_SL_FORWARD))

/** @brief Walk a list without locking it
 *
 * Use only when the caller already holds the lock, or when no other thread can
 * touch the list.
 */
#define for_each_sl(elem, list) \
   for(elem = nullptr, sge_sl_elem_next(list, &elem, SGE_SL_FORWARD); \
       elem != nullptr; \
       sge_sl_elem_next(list, &elem, SGE_SL_FORWARD))

/** @brief Orders two elements, following the `strcmp` convention
 *
 * @param data1 first element
 * @param data2 second element
 * @return 0 when equal, > 0 when @p data1 sorts after @p data2, < 0 otherwise
 */
typedef int (*sge_sl_compare_f)(const void *data1, const void *data2);

/** @brief Releases the data of one element
 *
 * Called by #sge_sl_destroy for every remaining element, so the list can free
 * payload it does not know the type of.
 *
 * @param[in,out] data address of the payload pointer; set to nullptr when freed
 * @return true on success
 */
typedef bool (*sge_sl_destroy_f)(void **data);

/** @brief Which end of the list to work from */
enum sge_sl_direction_t {
   SGE_SL_FORWARD,    ///< from the first element towards the last
   SGE_SL_BACKWARD    ///< from the last element towards the first
};

/** @brief One node of the list
 *
 * Nodes are **not** thread safe; see @ref uti_sl_threads before touching one.
 */
struct sge_sl_elem_t {
   sge_sl_elem_t *prev;   ///< previous node, nullptr at the head
   sge_sl_elem_t *next;   ///< next node, nullptr at the tail
   void *data;            ///< the payload, owned by the caller
};

/** @brief The list itself
 *
 * Thread safe: the list operations take #mutex themselves.
 */
struct sge_sl_list_t {
   pthread_mutex_t mutex;   ///< protects every other field, and the nodes
   sge_sl_elem_t *first;    ///< head of the list, nullptr when empty
   sge_sl_elem_t *last;     ///< tail of the list, nullptr when empty
   uint32_t elements;       ///< number of nodes currently in the list
};

bool
sge_sl_elem_create(sge_sl_elem_t **elem, void *data);

bool
sge_sl_elem_destroy(sge_sl_elem_t **elem, sge_sl_destroy_f destroy);

void *
sge_sl_elem_data(sge_sl_elem_t *elem);

bool
sge_sl_elem_next(sge_sl_list_t *list,
                 sge_sl_elem_t **elem, sge_sl_direction_t direction);


bool
sge_sl_elem_search(sge_sl_list_t *list, sge_sl_elem_t **elem, void *data,
                   sge_sl_compare_f compare, sge_sl_direction_t direction);

bool
sge_sl_create(sge_sl_list_t **list);

bool
sge_sl_destroy(sge_sl_list_t **list, sge_sl_destroy_f destroy);

bool
sge_sl_dechain(sge_sl_list_t *list, sge_sl_elem_t *elem);

bool
sge_sl_insert_before(sge_sl_list_t *list, sge_sl_elem_t *new_elem, sge_sl_elem_t *elem);

bool
sge_sl_append_after(sge_sl_list_t *list, sge_sl_elem_t *new_elem, sge_sl_elem_t *elem);

bool
sge_sl_lock(sge_sl_list_t *list);

void
sge_sl_unlock(sge_sl_list_t *list);

void
sge_sl_elem_insert(sge_sl_list_t *list, sge_sl_elem_t *new_elem, sge_sl_direction_t direction);

bool
sge_sl_insert(sge_sl_list_t *list, void *data, sge_sl_direction_t direction);

bool
sge_sl_insert_search(sge_sl_list_t *list, void *data, sge_sl_compare_f compare);

void
sge_sl_data(sge_sl_list_t *list, void **data, sge_sl_direction_t direction);

bool
sge_sl_data_search(sge_sl_list_t *list, void *key, void **data,
                   sge_sl_compare_f compare, sge_sl_direction_t direction);

bool
sge_sl_delete(sge_sl_list_t *list,
              sge_sl_destroy_f destroy, sge_sl_direction_t direction);

bool
sge_sl_delete_search(sge_sl_list_t *list, void *key, sge_sl_destroy_f destroy,
                     sge_sl_compare_f compare, sge_sl_direction_t direction);

uint32_t
sge_sl_get_elem_count(sge_sl_list_t *list);

pthread_mutex_t *
sge_sl_get_mutex(sge_sl_list_t *list);

bool
sge_sl_sort(sge_sl_list_t *list, sge_sl_compare_f compare);

/** @} */
