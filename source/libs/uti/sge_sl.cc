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
 *   Copyright: 2009 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Implementation of the thread safe doubly linked list, see @ref uti_sl
 */

#include <cstdlib>
#include <cstdio>

#include "uti/sge_err.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_sl.h"
#include "uti/sge_stdlib.h"

#include "msg_common.h"

/// debug layer used by every DENTER/DPRINTF in this file
#define SL_LAYER BASIS_LAYER
/// name reported when the list mutex is locked or unlocked
#define SL_MUTEX_NAME "sl_mutex"

/**
 * @brief Create a list element
 *
 * This function creates a new sl element that can later on be inserted
 * into a sl list.
 *
 * On success the function creates the element stores the 'data' pointer
 * in it and returns the element pointer in 'elem'.
 *
 * On error false is returned by this function and 'elem' contains a
 * nullptr pointer.
 *
 * sge_sl_elem_destroy() can be used to destroy elements that were
 * created with this function.
 *
 * @param elem location were the pointer to the new element will be stored
 * @param data data pointer that will be stored in elem
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_elem_create() is MT safe
 *
 * @see #sge_sl_elem_destroy
 */
bool
sge_sl_elem_create(sge_sl_elem_t **elem, void *data) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (elem != nullptr) {
      const size_t size = sizeof(sge_sl_elem_t);
      sge_sl_elem_t *new_elem;

      new_elem = (sge_sl_elem_t *) sge_malloc(size);
      if (new_elem != nullptr) {
         new_elem->prev = nullptr;
         new_elem->next = nullptr;
         new_elem->data = data;
         *elem = new_elem;
      } else {
         sge_err_set(SGE_ERR_MEMORY, MSG_UNABLETOALLOCATEBYTES_DS, size, __func__);
         *elem = nullptr;
         ret = false;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Destroys a sl element
 *
 * This function destroys the provided sl 'elem' and optionally destroys
 * also the data that is referenced in the element if a 'destroy'
 * function is passed.
 *
 * If 'elem' is part of a list it has to be unchained before this
 * function can be called. Otherwise the list would be corrupted.
 *
 * On success elem will be set to nullptr and the function returns true.
 *
 * @code
 * Here is an example for a destroy function for a C string.
 *
 *    bool
 *    destroy(void **data_ptr) {
 *       char *string = *(char **) data_ptr;
 *
 *       sge_free(&string);
 *       return true;
 *    }
 * @endcode
 *
 * @param elem pointer to a sl element pointer
 * @param destroy destroy function
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_elem_destroy() is MT safe.
 *
 * @see #sge_sl_elem_create
 */
bool
sge_sl_elem_destroy(sge_sl_elem_t **elem, sge_sl_destroy_f destroy) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (elem != nullptr && *elem != nullptr) {
      if (destroy != nullptr) {
         destroy(&(*elem)->data);
      }
      sge_free(elem);
   }
   DRETURN(ret);
}

/**
 * @brief Return first/last data pointer
 *
 * returns the stored data pointer of an element
 *
 * @param elem sl element
 *
 * @return data pointer
 *
 * @note MT-NOTE: sge_sl_elem_data() is MT safe
 *
 * @see #sge_sl_elem_create, #sge_sl_elem_destroy
 */
void *
sge_sl_elem_data(sge_sl_elem_t *elem) {
   DENTER(SL_LAYER);

   void *ret = nullptr;

   if (elem != nullptr) {
      ret = elem->data;
   }
   DRETURN(ret);
}

/**
 * @brief Unchains a element from a list
 *
 * This functions unchains 'elem' from 'list'. 'elem' can afterwards
 * be inserted into a list again or can be destroyed.
 *
 * @param list sl list
 * @param elem sl elem
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_dechain() is MT safe
 *
 * @see #sge_sl_elem_destroy, #sge_sl_dechain, `sge_sl_append()`, #sge_sl_append_after, #sge_sl_insert, #sge_sl_insert_before, #sge_sl_insert_search
 */
bool
sge_sl_dechain(sge_sl_list_t *list, sge_sl_elem_t *elem) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr && elem != nullptr) {
      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      if (elem->prev) {
         elem->prev->next = elem->next;
      } else {
         list->first = elem->next;
      }
      if (elem->next) {
         elem->next->prev = elem->prev;
      } else {
         list->last = elem->prev;
      }

      elem->next = nullptr;
      elem->prev = nullptr;

      list->elements--;
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Inserts a new element before another one
 *
 * Inserts 'new_elem' before 'elem' in 'list'. 'elem' must be
 * an element already part of 'list'.
 *
 * @param list sl list
 * @param new_elem new sl element
 * @param elem sl elem already part of 'list"
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_insert_before() is MT safe
 *
 * @see #sge_sl_elem_destroy, #sge_sl_dechain, `sge_sl_append()`, #sge_sl_append_after, #sge_sl_insert, #sge_sl_insert_before, #sge_sl_insert_search
 */
bool
sge_sl_insert_before(sge_sl_list_t *list, sge_sl_elem_t *new_elem, sge_sl_elem_t *elem) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr && new_elem != nullptr && elem != nullptr) {
      sge_sl_elem_t *last;

      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      last = elem->prev;
      if (last == nullptr) {
         elem->prev = new_elem;
         new_elem->next = elem;
         list->first = new_elem;
      } else {
         last->next = new_elem;
         elem->prev = new_elem;
         new_elem->prev = last;
         new_elem->next = elem;
      }
      list->elements++;
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Appends a new element after another one
 *
 * This elements appends 'new_elem' into 'list' after 'elem'.
 *
 * @param list sl list
 * @param new_elem new sl elem
 * @param elem sl elem already part of list
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_append_after() is MT safe
 *
 * @see #sge_sl_elem_destroy, #sge_sl_dechain, `sge_sl_append()`, #sge_sl_append_after, #sge_sl_insert, #sge_sl_insert_before, #sge_sl_insert_search
 */
bool
sge_sl_append_after(sge_sl_list_t *list, sge_sl_elem_t *new_elem, sge_sl_elem_t *elem) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr && new_elem != nullptr && elem != nullptr) {
      sge_sl_elem_t *current;

      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      current = elem->next;
      if (current == nullptr) {
         elem->next = new_elem;
         new_elem->prev = elem;
         list->last = new_elem;
      } else {
         elem->next = new_elem;
         current->prev = new_elem;
         new_elem->prev = elem;
         new_elem->next = current;
      }
      list->elements++;
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Provides the next element in sequence
 *
 * This function provides the possibility to iterate over all elements
 * in 'list'. 'elem' will contain a pointer to an element when the
 * function returns or the value nullptr.  'elem' is also an input
 * parameter and it defines what element of 'list' is returned.
 *
 * If *elem is nullptr and direction is SGE_SL_FORWARD them *elem will
 * contain the first element in 'list'. If direction is SGE_SL_BACKWARD
 * then it will contain the last.
 *
 * If *elem is not nullptr then the next element in the list sequence is
 * returned if direction is SGE_SL_FORWARD or the previous one if
 * direction is SGE_SL_BACKWARD.
 *
 * If the list is empty or if there is no previous/next element then
 * nullptr will be retuned in 'elem'.
 *
 * @code
 * Following code shows how it is possible to iterate over
 * the whole list:
 *
 * {
 *    sge_sl_list_t *list;
 *    sge_sl_elem_t *next;
 *    sge_sl_elem_t *current;
 *
 *    // assume that elements are added to the list here
 *
 *    next = nullptr;
 *    sge_sl_elem_next(list, &next, SGE_SL_FORWARD);
 *    while ((current = next) != nullptr) {
 *       sge_sl_elem_next(list, &next, SGE_SL_FORWARD);
 *
 *       // so something with 'current' here
 *    }
 * }
 * @endcode
 *
 * @param list sl list
 * @param elem input/output sl elem
 * @param direction direction
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_elem_next() is MT safe
 *
 * @see #sge_sl_elem_search
 */
bool
sge_sl_elem_next(sge_sl_list_t *list,
                 sge_sl_elem_t **elem, sge_sl_direction_t direction) {
   DENTER(BASIS_LAYER);

   bool ret = true;

   if (list != nullptr && elem != nullptr) {
      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      if (*elem != nullptr) {
         if (direction == SGE_SL_FORWARD) {
            *elem = (*elem)->next;
         } else {
            *elem = (*elem)->prev;
         }
      } else {
         if (direction == SGE_SL_FORWARD) {
            *elem = list->first;
         } else {
            *elem = list->last;
         }
      }
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Searches the next element in sequence
 *
 * This function provides the possibility to iterate over certain
 * elements in 'list'. 'elem' will contain a pointer to an element when
 * the function returns or the value nullptr.  'elem' is also an input
 * parameter and it defines what element of 'list' is returned.
 *
 * If *elem is nullptr and direction is SGE_SL_FORWARD then *elem will
 * contain the first element in 'list' that is equivalent with the
 * provided 'key'.  If direction is SGE_SL_BACKWARD then it will contain
 * the last element that matches.
 *
 * If *elem is not nullptr then the next element in the list sequence is
 * returned if direction is SGE_SL_FORWARD or the previous one if
 * direction is SGE_SL_BACKWARD.
 *
 * If the list is empty or if there is no previous/next element then
 * nullptr will be retuned in 'elem'.
 *
 * The provided 'compare' function is used to compare the provided 'key'
 * with the data that is contained in the element. 'key' is passed as
 * first parameter to the 'compare' function.
 *
 * @code
 * This compare function could match static C strings stored in
 * a sl list as data pointers.
 *
 * int
 * fnmatch_compare(const void *key_pattern, const void *data) {
 *    int ret = 0;
 *
 *    if (key_pattern != nullptr && data != nullptr) {
 *       ret = fnmatch(*(char**)key_pattern, *(char**)data, 0);
 *    }
 *    return ret;
 * }
 * @endcode
 *
 * @param list sl list
 * @param elem input/output sl elem
 * @param key key that must match
 * @param compare compare function
 * @param direction search direction
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_elem_search() is MT safe
 *
 * @see #sge_sl_elem_next
 */
bool
sge_sl_elem_search(sge_sl_list_t *list, sge_sl_elem_t **elem, void *key,
                   sge_sl_compare_f compare, sge_sl_direction_t direction) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr && elem != nullptr && compare != nullptr) {
      sge_sl_elem_t *next = nullptr;
      sge_sl_elem_t *current = nullptr;

      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      if (*elem != nullptr) {
         if (direction == SGE_SL_FORWARD) {
            next = (*elem)->next;
         } else {
            next = (*elem)->prev;
         }
      } else {
         if (direction == SGE_SL_FORWARD) {
            next = list->first;
         } else {
            next = list->last;
         }
      }
      while ((current = next) != nullptr && current != nullptr &&
             compare((const void *) &key, (const void *) &current->data) != 0) {
         if (direction == SGE_SL_FORWARD) {
            next = current->next;
         } else {
            next = current->prev;
         }
      }
      *elem = current;
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Create a new simple list
 *
 * This function creates a new simple list and returns the list in the
 * 'list' parameter. In case of an error nullptr will be returned.
 *
 * @param list new simple list
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_create() is MT safe
 *
 * @see #sge_sl_destroy
 */
bool
sge_sl_create(sge_sl_list_t **list) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr) {
      const size_t size = sizeof(sge_sl_list_t);
      sge_sl_list_t *new_list;

      new_list = (sge_sl_list_t *) sge_malloc(size);
      if (new_list != nullptr) {
         pthread_mutexattr_t mutex_attr;

         /* initialize the mutex */
         pthread_mutexattr_init(&mutex_attr);
         pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
         pthread_mutex_init(&new_list->mutex, &mutex_attr);
         pthread_mutexattr_destroy(&mutex_attr);

         /* other members */
         new_list->first = nullptr;
         new_list->last = nullptr;
         new_list->elements = 0;

         *list = new_list;
      } else {
         sge_err_set(SGE_ERR_MEMORY, MSG_UNABLETOALLOCATEBYTES_DS, size, __func__);
         ret = false;
         *list = nullptr;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Destroys a simple list
 *
 * This function destroys 'list' and sets the pointer to nullptr.
 * If a 'destroy' function is provided then it will be used
 * to destroy all data elements that are referenced by the list
 * elements.
 *
 * @code
 * Here is an example for a destroy function for a C string.
 *
 *    bool
 *    destroy(void **data_ptr) {
 *       char *string = *(char **) data_ptr;
 *
 *       sge_free(&string);
 *       return true;
 *    }
 * @endcode
 *
 * @param list sl list
 * @param destroy destroy function
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_destroy() is not MT safe
 *
 * @see #sge_sl_create, #sge_sl_elem_destroy
 */
bool
sge_sl_destroy(sge_sl_list_t **list, sge_sl_destroy_f destroy) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr && *list != nullptr) {
      sge_sl_elem_t *next;
      sge_sl_elem_t *current;

      /* destroy content */
      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &(*list)->mutex);
      next = (*list)->first;
      while ((current = next) != nullptr) {
         next = current->next;

         ret &= sge_sl_elem_destroy(&current, destroy);
      }
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &(*list)->mutex);

      /* final destroy */
      pthread_mutex_destroy(&(*list)->mutex);
      sge_free(list);
   }
   DRETURN(ret);
}

