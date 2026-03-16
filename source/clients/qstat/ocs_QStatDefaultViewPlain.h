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
   class QStatDefaultViewPlain : public QStatDefaultViewBase {
      bool  header_printed = false;
      bool  job_header_printed = false;

      /* id of the last reported job */
      u_long32 last_job_id = 0;
      dstring  last_queue_name = DSTRING_INIT;

      int  sub_task_count = 0;
      int  hard_resource_count = 0;
      int  soft_resource_count = 0;
      int  hard_requested_queue_count = 0;
      int  soft_requested_queue_count = 0;
      int  predecessor_requested_count = 0;
      int  predecessor_count = 0;
      int  ad_predecessor_requested_count = 0;
      int  ad_predecessor_count = 0;
   public:
      QStatDefaultViewPlain();
      ~QStatDefaultViewPlain() override = default;

      void report_started() override;
      void report_finished() override;
      void report_queue_summary(const char* qname,  queue_summary_t *summary, QStatParameter &parameter) override;
      void report_queue_started(const char* qname, QStatParameter &parameter) override;
      void report_queue_load_alarm(const char* qname, const char* reason) override;
      void report_queue_suspend_alarm(const char* qname, const char* reason) override;
      void report_queue_message(const char* qname, const char *message) override;
      void report_queue_resource(const char* dom, const char* name, const char* value, const char *details) override;
      void report_queue_finished(const char* qname, QStatParameter &parameter) override;
      void report_queue_jobs_started(const char* qname) override;
      void report_queue_jobs_finished(const char* qname, QStatParameter &parameter) override;
      void report_pending_jobs_started(QStatParameter &parameter) override;
      void report_pending_jobs_finished() override;
      void report_finished_jobs_started(QStatParameter &parameter) override;
      void report_finished_jobs_finished() override;
      void report_error_jobs_started(QStatParameter &parameter) override;
      void report_error_jobs_finished() override;
      void report_zombie_jobs_started() override;
      void report_zombie_jobs_finished() override;

      // region Job handling
      void report_job(u_long32 jid, job_summary_t *summary, QStatParameter &parameter, QStatGenericModel &model) override;
      void report_sub_tasks_started() override;
      void report_sub_task(task_summary_t *summary) override;
      void report_sub_tasks_finished() override;
      void report_additional_info(job_additional_info_t name, const char* value) override;
      void report_requested_pe(const char* pe_name, const char* pe_range) override;
      void report_granted_pe(const char* pe_name, int pe_slots) override;
      void report_request(const char* name, const char* value) override;
      void report_hard_resources_started(int scope) override;
      void report_hard_resource(int scope, const char* name, const char* value, double uc) override;
      void report_hard_resources_finished() override;
      void report_soft_resources_started(int scope) override;
      void report_soft_resource(int scope, const char* name, const char* value, double uc) override;
      void report_soft_resources_finished() override;
      void report_hard_requested_queues_started(int scope) override;
      void report_hard_requested_queue(int scope, const char* name) override;
      void report_hard_requested_queues_finished() override;
      void report_soft_requested_queues_started(int scope) override;
      void report_soft_requested_queue(int scope, const char* name) override;
      void report_soft_requested_queues_finished() override;
      void report_predecessors_requested_started() override;
      void report_predecessor_requested(const char* name) override;
      void report_predecessors_requested_finished() override;
      void report_predecessors_started() override;
      void report_predecessor(u_long32 jid) override;
      void report_predecessors_finished() override;
      void report_ad_predecessors_requested_started() override;
      void report_ad_predecessor_requested(const char* name) override;
      void report_ad_predecessors_requested_finished() override;
      void report_ad_predecessors_started() override;
      void report_ad_predecessor(u_long32 jid) override;
      void report_ad_predecessors_finished() override;
      void report_binding_started() override;
      void report_binding(const char *binding) override;
      void report_binding_finished() override;
      void report_job_finished(u_long32 jid) override;
      // endregion

   };
}