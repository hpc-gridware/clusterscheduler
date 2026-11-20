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
 *  Portions of this software are Copyright (c) 2025-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Resource usage reported for a job or task
 */

#include <cstring>
#include <math.h>
#include <utility>

#ifndef NO_SGE_COMPILE_DEBUG
#   define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "ocs_Usage.h"
#include "ocs_UserProject.h"
#include "sge_ja_task.h"
#include "sge_job.h"
#include "sge_pe_task.h"
#include "sge_schedd_conf.h"
#include "sge_usage.h"
#include "sge_userprj.h"

#include "cull/sge_eejob_SGEJ_L.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_UPP.h"
#include "sgeobj/tom/ocs_tom_UPU.h"
#include "sgeobj/tom/ocs_tom_UU.h"

/** @brief Calculate decay rate and constant based on half-life
 *
 * This function calculates the decay rate and decay constant based on
 * the provided half-life in minutes. The decay rate is calculated using
 * the formula: -log(0.5) / (halftime * 60). The decay constant is then
 * calculated as 1 - (decay_rate * sge_usage_interval).
 *
 * @param halftime The half-life in minutes.
 * @return the decay rate and the decay constant
 */
std::pair<double, double>
ocs::Usage::calculate_decay_constant(const double halftime) {
   DENTER(BASIS_LAYER);

   double decay_rate;
   double decay_constant;

   if (halftime < 0) {
      /* Sentinel "no decay" (used for the "finished_jobs" counter by
       * get_decay_list when no halflife_decay_list is configured).
       * Previously this branch set decay_constant=0 which meant
       * pow(0, interval)=0 in decay_usage and zeroed the counter every
       * tick - the "finished_jobs" PR_usage entry could never accumulate
       * and sge_sort_pending_job_nodes' job_count denominator stayed at
       * 0 + pending_position, starving leaves with many pending jobs.
       * decay_constant=1.0 means pow(1, x)=1, leaving the value
       * untouched - the intended "no decay" semantics for the sentinel. */
      decay_rate = 0;
      decay_constant = 1.0;
   } else if (halftime == 0) {
      decay_rate = 0;
      decay_constant = 1.0;
   } else {
      decay_rate = -log(0.5) / (halftime * 60);
      decay_constant = 1 - (decay_rate * sge_usage_interval);
   }

   DRETURN(std::make_pair(decay_rate, decay_constant));
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
   DENTER(BASIS_LAYER);

   const auto [sge_decay_rate, sge_decay_constant] = calculate_decay_constant(halftime * 60.0);
   sconf_set_decay_constant(sge_decay_constant);

   DRETURN_VOID;
}

/**
 * @brief Add one named decay constant to a decay list
 * @param[in,out] decay_list the list to extend (UA); created if it is empty
 * @param value the half life the decay constant is computed from
 * @param name the usage attribute it applies to
 */
void ocs::Usage::add_decay_element(ocs::tom::ListPtr<ocs::tom::UA> &decay_list, const double value, const char *name) {
   DENTER(BASIS_LAYER);

   const auto [decay_rate, decay_constant] = calculate_decay_constant(value);
   if (!decay_list) {
      decay_list = ocs::tom::ListPtr<ocs::tom::UA>::create("");
   }
   const auto u = decay_list->emplace_back<UA_name>(name);
   ocs::tom::set<UA_value>(u, decay_constant);

   DRETURN_VOID;
}

/** @brief Calculate decay constants for half-life decay list
 *
 * This function calculates the decay list based on the half-life
 * and decay constants provided in the configuration.
 *
 * @return A list of decay elements with their corresponding decay constants (UA), owned by the caller;
 *         empty if the configured half life list has no entries
 */
