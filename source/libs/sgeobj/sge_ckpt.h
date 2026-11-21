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
 * @brief Declarations for checkpointing environments
 *
 * @see sge_ckpt.cc
 */

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_ST.h"

#include "sgeobj/sge_answer.h"

bool
ckpt_is_referenced(ocs::tom::ConstElement<ocs::tom::CK> ckpt, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                   ocs::tom::ConstList<ocs::tom::JB> master_job_list,
                   ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list);

ocs::tom::Element<ocs::tom::CK>
ckpt_list_locate(ocs::tom::ConstList<ocs::tom::CK> ckpt_list, const char *ckpt_name);

int
sge_parse_checkpoint_attr(const char *attr_str);

int ckpt_validate(ocs::tom::ConstElement<ocs::tom::CK> this_elem, ocs::tom::ListPtr<ocs::tom::AN> *alpp);

bool
ckpt_list_do_all_exist(ocs::tom::ConstList<ocs::tom::CK> ckpt_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       ocs::tom::ConstList<ocs::tom::ST> ckpt_ref_list);

ocs::tom::ElementPtr<ocs::tom::CK> sge_generic_ckpt(const char *ckpt_name);
