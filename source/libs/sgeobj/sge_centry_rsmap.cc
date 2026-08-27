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
#include <string>

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "msg_common.h"

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
 * Give an RSMAP that was configured with a bare amount the ids it is missing.
 *
 * An RSMAP value may be written as a plain number, e.g. "gpu=4". The amount is then
 * known but no instance is, and since the scheduler picks the granted instances from
 * CE_resource_map_list it cannot grant anything at all - it discards the whole granted
 * resource list of the task. Such a value is read as "4 instances named 0 to 3", which
 * is what an administrator writing it that way expects, and what the value would look
 * like when written back: "gpu=4(0-3)".
 *
 * A value that already carries ids is left untouched, and so is an amount of 0, which
 * legitimately means "no instances at all".
 *
 * The expansion is bounded. An RSMAP materialises one CULL element per id, all of which
 * are spooled and travel with the exec host object, so a mistyped or simply large amount
 * would otherwise cost the qmaster a lot of memory. Beyond max_ids the value is rejected
 * and the administrator has to list the ids or raise the limit. max_ids of 0 turns the
 * implicit ids off completely, so that a bare amount is rejected as an RSMAP that needs
 * to define its ids.
 *
 * @param answer_list  answer list; a rejection is appended with
 *                     STATUS_EUNKNOWN/ANSWER_QUALITY_ERROR
 * @param centry       CE_Type element of a host's complex_values entry. Requires
 *                     CE_valtype and CE_doubleval to be filled in, i.e. the element
 *                     must have passed centry_list_fill_request()
 * @param max_ids      upper bound for the number of ids to create, 0 disables the
 *                     implicit ids
 * @return             true if nothing had to be done or the ids were created,
 *                     false if the amount exceeds max_ids
 */
bool
centry_rsmap_expand_implicit_ids(lList **answer_list, lListElem *centry, u_long32 max_ids) {
   if (centry == nullptr || lGetUlong(centry, CE_valtype) != TYPE_RSMAP) {
      return true;
   }

   // ids have been configured - nothing to do
   if (lGetList(centry, CE_resource_map_list) != nullptr) {
      return true;
   }

   const double dval = lGetDouble(centry, CE_doubleval);
   if (dval <= 0) {
      // "gpu=0" - the host provides no instance, which is a valid configuration
      return true;
   }
   const auto amount = static_cast<u_long32>(dval);

   if (max_ids == 0) {
      // the implicit ids are switched off - the instances have to be named explicitly
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_CONFIG_CONF_RSMAP_NEEDS_IDS_S, lGetString(centry, CE_name));
      return false;
   }
   if (amount > max_ids) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_RSMAP_TOO_MANY_IMPLICIT_IDS_SUU,
                              lGetString(centry, CE_name), amount, max_ids);
      return false;
   }

   for (u_long32 id = 0; id < amount; id++) {
      std::string id_str{std::to_string(id)};
      lListElem *resl = lAddSubStr(centry, RESL_value, id_str.c_str(),
                                   CE_resource_map_list, RESL_Type);
      if (resl != nullptr) {
         lSetUlong(resl, RESL_amount, 1);
      }
   }

   return true;
}

/**
 * Expand the implicit ids of every RSMAP in a host's complex_values list.
 *
 * Wrapper around centry_rsmap_expand_implicit_ids() which takes the limit from
 * MAX_RSMAP_IDS in qmaster_params. All entries are processed even if one of them is
 * rejected, so that an administrator sees every offending RSMAP at once.
 *
 * @param answer_list  answer list; rejections are appended
 * @param centry_list  CE_Type list of an exec host (EH_consumable_config_list)
 * @return             true if no entry was rejected
 */
bool
centry_list_rsmap_expand_implicit_ids(lList **answer_list, lList *centry_list) {
   bool ret = true;
   const auto max_ids = static_cast<u_long32>(mconf_get_max_rsmap_ids());

   lListElem *centry;
   for_each_rw(centry, centry_list) {
      if (!centry_rsmap_expand_implicit_ids(answer_list, centry, max_ids)) {
         ret = false;
      }
   }

   return ret;
}

