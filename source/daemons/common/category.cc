/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_ja_task.h"

#include "sched/sge_resource_quota_schedd.h"

#if 0 /* TODO: EB: ST: should this be enabled again? */

/* struct containing the cull field position of the job target structures
   and the reduced order elements */
typedef struct {
   int JB_hard_queue_list_pos;
   int JB_master_hard_queue_list_pos;
   int JB_hard_resource_list_pos;
   int JB_soft_resource_list_pos;
   int JB_checkpoint_name_pos;
   int JB_type_pos;
   int JB_owner_pos;
   int JB_group_pos;
   int JB_project_pos;
   int JB_pe_pos;
   int JB_range_pos;
   int JB_ar_pos;
} order_pos_t;

typedef struct {
   pthread_mutex_t cull_order_mutex; /* gards the last_update access */
   order_pos_t cull_order_pos;        /* stores cull positions in the job, ja-task, and order structure */
} sge_category_t;

static sge_category_t Category_Control = {PTHREAD_MUTEX_INITIALIZER, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};

#endif

/*-------------------------------------------------------------------------*/
/* build the category string                                               */
/* the category string includes now the soft requests                      */
/*-------------------------------------------------------------------------*/
/****** category/sge_build_job_category_dstring() ******************************
*  NAME
*     sge_build_job_category_dstring() -- build the category string   
*
*  SYNOPSIS
*     void sge_build_job_category_dstring(dstring *category_str, lListElem 
*     *job, lList *acl_list) 
*
*  FUNCTION
*     The following parameter are put into the category:
*        hard_queue_list
*        master_hard_queue_list
*        hard_resource_list
*        soft_resource_list
*        checkpoint_name
*        type
*
*        owner/group: -U user_lists 
*           Omitted, if user_lists/xuser_lists were not used in
*           host_conf(5), sge_pe(5) and queue_conf(5). In sge_conf(5) 
*           user_lists/xuser_lists still can be used, as it causes
*           jobs already be rejected at submit time.
*
*        project: -P user_lists 
*           Omitted, if projects/xprojects were not used in
*           host_conf(5), sge_pe(5) and queue_conf(5). In sge_conf(5) 
*           projects/xprojects still can be used, as it cuases
*           jobs already be rejected at submit time.
*
*        pe
*
*  INPUTS
*     dstring *category_str - target string, contains the category or nothing
*     lListElem *job        - the job for the category creating
*     lList *acl_list       - global access list
*
*  NOTES
*     MT-NOTE: sge_build_job_category_dstring() is MT safe as long as the caller is
*
*******************************************************************************/
void sge_build_job_category_dstring(dstring *category_str, lListElem *job, const lList *acl_list, const lList *prj_list, bool *did_project, const lList *rqs_list) {
   DENTER(TOP_LAYER);

#if 0
   sge_mutex_lock("cull_order_mutex", __func__, __LINE__, &Category_Control.cull_order_mutex);
   if (Category_Control.cull_order_pos.JB_hard_queue_list_pos == -1) {
      Category_Control.cull_order_pos.JB_checkpoint_name_pos = lGetPosViaElem(job, JB_checkpoint_name, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_soft_resource_list_pos = lGetPosViaElem(job, JB_soft_resource_list, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_master_hard_queue_list_pos = lGetPosViaElem(job, JB_master_hard_queue_list, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_hard_queue_list_pos = lGetPosViaElem(job, JB_hard_queue_list, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_owner_pos = lGetPosViaElem(job, JB_owner, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_group_pos = lGetPosViaElem(job, JB_group, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_hard_resource_list_pos = lGetPosViaElem(job, JB_hard_resource_list, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_type_pos = lGetPosViaElem(job, JB_type, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_project_pos = lGetPosViaElem(job, JB_project, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_ar_pos = lGetPosViaElem(job, JB_ar, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_pe_pos = lGetPosViaElem(job, JB_pe, SGE_NO_ABORT);
      Category_Control.cull_order_pos.JB_range_pos = lGetPosViaElem(job, JB_pe_range, SGE_NO_ABORT);
   }
   sge_mutex_unlock("cull_order_mutex", __func__, __LINE__, &Category_Control.cull_order_mutex);
#endif

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

   // -pe pe_name pe_range
   sge_unparse_pe_dstring(category_str, job, lGetPosViaElem(job, JB_pe, SGE_NO_ABORT), lGetPosViaElem(job, JB_pe_range, SGE_NO_ABORT), "-pe");

   // -ckpt ckpt_name
   sge_unparse_string_option_dstring(category_str, job, lGetPosViaElem(job, JB_checkpoint_name, SGE_NO_ABORT), "-ckpt");

   // interactive job type
   if (JOB_TYPE_IS_IMMEDIATE(lGetPosUlong(job, lGetPosViaElem(job, JB_type, SGE_NO_ABORT)))) {
      sge_dstring_append(category_str, "-I y ");
   }

   // -P project
   {
      int project_nm = lGetPosViaElem(job, JB_project, SGE_NO_ABORT);
      const char *project = lGetPosString(job, project_nm);

      if (project != nullptr) {
         const lListElem *prj = lGetElemStr(prj_list, PR_name, project);

         if (prj != nullptr && lGetBool(prj, PR_consider_with_categories)) {
            if (did_project) {
               *did_project = true;
            }
            sge_unparse_string_option_dstring(category_str, job, project_nm, "-P");
         } else {
            if (did_project) {
               *did_project = false;
            }
         }
      }
   }

   // -ar ar_id
   sge_unparse_ulong_option_dstring(category_str, job, lGetPosViaElem(job, JB_ar, SGE_NO_ABORT), "-ar");

   // remove the last white space that the last unparse function has written
   sge_dstring_strip_white_space_at_eol(category_str);

   DRETURN_VOID;
}
