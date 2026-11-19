/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025-2026 HPC-Gridware GmbH
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
 * @brief Job categories, grouping jobs with identical scheduling requirements
 */

#include <cstdio>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_Category.h"

#include "ocs_BindingIo.h"
#include "sge_job.h"
#include "sge_resource_quota_service.h"
#include "sge_userprj.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_PR.h"

uint32_t ocs::Category::next_id = 0;

/**
 * @brief Hand out an unused category id
 *
 * @param master_category_list the categories in use (CT_Type), so no id is handed out twice; may be an empty handle
 *
 * @return an id that is neither 0 nor already taken
 */
uint32_t
ocs::Category::get_next_id(const ocs::tom::ConstList<ocs::tom::CT> master_category_list) {
   DENTER(TOP_LAYER);

   uint32_t id;

   // do not use 0 as ID or IDs that already have been used
   do {
      id = next_id++;
   } while (id == 0 || master_category_list.find_first<CT_id>(id));

   DRETURN(id);
}

/**
 * @brief Build the job category string
 *
 * This function builds the job category string for a job. The category string is
 * used to group jobs together for scheduling purposes. The function takes a
 * dstring object to store the category string and a job object to extract
 * information from. It also takes a lists to identify if a job indirectly
 * references certain configuration objects (e.g. resource quota sets).
 *
 * The queue and resource request lists of the job are sorted in place.
 *
 * @param category_str   The target string, contains the category or nothing
 * @param job            The job for the category creating (JB_Type, possibly reduced)
 * @param acl_list       Global access list (US_Type), may be an empty handle
 * @param prj_list       Project list (PR_Type), may be an empty handle
 * @param rqs_list       Resource quota set list (RQS_Type), may be an empty handle
 */
void
ocs::Category::build_string(dstring *category_str, const ocs::tom::Element<ocs::tom::JB> job,
                            const ocs::tom::ConstList<ocs::tom::US> acl_list,
                            const ocs::tom::ConstList<ocs::tom::PR> prj_list,
                            const ocs::tom::ConstList<ocs::tom::RQS> rqs_list) {
   DENTER(TOP_LAYER);

   // owner (user, UNIX group, and ACLs)
   const char *owner = ocs::tom::get<JB_owner>(job);
   const char *group = ocs::tom::get<JB_group>(job);
   const ocs::tom::ConstList<ocs::tom::ST> grp_list = ocs::tom::get<JB_grp_list>(job);
   sge_unparse_acl_dstring(category_str, owner, group, grp_list, acl_list, "-U");

   // -u if referenced in resource quota sets
   //
   // TODO: A possible performance enhancement is to split user and group inside category.
   // Some users are only referenced by the unix group. Their jobs could be grouped
   // together by referencing only the group in the category string
   if (sge_user_is_referenced_in_rqs(rqs_list, owner, group, grp_list, acl_list)) {
      sge_dstring_append(category_str, "-u ");
      sge_dstring_append(category_str, owner);
      sge_dstring_append_char(category_str, ' ');
   }

   // -scope global -hard -q <queue_list>
   sge_unparse_queue_list_dstring(category_str, job_get_queue_listRW(job, JRS_SCOPE_GLOBAL, true),
                                  "-scope global -hard -q");

   // -scope master -hard -q <queue_list>
   sge_unparse_queue_list_dstring(category_str, job_get_queue_listRW(job, JRS_SCOPE_MASTER, true),
                                  "-scope master -hard -q");

   // -scope slave -hard -q <queue_list>
   sge_unparse_queue_list_dstring(category_str, job_get_queue_listRW(job, JRS_SCOPE_SLAVE, true),
                                  "-scope slave -hard -q");

   // -scope global -hard -l <resource_list>
   sge_unparse_resource_list_dstring(category_str, job_get_resource_listRW(job, JRS_SCOPE_GLOBAL, true),
                                     "-scope global -hard -l");

   // -scope master -hard -l <resource_list>
   sge_unparse_resource_list_dstring(category_str, job_get_resource_listRW(job, JRS_SCOPE_MASTER, true),
                                     "-scope master -hard -l");

   // -scope slave -hard -l <resource_list>
   sge_unparse_resource_list_dstring(category_str, job_get_resource_listRW(job, JRS_SCOPE_SLAVE, true),
                                     "-scope slave -hard -l");

   // -scope global|master|slave -hard -par <allocation_rule>
   for (const auto request_set : ocs::tom::get<JB_request_set_list>(job)) {
      // Add the -par switch(es) to the category string.
      const char *allocation_rule = ocs::tom::get<JRS_allocation_rule>(request_set);
      switch (ocs::tom::get<JRS_scope>(request_set)) {
         case JRS_SCOPE_GLOBAL:
            sge_unparse_string_option_dstring(category_str, allocation_rule, "-scope global -par");
            break;
         case JRS_SCOPE_MASTER:
            sge_unparse_string_option_dstring(category_str, allocation_rule, "-scope master -par");
            break;
         case JRS_SCOPE_SLAVE:
            sge_unparse_string_option_dstring(category_str, allocation_rule, "-scope slave -par");
            break;
      }
   }

   // TODO: evaluate if soft requests should be part of the category string
#if 1
   // -scope global -soft -q <resource_list>
   sge_unparse_queue_list_dstring(category_str, job_get_queue_listRW(job, JRS_SCOPE_GLOBAL, false),
                                  "-scope global -soft -q");

   // -scope global -soft -l <resource_list>
   sge_unparse_resource_list_dstring(category_str, job_get_resource_listRW(job, JRS_SCOPE_GLOBAL, false),
                                     "-scope global -soft -l");
#endif

   // -btype -bunit -bamount -bsort -bstart -bstop
   sge_unparse_binding_dstring(category_str, ocs::tom::get<JB_binding>(job));

   // -pe pe_name pe_range
   sge_unparse_pe_dstring(category_str, job, "-pe");

   // -ckpt ckpt_name
   sge_unparse_string_option_dstring(category_str, ocs::tom::get<JB_checkpoint_name>(job), "-ckpt");

   // interactive job type
   const uint32_t job_type = ocs::tom::get<JB_type>(job);
   if (JOB_TYPE_IS_IMMEDIATE(job_type)) {
      sge_dstring_append(category_str, "-I y ");
   }

   // -P project
   const char *project = ocs::tom::get<JB_project>(job);

   if (project != nullptr) {
      const auto prj = prj_list.find_first<PR_name>(project);

      if (prj && ocs::tom::get<PR_consider_with_categories>(prj)) {
         sge_unparse_string_option_dstring(category_str, project, "-P");
      }
   }

   // -ar ar_id
   sge_unparse_ulong_option_dstring(category_str, ocs::tom::get<JB_ar>(job), "-ar");

   // remove the last white space that the last unparse function has written
   sge_dstring_strip_trailing_blanks(category_str);

   // avoid null pointer as category string in case job has no specific requests
   if (sge_dstring_get_string(category_str) == nullptr) {
      sge_dstring_append(category_str, "-");
   }

   DRETURN_VOID;
}

