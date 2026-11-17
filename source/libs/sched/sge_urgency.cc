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
 * @brief Job urgency - the part of the priority that does not depend on the policies
 *
 * A job's priority has three contributions: the ticket policies, the POSIX
 * priority set with `-p`, and the **urgency**, which comes from the job
 * itself - its resource requests, its deadline, and how long it has been
 * waiting. This module computes the urgency part before the ticket policies
 * run.
 */

#include <cfloat>

#include "uti/ocs_Pattern.h"
#include "uti/sge.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"

#include "cull/cull.h"

#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_pe.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_RN.h"

#include "sge_urgency.h"

static void sge_normalize_urgency(ocs::tom::List<ocs::tom::JB> job_list, double min_urgency, double max_urgency);
static void sge_urgency(uint64_t now, double *min_urgency, double *max_urgency, ocs::tom::List<ocs::tom::JB> job_list,
                        ocs::tom::ConstList<ocs::tom::CE> centry_list, ocs::tom::ConstList<ocs::tom::PE> pe_list);


/**
 * @brief Compute normalized urgency
 *
 * Determine normalized urgency for all job lists passed:
 * * for the pending jobs we need it for determine dispatch order
 * * for the running jobs it is needed when running jobs priority must
 *   be compared with pending jobs (preemption only)
 *
 * @param now Current time
 * @param running_jobs The running jobs list
 * @param pending_jobs The pending jobs list
 * @param lists Additional config information
 */
void sge_do_urgency(uint64_t now, const ocs::tom::List<ocs::tom::JB> running_jobs,
                    const ocs::tom::List<ocs::tom::JB> pending_jobs, scheduler_all_data_t *lists) {
   DENTER(TOP_LAYER);
   double min_urgency = DBL_MAX;
   double max_urgency = DBL_MIN;
   const auto centries = lists->centry_list;
   const auto pes = ocs::tom::ConstList<ocs::tom::PE>(*lists->pe_list);

   /* determine absolute static urgency and related min/max values */
   sge_urgency(now, &min_urgency, &max_urgency, pending_jobs, centries, pes);
   sge_urgency(now, &min_urgency, &max_urgency, running_jobs, centries, pes);

   /* use min/max value to normalize static urgency */
   if (pending_jobs != nullptr) {
      sge_normalize_urgency(pending_jobs, min_urgency, max_urgency);
   }   
   if (running_jobs != nullptr) {
      sge_normalize_urgency(running_jobs, min_urgency, max_urgency);
   }
   DRETURN_VOID;
}

/**
 * @brief Determine urgency value for a list of jobs
 *
 * The urgency value is determined for all jobs in job_list. The urgency
 * value has two time dependent components (waiting time contribution and
 * deadline contribution) and a resource request dependent component. Only
 * resource requests that apply to the job irrespective what resources it
 * gets assigned finally are considered. Default requests specified for
 * consumable resources are not considered as they are placement dependent.
 * For the same reason soft request do not contribute to the urgency value.
 * The urgency value range is tracked via min/max urgency. Category-based
 * caching is used for the resource request urgency contribution.
 *
 * @param now Current time
 * @param min_urgency For tracking minimum urgency value
 * @param max_urgency For tracking minimum urgency value
 * @param job_list The jobs.
 * @param centry_list Needed for per resource urgency setting.
 * @param pe_list Needed to determine urgency slot setting.
 */
