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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Share Tree Node
*
* Represents one node of the share tree.
*
*    SGE_STRING(STN_name) - Node Name
*    Node name (symbolic name or user or project name in the leafs)
*
*    SGE_ULONG(STN_type) - Node Type
*    Kind of tree: STT_USER or STT_PROJECT.
*    All nodes in a tree have the same type.
*
*    SGE_ULONG(STN_id) - Node Id
*    Only for editing and save/restore. May be different each time.
*
*    SGE_ULONG(STN_shares) - Shares
*    Configured shares for the node or subtree under a leaf node.
*
*    SGE_LIST(STN_children) - Children
*    Configured childrens of this node. Same type, it is a recursive tree structure.
*
*    SGE_ULONG(STN_job_ref_count) - Job Reference Count
*    Number of running and pending jobs referencing this node (only used in scheduler thread).
*
*    SGE_ULONG(STN_active_job_ref_count) - Active Job Reference Count
*    Number of running jobs referencing this node (only used in scheduler thread).
*
*    SGE_ULONG(STN_project) - Is Project
*    Set to 1 if this node is a project (only used in scheduler thread).
*
*    SGE_DOUBLE(STN_proportion) - Proportion
*    Share proportion (only used in scheduler thread).
*
*    SGE_DOUBLE(STN_adjusted_proportion) - Adjusted Proportion
*    Share adjusted proportion (only used in scheduler thread).
*
*    SGE_DOUBLE(STN_combined_usage) - Combined Usage
*    Combined usage (only used in scheduler thread).
*
*    SGE_ULONG(STN_pass2_seqno) - Pass Two Sequence Number
*    Sequence number for pass 2 of share calculation (only used in scheduler thread).
*
*    SGE_ULONG(STN_sum_priority) - Sum of Priorities
*    Sum of job priorities (only used in scheduler thread).
*
*    SGE_DOUBLE(STN_actual_proportion) - Actual Proportion
*    Long term actual proportion.
*
*    SGE_DOUBLE(STN_m_share) - M Share
*    Dynamic long term targetted proportion.
*
*    SGE_DOUBLE(STN_last_actual_proportion) - Last Actual Proportion
*    Short term actual proportion.
*
*    SGE_DOUBLE(STN_adjusted_current_proportion) - Adjusted Current Proportion
*    Short term targetted proportion.
*
*    SGE_ULONG(STN_temp) - Temporary Node
*    Temporary Node used in scheduler only.
*
*    SGE_DOUBLE(STN_stt) - Short Term Targeted Proportion
*    Short term targeted proportion of node as compared to sibling nodes.
*    Calculated during scheduling of pending of pending jobs.
*
*    SGE_DOUBLE(STN_ostt) - Overall Short Term Targeted Proportion
*    Overall short term targeted proportion of node as compared to all nodes.
*    Calculated during scheduling of pending of pending jobs.
*
*    SGE_DOUBLE(STN_ltt) - Long Term Targeted Proportion
*    Long term targeted proportion of node as compared to all nodes.
*    Calculated during scheduling of pending of pending jobs.
*
*    SGE_DOUBLE(STN_oltt) - Overall Long Term Targeted Proportion
*    Overall long term targeted proportion of node as compared to all nodes.
*    Calculated during scheduling of pending of pending jobs.
*
*    SGE_DOUBLE(STN_shr) - Share
*    Hierarchical calculated share node.
*    Calculated during scheduling of pending of pending jobs.
*
*    SGE_DOUBLE(STN_sort) - Sort Value
*    Value for sorting jobs attached to a node.
*    Calculated during scheduling of pending of pending jobs.
*
*    SGE_ULONG(STN_ref) - Reference
*    Temporary index reference back into the array of pending jobs.
*    Used during scheduling of pending of pending jobs.
*
*    SGE_DOUBLE(STN_tickets) - Tickets
*    Temporary storage of pending tickets from higher level policies.
*    Used during scheduling of pending of pending jobs.
*
*    SGE_ULONG(STN_jobid) - Job Id
*    Job number of a temporary job node.
*    Used during scheduling of pending of pending jobs.
*
*    SGE_ULONG(STN_taskid) - Task Id
*    Array task number of a temporary job node.
*    Used during scheduling of pending of pending jobs.
*
*    SGE_LIST(STN_usage_list) - Usage List
*    Node usage list.
*    Used during scheduling of pending of pending jobs.
*
*    SGE_ULONG(STN_version) - @todo add summary
*    Version of share tree.
*    Set/increased in qmaster when sharetree changes.
*    Skip scheduling decisions based on an older version of the sharetree.
*
*/

