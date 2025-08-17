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
 *   Copyright: 2003 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <pthread.h>

#include "uti/sge_lock_fifo.h"

#include "ocs_cond.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"

static const int FIFO_LOCK_QUEUE_LENGTH = 512;

/**
 * initialize a fifo read/write lock.
 *
 * This function is used to initialize a fifo read/write lock.
 *
 * On success the function returns true. If the lock object can't be initialized then the function will return with
 * false.
 *
 * @param lock fifo lock object
 * @return true on success false in case of error
 */
bool
sge_fifo_lock_init(sge_fifo_rw_lock_t *lock) {
   bool ret = true;
   int lret = pthread_mutex_init(&(lock->mutex), nullptr);
   if (lret == 0) {
      lock->array = (sge_fifo_elem_t *) sge_malloc(sizeof(sge_fifo_elem_t) * FIFO_LOCK_QUEUE_LENGTH);

      if (lock->array != nullptr) {
         int i;

         for (i = 0; i < FIFO_LOCK_QUEUE_LENGTH; i++) {
            lock->array[i].is_reader = false;
            lock->array[i].is_signaled = false;
            lret = ocs::uti::condition_initialize(&(lock->array[i].cond));
            if (lret != 0) {
               ret = false;
               break;
            }
         }
         if (lret == 0) {
            lret = ocs::uti::condition_initialize(&(lock->cond));
            if (lret == 0) {
               lock->head = 0;
               lock->tail = 0;
               lock->size = FIFO_LOCK_QUEUE_LENGTH;
               lock->reader_active = 0;
               lock->reader_waiting = 0;
               lock->writer_active = 0;
               lock->writer_waiting = 0;
               lock->waiting = 0;
               lock->signaled = 0;
            } else {
               ret = false;
            }
         } else {
            // has already been handled in the for loop above
         }
      } else {
         ret = false;
      }
   } else {
      ret = false;
   }
   return ret;
}

/**
 * Acquire a read/write lock.
 *
 * A call to this function acquires either a read or a write lock depending on the value of "is_reader".
 * If the value of "is_reader" is "true" the function returns as soon as it gets the read lock. This is the case if
 * there is noone currently holding the write lock and if there was noone previously trying to get the write lock.
 *
 * If the value of "is_reader" is "false" then the function returns as soon as it gets the write lock. This is only
 * the case if there is noone holding a read or write lock and only if there was noone else who tried to get the read
 * or write lock.
 *
 * A thread my hold multiple concurrent read locks. If so the corresponding sge_fifo_unlock() function has to be
 * called once for each lock obtained.
 *
 * Multiple threads might obtain a read lock whereas only one thread can have the write lock at the same time.
 *
 * Threads which can't acquire a read or write lock block till the lock is available. A certain number of blocking
 * threads (defined by the define FIFO_LOCK_QUEUE_LENGTH) wait in a queue so that each of those threads has a chance
 * to get the lock.
 *
 * If more than FIFO_LOCK_QUEUE_LENGTH threads try to get the lock it might happen that then there are threads which
 * will never get the lock (This behaviour depends on the implementation of the pthread library).
 *
 * A read/write lock has to be initialized with sge_fifo_lock_init() before it can be used with this function.
 *
 * @param lock FIFO lock object
 * @param is_reader true if a read lock should be acquired
 * @param do_mutex_lock true (default) if the internal lock was not already acquired before this function call.
 * @return true if the requested lock could be acquired.
 */
