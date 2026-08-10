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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Unit tests for lock multiple in `libs/uti`
 */

#include <sys/time.h>
#include <cstdio>
#include <pthread.h>
#include <cstdlib>
#include <cstring>

#include "uti/sge_lock.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "test_uti_lock_main.h"

#define MAX_THREADS 6   ///< how many threads contend for the lock

/*-------------------------*/
/* part for the mutex test */
/*-------------------------*/

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t log_once = PTHREAD_ONCE_INIT;

/*--------------------------------*/
/* part for the thread local test */
/*--------------------------------*/
static pthread_key_t state_key;

/** @brief The per-thread state the thread-local storage test stores
 *
 * Two fields rather than one so that a thread writing its own copy cannot be
 * confused with a thread writing past it.
 */
typedef struct {
   int value;    ///< set to a known value at thread start
   int value2;   ///< counted up by the thread, to show the copy is its own
} state_t;

static void *thread_function(void *anArg);

static void state_destroy(void *state) {
   sge_free(&state);
}

static void state_init(state_t *state) {
   state->value = 258;
   state->value2 = 0;
}

/*---------------------------*/
/* sync part between threads */
/*---------------------------*/

/** @brief How the threads tell the main thread they have finished
 *
 * The main thread waits on `cond_var` until `working` reaches zero, rather
 * than joining, so that the elapsed time it reports is the time all threads
 * took together and not the time the slowest one took to be joined.
 */
typedef struct {
   pthread_mutex_t mutex;   ///< guards the other three
   int working;             ///< threads not yet finished
   double time;             ///< the total time they spent
   pthread_cond_t cond_var; ///< signalled as each thread finishes
} sge_control_t;

static int threads = MAX_THREADS;

static sge_control_t Control = {PTHREAD_MUTEX_INITIALIZER, MAX_THREADS, 0.0, PTHREAD_COND_INITIALIZER};

static void has_finished(const char *str, double time_in) {
   DENTER(TOP_LAYER);

   sge_mutex_lock("has_finished", __func__, __LINE__, &Control.mutex);
   Control.working--;
   Control.time += time_in;

   if (Control.working == 0) {
      Control.working = threads;
      Control.time /= threads;
      printf("%s : %fs\n", str, Control.time);
      Control.time = 0.0;
      pthread_cond_broadcast(&Control.cond_var);

   } else {
      struct timespec ts;
      ts.tv_sec = time(nullptr) + 180;
      ts.tv_nsec = 0;
      pthread_cond_timedwait(&Control.cond_var,
                             &Control.mutex, &ts);
   }

   sge_mutex_unlock("has_finished", __func__, __LINE__, &Control.mutex);
   DRETURN_VOID;
}

/*---------------------------*/
/* part of the general setup */
/*---------------------------*/

void set_thread_count(int count) {
   threads = count;
   Control.working = count;
}

int get_thread_demand() {
   int p = MAX_THREADS;  /* min num of threads */

   pthread_key_create(&state_key, &state_destroy);

   return (int) p;
}

static void log_once_init() {
   return;
}

/** @brief The function this test's threads run
 * @return a pointer to the thread function
 */
void *(*get_thread_func())(void *anArg) {
   return thread_function;
}

void *get_thread_func_arg() {
   return nullptr;
}

/****** test_sge_lock_multiple/thread_function() *********************************
*  NAME
*     thread_function() -- Thread function to execute 
*
*  SYNOPSIS
*     static void* thread_function(void *anArg) 
*
*  FUNCTION
*     Acquire multiple locks and sleep. Release the locks. After each 'sge_lock()'
*     and 'sge_unlock()' sleep to increase the probability of interlocked execution. 
*     Note that we deliberately test the boundaries of 'sge_locktype_t'.
*
*  INPUTS
*     void *anArg - thread function arguments 
*
*  RESULT
*     static void* - none
*
*  SEE ALSO
*     test_sge_lock_multiple/get_thrd_func()
*******************************************************************************/
static void *thread_function(void *anArg) {
   struct timeval before;
   struct timeval after;
   double time_new;
   int i;
   int max = 1000000;
   int test = 257;
   int result;

   DENTER(TOP_LAYER);

   has_finished("start", 0.0);

   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      result = test + 1;
      test = result + 1;
   }
   gettimeofday(&after, nullptr);

   time_new = after.tv_usec - before.tv_usec;
   time_new = after.tv_sec - before.tv_sec + (time_new / 1000000);

   has_finished("variable access", time_new);

   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      GET_SPECIFIC(state_t, state, state_init, state_key);
      state->value2 = state->value + 1;
      state->value = state->value2 + 1;
   }
   gettimeofday(&after, nullptr);

   time_new = after.tv_usec - before.tv_usec;
   time_new = after.tv_sec - before.tv_sec + (time_new / 1000000);

   has_finished("thread local   ", time_new);

   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      pthread_once(&log_once, log_once_init);
      {
         GET_SPECIFIC(state_t, state, state_init, state_key);
         state->value2 = state->value + 1;
         state->value = state->value2 + 1;
      }
   }
   gettimeofday(&after, nullptr);

   time_new = after.tv_usec - before.tv_usec;
   time_new = after.tv_sec - before.tv_sec + (time_new / 1000000);

   has_finished("thread local once ", time_new);

   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      sge_mutex_lock("mutex", __func__, __LINE__, &mutex);
      result = test + 1;
      test = result + 1;
      sge_mutex_unlock("mutex", __func__, __LINE__, &mutex);
   }
   gettimeofday(&after, nullptr);

   time_new = after.tv_usec - before.tv_usec;
   time_new = after.tv_sec - before.tv_sec + (time_new / 1000000);

   has_finished("mutex          ", time_new);

   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      SGE_LOCK(LOCK_GLOBAL, LOCK_READ);
      result = test + 1;
      test = result + 1;
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
   }
   gettimeofday(&after, nullptr);

   time_new = after.tv_usec - before.tv_usec;
   time_new = after.tv_sec - before.tv_sec + (time_new / 1000000);

   has_finished("read lock      ", time_new);

   gettimeofday(&before, nullptr);
   for (i = 0; i < max; i++) {
      SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE);
      result = test + 1;
      test = result + 1;
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
   }
   gettimeofday(&after, nullptr);

   time_new = after.tv_usec - before.tv_usec;
   time_new = after.tv_sec - before.tv_sec + (time_new / 1000000);

   has_finished("write lock     ", time_new);


   DRETURN((void *) nullptr);
} /* thread_function */

int validate(int count) {
   return 0;
}
