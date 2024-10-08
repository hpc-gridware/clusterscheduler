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

#include "sge_select_queue.h"
#include "sgeobj/cull/sge_resource_utilization_RDE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"

/* those are for treating resource utilization */
bool utilization_print_to_dstring(const lListElem *this_elem, dstring *string);
void utilization_print(const lListElem *cr, const char *object_name);
int utilization_add(lListElem *cr, u_long64 start_time, u_long64 duration, double utilization,
   u_long32 job_id, u_long32 ja_taskid, u_long32 level, const char *object_name, const char *type, bool for_job, bool implicit_non_exclusive);
double utilization_max(const lListElem *cr, u_long64 start_time, u_long64 duration, bool for_excl_request);
u_long64 utilization_below(const lListElem *cr, double max_util, const char *object_name, bool for_excl_request);

int add_job_utilization(const sge_assignment_t *a, const char *type, bool for_job_scheduling);
double utilization_queue_end(const lListElem *cr, bool for_excl_request);

int rc_add_job_utilization(lListElem *jep, const lListElem *pe, u_long32 task_id, const char *type, lListElem *ep,
                           const lList *centry_list, int slots, int config_nm, int actual_nm, const char *obj_name,
                           u_long64 start_time, u_long64 duration, u_long32 tag, bool for_job_scheduling,
                           bool is_master_task, bool do_per_host_booking);

void prepare_resource_schedules(const lList *running_jobs,
      const lList *suspended_jobs, lList *pe_list, lList *host_list,
      lList *queue_list, lList *rqs_list, const lList *centry_list, const lList *acl_list,
      const lList *hgroup_list, lList *ar_list, bool for_job_scheduling, u_long64 now);
