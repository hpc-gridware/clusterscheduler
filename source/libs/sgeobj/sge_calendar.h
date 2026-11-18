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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations for the calendar object
 *
 * @see sge_calendar.cc
 */

#include <ctime>
#include <tuple>

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CQU.h"
#include "sgeobj/tom/ocs_tom_TM.h"

#include "sgeobj/cull/sge_calendar_CAL_L.h"
#include "sgeobj/cull/sge_calendar_CA_L.h"
#include "sgeobj/cull/sge_calendar_CQU_L.h"
#include "sgeobj/cull/sge_calendar_TMR_L.h"
#include "sgeobj/cull/sge_calendar_TM_L.h"
#include "sgeobj/sge_answer.h"

bool calendar_parse_year(ocs::tom::Element<ocs::tom::CAL> cal, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
OCS_TRANSITIONAL bool calendar_parse_year(lListElem *cal, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool calendar_parse_week(ocs::tom::Element<ocs::tom::CAL> cal, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
OCS_TRANSITIONAL bool calendar_parse_week(lListElem *cal, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

[[nodiscard]] std::tuple<uint32_t, uint64_t, ocs::tom::ListPtr<ocs::tom::CQU>>
calender_state_changes(ocs::tom::ConstElement<ocs::tom::CAL> cep, const uint64_t *now64);
OCS_TRANSITIONAL uint32_t calender_state_changes(const lListElem *cep, lList **state_changes_list, uint64_t *when64,
                                                 uint64_t *now64);

bool calendar_is_referenced(ocs::tom::ConstElement<ocs::tom::CAL> calendar,
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list);
OCS_TRANSITIONAL bool calendar_is_referenced(const lListElem *calendar, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                             const lList *master_cqueue_list);

[[nodiscard]] ocs::tom::ElementPtr<ocs::tom::CAL> sge_generic_cal(const char *cal_name);

void cullify_tm(ocs::tom::Element<ocs::tom::TM> tm_ep, const struct tm *tm_now);
void uncullify_tm(ocs::tom::ConstElement<ocs::tom::TM> tm_ep, struct tm *tm_now);

bool calendar_open_in_time_frame(ocs::tom::ConstElement<ocs::tom::CAL> cep, uint64_t start_time, uint64_t duration);
OCS_TRANSITIONAL bool calendar_open_in_time_frame(const lListElem *cep, uint64_t start_time, uint64_t duration);
