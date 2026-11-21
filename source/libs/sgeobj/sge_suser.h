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
 * @brief Declarations for submit users: the per user job counter behind `max_u_jobs`
 *
 * @see sge_suser.cc
 */
 
#include "tom/ocs_Types.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_SU.h"

#include "sgeobj/sge_answer.h"

ocs::tom::Element<ocs::tom::SU> suser_list_add(ocs::tom::List<ocs::tom::SU> suser_list,
                                               ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *suser_name);

ocs::tom::ConstElement<ocs::tom::SU> suser_list_find(ocs::tom::ConstList<ocs::tom::SU> suser_list,
                                                    const char *suser_name);

ocs::tom::Element<ocs::tom::SU> suser_list_find(ocs::tom::List<ocs::tom::SU> suser_list, const char *suser_name);

void suser_increase_job_counter(ocs::tom::Element<ocs::tom::SU> suser);

void suser_decrease_job_counter(ocs::tom::Element<ocs::tom::SU> suser);

uint32_t suser_get_job_counter(ocs::tom::ConstElement<ocs::tom::SU> suser);

int suser_check_new_job(ocs::tom::ConstElement<ocs::tom::JB> job, uint32_t max_u_jobs,
                        ocs::tom::List<ocs::tom::SU> master_suser_list);

uint32_t suser_job_count(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstList<ocs::tom::SU> master_suser_list);

int suser_register_new_job(ocs::tom::ConstElement<ocs::tom::JB> job, uint32_t max_u_jobs, int force_registration,
                           ocs::tom::List<ocs::tom::SU> master_suser_list);

void suser_unregister_job(ocs::tom::ConstElement<ocs::tom::JB> job,
                          ocs::tom::List<ocs::tom::SU> master_suser_list);
