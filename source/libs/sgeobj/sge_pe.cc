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
 * @brief Parallel environments
 *
 * @see sge_pe.h
 */

#include <fnmatch.h>
#include <strings.h>
#include <cctype>
#include <limits>

#include "uti/config_file.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_PE.h"

#include "msg_common.h"
#include "msg_qmaster.h"

/**
 * @brief Does a parallel environment name match a request?
 *
 * A job may request a PE by wildcard, e.g. `-pe mpi* 4`.
 *
 * @param pe_name the defined PE's name
 * @param wildcard the pattern the job requested
 * @return true when the name matches
 */
bool pe_name_is_matching(const char *pe_name, const char *wildcard) {
   return fnmatch(wildcard, pe_name, 0) == 0 ? true : false;
}

/**
 * @brief Does Pe name match the wildcard?
 *
 * The function returns true (1) if the name of the given
 * "pe" matches the "wildcard".
 *
 * @param pe PE_Type element
 * @param wildcard wildcard
 *
 * @return true or false
 */
bool pe_is_matching(const ocs::tom::ConstElement<ocs::tom::PE> pe, const char *wildcard) {
   // no DENTER: called per job/queue in the scheduler
   return pe_name_is_matching(ocs::tom::get<PE_name>(pe), wildcard);
}

/**
 * @brief Find a PE matching  wildcard expr
 *
 * Try to find a PE that matches the given "wildcard" expression.
 *
 * @param pe_list PE_Type list
 * @param wildcard Wildcard expression
 *
 * @return the first matching PE_Type object or an empty handle
 */
ocs::tom::ConstElement<ocs::tom::PE> pe_list_find_matching(const ocs::tom::ConstList<ocs::tom::PE> pe_list,
                                                           const char *wildcard) {
   DENTER(TOP_LAYER);
   for (const auto pe : pe_list) {
      if (pe_is_matching(pe, wildcard)) {
         DRETURN(pe);
      }
   }
   DRETURN(nullptr);
}


/**
 * @brief Locate a certain PE
 *
 * Locate the PE with the name "pe_name".
 *
 * @param pe_list PE_Type list
 * @param pe_name PE name
 *
 * @return PE_Type object or an empty handle
 *
 * @note MT-NOTE: pe_list_locate() is MT safe
 */
ocs::tom::Element<ocs::tom::PE> pe_list_locate(const ocs::tom::ConstList<ocs::tom::PE> pe_list, const char *pe_name) {
   // no DENTER: called per job/queue in the scheduler
   return ocs::tom::find_first_rw<PE_name>(pe_list, pe_name);
}


/**
 * @brief Is a given PE referenced in other objects?
 *
 * This function returns true (1) if the given "pe" is referenced
 * in at least one of the objects contained in "master_job_list"
 * or "master_cqueue_list". If this is the case than
 * a corresponding message will be added to the "answer_list".
 *
 * @param pe PE_Type object
 * @param answer_list AN_Type list
 * @param master_job_list JB_Type list
 * @param master_cqueue_list CQ_Type list
 *
 * @return true or false
 */
bool pe_is_referenced(const ocs::tom::ConstElement<ocs::tom::PE> pe, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                      const ocs::tom::ConstList<ocs::tom::JB> master_job_list,
                      const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list) {
   DENTER(TOP_LAYER);
   bool ret = false;

   for (const auto job : master_job_list) {
      if (job_is_pe_referenced(job, pe)) {
         const char *pe_name = ocs::tom::get<PE_name>(pe);
         uint32_t job_id = ocs::tom::get<JB_job_number>(job);

         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_INFO, MSG_PEREFINJOB_SU,
                                 pe_name, job_id);
         ret = true;
         break;
      }
   }
   if (!ret) {
      /* fix for bug 6422335
       * check cq configuration for pe references instead of qinstances
       */
      const char *pe_name = ocs::tom::get<PE_name>(pe);

      for (const auto cqueue : master_cqueue_list) {
         for (const auto cpl : ocs::tom::get<CQ_pe_list>(cqueue)) {
            if (ocs::tom::find_first_in<ASTRLIST_value, ST_name>(cpl, pe_name))  {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_INFO,
                                       MSG_PEREFINQUEUE_SS,
                                       pe_name, ocs::tom::get<CQ_name>(cqueue));
               ret = true;
               break;
            }
         }
      }
   }
   DRETURN(ret);
}


