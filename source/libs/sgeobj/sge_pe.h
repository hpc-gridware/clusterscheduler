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
 * @brief Declarations for parallel environments
 *
 * @see sge_pe.cc
 */

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "sgeobj/sge_answer.h"

bool
pe_name_is_matching(const char *pe_name, const char *wildcard);

bool
pe_is_matching(ocs::tom::ConstElement<ocs::tom::PE> pe, const char *wildcard);

ocs::tom::ConstElement<ocs::tom::PE>
pe_list_find_matching(ocs::tom::ConstList<ocs::tom::PE> pe_list, const char *wildcard);

bool
pe_is_referenced(ocs::tom::ConstElement<ocs::tom::PE> pe, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                 ocs::tom::ConstList<ocs::tom::JB> master_job_list,
                 ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list);

int
pe_validate(ocs::tom::Element<ocs::tom::PE> pep, ocs::tom::ListPtr<ocs::tom::AN> *alpp, int startup,
            ocs::tom::ConstList<ocs::tom::US> master_userset_list);

int
pe_validate_slots(ocs::tom::ListPtr<ocs::tom::AN> *alpp, uint32_t slots, bool warn_only);

int
pe_validate_urgency_slots(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *s);

bool
pe_validate_allocation_rule(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *allocation_rule,
                            bool output_errors = false);

int
pe_urgency_slots(ocs::tom::ConstElement<ocs::tom::PE> pe, const char *urgency_slot_setting,
                 ocs::tom::ConstList<ocs::tom::RN> range_list);

bool
pe_list_do_all_exist(ocs::tom::ConstList<ocs::tom::PE> pe_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                     ocs::tom::ConstList<ocs::tom::ST> pe_ref_list, bool ignore_make_pe);

ocs::tom::ElementPtr<ocs::tom::PE>
pe_create_template(const char *pe_name);

int
pe_get_slots_used(ocs::tom::ConstElement<ocs::tom::PE> pe);

int
pe_set_slots_used(ocs::tom::Element<ocs::tom::PE> pe, int slots);

void
pe_debit_slots(ocs::tom::Element<ocs::tom::PE> pep, int slots, uint32_t job_id);

bool
pe_do_accounting_summary(ocs::tom::ConstElement<ocs::tom::PE> pe);
