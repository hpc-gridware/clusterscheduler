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
 * @brief Verifying one cluster queue attribute each
 *
 * Every function here is the `verify` member of an entry of
 * #ocs_cqueue_attributes; qmaster calls the one belonging to an attribute
 * before it accepts a changed cluster queue. Each takes the entry of the
 * attribute list typed as that attribute's type, and the master list it checks
 * against, if any, typed as well; the table entry names the master list's type.
 *
 * @see sge_cqueue_verify.h
 */

#include <cstring>
#include <limits>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_cqueue_verify.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"


/// Debug layer the cluster queue verification traces are written to
#define CQUEUE_VERIFY_LAYER TOP_LAYER

/**
 * @brief Verify that the calendar a queue refers to exists
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @param master_calendar_list the defined calendars (CAL)
 * @return true when the value is acceptable
 */
bool cqueue_verify_calendar(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            const ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem,
                            const ocs::tom::ConstList<ocs::tom::CAL> master_calendar_list) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      const char *name = ocs::tom::get<ASTR_value>(attr_elem);

      if (name != nullptr && strcasecmp("none", name)) {
         const auto calendar = ocs::tom::find_first<CAL_name>(master_calendar_list, name);
         if (!calendar) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_UNKNOWNCALENDAR_S, name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = false;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Verify that every checkpointing environment a queue refers to exists
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @param master_ckpt_list the defined checkpointing environments
 * @return true when the value is acceptable
 */
bool cqueue_verify_ckpt_list(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                             const ocs::tom::ConstElement<ocs::tom::ASTRLIST> attr_elem,
                             const ocs::tom::ConstList<ocs::tom::CK> master_ckpt_list) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      ocs::tom::ConstList<ocs::tom::ST> ckpt_list = ocs::tom::get<ASTRLIST_value>(attr_elem);

      if (ckpt_list != nullptr) {
         if (!ckpt_list_do_all_exist(master_ckpt_list, answer_list, ckpt_list)) {
            ret = false;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Verify that every resource in the complex values is defined and its value fits its type
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @param master_centry_list the defined complex entries
 * @return true when the value is acceptable
 */
bool cqueue_verify_consumable_config_list(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                          const ocs::tom::ConstElement<ocs::tom::ACELIST> attr_elem,
                                          const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   DENTER(CQUEUE_VERIFY_LAYER);

   // early exit if there is nothing to do
   if (!cqueue || !attr_elem) {
      DRETURN(true);
   }

   // consumable list empty, then all is fine
   ocs::tom::ConstList<ocs::tom::CE> centry_list = ocs::tom::get<ACELIST_value>(attr_elem);
   if (centry_list == nullptr) {
      DRETURN(true);
   }

   // reject duplicates in the consumable list
   if (ocs::CEntry::has_duplicates(centry_list, answer_list, "complex_values")) {
      DRETURN(false);
   }

   // check that all centries in the consumable list exist in the master centry list
   if (!centry_list_do_all_exists(master_centry_list, answer_list, centry_list)) {
      DRETURN(false);
   }
   DRETURN(true);
}

/**
 * @brief Verify that the initial state is one of the accepted words
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @return true when the value is acceptable
 */
bool cqueue_verify_initial_state(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 const ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      const char *names[] = {"default", "enabled", "disabled", nullptr};
      const char *name = ocs::tom::get<ASTR_value>(attr_elem);
      bool found = false;
      int i = 0;

      while (names[i] != nullptr) {
         if (!strcasecmp(name, names[i])) {
            found = true;
         }
         i++;
      }
      if (!found) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_UNKNOWNINITSTATE_S, name);
         answer_list_add(answer_list, SGE_EVENT,
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Verify that every parallel environment a queue refers to exists
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @param master_pe_list the defined parallel environments
 * @return true when the value is acceptable
 */
bool cqueue_verify_pe_list(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           const ocs::tom::ConstElement<ocs::tom::ASTRLIST> attr_elem,
                           const ocs::tom::ConstList<ocs::tom::PE> master_pe_list) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      ocs::tom::ConstList<ocs::tom::ST> pe_list = ocs::tom::get<ASTRLIST_value>(attr_elem);

      if (pe_list != nullptr) {
         if (!pe_list_do_all_exist(master_pe_list, answer_list, pe_list, true)) {
            ret = false;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Verify that the priority is within the range a nice value allows
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @return true when the value is acceptable
 */
bool cqueue_verify_priority(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            const ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      const char *priority_string = ocs::tom::get<ASTR_value>(attr_elem);

      if (priority_string != nullptr) {
         const int priority = atoi(priority_string);

         if (priority == 0 && priority_string[0] != '0') {
            answer_list_add(answer_list, MSG_CQUEUE_WRONGCHARINPRIO, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = false;
         } else if (priority < -20 || priority > 20 ) {
            answer_list_add(answer_list, MSG_CQUEUE_PRIORITYNOTINRANGE, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = false;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Verify that the processor set specification is well formed
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @return true when the value is acceptable
 */
bool cqueue_verify_processors(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                              const ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      const char *processors_string = ocs::tom::get<ASTR_value>(attr_elem);

      if (processors_string != nullptr) {
         ocs::tom::ListPtr<ocs::tom::RN> range_list;   // stays empty: the string is only parsed

         range_list_parse_from_string(range_list, answer_list, processors_string, JUST_PARSE, false, INF_ALLOWED);
         if (*answer_list) {
            ret = false;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Verify that every project a queue refers to exists
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @param master_project_list the defined projects
 * @return true when the value is acceptable
 */
bool cqueue_verify_project_list(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                const ocs::tom::ConstElement<ocs::tom::APRJLIST> attr_elem,
                                const ocs::tom::ConstList<ocs::tom::PR> master_project_list) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      ocs::tom::ConstList<ocs::tom::PR> project_list = ocs::tom::get<APRJLIST_value>(attr_elem);

      if (project_list != nullptr) {
         if (!prj_list_do_all_exist(master_project_list, answer_list,
                                    project_list)) {
            ret = false;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Verify that the shell start mode is one of the accepted words
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @return true when the value is acceptable
 */
bool cqueue_verify_shell_start_mode(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    const ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      const char *names[] = {
         "unix_behavior", "posix_compliant", "script_from_stdin",
         nullptr
      };
      const char *name = ocs::tom::get<ASTR_value>(attr_elem);
      bool found = false;
      int i = 0;

      while (names[i] != nullptr) {
         if (!strcasecmp(name, names[i])) {
            found = true;
         }
         i++;
      }
      if (!found) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_UNKNOWNSTARTMODE_S, name);
         answer_list_add(answer_list, SGE_EVENT,
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }
   DRETURN(ret);
}
/**
 * @brief Verify that the login shell is given as an absolute path
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @return true when the value is acceptable
 */
bool cqueue_verify_shell(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                         const ocs::tom::ConstElement<ocs::tom::ASTR> attr_elem) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;
   bool path_found = true;

   const char *name = ocs::tom::get<ASTR_value>(attr_elem);

   /* Check also if it is an absolute valid path */
   path_found = path_verify(name, answer_list, "shell", true);

   if (!path_found) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_UNKNOWNSHELL_S, name);
      answer_list_add(answer_list, SGE_EVENT,
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   DRETURN(ret);
}

/**
 * @brief Verify that every subordinate relation names an existing cluster queue
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @param master_cqueue_list the defined cluster queues (CQ)
 * @return true when the value is acceptable
 */
bool cqueue_verify_subordinate_list(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    const ocs::tom::ConstElement<ocs::tom::ASOLIST> attr_elem,
                                    const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      const char *cqueue_name = ocs::tom::get<CQ_name>(cqueue);
      const auto so_list = ocs::tom::get<ASOLIST_value>(attr_elem);

      for (const auto so : so_list) {
         const char *so_name = ocs::tom::get<SO_name>(so);
 
         /*
          * Check for recursions to ourself
          */
         if (strcmp(cqueue_name, so_name) != 0) {
            /*
             * Check if cqueue exists
             */
            const auto lcqueue = cqueue_list_locate(master_cqueue_list, so_name);
            if (lcqueue) {
               /*
                * Success
                */
               ;
            } else {
               ERROR(MSG_CQUEUE_UNKNOWNSUB_SS, so_name, cqueue_name);
               answer_list_add(answer_list, SGE_EVENT,
                               STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         } else {
            ERROR(MSG_CQUEUE_SUBITSELF_S, cqueue_name);
            answer_list_add(answer_list, SGE_EVENT,
                            STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = false;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Verify that every userset a queue refers to exists
 *
 * The `verify` member of the attribute's entry of #ocs_cqueue_attributes;
 * qmaster calls it for the attribute before accepting a changed cluster queue.
 *
 * @param cqueue the cluster queue being verified
 * @param[out] answer_list receives the reason the value was rejected
 * @param attr_elem the attribute value to verify
 * @param master_userset_list the defined usersets
 * @return true when the value is acceptable
 */
bool cqueue_verify_user_list(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                             const ocs::tom::ConstElement<ocs::tom::AUSRLIST> attr_elem,
                             const ocs::tom::ConstList<ocs::tom::US> master_userset_list) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      ocs::tom::ConstList<ocs::tom::US> user_list = ocs::tom::get<AUSRLIST_value>(attr_elem);

      if (user_list != nullptr) {
         if (userset_list_validate_acl_list(user_list, answer_list,
                                            master_userset_list) ==
             STATUS_EUNKNOWN) {
            ret = false;
         }
      }
   }
   DRETURN(ret);
}


/**
 * @brief Verify the queue slots attribute
 *
 * Verifies if the slots attribute of a queue is in the expected range
 * (0 .. #OCS_MAX_SLOTS). The limit is its own constant since CS-3146: CS-173 raised the job and AR id
 * limit it shared before, which silently lifted the slots limit too.
 *
 * @param cqueue The queue to verify.
 * @param answer_list answer list to report errors
 * @param attr_elem the attribute to verify
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: cqueue_verify_job_slots() is MT safe
 */
bool cqueue_verify_job_slots(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                             const ocs::tom::ConstElement<ocs::tom::AULNG> attr_elem) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      uint32_t slots = ocs::tom::get<AULNG_value>(attr_elem);

      if (slots > OCS_MAX_SLOTS) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_ATTR_INVALID_ULONGVALUE_USUU, slots, "slots",
                                 static_cast<uint32_t>(0), OCS_MAX_SLOTS);
         ret = false;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Verify a queue memory attribute like h_vmem
 *
 * Verifies if a memory attribute of a queue is in the expected range
 * (0 .. INFINITY) NONE is no allowed value.
 *
 * @param cqueue The queue to verify.
 * @param answer_list answer list to report errors
 * @param attr_elem the attribute to verify (AMEM)
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: cqueue_verify_memory_value() is MT safe
 */
bool cqueue_verify_memory_value(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                const ocs::tom::ConstElement<ocs::tom::AMEM> attr_elem) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      const char *memory_string = ocs::tom::get<AMEM_value>(attr_elem);

      // the check parsing AMEM_value does, without storing the text
      if (!ocs_object_check_value_string(answer_list, ocs::CEntry::Type::MEM, memory_string)) {
         ret = false;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Verify a queue time attribute like h_cpu
 *
 * Verifies if a time attribute of a queue is in the expected range
 * (0:0:0 .. INFINITY) NONE is no allowed value.
 *
 * @param cqueue The queue to verify.
 * @param answer_list answer list to report errors
 * @param attr_elem the attribute to verify
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: cqueue_verify_time_value() is MT safe
 */
bool cqueue_verify_time_value(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                              const ocs::tom::ConstElement<ocs::tom::ATIME> attr_elem) {
   DENTER(CQUEUE_VERIFY_LAYER);

   bool ret = true;

   if (cqueue && attr_elem) {
      const char *time_string = ocs::tom::get<ATIME_value>(attr_elem);

      if (time_string == nullptr || !strcasecmp(time_string, "none")) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                         MSG_NONE_NOT_ALLOWED_S, "time values");
         ret = false;
      }
   }
   DRETURN(ret);
}
