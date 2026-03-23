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
#include "ocs_QStatGenericModel.h"
#include "ocs_QStatDefaultViewBase.h"

namespace ocs {
   class QStatDefaultController {
      void remove_tagged_jobs(lList *job_list);

      void process_queues_with_its_jobs(std::ostream &os, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view);
      void process_queue(std::ostream &os, lListElem *queue, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view);
      void process_jobs_in_queue(std::ostream &os, lListElem *queue, bool print_jobs_of_queue, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view);

      void process_jobs_pending_state(std::ostream &os, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view);
      void process_jobs_finished_state(std::ostream &os, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view);
      void process_jobs_error_state(std::ostream &os, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view);
      void process_jobs_not_enrolled(std::ostream &os, lListElem *job, bool print_jobid, char *master, int slots, int slot, int *count,
                                     QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view);

      void process_job(std::ostream &os, lListElem *job, lListElem *jatep, lListElem *qep, lListElem *gdil_ep, bool print_jobid,
                       const char *master, dstring *dyn_task_str, int slots, int slot, int slots_per_line,
                       QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view);
      void process_subtask(std::ostream &os, lListElem *job, lListElem *ja_task, lListElem *pe_task, QStatDefaultViewBase &view);
      void process_resources(std::ostream &os, const lList* cel, lList* centry_list, int slots, int scope, bool is_hard_resource, QStatDefaultViewBase &view);


   public:
      QStatDefaultController() = default;
      ~QStatDefaultController() = default;

      void process_request(QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view);
   };
}
