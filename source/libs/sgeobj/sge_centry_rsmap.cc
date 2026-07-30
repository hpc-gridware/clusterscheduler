/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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

#include <unordered_set>

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "sgeobj/sge_centry_rsmap.h"

bool centry_check_rsmap(lList **answer_list, u_long32 consumable, const char *attrname) {
   bool ret = true;

   if (consumable == CONSUMABLE_NO) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_INVALID_CENTRY_RSMAP_NOT_CONSUMABLE_S, attrname);
      ret = false;
   }

   return ret;
}

/**
 * Validate the per-instance characteristics on an RSMAP CE element. Each
 * RESL entry in CE_resource_map_list may carry a RESL_properties list of
 * ComplexEntry (CE_Type) elements — after the flatfile reader they hold only
 * CE_name and CE_stringval verbatim. This function resolves each name against
 * the master centry list, copies the referenced complex's valtype into the
 * property, type-checks its value via centry_fill_and_check, and rejects
 * duplicates within one id. Called from the host-side complex_values path
 * (centry_list_fill_request) after the top-level RSMAP entry has been typed.
 *
 * @param answer_list          answer list; validation errors are appended
 *                             with STATUS_EUNKNOWN/ANSWER_QUALITY_ERROR
 * @param centry               CE_Type element of a host's complex_values
 *                             entry; its CE_resource_map_list is walked and
 *                             each property's CE_name/CE_valtype/CE_doubleval
 *                             may be updated in place
 * @param master_centry_list   the master complex-list used to resolve
 *                             characteristic names and their valtypes
 * @return                     true if every property resolves and its value
 *                             parses; false on the first invalid property
 *                             (all detected problems are still reported to
 *                             the answer_list — the function does not stop
 *                             on the first error within a single centry)
 */
bool centry_check_rsmap_characteristics(lList **answer_list, lListElem *centry,
                                        const lList *master_centry_list) {
   bool ret = true;

   if (centry == nullptr) {
      return ret;
   }

   const char *rsmap_name = lGetString(centry, CE_name);
   lList *resource_map = lGetListRW(centry, CE_resource_map_list);
   if (resource_map == nullptr) {
      return ret;
   }

   lListElem *resl;
   for_each_rw (resl, resource_map) {
      lList *props = lGetListRW(resl, RESL_properties);
      if (props == nullptr || lGetNumberOfElem(props) == 0) {
         continue;
      }
      const char *id = lGetString(resl, RESL_value);

      std::unordered_set<std::string> seen;
      lListElem *prop;
      for_each_rw (prop, props) {
         const char *pname = lGetString(prop, CE_name);
         if (pname == nullptr) {
            continue;
         }

         // duplicate characteristic on the same id
         if (!seen.insert(pname).second) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_CHARACTERISTIC_DUPLICATE_SSS,
                                    rsmap_name, id != nullptr ? id : "", pname);
            ret = false;
            continue;
         }

         // resolve against master centry list
         const lListElem *master_cep = centry_list_locate(master_centry_list, pname);
         if (master_cep == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_CHARACTERISTIC_UNKNOWN_SSS,
                                    rsmap_name, id != nullptr ? id : "", pname);
            ret = false;
            continue;
         }

         // copy the canonical name (in case the user wrote a shortcut) and the valtype
         lSetString(prop, CE_name, lGetString(master_cep, CE_name));
         lSetUlong(prop, CE_valtype, lGetUlong(master_cep, CE_valtype));

         // type-check the value; centry_fill_and_check populates CE_doubleval for
         // numeric types and returns -1 on parse failure (with its own answer_list
         // entry). We wrap that with our own message so the host/RSMAP/id context
         // is preserved in the error.
         lList *sub_answers = nullptr;
         if (centry_fill_and_check(prop, &sub_answers, false, false) != 0) {
            const char *sub_msg = "";
            const lListElem *first = lFirst(sub_answers);
            if (first != nullptr) {
               const char *txt = lGetString(first, AN_text);
               if (txt != nullptr) {
                  sub_msg = txt;
               }
            }
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_CHARACTERISTIC_PARSE_SSSS,
                                    rsmap_name, id != nullptr ? id : "", pname, sub_msg);
            lFreeList(&sub_answers);
            ret = false;
            continue;
         }
         lFreeList(&sub_answers);
      }
   }

   return ret;
}
