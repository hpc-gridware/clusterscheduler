/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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
 * @brief Marshalling the qconf write parameters, see the header
 */

#include "sgeobj/cull/sge_all_listsL.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_QConfWriteParameter.h"

/** @brief Add one object to write
 * @param label how the caller wants this item named in the answer
 * @param text the object in the form a read renders it
 */
void
ocs::QConfWriteParameter::add_item(const char *label, const std::string &text) {
   DENTER(TOP_LAYER);

   lListElem *item = lAddElemStr(&items_, VA_variable, label != nullptr ? label : "", VA_Type);
   lSetString(item, VA_value, text.c_str());

   DRETURN_VOID;
}

/** @brief Marshal the items and the batch policy into the request bundle
 *
 * The items go over as a `VA_Type` list, the pair shape ProcedureParameter
 * already uses for its own name/value entries - so no new CULL type and no
 * change to the wire format.
 *
 * @return the bundle, owned by the caller
 */
lList *
ocs::QConfWriteParameter::get_bundle() {
   DENTER(TOP_LAYER);

   lList *bundle = ProcedureParameter::get_bundle();

   add_parameter_bundle(bundle, OBJECT_SET, lCopyList("", items_));

   lList *mode_list = nullptr;
   lAddElemUlong(&mode_list, ULNG_value, mode_, ULNG_Type);
   add_parameter_bundle(bundle, WRITE_MODE, mode_list);

   /* sent only when there is a selection, so a request that writes everything
    * looks exactly as it did before this existed */
   if (!select_.empty()) {
      lList *select_list = nullptr;
      lAddElemStr(&select_list, ST_name, select_.c_str(), ST_Type);
      add_parameter_bundle(bundle, OBJECT_SELECT, select_list);
   }

   DRETURN(bundle);
}

/** @brief Read the items and the batch policy back out of the request bundle
 *
 * The item list is taken over rather than copied: it is the request's payload
 * and the bundle is freed right after the parameters are built.
 *
 * An empty payload arrives as a null pointer, because `unpackstr()` maps an
 * empty string to one by design. That is read back as an empty text and the
 * provider refuses it per item - what must not happen is a crash on the null
 * pointer.
 *
 * @param bundle the request bundle received over GDI
 */
void
ocs::QConfWriteParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   ProcedureParameter::set_bundle(bundle);

   lListElem *set_param = lGetElemStrRW(const_cast<lList *>(bundle), SPP_name, OBJECT_SET);
   if (set_param != nullptr) {
      lFreeList(&items_);
      lXchgList(set_param, SPP_value_list, &items_);
   }

   const lListElem *mode_param = lGetElemStr(bundle, SPP_name, WRITE_MODE);
   if (mode_param != nullptr) {
      mode_ = static_cast<uint32_t>(lGetUlong(lFirst(lGetList(mode_param, SPP_value_list)), ULNG_value));
   }

   const lListElem *select_param = lGetElemStr(bundle, SPP_name, OBJECT_SELECT);
   if (select_param != nullptr) {
      const lListElem *value = lFirst(lGetList(select_param, SPP_value_list));
      const char *names = (value != nullptr) ? lGetString(value, ST_name) : nullptr;
      select_ = (names != nullptr) ? names : "";
   }

   DRETURN_VOID;
}