ocs::tom::ListPtr<ocs::tom::UA>
ocs::Usage::get_decay_list() {
   DENTER(BASIS_LAYER);

   ocs::tom::ListPtr<ocs::tom::UA> decay_list;
   const auto halflife_decay_list = sconf_get_halflife_decay_list();

   if (halflife_decay_list) {
      for (const auto ep : halflife_decay_list.get()) {
         add_decay_element(decay_list, ocs::tom::get<UA_value>(ep), ocs::tom::get<UA_name>(ep));
      }
   } else {
      /* The "finished_jobs" counter is summed into UU_/PR_/UPP_ from each
       * finished ja_task (see sge_book_finished_job_usage). It is read by
       * the pending-priority formula in sge_sort_pending_job_nodes
       * (sgeee.cc) as the historical-throughput denominator that keeps
       * leaves with many finished jobs from being out-prioritized by
       * leaves with fewer pending. We seed it with the "no decay"
       * sentinel (halftime < 0 in calculate_decay_constant -> decay
       * constant 1.0) so the counter is a stable count, not a decayed
       * weight. If the admin configures halflife_decay_list explicitly,
       * they are expected to provide their own "finished_jobs" entry
       * with the decay they want. */
      add_decay_element(decay_list, -1, "finished_jobs");
   }
   DRETURN(decay_list);
}

/** @brief Decay usage for the passed usage list
 *
 * This function decays the usage values in the given usage list based on
 * the decay list and the specified interval. If a decay value is not found
 * in the decay list, a default decay constant is used.
 *
 * @param usage_list The list of usage elements to be decayed (UA), may be an empty handle.
 * @param decay_list The list of decay elements to use for decay calculations (UA), may be an empty handle.
 * @param interval The time interval over which to apply the decay.
 */
void
ocs::Usage::decay_usage(const ocs::tom::List<ocs::tom::UA> usage_list,
                        const ocs::tom::ConstList<ocs::tom::UA> decay_list, const double interval) {
   DENTER(BASIS_LAYER);

   for (const auto usage : usage_list) {
      double decay;

      if (const auto decay_elem = ocs::tom::find_first<UA_name>(decay_list, ocs::tom::get<UA_name>(usage));
          decay_elem) {
         decay = pow(ocs::tom::get<UA_value>(decay_elem), interval / sge_usage_interval);
      } else {
         decay = pow(sconf_get_decay_constant(), interval / sge_usage_interval);
      }
      ocs::tom::set<UA_value>(usage, ocs::tom::get<UA_value>(usage) * decay);
   }

   DRETURN_VOID;
}


/** @brief Decide whether a usage attribute is relevant for share tree usage.
 *
 * A finished job reports the full set of online usage attributes (cpu, mem,
 * io, iow, vmem, all ru_* and acct_* values, ...). Only a small subset of
 * these is actually used in share tree usage calculations: the attributes
 * cpu, mem and io (always, they are also the seeded defaults), any attribute
 * for which the administrator configured a usage weight, and the synthetic
 * finished_jobs counter. All other attributes must not be booked into the
 * user/project usage lists - otherwise those objects (and their spooled
 * representation) grow huge over time (see CS-1385).
 *
 * @param name the usage attribute
 * @param usage_weight_list the configured usage weights (UA), may be an empty handle
 * @return true if the attribute is booked into the user and project usage
 */
static bool
usage_relevant_for_sharetree(const char *name, const ocs::tom::ConstList<ocs::tom::UA> usage_weight_list) {
   DENTER(BASIS_LAYER);

   bool ret;
   if (strcmp(name, USAGE_ATTR_CPU) == 0 ||
       strcmp(name, USAGE_ATTR_MEM) == 0 ||
       strcmp(name, USAGE_ATTR_IO) == 0 ||
       strcmp(name, "finished_jobs") == 0) {
      ret = true;
   } else {
      ret = static_cast<bool>(ocs::tom::find_first<UA_name>(usage_weight_list, name));
   }
   DRETURN(ret);
}

/**
 * @brief The usage list held in a field, built with the default attributes if the field holds none
 *
 * @tparam F the usage list field, UU_usage, UPP_long_term_usage, ...
 * @tparam E the element handle type holding @p F
 * @param ep the user, project or user-project element, not empty
 * @param name the name a new list gets
 * @return the list held in the field
 */