/**
 * @brief Validate a parallel environment
 *
 * Ensures that a new pe is not a duplicate of an already existing one
 * and checks consistency of the parallel environment:
 *    - pseudo parameters in start and stop proc
 *    - validity of the allocation rule
 *    - correctness of the queue list, the user list and the xuser list
 *
 * @param pep the pe to check; `PE_start_proc_args` and `PE_stop_proc_args` set to "none" are nulled out
 * @param alpp answer list pointer, if an answer shall be created, else errors will in any case be output
 *             using the Cluster Scheduler error logging macros
 * @param startup are we in qmaster startup phase? Only then the user lists are checked.
 * @param master_userset_list the defined usersets the user lists are checked against (US_Type)
 *
 * @return STATUS_OK, if everything is ok, else other status values, see libs/gdi/sge_answer.h
 *
 * @note MT-NOTE: pe_validate() is not MT safe
 */
int pe_validate(const ocs::tom::Element<ocs::tom::PE> pep, ocs::tom::ListPtr<ocs::tom::AN> *alpp, const int startup,
                const ocs::tom::ConstList<ocs::tom::US> master_userset_list) {
   DENTER(TOP_LAYER);

   const char *s;
   int ret;

   const char *pe_name = ocs::tom::get<PE_name>(pep);
   if (pe_name != nullptr && verify_obj_name(alpp, pe_name, MAX_VERIFY_STRING, MSG_OBJ_PE) != STATUS_OK) {
      if (alpp == nullptr) {
         ERROR(MSG_PE_INVALIDCHARACTERINPE_S, pe_name);
      } else {
         answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                 MSG_PE_INVALIDCHARACTERINPE_S, pe_name);
      }
      DRETURN(STATUS_EEXIST);
   }

   /* register our error function for use in replace_params() */
   config_errfunc = set_error;

   /* -------- start_proc_args */
   ocs_null_out_none<PE_start_proc_args>(pep);
   s = ocs::tom::get<PE_start_proc_args>(pep);
   if (s != nullptr && replace_params(s, nullptr, 0, pe_variables)) {
      if (alpp == nullptr) {
         ERROR(MSG_PE_STARTPROCARGS_SS, pe_name, err_msg);
      } else {
         answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                 MSG_PE_STARTPROCARGS_SS, pe_name, err_msg);
      }
      DRETURN(STATUS_EEXIST);
   }

   /* -------- stop_proc_args */
   ocs_null_out_none<PE_stop_proc_args>(pep);
   s = ocs::tom::get<PE_stop_proc_args>(pep);
   if (s != nullptr && replace_params(s, nullptr, 0, pe_variables)) {
      if (alpp == nullptr) {
         ERROR(MSG_PE_STOPPROCARGS_SS, pe_name, err_msg);
      } else {
         answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                 MSG_PE_STOPPROCARGS_SS, pe_name, err_msg);
      }
      DRETURN(STATUS_EEXIST);
   }

   /* -------- slots */
   if ((ret = pe_validate_slots(alpp, ocs::tom::get<PE_slots>(pep), startup != 0)) != STATUS_OK) {
      DRETURN(ret);
   }

   /* -------- allocation_rule */
   s = ocs::tom::get<PE_allocation_rule>(pep);
   if (!pe_validate_allocation_rule(alpp, s, alpp == nullptr)) {
      DRETURN(STATUS_EEXIST);
   }

   /* do this only in qmaster. we don't have the usersets in qconf */
   if (startup) {
      /* -------- PE_user_list */
      if ((ret = userset_list_validate_acl_list(ocs::tom::get<PE_user_list>(pep), alpp,
                                                master_userset_list)) != STATUS_OK) {
         DRETURN(ret);
      }

      /* -------- PE_xuser_list */
      if ((ret = userset_list_validate_acl_list(ocs::tom::get<PE_xuser_list>(pep), alpp,
                                                master_userset_list)) != STATUS_OK) {
         DRETURN(ret);
      }
   }

   /* -------- PE_urgency_slots */
   if ((ret=pe_validate_urgency_slots(alpp, ocs::tom::get<PE_urgency_slots>(pep)))!=STATUS_OK) {
      DRETURN(ret);
   }

   DRETURN(STATUS_OK);
}


