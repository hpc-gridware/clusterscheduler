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
 * @brief Checkpointing environments
 *
 * @see sge_ckpt.h
 */

#include <strings.h>
#include <cctype>

#include "uti/config_file.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "tom/ocs_Element.h"

#include "symbols.h"
#include "msg_common.h"

/**
 * @brief Is a given CKPT referenced in other objects?
 *
 * This function returns true if the given "ckpt" is referenced
 * in at least one of the objects contained in "master_job_list" or
 * "master_cqueue_list". If this is the case than
 * a corresponding message will be added to the "answer_list".
 *
 * @param ckpt CK object
 * @param answer_list AN list
 * @param master_job_list JB list
 * @param master_cqueue_list CQ list
 *
 * @return true or false
 */
bool ckpt_is_referenced(const ocs::tom::ConstElement<ocs::tom::CK> ckpt, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const ocs::tom::ConstList<ocs::tom::JB> master_job_list,
                        const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list) {
   DENTER(TOP_LAYER);
   bool ret = false;

   for (const auto job : master_job_list) {
      if (job_is_ckpt_referenced(job, ckpt)) {
         const char *ckpt_name = ckpt.get<CK_name>();
         uint32_t job_id = job.get<JB_job_number>();

         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_INFO, MSG_CKPTREFINJOB_SU,
                                 ckpt_name, job_id);
         ret = true;
         break;
      }
   }
   if (!ret) {
      /* fix for bug 6422335
       * check the cq configuration for ckpt references instead of qinstances
       */
      const char *ckpt_name = ckpt.get<CK_name>();

      for (const auto queue : master_cqueue_list) {
         for (const auto ckl : queue.get<CQ_ckpt_list>()) {
            if (ckl.find_first_in<ASTRLIST_value, ST_name>(ckpt_name))  {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_INFO, 
                                       MSG_CKPTREFINQUEUE_SS,
                                       ckpt_name, queue.get<CQ_name>());
               ret = true;
               break;
            }
         }
      }
   }
   DRETURN(ret);
}


/**
 * @brief Find a ckpt object in a list
 *
 * This function will return a ckpt object by name if it exists.
 *
 * @param ckpt_list CK list
 * @param ckpt_name name of the ckpt object.
 *
 * @return the ckpt object (CK) named "ckpt_name", or an empty handle if it does not exist
 */
ocs::tom::Element<ocs::tom::CK> ckpt_list_locate(const ocs::tom::ConstList<ocs::tom::CK> ckpt_list,
                                                 const char *ckpt_name) {
   DENTER(TOP_LAYER);
   DRETURN(ocs::tom::find_first_rw<CK_name>(ckpt_list, ckpt_name));
}


/**
 * @brief Make "when" bitmask from string
 *
 * Parse checkpoint "when" string and return a bitmask.
 *
 * @param attr_str when string
 *
 * @return bitmask of checkpoint specifers 0 if attr_str == nullptr or nothing set or value may be a time value
 *
 * @note MT-NOTE: sge_parse_checkpoint_attr() is MT safe
 */
int sge_parse_checkpoint_attr(const char *attr_str) {
   int opr;

   if (attr_str == nullptr) {
      return 0;
   }

   /* May be it's a time value */
   if (isdigit((int) *attr_str) || (*attr_str == ':')) {
      return 0;
   }

   opr = 0;
   while (*attr_str) {
      if (*attr_str == CHECKPOINT_AT_MINIMUM_INTERVAL_SYM)
         opr = opr | CHECKPOINT_AT_MINIMUM_INTERVAL;
      else if (*attr_str == CHECKPOINT_AT_SHUTDOWN_SYM)
         opr = opr | CHECKPOINT_AT_SHUTDOWN;
      else if (*attr_str == CHECKPOINT_SUSPEND_SYM)
         opr = opr | CHECKPOINT_SUSPEND;
      else if (*attr_str == NO_CHECKPOINT_SYM)
         opr = opr | NO_CHECKPOINT;
      else if (*attr_str == CHECKPOINT_AT_AUTO_RES_SYM)
         opr = opr | CHECKPOINT_AT_AUTO_RES;
      else {
         opr = -1;
         break;
      }
      attr_str++;
   }

   return opr;
}

/**
 * @brief Validate all ckpt interface parameters
 *
 * This function will test all ckpt interface parameters.
 * If all are valid then it will return success.
 *
 * @param this_elem element which should be verified
 * @param alpp answer list where the function stores error messages
 *
 * @return STATUS_OK - success, STATUS_EUNKNOWN or STATUS_EEXIST - error
 *
 * @note MT-NOTE: ckpt_validate() is not MT safe
 */
