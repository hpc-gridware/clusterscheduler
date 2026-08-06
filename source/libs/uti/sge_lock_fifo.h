#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2023-2024,2026 HPC-Gridware GmbH
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *  
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief A reader/writer lock that serves waiters in arrival order
 */

#include <pthread.h>

#include <cinttypes>

#include "uti/sge_lock.h"

/// One thread waiting in the queue of a @ref sge_fifo_rw_lock_t
struct sge_fifo_elem_t {
   bool is_reader; ///< is the waiting thread a reader or writer
   bool is_signaled; ///< has this thread already been signaled
   pthread_cond_t cond; ///< condition to wake up a waiting thread
};

/**
 * @brief A reader/writer lock that hands the lock out in arrival order
 *
 * Unlike `pthread_rwlock_t`, waiters are queued, so a steady stream of readers
 * cannot starve a waiting writer. Threads that arrive when the queue is full
 * wait on sge_fifo_rw_lock_t::cond instead and take a position as one frees up.
 */
struct sge_fifo_rw_lock_t {
   pthread_mutex_t mutex; ///< mutex to guard this structure
   pthread_cond_t cond; ///< condition to wake up a waiting thread which got no position in the queue of waiting threads
   sge_fifo_elem_t *array; ///< fifo array where information about waiting threads is stored.
   int head; ///< position of the next thread which gets the lock
   int tail; ///< position in the array where the next thread will be placed which has to wait
   int size; ///< maximum array size
   int reader_active; ///< number of reader threads currently active
   int reader_waiting; ///< number of waiting threads in the queue which try to get a read lock
   int writer_active; ///< number of writer threads currently active (maximum is 1)
   int writer_waiting; ///< number of waiting threads in the queue which try to get the write lock
   int waiting; ///< number of threads which do neither get a lock nor get a free position in the array
   int signaled; ///< number of waiting threads which have been signaled so that they wake up (maximum is 1)
};

/// Largest thread count of any OCS component (e.g. qmaster); sizes the waiting queue
constexpr int MAX_TOTAL_COMPONENT_THREADS = 512;

bool
sge_fifo_lock_init(sge_fifo_rw_lock_t *lock);

bool
sge_fifo_lock(sge_fifo_rw_lock_t *lock, bool is_reader);

bool
sge_fifo_try_lock(sge_fifo_rw_lock_t *lock, bool is_reader);

bool
sge_fifo_ulock(sge_fifo_rw_lock_t *lock, bool is_reader);

/**
 * @brief Log the current state of one lock
 *
 * @param aType which lock to report on
 */
void
sge_fifo_debug(sge_locktype_t aType);

/**
 * @brief Log how long one lock has been held and waited for
 *
 * @param aType which lock to report on
 */
void
sge_debug_time(sge_locktype_t aType);