/**
 * @brief Ensure urgency slot setting is valid
 *
 * Validates the slots setting against #OCS_MAX_SLOTS (CS-3146). A parallel environment read from the spool
 * at startup may hold more, as the limit was not enforced between CS-173 and CS-3146: it is reported as a
 * warning then and kept, so qmaster still starts; a new or modified one is refused.
 *
 * @param alpp On error a context message is returned; nullptr to log it instead.
 * @param slots The slots value.
 * @param warn_only true at startup: report a value above the limit as a warning and accept it
 *
 * @return values other than STATUS_OK indicate error condition
 *
 * @note MT-NOTE: pe_validate_slots() is MT safe
 */
int pe_validate_slots(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const uint32_t slots, const bool warn_only) {
   DENTER(TOP_LAYER);

   if (slots > OCS_MAX_SLOTS) {
      const answer_quality_t quality = warn_only ? ANSWER_QUALITY_WARNING : ANSWER_QUALITY_ERROR;
      if (alpp == nullptr) {
         if (warn_only) {
            WARNING(MSG_ATTR_INVALID_ULONGVALUE_USUU, slots, "slots", static_cast<uint32_t>(0), OCS_MAX_SLOTS);
         } else {
            ERROR(MSG_ATTR_INVALID_ULONGVALUE_USUU, slots, "slots", static_cast<uint32_t>(0), OCS_MAX_SLOTS);
         }
      } else {
         answer_list_add_sprintf(alpp, warn_only ? STATUS_OK : STATUS_EEXIST, quality,
                                 MSG_ATTR_INVALID_ULONGVALUE_USUU, slots, "slots", static_cast<uint32_t>(0),
                                 OCS_MAX_SLOTS);
      }
      if (!warn_only) {
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   DRETURN(STATUS_OK);
}
/**
 * @brief Ensure urgency slot setting is valid
 *
 * Validates urgency slot setting.
 *
 * @param answer_list On error a context message is returned.
 * @param s The urgency slot string to be validated.
 *
 * @return values other than STATUS_OK indicate error condition
 *
 * @note MT-NOTE: pe_validate_urgency_slots() is MT safe
 */
int pe_validate_urgency_slots(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *s) {
   DENTER(TOP_LAYER);

   if (strcasecmp(s, SGE_ATTRVAL_MIN) &&
       strcasecmp(s, SGE_ATTRVAL_MAX) &&
       strcasecmp(s, SGE_ATTRVAL_AVG) &&
       !isdigit(s[0])) {
      if (answer_list == nullptr) {
         ERROR("rejecting invalid urgency_slots setting \"%s\"\n", s);
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR, MSG_PE_REJECTINGURGENCYSLOTS_S, s);
      }
      DRETURN(STATUS_ESEMANTIC);
   }

   DRETURN(STATUS_OK);
}

/**
 * @brief validate an allocation rule
 *
 * Validates an allocation rule, which can be configured in a parallel environment
 * or be passed as a parameter to the `-par` submit option.
 *
 * @param answer_list      - answer list used for reporting errors
 * @param allocation_rule  - the allocation rule to check
 * @param output_errors    - if true, then errors will be output via ERROR macro (possibly in addition to
 *                           adding them to an answer_list).
 * @return true if the allocation rule is valid, else false
 */
bool
pe_validate_allocation_rule(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *allocation_rule,
                            bool output_errors) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (allocation_rule == nullptr) {
      if (output_errors) {
         ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(PE_allocation_rule), "pe_validate_allocation_rule");
      }
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR, MSG_SGETEXT_MISSINGCULLFIELD_SS,
                              lNm2Str(PE_allocation_rule), "pe_validate_allocation_rule");
      ret = false;
   } else {
      // can be a number or one of the placeholders $pe_slots, $fill_up, $round_robin
      if (isdigit(allocation_rule[0])) {
         // check the fixed allocation rules
         int rule = atoi(allocation_rule);
         if (rule <= 0) {
            // negative and 0 are not allowed
            ret = false;
         } else {
            // disallow e.g., "4x"
            DSTRING_STATIC(dstr, 32);
            const char *rewritten_allocation_rule = sge_dstring_sprintf(&dstr, "%d", rule);
            if (strcmp(rewritten_allocation_rule, allocation_rule) != 0) {
               ret = false;
            }
         }
      } else {
         // check the named allocation rules
         if (strcmp(allocation_rule, "$pe_slots") != 0 && strcmp(allocation_rule, "$fill_up") != 0 &&
             strcmp(allocation_rule, "$round_robin") != 0) {
            ret = false;
         }
      }
      if (!ret) {
         if (output_errors) {
            ERROR(MSG_PE_ALLOCRULE_S, allocation_rule);
         }
         answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR, MSG_PE_ALLOCRULE_S,
                                 allocation_rule);
      }
   }

   DRETURN(ret);
}