enum {
   STN_name = STN_LOWERBOUND,
   STN_type,
   STN_id,
   STN_shares,
   STN_children,
   STN_job_ref_count,
   STN_active_job_ref_count,
   STN_project,
   STN_proportion,
   STN_adjusted_proportion,
   STN_combined_usage,
   STN_pass2_seqno,
   STN_sum_priority,
   STN_actual_proportion,
   STN_m_share,
   STN_last_actual_proportion,
   STN_adjusted_current_proportion,
   STN_temp,
   STN_stt,
   STN_ostt,
   STN_ltt,
   STN_oltt,
   STN_shr,
   STN_sort,
   STN_ref,
   STN_tickets,
   STN_jobid,
   STN_taskid,
   STN_usage_list,
   STN_version
};

LISTDEF(STN_Type)
   SGE_STRING(STN_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_ULONG(STN_type, CULL_SPOOL)
   SGE_ULONG(STN_id, CULL_DEFAULT)
   SGE_ULONG(STN_shares, CULL_SPOOL)
   SGE_LIST(STN_children, STN_Type, CULL_SPOOL)
   SGE_ULONG(STN_job_ref_count, CULL_DEFAULT)
   SGE_ULONG(STN_active_job_ref_count, CULL_DEFAULT)
   SGE_ULONG(STN_project, CULL_DEFAULT)
   SGE_DOUBLE(STN_proportion, CULL_DEFAULT)
   SGE_DOUBLE(STN_adjusted_proportion, CULL_DEFAULT)
   SGE_DOUBLE(STN_combined_usage, CULL_DEFAULT)
   SGE_ULONG(STN_pass2_seqno, CULL_DEFAULT)
   SGE_ULONG(STN_sum_priority, CULL_DEFAULT)
   SGE_DOUBLE(STN_actual_proportion, CULL_DEFAULT)
   SGE_DOUBLE(STN_m_share, CULL_DEFAULT)
   SGE_DOUBLE(STN_last_actual_proportion, CULL_DEFAULT)
   SGE_DOUBLE(STN_adjusted_current_proportion, CULL_DEFAULT)
   SGE_ULONG(STN_temp, CULL_DEFAULT)
   SGE_DOUBLE(STN_stt, CULL_DEFAULT)
   SGE_DOUBLE(STN_ostt, CULL_DEFAULT)
   SGE_DOUBLE(STN_ltt, CULL_DEFAULT)
   SGE_DOUBLE(STN_oltt, CULL_DEFAULT)
   SGE_DOUBLE(STN_shr, CULL_DEFAULT)
   SGE_DOUBLE(STN_sort, CULL_DEFAULT)
   SGE_ULONG(STN_ref, CULL_DEFAULT)
   SGE_DOUBLE(STN_tickets, CULL_DEFAULT)
   SGE_ULONG(STN_jobid, CULL_DEFAULT)
   SGE_ULONG(STN_taskid, CULL_DEFAULT)
   SGE_LIST(STN_usage_list, UA_Type, CULL_DEFAULT)
   SGE_ULONG(STN_version, CULL_DEFAULT)
LISTEND

NAMEDEF(STNN)
   NAME("STN_name")
   NAME("STN_type")
   NAME("STN_id")
   NAME("STN_shares")
   NAME("STN_children")
   NAME("STN_job_ref_count")
   NAME("STN_active_job_ref_count")
   NAME("STN_project")
   NAME("STN_proportion")
   NAME("STN_adjusted_proportion")
   NAME("STN_combined_usage")
   NAME("STN_pass2_seqno")
   NAME("STN_sum_priority")
   NAME("STN_actual_proportion")
   NAME("STN_m_share")
   NAME("STN_last_actual_proportion")
   NAME("STN_adjusted_current_proportion")
   NAME("STN_temp")
   NAME("STN_stt")
   NAME("STN_ostt")
   NAME("STN_ltt")
   NAME("STN_oltt")
   NAME("STN_shr")
   NAME("STN_sort")
   NAME("STN_ref")
   NAME("STN_tickets")
   NAME("STN_jobid")
   NAME("STN_taskid")
   NAME("STN_usage_list")
   NAME("STN_version")
NAMEEND

#define STN_SIZE sizeof(STNN)/sizeof(char *)


