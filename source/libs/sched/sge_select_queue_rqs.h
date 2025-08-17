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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sge_select_queue.h"

/* parallel assignments */
dispatch_t
parallel_rqs_slots_by_time(sge_assignment_t *a, int *slots, lListElem *qep, bool need_master,
                           bool is_master_queue);

void parallel_check_and_debit_rqs_slots(sge_assignment_t *a, const char *host, const char *queue,
      int *slots, dstring *rule_name, dstring *rue_name, dstring *limit_name);

void parallel_revert_rqs_slot_debitation(sge_assignment_t *a, const char *host, const char *queue,
      int slots, dstring *rule_name, dstring *rue_name, dstring *limit_name);

/* sequential assignments */
dispatch_t rqs_by_slots(sge_assignment_t *a, const char *queue, const char *host,
  u_long64 *tt_rqs_all, bool *is_global, dstring *rue_string, dstring *limit_name, dstring *rule_name, u_long64 tt_best);

void rqs_can_optimize(const lListElem *rule, bool *host, bool *queue, sge_assignment_t *a);

void rqs_expand_cqueues(const lListElem *rule, sge_assignment_t *a);
void rqs_expand_hosts(const lListElem *rule, sge_assignment_t *a);

bool cqueue_shadowed(const lListElem *rule, sge_assignment_t *a);
bool host_shadowed(const lListElem *rule, sge_assignment_t *a);

void rqs_excluded_hosts(const lListElem *rule, sge_assignment_t *a);
void rqs_excluded_cqueues(const lListElem *rule, sge_assignment_t *a);

bool cqueue_shadowed_by(const char *cqname, const lListElem *rule, sge_assignment_t *a);
bool host_shadowed_by(const char *host, const lListElem *rule, sge_assignment_t *a);

bool rqs_set_dynamical_limit(lListElem *limit, lListElem *global_host, lListElem *exec_host, const lList *centry);