static bool
sge_fifo_lock_internal(sge_fifo_rw_lock_t *lock, bool is_reader, bool do_mutex_lock = true) {
   bool ret = true;

   // lock the lock-structure
   int lret = 0;
   if (do_mutex_lock) {
      lret = pthread_mutex_lock(&(lock->mutex));
   }
   if (lret == 0) {
      bool do_wait = false;
      bool do_wait_in_queue = false;

      // If the current thread has to wait later on and if there is no place available in the list of waiting threads
      // then wait till there is space in the queue.
      //
      // read lock:
      //    if the queue is full and this readers can't get the lock because a writer has it already then this thread
      //    will wait either:
      //       - till there is a place in the queue or
      //       - till the writer released the lock so that this reader can have it
      //
      // write lock:
      //    if the queue is full then wait till there is space available
      do {
         do_wait = (bool) ((lock->reader_waiting + lock->writer_waiting) == FIFO_LOCK_QUEUE_LENGTH);
         if (do_wait) {
            lock->waiting++;
            pthread_cond_wait(&(lock->cond), &(lock->mutex));
            lock->waiting--;
         }
      } while (do_wait);

      // Append the thread to the queue if it is necessary
      //
      // read lock:
      //    if there is currently a writer active or waiting or another thread is currently waking up (because it
      //    was signaled) then this reader has to wait in queue. If there are other readers active or none is active
      //    then this reader can continue.
      //
      // write lock:
      //    the writer has to wait in queue if there is an active reader or writer or if someone is currently waking up
      if (is_reader) {
         do_wait_in_queue = (bool) (lock->writer_active + lock->writer_waiting + lock->signaled > 0);
      } else {
         do_wait_in_queue = (bool) ((lock->writer_active + lock->reader_active + lock->signaled > 0));
      }
      if (do_wait_in_queue) {
         int index;

         // position the tail pointer behind the element which will be filled now. This will be the place where the
         // next waiting thread will be stored.
         index = lock->tail;
         lock->tail++;

         // check if the new tail is behind the position of the allocated array. Move then to the first array element.
         if (lock->tail == lock->size) {
            lock->tail = 0;
         }

         // store information about the thread which will wait
         lock->array[index].is_reader = is_reader;
         lock->array[index].is_signaled = false;

         // block this thread now till it gets a signal to continue. The signal will be sent by an unlock call of
         // another reader or writer which hat the lock before.
         while (!lock->array[index].is_signaled) {
            if (is_reader) {
               lock->reader_waiting++;
            } else {
               lock->writer_waiting++;
            }
            pthread_cond_wait(&(lock->array[index].cond), &(lock->mutex));
            if (is_reader) {
               lock->reader_waiting--;
            } else {
               lock->writer_waiting--;
            }
         }

         // remove this thread from the signaled threads counter
         if (lock->array[index].is_signaled) {
            lock->signaled--;
         }

         // This thread will get the lock because it is the first in the queue. Remove the information about this
         // thread from the queue.
         index = lock->head;
         lock->head++;

         // check if the new head is behind the position of the allocated array. Move then to the first array element.
         if (lock->head == lock->size) {
            lock->head = 0;
         }

         // if this thread is a reader and if there is at least one additional thread in the queue and if that thread
         // is also a reader then wake it so that they can do work simultaneously

         if (lock->array[index].is_reader && lock->reader_waiting > 0 && lock->array[lock->head].is_reader) {
            lock->array[lock->head].is_signaled = true;
            lock->signaled++;
            pthread_cond_signal(&(lock->array[lock->head].cond));
         }

         // there is now space in the queue available. if ther are threads waiting outside the queue then notify
         // one so that it can append at the end.
         if (lock->waiting > 0) {
            pthread_cond_signal(&(lock->cond));
         }

         // this is not necessary, but it might make debugging easier. pre-initialize the array element with
         // predefined values which indicate that this entry is 'empty'
         lock->array[index].is_reader = false;
         lock->array[index].is_signaled = false;
      }

      // now the thread has the lock. increase the counter.
      if (is_reader) {
         lock->reader_active++;
      } else {
         lock->writer_active++;
      }

      // unlock the lock-structure
      lret = 0;
      if (do_mutex_lock) {
         lret = pthread_mutex_unlock(&(lock->mutex));
      }
      if (lret != 0) {
         ret = false;
      }
   } else {
      ret = false;
   }
   return ret;
}

