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
 *  Copyright: 2007 by Rising Sun Pictures
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Array task dependencies
 */

#include "tom/ocs_Element.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/sge_answer.h"

bool
sge_task_depend_is_same_range(ocs::tom::ConstElement<ocs::tom::JB> pre_jep,
                              ocs::tom::ConstElement<ocs::tom::JB> suc_jep);

ocs::tom::ElementPtr<ocs::tom::RN>
sge_task_depend_get_range(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::ConstElement<ocs::tom::JB> pre_jep,
                          ocs::tom::ConstElement<ocs::tom::JB> suc_jep, uint32_t task_id);

bool
sge_task_depend_update(ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::ListPtr<ocs::tom::AN> *alpp, uint32_t task_id,
                       uint64_t gdi_request);

bool
sge_task_depend_init(ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::ListPtr<ocs::tom::AN> *alpp, uint64_t gdi_session);

bool
sge_task_depend_flush(ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::ListPtr<ocs::tom::AN> *alpp, uint64_t gdi_session);
