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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "ocs_cond.h"
#include "sge_tq.h"


#include "sge_err.h"
#include "sge_log.h"
#include "sge_mtutil.h"
#include "sge_rmon_macros.h"
#include "sge_sl.h"
#include "sge_stdlib.h"
#include "sge_thread_ctrl.h"

#include "msg_common.h"

#define TQ_LAYER BASIS_LAYER
#define TQ_MUTEX_NAME "tq_mutex"

/****** uti/tq/sge_tq_task_compare_type() **************************************
*  NAME
*     sge_tq_task_compare_type() -- compare two tasks
*
*  SYNOPSIS
*     static int sge_tq_task_compare_type(const void *data1, const void *data2)
*
*  FUNCTION
*     This function compares two tasks and returns if the first ('data1')
*     is smaller or bigger that the second ('data2'). It is equal if the
*     type field of the elements are equal.
*
*     If the type field in 'data1' is SGE_TQ_UNKNOWN then the function
*     will always return 0 to indicate that independent of the type field
*     inf 'data2' all tasks are identified as equal. This makes it possible
*     to use this function as a search function where SGE_TQ_UNKNOWN
*     matches any object in a list like a wild card '*'.
*
*  INPUTS
*     const void *data1 - key or task object
*     const void *data2 - task
*
*  RESULT
*     static int - compare result -1, 1 or 0
*
*  NOTES
*     MT-NOTE: sge_tq_task_compare_type() is MT safe
*******************************************************************************/
static int
sge_tq_task_compare_type(const void *data1, const void *data2) {
   int ret;
   sge_tq_task_t *task1 = *(sge_tq_task_t **) data1;
   sge_tq_task_t *task2 = *(sge_tq_task_t **) data2;

   if (task1->type == SGE_TQ_UNKNOWN) {
      /*
       * SGE_TQ_UNKNOWN should match any other type
       */
      ret = 0;
   } else {
      if (task1->type < task2->type) {
         ret = -1;
      } else if (task1->type > task2->type) {
         ret = +1;
      } else {
         ret = 0;
      }
   }
   return ret;
}

/****** uti/tq/sge_tq_task_create() ********************************************
*  NAME
*     sge_tq_task_create() -- creates a task element
*
*  SYNOPSIS
*     static bool
*     sge_tq_task_create(sge_tq_task_t **task, sge_tq_type_t type, void *data)
*
*  FUNCTION
*     Creates a task element that can be added to a queue. Each task
*     has a type and a data pointer. The type makes it possible to identify
*     the type of the data pointer.
*
*  INPUTS
*     sge_tq_task_t **task - new task element
*     sge_tq_type_t type   - type id
*     void *data           - data pointer
*
*  RESULT
*     static bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_tq_task_create() is MT safe
*******************************************************************************/
static bool
sge_tq_task_create(sge_tq_task_t **task, sge_tq_type_t type, void *data) {
   bool ret = true;

   DENTER(TQ_LAYER);
   if (task != nullptr && type != SGE_TQ_UNKNOWN && data != nullptr) {
      sge_tq_task_t *new_task;
      int size = sizeof(sge_tq_task_t);

      new_task = (sge_tq_task_t *) sge_malloc(size);
      if (new_task != nullptr) {
         new_task->type = type;
         new_task->data = data;

         *task = new_task;
      } else {
         sge_err_set(SGE_ERR_MEMORY, MSG_UNABLETOALLOCATEBYTES_DS, size, __func__);
         *task = nullptr;
         ret = false;
      }
   }
   DRETURN(ret);
}

/****** uti/tq/sge_tq_task_destroy() *******************************************
*  NAME
*     sge_tq_task_destroy() -- Destroy a task
*
*  SYNOPSIS
*     static bool sge_tq_task_destroy(sge_tq_task_t **task)
*
*  FUNCTION
*     Destroy a task.
*
*  INPUTS
*     sge_tq_task_t **task - task to be destroyed
*
*  RESULT
*     static bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_tq_task_destroy() is MT safe
*******************************************************************************/
static bool
sge_tq_task_destroy(sge_tq_task_t **task) {
   bool ret = true;

   DENTER(TQ_LAYER);
   if (task != nullptr && *task != nullptr) {
      sge_free(task);
   }
   DRETURN(ret);
}