/**
 * @brief Locks a list
 *
 * A call of this functions locks the provided 'list' so that all
 * list operations executed between the lock and unlock are
 * executed as atomic operation.
 *
 * @param list list
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_lock() is MT safe
 *
 * @see #sge_sl_unlock
 */
bool
sge_sl_lock(sge_sl_list_t *list) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr) {
      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Unlocks a list
 *
 * A call of this functions unlocks the provided 'list' that was
 * previously locked with sge_sl_lock.
 *
 * @param list list
 *
 *
 * @note MT-NOTE: sge_sl_unlock() is MT safe
 *
 * @see #sge_sl_lock
 */
void
sge_sl_unlock(sge_sl_list_t *list) {
   DENTER(SL_LAYER);
   if (list != nullptr) {
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN_VOID;
}

/** @brief Insert a new element into a list
 *
 *  This function inserts a new element into a list. The element will be
 *  inserted at the beginning of the list if 'direction' is SGE_SL_FORWARD
 *  otherwise at the end.
 *
 *  @param list      - list
 *  @param new_elem  - new element
 *  @param direction - direction
 */
void
sge_sl_elem_insert(sge_sl_list_t *list, sge_sl_elem_t *new_elem, sge_sl_direction_t direction) {
   DENTER(SL_LAYER);
   if (list != nullptr && new_elem != nullptr) {
      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      if (direction == SGE_SL_FORWARD) {
         if (list->first != nullptr) {
            list->first->prev = new_elem;
         }
         new_elem->next = list->first;
         list->first = new_elem;
         if (list->last == nullptr) {
            list->last = new_elem;
         }
      } else {
         if (list->last != nullptr) {
            list->last->next = new_elem;
         }
         new_elem->prev = list->last;
         list->last = new_elem;
         if (list->first == nullptr) {
            list->first = new_elem;
         }
      }
      list->elements++;
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN_VOID;
}

/**
 * @brief Insert a new element
 *
 * Insert a new node in 'list' that references 'data'. If 'direction'
 * is SGE_SL_FORWARD then the element will be inserted at the beginning
 * of 'list' otherwise at the end.
 *
 * @param list simple list
 * @param data data
 * @param direction insert at the head or at the tail of the list
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_insert() is MT safe
 *
 * @see `sge_sl_append()`, #sge_sl_insert_search
 */
bool
sge_sl_insert(sge_sl_list_t *list, void *data, sge_sl_direction_t direction) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr) {
      sge_sl_elem_t *new_elem;

      ret = sge_sl_elem_create(&new_elem, data);
      if (ret) {
         sge_sl_elem_insert(list, new_elem, direction);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Inserts a new element in a sorted list
 *
 * Inserts a new element in 'list' that references 'data'. The function
 * assumes that 'list' is sorted in ascending order. To find the correct
 * position for the new element the 'compare' function will be used.
 *
 * @code
 * Example for a compare function when data is a C string
 *
 * int
 * compare(const void *data1, const void *data2) {
 *    int ret = 0;
 *
 *    if (data1 != nullptr && data2 != nullptr) {
 *       ret = strcmp(*(char**)data1, *(char**)data2);
 *    }
 *    return ret;
 * }
 * @endcode
 *
 * @param list list
 * @param data data reference
 * @param compare compare function
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_insert_search() is MT safe
 *
 * @see #sge_sl_insert, `sge_sl_append()`
 */
bool
sge_sl_insert_search(sge_sl_list_t *list, void *data, sge_sl_compare_f compare) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr && compare != nullptr) {
      sge_sl_elem_t *new_elem;

      ret = sge_sl_elem_create(&new_elem, data);
      if (ret) {
         sge_sl_elem_t *last = nullptr;
         sge_sl_elem_t *current = nullptr;

         sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
         current = list->first;
         while (current != nullptr &&
                compare((const void *) &data, (const void *) &current->data) > 0) {
            last = current;
            current = current->next;
         }

         if (last == nullptr && current == nullptr) {
            list->first = new_elem;
            list->last = new_elem;
         } else if (last == nullptr) {
            current->prev = new_elem;
            new_elem->next = current;
            list->first = new_elem;
         } else if (current == nullptr) {
            last->next = new_elem;
            new_elem->prev = last;
            list->last = new_elem;
         } else {
            last->next = new_elem;
            current->prev = new_elem;
            new_elem->prev = last;
            new_elem->next = current;
         }
         list->elements++;
         sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Returns the first or last data element
 *
 * Depending on 'direction' this function returns the pointer
 * to the first/last data object of 'list' in 'data'.
 *
 * @param list list
 * @param data data pointer
 * @param direction direction
 *
 *
 * @note MT-NOTE: sge_sl_data() is MT safe
 */
void
sge_sl_data(sge_sl_list_t *list, void **data, sge_sl_direction_t direction) {
   DENTER(SL_LAYER);
   if (list != nullptr && data != nullptr) {
      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      if (direction == SGE_SL_FORWARD && list->first != nullptr) {
         *data = list->first->data;
      } else if (direction == SGE_SL_BACKWARD && list->last != nullptr) {
         *data = list->last->data;
      } else {
         *data = nullptr;
      }
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
}

/**
 * @brief Search a elements in list
 *
 * This function tries to find a element in 'list'. As result of the
 * search the 'data' pointer will be returned. To find the data pointer
 * the 'compare' function and the 'key' will be used. 'key' is past as
 * first argument to the 'compare' function. 'direction' decides if
 * this function starts the search from beginning or end of the list.
 *
 * @param list list
 * @param key search key
 * @param data returned data pointer
 * @param compare compare function
 * @param direction direction
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_data_search() is MT safe
 */
bool
sge_sl_data_search(sge_sl_list_t *list, void *key, void **data,
                   sge_sl_compare_f compare, sge_sl_direction_t direction) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr && data != nullptr && compare != nullptr) {
      sge_sl_elem_t *elem = nullptr;

      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      ret &= sge_sl_elem_search(list, &elem, key, compare, direction);
      if (ret && elem != nullptr) {
         *data = elem->data;
      } else {
         *data = nullptr;
      }
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Delete first/last element
 *
 * This function deletes the first/last element of 'list' depending
 * on the provided 'direction'. If 'destroy' is not nullptr then
 * this function will be used to destroy the element data.
 *
 * @param list list
 * @param destroy destroy
 * @param direction direction
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_delete() is MT safe
 */
bool
sge_sl_delete(sge_sl_list_t *list,
              sge_sl_destroy_f destroy, sge_sl_direction_t direction) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr) {
      sge_sl_elem_t *elem;

      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      if (direction == SGE_SL_FORWARD) {
         elem = list->first;
      } else {
         elem = list->last;
      }
      ret &= sge_sl_dechain(list, elem);
      if (ret) {
         ret &= sge_sl_elem_destroy(&elem, destroy);
      }
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Search a element and delete it
 *
 * This function searches a element in 'list' using the 'key' and
 * 'compare' function and then deletes it. If 'direction' is
 * SGE_SL_FORWARD then the search will start from beginning, otherwise
 * from the end of the list. The first matched element will be
 * destroyed. If there is a 'destroy' function provided then this
 * function will be used to destroy the element data.
 *
 * @param list list
 * @param key search key
 * @param destroy destroy function
 * @param compare compare function
 * @param direction search direction
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_delete_search() is MT safe
 */
bool
sge_sl_delete_search(sge_sl_list_t *list, void *key, sge_sl_destroy_f destroy,
                     sge_sl_compare_f compare, sge_sl_direction_t direction) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr && key != nullptr && compare != nullptr) {
      sge_sl_elem_t *elem = nullptr;

      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      ret &= sge_sl_elem_search(list, &elem, key, compare, direction);
      if (ret) {
         ret &= sge_sl_dechain(list, elem);
      }
      if (ret) {
         ret &= sge_sl_elem_destroy(&elem, destroy);
      }
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Returns the number of elements
 *
 * This function returns the number of elements contained in 'list'.
 *
 * @param list list pointer
 *
 * @return number of elements
 *
 * @note MT-NOTE: sge_sl_elem_count() is MT safe
 */
uint32_t
sge_sl_get_elem_count(sge_sl_list_t *list) {
   DENTER(SL_LAYER);

   uint32_t elems = 0;

   if (list != nullptr) {
      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      elems = list->elements;
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(elems);
}

/**
 * @brief Sorts the list
 *
 * This function sorts the 'list' with the quick sort algorithm.
 * 'compare' function will be used to compare the list elements.
 *
 * @param list list
 * @param compare compare function
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: sge_sl_sort() is MT safe
 *
 * @see #sge_sl_insert_search
 */
bool
sge_sl_sort(sge_sl_list_t *list, sge_sl_compare_f compare) {
   DENTER(SL_LAYER);

   bool ret = true;

   if (list != nullptr && compare != nullptr) {
      void **pointer_array;
      size_t size;

      sge_mutex_lock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
      size = sizeof(void *) * list->elements;
      pointer_array = (void **) sge_malloc(size);
      if (pointer_array != nullptr) {
         sge_sl_elem_t *elem = nullptr;
         int i;

         /* fill the pointer array with the data pointers */
         i = 0;
         for_each_sl(elem, list) {
            pointer_array[i++] = elem->data;
         }

         /* sort */
         qsort((void *) pointer_array, list->elements, sizeof(void *), compare);

         /* now move the sorted pointers back to elemnts in the list */
         i = 0;
         for_each_sl(elem, list) {
            elem->data = pointer_array[i++];
         }

         /* cleanup */
         sge_free(&pointer_array);
      } else {
         sge_err_set(SGE_ERR_MEMORY, MSG_UNABLETOALLOCATEBYTES_DS, size, __func__);
         ret = false;
      }
      sge_mutex_unlock(SL_MUTEX_NAME, __func__, __LINE__, &list->mutex);
   }
   DRETURN(ret);
}

/**
 * @brief Returns the list mutex
 *
 * retrns the list mutex
 *
 * @param list list
 *
 * @return mutex used in the list to secure actions
 *
 * @note MT-NOTE: sge_sl_get_mutex() is MT safe
 */
pthread_mutex_t *
sge_sl_get_mutex(sge_sl_list_t *list) {
   DENTER(SL_LAYER);

   pthread_mutex_t *mutex = nullptr;

   if (list != nullptr) {
      mutex = &list->mutex;
   }
   DRETURN(mutex);
}
