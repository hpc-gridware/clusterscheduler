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
 * @brief Mirroring the array tasks of a job
 *
 * Array tasks are a sublist of their job, so an event about one is applied by
 * finding its job first.
 *
 * @see sge_ja_task_mirror.h
 * @see sge_mirror.h
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_UA.h"

#include "mir/sge_mirror.h"
#include "mir/msg_mirlib.h"
#include "mir/sge_ja_task_mirror.h"

/**
 * @brief Exchange a list field of an array task if the task holds the field
 *
 * An event client may subscribe a reduced array task (qevent asks for
 * JAT_status only); such a task has none of the lists kept across a
 * modify event, and there is nothing to exchange.
 *
 * Hot path: called twice per array task modify event, so without DENTER.
 *
 * @tparam F the list field, e.g. JAT_task_list
 * @param ja_task the array task
 * @param list the list to put in; the list held before afterwards
 */
template <auto F>
static void
ocs_ja_task_exchange_kept_list(const ocs::tom::Element<ocs::tom::JAT> ja_task,
                               ocs::tom::ListPtr<typename ocs::tom::FieldTraits<F>::Sub> &list) {
   if (ja_task.has<F>()) {
      list = ja_task.exchange<F>(std::move(list));
   }
}

/**
 * @brief Update an array tasks usage
 *
 * Updates the scaled usage of an array task (also task data structure
 * of a non array job).
 *
 * @param job_list master job list (JB)
 * @param event the event (ET) containing the new usage list; the list is exchanged with the task's
 *
 * @return SGE_EMA_OK, if the operation succeeds, else SGE_EMA_FAILURE
 *
 * @see `job_update_master_list_usage()`, `pe_task_update_master_list_usage()`
 */
sge_callback_result
ja_task_update_master_list_usage(const ocs::tom::List<ocs::tom::JB> job_list,
                                 const ocs::tom::Element<ocs::tom::ET> event) {
   DENTER(TOP_LAYER);

   const uint32_t job_id = event.get<ET_intkey>();
   const uint32_t ja_task_id = event.get<ET_intkey2>();

   const auto job = job_list.find_first<JB_job_number>(job_id);

   if (!job) {
      dstring id_dstring = DSTRING_INIT;
      ERROR(MSG_JOB_CANTFINDJOBFORUPDATEIN_SS, job_get_id_string(job_id, 0, nullptr, &id_dstring), __func__);
      sge_dstring_free(&id_dstring);
      DRETURN(SGE_EMA_FAILURE);
   }

   // an array task of the job (JAT), fetched with its fixed type
   const auto ja_task = ocs::tom::Element<ocs::tom::JAT>(job_search_task(job, nullptr, ja_task_id));
   if (!ja_task) {
      dstring id_dstring = DSTRING_INIT;
      ERROR(MSG_JOB_CANTFINDJATASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, nullptr, &id_dstring),
            __func__);
      sge_dstring_free(&id_dstring);
      DRETURN(SGE_EMA_FAILURE);
   }

   // a usage event carries the usage list (UA); the old usage goes back into the event and is freed with it
   auto usage = ocs_mirror_exchange_event_objects(event, ocs::tom::ListPtr<ocs::tom::UA>());
   usage = ja_task.exchange<JAT_scaled_usage_list>(std::move(usage));
   static_cast<void>(ocs_mirror_exchange_event_objects(event, std::move(usage)));

   DRETURN(SGE_EMA_OK);
}

/**
 * @brief Update array tasks of a job
 *
 * Update the list of array tasks of a job
 * based on an event.
 * The function is called from the event mirroring interface.
 * An array tasks list of parallel tasks and the
 * scaled usage list are not updated in this function,
 * as this data is maintained by separate events.
 *
 * @param evc the event client the event arrived on
 * @param type event type
 * @param action action to perform
 * @param event the event (ET); its array task is taken over into the job
 * @param clientdata client data
 *
 * @return SGE_EMA_OK if the update is successful, else SGE_EMA_FAILURE
 *
 * @note The function should only be called from the event mirror interface.
 *
 * @see `sge_mirror_update_master_list()`
 */
