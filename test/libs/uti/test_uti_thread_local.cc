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
 *   Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>
#include <pthread.h>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include <sge_log.h>

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

static pthread_key_t thread_local_key;
static pthread_key_t thread_local_key2;

// counts destructor calls for thread_local_key; protected by destructor_mutex
static int destructor_call_count = 0;
static pthread_mutex_t destructor_mutex = PTHREAD_MUTEX_INITIALIZER;

// pointer saved by the null-setspecific test so main can free it manually
static void *null_test_data = nullptr;

static void
free_local_storage(void *data) {
   pthread_mutex_lock(&destructor_mutex);
   destructor_call_count++;
   pthread_mutex_unlock(&destructor_mutex);
   free(data);
}

struct thread_result_t {
   bool pre_init_null;     // getspecific before setspecific returned nullptr
   bool post_init_correct; // getspecific immediately after setspecific returned correct value
   bool after_yield_correct; // getspecific after sched_yield still returns correct value
};

static void *
t1_main(void *args) {
   DENTER(TOP_LAYER);
   auto *result = static_cast<thread_result_t *>(args);

   // check that TLS is nullptr before any setspecific in this thread
   result->pre_init_null = (pthread_getspecific(thread_local_key) == nullptr);

   // initialize TLS
   char *data = static_cast<char *>(sge_malloc(100));
   SGE_ASSERT(data != nullptr);
   snprintf(data, 100, "Thread 1");
   pthread_setspecific(thread_local_key, data);

   // verify immediately after setspecific
   char *got = static_cast<char *>(pthread_getspecific(thread_local_key));
   result->post_init_correct = (got != nullptr && strcmp(got, "Thread 1") == 0);

   // yield to allow t2 to run, then verify isolation
   sched_yield();
   got = static_cast<char *>(pthread_getspecific(thread_local_key));
   result->after_yield_correct = (got != nullptr && strcmp(got, "Thread 1") == 0);

   DRETURN(nullptr);
}

static void *
t2_main(void *args) {
   DENTER(TOP_LAYER);
   auto *result = static_cast<thread_result_t *>(args);

   result->pre_init_null = (pthread_getspecific(thread_local_key) == nullptr);

   char *data = static_cast<char *>(sge_malloc(100));
   SGE_ASSERT(data != nullptr);
   snprintf(data, 100, "Thread 2");
   pthread_setspecific(thread_local_key, data);

   char *got = static_cast<char *>(pthread_getspecific(thread_local_key));
   result->post_init_correct = (got != nullptr && strcmp(got, "Thread 2") == 0);

   sched_yield();
   got = static_cast<char *>(pthread_getspecific(thread_local_key));
   result->after_yield_correct = (got != nullptr && strcmp(got, "Thread 2") == 0);

   DRETURN(nullptr);
}

struct two_key_result_t {
   bool key1_correct;  // getspecific(key1) returned expected value
   bool key2_correct;  // getspecific(key2) returned expected value
   bool keys_differ;   // the two values are distinct pointers (no aliasing)
};

static void *
two_key_thread_main(void *args) {
   DENTER(TOP_LAYER);
   auto *result = static_cast<two_key_result_t *>(args);

   // set different values on each key and verify they are independent
   char *v1 = static_cast<char *>(sge_malloc(100));
   char *v2 = static_cast<char *>(sge_malloc(100));
   SGE_ASSERT(v1 != nullptr && v2 != nullptr);
   snprintf(v1, 100, "key1-value");
   snprintf(v2, 100, "key2-value");
   pthread_setspecific(thread_local_key, v1);
   pthread_setspecific(thread_local_key2, v2);

   char *got1 = static_cast<char *>(pthread_getspecific(thread_local_key));
   char *got2 = static_cast<char *>(pthread_getspecific(thread_local_key2));
   result->key1_correct = (got1 != nullptr && strcmp(got1, "key1-value") == 0);
   result->key2_correct = (got2 != nullptr && strcmp(got2, "key2-value") == 0);
   result->keys_differ  = (got1 != got2);

   // v1 freed by free_local_storage destructor on exit; v2 freed by free() destructor
   DRETURN(nullptr);
}