/****** uti/tq/sge_tq_create() *************************************************
*  NAME
*     sge_tq_create() -- Creates a task queue
*
*  SYNOPSIS
*     bool sge_tq_create(sge_tq_queue_t **queue)
*
*  FUNCTION
*     This function creates a task queue.
*
*  INPUTS
*     sge_tq_queue_t **queue - task queue
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_tq_create() is MT safe
*
*  SEE ALSO
*     uti/tq/sge_tq_destroy()
*******************************************************************************/
bool
sge_tq_create(sge_tq_queue_t **queue) {
   bool ret = true;

   DENTER(TQ_LAYER);
   if (queue != nullptr) {
      sge_tq_queue_t *new_queue;
      int size = sizeof(sge_tq_queue_t);

      new_queue = (sge_tq_queue_t *) sge_malloc(size);
      if (new_queue != nullptr) {
         sge_sl_create(&new_queue->list);
         ocs::uti::condition_initialize(&new_queue->cond);
         new_queue->waiting = 0;

         *queue = new_queue;
      } else {
         sge_err_set(SGE_ERR_MEMORY, MSG_UNABLETOALLOCATEBYTES_DS, size, __func__);
         *queue = nullptr;
         ret = false;
      }
   }
   DRETURN(ret);
}

/****** uti/tq/sge_tq_destroy() ************************************************
*  NAME
*     sge_tq_destroy() -- destroys a queue.
*
*  SYNOPSIS
*     bool sge_tq_destroy(sge_tq_queue_t **queue)
*
*  FUNCTION
*     This function destroys a queue. The queue must be empty otherwise
*     this would produce a memory leak.
*
*  INPUTS
*     sge_tq_queue_t **queue - task queue
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_tq_destroy() is MT safe
*
*  SEE ALSO
*     uti/tq/sge_tq_create()
*******************************************************************************/
void
sge_tq_destroy(sge_tq_queue_t **queue) {
   DENTER(TQ_LAYER);
   if (queue != nullptr && *queue != nullptr) {
      pthread_cond_destroy(&(*queue)->cond);
      sge_sl_destroy(&(*queue)->list, (sge_sl_destroy_f) sge_tq_task_destroy);
      sge_free(queue);
   }
   DRETURN_VOID;
}

/****** uti/tq/sge_tq_get_task_count() *****************************************
*  NAME
*     sge_tq_get_task_count() -- Returns the number of tasks in queue
*
*  SYNOPSIS
*     u_long32 sge_tq_get_task_count(sge_tq_queue_t *queue)
*
*  FUNCTION
*     This function returns the number of tasks in 'queue'
*
*  INPUTS
*     sge_tq_queue_t *queue - task queue
*
*  RESULT
*     u_long32 - number of tasks
*
*  NOTES
*     MT-NOTE: sge_tq_get_task_count() is MT safe
*******************************************************************************/
u_long32
sge_tq_get_task_count(sge_tq_queue_t *queue) {
   u_long32 count = 0;

   DENTER(TQ_LAYER);
   if (queue != nullptr) {
      count = sge_sl_get_elem_count(queue->list);
   }
   DRETURN(count);
}

/****** uti/tq/sge_tq_get_waiting_count() **************************************
*  NAME
*     sge_tq_get_waiting_count() -- Returns number of waiting threads
*
*  SYNOPSIS
*     u_long32 sge_tq_get_waiting_count(sge_tq_queue_t *queue)
*
*  FUNCTION
*     This function returns the number of waiting threads. If this number
*     is bigger than 0 than this indicates that threads are waiting
*     in the function sge_tq_wait_for_task().
*
*  INPUTS
*     sge_tq_queue_t *queue - task queue
*
*  RESULT
*     u_long32 - number of threads waiting
*
*  NOTES
*     MT-NOTE: sge_tq_get_waiting_count() is MT safe
*
*  SEE ALSO
*     uti/tq/sge_tq_wait_for_task()
*******************************************************************************/
u_long32
sge_tq_get_waiting_count(sge_tq_queue_t *queue) {
   u_long32 count = 0;

   DENTER(TQ_LAYER);
   if (queue != nullptr) {
      sge_mutex_lock(TQ_MUTEX_NAME, __func__, __LINE__, sge_sl_get_mutex(queue->list));
      count = queue->waiting;
      sge_mutex_unlock(TQ_MUTEX_NAME, __func__, __LINE__, sge_sl_get_mutex(queue->list));
   }
   DRETURN(count);
}