/**
 * @brief Check if a list of PE's really exists
 *
 * Check if all PE's in "pe_ref_list" really exist in "pe_list".
 * If "ignore_make_pe" is 'true' than the test for the PE with the
 * name "make" will not be done.
 *
 * @param pe_list PE_Type list
 * @param answer_list AN_Type list
 * @param pe_ref_list ST_Type list of PE names
 * @param ignore_make_pe bool
 *
 * @return true if all PE's exist, false if at least one PE does not exist
 *
 * @note MT-NOTE: pe_list_do_all_exist() is MT safe
 */
bool pe_list_do_all_exist(const ocs::tom::ConstList<ocs::tom::PE> pe_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                          const ocs::tom::ConstList<ocs::tom::ST> pe_ref_list, const bool ignore_make_pe) {
   DENTER(TOP_LAYER);
   bool ret = true;

   for (const auto pe_ref_elem : pe_ref_list) {
      const char *pe_ref_string = ocs::tom::get<ST_name>(pe_ref_elem);

      if (ignore_make_pe && !strcmp(pe_ref_string, "make")) {
         continue;
      }
      if (!pe_list_locate(pe_list, pe_ref_string)) {
         answer_list_add_sprintf(answer_list, STATUS_EEXIST,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_PEREFDOESNOTEXIST_S, pe_ref_string);
         ret = false;
         break;
      }
   }
   DRETURN(ret);
}


/**
 * @brief Compute PEs urgency slot amount for a slot range
 *
 * Compute PEs urgency slot amount for a slot range. The urgency slot
 * amount is the amount that is assumed for a job with a slot range
 * before an assignment.
 *
 * @param pe PE_Type object.
 * @param urgency_slot_setting Ugency slot setting as in sge_pe(5)
 * @param range_list RN_Type list.
 *
 * @return The slot amount.
 *
 * @note MT-NOTE: pe_urgency_slots() is MT safe
 */
int pe_urgency_slots(const ocs::tom::ConstElement<ocs::tom::PE> pe, const char *urgency_slot_setting,
                     const ocs::tom::ConstList<ocs::tom::RN> range_list) {
   DENTER(TOP_LAYER);

   int n;

   if (!strcasecmp(urgency_slot_setting, SGE_ATTRVAL_MIN)) {
      n = range_list_get_first_id(range_list, nullptr);
   } else if (!strcasecmp(urgency_slot_setting, SGE_ATTRVAL_MAX)) {
      /*
       * in case of an infinity slot range we use the
       * maximum PE slot number instead
       */
      n = range_list_get_last_id(range_list, nullptr);
      if (n == RANGE_INFINITY) {
         n = ocs::tom::get<PE_slots>(pe);
      }
   } else if (!strcasecmp(urgency_slot_setting, SGE_ATTRVAL_AVG)) {
      /*
       * to handle infinity slot ranges we use the maximum PE
       * slot number as upper bound when determining the average
       */
      n = range_list_get_average(range_list, ocs::tom::get<PE_slots>(pe));
   } else if (isdigit(urgency_slot_setting[0])) {
      n = atoi(urgency_slot_setting);
   } else {
      CRITICAL(MSG_PE_UNKNOWN_URGENCY_SLOT_SS, urgency_slot_setting, ocs::tom::get<PE_name>(pe));
      n = 1;
   }
   DRETURN(n);
}

/**
 * @brief Build up a generic pe object
 *
 * build up a generic pe object
 *
 * @param pe_name name used for the PE_name attribute of the generic pe object. If nullptr then "template"
 *                is the default name.
 *
 * @return the new PE_Type object
 *
 * @note MT-NOTE: pe_create_template() is MT safe
 */
