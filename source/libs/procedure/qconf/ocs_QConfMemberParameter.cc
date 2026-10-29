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
 * @brief Marshalling the access list membership parameters, see the header
 */

#include "sgeobj/cull/sge_all_listsL.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_QConfMemberParameter.h"

namespace {
   /** @brief Read one string back that get_bundle() put into the bundle
    *
    * An empty string arrives as a null pointer, because unpackstr() maps it to
    * one, and is read back as an empty string.
    *
    * @param bundle the request bundle
    * @param name the parameter name
    * @return the string, empty when it is missing
    */
   std::string
   get_string(const lList *bundle, const char *name) {
      const lListElem *param = lGetElemStr(bundle, SPP_name, name);
      const lListElem *value = (param != nullptr) ? lFirst(lGetList(param, SPP_value_list)) : nullptr;
      const char *str = (value != nullptr) ? lGetString(value, ST_name) : nullptr;
      return (str != nullptr) ? str : "";
   }
}

/** @brief Marshal the users, the lists and the direction into the request bundle
 * @return the bundle, owned by the caller
 */
lList *
ocs::QConfMemberParameter::get_bundle() {
   DENTER(TOP_LAYER);

   lList *bundle = ProcedureParameter::get_bundle();

   lList *users = nullptr;
   lAddElemStr(&users, ST_name, users_.c_str(), ST_Type);
   add_parameter_bundle(bundle, MEMBER_USERS, users);

   lList *lists = nullptr;
   lAddElemStr(&lists, ST_name, lists_.c_str(), ST_Type);
   add_parameter_bundle(bundle, MEMBER_LISTS, lists);

   lList *add = nullptr;
   lAddElemUlong(&add, ULNG_value, add_ ? 1 : 0, ULNG_Type);
   add_parameter_bundle(bundle, MEMBER_ADD, add);

   DRETURN(bundle);
}

/** @brief Read the users, the lists and the direction back out of the request bundle
 * @param bundle the request bundle received over GDI
 */
void
ocs::QConfMemberParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   ProcedureParameter::set_bundle(bundle);
   users_ = get_string(bundle, MEMBER_USERS);
   lists_ = get_string(bundle, MEMBER_LISTS);

   const lListElem *add_param = lGetElemStr(bundle, SPP_name, MEMBER_ADD);
   if (add_param != nullptr) {
      add_ = lGetUlong(lFirst(lGetList(add_param, SPP_value_list)), ULNG_value) != 0;
   }

   DRETURN_VOID;
}
