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
 * @brief Declarations for the share tree object
 *
 * @see sge_sharetree.cc
 */

#include "sgeobj/cull/sge_sharetree_STN_L.h"

/**
 * @brief The position of each share tree node attribute within `STN_Type`
 *
 * Reading an attribute by position skips the lookup by name, which matters in
 * the share tree because the scheduler walks every node on every run.
 *
 * @warning Must stay in sync with `libs/sgeobj/json/STN.json`, which is what
 *          the attribute order is generated from. A value inserted there and
 *          not here makes every position below it read the wrong attribute.
 */
enum {
   STN_name_POS = 0,                          ///< position of `STN_name`
   STN_type_POS,                           ///< position of `STN_type`
   STN_id_POS,                             ///< position of `STN_id`
   STN_shares_POS,                         ///< position of `STN_shares`
   STN_children_POS,                       ///< position of `STN_children`
   STN_job_ref_count_POS,                  ///< position of `STN_job_ref_count`
   STN_active_job_ref_count_POS,           ///< position of `STN_active_job_ref_count`
   STN_project_POS,                        ///< position of `STN_project`
   STN_proportion_POS,                     ///< position of `STN_proportion`
   STN_adjusted_proportion_POS,            ///< position of `STN_adjusted_proportion`
   STN_combined_usage_POS,                 ///< position of `STN_combined_usage`
   STN_pass2_seqno_POS,                    ///< position of `STN_pass2_seqno`
   STN_sum_priority_POS,                   ///< position of `STN_sum_priority`
   STN_actual_proportion_POS,              ///< position of `STN_actual_proportion`
   STN_m_share_POS,                        ///< position of `STN_m_share`
   STN_last_actual_proportion_POS,         ///< position of `STN_last_actual_proportion`
   STN_adjusted_current_proportion_POS,    ///< position of `STN_adjusted_current_proportion`
   STN_temp_POS,                           ///< position of `STN_temp`
   STN_stt_POS,                            ///< position of `STN_stt`
   STN_ostt_POS,                           ///< position of `STN_ostt`
   STN_ltt_POS,                            ///< position of `STN_ltt`
   STN_oltt_POS,                           ///< position of `STN_oltt`
   STN_shr_POS,                            ///< position of `STN_shr`
   STN_sort_POS,                           ///< position of `STN_sort`
   STN_ref_POS,                            ///< position of `STN_ref`
   STN_tickets_POS,                        ///< position of `STN_tickets`
   STN_jobid_POS,                          ///< position of `STN_jobid`
   STN_taskid_POS,                         ///< position of `STN_taskid`
   STN_usage_list_POS,                     ///< position of `STN_usage_list`
   STN_version_POS                         ///< position of `STN_version`
};

/**
 * @name Share tree node types
 *
 * A share tree node is either a user or a project; `STN_type` says which.
 * @{
 */
#define STT_USER    0 ///< the node stands for a user
#define STT_PROJECT 1 ///< the node stands for a project
/** @} */

/// The path from the share tree root down to one node
typedef struct {
   int depth;           ///< how many entries `nodes` has
   lListElem **nodes;   ///< the nodes, root first
} ancestors_t;

bool id_sharetree(lList **alpp, lListElem *ep, int id, int *ret_id);
int show_sharetree_path(lListElem *root, const char *path);
int show_sharetree(const lListElem *ep, const char *indent);
lListElem *getSNTemplate();
lListElem *search_named_node ( lListElem *ep, const char *name );
lListElem *search_named_node_path ( lListElem *ep, const char *path, ancestors_t *ancestors );
void free_ancestors( ancestors_t *ancestors);
lListElem *sge_search_unspecified_node(lListElem *ep);
#ifdef notdef
lListElem *search_ancestor_list ( lListElem *ep, char *name, ancestors_t *ancestors );
#endif
lListElem *search_ancestors(lListElem *ep, const char *name,
                                   ancestors_t *ancestors, int depth);
