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
 * @brief Mirroring the tasks of a parallel job
 *
 * Parallel tasks are a sublist of an array task, one level deeper than
 * sge_ja_task_mirror.cc.
 *
 * @see sge_pe_task_mirror.h
 * @see sge_mirror.h
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_UA.h"

#include "mir/sge_mirror.h"
#include "mir/msg_mirlib.h"
#include "mir/sge_pe_task_mirror.h"

/**
 * @brief Update a parallel tasks usage
 *
 * Updates the scaled usage of a parallel task.
 *
 * @param job_list the master job list (JB); not used, the job is looked up in the master list of the
 *        data store, which is the same list
 * @param event the event (ET) containing the new usage list; the list is exchanged with the task's
 *
 * @return SGE_EMA_OK, if the operation succeeds, else SGE_EMA_FAILURE
 *
 * @see `job_update_master_list_usage()`, `ja_task_update_master_list_usage()`
 */
sge_callback_result
pe_task_update_master_list_usage([[maybe_unused]] const ocs::tom::List<ocs::tom::JB> job_list,
                                 const ocs::tom::Element<ocs::tom::ET> event) {
   DENTER(TOP_LAYER);

   const uint32_t job_id = event.get<ET_intkey>();
   const uint32_t ja_task_id = event.get<ET_intkey2>();
   const char *pe_task_id = event.get<ET_strkey>();

   const auto job = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>().find_first<JB_job_number>(job_id);
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

   // a parallel task of the array task (PET), fetched with its fixed type
   const auto pe_task = ocs::tom::Element<ocs::tom::PET>(ja_task_search_pe_task(ja_task, pe_task_id));
   if (!pe_task) {
      dstring id_dstring = DSTRING_INIT;
      ERROR(MSG_JOB_CANTFINDPETASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring),
            __func__);
      sge_dstring_free(&id_dstring);
      DRETURN(SGE_EMA_FAILURE);
   }

   // a usage event carries the usage list (UA); the old usage goes back into the event and is freed with it
   auto usage = ocs_mirror_exchange_event_objects(event, ocs::tom::ListPtr<ocs::tom::UA>());
   usage = pe_task.exchange<PET_scaled_usage>(std::move(usage));
   static_cast<void>(ocs_mirror_exchange_event_objects(event, std::move(usage)));
   
   DRETURN(SGE_EMA_OK);
}

/**
 * @brief Update parallel tasks of an array task
 *
 * Update the list of parallel tasks of an array task
 * based on an event.
 * The function is called from the event mirroring interface.
 * The scaled usage list of a parallel task is not updated
 * by this function, as this data is maintained by a
 * separate event.
 *
 * @param evc the event client the event arrived on
 * @param type event type
 * @param action action to perform
 * @param event the event (ET); its parallel task is taken over into the array task
 * @param clientdata client data
 *
 * @return SGE_EMA_OK if the update is successful, else SGE_EMA_FAILURE
 *
 * @note The function should only be called from the event mirror interface.
 *
 * @see `sge_mirror_update_master_list()`
 */
sge_callback_result
pe_task_update_master_list([[maybe_unused]] sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                           const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event,
                           [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   // the scaled usage of the parallel task, kept across a modify event
   ocs::tom::ListPtr<ocs::tom::UA> usage;

   char id_buffer[MAX_STRING_SIZE];
   dstring id_dstring;

   sge_dstring_init(&id_dstring, id_buffer, MAX_STRING_SIZE);

   const uint32_t job_id = event.get<ET_intkey>();
   const uint32_t ja_task_id = event.get<ET_intkey2>();
   const char *pe_task_id = event.get<ET_strkey>();

   const auto job = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>().find_first<JB_job_number>(job_id);
   if (!job) {
      ERROR(MSG_JOB_CANTFINDJOBFORUPDATEIN_SS, job_get_id_string(job_id, 0, nullptr, &id_dstring), __func__);
      DRETURN(SGE_EMA_FAILURE);
   }

   // an array task of the job (JAT), fetched with its fixed type
   const auto ja_task = ocs::tom::Element<ocs::tom::JAT>(job_search_task(job, nullptr, ja_task_id));
   if (!ja_task) {
      ERROR(MSG_JOB_CANTFINDJATASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, nullptr, &id_dstring),
            __func__);
      DRETURN(SGE_EMA_FAILURE);
   }

   // a parallel task of the array task (PET), fetched with its fixed type
   auto pe_task = ocs::tom::Element<ocs::tom::PET>(ja_task_search_pe_task(ja_task, pe_task_id));

   // stays untyped: the generic merge below may create the list through the pointer
   lList *pe_task_list = ja_task.get<JAT_task_list>();

   if (action == SGE_EMA_MOD) {
      /* modify event for pe_task.
       * we may not update
       * - PET_scaled_usage - it is maintained by JOB_USAGE events
       */
      if (!pe_task) {
         ERROR(MSG_JOB_CANTFINDPETASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring),
               __func__);
         DRETURN(SGE_EMA_FAILURE);
      }
      usage = pe_task.exchange<PET_scaled_usage>(std::move(usage));
   }

   if (sge_mirror_update_master_list(&pe_task_list, pe_task, job_get_id_string(job_id, ja_task_id, pe_task_id,
                                                                                &id_dstring),
                                     action, event) != SGE_EM_OK) {
      DRETURN(SGE_EMA_FAILURE);
   }

   /* restore pe_task list after modify event */
   if (action == SGE_EMA_MOD) {
      pe_task = ocs::tom::Element<ocs::tom::PET>(ja_task_search_pe_task(ja_task, pe_task_id));
      if (!pe_task) {
         ERROR(MSG_JOB_CANTFINDPETASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring),
               __func__);
         DRETURN(SGE_EMA_FAILURE);
      }

      // the list the event brought in comes back and is freed with the handle
      usage = pe_task.exchange<PET_scaled_usage>(std::move(usage));
      usage.reset();
   }

   /* first petask add event could have created new pe_task list for job */
   if (!ja_task.get<JAT_task_list>() && pe_task_list != nullptr) {
      ocs::tom::set<JAT_task_list>(ja_task, pe_task_list);
   }

   DRETURN(SGE_EMA_OK);
}
