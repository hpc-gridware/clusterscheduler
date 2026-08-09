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
 * @brief Base of the models: fetching or computing what a command reports
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/sge_str.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_ProcedureModel.h"

#include <iostream>
#include <ostream>

#include "ocs_ProcedureParameter.h"
#include "sgeobj/sge_answer.h"

/** @brief Send the procedure request to qmaster and store the response.
 *
 * Serialises @p parameter into a CULL bundle, issues a GDI GET_PROCEDURE
 * request, and stores the resulting response list.  On success the rendered
 * output is accessible via get_output_text().
 *
 * @param answer_list  Receives error messages on failure.
 * @param parameter    The parsed procedure parameters to send.
 * @return true on success, false if the GDI call returned an error.
 */
bool
ocs::ProcedureModel::make_snapshot(lList **answer_list, ProcedureParameter &parameter) {
   DENTER(TOP_LAYER);

   // Fetch the SPP_Type list and pass it to qmaster
   lList *request_and_response = parameter.get_bundle();

   *answer_list = gdi::Client::sge_gdi(gdi::Target::PROCEDURE, gdi::Command::GET_PROCEDURE, gdi::SubCommand::NONE,
                                       &request_and_response, nullptr, nullptr);

   if (answer_list_has_error(answer_list)) {
      lFreeList(&request_and_response);
      DRETURN(false);
   }

   procedure_response = request_and_response;
   request_and_response = nullptr;

   DRETURN(true);
}

/** @brief Return the pre-rendered output text received from qmaster.
 *
 * Returns an empty string if the response list is absent or malformed.
 * The returned pointer is valid for the lifetime of this object.
 *
 * @return the rendered text, empty when the response is absent or malformed
 */
const char *
ocs::ProcedureModel::get_output_text() const {
   DENTER(TOP_LAYER);
   const lListElem *response_elem = lGetElemStr(procedure_response, SPP_name, ProcedureParameter::RESPONSE);
   if (!response_elem) {
      DRETURN("");
   }
   const lListElem *output_elem = lFirst(lGetList(response_elem, SPP_value_list));
   if (!output_elem) {
      DRETURN("");
   }
   const char *output = lGetString(output_elem, ST_name);
   DRETURN(output);
}
