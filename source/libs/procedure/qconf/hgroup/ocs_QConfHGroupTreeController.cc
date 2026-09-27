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

#include "ocs_QConfHGroupTreeController.h"

/** @brief Render the host group tree
 *
 * @param parameter the invocation; unused, the name travels in the model
 * @param model     the snapshot, holding every host group
 * @param view      the tree rendering
 */
void
ocs::QConfHGroupTreeController::process_request([[maybe_unused]] QConfParameter &parameter, QConfModelBase &model,
                                               QConfHGroupStructureView &view) {
   DENTER(TOP_LAYER);

   view.show_tree(out_, model.get_object_list(), model.get_requested_name());

   DRETURN_VOID;
}
