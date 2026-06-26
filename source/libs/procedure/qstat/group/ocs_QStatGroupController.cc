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

#include <algorithm>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_host.h"

#include "sched/load_correction.h"
#include "sched/sge_resource_utilization.h"
#include "sched/sge_select_queue.h"

#include "qstat/group/ocs_QStatGroupController.h"
#include "qstat/group/ocs_QStatGroupViewBase.h"

bool ocs::QStatGroupController::cqueue_calculate_summary(const lListElem *cqueue, const lList *exechost_list, const lList *centry_list,
                                                         double *load, bool *is_load_available, uint32_t *used, uint32_t *resv, uint32_t *total,
                                                         uint32_t *suspend_manual, uint32_t *suspend_threshold, uint32_t *suspend_on_subordinate,
                                                         uint32_t *suspend_calendar, uint32_t *unknown, uint32_t *load_alarm,
                                                         uint32_t *disabled_manual, uint32_t *disabled_calendar, uint32_t *ambiguous,
                                                         uint32_t *orphaned, uint32_t *error, uint32_t *available, uint32_t *temp_disabled,
                                                         uint32_t *manual_intervention) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (cqueue != nullptr) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      double host_load_avg = 0.0;
      uint32_t load_slots = 0;
      uint32_t used_available = 0;
      uint32_t used_slots = 0;
      uint32_t resv_slots = 0;

      *load = 0.0;
      *is_load_available = false;
      *used = *total = *resv = 0;
      *available = *temp_disabled = *manual_intervention = 0;
      *suspend_manual = *suspend_threshold = *suspend_on_subordinate = 0;
      *suspend_calendar = *unknown = *load_alarm = 0;
      *disabled_manual = *disabled_calendar = *ambiguous = 0;
      *orphaned = *error = 0;
      for_each_ep_lv(qinstance, qinstance_list) {
         uint32_t slots = lGetUlong(qinstance, QU_job_slots);
         bool has_value_from_object;

         used_slots = qinstance_slots_used(qinstance);
         resv_slots = qinstance_slots_reserved_now(qinstance);
         // saturating adds: these uint32 accumulators sum slot counts across all
         // queue instances and must not wrap (CS-2368, CWE-190).
         *used  = ProcedureView::add_saturating_u32(*used, used_slots);
         *resv  = ProcedureView::add_saturating_u32(*resv, resv_slots);
         *total = ProcedureView::add_saturating_u32(*total, slots);

         if (!sge_get_double_qattr(&host_load_avg, LOAD_ATTR_NP_LOAD_AVG, qinstance, exechost_list, centry_list,
                                   &has_value_from_object)) {
            if (has_value_from_object) {
               load_slots = ProcedureView::add_saturating_u32(load_slots, slots);
               *load += host_load_avg * slots;
            }
         }

         /*
          * manual_intervention: cdsuE
          * temp_disabled: aoACDS
          */
         if (qinstance_state_is_manual_suspended(qinstance) || qinstance_state_is_unknown(qinstance) ||
             qinstance_state_is_manual_disabled(qinstance) || qinstance_state_is_ambiguous(qinstance) ||
             qinstance_state_is_error(qinstance)) {
            *manual_intervention = ProcedureView::add_saturating_u32(*manual_intervention, slots);
         } else if (qinstance_state_is_alarm(qinstance) || qinstance_state_is_cal_disabled(qinstance) ||
                    qinstance_state_is_orphaned(qinstance) || qinstance_state_is_susp_on_sub(qinstance) ||
                    qinstance_state_is_cal_suspended(qinstance) || qinstance_state_is_suspend_alarm(qinstance)) {
            *temp_disabled = ProcedureView::add_saturating_u32(*temp_disabled, slots);
         } else {
            *available = ProcedureView::add_saturating_u32(*available, slots);
            used_available = ProcedureView::add_saturating_u32(used_available, used_slots);
         }
         if (qinstance_state_is_unknown(qinstance)) {
            *unknown = ProcedureView::add_saturating_u32(*unknown, slots);
         }
         if (qinstance_state_is_alarm(qinstance)) {
            *load_alarm = ProcedureView::add_saturating_u32(*load_alarm, slots);
         }
         if (qinstance_state_is_manual_disabled(qinstance)) {
            *disabled_manual = ProcedureView::add_saturating_u32(*disabled_manual, slots);
         }
         if (qinstance_state_is_cal_disabled(qinstance)) {
            *disabled_calendar = ProcedureView::add_saturating_u32(*disabled_calendar, slots);
         }
         if (qinstance_state_is_ambiguous(qinstance)) {
            *ambiguous = ProcedureView::add_saturating_u32(*ambiguous, slots);
         }
         if (qinstance_state_is_orphaned(qinstance)) {
            *orphaned = ProcedureView::add_saturating_u32(*orphaned, slots);
         }
         if (qinstance_state_is_manual_suspended(qinstance)) {
            *suspend_manual = ProcedureView::add_saturating_u32(*suspend_manual, slots);
         }
         if (qinstance_state_is_susp_on_sub(qinstance)) {
            *suspend_on_subordinate = ProcedureView::add_saturating_u32(*suspend_on_subordinate, slots);
         }
         if (qinstance_state_is_cal_suspended(qinstance)) {
            *suspend_calendar = ProcedureView::add_saturating_u32(*suspend_calendar, slots);
         }
         if (qinstance_state_is_suspend_alarm(qinstance)) {
            *suspend_threshold = ProcedureView::add_saturating_u32(*suspend_threshold, slots);
         }
         if (qinstance_state_is_error(qinstance)) {
            *error = ProcedureView::add_saturating_u32(*error, slots);
         }
      }
      if (load_slots > 0) {
         *is_load_available = true;
         *load /= load_slots;
      }
      *available -= std::min(used_available, *available);
   }
   DRETURN(ret);
}


void ocs::QStatGroupController::process_request(QStatParameter &parameter, QStatModelBase &model, QStatGroupViewBase &view) {
   DENTER(TOP_LAYER);

   model.calc_longest_queue_length(parameter);
   correct_capacities(model.get_exechost_list(), model.get_centry_list());

   view.report_started(out_, parameter);

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

         const char *cq_name = lGetString(cqueue, CQ_name);
         if (cq_name != nullptr) {
            view.report_cqueue(out_, cq_name, &summary, parameter);
         }
      }
   }

   view.report_finished(out_, parameter);

   DRETURN_VOID;
}