/****** uti/tq/sge_tq_store_notify() *******************************************
*  NAME
*     sge_tq_store_notify() -- Appends a new task at the end of queue
*
*  SYNOPSIS
*     bool
*     sge_tq_store_notify(sge_tq_queue_t *queue,
*                         sge_tq_type_t type, void *data)
*
*  FUNCTION
*     This function creates a new task using 'type' and 'data'. The new
*     task will then be appended to 'queue'. If there are threads waiting
*     in sge_tq_wait_for_task() then one of those waiting threads will be
*     triggered so that it will wake up.
*
*     If there are multiple threads waiting then it might happen that
*     always the same thread wakes up to catch the task (starvation).
*
*  INPUTS
*     sge_tq_queue_t *queue - task queue
*     sge_tq_type_t type    - task type
*     void *data            - task data
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_tq_store_notify() is MT safe
*
*  SEE ALSO
*     uti/tq/sge_tq_wait_for_task()
*******************************************************************************/
bool
sge_tq_store_notify(sge_tq_queue_t *queue, sge_tq_type_t type, void *data) {
   bool ret = true;

   DENTER(TQ_LAYER);
   if (queue != nullptr && type != SGE_TQ_UNKNOWN && data != nullptr) {
      sge_tq_task_t *new_task = nullptr;

      /* create a new task */
      ret &= sge_tq_task_create(&new_task, type, data);

      /* insert the task in the list and notify one waiting thread if there is one */
      sge_mutex_lock(TQ_MUTEX_NAME, __func__, __LINE__, sge_sl_get_mutex(queue->list));
      if (ret) {
         ret = sge_sl_insert(queue->list, new_task, SGE_SL_BACKWARD);
      }
      if (ret && queue->waiting > 0) {
         sge_tq_wakeup_waiting(queue);
      }
      sge_mutex_unlock(TQ_MUTEX_NAME, __func__, __LINE__, sge_sl_get_mutex(queue->list));
   }
   DRETURN(ret);
}

/****** uti/tq/sge_tq_wakeup_waiting() *****************************************
*  NAME
*     sge_tq_wakeup_waiting() -- wake up all waitng threads
*
*  SYNOPSIS
*     bool sge_tq_wakeup_waiting(sge_tq_queue_t *queue)
*
*  FUNCTION
*     This function wakes up all waiting threads that are blocking in
*     sge_tq_wait_for_task().
*
*  INPUTS
*     sge_tq_queue_t *queue - task queue
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_tq_wakeup_waiting() is MT safe
*
*  SEE ALSO
*     uti/tq/sge_tq_wait_for_task()
*******************************************************************************/
void
sge_tq_wakeup_waiting(sge_tq_queue_t *queue) {
   DENTER(TQ_LAYER);
   if (queue != nullptr) {
      sge_mutex_lock(TQ_MUTEX_NAME, __func__, __LINE__, sge_sl_get_mutex(queue->list));

      /* wake up all threads waiting for a task */
      pthread_cond_broadcast(&(queue->cond));

      sge_mutex_unlock(TQ_MUTEX_NAME, __func__, __LINE__, sge_sl_get_mutex(queue->list));
   }
   DRETURN_VOID;
}

