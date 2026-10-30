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
#include "uti/sge_rmon_macros.h"

#include "ocs_QConfActionParameter.h"

/** @brief Set one argument, see the header
 * @param name the argument
 * @param value its value
 */
void
ocs::QConfActionParameter::set_arg(const char *name, const std::string &value) {
   DENTER(TOP_LAYER);

   lListElem *arg = lGetElemStrRW(args_, VA_variable, name);
   if (arg == nullptr) {
      arg = lAddElemStr(&args_, VA_variable, name, VA_Type);
   }
   lSetString(arg, VA_value, value.c_str());

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
   const lListElem *arg = lGetElemStr(args_, VA_variable, name);
   const char *value = (arg != nullptr) ? lGetString(arg, VA_value) : nullptr;
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

   lListElem *args_param = lGetElemStrRW(const_cast<lList *>(bundle), SPP_name, ACTION_ARGS);
   if (args_param != nullptr) {
      lFreeList(&args_);
      lXchgList(args_param, SPP_value_list, &args_);
   }

   DRETURN_VOID;
}
