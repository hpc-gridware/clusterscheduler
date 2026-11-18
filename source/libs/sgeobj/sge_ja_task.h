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
 * @brief Declarations for array tasks: one task of a job array
 *
 * @see sge_ja_task.cc
 */

#include <utility>

#include "uti/sge_dstring.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_RN.h"

#include "sgeobj/cull/sge_ja_task_JAT_L.h"
#include "sgeobj/sge_answer.h"

ocs::tom::Element<ocs::tom::PET> ja_task_search_pe_task(ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                                        const char *pe_task_id);

void ja_task_set_deleted_by(ocs::tom::Element<ocs::tom::JAT> ja_task, const char *deleted_by);
OCS_TRANSITIONAL void ja_task_set_deleted_by(lListElem *ja_task, const char *deleted_by);

const char *ja_task_get_deleted_by(ocs::tom::ConstElement<ocs::tom::JAT> ja_task);
OCS_TRANSITIONAL const char *ja_task_get_deleted_by(const lListElem *ja_task);

void ja_task_list_print_to_string(ocs::tom::ConstList<ocs::tom::JAT> ja_task_list, dstring *range_string);

[[nodiscard]] ocs::tom::ListPtr<ocs::tom::JAT>
ja_task_list_split_group(ocs::tom::ListPtr<ocs::tom::JAT> &ja_task_list);

bool ja_task_add_finished_pe_task(ocs::tom::Element<ocs::tom::JAT> ja_task, const char *pe_task_id);

bool ja_task_clear_finished_pe_tasks(ocs::tom::Element<ocs::tom::JAT> ja_task);
OCS_TRANSITIONAL bool ja_task_clear_finished_pe_tasks(lListElem *ja_task);

[[nodiscard]] std::pair<int, ocs::tom::Element<ocs::tom::ID>>
sge_parse_jobtasks(lList **lp, const char *str, ocs::tom::ListPtr<ocs::tom::AN> *alpp, bool include_names,
                   ocs::tom::ConstList<ocs::tom::RN> arrayDefList);

bool ja_task_message_add(ocs::tom::Element<ocs::tom::JAT> this_elem, uint32_t type, const char *message);
OCS_TRANSITIONAL bool ja_task_message_add(lListElem *this_elem, uint32_t type, const char *message);

bool ja_task_message_trash_all_of_type_X(ocs::tom::Element<ocs::tom::JAT> this_elem, uint32_t type);
OCS_TRANSITIONAL bool ja_task_message_trash_all_of_type_X(lListElem *this_elem, uint32_t type);

bool ja_task_verify(ocs::tom::ConstElement<ocs::tom::JAT> ja_task, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool ja_task_verify_execd_job(ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool ja_task_verify_granted_destin_identifier_list(ocs::tom::ConstList<ocs::tom::JG> gdil,
                                                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool ja_task_verify_granted_destin_identifier(ocs::tom::ConstElement<ocs::tom::JG> ep,
                                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool ja_task_is_tightly_integrated(ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                   ocs::tom::ConstList<ocs::tom::PE> master_pe_list);
OCS_TRANSITIONAL bool ja_task_is_tightly_integrated(const lListElem *ja_task, const lList *master_pe_list);

int ja_task_debit_host_rsmaps(ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list,
                              ocs::tom::Element<ocs::tom::EH> host, int slots, bool *just_check);

int ja_task_debit_host_bindings(ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list,
                                ocs::tom::Element<ocs::tom::EH> host, int slots, bool *just_check);

bool ja_task_is_running(ocs::tom::ConstElement<ocs::tom::JAT> ja_task);
OCS_TRANSITIONAL bool ja_task_is_running(const lListElem *ja_task);
