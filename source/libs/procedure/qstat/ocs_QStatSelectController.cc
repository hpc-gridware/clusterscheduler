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

#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_str.h"

#include "ocs_QStatSelectController.h"

void ocs::QStatSelectController::process_request(QStatParameter &parameter, QStatGenericModel &model, QStatSelectViewBase &view) {
   DENTER(TOP_LAYER);

   std::ostringstream oss;

   view.report_started(oss);

   const lListElem *cqueue = nullptr;
   for_each_ep(cqueue, model.queue_list) {

      const lListElem *qep = nullptr;
      for_each_ep(qep, lGetList(cqueue, CQ_qinstances)) {
         if ((lGetUlong(qep, QU_tag) & TAG_SHOW_IT) == TAG_SHOW_IT) {
            view.report_queue(oss, lGetString(qep, QU_full_name));
         }
      }
   }

   view.report_finished(oss);

   std::cout << oss.str();

   DRETURN_VOID;
}
