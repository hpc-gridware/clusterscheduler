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

#include "sgeobj/sge_str.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_ProcedureModel.h"

#include <iostream>
#include <ostream>

#include "ocs_ProcedureParameter.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Element.h"

/** @brief Send the procedure request to qmaster and store the response.
 *
 * Serialises @p parameter into a bundle, issues a GDI procedure request
 * under the command get_command() names, and stores the resulting response
 * list.  On success the rendered output is accessible via get_output_text().
 *
 * @param answer_list  Receives error messages on failure.
 * @param parameter    The parsed procedure parameters to send.
 * @return true on success, false if the GDI call returned an error.
 */
bool
ocs::ProcedureModel::make_snapshot(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ProcedureParameter &parameter) {
   DENTER(TOP_LAYER);

   // Fetch the SPP list and pass it to qmaster; the bundle is replaced by the response, which is SPP too
   auto request_and_response = parameter.get_bundle();

   *answer_list = gdi::Client::sge_gdi(get_target(), get_command(), gdi::SubCommand::NONE, request_and_response);

   /* CS-2820: the response is kept even when the answer list carries an error.
    * A procedure may legitimately render output *and* report a problem -
    * "qconf -sstnode /,/nosuchnode" prints the node it found and reports the one
    * it did not - and exec_procedure() in qmaster makes exactly that
    * distinction: it skips the rendering only when the snapshot fails. Dropping
    * the text here discarded half of what the server had decided to send. A read
    * that really failed renders nothing, so its response is empty and the
    * caller prints nothing extra. */
   procedure_response = std::move(request_and_response);

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
   const auto entry = procedure_response.get().find_first<SPP_name>(ProcedureParameter::REDUCTION);
   if (entry == nullptr) {
      DPRINTF("no reduction entry in the response\n");
      DRETURN(ProcedureParameter::REDUCTION_NONE);
   }
   // the value list is of any type; the checked handle throws on another type, as the
   // getter on the raw element did
   const auto value = entry.get_as<SPP_value_list, ocs::tom::ULNG>().front();
   if (!value) {
      DPRINTF("reduction entry present but empty\n");
      DRETURN(ProcedureParameter::REDUCTION_NONE);
   }

   DRETURN(value.get<ULNG_value>());
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

   ocs::tom::ConstElement<ocs::tom::SPP> applied_entry = procedure_response.get().find_first<SPP_name>(
                                                ProcedureParameter::WRITE_APPLIED);
   ocs::tom::ConstElement<ocs::tom::SPP> failed_entry = procedure_response.get().find_first<SPP_name>(
                                               ProcedureParameter::WRITE_PARSE_FAILED);
   if (applied_entry == nullptr || failed_entry == nullptr) {
      DPRINTF("no write result in the response\n");
      DRETURN(false);
   }

   const auto applied_value =
           applied_entry.get_as<SPP_value_list, ocs::tom::ULNG>().front();
   const auto failed_value = failed_entry.get_as<SPP_value_list, ocs::tom::ULNG>().front();
   if (!applied_value || !failed_value) {
      DPRINTF("write result present but empty\n");
      DRETURN(false);
   }

   applied = applied_value.get<ULNG_value>();
   parse_failed = failed_value.get<ULNG_value>();

   /* The element count is reported next to the two above and asked for by the
    * one caller that prints a count of objects. It is read on its own, so a
    * response that carries only the two older entries is still a complete
    * write result - the counts a caller must have are those two. */
   if (elements != nullptr) {
      ocs::tom::ConstElement<ocs::tom::SPP> elements_entry = procedure_response.get().find_first<SPP_name>(
                                                    ProcedureParameter::WRITE_ELEMENTS);
      const auto elements_value =
              elements_entry
              ? elements_entry.get_as<SPP_value_list, ocs::tom::ULNG>().front()
              : nullptr;
      if (elements_value) {
         *elements = elements_value.get<ULNG_value>();
      }
   }

   DRETURN(true);
}

/** @brief Return the exported objects received from qmaster, see the header
 * @return the name/value pairs; std::nullopt when the response carried no export result
 */
std::optional<ocs::tom::ConstList<ocs::tom::VA>>
ocs::ProcedureModel::get_export_objects() const {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::SPP> entry = procedure_response.get().find_first<SPP_name>(
                                        ProcedureParameter::EXPORT_OBJECTS);
   if (entry == nullptr) {
      DPRINTF("no export entry in the response\n");
      DRETURN(std::nullopt);
   }

   // an empty export is stored as no list at all; get_as() gives an empty view then, and a list of another
   // type than VA throws WrongType
   const auto exported = entry.get_as<SPP_value_list, ocs::tom::VA>();
   DPRINTF("export entry holds %d object(s)\n", static_cast<int>(exported.size()));
   DRETURN(exported);
}

/** @brief Return the pre-rendered output text received from qmaster.
 *
 * Returns an empty string if the response list is absent or malformed, and
 * also when the text itself is empty: an empty string travels over GDI as a
 * null pointer, and a null written to a stream fails the stream for good, which
 * swallowed every later line of output after a failed read (CS-2956).
 * The returned pointer is valid for the lifetime of this object.
 *
 * @return the rendered text, never null; empty when there is none
 */
const char *
ocs::ProcedureModel::get_output_text() const {
   DENTER(TOP_LAYER);
   const auto response_elem = procedure_response.get().find_first<SPP_name>(ProcedureParameter::RESPONSE);
   if (!response_elem) {
      DRETURN("");
   }
   const auto output_elem = response_elem.get_as<SPP_value_list, ocs::tom::ST>().front();
   if (!output_elem) {
      DRETURN("");
   }
   const char *output = output_elem.get<ST_name>();
   DRETURN(output != nullptr ? output : "");
}