/**
 * Blocks the calling thread till it can acquire the requested lock.
 *
 * If is_reader is true then the function tries to acquire a read lock otherwise a write lock
 * For details see sge_fifo_lock_internal()
 *
 * @param lock FIFO lock object
 * @param is_reader true if a read lock should be acquired
 * @return true if the requested lock could be acquired.
 */
bool
sge_fifo_lock(sge_fifo_rw_lock_t *lock, bool is_reader) {
   return sge_fifo_lock_internal(lock, is_reader);
}

/**
 * Trys to acquire the lock but does not block the calling thread if the lock cannot be acquired now.
 *
 * @param lock FIFO lock object
 * @param is_reader true if a read lock should be acquired
 * @return true if the requested lock could be acquired. Returns immediately with false if lock cannot be granted.
 */
bool
sge_fifo_try_lock(sge_fifo_rw_lock_t *lock, bool is_reader) {
   bool ret = false;

   int lret = pthread_mutex_lock(&(lock->mutex));
   if (lret == 0) {
      bool queue_already_full = ((lock->reader_waiting + lock->writer_waiting) == FIFO_LOCK_QUEUE_LENGTH);

      bool someone_blocks_us = false;
      if (is_reader) {
         someone_blocks_us = (lock->writer_active + lock->writer_waiting > 0);
      } else {
         someone_blocks_us = (lock->writer_active + lock->reader_active + lock->signaled > 0);
      }

      if (queue_already_full || someone_blocks_us) {
         // we can't get the lock immediately
         ret = false;
      } else {
         // we can be sure that we can get the lock
         ret = sge_fifo_lock_internal(lock, is_reader, false);
      }
      pthread_mutex_unlock(&(lock->mutex));
   }
   return ret;
}

/**
 * Release a read or write lock.
 *
 * Releases a read or write lock previously obtained with sge_fifo_lock() or sge_fifo_unlock().
 *
 * @param lock lock object
 * @param is_reader true for a read lock
 * @return true on success false otherwise
 */
bool
sge_fifo_ulock(sge_fifo_rw_lock_t *lock, bool is_reader) {
   bool ret = true;

   // lock the lock-structure
   int lret = pthread_mutex_lock(&(lock->mutex));
   if (lret == 0) {

      // decrease the counter.
      if (is_reader) {
         lock->reader_active--;
      } else {
         lock->writer_active--;
      }

      // notify the next waiting thread if there is one
      if ((lock->reader_active + lock->writer_active + lock->signaled) == 0 &&
          (lock->reader_waiting + lock->writer_waiting > 0)) {
         lock->array[lock->head].is_signaled = true;
         lock->signaled++;
         pthread_cond_signal(&(lock->array[lock->head].cond));
      }

      // unlock the lock-structure
      lret = pthread_mutex_unlock(&(lock->mutex));
      if (lret != 0) {
         ret = false;
      }
   } else {
      ret = false;
   }
   return ret;
}

/** @brief Returns the number of threads currently waiting for a lock.
 *
 * @param lock FIFO lock object
 * @param reader_active number of threads currently holding a read lock
 * @param reader_waiting number of threads currently waiting for a read lock
 * @param writer_active number of threads currently holding a write lock (max 1)
 * @param writer_waiting number of threads currently waiting for a write lock
 * @param waiting number of threads currently waiting for a lock (outside the queue)
 * @param signaled number of threads currently waiting for a lock and which have been signaled
 */
void
sge_fifo_get_details(sge_fifo_rw_lock_t *lock, int *reader_active, int *reader_waiting, int *writer_active, int *writer_waiting, int *waiting, int *signaled) {
   int lret = pthread_mutex_lock(&lock->mutex);
   if (lret == 0) {
      *reader_active = lock->reader_active;
      *reader_waiting = lock->reader_waiting;
      *writer_active = lock->writer_active;
      *writer_waiting = lock->writer_waiting;
      *waiting = lock->waiting;
      *signaled = lock->signaled;
      pthread_mutex_unlock(&lock->mutex);
   }
}