template <auto F, typename E>
static ocs::tom::List<ocs::tom::UA>
ocs_usage_list_get_or_build(const E ep, const char *name) {
   DENTER(BASIS_LAYER);

   auto list = ocs::tom::get<F>(ep);
   if (!list) {
      ocs::tom::set<F>(ep, ocs::Usage::build_usage_list(name, nullptr));
      list = ocs::tom::get<F>(ep);
   }
   DRETURN(list);
}

/**
 * @brief The usage element of an attribute in a usage list, appended with value 0 if the list has none
 *
 * @param usage_list the usage list (UA), not empty
 * @param name the usage attribute
 * @return the element
 */
static ocs::tom::Element<ocs::tom::UA>
ocs_usage_find_or_append(const ocs::tom::List<ocs::tom::UA> usage_list, const char *name) {
   DENTER(BASIS_LAYER);

   auto usage = ocs::Usage::get_usage(usage_list, name);
   if (!usage) {
      usage = usage_list.push_back(ocs::Usage::create_usage_elem(name));
   }
   DRETURN(usage);
}

/**
 * @brief Shared body of decay_and_sum_usage() and sum_usage()
 *
 * The decay step is gated by do_decay so the worker-thread booking path
 * (CS-1239) can sum the finished job's usage without applying decay;
 * decay is then done by a periodic TET task.
 *
 * @param job the job
 * @param ja_task the array task, may be an empty handle
 * @param user the job owner's user object, may be an empty handle
 * @param project the job's project object, may be an empty handle
 * @param decay_list the decay constants (UA), may be an empty handle; only read if @p do_decay
 * @param usage_weight_list the usage weights (UA), may be an empty handle
 * @param seqno the current usage sequence number, see ocs::UserProject::decay_userprj_usage()
 * @param curr_time now, the base of the decay
 * @param do_decay whether the user and project usage is decayed first
 */
