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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The resource utilization diagram - what is booked, and when
 *
 * A consumable is not just "so much free right now". The scheduler keeps, per
 * resource and per object, a **diagram** over time: a list of points at which
 * the booked amount changes, so it can answer "how much of this is free
 * between t and t+d" - which is what resource reservation and advance
 * reservations need.
 *
 * utilization_add() books an interval into a diagram, utilization_max()
 * answers the maximum booking within an interval, and
 * prepare_resource_schedules() fills the diagrams of a whole scheduling run
 * from the running and suspended jobs.
 */

#include "sge_select_queue.h"
#include "sgeobj/tom/ocs_tom_RDE.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/* those are for treating resource utilization */
bool utilization_print_to_dstring(ocs::tom::ConstElement<ocs::tom::RUE> this_elem, dstring *string);

int
utilization_add(ocs::tom::Element<ocs::tom::RUE> cr, uint64_t start_time, uint64_t duration, double utilization,
                uint32_t job_id, uint32_t ja_taskid, uint32_t level, const char *object_name,
                const char *type, bool for_job, bool implicit_non_exclusive,
                ocs::tom::ConstList<ocs::tom::ST> binding_touse, ocs::tom::ConstList<ocs::tom::RESL> rsmap_inuse);

ocs::tom::ListPtr<ocs::tom::RESL> utilization_rsmap_max(ocs::tom::ConstElement<ocs::tom::RUE> cr, uint64_t now,
                                                        uint64_t start_time, uint64_t duration);

uint64_t
utilization_rsmap_below(ocs::tom::ConstElement<ocs::tom::CE> definition, ocs::tom::ConstElement<ocs::tom::RUE> cr,
                        const char *key_name, uint32_t amount, const char *id_expr = nullptr,
                        ocs::tom::ConstList<ocs::tom::CE> required_props = ocs::tom::ConstList<ocs::tom::CE>());

double utilization_max(const sge_assignment_t *a, ocs::tom::ConstElement<ocs::tom::EH> host,
                       ocs::tom::ConstElement<ocs::tom::RUE> cr, uint64_t start_time, uint64_t duration, double total,
                       double request, double slots, bool for_excl_request, ocs::TopologyString& binding_inuse);

uint64_t
utilization_below(const sge_assignment_t *a, ocs::tom::ConstElement<ocs::tom::EH> host,
                  ocs::tom::ConstElement<ocs::tom::RUE> cr, double max_util, double total, double slots,
                  const char *object_name, bool for_excl_request, ocs::TopologyString& combined_binding_inuse);

int add_job_utilization(const sge_assignment_t *a, const char *type, bool for_job_scheduling);

int rc_add_job_utilization(ocs::tom::ConstElement<ocs::tom::JG> gdil, ocs::tom::ConstElement<ocs::tom::JB> jep,
                           ocs::tom::ConstElement<ocs::tom::PE> pe, uint32_t task_id, const char *type,
                           ocs::tom::Element<ocs::tom::EH> host, ocs::tom::ConstList<ocs::tom::CE> centry_list,
                           int slots, const char *obj_name, uint64_t start_time, uint64_t duration, uint32_t tag,
                           bool for_job_scheduling, bool is_master_task, bool do_per_host_booking,
                           ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list);
int rc_add_job_utilization(ocs::tom::ConstElement<ocs::tom::JG> gdil, ocs::tom::ConstElement<ocs::tom::JB> jep,
                           ocs::tom::ConstElement<ocs::tom::PE> pe, uint32_t task_id, const char *type,
                           ocs::tom::Element<ocs::tom::QU> queue, ocs::tom::ConstList<ocs::tom::CE> centry_list,
                           int slots, const char *obj_name, uint64_t start_time, uint64_t duration, uint32_t tag,
                           bool for_job_scheduling, bool is_master_task, bool do_per_host_booking,
                           ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list);

void prepare_resource_schedules(ocs::tom::List<ocs::tom::JB> running_jobs, ocs::tom::List<ocs::tom::JB> suspended_jobs,
                                ocs::tom::List<ocs::tom::PE> pe_list, ocs::tom::List<ocs::tom::EH> host_list,
                                ocs::tom::List<ocs::tom::QU> queue_list, ocs::tom::List<ocs::tom::RQS> rqs_list,
                                ocs::tom::List<ocs::tom::CE> centry_list,
                                ocs::tom::ConstList<ocs::tom::US> acl_list,
                                ocs::tom::ConstList<ocs::tom::HGRP> hgroup_list, ocs::tom::List<ocs::tom::AR> ar_list,
                                bool for_job_scheduling, uint64_t now);

int qinstance_slots_reserved_now(ocs::tom::ConstElement<ocs::tom::QU> this_elem);
