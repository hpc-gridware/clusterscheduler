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

#include <sstream>
#include <iostream>

#include "uti/sge_rmon_macros.h"

#include "qstat/job/ocs_QStatJobController.h"
#include "qstat/job/ocs_QStatJobViewBase.h"

void ocs::QStatJobController::process_request(QStatParameter &parameter, QStatModelBase &model, QStatJobViewBase &view) {
   DENTER(TOP_LAYER);

   view.report_started(out_, parameter);

   if (parameter.get_jid_list() != nullptr) {
      view.show_jobs_and_reasons(out_, parameter, model);
   } else {
      view.show_reasons(out_, parameter, model);
   }

   view.report_finished(out_, parameter);

   DRETURN_VOID;
}
