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

/** @file
 * @brief Declarations of the cluster queue attribute verification functions
 *
 * @see sge_cqueue_verify.cc
 */

#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_ACELIST.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_ASOLIST.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_ASTRLIST.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_AULNG.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"

bool cqueue_verify_calendar(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem,
                            ocs::tom::ConstList<ocs::tom::CAL> master_calendar_list);

bool cqueue_verify_ckpt_list(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                             ocs::tom::ConstElement<ocs::tom::ASTRLIST> attr_elem,
                             ocs::tom::ConstList<ocs::tom::CK> master_ckpt_list);

bool cqueue_verify_consumable_config_list(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                          ocs::tom::ConstElement<ocs::tom::ACELIST> attr_elem,
                                          ocs::tom::ConstList<ocs::tom::CE> master_centry_list);

bool cqueue_verify_initial_state(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem);

bool cqueue_verify_pe_list(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           ocs::tom::ConstElement<ocs::tom::ASTRLIST> attr_elem,
                           ocs::tom::ConstList<ocs::tom::PE> master_pe_list);

bool cqueue_verify_priority(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem);

bool cqueue_verify_processors(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                              ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem);

bool cqueue_verify_project_list(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                ocs::tom::ConstElement<ocs::tom::APRJLIST> attr_elem,
                                ocs::tom::ConstList<ocs::tom::PR> master_project_list);

bool cqueue_verify_shell_start_mode(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem);

bool cqueue_verify_shell(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                         ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem);

bool cqueue_verify_subordinate_list(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    ocs::tom::ConstElement<ocs::tom::ASOLIST> attr_elem,
                                    ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list);

bool cqueue_verify_user_list(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                             ocs::tom::ConstElement<ocs::tom::AUSRLIST> attr_elem,
                             ocs::tom::ConstList<ocs::tom::US> master_userset_list);

bool cqueue_verify_job_slots(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                             ocs::tom::ConstElement<ocs::tom::AULNG> attr_elem);

bool cqueue_verify_memory_value(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                ocs::tom::ConstElement<ocs::tom::AMEM> attr_elem);

bool cqueue_verify_time_value(ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                              ocs::tom::ConstElement<ocs::tom::ATIME> attr_elem);