static void *
null_setspecific_thread_main(void * /*args*/) {
   DENTER(TOP_LAYER);

   // allocate and record the pointer, then null out the key before exit
   char *data = static_cast<char *>(sge_malloc(100));
   SGE_ASSERT(data != nullptr);
   snprintf(data, 100, "will-be-nulled");
   null_test_data = data;

   pthread_setspecific(thread_local_key, data);
   // overwrite with nullptr: per POSIX the destructor must NOT fire on exit
   pthread_setspecific(thread_local_key, nullptr);

   DRETURN(nullptr);
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_thread_local");
   int id = 1;

   pthread_key_create(&thread_local_key, free_local_storage);
   pthread_key_create(&thread_local_key2, free); // key2 uses plain free; not counted

   thread_result_t r1{}, r2{};
   pthread_t t1, t2;
   pthread_create(&t1, nullptr, t1_main, &r1);
   pthread_create(&t2, nullptr, t2_main, &r2);
   pthread_join(t1, nullptr);
   pthread_join(t2, nullptr);

   printf("\n--- TLS isolation ---\n");
   // T01/T02: getspecific returns nullptr before setspecific in each thread
   CHECK(id, "thread 1: getspecific nullptr before setspecific", r1.pre_init_null); id++;
   CHECK(id, "thread 2: getspecific nullptr before setspecific", r2.pre_init_null); id++;
   // T03/T04: each thread sees its own value immediately after setspecific
   CHECK(id, "thread 1: getspecific returns \"Thread 1\" after setspecific", r1.post_init_correct); id++;
   CHECK(id, "thread 2: getspecific returns \"Thread 2\" after setspecific", r2.post_init_correct); id++;
   // T05/T06: values survive a sched_yield (i.e. the other thread's setspecific does not overwrite)
   CHECK(id, "thread 1: TLS still \"Thread 1\" after yield", r1.after_yield_correct); id++;
   CHECK(id, "thread 2: TLS still \"Thread 2\" after yield", r2.after_yield_correct); id++;

   printf("\n--- TLS destructor ---\n");
   // T07: destructor called exactly once per thread (2 total) after both threads exit
   CHECK(id, "destructor called exactly 2 times after both threads join", destructor_call_count == 2); id++;

   printf("\n--- multiple TLS keys are independent ---\n");
   {
      two_key_result_t rk{};
      pthread_t tk;
      pthread_create(&tk, nullptr, two_key_thread_main, &rk);
      pthread_join(tk, nullptr);
      // T08/T09: each key holds its own distinct value
      CHECK(id, "key1 and key2 hold different values (no aliasing)", rk.keys_differ); id++;
      CHECK(id, "key1 value correct after key2 was set", rk.key1_correct); id++;
      CHECK(id, "key2 value correct after key1 was set", rk.key2_correct); id++;
   }

   printf("\n--- setspecific(nullptr) suppresses destructor ---\n");
   {
      int count_before = destructor_call_count;
      pthread_t tn;
      pthread_create(&tn, nullptr, null_setspecific_thread_main, nullptr);
      pthread_join(tn, nullptr);
      // T11: destructor must NOT fire when TLS was set to nullptr before thread exit
      CHECK(id, "destructor not called when key was nulled before exit", destructor_call_count == count_before); id++;
      // free the allocation ourselves since the destructor did not run
      free(null_test_data);
      null_test_data = nullptr;
   }

   printf("\n--- main thread can use TLS ---\n");
   {
      char *main_data = static_cast<char *>(sge_malloc(100));
      SGE_ASSERT(main_data != nullptr);
      snprintf(main_data, 100, "main-thread");
      pthread_setspecific(thread_local_key, main_data);
      char *got = static_cast<char *>(pthread_getspecific(thread_local_key));
      // T12: main thread getspecific returns the value set by main thread setspecific
      CHECK(id, "main thread: getspecific returns value set by setspecific", got != nullptr && strcmp(got, "main-thread") == 0); id++;
      // destructor is NOT called for main thread on return; free manually to avoid leak
      pthread_setspecific(thread_local_key, nullptr);
      free(main_data);
   }

   pthread_key_delete(thread_local_key);
   pthread_key_delete(thread_local_key2);
   pthread_mutex_destroy(&destructor_mutex);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
