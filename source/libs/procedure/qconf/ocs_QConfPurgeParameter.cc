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
 * @brief Marshalling the qconf purge parameters, see the header
 */

#include "sgeobj/cull/sge_all_listsL.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_QConfPurgeParameter.h"

namespace {
   /** @brief Add one string to the bundle as a one element ST_Type list
    * @param bundle the request bundle
    * @param name the parameter name
    * @param value the string
    */
   void
   add_string(lList *bundle, const char *name, const std::string &value) {
      lList *list = nullptr;
      lAddElemStr(&list, ST_name, value.c_str(), ST_Type);
      ocs::ProcedureParameter::add_parameter_bundle(bundle, name, list);
   }

   /** @brief Read one string back that add_string() put into the bundle
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

/** @brief Marshal what to purge into the request bundle
 * @return the bundle, owned by the caller
 */
lList *
ocs::QConfPurgeParameter::get_bundle() {
   DENTER(TOP_LAYER);

   lList *bundle = ProcedureParameter::get_bundle();
   add_string(bundle, PURGE_QUEUE, queue_);
   add_string(bundle, PURGE_ATTRIBUTES, attributes_);
   add_string(bundle, PURGE_DOMAIN, domain_);

   DRETURN(bundle);
}

/** @brief Read what to purge back out of the request bundle
 * @param bundle the request bundle received over GDI
 */
void
ocs::QConfPurgeParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   ProcedureParameter::set_bundle(bundle);
   queue_ = get_string(bundle, PURGE_QUEUE);
   attributes_ = get_string(bundle, PURGE_ATTRIBUTES);
   domain_ = get_string(bundle, PURGE_DOMAIN);

   DRETURN_VOID;
}