static void
book_user_project_usage(const ocs::tom::ConstElement<ocs::tom::JB> job,
                        const ocs::tom::ConstElement<ocs::tom::JAT> ja_task, const ocs::tom::Element<ocs::tom::UU> user,
                        const ocs::tom::Element<ocs::tom::PR> project,
                        const ocs::tom::ConstList<ocs::tom::UA> decay_list,
                        const ocs::tom::ConstList<ocs::tom::UA> usage_weight_list, const u_long seqno,
                        const uint64_t curr_time, const bool do_decay) {
   DENTER(BASIS_LAYER);

   if (!user && !project) {
      DRETURN_VOID;
   }

   /*-------------------------------------------------------------
    * Decay the usage for the associated user and project
    *-------------------------------------------------------------*/

   if (do_decay) {
      if (user) {
         ocs::UserProject::decay_userprj_usage(user, decay_list, seqno, curr_time);
      }

      if (project) {
         ocs::UserProject::decay_userprj_usage(project, decay_list, seqno, curr_time);
      }
   }

   /*-------------------------------------------------------------
    * Note: Since SGE will update job.usage directly, we
    * maintain the job usage the last time we collected it from
    * the job.  The difference between the new usage and the old
    * usage is what needs to be added to the user or project node.
    * This old usage is maintained in the user or project node
    * depending on the type of share tree.
    *-------------------------------------------------------------*/

   ocs::tom::ListPtr<ocs::tom::UA> job_usage_list;
   if (ja_task) {
      job_usage_list = ocs::tom::get<JAT_scaled_usage_list>(ja_task).clone("");

      /* sum sub-task usage into job_usage_list */
      if (job_usage_list) {
         for (const auto petask : ocs::tom::get<JAT_task_list>(ja_task)) {
            for (const auto src : ocs::tom::get<PET_scaled_usage>(petask)) {
               const auto dst = ocs::tom::find_first_rw<UA_name>(job_usage_list.get(), ocs::tom::get<UA_name>(src));
               if (dst) {
                  ocs::tom::set<UA_value>(dst, ocs::tom::get<UA_value>(dst) + ocs::tom::get<UA_value>(src));
               } else {
                  job_usage_list->push_back(src.clone());
               }
            }
         }
      }
   }

   // the debited job usage is kept in the user if there is one, else in the project
   ocs::tom::ListPtr<ocs::tom::UA> old_usage_list;
   const ocs::tom::ConstList<ocs::tom::UPU> upu_list =
      user ? ocs::tom::get<UU_debited_job_usage>(user) : ocs::tom::get<PR_debited_job_usage>(project);
   if (const auto upu = ocs::tom::find_first<UPU_job_number>(upu_list, ocs::tom::get<JB_job_number>(job)); upu) {
      old_usage_list = ocs::tom::get<UPU_old_usage_list>(upu).clone("");
   }

   if (!old_usage_list) {
      old_usage_list = ocs::Usage::build_usage_list("old_usage_list", nullptr);
   }

   ocs::tom::List<ocs::tom::UA> user_usage_list;
   ocs::tom::List<ocs::tom::UA> project_usage_list;
   ocs::tom::List<ocs::tom::UA> user_long_term_usage_list;
   ocs::tom::List<ocs::tom::UA> project_long_term_usage_list;

   if (user) {

      /* if there is a user & project, usage is kept in the project sub-list */

      if (project) {
         const char *project_name = ocs::tom::get<PR_name>(project);
         const auto upp_list = ocs::tom::get_or_create<UU_project>(user, "");
         auto upp = ocs::tom::find_first_rw<UPP_name>(upp_list, project_name);
         if (!upp) {
            upp = upp_list.emplace_back<UPP_name>(project_name);
         }
         user_long_term_usage_list = ocs_usage_list_get_or_build<UPP_long_term_usage>(upp, "upp_long_term_usage_list");
         user_usage_list = ocs_usage_list_get_or_build<UPP_usage>(upp, "upp_usage_list");
      } else {
         user_long_term_usage_list = ocs_usage_list_get_or_build<UU_long_term_usage>(user, "user_long_term_usage_list");
         user_usage_list = ocs_usage_list_get_or_build<UU_usage>(user, "user_usage_list");
      }
   }

   if (project) {
      project_long_term_usage_list =
         ocs_usage_list_get_or_build<PR_long_term_usage>(project, "project_long_term_usage_list");
      project_usage_list = ocs_usage_list_get_or_build<PR_usage>(project, "project_usage_list");
   }

   /*-------------------------------------------------------------
    * Add to node usage for each usage type
    *-------------------------------------------------------------*/
   for (const auto job_usage : job_usage_list.get()) {
      const char *usage_name = ocs::tom::get<UA_name>(job_usage);

      /*---------------------------------------------------------
       * Only attributes relevant for share tree usage may be
       * booked into the user/project usage lists. All other
       * online usage attributes are ignored here (CS-1385).
       *---------------------------------------------------------*/

      if (!usage_relevant_for_sharetree(usage_name, usage_weight_list)) {
         continue;
      }

      /*---------------------------------------------------------
       * Locate the corresponding usage element for the job
       * usage type in the node usage, old job usage, user usage,
       * and project usage.  If it does not exist, create a new
       * corresponding usage element.
       *---------------------------------------------------------*/

      const auto old_usage = ocs_usage_find_or_append(old_usage_list.get(), usage_name);
      ocs::tom::Element<ocs::tom::UA> user_usage;
      ocs::tom::Element<ocs::tom::UA> user_long_term_usage;
      ocs::tom::Element<ocs::tom::UA> project_usage;
      ocs::tom::Element<ocs::tom::UA> project_long_term_usage;

      if (user_usage_list) {
         user_usage = ocs_usage_find_or_append(user_usage_list, usage_name);
      }
      if (user_long_term_usage_list) {
         user_long_term_usage = ocs_usage_find_or_append(user_long_term_usage_list, usage_name);
      }
      if (project_usage_list) {
         project_usage = ocs_usage_find_or_append(project_usage_list, usage_name);
      }
      if (project_long_term_usage_list) {
         project_long_term_usage = ocs_usage_find_or_append(project_long_term_usage_list, usage_name);
      }

      const double usage_value =
         std::max(ocs::tom::get<UA_value>(job_usage) - ocs::tom::get<UA_value>(old_usage), 0.0);

      /*---------------------------------------------------
       * Add usage to decayed user usage
       *---------------------------------------------------*/
      if (user_usage) {
         ocs::tom::set<UA_value>(user_usage, ocs::tom::get<UA_value>(user_usage) + usage_value);
      }

      /*---------------------------------------------------
       * Add usage to long term user usage
       *---------------------------------------------------*/
      if (user_long_term_usage) {
         ocs::tom::set<UA_value>(user_long_term_usage, ocs::tom::get<UA_value>(user_long_term_usage) + usage_value);
      }

      /*---------------------------------------------------
       * Add usage to decayed project usage
       *---------------------------------------------------*/
      if (project_usage) {
         ocs::tom::set<UA_value>(project_usage, ocs::tom::get<UA_value>(project_usage) + usage_value);
      }

      /*---------------------------------------------------
       * Add usage to long term project usage
       *---------------------------------------------------*/
      if (project_long_term_usage) {
         ocs::tom::set<UA_value>(project_long_term_usage,
                                 ocs::tom::get<UA_value>(project_long_term_usage) + usage_value);
      }
   }

   /*-------------------------------------------------------------
    * save off current job usage in debitted job usage list
    *-------------------------------------------------------------*/

   old_usage_list.reset();

   if (job_usage_list) {
      const uint32_t jobnum = ocs::tom::get<JB_job_number>(job);
      const auto debited = user ? ocs::tom::get_or_create<UU_debited_job_usage>(user, "")
                                : ocs::tom::get_or_create<PR_debited_job_usage>(project, "");
      if (const auto upu = ocs::tom::find_first_rw<UPU_job_number>(debited, jobnum); upu) {
         ocs::tom::set<UPU_old_usage_list>(upu, job_usage_list->clone());
      } else {
         auto new_upu = ocs::tom::ElementPtr<ocs::tom::UPU>::create();
         ocs::tom::set<UPU_job_number>(new_upu, jobnum);
         ocs::tom::set<UPU_old_usage_list>(new_upu, job_usage_list->clone());
         debited.push_back(std::move(new_upu));
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Decay the already booked usage and add this job's scaled usage on top
 *
 * Accumulates and decays usage in the correct user and project objects for
 * the specified job.
 *
 * @param job the finished job
 * @param ja_task the finished array task
 * @param node the job owner's share tree node; unused
 * @param user the submitting user's object
 * @param project the job's project
 * @param decay_list the decay constants, one per usage attribute
 * @param usage_weight_list the weights each usage attribute is scaled by
 * @param seqno the current usage sequence number, so usage is decayed only once
 * @param curr_time now, in seconds, as the base of the decay
 */
void
ocs::Usage::decay_and_sum_usage(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                [[maybe_unused]] const ocs::tom::Element<ocs::tom::STN> node,
                                const ocs::tom::Element<ocs::tom::UU> user,
                                const ocs::tom::Element<ocs::tom::PR> project,
                                const ocs::tom::ConstList<ocs::tom::UA> decay_list,
                                const ocs::tom::ConstList<ocs::tom::UA> usage_weight_list, const u_long seqno,
                                const uint64_t curr_time) {
   DENTER(BASIS_LAYER);
   book_user_project_usage(job, ja_task, user, project, decay_list, usage_weight_list, seqno, curr_time, true);
   DRETURN_VOID;
}


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
void
ocs::Usage::sum_usage(const ocs::tom::ConstElement<ocs::tom::JB> job,
                      const ocs::tom::ConstElement<ocs::tom::JAT> ja_task, const ocs::tom::Element<ocs::tom::UU> user,
                      const ocs::tom::Element<ocs::tom::PR> project,
                      const ocs::tom::ConstList<ocs::tom::UA> usage_weight_list) {
   DENTER(BASIS_LAYER);
   book_user_project_usage(job, ja_task, user, project, nullptr, usage_weight_list, 0, 0, false);
   DRETURN_VOID;
}



/**
 * @brief Remove usage attributes that are irrelevant for share tree usage
 *
 * Older versions accumulated all online usage attributes (vmem, rss, all
 * `ru_*` and `acct_*` values, ...) into the user/project usage lists, which
 * made those objects and their spooled representation grow huge (CS-1385).
 * This strips a usage list down to the attributes that actually matter for
 * share tree usage.
 *
 * @param usage_list the list to strip (UA); an empty handle is treated as an empty list
 * @param usage_weight_list the weights (UA); an attribute without a weight is irrelevant
 * @return true when at least one element was removed, so the caller knows the
 *         owning object has to be re-spooled
 */
bool
ocs::Usage::strip_irrelevant_usage(const ocs::tom::List<ocs::tom::UA> usage_list,
                                   const ocs::tom::ConstList<ocs::tom::UA> usage_weight_list) {
   DENTER(BASIS_LAYER);

   bool removed = false;

   auto next_usage = usage_list.front();
   while (const auto usage = next_usage) {
      next_usage = usage.next();
      if (!usage_relevant_for_sharetree(ocs::tom::get<UA_name>(usage), usage_weight_list)) {
         usage_list.erase(usage);
         removed = true;
      }
   }
   DRETURN(removed);
}

/** @brief Build a named usage list, reusing the attributes of an existing one
 *
 * The values are zero. Without an existing list the list holds the default
 * attributes cpu, mem and io.
 *
 * @param name the name the resulting list is stored under
 * @param old_usage_list the list to take the existing attributes from (UA); may be an empty handle
 * @return the new list, owned by the caller
 */
ocs::tom::ListPtr<ocs::tom::UA>
ocs::Usage::build_usage_list(const char *name, const ocs::tom::ConstList<ocs::tom::UA> old_usage_list) {
   DENTER(BASIS_LAYER);

   ocs::tom::ListPtr<ocs::tom::UA> usage_list;

   if (old_usage_list) {

      /*-------------------------------------------------------------
       * Copy the old list and zero out the usage values
       *-------------------------------------------------------------*/

      usage_list = old_usage_list.clone(name);
      for (const auto usage : usage_list.get()) {
         ocs::tom::set<UA_value>(usage, 0);
      }

   } else {

      /*
       * the UA_value fields are implicitly set to 0 at creation
       * time of a new element
       */

      usage_list = ocs::tom::ListPtr<ocs::tom::UA>::create("");
      usage_list->emplace_back<UA_name>(USAGE_ATTR_CPU);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_MEM);
      usage_list->emplace_back<UA_name>(USAGE_ATTR_IO);
   }

   DRETURN(usage_list);
}

/** @brief Look up one usage attribute by name
 * @param usage_list the list to search (UA), may be an empty handle
 * @param name the attribute to find
 * @return the element, or an empty handle when the attribute is not booked
 */
ocs::tom::Element<ocs::tom::UA>
ocs::Usage::get_usage(const ocs::tom::List<ocs::tom::UA> usage_list, const char *name) {
   DENTER(BASIS_LAYER);
   const auto ret = ocs::tom::find_first_rw<UA_name>(usage_list, name);
   DRETURN(ret);
}

/** @brief Create a usage element for the named attribute, initialised to zero
 * @param name the attribute name
 * @return the new element, owned by the caller
 */
ocs::tom::ElementPtr<ocs::tom::UA>
ocs::Usage::create_usage_elem(const char *name) {
   DENTER(BASIS_LAYER);
   auto usage = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(usage, name);
   ocs::tom::set<UA_value>(usage, 0);
   DRETURN(usage);
}
