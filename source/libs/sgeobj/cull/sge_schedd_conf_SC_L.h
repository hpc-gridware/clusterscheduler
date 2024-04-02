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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(SC_algorithm) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SC_schedule_interval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_maxujobs) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_queue_sort_method) - @todo add summary
*    @todo add description
*
*    SGE_LIST(SC_job_load_adjustments) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SC_load_adjustment_decay_time) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SC_load_formula) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SC_schedd_job_info) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_flush_submit_sec) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_flush_finish_sec) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SC_params) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SC_reprioritize_interval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_halftime) - @todo add summary
*    @todo add description
*
*    SGE_LIST(SC_usage_weight_list) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_compensation_factor) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_weight_user) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_weight_project) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_weight_department) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_weight_job) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_weight_tickets_functional) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_weight_tickets_share) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_weight_tickets_override) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(SC_share_override_tickets) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(SC_share_functional_shares) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_max_functional_jobs_to_schedule) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(SC_report_pjob_tickets) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_max_pending_tasks_per_job) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SC_halflife_decay_list) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SC_policy_hierarchy) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_weight_ticket) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_weight_waiting_time) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_weight_deadline) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_weight_urgency) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(SC_weight_priority) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SC_max_reservation) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SC_default_duration) - @todo add summary
*    @todo add description
*
*/

enum {
   SC_algorithm = SC_LOWERBOUND,
   SC_schedule_interval,
   SC_maxujobs,
   SC_queue_sort_method,
   SC_job_load_adjustments,
   SC_load_adjustment_decay_time,
   SC_load_formula,
   SC_schedd_job_info,
   SC_flush_submit_sec,
   SC_flush_finish_sec,
   SC_params,
   SC_reprioritize_interval,
   SC_halftime,
   SC_usage_weight_list,
   SC_compensation_factor,
   SC_weight_user,
   SC_weight_project,
   SC_weight_department,
   SC_weight_job,
   SC_weight_tickets_functional,
   SC_weight_tickets_share,
   SC_weight_tickets_override,
   SC_share_override_tickets,
   SC_share_functional_shares,
   SC_max_functional_jobs_to_schedule,
   SC_report_pjob_tickets,
   SC_max_pending_tasks_per_job,
   SC_halflife_decay_list,
   SC_policy_hierarchy,
   SC_weight_ticket,
   SC_weight_waiting_time,
   SC_weight_deadline,
   SC_weight_urgency,
   SC_weight_priority,
   SC_max_reservation,
   SC_default_duration
};

LISTDEF(SC_Type)
   SGE_STRING(SC_algorithm, CULL_SPOOL)
   SGE_STRING(SC_schedule_interval, CULL_SPOOL)
   SGE_ULONG(SC_maxujobs, CULL_SPOOL)
   SGE_ULONG(SC_queue_sort_method, CULL_SPOOL)
   SGE_LIST(SC_job_load_adjustments, CE_Type, CULL_SPOOL)
   SGE_STRING(SC_load_adjustment_decay_time, CULL_SPOOL)
   SGE_STRING(SC_load_formula, CULL_SPOOL)
   SGE_STRING(SC_schedd_job_info, CULL_SPOOL)
   SGE_ULONG(SC_flush_submit_sec, CULL_SPOOL)
   SGE_ULONG(SC_flush_finish_sec, CULL_SPOOL)
   SGE_STRING(SC_params, CULL_SPOOL)
   SGE_STRING(SC_reprioritize_interval, CULL_SPOOL)
   SGE_ULONG(SC_halftime, CULL_SPOOL)
   SGE_LIST(SC_usage_weight_list, UA_Type, CULL_SPOOL)
   SGE_DOUBLE(SC_compensation_factor, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_user, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_project, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_department, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_job, CULL_SPOOL)
   SGE_ULONG(SC_weight_tickets_functional, CULL_SPOOL)
   SGE_ULONG(SC_weight_tickets_share, CULL_SPOOL)
   SGE_ULONG(SC_weight_tickets_override, CULL_SPOOL)
   SGE_BOOL(SC_share_override_tickets, CULL_SPOOL)
   SGE_BOOL(SC_share_functional_shares, CULL_SPOOL)
   SGE_ULONG(SC_max_functional_jobs_to_schedule, CULL_SPOOL)
   SGE_BOOL(SC_report_pjob_tickets, CULL_SPOOL)
   SGE_ULONG(SC_max_pending_tasks_per_job, CULL_SPOOL)
   SGE_STRING(SC_halflife_decay_list, CULL_SPOOL)
   SGE_STRING(SC_policy_hierarchy, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_ticket, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_waiting_time, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_deadline, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_urgency, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_priority, CULL_SPOOL)
   SGE_ULONG(SC_max_reservation, CULL_SPOOL)
   SGE_STRING(SC_default_duration, CULL_SPOOL)
LISTEND

NAMEDEF(SCN)
   NAME("SC_algorithm")
   NAME("SC_schedule_interval")
   NAME("SC_maxujobs")
   NAME("SC_queue_sort_method")
   NAME("SC_job_load_adjustments")
   NAME("SC_load_adjustment_decay_time")
   NAME("SC_load_formula")
   NAME("SC_schedd_job_info")
   NAME("SC_flush_submit_sec")
   NAME("SC_flush_finish_sec")
   NAME("SC_params")
   NAME("SC_reprioritize_interval")
   NAME("SC_halftime")
   NAME("SC_usage_weight_list")
   NAME("SC_compensation_factor")
   NAME("SC_weight_user")
   NAME("SC_weight_project")
   NAME("SC_weight_department")
   NAME("SC_weight_job")
   NAME("SC_weight_tickets_functional")
   NAME("SC_weight_tickets_share")
   NAME("SC_weight_tickets_override")
   NAME("SC_share_override_tickets")
   NAME("SC_share_functional_shares")
   NAME("SC_max_functional_jobs_to_schedule")
   NAME("SC_report_pjob_tickets")
   NAME("SC_max_pending_tasks_per_job")
   NAME("SC_halflife_decay_list")
   NAME("SC_policy_hierarchy")
   NAME("SC_weight_ticket")
   NAME("SC_weight_waiting_time")
   NAME("SC_weight_deadline")
   NAME("SC_weight_urgency")
   NAME("SC_weight_priority")
   NAME("SC_max_reservation")
   NAME("SC_default_duration")
NAMEEND

#define SC_SIZE sizeof(SCN)/sizeof(char *)


