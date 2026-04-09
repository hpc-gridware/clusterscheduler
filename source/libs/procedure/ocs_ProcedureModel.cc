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

#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/sge_str.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_ProcedureModel.h"

#include <iostream>
#include <ostream>

#include "ocs_ProcedureParameter.h"
#include "sgeobj/sge_answer.h"

bool
ocs::ProcedureModel::make_snapshot(lList **answer_list, ProcedureParameter &parameter) {
   DENTER(TOP_LAYER);

   // Fetch the SPP_Type list and pass it to qmaster
   lList *request_and_response = parameter.get_bundle();

   *answer_list = gdi::Client::sge_gdi(gdi::Target::PROCEDURE, gdi::Command::PROCEDURE, gdi::SubCommand::NONE,
                                     &request_and_response, nullptr, nullptr);

   if (answer_list_has_error(answer_list)) {
      lFreeList(&request_and_response);
      DRETURN(false);
   }

   procedure_response = request_and_response;
   request_and_response = nullptr;

   DRETURN(true);
}

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