static void sge_urgency(uint64_t now, double *min_urgency, double *max_urgency,
                        const ocs::tom::List<ocs::tom::JB> job_list,
                        const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                        const ocs::tom::ConstList<ocs::tom::PE> pe_list) {
   DENTER(TOP_LAYER);
   double rrc, wtc, dtc, absolute_urgency;
   int slots;
   double weight_deadline = sconf_get_weight_deadline();
   double weight_waiting_time = sconf_get_weight_waiting_time();


   for (const auto jep : job_list) {
      uint64_t deadline;

      rrc = dtc = 0.0;

      /* waiting time dependent contribution */
      wtc = weight_waiting_time * sge_gmt64_to_gmt32(now - ocs::tom::get<JB_submission_time>(jep));

      /* job deadline dependent contribution */
      deadline = ocs::tom::get<JB_deadline>(jep);
      if (deadline > 0) {
         if (deadline >= now) {
            dtc = weight_deadline / std::max(sge_gmt64_to_gmt32_double(deadline - now), 1.0);
         } else {
            dtc = weight_deadline;
         }
/*           DPRINTF("free: %d now: " sge_u32 " deadline: " sge_u32 "\n", time_left, now, deadline); */
          /* might be too late for this job anyway we're optimistic and treat it high prior */
      }

      /* we do category based caching when determining the resource request dependent contribution */
      // the category is a reference (lRefT) to the job's category element
      const auto cat =
         ocs::tom::Element<ocs::tom::CT>::from_typed(static_cast<lListElem *>(ocs::tom::get<JB_category>(jep)));
      if (cat != nullptr && ocs::tom::get<CT_rc_valid>(cat)) {
         rrc = ocs::tom::get<CT_resource_contribution>(cat);
/*         DPRINTF("  resource contribution from category cache ---> %7f\n", rrc); */
      } else {
         double contribution;

         slots = sge_job_slot_request(jep, pe_list);

         /* contribution for implicit slot request */ 
         const auto slots_centry =
            ocs::tom::ConstElement<ocs::tom::CE>::from_typed(centry_list_locate(centry_list, SGE_ATTR_SLOTS));
         if (slots_centry == nullptr) {
            continue;
         }
         contribution = centry_urgency_contribution(slots, SGE_ATTR_SLOTS, 1.0, slots_centry);
         rrc += contribution;

         /* contribution for all explicit requests */
         for (const auto rr : job_get_hard_resource_list(jep)) {
            const char *name = ocs::tom::get<CE_name>(rr);
            const auto centry = ocs::tom::ConstElement<ocs::tom::CE>::from_typed(centry_list_locate(centry_list, name));
            if (centry == nullptr) {
               continue;
            }
            contribution = centry_urgency_contribution(slots, ocs::tom::get<CE_name>(rr), 
                  ocs::tom::get<CE_doubleval>(rr), centry);
            rrc += contribution;
         }

         /* cache in category */
         if (cat != nullptr) {
            ocs::tom::set<CT_rc_valid>(cat,              true);
            ocs::tom::set<CT_resource_contribution>(cat, rrc);
         }
      }
      absolute_urgency = wtc + dtc + rrc; 

      /* store these values with the job */
      ocs::tom::set<JB_dlcontr>(jep, dtc);
      ocs::tom::set<JB_rrcontr>(jep, rrc);
      ocs::tom::set<JB_wtcontr>(jep, wtc);
      ocs::tom::set<JB_urg>(jep, absolute_urgency);

/*      DPRINTF("--- job " sge_u32 " (dtc %7f + wtc %7f + rrc %7f) = asu %7f\n",
            lGetUlong(jep, JB_job_number), dtc, wtc, rrc, absolute_urgency);
*/            
      
      /* track min/max values */
      if (min_urgency) {
         *min_urgency = std::min(*min_urgency, absolute_urgency);
      }   
      if (max_urgency) {
         *max_urgency = std::max(*max_urgency, absolute_urgency);
      }   
   }

   DRETURN_VOID;
}

/**
 * @brief Computes normalized urgency for job list
 *
 * The normalized urgency is determined for a list of jobs based on the
 * min/max urgency values passed and the JB_urg value of each job.
 *
 * @param job_list The job list
 * @param min_urgency minimum urgency value
 * @param max_urgency maximum urgency value
 *
 * @note MT-NOTES: sge_normalize_urgency() is MT safe
 */
