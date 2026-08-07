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

#include "cull/cull.h"

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

      static void calculate_decay_constant(double halftime, double *decay_rate, double *decay_constant);

      static void decay_usage(const lList *usage_list, const lList *decay_list, double interval);

      /**
       * @brief Add one named decay constant to a decay list
       * @param[in,out] decay_list the list to extend
       * @param value the decay constant
       * @param name the usage attribute it applies to
       */
      static void add_decay_element(lList **decay_list, double value, const char *name);

      static lList *get_decay_list();

      /**
       * @brief Decay the already booked usage and add this job's scaled usage on top
       * @param job the finished job
       * @param ja_task the finished array task
       * @param node the job owner's share tree node
       * @param user the submitting user's object
       * @param project the job's project
       * @param decay_list the decay constants, one per usage attribute
       * @param usage_weight_list the weights each usage attribute is scaled by
       * @param seqno the current usage sequence number, so usage is decayed only once
       * @param curr_time now, in seconds, as the base of the decay
       */
      static void decay_and_sum_usage(lListElem *job, lListElem *ja_task, lListElem *node, lListElem *user, lListElem *project,
                                      lList *decay_list, const lList *usage_weight_list, u_long seqno, uint64_t curr_time);

      /**
       * @brief Sum a finished job's scaled usage into UU_usage / PR_usage / UPP_usage
       *
       * No decay is applied. This is the worker-thread booking path introduced
       * in CS-1239: decay moved to a periodic Timed Event Thread task, and only
       * the additive part of #decay_and_sum_usage runs at finish time.
       * `usage_time_stamp` is left untouched; the TET decay task owns it.
       *
       * @param job the finished job
       * @param ja_task the finished array task
       * @param user the submitting user's object, booked into `UU_usage`
       * @param project the job's project, booked into `PR_usage` and `UPP_usage`
       * @param usage_weight_list the weights each usage attribute is scaled by
       */
      static void sum_usage(lListElem *job, lListElem *ja_task, lListElem *user, lListElem *project,
                            const lList *usage_weight_list);

      static bool strip_irrelevant_usage(lList *usage_list, const lList *usage_weight_list);

      /**
       * @brief Build a named usage list, reusing the values of an existing one
       * @param name the name the resulting list is stored under
       * @param old_usage_list the list to take the existing values from; may be nullptr
       * @return the new list
       */
      static lList *build_usage_list(const char *name, lList *old_usage_list);

      /**
       * @brief Look up one usage attribute by name
       * @param usage_list the list to search
       * @param name the attribute to find
       * @return the element, or nullptr when the attribute is not booked
       */
      static lListElem *get_usage(lList *usage_list, const char *name);

      /**
       * @brief Create a usage element for the named attribute, initialised to zero
       * @param name the attribute name
       * @return the new element
       */
      static lListElem *create_usage_elem(const char *name);
   };
}
