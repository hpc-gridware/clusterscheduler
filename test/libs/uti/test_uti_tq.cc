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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fnmatch.h>

#include "uti/sge_err.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_thread_ctrl.h"
#include "uti/sge_tq.h"

static int s_fail = 0;

#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/*
 * Producer and consumer maximum should be a multiple of 2 and 3
 */
#define TEST_SL_MAX_CONSUMER 24
#define TEST_SL_MAX_PRODUCER 24

#define TEST_SL_MAX_ELEMENTS 10000

struct _test_sl_thread_cp_t {
   pthread_mutex_t mutex;
   sge_tq_queue_t *queue;
   uint32_t counter;
   dstring sequence;
   volatile bool do_terminate;
   bool thread_error; ///< set by any thread that detects an unexpected outcome
};

typedef struct _test_sl_thread_cp_t test_sl_thread_cp_t;

// matches every element regardless of type; used by sge_tq_move_from_to_if tests
static int match_all(const void * /*key*/, const void * /*elem*/) { return 0; }

void *
test_thread_consumer_template(void *arg, sge_tq_type_t type, const char *type_string) {
   void *ret = nullptr;
   test_sl_thread_cp_t *global = (test_sl_thread_cp_t *) arg;

   DENTER(TOP_LAYER);
   while (global->do_terminate != true) {
      const char *string;

      /* consume: first element */
      sge_tq_wait_for_task(global->queue, 1, type, (void **) &string);

      if (string != nullptr) {
         if (fnmatch(type_string, string, 0) != 0) {
            fprintf(stderr, "got %s from queue and not %s\n", string, type_string);
            pthread_mutex_lock(&global->mutex);
            global->thread_error = true;
            pthread_mutex_unlock(&global->mutex);
            break;
         }
      } else {
         if (!sge_thread_has_shutdown_started()) {
            fprintf(stderr, "got nullptr from queue although thread was not terminated\n");
            pthread_mutex_lock(&global->mutex);
            global->thread_error = true;
            pthread_mutex_unlock(&global->mutex);
            break;
         }
      }

      pthread_mutex_lock(&global->mutex);
      sge_dstring_append_char(&global->sequence, 'c');
      pthread_mutex_unlock(&global->mutex);
   }
   DRETURN(ret);
}

void *
test_thread_producer_template(void *arg, sge_tq_type_t type, const char *type_string) {
   void *ret = nullptr;
   test_sl_thread_cp_t *global = (test_sl_thread_cp_t *) arg;

   DENTER(TOP_LAYER);
   while (global->do_terminate != true) {
      /* produce: new element */
      sge_tq_store_notify(global->queue, type, (void *) type_string);

      /* trigger termination */
      pthread_mutex_lock(&global->mutex);
      sge_dstring_append_char(&global->sequence, 'p');
      global->counter++;
      if (global->counter > TEST_SL_MAX_ELEMENTS) {
         global->do_terminate = true;
         sge_thread_notify_all_waiting();
      }
      pthread_mutex_unlock(&global->mutex);
   }
   DRETURN(ret);
}

void *
test_thread_consumer_type1(void *arg) {
   return test_thread_consumer_template(arg, SGE_TQ_TYPE1, "type_1");
}

void *
test_thread_consumer_type2(void *arg) {
   return test_thread_consumer_template(arg, SGE_TQ_TYPE2, "type_2");
}

void *
test_thread_consumer_unknown(void *arg) {
   return test_thread_consumer_template(arg, SGE_TQ_UNKNOWN, "type_?");
}

void *
test_thread_producer_type1(void *arg) {
   return test_thread_producer_template(arg, SGE_TQ_TYPE1, "type_1");
}

void *
test_thread_producer_type2(void *arg) {
   return test_thread_producer_template(arg, SGE_TQ_TYPE2, "type_2");
}

/*
 * Scenario: Producer - Consumer
 * - TEST_SL_MAX_CONSUMER consumer threads will be created
 * - TEST_SL_MAX_PRODUCER producer threads will be created
 * - consumer threads wait for an element in a global list
 * - producer threads put an element into the list
 * - consumer und producer append a c or p letter into a global string
 * - the producer creating the 10000th element triggers termination of threads
 * - global string contains execution sequence of p and c threads
 */
