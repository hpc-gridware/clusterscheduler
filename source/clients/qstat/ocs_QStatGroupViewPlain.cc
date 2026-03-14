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

#include <cstdio>
#include <cstdlib>
#include <format>
#include <iomanip>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_QStatGroupViewPlain.h"

ocs::QStatGroupViewPlain::QStatGroupViewPlain() {
}

void ocs::QStatGroupViewPlain::report_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   // Show header
   os << std::format(
         "{:<{}.{}s} {:>7} {:>6} {:>6} {:>6} {:>6} {:>6} {:>6} ",
         "CLUSTER QUEUE",
         parameter.longest_queue_length,
         parameter.longest_queue_length,
         "CQLOAD",
         "USED",
         "RES",
         "AVAIL",
         "TOTAL",
         "aoACDS",
         "cdsuE");
   const bool show_states = (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) ? true : false;
   if (show_states) {
      os << std::format(
              "{:>5} {:>5} {:>5} {:>5} {:>5} {:>5} {:>5} {:>5} {:>5} {:>5} {:>5}",
              "s", "A", "S", "C", "u", "a", "d", "D", "c", "o", "E");
   }
   os << '\n';

   // show dashes
   auto print_dashes = [&](int n) {
      os << std::setfill('-') << std::setw(n) << "";
   };
   print_dashes(parameter.longest_queue_length + 7 + 6 + 6 + 6 + 6 + 6 + 6 + 7*1);
   if (show_states) {
      print_dashes(11 * 6);
   }
   os << '\n';

   DRETURN_VOID;
}

void ocs::QStatGroupViewPlain::report_finished(std::ostream &os, QStatParameter &parameter) {
}

void ocs::QStatGroupViewPlain::report_cqueue(std::ostream &os, const char* cq_name, Summary *summary, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   const bool show_states = (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) ? true : false;

   os << std::format("{:<{}.{}s} ",
                   cq_name,
                   parameter.longest_queue_length,
                   parameter.longest_queue_length);

   if (summary->is_load_available) {
      os << std::format("{:>7.2f} ", summary->load);
   } else {
      os << std::format("{:>7} ", "-NA-");
   }

   os << std::format("{:>6} ", summary->used);
   os << std::format("{:>6} ", summary->resv);
   os << std::format("{:>6} ", summary->available);
   os << std::format("{:>6} ", summary->total);
   os << std::format("{:>6} ", summary->temp_disabled);
   os << std::format("{:>6} ", summary->manual_intervention);

   if (show_states) {
      os << std::format("{:>5} ", summary->suspend_manual);
      os << std::format("{:>5} ", summary->suspend_threshold);
      os << std::format("{:>5} ", summary->suspend_on_subordinate);
      os << std::format("{:>5} ", summary->suspend_calendar);
      os << std::format("{:>5} ", summary->unknown);
      os << std::format("{:>5} ", summary->load_alarm);
      os << std::format("{:>5} ", summary->disabled_manual);
      os << std::format("{:>5} ", summary->disabled_calendar);
      os << std::format("{:>5} ", summary->ambiguous);
      os << std::format("{:>5} ", summary->orphaned);
      os << std::format("{:>5} ", summary->error);
   }

   os << '\n';

   DRETURN_VOID;
}