static void sge_normalize_urgency(const ocs::tom::List<ocs::tom::JB> job_list, double min_urgency, double max_urgency) {
   DENTER(TOP_LAYER);

   DPRINTF("ASU min = %13.11f, ASU max = %13.11f\n", min_urgency, max_urgency);
   for (const auto jep : job_list) {
      double asu = ocs::tom::get<JB_urg>(jep);
      double nsu = sge_normalize_value(asu, min_urgency, max_urgency);
      ocs::tom::set<JB_nurg>(jep, nsu);
/*    DPRINTF("NSU(job " sge_u32 ") = %f from %f\n", lGetUlong(jep, JB_job_number), nsu, asu); */
   }
   DRETURN_VOID;
}


/**
 * @brief Return static urgency jobs slot request
 *
 * For sequential jobs the static urgency job slot request is always 1.
 * For parallel jobs the static urgency job slot request depends on
 * static urgency slots as defined with sge_pe(5).
 *
 * @param job the job (JB_Type)
 * @param pe_list the PE list (PE_Type)
 *
 * @return Number of slots
 *
 * @note In case of a wildcard parallel environment request the setting of the
 *       first matching is used. Behaviour is undefined if multiple parallel
 *       environments specify different settings!
 */
int sge_job_slot_request(const ocs::tom::ConstElement<ocs::tom::JB> job,
                         const ocs::tom::ConstList<ocs::tom::PE> pe_list) {
   DENTER(TOP_LAYER);

   const char *pe_name;
   const char *urgency_slot_setting; 
   ocs::tom::ConstElement<ocs::tom::PE> pep;
   int n;
  
   /* sequential job */
   if (!(pe_name=ocs::tom::get<JB_pe>(job))) {
      DRETURN(1);
   } 

   /* parallel job with fixed slot request */
   const auto range_list = ocs::tom::get<JB_pe_range>(job);
   if (range_list_get_number_of_ids(range_list)==1) { 
      DRETURN(range_list_get_first_id(range_list, nullptr));
   } 

   /* parallel job with slot range request */
   if (!ocs::is_pattern(pe_name))
      pep = ocs::tom::ConstElement<ocs::tom::PE>::from_typed(pe_list_locate(pe_list, pe_name));
   else {
      /* use the first matching pe */
      pep = ocs::tom::ConstElement<ocs::tom::PE>::from_typed(pe_list_find_matching(pe_list, pe_name));
      if (pep != nullptr) {
         DPRINTF("use %s as first matching pe for %s to verify schedulability\n", ocs::tom::get<PE_name>(pep), pe_name);
      }
   }
   if (pep == nullptr) {
      ERROR("no matching parallel environment " "for job's PE request \"%s\"\n", pe_name);
      DRETURN(1);
   }

   urgency_slot_setting = ocs::tom::get<PE_urgency_slots>(pep);

   n = pe_urgency_slots(pep, urgency_slot_setting, range_list); 

   {
      char pe_range_str[1024];
      dstring pe_range;
      sge_dstring_init(&pe_range, pe_range_str, sizeof(pe_range_str));
      range_list_print_to_string(range_list, &pe_range, true, false, false);
      DPRINTF("slot request assumed for static urgency is %d for %s PE range due to "
          "PE's \"%s\" setting \"%s\"\n", n, pe_range_str, ocs::tom::get<PE_name>(pep), 
          urgency_slot_setting);
   }

   DRETURN(n);
}

/**
 * @brief The number of slots assumed for a job's static urgency, for a caller with untyped objects (N-X-2)
 *
 * @param job     the job (JB_Type), checked
 * @param pe_list the parallel environments (PE_Type), checked
 *
 * @return the number of slots
 */
int sge_job_slot_request(const lListElem *job, const lList *pe_list) {
   return sge_job_slot_request(ocs::tom::ConstElement<ocs::tom::JB>(job), ocs::tom::ConstList<ocs::tom::PE>(pe_list));
}
