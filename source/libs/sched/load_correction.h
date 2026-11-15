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
 * @brief Load correction and capacity correction for the scheduler
 *
 * A job that has just started is not yet visible in the load values the
 * execution host reports - the load report is minutes behind. Without a
 * correction the scheduler would keep dispatching to a host it has just
 * filled. correct_load() therefore adds an artificial load for every recently
 * started job, decaying to zero over `load_adjustment_decay_time`.
 */

#include "cull/cull.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"

int correct_load(ocs::tom::ConstList<ocs::tom::JB> running_jobs, ocs::tom::ConstList<ocs::tom::QU> queue_list,
                 ocs::tom::List<ocs::tom::EH> host_list, uint64_t decay_time, bool monitor_next_run);
OCS_TRANSITIONAL int correct_load(lList *running_jobs, lList *queue_list, lList *host_list, uint64_t decay_time,
                                  bool monitor_next_run);

int correct_capacities(ocs::tom::List<ocs::tom::EH> host_list, ocs::tom::ConstList<ocs::tom::CE> centry_list,
                       ocs::tom::ConstList<ocs::tom::CE> load_adjustments);
OCS_TRANSITIONAL int correct_capacities(lList *host_list, const lList *centry_list, const lList *load_adjustments);
