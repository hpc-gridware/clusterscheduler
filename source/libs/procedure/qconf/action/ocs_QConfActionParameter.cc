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
 * @brief Marshalling the qconf action parameters, see the header
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_QConfActionParameter.h"

/** @brief Set one argument, see the header
 * @param name the argument
 * @param value its value
 * @return none
 */
void
ocs::QConfActionParameter::set_arg(const char *name, const std::string &value) {
   DENTER(TOP_LAYER);

   if (args_ == nullptr) {
      args_ = ocs::tom::ListPtr<ocs::tom::VA>::create("");
   }
   auto arg = args_->find_first<VA_variable>(name);
   if (arg == nullptr) {
      arg = args_->emplace_back<VA_variable>(name);
   }
   arg.set<VA_value>(value.c_str());

   DRETURN_VOID;
}

/** @brief Read one argument, see the header
 *
 * An empty value arrives as a null pointer, because ocs::tom::get_str() maps an empty
 * string to one, and is read back as an empty string.
 *
 * @param name the argument
 * @return its value, empty when it is missing
 */
std::string
ocs::QConfActionParameter::get_arg(const char *name) const {
   const ocs::tom::ConstElement<ocs::tom::VA> arg = args_->find_first<VA_variable>(name);
   const char *value = (arg != nullptr) ? arg.get<VA_value>() : nullptr;
   return (value != nullptr) ? value : "";
}

/** @brief Marshal the arguments into the request bundle
 *
 * As a `VA` list, the pair shape the write procedure uses for its items -
 * no new CULL type and no change to the wire format.
 *
 * @return the bundle, owned by the caller
 */
ocs::tom::ListPtr<ocs::tom::SPP> ocs::QConfActionParameter::get_bundle() {
   DENTER(TOP_LAYER);

   auto bundle = ProcedureParameter::get_bundle();
   add_parameter_bundle(*bundle, ACTION_ARGS, args_->clone(""));

   DRETURN(bundle);
}

/** @brief Read the arguments back out of the request bundle
 *
 * The argument list is taken over rather than copied: it is the request's
 * payload and the bundle is freed right after the parameters are built.
 *
 * The list arrives through the untyped value list of the bundle entry. It is taken
 * over without the descriptor check (from_typed), as the raw member was before; the
 * typed field reads still check every element.
 *
 * @param bundle the request bundle received over GDI
 * @return none
 */
void
ocs::QConfActionParameter::set_bundle(const ocs::tom::List<ocs::tom::SPP> bundle) {
   DENTER(TOP_LAYER);

   ProcedureParameter::set_bundle(bundle);

   const auto args_param = bundle.find_first<SPP_name>(ACTION_ARGS);
   if (args_param != nullptr) {
      args_ = ocs::tom::ListPtr<ocs::tom::VA>::from_typed(
         args_param.exchange<SPP_value_list>(ocs::tom::ListPtr<ocs::tom::VA>{}).release());
   }

   DRETURN_VOID;
}
