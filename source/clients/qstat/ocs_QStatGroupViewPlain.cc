/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_QStatGroupViewPlain.h"

ocs::QStatGroupViewPlain::QStatGroupViewPlain() {
}

void ocs::QStatGroupViewPlain::report_started(std::ostream &os, QStatParameter &parameter) {
   int i;
   bool show_states = (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) ? true : false;

   char queue_def[50];
   char fields[] = "%7s %6s %6s %6s %6s %6s %6s ";

   DENTER(TOP_LAYER);

   snprintf(queue_def, sizeof(queue_def), "%%-%d.%ds %s ", parameter.longest_queue_length, parameter.longest_queue_length, fields);
   printf(queue_def, "CLUSTER QUEUE", "CQLOAD", "USED", "RES", "AVAIL", "TOTAL", "aoACDS", "cdsuE");
   if (show_states) {
      printf("%5s %5s %5s %5s %5s %5s %5s %5s %5s %5s %5s", "s", "A", "S", "C", "u", "a", "d", "D", "c", "o", "E");
   }
   printf("\n");

   printf("--------------------");
   printf("--------------------");
   printf("--------------------");
   printf("--------------------");
   if (show_states) {
      printf("--------------------");
      printf("--------------------");
      printf("--------------------");
      printf("------");
   }
   for(i=0; i< parameter.longest_queue_length - 36; i++) {
      printf("-");
   }
   printf("\n");

   DRETURN_VOID;
}

void ocs::QStatGroupViewPlain::report_finished(std::ostream &os, QStatParameter &parameter) {
}

void ocs::QStatGroupViewPlain::report_cqueue(std::ostream &os, const char* cq_name, cqueue_summary_t *summary, QStatParameter &parameter) {
   bool show_states = (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) ? true : false;
   char queue_def[50];

   DENTER(TOP_LAYER);

   snprintf(queue_def, sizeof(queue_def), "%%-%d.%ds ", parameter.longest_queue_length, parameter.longest_queue_length);

   printf(queue_def, cq_name);

   if (summary->is_load_available) {
      printf("%7.2f ", summary->load);
   } else {
      printf("%7s ", "-NA-");
   }

   printf("%6d ", (int)summary->used);
   printf("%6d ", (int)summary->resv);
   printf("%6d ", (int)summary->available);
   printf("%6d ", (int)summary->total);
   printf("%6d ", (int)summary->temp_disabled);
   printf("%6d ", (int)summary->manual_intervention);
   if (show_states) {
      printf("%5d ", (int)summary->suspend_manual);
      printf("%5d ", (int)summary->suspend_threshold);
      printf("%5d ", (int)summary->suspend_on_subordinate);
      printf("%5d ", (int)summary->suspend_calendar);
      printf("%5d ", (int)summary->unknown);
      printf("%5d ", (int)summary->load_alarm);
      printf("%5d ", (int)summary->disabled_manual);
      printf("%5d ", (int)summary->disabled_calendar);
      printf("%5d ", (int)summary->ambiguous);
      printf("%5d ", (int)summary->orphaned);
      printf("%5d ", (int)summary->error);
   }
   printf("\n");

   DRETURN_VOID;
}
