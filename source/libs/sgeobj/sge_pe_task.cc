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
 * @brief Tasks of a tightly integrated parallel job
 *
 * A parallel job may run many short tasks. Keeping one element per finished
 * task would grow the job without bound, so their usage is summed into one
 * container - see #PE_TASK_PAST_USAGE_CONTAINER.
 *
 * @see sge_pe_task.h
 */

#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_PA.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_PETR.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_VA.h"

#include "msg_common.h"

/**
 * @brief Sum up pe tasks past usage
 *
 * The pe task list of a ja task can contain one container
 * element to hold the usage of finished pe tasks no longer
 * stored in the task list.
 * This function adds the usage of pe_task to the usage of container.
 *
 * @param container container object to hold past usage (PET_Type), may be an empty handle
 * @param pe_task the pe task from which to copy usage (PET_Type), may be an empty handle
 *
 * @return the container object, an empty handle if one of the arguments is empty
 *
 * @see #pe_task_sum_past_usage_all, #pe_task_sum_past_usage_list
 */
ocs::tom::Element<ocs::tom::PET> pe_task_sum_past_usage(const ocs::tom::Element<ocs::tom::PET> container,
                                                        const ocs::tom::ConstElement<ocs::tom::PET> pe_task) {
   DENTER(TOP_LAYER);

   /* invalid input - nothing to do */
   if (!container || !pe_task) {
      DRETURN(nullptr);
   }

   if (container.raw() == pe_task.raw()) {
      /* container and pe_task are the same element - nothing to do */
      DRETURN(container);
   }

   /* we sum up the scaled_usage */
   const auto container_usage = ocs::tom::get_or_create<PET_scaled_usage>(container, "reported_usage");
   if (const auto pe_task_usage = ocs::tom::get<PET_scaled_usage>(pe_task); pe_task_usage) {
      usage_list_sum(container_usage, pe_task_usage);
   }

   /*
    * and we sum up the already reported usage
    * (for long running jobs having intermediate usage records)
    */
   const auto container_reported_usage = ocs::tom::get_or_create<PET_reported_usage>(container, "reported_usage");
   if (const auto pe_task_reported_usage = ocs::tom::get<PET_reported_usage>(pe_task); pe_task_reported_usage) {
      usage_list_sum(container_reported_usage, pe_task_reported_usage);
   }

   DRETURN(container);
}


/**
 * @brief Sum up all pe tasks past usage
 *
 * Similar to pe_task_sum_past_usage, but will sum up the usage of
 * all pe tasks in a task list to the container object in this list.
 * If the container object does not yet exist, it will be created.
 *
 * @param pe_task_list the pe task list to process (PET_Type), may be an empty handle
 *
 * @return the container object, an empty handle if the list is empty
 *
 * @see #pe_task_sum_past_usage, #pe_task_sum_past_usage_list
 */
ocs::tom::Element<ocs::tom::PET> pe_task_sum_past_usage_all(const ocs::tom::List<ocs::tom::PET> pe_task_list) {
   DENTER(TOP_LAYER);
   ocs::tom::Element<ocs::tom::PET> container = nullptr;

   /* no pe task list - nothing to do */
   if (!pe_task_list) {
      DRETURN(nullptr);
   }

   /* loop over all pe tasks and sum up usage */
   for (const auto pe_task : pe_task_list) {
      if (!container) {
         container = pe_task_sum_past_usage_list(pe_task_list, pe_task);
      } else {
         pe_task_sum_past_usage(container, pe_task);
      }
   }

   DRETURN(container);
}

/**
 * @brief Sum up pe tasks past usage
 *
 * Similar to pe_task_sum_past_usage.
 * The container is retrieved from pe_task_list, if it does not
 * yet exist it is created and inserted into pe_task_list as
 * first element.
 *
 * @param pe_task_list list containing the container object (PET_Type), may be an empty handle
 * @param pe_task the pe task from which to copy usage (PET_Type)
 *
 * @return the container object, an empty handle if the list is empty
 *
 * @see #pe_task_sum_past_usage, #pe_task_sum_past_usage_all
 */
ocs::tom::Element<ocs::tom::PET> pe_task_sum_past_usage_list(const ocs::tom::List<ocs::tom::PET> pe_task_list,
                                                             const ocs::tom::ConstElement<ocs::tom::PET> pe_task) {
   DENTER(TOP_LAYER);

   /* no pe task list - nothing to do */
   if (!pe_task_list) {
      DRETURN(nullptr);
   }

   /* get container - if it does not yet exist, create it as first element in pe task list */
   auto container = ocs::tom::find_first_rw<PET_id>(pe_task_list, PE_TASK_PAST_USAGE_CONTAINER);
   if (!container) {
      auto created = ocs::tom::ElementPtr<ocs::tom::PET>::create();
      ocs::tom::set<PET_id>(*created, PE_TASK_PAST_USAGE_CONTAINER);
      ocs::tom::set<PET_do_contact>(*created, true);
      container = pe_task_list.push_front(std::move(created));
   }

   /* sum up usage */
   pe_task_sum_past_usage(container, pe_task);

   DRETURN(container);
}


/**
 * @brief Verify a pe task request object
 *
 * Verifies structure and contents of a pe task request.
 * A pe task request is sent to sge_execd by qrsh -inherit.
 *
 * @param petr the object to verify (PETR_Type), may be an empty handle
 * @param answer_list answer list to pass back error messages
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: pe_task_verify_request() is MT safe
 *
 * @bug The function is far from being complete.
 *      Currently, only the CULL structure is verified, not the contents.
 *
 * @see `object_verify_cull()`
 */
bool
pe_task_verify_request(const ocs::tom::ConstElement<ocs::tom::PETR> petr,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (!petr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_NULLELEMENTPASSEDTO_S, __func__);
      ret = false;
   }

   /* 
    * A pe task request entering execd must have some additional properties: 
    *    - PETR_jobid > 0
    *    - PETR_jataskid > 0
    *    - PETR_owner != nullptr
    *    - verify PETR_queuename, if != nullptr
    *    - verify PETR_cwd, if it is != nullptr
    *    - verify PETR_path_aliases, if they are != nullptr
    *    - verify PETR_environment, if it is != nullptr
    *    - PETR_submission_time
    */

   if (ret) {
      ret = object_verify_ulong_not_null<PETR_jobid>(petr, answer_list);
   }

   if (ret) {
      ret = object_verify_ulong_not_null<PETR_jataskid>(petr, answer_list);
   }

   if (ret) {
      ret = object_verify_string_not_null<PETR_owner>(petr, answer_list);
   }

   if (ret) {
      const char *queue_name = ocs::tom::get<PETR_queuename>(petr);

      if (queue_name != nullptr) {
         if (verify_str_key(
               answer_list, queue_name, MAX_VERIFY_STRING,
               lNm2Str(PETR_queuename), KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   if (ret) {
      const char *cwd = ocs::tom::get<PETR_cwd>(petr);

      if (cwd != nullptr) {
         ret = path_verify(cwd, answer_list, "cwd", true);
      }
   }

   if (ret) {
      ocs::tom::ConstList<ocs::tom::PA> path_aliases = ocs::tom::get<PETR_path_aliases>(petr);

      if (path_aliases != nullptr) {
         ret = path_alias_verify(path_aliases, answer_list);
      }
   } 

   if (ret) {
      ocs::tom::ConstList<ocs::tom::VA> env_list = ocs::tom::get<PETR_environment>(petr);

      if (env_list != nullptr) {
         ret = var_list_verify(env_list, answer_list);
      }
   } 

   DRETURN(ret);
}
