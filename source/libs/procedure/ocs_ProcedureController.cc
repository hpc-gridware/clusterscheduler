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

/** @file
 * @brief The MVC framework the CLI commands are built on
 */

#include "uti/sge_rmon_macros.h"

#include "ocs_ProcedureController.h"

#include <iostream>

/** @brief Render the model data via the view.
 *
 * @param parameter  The parsed procedure parameters (used by subclass overrides).
 * @param model      The model whose data has already been fetched via make_snapshot().
 * @param view       The view that writes formatted output to out_.
 */
void ocs::ProcedureController::process_request(ProcedureParameter &parameter, ProcedureModel &model, ProcedureView &view) {
   DENTER(TOP_LAYER);
   view.show(out_, model.get_output_text());
   DRETURN_VOID;
}
