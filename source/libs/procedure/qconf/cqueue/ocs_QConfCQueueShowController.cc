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
 * @brief See the header (CS-2818)
 */

#include "uti/sge_rmon_macros.h"

#include "ocs_QConfCQueueShowController.h"

/** @brief Write the queues, instances or template the model resolved
 *
 * The answer list the view fills is a local one and is dropped: a write error
 * has already been reported into it, and process_request() has no answer list of
 * its own to pass it on through - which is why every error a caller has to see
 * is raised in make_snapshot() instead.
 *
 * @param parameter the invocation; unused, everything needed is in the model
 * @param model     the snapshot, holding the resolved groups
 * @param view      the rendering
 */
void
ocs::QConfCQueueShowController::process_request([[maybe_unused]] QConfParameter &parameter,
                                                QConfModelBase &model,
                                                QConfCQueueShowView &view) {
   DENTER(TOP_LAYER);

   lList *answer_list = nullptr;
   view.show_queues(out_, &answer_list, model);
   lFreeList(&answer_list);

   DRETURN_VOID;
}
