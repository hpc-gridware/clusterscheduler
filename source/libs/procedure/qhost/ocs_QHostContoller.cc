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

/** @file
 * @brief Controller of `qhost`: runs the request and drives the view
 */

#include <sstream>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_host.h"

#include "qhost/ocs_QHostContoller.h"
#include "qhost/ocs_QHostModelBase.h"

/** @brief Run the request
 * @param parameter the parsed parameters
 * @param model the model, client or server side
 * @param view the view for the requested output format
 */
void
ocs::QHostController::process_request(QHostParameter &parameter, QHostModelBase &model, QHostViewBase &view) {
   DENTER(TOP_LAYER);

   std::ostringstream oss;

   // start report
   view.start(oss);
   for_each_rw_lv(ep, model.get_exec_host_list()) {

      // @todo when we have the code running as stored procedure we should find an early exit so that reader threads can shutdown fast
      // early termination if termination signal was received
      //if (shut_me_down) {
      //   DRETURN_VOID;
      //}

      // start host entry
      view.host_start(oss, lGetHost(ep, EH_name));

      // print host section
      view.show_host(oss, ep, parameter, model, view);

      // print resource section
      view.show_host_resources(oss, ep, parameter, model, view);

      // print queues and jobs of the host
      view.show_host_queues(oss, ep, parameter, model, view);

      // end host entry
      view.host_end(oss);
   }

   // end report
   view.end(oss);

   // show the full output
   view.show(out_, oss.str().c_str());
   DRETURN_VOID;
}