int ckpt_validate(const ocs::tom::ConstElement<ocs::tom::CK> this_elem, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   static const char* ckpt_interfaces[] = {
      "USERDEFINED",
      "HIBERNATOR",
      "TRANSPARENT",
      "APPLICATION-LEVEL",
      "CPR",
      "CRAY-CKPT"
   };
   size_t i;
   int found = 0;
   const char *s, *interface;

   if (!this_elem) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* -------- CK_name */
   if (verify_obj_name(
         alpp, this_elem.get<CK_name>(),
         MAX_VERIFY_STRING, "checkpoint interface") != STATUS_OK) {
      DRETURN(STATUS_EUNKNOWN);
   }

   /*
    * check if ckpt obj can be added
    * check allowed interfaces and license
    */
   interface = this_elem.get<CK_interface>();
   found = 0;
   /* handle nullptr pointer for interface name */
   if (interface == nullptr) {
      interface = "<null>";
   } else {

      for (i=0; i < (sizeof(ckpt_interfaces)/sizeof(char*)); i++) {
         if (!strcasecmp(interface, ckpt_interfaces[i])) {
            found = 1;
            break;
         }
      }
   }

   if (!found) {
      ERROR(MSG_SGETEXT_NO_INTERFACE_S, interface);
      answer_list_add(alpp, SGE_EVENT, 
                      STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   // the command fields and their attribute names, checked in this order
   const struct {
      const char *value;
      const char *text;
   } ckpt_commands[] = {
      { this_elem.get<CK_ckpt_command>(), "ckpt_command" },
      { this_elem.get<CK_migr_command>(), "migr_command" },
      { this_elem.get<CK_rest_command>(), "restart_command" },
      { this_elem.get<CK_clean_command>(), "clean_command" }
   };
   for (const auto &command : ckpt_commands) {
      if (replace_params(command.value, nullptr, 0, ckpt_variables)) {
         ERROR(MSG_OBJ_CKPTENV_SSS, command.text, this_elem.get<CK_name>(), err_msg);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   }

   /* -------- CK_signal */
   if ((s=this_elem.get<CK_signal>()) &&
         strcasecmp(s, "none") &&
         sge_sys_str2signal(s)==SIGUNKNOWN) {
      ERROR(MSG_CKPT_XISNOTASIGNALSTRING_S , s);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   DRETURN(STATUS_OK);
}


/**
 * @brief Do all ckpt's exist?
 *
 * Test if the checkpointing objects whose name is contained in
 * "ckpt_ref_list" is contained in "ckpt_list".
 *
 * @param ckpt_list CK list
 * @param answer_list AN list
 * @param ckpt_ref_list ST list containing ckpt names
 *
 * @return true if all ckpt objects exist
 */
bool ckpt_list_do_all_exist(const ocs::tom::ConstList<ocs::tom::CK> ckpt_list,
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            const ocs::tom::ConstList<ocs::tom::ST> ckpt_ref_list) {
   DENTER(TOP_LAYER);
   bool ret = true;

   for (const auto ckpt_ref_elem : ckpt_ref_list) {
      const char *ckpt_ref_string = ckpt_ref_elem.get<ST_name>();

      if (!ckpt_list_locate(ckpt_list, ckpt_ref_string)) {
         answer_list_add_sprintf(answer_list, STATUS_EEXIST,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_CKPTREFDOESNOTEXIST_S, ckpt_ref_string);
         ret = false;
         break;
      }
   }
   DRETURN(ret);
}


/**
 * @brief Build up a generic ckpt object
 *
 * @param ckpt_name name used for the CK_name attribute of the generic ckpt object. If nullptr then "template"
 *                  is the default name.
 *
 * @return the new CK object
 */
ocs::tom::ElementPtr<ocs::tom::CK> sge_generic_ckpt(const char *ckpt_name) {
   DENTER(TOP_LAYER);

   auto ep = ocs::tom::ElementPtr<ocs::tom::CK>::create();

   if (ckpt_name)
      ep->set<CK_name>(ckpt_name);
   else
      ep->set<CK_name>("template");

   ep->set<CK_interface>("userdefined");
   ep->set<CK_ckpt_command>(NONE_STR);
   ep->set<CK_migr_command>(NONE_STR);
   ep->set<CK_rest_command>(NONE_STR);
   ep->set<CK_clean_command>(NONE_STR);
   ep->set<CK_ckpt_dir>("/tmp");
   ep->set<CK_when>("sx");
   ep->set<CK_signal>(NONE_STR);
   ep->set<CK_job_pid>(0);

   DRETURN(ep);
}
