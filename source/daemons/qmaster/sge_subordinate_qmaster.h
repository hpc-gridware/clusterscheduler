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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Subordinate queues, including the slot-wise suspension tree
 */

#include "uti/sge_monitor.h"
#include "sgeobj/sge_daemonize.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/sge_answer.h"

bool
cqueue_list_x_on_subordinate_gdil(ocs::tom::List<ocs::tom::CQ> master_cqueue_list, bool suspend,
                                  ocs::tom::ConstList<ocs::tom::JG> gdil, monitoring_t *monitor, uint64_t gdi_session);

bool
cqueue_list_x_on_subordinate_so(ocs::tom::List<ocs::tom::CQ> master_cqueue_list,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bool suspend,
                                ocs::tom::ConstList<ocs::tom::SO> resolved_so_list, monitoring_t *monitor,
                                uint64_t gdi_session);

[[nodiscard]] ocs::tom::ListPtr<ocs::tom::SO>
qinstance_find_suspended_subordinates(ocs::tom::ConstElement<ocs::tom::QU> this_elem,
                                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                      ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list);

bool
qinstance_initialize_sos_attr(ocs::tom::Element<ocs::tom::QU> this_elem, monitoring_t *monitor,
                              ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list, uint64_t gdi_session);

bool
do_slotwise_x_on_subordinate_check(ocs::tom::Element<ocs::tom::QU> queue_instance, bool suspend,
                                   bool check_subtree_only, monitoring_t *monitor);

void
unsuspend_all_tasks_in_slotwise_sub_tree(ocs::tom::Element<ocs::tom::QU> qinstance, monitoring_t *monitor);

bool
check_new_slotwise_subordinate_tree(ocs::tom::Element<ocs::tom::QU> qinstance,
                                    ocs::tom::ConstList<ocs::tom::SO> new_so_list,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool do_slotwise_subordinate_lists_differ(ocs::tom::ConstList<ocs::tom::SO> old_so_list,
                                          ocs::tom::ConstList<ocs::tom::SO> new_so_list);
