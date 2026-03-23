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

#include "basis_types.h"

#include "ocs_QStatParameter.h"
#include "ocs_QStatGenericModel.h"

namespace ocs {
   class QStatDefaultViewBase {
   public:
      enum job_additional_info_t {
         JOB_ADDITIONAL_INFO_ERROR = 0,
         CHECKPOINT_ENV  = 1,
         MASTER_QUEUE = 2,
         FULL_JOB_NAME = 3,
         REQUESTED_PE = 4,
         GRANTED_PE = 5
      };

      struct queue_summary_t {
         const char* queue_type;

         u_long32    used_slots;
         u_long32    resv_slots;
         u_long32    total_slots;

         const char* arch;
         const char* state;

         const char* load_avg_str;
         bool has_load_value;
         bool has_load_value_from_object;
         double load_avg;

      };

      struct job_summary_t {
         bool print_jobid;
         int priority;
         double nurg;
         double urg;
         double nppri;
         double nprior;
         double ntckts;
         double rrcontr;
         double wtcontr;
         double dlcontr;
         const char* name;
         const char* user;
         const char* project;
         const char* department;
         char state[8];
         u_long64 submit_time;
         u_long64 start_time;
         u_long64 deadline;

         bool   has_cpu_usage;
         u_long32 cpu_usage;
         bool   has_mem_usage;
         double mem_usage;
         bool   has_io_usage;
         double io_usage;

         u_long override_tickets;
         bool   is_queue_assigned;
         u_long tickets;
         u_long otickets;
         u_long ftickets;
         u_long stickets;

         double share;
         const char* queue;
         const char* master;
         u_long32 slots;
         bool is_array;
         bool is_running;
         const char* task_id;

      };

      struct task_summary_t {
         const char* task_id;
         const char* state;
         bool has_cpu_usage;
         double cpu_usage;
         bool has_mem_usage;
         double mem_usage;
         bool has_io_usage;
         double io_usage;
         bool is_running;
         bool has_exit_status;
         u_long32 exit_status;
      };

      QStatDefaultViewBase() = default;
      virtual ~QStatDefaultViewBase() = default;

      // region General report handling
      virtual void report_queue_summary(std::ostream &os, const char* qname,  queue_summary_t *summary, QStatParameter &parameter) = 0;
      virtual void report_started(std::ostream &os) = 0;
      virtual void report_finished(std::ostream &os) = 0;
      virtual void report_queue_started(std::ostream &os, const char* qname, QStatParameter &parameter) = 0;
      virtual void report_queue_load_alarm(std::ostream &os, const char* qname, const char* reason) = 0;
      virtual void report_queue_suspend_alarm(std::ostream &os, const char* qname, const char* reason) = 0;
      virtual void report_queue_message(std::ostream &os, const char* qname, const char *message) = 0;
      virtual void report_queue_resource(std::ostream &os, const char* dom, const char* name, const char* value, const char *details) = 0;
      virtual void report_queue_finished(std::ostream &os, const char* qname, QStatParameter &parameter) = 0;
      virtual void report_queue_jobs_started(std::ostream &os, const char* qname) = 0;
      virtual void report_queue_jobs_finished(std::ostream &os, const char* qname, QStatParameter &parameter) = 0;
      virtual void report_pending_jobs_started(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_pending_jobs_finished(std::ostream &os) = 0;
      virtual void report_finished_jobs_started(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_finished_jobs_finished(std::ostream &os) = 0;
      virtual void report_error_jobs_started(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_error_jobs_finished(std::ostream &os) = 0;
      // endregion

      // region Job handling
      virtual void report_job(std::ostream &os, u_long32 jid, job_summary_t *summary, QStatParameter &parameter, QStatGenericModel &model) = 0;
      virtual void report_sub_tasks_started(std::ostream &os) = 0;
      virtual void report_sub_task(std::ostream &os, task_summary_t *summary) = 0;
      virtual void report_sub_tasks_finished(std::ostream &os) = 0;
      virtual void report_additional_info(std::ostream &os, job_additional_info_t name, const char* value) = 0;
      virtual void report_requested_pe(std::ostream &os, const char* pe_name, const char* pe_range) = 0;
      virtual void report_granted_pe(std::ostream &os, const char* pe_name, int pe_slots) = 0;
      virtual void report_request(std::ostream &os, const char* name, const char* value) = 0;
      virtual void report_hard_resources_started(std::ostream &os, int scope) = 0;
      virtual void report_hard_resource(std::ostream &os, int scope, const char* name, const char* value, double uc) = 0;
      virtual void report_hard_resources_finished(std::ostream &os) = 0;
      virtual void report_soft_resources_started(std::ostream &os, int scope) = 0;
      virtual void report_soft_resource(std::ostream &os, int scope, const char* name, const char* value, double uc) = 0;
      virtual void report_soft_resources_finished(std::ostream &os) = 0;
      virtual void report_hard_requested_queues_started(std::ostream &os, int scope) = 0;
      virtual void report_hard_requested_queue(std::ostream &os, int scope, const char* name) = 0;
      virtual void report_hard_requested_queues_finished(std::ostream &os) = 0;
      virtual void report_soft_requested_queues_started(std::ostream &os, int scope) = 0;
      virtual void report_soft_requested_queue(std::ostream &os, int scope, const char* name) = 0;
      virtual void report_soft_requested_queues_finished(std::ostream &os) = 0;
      virtual void report_predecessors_requested_started(std::ostream &os) = 0;
      virtual void report_predecessor_requested(std::ostream &os, const char* name) = 0;
      virtual void report_predecessors_requested_finished(std::ostream &os) = 0;
      virtual void report_predecessors_started(std::ostream &os) = 0;
      virtual void report_predecessor(std::ostream &os, u_long32 jid) = 0;
      virtual void report_predecessors_finished(std::ostream &os) = 0;
      virtual void report_ad_predecessors_requested_started(std::ostream &os) = 0;
      virtual void report_ad_predecessor_requested(std::ostream &os, const char* name) = 0;
      virtual void report_ad_predecessors_requested_finished(std::ostream &os) = 0;
      virtual void report_ad_predecessors_started(std::ostream &os) = 0;
      virtual void report_ad_predecessor(std::ostream &os, u_long32 jid) = 0;
      virtual void report_ad_predecessors_finished(std::ostream &os) = 0;
      virtual void report_binding_started(std::ostream &os) = 0;
      virtual void report_binding(std::ostream &os, const char *binding) = 0;
      virtual void report_binding_finished(std::ostream &os) = 0;
      virtual void report_job_finished(std::ostream &os, u_long32 jid) = 0;
      // endregion

   };
}