/****** uti/tq/sge_tq_wait_for_task() ******************************************
*  NAME
*     sge_tq_wait_for_task() -- Waits for a task
*
*  SYNOPSIS
*     bool
*     sge_tq_wait_for_task(sge_tq_queue_t *queue, int seconds,
*                          sge_tq_type_t type, void **data);
*
*  FUNCTION
*     This function tries to get a task of 'type'. If there is one
*     available then the data pointer will be returned in 'data'.  If there
*     is none available the this function will sleep a number of 'seconds'
*     before it checks again if there is a task available.
*
*     The sleep time might be interrupted by another thread that adds a
*     task to the queue via sge_tq_store_notify() or by a thread that
*     called sge_tq_wakeup_waiting().
*
*     The function returns either if a task of the correct type is
*     available of if sge_thread_has_shutdown_started() returns true to
*     indicate that the shutdown of the thread/process has been triggered.
*     In that case the function might return with nullptr in *data.
*
*  INPUTS
*     sge_tq_queue_t *queue - task queue
*     int seconds           - max number of seconds to sleep
*     sge_tq_type_t type    - type of the task that should be returned
*     void **data           - nullptr in case of thread/process shutdown
*                             or the data pointer of the task
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_tq_wait_for_task() is MT safe
*
*  SEE ALSO
*     uti/tq/sge_tq_store_notify()
*     uti/tq/sge_tq_wakeup_waiting()
*     uti/thread_ctrl/sge_thread_has_shutdown_started()
*     uti/thread_ctrl/sge_thread_notify_all_waiting()
*******************************************************************************/
bool
sge_tq_wait_for_task(sge_tq_queue_t *queue, int seconds,
                     sge_tq_type_t type, void **data) {
   bool ret = true;

   DENTER(TQ_LAYER);
   if (queue != nullptr && data != nullptr) {
      sge_sl_elem_t *elem = nullptr;
      sge_tq_task_t key{};

      key.type = type;
      *data = nullptr;

      sge_mutex_lock(TQ_MUTEX_NAME, __func__, __LINE__, sge_sl_get_mutex(queue->list));

      /*
       * block until either
       * - number of 'seconds' have elapsed and there is a task in the list
       * - someone notified to wakeup and shutdown of current thread/process has been triggered
       */
      ret = sge_sl_elem_search(queue->list, &elem, &key,
                               sge_tq_task_compare_type, SGE_SL_FORWARD);
      if (ret && elem == nullptr && !sge_thread_has_shutdown_started()) {
         queue->waiting++;
         do {
            ocs::uti::condition_timedwait(&(queue->cond), sge_sl_get_mutex(queue->list), seconds);
            ret = sge_sl_elem_search(queue->list, &elem, &key, sge_tq_task_compare_type, SGE_SL_FORWARD);
         } while (ret && elem == nullptr && !sge_thread_has_shutdown_started());
         queue->waiting--;
      }

      /*
       * If we found a element that matches the key then remove and destroy if and return the data
       */
      if (ret && elem != nullptr) {
         ret = sge_sl_dechain(queue->list, elem);
         if (ret) {
            auto *task = (sge_tq_task_t *)sge_sl_elem_data(elem);
            *data = task->data;
            ret = sge_sl_elem_destroy(&elem, (sge_sl_destroy_f) sge_tq_task_destroy);
         }
      }

      sge_mutex_unlock(TQ_MUTEX_NAME, __func__, __LINE__, sge_sl_get_mutex(queue->list));
   }
   DRETURN(ret);
}

/** @brief Move all elements from src to dst if cmp_func returns true
 *
 * This function moves all elements from src to dst if cmp_func returns true.
 * The elements are moved in the order they are found in the list. The
 * function is thread safe and can be used to move elements from one queue
 * to another as long as no other thread used the same queues as src and dst
 * in reverse order.
 *
 * @param src - source queue
 * @param dst - destination queue
 * @param cmp_func - compare function
 * @return true if all elements were moved, false otherwise
 */
int
sge_tq_move_from_to_if(sge_tq_queue_t *src, sge_tq_queue_t *dst, sge_sl_compare_f cmp_func) {
   DENTER(TOP_LAYER);
   int count = 0;

   // lock sequence is not problem for deadlocks as long as no one else is using src as dst and dst as src at the same time.
   sge_mutex_lock(TQ_MUTEX_NAME, __func__, __LINE__, &src->list->mutex);
   sge_mutex_lock(TQ_MUTEX_NAME, __func__, __LINE__, &dst->list->mutex);

   sge_sl_elem_t *thiz = nullptr;
   sge_sl_elem_t *next = nullptr;
   bool lret = sge_sl_elem_search(src->list, &next, nullptr, cmp_func, SGE_SL_FORWARD);

   while (lret && (thiz = next) != nullptr) {
      // search the next element
      lret &= sge_sl_elem_search(src->list, &next, nullptr, cmp_func, SGE_SL_FORWARD);

      // move the element from src to dst
      sge_sl_dechain(src->list, thiz);
      sge_sl_elem_insert(dst->list, thiz, SGE_SL_BACKWARD);

      // count the number of moved elements
      count++;
   }

   sge_mutex_unlock(TQ_MUTEX_NAME, __func__, __LINE__, &dst->list->mutex);
   sge_mutex_unlock(TQ_MUTEX_NAME, __func__, __LINE__, &src->list->mutex);

   DRETURN(count);
}

