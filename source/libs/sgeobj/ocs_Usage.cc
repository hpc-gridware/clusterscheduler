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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <math.h>

#ifndef NO_SGE_COMPILE_DEBUG
#   define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "ocs_Usage.h"
#include "sge_job.h"
#include "sge_schedd_conf.h"
#include "sge_usage.h"

/** @brief Calculate decay rate and constant based on half-life
 *
 * This function calculates the decay rate and decay constant based on
 * the provided half-life in minutes. The decay rate is calculated using
 * the formula: -log(0.5) / (halftime * 60). The decay constant is then
 * calculated as 1 - (decay_rate * sge_usage_interval).
 *
 * @param halftime The half-life in minutes.
 * @param decay_rate Pointer to store the calculated decay rate.
 * @param decay_constant Pointer to store the calculated decay constant.
 */
void
ocs::Usage::calculate_decay_constant(const double halftime, double *decay_rate, double *decay_constant) {
   if (halftime < 0) {
      *decay_rate = 1.0;
      *decay_constant = 0;
   } else if (halftime == 0) {
      *decay_rate = 0;
      *decay_constant = 1.0;
   } else {
      *decay_rate = -log(0.5) / (halftime * 60);
      *decay_constant = 1 - (*decay_rate * sge_usage_interval);
   }
}

/** @brief Calculate the default decay constant based on half-life
 *
 * This function calculates the default decay constant based on the
 * provided half-life in hours. It converts the half-life to seconds
 * and then calls calculate_decay_constant to compute the decay rate
 * and constant.
 *
 * @param halftime The half-life in hours.
 */
void
ocs::Usage::calculate_default_decay_constant(const int halftime) {
   double sge_decay_rate = 0.0;
   double sge_decay_constant = 0.0;

   calculate_decay_constant(halftime * 60.0, &sge_decay_rate, &sge_decay_constant);
   sconf_set_decay_constant(sge_decay_constant);
}

/** @brief Decay usage for the passed usage list
 *
 * This function decays the usage values in the given usage list based on
 * the decay list and the specified interval. If a decay value is not found
 * in the decay list, a default decay constant is used.
 *
 * @param usage_list The list of usage elements to be decayed.
 * @param decay_list The list of decay elements to use for decay calculations.
 * @param interval The time interval over which to apply the decay.
 */
void
ocs::Usage::decay_usage(const lList *usage_list, const lList *decay_list, const double interval) {
   if (usage_list) {
      lListElem *usage = nullptr;

      for_each_rw (usage, usage_list) {
         double decay;

         if (const lListElem *decay_elem;
             decay_list != nullptr && (decay_elem = lGetElemStr(decay_list, UA_name, lGetPosString(usage, UA_name_POS))) != nullptr) {
            decay = pow(lGetPosDouble(decay_elem, UA_value_POS), interval / sge_usage_interval);
         } else {
            decay = pow(sconf_get_decay_constant(), interval / sge_usage_interval);
         }
         lSetPosDouble(usage, UA_value_POS, lGetPosDouble(usage, UA_value_POS) * decay);
      }
   }
}

/** @brief Create a decay list based on half-life values
 *
 * This function creates a decay list based on the half-life values
 * defined in the scheduler configuration. If no half-life values are
 * defined, it creates a default decay list for finished jobs.
 *
 * @return A pointer to the created decay list.
 */
lList *
ocs::Usage::create_decay_list() {
   lList *decay_list = nullptr;
   lList *halflife_decay_list = sconf_get_halflife_decay_list();

   if (halflife_decay_list != nullptr) {
      double decay_rate, decay_constant;
      const lListElem *ep = nullptr;

      for_each_ep(ep, halflife_decay_list) {
         calculate_decay_constant(lGetDouble(ep, UA_value), &decay_rate, &decay_constant);
         lListElem *u = lAddElemStr(&decay_list, UA_name, lGetString(ep, UA_name), UA_Type);
         lSetDouble(u, UA_value, decay_constant);
      }
   } else {
      double decay_rate, decay_constant;

      // @todo: For what is this required?
      calculate_decay_constant(-1, &decay_rate, &decay_constant);
      lListElem *u = lAddElemStr(&decay_list, UA_name, "finished_jobs", UA_Type);
      lSetDouble(u, UA_value, decay_constant);
   }
   lFreeList(&halflife_decay_list);
   return decay_list;
}

lListElem *
ocs::Usage::get_by_name(const lList *usage_list, const char *name) {
   return lGetElemStrRW(usage_list, UA_name, name);
}

lListElem *
ocs::Usage::create_with_name(const char *name) {
   lListElem *usage = lCreateElem(UA_Type);
   lSetString(usage, UA_name, name);
   lSetDouble(usage, UA_value, 0);
   return usage;
}

lListElem *
ocs::Usage::get_or_create_by_name(lList *usage_list, const char *name) {
   if (usage_list == nullptr) {
      return nullptr;
   }

   lListElem *usage = get_by_name(usage_list, name);
   if (usage == nullptr) {
      usage = create_with_name(name);
      lAppendElem(usage_list, usage);
   }
   return usage;
}