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
 * @brief See the header (CS-2816)
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_hgroup.h"

#include "ocs_QConfHGroupResolvedController.h"

/** @brief Render the resolved membership of the host group
 *
 * @param parameter the invocation; unused, the name travels in the model
 * @param model     the snapshot, holding every host group
 * @param view      the resolved rendering
 */
void
ocs::QConfHGroupResolvedController::process_request([[maybe_unused]] QConfParameter &parameter, QConfModelBase &model,
                                                   QConfHGroupStructureView &view) {
   DENTER(TOP_LAYER);

   const lListElem *hgroup =
      lGetElemHost(model.get_object_list(), model.get_key_nm(), model.get_requested_name());
   if (hgroup != nullptr) {
      view.show_resolved(out_, hgroup, model.get_object_list());
   }

   DRETURN_VOID;
}