sge_callback_result
ja_task_update_master_list([[maybe_unused]] sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                           const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event,
                           [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   // the lists of the array task kept across a modify event
   ocs::tom::ListPtr<ocs::tom::PET> pe_tasks;
   ocs::tom::ListPtr<ocs::tom::UA> usage;

   char id_buffer[MAX_STRING_SIZE];
   dstring id_dstring;

   sge_dstring_init(&id_dstring, id_buffer, MAX_STRING_SIZE);

   const uint32_t job_id = event.get<ET_intkey>();
   const uint32_t ja_task_id = event.get<ET_intkey2>();

   const auto job = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>().find_first<JB_job_number>(job_id);
   if (!job) {
      ERROR(MSG_JOB_CANTFINDJOBFORUPDATEIN_SS, job_get_id_string(job_id, 0, nullptr, &id_dstring), __func__);
      DRETURN(SGE_EMA_FAILURE);
   }

   // an array task of the job (JAT), fetched with its fixed type
   auto ja_task = ocs::tom::Element<ocs::tom::JAT>(job_search_task(job, nullptr, ja_task_id));
   if (action == SGE_EMA_MOD) {
      /* modify event for ja_task.
       * we may not update
       * - JAT_task_list - it is maintained by PETASK events
       * - JAT_scaled_usage - it is maintained by JOB_USAGE events
       */
      if (!ja_task) {
         ERROR(MSG_JOB_CANTFINDJATASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, nullptr, &id_dstring),
               __func__);
         DRETURN(SGE_EMA_FAILURE);
      }

      ocs_ja_task_exchange_kept_list<JAT_task_list>(ja_task, pe_tasks);
      ocs_ja_task_exchange_kept_list<JAT_scaled_usage_list>(ja_task, usage);
   }

   /* If an array job is deleted, a delete event is sent for
    * each ja_task. If it is not yet enrolled,
    * sge_mirror_update_master_list will fail.
    * If it is not enrolled, but in the range list
    * for pending tasks, remove it from ranges.
    */
   if (action == SGE_EMA_DEL) {
      if (!ja_task &&
          job_is_ja_task_defined(job, ja_task_id) &&
          (!job_is_enrolled(job, ja_task_id))
         ) {
         job_delete_not_enrolled_ja_task(job, nullptr, ja_task_id);
         DRETURN(SGE_EMA_OK);
      }
   }

   // a list created for a job without one goes into JB_ja_tasks
   if (ocs_mirror_update_sub_list<JB_ja_tasks>(job, ja_task, job_get_id_string(job_id, ja_task_id, nullptr,
                                                                               &id_dstring),
                                               action, event) != SGE_EM_OK) {
      DRETURN(SGE_EMA_FAILURE);
   }

   /* restore pe_task list after modify event */
   if (action == SGE_EMA_MOD) {
      /* we have to search the replaced ja_task */
      ja_task = ocs::tom::Element<ocs::tom::JAT>(job_search_task(job, nullptr, ja_task_id));
      if (!ja_task) {
         ERROR(MSG_JOB_CANTFINDJATASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, nullptr, &id_dstring),
               __func__);
         DRETURN(SGE_EMA_FAILURE);
      }

      // the lists the event brought in come back and are freed with the handles
      ocs_ja_task_exchange_kept_list<JAT_task_list>(ja_task, pe_tasks);
      ocs_ja_task_exchange_kept_list<JAT_scaled_usage_list>(ja_task, usage);
      pe_tasks.reset();
      usage.reset();
   }

   if (action == SGE_EMA_ADD) {
      /* we must enroll the task to have it removed in the pending range list */
      job_enroll(job, nullptr, ja_task_id);
   }

   DRETURN(SGE_EMA_OK);
}
