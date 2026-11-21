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
 * @brief Queue instances: the state a queue has on one host
 */

#include <cinttypes>
#include "tom/ocs_Types.h"
#include "sgeobj/sge_daemonize.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CQU.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/sge_answer.h"

bool
qinstance_change_state_on_command(ocs::tom::Element<ocs::tom::QU> this_elem,
                                  ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                  uint32_t transition, bool force_transition, const char *user, const char *host,
                                  bool is_operator, bool is_owner, monitoring_t *monitor, uint64_t gdi_session);

bool
qinstance_change_state_on_calendar(ocs::tom::Element<ocs::tom::QU> this_elem,
                                   ocs::tom::ConstElement<ocs::tom::CAL> calendar, monitoring_t *monitor,
                                   uint64_t gdi_session);

bool
qinstance_modify_attributes(ocs::tom::Element<ocs::tom::QU> this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            ocs::tom::ConstElement<ocs::tom::CQ> cqueue, ocs::tom::ConstRawElement reduced_elem,
                            bool all_attributes, bool is_create, bool initial_modify, bool *is_ambiguous,
                            bool *has_changed_conf_attr, bool *need_reinitialize, monitoring_t *monitor,
                            ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                            ocs::tom::List<ocs::tom::CQ> master_cqueue_list, uint64_t gdi_session);

bool
qinstance_change_state_on_calendar_all(const char *cal_name, uint32_t cal_order,
                                       ocs::tom::ConstList<ocs::tom::CQU> state_change_list, monitoring_t *monitor,
                                       uint64_t gdi_session);

bool
sge_qmaster_qinstance_state_set_manual_disabled(ocs::tom::Element<ocs::tom::QU> this_elem, bool set_state,
                                                uint64_t gdi_session);

bool
sge_qmaster_qinstance_state_set_manual_suspended(ocs::tom::Element<ocs::tom::QU> this_elem, bool set_state,
                                                 uint64_t gid_session);

bool
sge_qmaster_qinstance_state_set_unknown(ocs::tom::Element<ocs::tom::QU> this_elem, bool set_state,
                                        uint64_t gid_session);

bool
sge_qmaster_qinstance_state_set_error(ocs::tom::Element<ocs::tom::QU> this_elem, bool set_state, uint64_t gid_session);

bool
sge_qmaster_qinstance_state_set_susp_on_sub(ocs::tom::Element<ocs::tom::QU> this_elem, bool set_state,
                                            uint64_t gid_session);

bool
sge_qmaster_qinstance_state_set_cal_disabled(ocs::tom::Element<ocs::tom::QU> this_elem, bool set_state,
                                             uint64_t gid_session);

bool
sge_qmaster_qinstance_state_set_cal_suspended(ocs::tom::Element<ocs::tom::QU> this_elem, bool set_state,
                                              uint64_t gid_session);

bool
sge_qmaster_qinstance_state_set_orphaned(ocs::tom::Element<ocs::tom::QU> this_elem, bool set_state,
                                         uint64_t gid_session);

bool
sge_qmaster_qinstance_state_set_ambiguous(ocs::tom::Element<ocs::tom::QU> this_elem, bool set_state,
                                          uint64_t gid_session);

bool
sge_qmaster_qinstance_set_initial_state(ocs::tom::Element<ocs::tom::QU> this_elem, uint64_t gdi_session);

bool
qinstance_reinit_consumable_actual_list(ocs::tom::Element<ocs::tom::QU> this_elem,
                                        ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