ocs::tom::ElementPtr<ocs::tom::PE> pe_create_template(const char *pe_name) {
   DENTER(TOP_LAYER);

   auto pep = ocs::tom::ElementPtr<ocs::tom::PE>::create();

   if (pe_name) {
      ocs::tom::set<PE_name>(pep, pe_name);
   } else {
      ocs::tom::set<PE_name>(pep, "template");
   }

   ocs::tom::set<PE_allocation_rule>(pep, "$pe_slots");
   ocs::tom::set<PE_start_proc_args>(pep, "NONE");
   ocs::tom::set<PE_stop_proc_args>(pep, "NONE");

   /* PE_control_slaves initialized implicitly to false */
   ocs::tom::set<PE_job_is_first_task>(pep, true);

   ocs::tom::set<PE_urgency_slots>(pep, SGE_ATTRVAL_MIN);

   DRETURN(pep);
}

/**
 * @brief Returns used PE slots
 *
 * Returns the number of currently used PE slots.
 *
 * @param pe The PE object (PE_Type)
 *
 * @return number of currently used PE slots or -1 on error
 *
 * @note MT-NOTE: pe_get_slots_used() is MT safe
 */
int pe_get_slots_used(const ocs::tom::ConstElement<ocs::tom::PE> pe) {
   // no DENTER: called per job/queue in the scheduler
   int ret = -1;
   const auto actual = ocs::tom::find_first_in<PE_resource_utilization, RUE_name>(pe, SGE_ATTR_SLOTS);

   if (actual) {
      ret = ocs::tom::get<RUE_utilized_now>(actual);
   }
   return ret;
}

/**
 * @brief Set number of used PE slots
 *
 * Sets the number of used PE slots.
 *
 * @param pe The pe object (PE_Type)
 * @param slots Number of slots.
 *
 * @return 0 on success -1 on error
 *
 * @note MT-NOTE: pe_set_slots_used() is MT safe
 */
int pe_set_slots_used(const ocs::tom::Element<ocs::tom::PE> pe, const int slots) {
   DENTER(TOP_LAYER);
   auto actual = ocs::tom::find_first_in_rw<PE_resource_utilization, RUE_name>(pe, SGE_ATTR_SLOTS);
   if (!actual && (!(actual = ocs::tom::emplace_back_in<PE_resource_utilization, RUE_name>(pe, SGE_ATTR_SLOTS)))) {
      DRETURN(-1);
   }
   ocs::tom::set<RUE_utilized_now>(actual, slots);
   DRETURN(0);
}



/**
 * @brief Debit pos/neg amount of slots from PE
 *
 * Increases or decreses the number of slots used with a PE.
 *
 * @param pep The PE (PE_Type)
 * @param slots Pos/neg number of slots.
 * @param job_id Job id for monitoring purposes.
 *
 * @note MT-NOTE: pe_debit_slots() is MT safe
 */
void pe_debit_slots(const ocs::tom::Element<ocs::tom::PE> pep, const int slots,
                    [[maybe_unused]] const uint32_t job_id) {
   DENTER(TOP_LAYER);

   if (pep) {
      int n = pe_get_slots_used(pep);
      n += slots;
      if (n < 0) {
         ERROR(MSG_PE_USEDSLOTSTOOBIG_S, ocs::tom::get<PE_name>(pep));
      }
      pe_set_slots_used(pep, n);
   }
   DRETURN_VOID;
}


/**
 * @brief Do accounting summary?
 *
 * Returns whether for tightly integrated jobs
 * - a single accounting record shall be created (true), or
 * - an individual accounting record shall be created for every task,
 *   and an accounting record for the master task (job script).
 *
 * @param pe the parallel environment
 *
 * @return true (do summary), or false (many records)
 *
 * @note MT-NOTE: pe_do_accounting_summary() is MT safe
 */
bool pe_do_accounting_summary(const ocs::tom::ConstElement<ocs::tom::PE> pe) {
   // no DENTER: called per job/queue in the scheduler
   bool ret = false;

   /*
    * the accounting_summary attribute has only a meaning
    * for tightly integrated jobs
    */
   if (pe && ocs::tom::get<PE_control_slaves>(pe)) {
      ret = ocs::tom::get<PE_accounting_summary>(pe) ? true : false;
   }

   return ret;
}

