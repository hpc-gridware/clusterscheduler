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

#include <sstream>
#include <iostream>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_host.h"

#include "ocs_QHostContoller.h"
#include "sig_handlers.h"

void
ocs::QHostController::process_request(QHostParameter &parameter, QHostModel &model, QHostViewBase &report_handler) {
   DENTER(TOP_LAYER);

   std::ostringstream oss;

   // start report
   report_handler.start(oss);
   lListElem *ep;
   for_each_rw (ep, model.get_exechost_list()) {

      // @todo when we have the code running as stored prcedure we should find an early exit so that reader threads can shutdown fast
      // early termination if termination signal was received
      //if (shut_me_down) {
      //   DRETURN_VOID;
      //}

      // start host entry
      report_handler.host_start(oss, lGetHost(ep, EH_name));

      // print host section
      report_handler.sge_print_host(oss, ep, parameter, model, report_handler);

      // print resource section
      report_handler.sge_print_resources(oss, ep, parameter, model, report_handler);

      // print queues and jobs of the host
      report_handler.sge_print_queues(oss, ep, nullptr, parameter, model, report_handler);

      // end host entry
      report_handler.host_end(oss);
   }

   // end report
   report_handler.end(oss);

   std::cout << oss.str();
   DRETURN_VOID;
}
