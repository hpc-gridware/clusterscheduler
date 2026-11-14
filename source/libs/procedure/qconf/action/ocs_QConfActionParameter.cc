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

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_Element.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_QConfActionParameter.h"

/** @brief Set one argument, see the header
 * @param name the argument
 * @param value its value
 */
void
ocs::QConfActionParameter::set_arg(const char *name, const std::string &value) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::VA> arg = ocs::tom::find_first_rw<VA_variable>(args_, name);
   if (arg == nullptr) {
      arg = ocs::tom::emplace_back<VA_variable>(&args_, name);
   }
   ocs::tom::set<VA_value>(arg, value.c_str());

   DRETURN_VOID;
}

/** @brief Read one argument, see the header
 *
 * An empty value arrives as a null pointer, because unpackstr() maps an empty
 * string to one, and is read back as an empty string.
 *
 * @param name the argument
 * @return its value, empty when it is missing
 */
std::string
ocs::QConfActionParameter::get_arg(const char *name) const {
   ocs::tom::ConstElement<ocs::tom::VA> arg = ocs::tom::find_first<VA_variable>(args_, name);
   const char *value = (arg != nullptr) ? ocs::tom::get<VA_value>(arg) : nullptr;
   return (value != nullptr) ? value : "";
}

/** @brief Marshal the arguments into the request bundle
 *
 * As a `VA_Type` list, the pair shape the write procedure uses for its items -
 * no new CULL type and no change to the wire format.
 *
 * @return the bundle, owned by the caller
 */
lList *
ocs::QConfActionParameter::get_bundle() {
   DENTER(TOP_LAYER);

   lList *bundle = ProcedureParameter::get_bundle();
   add_parameter_bundle(bundle, ACTION_ARGS, lCopyList("", args_));

   DRETURN(bundle);
}

/** @brief Read the arguments back out of the request bundle
 *
 * The argument list is taken over rather than copied: it is the request's
 * payload and the bundle is freed right after the parameters are built.
 *
 * @param bundle the request bundle received over GDI
 */
void
ocs::QConfActionParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   ProcedureParameter::set_bundle(bundle);

   ocs::tom::Element<ocs::tom::SPP> args_param = ocs::tom::find_first_rw<SPP_name>(const_cast<lList *>(bundle), ACTION_ARGS);
   if (args_param != nullptr) {
      lFreeList(&args_);
      ocs::tom::exchange<SPP_value_list>(args_param, &args_);
   }

   DRETURN_VOID;
}
