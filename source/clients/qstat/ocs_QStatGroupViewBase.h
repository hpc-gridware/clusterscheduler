#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
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
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <ostream>

#include "ocs_QStatParameter.h"

namespace ocs {
   class QStatGroupViewBase {
   public:
      struct Summary {
         double load;
         bool   is_load_available;
         u_long32 used;
         u_long32 resv;
         u_long32 total;
         u_long32 temp_disabled;
         u_long32 available;
         u_long32 manual_intervention;
         u_long32 suspend_manual;
         u_long32 suspend_threshold;
         u_long32 suspend_on_subordinate;
         u_long32 suspend_calendar;
         u_long32 unknown, load_alarm;
         u_long32 disabled_manual;
         u_long32 disabled_calendar;
         u_long32 ambiguous;
         u_long32 orphaned, error;
      };

      QStatGroupViewBase() = default;
      virtual ~QStatGroupViewBase() = default;

      virtual void report_started(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_finished(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_cqueue(std::ostream &os, const char* qname, Summary *cqueue_summary, QStatParameter &parameter) = 0;
   };
}
