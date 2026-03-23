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

      void show_header_with_title(std::ostream &os, const QStatParameter &parameter, const char *title);
      void show_header_with_subtitle(std::ostream &os, job_additional_info_t subtitle, const char *name, const char *value);
      void show_queues_or_resource_started(std::ostream &os, int scope, bool queue, bool hard);
   public:
      QStatDefaultViewPlain() = default;
      ~QStatDefaultViewPlain() override = default;

      // region General report handling
      void report_started(std::ostream &os) override;
      void report_finished(std::ostream &os) override;
      void report_queue_summary(std::ostream &os, const char* qname,  queue_summary_t *summary, QStatParameter &parameter) override;
      void report_queue_started(std::ostream &os, const char* qname, QStatParameter &parameter) override;
      void report_queue_load_alarm(std::ostream &os, const char* qname, const char* reason) override;
      void report_queue_suspend_alarm(std::ostream &os, const char* qname, const char* reason) override;
      void report_queue_message(std::ostream &os, const char* qname, const char *message) override;
      void report_queue_resource(std::ostream &os, const char* dom, const char* name, const char* value, const char *details) override;
      void report_queue_finished(std::ostream &os, const char* qname, QStatParameter &parameter) override;
      void report_queue_jobs_started(std::ostream &os, const char* qname) override;
      void report_queue_jobs_finished(std::ostream &os, const char* qname, QStatParameter &parameter) override;
      void report_pending_jobs_started(std::ostream &os, QStatParameter &parameter) override;
      void report_pending_jobs_finished(std::ostream &os) override;
      void report_finished_jobs_started(std::ostream &os, QStatParameter &parameter) override;
      void report_finished_jobs_finished(std::ostream &os) override;
      void report_error_jobs_started(std::ostream &os, QStatParameter &parameter) override;
      void report_error_jobs_finished(std::ostream &os) override;
      // endregion

      // region Job handling
      void report_job(std::ostream &os, u_long32 jid, job_summary_t *summary, QStatParameter &parameter, QStatGenericModel &model) override;
      void report_sub_tasks_started(std::ostream &os) override;
      void report_sub_task(std::ostream &os, task_summary_t *summary) override;
      void report_sub_tasks_finished(std::ostream &os) override;
      void report_additional_info(std::ostream &os, job_additional_info_t name, const char* value) override;
      void report_requested_pe(std::ostream &os, const char* pe_name, const char* pe_range) override;
      void report_granted_pe(std::ostream &os, const char* pe_name, int pe_slots) override;
      void report_request(std::ostream &os, const char* name, const char* value) override;
      void report_hard_resources_started(std::ostream &os, int scope) override;
      void report_hard_resource(std::ostream &os, int scope, const char* name, const char* value, double uc) override;
      void report_hard_resources_finished(std::ostream &os) override;
      void report_soft_resources_started(std::ostream &os, int scope) override;
      void report_soft_resource(std::ostream &os, int scope, const char* name, const char* value, double uc) override;
      void report_soft_resources_finished(std::ostream &os) override;
      void report_hard_requested_queues_started(std::ostream &os, int scope) override;
      void report_hard_requested_queue(std::ostream &os, int scope, const char* name) override;
      void report_hard_requested_queues_finished(std::ostream &os) override;
      void report_soft_requested_queues_started(std::ostream &os, int scope) override;
      void report_soft_requested_queue(std::ostream &os, int scope, const char* name) override;
      void report_soft_requested_queues_finished(std::ostream &os) override;
      void report_predecessors_requested_started(std::ostream &os) override;
      void report_predecessor_requested(std::ostream &os, const char* name) override;
      void report_predecessors_requested_finished(std::ostream &os) override;
      void report_predecessors_started(std::ostream &os) override;
      void report_predecessor(std::ostream &os, u_long32 jid) override;
      void report_predecessors_finished(std::ostream &os) override;
      void report_ad_predecessors_requested_started(std::ostream &os) override;
      void report_ad_predecessor_requested(std::ostream &os, const char* name) override;
      void report_ad_predecessors_requested_finished(std::ostream &os) override;
      void report_ad_predecessors_started(std::ostream &os) override;
      void report_ad_predecessor(std::ostream &os, u_long32 jid) override;
      void report_ad_predecessors_finished(std::ostream &os) override;
      void report_binding_started(std::ostream &os) override;
      void report_binding(std::ostream &os, const char *binding) override;
      void report_binding_finished(std::ostream &os) override;
      void report_job_finished(std::ostream &os, u_long32 jid) override;
      // endregion

   };
}