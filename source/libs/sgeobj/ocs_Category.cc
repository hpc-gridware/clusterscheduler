/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include <cstdio>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_Category.h"

#include "ocs_BindingIo.h"
#include "sge_job.h"
#include "sge_resource_quota_service.h"
#include "sge_userprj.h"

u_long32 ocs::Category::next_id = 0;

/** @brief Build the job category string
 *
 * This function builds the job category string for a job. The category string is
 * used to group jobs together for scheduling purposes. The function takes a
 * dstring object to store the category string and a job object to extract
 * information from. It also takes a lists to identify if a job indirectly
 * references certain configuration objects (e.g. resource quota sets).
 *
 * @param category_str   The target string, contains the category or nothing
 * @param job            The job for the category creating
 * @param acl_list       Global access list
 * @param prj_list       Project list
 * @param rqs_list       Resource quota set list
 */
void
ocs::Category::build_string(dstring *category_str, lListElem *job,
                            const lList *acl_list, const lList *prj_list, const lList *rqs_list) {
   DENTER(TOP_LAYER);

   // owner (user, UNIX group, and ACLs)
   const char *owner = lGetString(job, JB_owner);
   const char *group = lGetString(job, JB_group);
   const lList *grp_list = lGetList(job, JB_grp_list);
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
   sge_unparse_queue_list_dstring(category_str, job_get_queue_listRW(job, JRS_SCOPE_GLOBAL, true), "-scope global -hard -q");

   // -scope master -hard -q <queue_list>
   sge_unparse_queue_list_dstring(category_str, job_get_queue_listRW(job, JRS_SCOPE_MASTER, true), "-scope master -hard -q");

   // -scope slave -hard -q <queue_list>
   sge_unparse_queue_list_dstring(category_str, job_get_queue_listRW(job, JRS_SCOPE_SLAVE, true), "-scope slave -hard -q");


   // -scope global -hard -l <resource_list>
   sge_unparse_resource_list_dstring(category_str, job_get_resource_listRW(job, JRS_SCOPE_GLOBAL, true), "-scope global -hard -l");

   // -scope master -hard -l <resource_list>
   sge_unparse_resource_list_dstring(category_str, job_get_resource_listRW(job, JRS_SCOPE_MASTER, true), "-scope master -hard -l");

   // -scope slave -hard -l <resource_list>
   sge_unparse_resource_list_dstring(category_str, job_get_resource_listRW(job, JRS_SCOPE_SLAVE, true), "-scope slave -hard -l");

   // TODO: evaluate if soft requests should be part of the category string
#if 1
   // -scope global -soft -q <resource_list>
   sge_unparse_queue_list_dstring(category_str, job_get_queue_listRW(job, JRS_SCOPE_GLOBAL, false), "-scope global -soft -q");

   // -scope global -soft -l <resource_list>
   sge_unparse_resource_list_dstring(category_str, job_get_resource_listRW(job, JRS_SCOPE_GLOBAL, false), "-scope global -soft -l");
#endif

   // -btype -bunit -bamount -bsort -bstart -bend
   sge_unparse_binding_dstring(category_str, job, lGetPosViaElem(job, JB_new_binding, SGE_NO_ABORT));

   // -pe pe_name pe_range
   sge_unparse_pe_dstring(category_str, job, lGetPosViaElem(job, JB_pe, SGE_NO_ABORT), lGetPosViaElem(job, JB_pe_range, SGE_NO_ABORT), "-pe");

   // -ckpt ckpt_name
   sge_unparse_string_option_dstring(category_str, job, lGetPosViaElem(job, JB_checkpoint_name, SGE_NO_ABORT), "-ckpt");

   // interactive job type
   if (JOB_TYPE_IS_IMMEDIATE(lGetPosUlong(job, lGetPosViaElem(job, JB_type, SGE_NO_ABORT)))) {
      sge_dstring_append(category_str, "-I y ");
   }

   // -P project
   int project_nm = lGetPosViaElem(job, JB_project, SGE_NO_ABORT);
   const char *project = lGetPosString(job, project_nm);

   if (project != nullptr) {
      const lListElem *prj = lGetElemStr(prj_list, PR_name, project);

      if (prj != nullptr && lGetBool(prj, PR_consider_with_categories)) {
         sge_unparse_string_option_dstring(category_str, job, project_nm, "-P");
      }
   }

   // -ar ar_id
   sge_unparse_ulong_option_dstring(category_str, job, lGetPosViaElem(job, JB_ar, SGE_NO_ABORT), "-ar");

   // remove the last white space that the last unparse function has written
   sge_dstring_strip_white_space_at_eol(category_str);

   // avoid null pointer as category string in case job has no specific requests
   if (sge_dstring_get_string(category_str) == nullptr) {
      sge_dstring_append(category_str, "-");
   }

   DRETURN_VOID;
}
