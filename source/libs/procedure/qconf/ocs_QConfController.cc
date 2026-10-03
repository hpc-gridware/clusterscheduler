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
 * @brief Driving one `qconf` read operation from model to view
 */

#include "uti/sge_rmon_macros.h"

#include "ocs_QConfController.h"

/**
 * @brief Render what the model fetched
 *
 * The read path of `qconf` has no report structure to walk, unlike qquota, so
 * there is nothing here but the handover - to one of two renderings, depending
 * on what the switch asked for.
 *
 * @param parameter what the call asked for
 * @param model     what it fetched
 * @param view      how it renders
 * @return none
 */
void
ocs::QConfController::process_request([[maybe_unused]] QConfParameter &parameter, QConfModelBase &model,
                                      QConfViewBase &view) {
   DENTER(TOP_LAYER);

   if (model.get_kind() == QConfReadKind::NAME_LIST) {
      view.show_name_list(out_, model.get_object_list(), model.get_key_nm(), *model.get_desc());
   } else if (model.get_kind() == QConfReadKind::OBJECT_LIST) {
      view.show_object_list(out_, model.get_object_list(), *model.get_desc());
   } else {
      view.show_object(out_, lFirst(model.get_object_list()), *model.get_desc());
   }

   DRETURN_VOID;
}
