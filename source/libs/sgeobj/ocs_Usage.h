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
 * @brief Resource usage reported for a job or task
 */

#include <utility>

#include "cull/cull.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_UU.h"

/// Length of one usage interval in seconds; a double so the decay maths does not divide integers
constexpr double sge_usage_interval = 60.0;

namespace ocs {
   /**
    * @brief Accumulating and decaying the resource usage the share tree is scheduled on
    *
    * Usage is booked additively when a job finishes and decays over time, so
    * recent consumption weighs more than old consumption. Both halves used to
    * run at finish time; since CS-1239 the decay half runs periodically in the
    * Timed Event Thread and only #sum_usage runs on the worker.
    */
   class Usage {
   public:
      static void calculate_default_decay_constant(int halftime);

      static std::pair<double, double> calculate_decay_constant(double halftime);

      static void decay_usage(ocs::tom::List<ocs::tom::UA> usage_list, ocs::tom::ConstList<ocs::tom::UA> decay_list,
                              double interval);

      static void add_decay_element(ocs::tom::ListPtr<ocs::tom::UA> &decay_list, double value, const char *name);

      static ocs::tom::ListPtr<ocs::tom::UA> get_decay_list();

      static void decay_and_sum_usage(ocs::tom::ConstElement<ocs::tom::JB> job,
                                      ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                      ocs::tom::Element<ocs::tom::STN> node, ocs::tom::Element<ocs::tom::UU> user,
                                      ocs::tom::Element<ocs::tom::PR> project,
                                      ocs::tom::ConstList<ocs::tom::UA> decay_list,
                                      ocs::tom::ConstList<ocs::tom::UA> usage_weight_list, u_long seqno,
                                      uint64_t curr_time);
      OCS_TRANSITIONAL static void decay_and_sum_usage(lListElem *job, lListElem *ja_task, lListElem *node,
                                                       lListElem *user, lListElem *project, const lList *decay_list,
                                                       const lList *usage_weight_list, u_long seqno,
                                                       uint64_t curr_time);

      static void sum_usage(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                            ocs::tom::Element<ocs::tom::UU> user, ocs::tom::Element<ocs::tom::PR> project,
                            ocs::tom::ConstList<ocs::tom::UA> usage_weight_list);
      OCS_TRANSITIONAL static void sum_usage(lListElem *job, lListElem *ja_task, lListElem *user, lListElem *project,
                                             const lList *usage_weight_list);

      static bool strip_irrelevant_usage(ocs::tom::List<ocs::tom::UA> usage_list,
                                         ocs::tom::ConstList<ocs::tom::UA> usage_weight_list);

      static ocs::tom::ListPtr<ocs::tom::UA> build_usage_list(const char *name,
                                                             ocs::tom::ConstList<ocs::tom::UA> old_usage_list);

      static ocs::tom::Element<ocs::tom::UA> get_usage(ocs::tom::List<ocs::tom::UA> usage_list, const char *name);

      static ocs::tom::ElementPtr<ocs::tom::UA> create_usage_elem(const char *name);
   };
}
