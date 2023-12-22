#ifndef __SGE_SHARETREE_H 
#define __SGE_SHARETREE_H 
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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sgeobj/cull/sge_sharetree_STN_L.h"

// make sure that the following enum is in sync with libs/sgeobj/json/STN.json
enum {
   STN_name_POS = 0,
   STN_type_POS,
   STN_id_POS,
   STN_shares_POS,
   STN_children_POS,
   STN_job_ref_count_POS,
   STN_active_job_ref_count_POS,
   STN_project_POS,
   STN_proportion_POS,
   STN_adjusted_proportion_POS,
   STN_combined_usage_POS,
   STN_pass2_seqno_POS,
   STN_sum_priority_POS,
   STN_actual_proportion_POS,
   STN_m_share_POS,
   STN_last_actual_proportion_POS,
   STN_adjusted_current_proportion_POS,
   STN_temp_POS,
   STN_stt_POS,
   STN_ostt_POS,
   STN_ltt_POS,
   STN_oltt_POS,
   STN_shr_POS,
   STN_sort_POS,
   STN_ref_POS,
   STN_tickets_POS,
   STN_jobid_POS,
   STN_taskid_POS,
   STN_usage_list_POS,
   STN_version_POS
};

/*
 *  * This is the list type we use to hold the 
 *   * nodes of a share tree.
 *    */
#define STT_USER    0
#define STT_PROJECT 1

typedef struct {
   int depth;
   lListElem **nodes;
} ancestors_t;

/************************************************************************
   id_sharetree - set the sharetree node id
************************************************************************/
bool id_sharetree(lList **alpp, lListElem *ep, int id, int *ret_id);
int show_sharetree_path(lListElem *root, const char *path);
int show_sharetree(const lListElem *ep, char *indent);
lListElem *getSNTemplate(void);
lListElem *search_named_node ( lListElem *ep, const char *name );
lListElem *search_named_node_path ( lListElem *ep, const char *path, ancestors_t *ancestors );
void free_ancestors( ancestors_t *ancestors);
lListElem *sge_search_unspecified_node(lListElem *ep);
#ifdef notdef
lListElem *search_ancestor_list ( lListElem *ep, char *name, ancestors_t *ancestors );
#endif
lListElem *search_ancestors(lListElem *ep, char *name,
                                   ancestors_t *ancestors, int depth);

#endif /* __SGE_SHARETREE_H */