static bool
test_mt_consumer_producer() {
   bool ret = true;
   test_sl_thread_cp_t global;

   DENTER(TOP_LAYER);

   // create a list
   memset(&global, 0, sizeof(test_sl_thread_cp_t));
   global.do_terminate = false;
   global.counter = 0;
   global.thread_error = false;
   pthread_mutex_init(&global.mutex, nullptr);
   ret = sge_tq_create(&global.queue);

   // spawn threads
   if (ret) {
      pthread_t consumer[TEST_SL_MAX_CONSUMER];
      pthread_t producer[TEST_SL_MAX_PRODUCER];

      for (size_t i = 0; i < TEST_SL_MAX_CONSUMER; i++) {
         if (i < TEST_SL_MAX_CONSUMER / 3) {
            pthread_create(&(consumer[i]), nullptr, test_thread_consumer_type1, &global);
         } else if (i < TEST_SL_MAX_CONSUMER * 2 / 3) {
            pthread_create(&(consumer[i]), nullptr, test_thread_consumer_type2, &global);
         } else {
            pthread_create(&(consumer[i]), nullptr, test_thread_consumer_unknown, &global);
         }
      }
      for (size_t i = 0; i < TEST_SL_MAX_PRODUCER; i++) {
         if (i < TEST_SL_MAX_PRODUCER / 2) {
            pthread_create(&(producer[i]), nullptr, test_thread_producer_type1, &global);
         } else {
            pthread_create(&(producer[i]), nullptr, test_thread_producer_type2, &global);
         }
      }

      for (size_t i = 0; i < TEST_SL_MAX_CONSUMER; i++) {
         pthread_join(consumer[i], nullptr);
      }
      for (size_t i = 0; i < TEST_SL_MAX_PRODUCER; i++) {
         pthread_join(producer[i], nullptr);
      }
   }

   ret = ret && !global.thread_error && global.counter > TEST_SL_MAX_ELEMENTS;

   // cleanup
   sge_dstring_free(&global.sequence);
   pthread_mutex_destroy(&global.mutex);
   sge_tq_destroy(&global.queue);

   DRETURN(ret);
}

