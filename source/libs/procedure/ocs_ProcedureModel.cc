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
#include "sgeobj/cull/sge_ulong_ULNG_L.h"
#include "sgeobj/sge_str.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_ProcedureModel.h"

#include <iostream>
#include <ostream>

#include "ocs_ProcedureParameter.h"
#include "sgeobj/sge_answer.h"

/** @brief Send the procedure request to qmaster and store the response.
 *
 * Serialises @p parameter into a CULL bundle, issues a GDI procedure request
 * under the command get_command() names, and stores the resulting response
 * list.  On success the rendered output is accessible via get_output_text().
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

   *answer_list = gdi::Client::sge_gdi(get_target(), get_command(), gdi::SubCommand::NONE,
                                       &request_and_response, nullptr, nullptr);

   /* CS-2820: the response is kept even when the answer list carries an error.
    * A procedure may legitimately render output *and* report a problem -
    * "qconf -sstnode /,/nosuchnode" prints the node it found and reports the one
    * it did not - and exec_procedure() in qmaster makes exactly that
    * distinction: it skips the rendering only when the snapshot fails. Dropping
    * the text here discarded half of what the server had decided to send. A read
    * that really failed renders nothing, so its response is empty and the
    * caller prints nothing extra. */
   procedure_response = request_and_response;
   request_and_response = nullptr;

   DRETURN(!answer_list_has_error(answer_list));
}

/** @brief What RBAC removed from this response, see the header
 * @return the reduction mask, zero when the entry is absent
 */
uint32_t
ocs::ProcedureModel::get_reduction() const {
   DENTER(TOP_LAYER);

   /* The two "not reduced" answers are traced apart on purpose. Both return
    * zero, so a reading of zero alone cannot tell "qmaster said nothing was
    * reduced" from "the entry never arrived" - and the second would mean the
    * channel is broken while looking exactly like the healthy case. */
   const lListElem *entry = lGetElemStr(procedure_response, SPP_name, ProcedureParameter::REDUCTION);
   if (entry == nullptr) {
      DPRINTF("no reduction entry in the response\n");
      DRETURN(ProcedureParameter::REDUCTION_NONE);
   }
   const lListElem *value = lFirst(lGetList(entry, SPP_value_list));
   if (value == nullptr) {
      DPRINTF("reduction entry present but empty\n");
      DRETURN(ProcedureParameter::REDUCTION_NONE);
   }

   DRETURN(static_cast<uint32_t>(lGetUlong(value, ULNG_value)));
}

/** @brief What a write procedure did, see the header
 * @param applied receives how many items were written
 * @param parse_failed receives how many could not be read
 * @param elements receives how many objects that was, nullptr to ignore it
 * @return true when the response carried a write result
 */
bool
ocs::ProcedureModel::get_write_result(uint32_t &applied, uint32_t &parse_failed,
                                      uint32_t *elements) const {
   DENTER(TOP_LAYER);

   const lListElem *applied_entry = lGetElemStr(procedure_response, SPP_name,
                                                ProcedureParameter::WRITE_APPLIED);
   const lListElem *failed_entry = lGetElemStr(procedure_response, SPP_name,
                                               ProcedureParameter::WRITE_PARSE_FAILED);
   if (applied_entry == nullptr || failed_entry == nullptr) {
      DPRINTF("no write result in the response\n");
      DRETURN(false);
   }

   const lListElem *applied_value = lFirst(lGetList(applied_entry, SPP_value_list));
   const lListElem *failed_value = lFirst(lGetList(failed_entry, SPP_value_list));
   if (applied_value == nullptr || failed_value == nullptr) {
      DPRINTF("write result present but empty\n");
      DRETURN(false);
   }

   applied = static_cast<uint32_t>(lGetUlong(applied_value, ULNG_value));
   parse_failed = static_cast<uint32_t>(lGetUlong(failed_value, ULNG_value));

   /* The element count is reported next to the two above and asked for by the
    * one caller that prints a count of objects. It is read on its own, so a
    * response that carries only the two older entries is still a complete
    * write result - the counts a caller must have are those two. */
   if (elements != nullptr) {
      const lListElem *elements_entry = lGetElemStr(procedure_response, SPP_name,
                                                    ProcedureParameter::WRITE_ELEMENTS);
      const lListElem *elements_value = (elements_entry != nullptr)
                                        ? lFirst(lGetList(elements_entry, SPP_value_list))
                                        : nullptr;
      if (elements_value != nullptr) {
         *elements = static_cast<uint32_t>(lGetUlong(elements_value, ULNG_value));
      }
   }

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
