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
 * @brief Marshalling the calendar modify parameters, see the header
 */

#include "sgeobj/cull/sge_all_listsL.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_CalendarModParameter.h"

/** @brief Marshal the object text into the request bundle
 * @return the bundle, owned by the caller
 */
lList *
ocs::CalendarModParameter::get_bundle() {
   DENTER(TOP_LAYER);

   lList *bundle = ProcedureParameter::get_bundle();

   lList *text_list = nullptr;
   lAddElemStr(&text_list, ST_name, object_text_.c_str(), ST_Type);
   add_parameter_bundle(bundle, OBJECT_TEXT, text_list);

   DRETURN(bundle);
}

/** @brief Read the object text back out of the request bundle
 *
 * An empty text arrives as a null pointer, because `unpackstr()` maps an empty
 * string to one by design, and is read back as an empty string. make_snapshot()
 * refuses it either way; what must not happen is a crash on the null pointer.
 *
 * @param bundle the request bundle received over GDI
 */
void
ocs::CalendarModParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   ProcedureParameter::set_bundle(bundle);

   const lListElem *text_param = lGetElemStr(bundle, SPP_name, OBJECT_TEXT);
   if (text_param != nullptr) {
      const char *value = lGetString(lFirst(lGetList(text_param, SPP_value_list)), ST_name);
      object_text_ = (value != nullptr) ? value : "";
   }

   DRETURN_VOID;
}

