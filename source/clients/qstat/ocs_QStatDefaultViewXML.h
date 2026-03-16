#pragma once
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

#include "ocs_QStatDefaultViewBase.h"

namespace ocs {
   class QStatDefaultViewXML : public QStatDefaultViewBase {
      lListElem *queue_list_elem = nullptr;
      lListElem *queue_elem = nullptr;

      lListElem *job_list_elem = nullptr;
      lList     *job_list = nullptr;
      lListElem *job_elem = nullptr;


      int qstat_xml_create_job_list(lList **alpp);
      int qstat_xml_finish_job_list(const char* state, lList* target_list, lList **alpp);
   public:
      QStatDefaultViewXML();
      ~QStatDefaultViewXML() override = default;

      void report_started(lList** alpp) override;
      void report_finished(lList** alpp) override;
      void report_queue_summary(const char* qname,  queue_summary_t *summary, lList **alpp, QStatParameter &parameter) override;
      void report_queue_started(const char* qname, lList **alpp, QStatParameter &parameter) override;
      void report_queue_load_alarm(const char* qname, const char* reason, lList **alpp) override;
      void report_queue_suspend_alarm(const char* qname, const char* reason, lList **alpp) override;
      void report_queue_message(const char* qname, const char *message, lList **alpp) override;
      void report_queue_resource(const char* dom, const char* name, const char* value, const char *details, lList **alpp) override;
      void report_queue_finished(const char* qname, lList **alpp, ocs::QStatParameter &parameter) override;
      void report_queue_jobs_started(const char* qname, lList **alpp) override;
      void report_queue_jobs_finished(const char* qname, lList **alpp, QStatParameter &parameter) override;
      void report_pending_jobs_started(lList **alpp, QStatParameter &parameter) override;
      void report_pending_jobs_finished(lList **alpp) override;
      void report_finished_jobs_started(lList **alpp, QStatParameter &parameter) override;
      void report_finished_jobs_finished(lList **alpp) override;
      void report_error_jobs_started(lList **alpp, QStatParameter &parameter) override;
      void report_error_jobs_finished(lList **alpp) override;
      void report_zombie_jobs_started(lList **alpp) override;
      void report_zombie_jobs_finished(lList **alpp) override;

      // region Job handling
      void report_job(u_long32 jid, job_summary_t *summary, lList **alpp, QStatParameter &parameter, QStatModel &model) override;
      void report_sub_tasks_started(lList **alpp) override;
      void report_sub_task(task_summary_t *summary, lList **alpp) override;
      void report_sub_tasks_finished(lList **alpp) override;
      void report_additional_info(job_additional_info_t name, const char* value, lList **alpp) override;
      void report_requested_pe(const char* pe_name, const char* pe_range, lList **alpp) override;
      void report_granted_pe(const char* pe_name, int pe_slots, lList **alpp) override;
      void report_request(const char* name, const char* value, lList **alpp) override;
      void report_hard_resources_started(int scope, lList **alpp) override;
      void report_hard_resource(int scope, const char* name, const char* value, double uc, lList **alpp) override;
      void report_hard_resources_finished(lList **alpp) override;
      void report_soft_resources_started(int scope, lList **alpp) override;
      void report_soft_resource(int scope, const char* name, const char* value, double uc, lList **alpp) override;
      void report_soft_resources_finished(lList **alpp) override;
      void report_hard_requested_queues_started(int scope, lList **alpp) override;
      void report_hard_requested_queue(int scope, const char* name, lList **alpp) override;
      void report_hard_requested_queues_finished(lList **alpp) override;
      void report_soft_requested_queues_started(int scope, lList **alpp) override;
      void report_soft_requested_queue(int scope, const char* name, lList **alpp) override;
      void report_soft_requested_queues_finished(lList **alpp) override;
      void report_predecessors_requested_started(lList **alpp) override;
      void report_predecessor_requested(const char* name, lList **alpp) override;
      void report_predecessors_requested_finished(lList **alpp) override;
      void report_predecessors_started(lList **alpp) override;
      void report_predecessor(u_long32 jid, lList **alpp) override;
      void report_predecessors_finished(lList **alpp) override;
      void report_ad_predecessors_requested_started(lList **alpp) override;
      void report_ad_predecessor_requested(const char* name, lList **alpp) override;
      void report_ad_predecessors_requested_finished(lList **alpp) override;
      void report_ad_predecessors_started(lList **alpp) override;
      void report_ad_predecessor(u_long32 jid, lList **alpp) override;
      void report_ad_predecessors_finished(lList **alpp) override;
      void report_binding_started(lList **alpp) override;
      void report_binding(const char *binding, lList **alpp) override;
      void report_binding_finished(lList **alpp) override;
      void report_job_finished(u_long32 jid, lList **alpp) override;
      // endregion
   };
}