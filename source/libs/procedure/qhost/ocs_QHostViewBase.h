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

#include "ocs_QHostParameter.h"
#include "ocs_QHostModel.h"

namespace ocs {
   class QHostViewBase {
      u_long32 full_listing_;
   protected:
      size_t indent_ = 0;
   public:
      static void reformat_double_string(char *new_string, size_t result_size, const char *format, const char *old_string);

      virtual void show_host(std::ostream &os, const lListElem *hep, const QHostParameter &parameter, const QHostModel &model, QHostViewBase &report_handler);
      virtual void show_host_resources(std::ostream &os, lListElem *host, const QHostParameter &parameter, const QHostModel &model, QHostViewBase &report_handler);
      virtual void show_job(std::ostream &os, lListElem *job, lListElem *jatep, lListElem *qep, int print_jobid, const char *master,
                                 dstring *dyn_task_str, u_long32 full_listing, int slots, int slot, const char *indent, u_long32 group_opt, int slots_per_line,
                                 int queue_name_length, QHostParameter &parameter, QHostModel &model, QHostViewBase &report_handler);
      virtual void show_jobs_per_queue(std::ostream &os, lListElem *qep, int print_jobs_of_queue, u_long32 full_listing, const char *indent,
                                       u_long32 group_opt, int queue_name_length, QHostParameter &parameter, QHostModel &model, QHostViewBase &report_handler);
      virtual void show_host_queues(std::ostream &os, lListElem *host, QHostParameter &parameter, QHostModel &model, QHostViewBase &report_handler);

   public:
      explicit QHostViewBase(const QHostParameter &parameter);
      virtual ~QHostViewBase() = default;

      virtual void start(std::ostream &os) = 0;
      virtual void end(std::ostream &os) = 0;

      virtual void host_start(std::ostream &os, const char *host_name) = 0;
      virtual void host_end(std::ostream &os) = 0;
      virtual void host_value(std::ostream &os, const char *format_str, const char *name, const char *value) = 0;
      virtual void host_value(std::ostream &os, const char *format_str, const char* name, u_long32 value) = 0;

      virtual void queue_start(std::ostream &os, const char *format_str, const char* qname) = 0;
      virtual void queue_end(std::ostream &os) = 0;
      virtual void queue_value(std::ostream &os, const char *qname, const char *format_str, const char* name, const char *value) = 0;
      virtual void queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, u_long32 value) = 0;

      virtual void job_start(std::ostream &os, const char *format_str, u_long32 jid) = 0;
      virtual void job_end(std::ostream &os) = 0;
      virtual void job_value(std::ostream &os, u_long32 jid, const char *format_str, const char* name, const char *value) = 0;
      virtual void job_value(std::ostream &os, u_long32 jid, const char *format_str, const char* name, u_long64 value) = 0;
      virtual void job_value(std::ostream &os, u_long32 jid, const char *format_str, const char* name, double value) = 0;

      virtual void resource_value(std::ostream &os, const char* dominance, const char* name, const char* value, const char *details) = 0;
   };
}
