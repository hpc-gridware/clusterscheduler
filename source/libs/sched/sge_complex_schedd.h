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
 * @brief Assembling the complex attributes the scheduler matches against
 *
 * An attribute can be defined at three levels - global, execution host, queue
 * instance - and a job's `-l` request has to be matched against the value
 * that actually applies. Which level wins is what the **dominance** of an
 * attribute records, and monitor_dominance() renders it as the `g`/`h`/`q`
 * prefix seen in `qstat -F` output.
 *
 * get_attribute() answers for one level, get_attribute_by_name() resolves
 * across all three, and host_complexes2scheduler() and
 * queue_complexes2scheduler() build the complete attribute list of one object
 * for the scheduler to match a request against.
 */

#include "cull/cull.h"

#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

void monitor_dominance(char *str, uint32_t mask);

ocs::tom::ListPtr<ocs::tom::CE>
host_complexes2scheduler(ocs::tom::ConstElement<ocs::tom::EH> host, ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                         ocs::tom::ConstList<ocs::tom::CE> centry_list,
                         ocs::tom::ConstList<ocs::tom::CE> load_adjustments);

ocs::tom::ListPtr<ocs::tom::CE>
queue_complexes2scheduler(ocs::tom::ConstElement<ocs::tom::QU> queue, ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                          ocs::tom::ConstList<ocs::tom::CE> centry_list,
                          ocs::tom::ConstList<ocs::tom::CE> load_adjustments);

ocs::tom::ElementPtr<ocs::tom::CE>
get_attribute_by_name(ocs::tom::ConstElement<ocs::tom::EH> global, ocs::tom::ConstElement<ocs::tom::EH> host,
                      ocs::tom::ConstElement<ocs::tom::QU> queue, const char *attrname,
                      ocs::tom::ConstList<ocs::tom::CE> centry_list, ocs::tom::ConstList<ocs::tom::CE> load_adjustments,
                      uint64_t start_time, uint64_t duration);

int compare_complexes(int slots, ocs::tom::ConstElement<ocs::tom::CE> req_cplx,
                      ocs::tom::ConstElement<ocs::tom::CE> src_cplx, char *availability_text,
                      int is_threshold, int force_existence);

ocs::tom::ElementPtr<ocs::tom::CE>
get_attribute(const char *attrname, ocs::tom::ConstList<ocs::tom::CE> config_attr,
              ocs::tom::ConstList<ocs::tom::RUE> actual_attr, ocs::tom::ConstList<ocs::tom::HL> load_attr,
              ocs::tom::ConstList<ocs::tom::CE> centry_list, ocs::tom::ConstList<ocs::tom::CE> load_adjustments,
              ocs::tom::ConstElement<ocs::tom::QU> queue, uint32_t layer, double lc_factor, dstring *reason,
              bool zero_utilization, uint64_t start_time, uint64_t duration);

int string_base_cmp(ocs::CEntry::Type type, const char *s1, const char *s2);
int string_base_cmp_old(ocs::CEntry::Type type, const char *s1, const char *s2);

bool request_cq_rejected(ocs::tom::ConstList<ocs::tom::CE> hard_resource_list, ocs::tom::ConstElement<ocs::tom::CQ> cq,
                         ocs::tom::ConstList<ocs::tom::CE> centry_list, bool single_slot, dstring *unsatisfied);
