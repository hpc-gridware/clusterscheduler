#pragma once
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

#include "sgeobj/cull/sge_userprj_PR_L.h"
#include "sgeobj/cull/sge_userprj_UU_L.h"
#include "sgeobj/cull/sge_userprj_UPU_L.h"
#include "sgeobj/cull/sge_userprj_UPP_L.h"

// make sure that the following enum is in sync with libs/sgeobj/json/UU.json
enum {
   UU_name_POS = 0,
   UU_oticket_POS,
   UU_fshare_POS,
   UU_delete_time_POS,
   UU_job_cnt_POS,
   UU_pending_job_cnt_POS,
   UU_usage_POS,
   UU_usage_time_stamp_POS,
   UU_usage_seqno_POS,
   UU_long_term_usage_POS,
   UU_project_POS,
   UU_debited_job_usage_POS,
   UU_default_project_POS,
   UU_version_POS,
   UU_consider_with_categories_POS
};

// make sure that the following enum is in sync with libs/sgeobj/json/PR.json
enum {
   PR_name_POS = 0,
   PR_oticket_POS,
   PR_fshare_POS,
   PR_job_cnt_POS,
   PR_pending_job_cnt_POS,
   PR_usage_POS,
   PR_usage_time_stamp_POS,
   PR_usage_seqno_POS,
   PR_long_term_usage_POS,
   PR_project_POS,
   PR_acl_POS,
   PR_xacl_POS,
   PR_debited_job_usage_POS,
   PR_version_POS,
   PR_consider_with_categories_POS
};

// make sure that the following enum is in sync with libs/sgeobj/json/UPP.json
enum {
   UPP_name_POS = 0,
   UPP_usage_POS,
   UPP_long_term_usage_POS
};

lListElem *prj_list_locate(const lList *prj_list,
                           const char *prj_name);

lListElem *user_list_locate(const lList *user_list,
                            const char *user_name);

const char *prj_list_append_to_dstring(const lList *this_list, dstring *string);

bool prj_list_do_all_exist(const lList *this_list, lList **answer_list,
                           const lList *userprj_list);

lListElem *getUserTemplate();
lListElem *getPrjTemplate();
