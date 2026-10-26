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
 * @brief See the header (CS-2816)
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_hgroup.h"

#include "spool/flatfile/ocs_spool_json.h"

#include "ocs_QConfHGroupShowViewJSON.h"

/** @brief Write one host group as JSON, with its member classes split out
 *
 * The derived lists are computed from the member list at render time, which is
 * why this cannot go through spool_flatfile_write_object(): that one renders the
 * fields of the object, and these are three keys that are not fields. A group
 * without matchers is rendered by the generic path, so nothing changes for an
 * installation that does not use them.
 *
 * Errors are dropped the same way QConfViewBase::show_object() drops them: a
 * view is handed no answer list, because by the time one is chosen the framework
 * has no way left to report. Anything that can fail meaningfully is checked in
 * the model.
 *
 * @param os   the stream to write to
 * @param ep   the host group
 * @param desc its row in the descriptor table, for the field list
 */
void
ocs::QConfHGroupShowViewJSON::show_object(std::ostream &os, const lListElem *ep,
                                          const qconf_object_desc &desc) {
   DENTER(TOP_LAYER);

   lList *derived_hosts = nullptr;
   lList *derived_groups = nullptr;
   lList *derived_matchers = nullptr;

   if (!hgroup_split_members(ep, &derived_hosts, &derived_groups, &derived_matchers)) {
      /* no matchers: the generic rendering is the whole answer */
      lFreeList(&derived_hosts);
      lFreeList(&derived_groups);
      lFreeList(&derived_matchers);
      QConfViewBase::show_object(os, ep, desc);
      DRETURN_VOID;
   }

   const ocs_json_derived_list derived[] = {
      {"hosts",      derived_hosts,    HR_name},
      {"hostgroups", derived_groups,   HR_name},
      {"matchers",   derived_matchers, HR_name},
      {nullptr,      nullptr,          0}
   };

   const qconf_build_fields_fn provider =
      (desc.read.fields != nullptr) ? desc.read.fields : desc.build_fields;
   bool owned = false;
   spooling_field *fields = provider(SP_FORM_JSON, more_info_, &owned);

   lList *alp = nullptr;
   dstring document = DSTRING_INIT;
   if (spool_json_write_object_ex(&alp, ep, fields, derived, &document)) {
      const char *text = sge_dstring_get_string(&document);
      if (text != nullptr) {
         os << text;
      }
   }
   sge_dstring_free(&document);
   if (owned) {
      sge_free(&fields);
   }
   lFreeList(&alp);

   lFreeList(&derived_hosts);
   lFreeList(&derived_groups);
   lFreeList(&derived_matchers);

   DRETURN_VOID;
}