// single-threaded tests for the basic queue API
static bool
test_basic_api(int &id) {
   bool ok = true;

   DENTER(TOP_LAYER);

   printf("\n--- queue lifecycle ---\n");
   sge_tq_queue_t *q = nullptr;
   // T01: create succeeds and yields a non-null queue
   bool created = sge_tq_create(&q);
   CHECK(id, "sge_tq_create returns true", created); id++;
   CHECK(id, "sge_tq_create produces non-null queue", q != nullptr); id++;
   if (!created || q == nullptr) {
      // cannot continue without a valid queue
      ok = false;
      DRETURN(ok);
   }
   // T03: freshly created queue has no tasks
   CHECK(id, "initial task count is 0", sge_tq_get_task_count(q) == 0); id++;
   // T04: freshly created queue has no waiting threads
   CHECK(id, "initial waiting count is 0", sge_tq_get_waiting_count(q) == 0); id++;

   printf("\n--- store and count ---\n");
   // T05: store a TYPE1 task
   CHECK(id, "sge_tq_store_notify(TYPE1) returns true",
         sge_tq_store_notify(q, SGE_TQ_TYPE1, (void *)"type_1")); id++;
   // T06: count reflects the stored task
   CHECK(id, "task count is 1 after first store", sge_tq_get_task_count(q) == 1); id++;
   // T07: store a second task of a different type
   CHECK(id, "sge_tq_store_notify(TYPE2) returns true",
         sge_tq_store_notify(q, SGE_TQ_TYPE2, (void *)"type_2")); id++;
   // T08: count reflects both stored tasks
   CHECK(id, "task count is 2 after second store", sge_tq_get_task_count(q) == 2); id++;

   printf("\n--- retrieve tasks by type ---\n");
   // T09: retrieve TYPE1 (queue is pre-populated so wait_for_task returns immediately)
   {
      void *data = nullptr;
      bool got = sge_tq_wait_for_task(q, 1, SGE_TQ_TYPE1, &data);
      CHECK(id, "wait_for_task(TYPE1) returns true", got); id++;
      CHECK(id, "wait_for_task(TYPE1) delivers correct data",
            data != nullptr && strcmp((const char *)data, "type_1") == 0); id++;
   }
   // T11: TYPE2 still in queue after consuming TYPE1
   CHECK(id, "task count is 1 after consuming TYPE1", sge_tq_get_task_count(q) == 1); id++;
   // T12: retrieve TYPE2
   {
      void *data = nullptr;
      bool got = sge_tq_wait_for_task(q, 1, SGE_TQ_TYPE2, &data);
      CHECK(id, "wait_for_task(TYPE2) returns true", got); id++;
      CHECK(id, "wait_for_task(TYPE2) delivers correct data",
            data != nullptr && strcmp((const char *)data, "type_2") == 0); id++;
   }
   // T14: queue is empty after consuming both tasks
   CHECK(id, "task count is 0 after consuming both", sge_tq_get_task_count(q) == 0); id++;

   printf("\n--- UNKNOWN type acts as wildcard consumer ---\n");
   // T15: SGE_TQ_UNKNOWN matches any task type when consuming
   CHECK(id, "sge_tq_store_notify(TYPE1) for wildcard test",
         sge_tq_store_notify(q, SGE_TQ_TYPE1, (void *)"type_1")); id++;
   {
      void *data = nullptr;
      bool got = sge_tq_wait_for_task(q, 1, SGE_TQ_UNKNOWN, &data);
      CHECK(id, "wait_for_task(UNKNOWN) returns true", got); id++;
      CHECK(id, "wait_for_task(UNKNOWN) retrieves any task",
            data != nullptr && strcmp((const char *)data, "type_1") == 0); id++;
   }
   // T18: queue is empty after wildcard consume
   CHECK(id, "task count is 0 after wildcard consume", sge_tq_get_task_count(q) == 0); id++;

   printf("\n--- store with UNKNOWN type is a no-op ---\n");
   // T19: UNKNOWN is reserved for consuming — storing with it silently does nothing
   CHECK(id, "sge_tq_store_notify(UNKNOWN) returns true",
         sge_tq_store_notify(q, SGE_TQ_UNKNOWN, (void *)"x")); id++;
   // T20: count must remain 0 — UNKNOWN store does not enqueue a task
   CHECK(id, "task count is still 0 after UNKNOWN store",
         sge_tq_get_task_count(q) == 0); id++;

   printf("\n--- move_from_to_if ---\n");
   {
      sge_tq_queue_t *src = nullptr;
      sge_tq_queue_t *dst = nullptr;
      sge_tq_create(&src);
      sge_tq_create(&dst);
      sge_tq_store_notify(src, SGE_TQ_TYPE1, (void *)"type_1");
      sge_tq_store_notify(src, SGE_TQ_TYPE2, (void *)"type_2");
      // T21: move all elements from src to dst using a match-all predicate
      int moved = sge_tq_move_from_to_if(src, dst, match_all);
      CHECK(id, "move_from_to_if returns 2 (all elements moved)", moved == 2); id++;
      // T22: source is empty after the move
      CHECK(id, "src task count is 0 after move", sge_tq_get_task_count(src) == 0); id++;
      // T23: destination holds all moved elements
      CHECK(id, "dst task count is 2 after move", sge_tq_get_task_count(dst) == 2); id++;
      sge_tq_destroy(&src);
      sge_tq_destroy(&dst);
   }

   printf("\n--- null-safety ---\n");
   // T24: count functions must not crash on a null queue pointer
   CHECK(id, "sge_tq_get_task_count(nullptr) returns 0",
         sge_tq_get_task_count(nullptr) == 0); id++;
   CHECK(id, "sge_tq_get_waiting_count(nullptr) returns 0",
         sge_tq_get_waiting_count(nullptr) == 0); id++;

   sge_tq_destroy(&q);
   DRETURN(ok);
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_tq");
   int id = 1;

   // single-threaded basic API tests must run before test_mt_consumer_producer
   // because that test calls sge_thread_notify_all_waiting() which sets the
   // global shutdown flag for the rest of the process lifetime
   test_basic_api(id);

   printf("\n--- producer-consumer (24x24 threads, 10000 elements) ---\n");
   // T26: multi-threaded producer-consumer completes without thread errors
   CHECK(id, "producer-consumer: all threads complete without errors",
         test_mt_consumer_producer()); id++;

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
