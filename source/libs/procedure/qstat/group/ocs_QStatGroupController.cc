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

#include <sstream>
#include <iostream>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_str.h"

#include "sched/load_correction.h"

#include "qstat/group/ocs_QStatGroupController.h"
#include "qstat/group/ocs_QStatGroupViewBase.h"
#include "ocs_client_cqueue.h"


void ocs::QStatGroupController::process_request(QStatParameter &parameter, QStatModelClient &model, QStatGroupViewBase &view) {
   DENTER(TOP_LAYER);

   std::ostringstream oss;

   model.calc_longest_queue_length(parameter);
   correct_capacities(model.get_exechost_list(), model.get_centry_list());

   view.report_started(oss, parameter);

   for_each_ep_lv(cqueue, model.get_queue_list()) {
      if (lGetUlong(cqueue, CQ_tag) != TAG_DEFAULT) {
         QStatGroupViewBase::Summary summary{};

         cqueue_calculate_summary(cqueue,
                                  model.get_exechost_list(),
                                  model.get_centry_list(),
                                  &(summary.load),
                                  &(summary.is_load_available),
                                  &(summary.used),
                                  &(summary.resv),
                                  &(summary.total),
                                  &(summary.suspend_manual),
                                  &(summary.suspend_threshold),
                                  &(summary.suspend_on_subordinate),
                                  &(summary.suspend_calendar),
                                  &(summary.unknown),
                                  &(summary.load_alarm),
                                  &(summary.disabled_manual),
                                  &(summary.disabled_calendar),
                                  &(summary.ambiguous),
                                  &(summary.orphaned),
                                  &(summary.error),
                                  &(summary.available),
                                  &(summary.temp_disabled),
                                  &(summary.manual_intervention));

         view.report_cqueue(oss, lGetString(cqueue, CQ_name), &summary, parameter);
      }
   }

   view.report_finished(oss, parameter);

   std::cout << oss.str();

   DRETURN_VOID;
}
