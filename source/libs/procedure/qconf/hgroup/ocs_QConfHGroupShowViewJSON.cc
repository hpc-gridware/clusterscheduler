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
#include "sgeobj/tom/ocs_tom_HR.h"
#include "tom/ocs_List.h"

#include "spool/flatfile/ocs_spool_json.h"

#include "ocs_QConfHGroupShowViewJSON.h"
#include "sgeobj/sge_answer.h"

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
 * The parameter stays untyped: this overrides the generic show_object(), whose
 * element type is only known at run time; it is taken into a host group handle
 * once. The derived lists are `HR_Type`.
 *
 * @param os   the stream to write to
 * @param ep   the host group
 * @param desc its row in the descriptor table, for the field list
 * @return none
 */
void
ocs::QConfHGroupShowViewJSON::show_object(std::ostream &os, const ocs::tom::ConstRawElement ep,
                                          const qconf_object_desc &desc) {
   DENTER(TOP_LAYER);

   // the view of -shgrp renders host groups only, see QConfHGroupShowController
   const auto hgroup = ocs::tom::ConstElement<ocs::tom::HGRP>(ep.raw());
   const auto [has_matcher, derived_hosts, derived_groups, derived_matchers] = hgroup_split_members(hgroup);

   if (!has_matcher) {
      /* no matchers: the generic rendering is the whole answer */
      QConfViewBase::show_object(os, hgroup, desc);
      DRETURN_VOID;
   }

   const ocs_json_derived_list derived[] = {
      {"hosts",      *derived_hosts,    HR_name},
      {"hostgroups", *derived_groups,   HR_name},
      {"matchers",   *derived_matchers, HR_name},
      {nullptr,      nullptr,           0}
   };

   const qconf_build_fields_fn provider =
      (desc.read.fields != nullptr) ? desc.read.fields : desc.build_fields;
   bool owned = false;
   spooling_field *fields = provider(SP_FORM_JSON, more_info_, &owned);

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   dstring document = DSTRING_INIT;
   if (spool_json_write_object_ex(&alp, hgroup, fields, derived, &document)) {
      const char *text = sge_dstring_get_string(&document);
      if (text != nullptr) {
         os << text;
      }
   }
   sge_dstring_free(&document);
   if (owned) {
      sge_free(&fields);
   }
   alp.reset();

   DRETURN_VOID;
}
