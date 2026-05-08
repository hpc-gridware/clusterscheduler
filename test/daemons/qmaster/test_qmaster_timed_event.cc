/*___INFO__MARK_BEGIN_NEW__*/
/*************************************************************************
 *
 *  Copyright 2003 Sun Microsystems, Inc.
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 ************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <cstdio>
#include <cstring>

#include "uti/sge_component.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/cull/sge_all_listsL.h"

#include "sge_qmaster_timed_event.h"

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

// handler stubs — event delivery requires sge_thread_timer, which is not started here
void calendar_event_handler(te_event_t /*anEvent*/, monitoring_t * /*monitor*/) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void signal_resend_event_handler(te_event_t /*anEvent*/, monitoring_t * /*monitor*/) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void job_resend_event_handler(te_event_t /*anEvent*/, monitoring_t * /*monitor*/) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sge_qmaster_timed_event");
   component_set_daemonized(true);
   sge_prof_set_enabled(false);
   lInit(nmv);
   te_init();

   te_register_event_handler(calendar_event_handler, TYPE_CALENDAR_EVENT);
   te_register_event_handler(signal_resend_event_handler, TYPE_SIGNAL_RESEND_EVENT);
   te_register_event_handler(job_resend_event_handler, TYPE_JOB_RESEND_EVENT);

   // --- event creation and accessors ---
   printf("\n--- event creation and accessors ---\n");
   {
      te_event_t ev = te_new_event(0, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 42, 43, "test-key");
      CHECK(1, "te_new_event returns non-null", ev != nullptr);
      CHECK(2, "te_get_type returns correct type", te_get_type(ev) == TYPE_CALENDAR_EVENT);
      CHECK(3, "te_get_first_numeric_key returns 42", te_get_first_numeric_key(ev) == 42);
      CHECK(4, "te_get_second_numeric_key returns 43", te_get_second_numeric_key(ev) == 43);
      char *key = te_get_alphanumeric_key(ev);
      CHECK(5, "te_get_alphanumeric_key returns test-key", key != nullptr && strcmp(key, "test-key") == 0);
      sge_free(&key);
      te_free_event(&ev);
      CHECK(6, "te_free_event nulls the pointer", ev == nullptr);
   }

   // --- delete from empty list ---
   printf("\n--- delete from empty list ---\n");
   CHECK(7, "te_delete_one_time_event on empty list returns 0",
         te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, "no-event") == 0);

   // --- add and delete by key ---
   printf("\n--- add and delete by key ---\n");
   {
      // single event: add then delete
      te_event_t ev = te_new_event(0, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "ev-a");
      te_add_event(ev);
      te_free_event(&ev);
      CHECK(8, "delete by key returns 1",
            te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, "ev-a") == 1);
      CHECK(9, "second delete of same key returns 0",
            te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, "ev-a") == 0);

      // 3 events with same key: all deleted in one call
      for (int i = 0; i < 3; i++) {
         te_event_t e = te_new_event(0, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "ev-b");
         te_add_event(e);
         te_free_event(&e);
      }
      CHECK(10, "delete 3 events with same key returns 3",
            te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, "ev-b") == 3);

      // 2 events with different keys: each deleted independently
      te_event_t e1 = te_new_event(0, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "ev-c1");
      te_event_t e2 = te_new_event(0, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "ev-c2");
      te_add_event(e1);
      te_add_event(e2);
      te_free_event(&e1);
      te_free_event(&e2);
      CHECK(11, "delete key-1 from two-event list returns 1",
            te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, "ev-c1") == 1);
      CHECK(12, "delete key-2 returns 1 (remaining event gone)",
            te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, "ev-c2") == 1);
   }

   // --- delete all of type ---
   printf("\n--- delete all of type ---\n");
   {
      for (int i = 0; i < 3; i++) {
         te_event_t e = te_new_event(0, TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, 0, 0, nullptr);
         te_add_event(e);
         te_free_event(&e);
      }
      CHECK(13, "te_delete_all_one_time_events returns 3",
            te_delete_all_one_time_events(TYPE_SIGNAL_RESEND_EVENT) == 3);
      CHECK(14, "second call on now-empty type returns 0",
            te_delete_all_one_time_events(TYPE_SIGNAL_RESEND_EVENT) == 0);
   }

   // --- recurring events not deleted by one-time delete ---
   printf("\n--- recurring events not deleted by one-time delete ---\n");
   {
      // RECURRING_EVENT events are excluded from te_delete_*_one_time_event by design
      te_event_t ev = te_new_event(2 * 1000000, TYPE_JOB_RESEND_EVENT, RECURRING_EVENT, 0, 0, "rec-ev");
      te_add_event(ev);
      te_free_event(&ev);
      CHECK(15, "te_delete_one_time_event does not delete recurring event",
            te_delete_one_time_event(TYPE_JOB_RESEND_EVENT, 0, 0, "rec-ev") == 0);
      CHECK(16, "te_delete_all_one_time_events does not delete recurring event",
            te_delete_all_one_time_events(TYPE_JOB_RESEND_EVENT) == 0);
   }

   te_shutdown();
   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
