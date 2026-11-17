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
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "ocs_QConfHGroupShowController.h"

/** @brief Render the one host group `-shgrp` asked about
 *
 * @param parameter the invocation; unused, the name travels in the model
 * @param model     the snapshot, holding every host group
 * @param view      the plain or JSON rendering
 * @return none
 */
void
ocs::QConfHGroupShowController::process_request([[maybe_unused]] QConfParameter &parameter, QConfModelBase &model,
                                               QConfViewBase &view) {
   DENTER(TOP_LAYER);

   const auto hgroup =
      ocs::tom::find_first<HGRP_name>(ocs::tom::ConstList<ocs::tom::HGRP>(model.get_object_list()),
                                      model.get_requested_name());
   if (hgroup != nullptr) {
      view.show_object(out_, hgroup, *model.get_desc());
   }

   DRETURN_VOID;
}
