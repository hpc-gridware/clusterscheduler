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

/** @file
 * @brief Declarations and attribute positions of the user and project objects
 *
 * @see sge_userprj.cc
 */

#include "sgeobj/cull/sge_userprj_PR_L.h"
#include "sgeobj/cull/sge_userprj_UU_L.h"
#include "sgeobj/cull/sge_userprj_UPU_L.h"
#include "sgeobj/cull/sge_userprj_UPP_L.h"

/**
 * @brief The position of each attribute within the object
 *
 * Reading by position skips the lookup by name, which pays off where the
 * same attribute is read for every element of a long list.
 *
 * @warning Must stay in sync with `libs/sgeobj/json/UU.json`, which is what
 *          the attribute order is generated from. A value inserted there and
 *          not here makes every position below it read the wrong attribute.
 */
enum {
   UU_name_POS = 0,                        ///< position of `UU_name`
   UU_oticket_POS,                         ///< position of `UU_oticket`
   UU_fshare_POS,                          ///< position of `UU_fshare`
   UU_delete_time_POS,                     ///< position of `UU_delete_time`
   UU_job_cnt_POS,                         ///< position of `UU_job_cnt`
   UU_pending_job_cnt_POS,                 ///< position of `UU_pending_job_cnt`
   UU_usage_POS,                           ///< position of `UU_usage`
   UU_usage_time_stamp_POS,                ///< position of `UU_usage_time_stamp`
   UU_usage_seqno_POS,                     ///< position of `UU_usage_seqno`
   UU_long_term_usage_POS,                 ///< position of `UU_long_term_usage`
   UU_project_POS,                         ///< position of `UU_project`
   UU_debited_job_usage_POS,               ///< position of `UU_debited_job_usage`
   UU_default_project_POS,                 ///< position of `UU_default_project`
   UU_version_POS,                         ///< position of `UU_version`
   UU_consider_with_categories_POS         ///< position of `UU_consider_with_categories`
};

/**
 * @brief The position of each attribute within the object
 *
 * Reading by position skips the lookup by name, which pays off where the
 * same attribute is read for every element of a long list.
 *
 * @warning Must stay in sync with `libs/sgeobj/json/PR.json`, which is what
 *          the attribute order is generated from. A value inserted there and
 *          not here makes every position below it read the wrong attribute.
 */
enum {
   PR_name_POS = 0,                        ///< position of `PR_name`
   PR_oticket_POS,                         ///< position of `PR_oticket`
   PR_fshare_POS,                          ///< position of `PR_fshare`
   PR_job_cnt_POS,                         ///< position of `PR_job_cnt`
   PR_pending_job_cnt_POS,                 ///< position of `PR_pending_job_cnt`
   PR_usage_POS,                           ///< position of `PR_usage`
   PR_usage_time_stamp_POS,                ///< position of `PR_usage_time_stamp`
   PR_usage_seqno_POS,                     ///< position of `PR_usage_seqno`
   PR_long_term_usage_POS,                 ///< position of `PR_long_term_usage`
   PR_project_POS,                         ///< position of `PR_project`
   PR_acl_POS,                             ///< position of `PR_acl`
   PR_xacl_POS,                            ///< position of `PR_xacl`
   PR_debited_job_usage_POS,               ///< position of `PR_debited_job_usage`
   PR_version_POS,                         ///< position of `PR_version`
   PR_consider_with_categories_POS         ///< position of `PR_consider_with_categories`
};

/**
 * @brief The position of each attribute within the object
 *
 * Reading by position skips the lookup by name, which pays off where the
 * same attribute is read for every element of a long list.
 *
 * @warning Must stay in sync with `libs/sgeobj/json/UPP.json`, which is what
 *          the attribute order is generated from. A value inserted there and
 *          not here makes every position below it read the wrong attribute.
 */
enum {
   UPP_name_POS = 0,                       ///< position of `UPP_name`
   UPP_usage_POS,                          ///< position of `UPP_usage`
   UPP_long_term_usage_POS                 ///< position of `UPP_long_term_usage`
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
