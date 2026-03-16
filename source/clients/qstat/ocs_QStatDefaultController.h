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

#include "ocs_QStatParameter.h"
#include "ocs_QStatModel.h"
#include "ocs_QStatDefaultViewBase.h"

namespace ocs {
   class QStatDefaultController {
      void remove_tagged_jobs(lList *job_list);
      void qstat_handle_running_jobs(QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
      void handle_jobs_queue(lListElem *qep, int print_jobs_of_queue, QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
      void sge_handle_job(lListElem *job, lListElem *jatep, lListElem *qep, lListElem *gdil_ep, bool print_jobid,
                          const char *master, dstring *dyn_task_str, int slots, int slot, int slots_per_line,
                          QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
      void job_handle_subtask(lListElem *job, lListElem *ja_task, lListElem *pe_task, QStatDefaultViewBase &view);
      void job_handle_resources(const lList* cel, lList* centry_list, int slots, int scope, bool is_hard_resource, QStatDefaultViewBase &view);
      void handle_pending_jobs(QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
      void handle_finished_jobs(QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
      void handle_error_jobs(QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
      void handle_zombie_jobs(QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
      void handle_jobs_not_enrolled(lListElem *job, bool print_jobid, char *master,
                                          int slots, int slot, int *count,
                                          QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
      void handle_queue(lListElem *q, QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
   public:
      QStatDefaultController() = default;
      ~QStatDefaultController() = default;

      void process_request(QStatParameter &parameter, QStatModel &model, QStatDefaultViewBase &view);
   };
}
