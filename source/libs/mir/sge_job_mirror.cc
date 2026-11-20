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
 * @brief Mirroring the job list
 *
 * Also carries the scheduler job info - the reason messages for pending jobs -
 * which is a list of its own.
 *
 * @see sge_job_mirror.h
 * @see sge_mirror.h
 */

#include <utility>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_SME.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "mir/msg_mirlib.h"
#include "mir/sge_mirror.h"
#include "mir/sge_job_mirror.h"
#include "mir/sge_ja_task_mirror.h"
#include "mir/sge_pe_task_mirror.h"

static bool job_update_master_list_usage(ocs::tom::List<ocs::tom::JB> job_list, ocs::tom::Element<ocs::tom::ET> event);

/**
 * @brief Update usage for a jobs tasks
 *
 * Events containing usage reports are sent for a jobs tasks.
 * This can be array tasks (where a non array job has a single
 * array task) or tasks of a parallel job.
 * This function decides which type of task has to receive
 * the updated usage report and passes the event
 * information to the corresponding update functions.
 *
 * @param job_list master job list (JB)
 * @param event the event (ET) containing the new usage list, which is taken over
 *
 * @return true, if the operation succeeds, else false
 *
 * @see `pe_task_update_master_list_usage()`
 */
static bool job_update_master_list_usage(const ocs::tom::List<ocs::tom::JB> job_list,
                                         const ocs::tom::Element<ocs::tom::ET> event) {
   DENTER(TOP_LAYER);

   bool ret = true;
   uint32_t job_id, ja_task_id;
   const char *pe_task_id;

   job_id = ocs::tom::get<ET_intkey>(event);
   ja_task_id = ocs::tom::get<ET_intkey2>(event);
   pe_task_id = ocs::tom::get<ET_strkey>(event);

   if(ja_task_id == 0) {
      dstring id_dstring = DSTRING_INIT;
      ERROR(MSG_JOB_RECEIVEDINVALIDUSAGEEVENTFORJOB_S, job_get_id_string(job_id, ja_task_id, pe_task_id, &id_dstring));
      sge_dstring_free(&id_dstring);
      ret = false;
   }

   if (pe_task_id != nullptr) {
      /* usage for a pe task */
      ret = pe_task_update_master_list_usage(job_list, event) != SGE_EMA_FAILURE;
   } else {
      /* usage for a ja task */
      ret = ja_task_update_master_list_usage(job_list, event) != SGE_EMA_FAILURE;
   }

   DRETURN(ret);
}

/**
 * @brief Update the master list of jobs
 *
 * Update the global master list of jobs
 * based on an event.
 * The function is called from the event mirroring interface.
 * A jobs array tasks are not updated by this function,
 * as they are maintained by separate events.
 * In addition, some scheduler specific attributes, that
 * are only used in scheduler, are not updated.
 *
 * @param evc the event client the event arrived on
 * @param type event type
 * @param action action to perform
 * @param event the event (ET); its job is taken over into the master list
 * @param clientdata client data
 *
 * @return SGE_EMA_OK if the update is successful, else SGE_EMA_FAILURE
 *
 * @note The function should only be called from the event mirror interface.
 *
 * @see `sge_mirror_update_master_list()`, `job_update_master_list_usage`
 */
sge_callback_result
job_update_master_list([[maybe_unused]] sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                       const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event,
                       [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);
   // the array tasks of the job, kept across a modify event
   ocs::tom::ListPtr<ocs::tom::JAT> ja_tasks;
   char id_buffer[MAX_STRING_SIZE];
   dstring id_dstring;

   sge_dstring_init(&id_dstring, id_buffer, MAX_STRING_SIZE);

   // the master list slot stays untyped: the generic merge below may create the list through it
   lList **list = ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   const uint32_t job_id = ocs::tom::get<ET_intkey>(event);
   const auto job = ocs::tom::find_first_rw<JB_job_number>(*list, job_id);

   if (action == SGE_EMA_MOD) {
      uint32_t event_type = ocs::tom::get<ET_type>(event);

      if (!job) {
         ERROR(MSG_JOB_CANTFINDJOBFORUPDATEIN_SS, job_get_id_string(job_id, 0, nullptr, &id_dstring),
               "job_update_master_list");
         DRETURN(SGE_EMA_FAILURE);
      }

      if (event_type == sgeE_JOB_USAGE || event_type == sgeE_JOB_FINAL_USAGE ) {
         /* special handling needed for JOB_USAGE and JOB_FINAL_USAGE events.
         * they are sent for jobs, ja_tasks and pe_tasks and only contain
         * the usage list.
         * Preferable would probably be to send MOD events for the different
         * object types.
         */
         // the master job list (JB), fetched with its fixed type
         const bool ret = job_update_master_list_usage(ocs::tom::List<ocs::tom::JB>(*list), event);
         DRETURN(ret?SGE_EMA_OK:SGE_EMA_FAILURE);
      } else if (event_type == sgeE_JOB_FINISH) {
         // @todo instead of data of a job a job report is received. why?
         DRETURN(SGE_EMA_OK);
      } else {
         /* this is the true modify event.
          * we may not update several fields:
          * - JB_ja_tasks is the task list - it is maintained by JATASK events
          * - JB_host and JB_category are scheduler internal attributes
          *   they may not be overwritten.
          *   Better would be to move them from JB to some scheduler specific
          *   object.
          */

          // a job event carries jobs (JB)
          const auto event_job = ocs_mirror_event_objects<ocs::tom::JB>(event).front();
          if (event_job) {
             // Preserve old ja_tasks list. Job update events will not contain them
             ja_tasks = job.exchange<JB_ja_tasks>(std::move(ja_tasks));

             // @todo Why is that needed?
             const bool has_host = ocs::tom::has<JB_host>(event_job);
             if (has_host) {
                ocs::tom::set<JB_host>(event_job, ocs::tom::get<JB_host>(job));
             }

             // find category ID
             uint32_t category_id = 0;
             const bool has_category_id = ocs::tom::has<JB_category_id>(event_job);
             if (has_category_id) {
                category_id = ocs::tom::get<JB_category_id>(event_job);
             }

             // convert category id to category ref for the modified job within the current client
             // (scheduler, reader, ...)
             const bool has_category = ocs::tom::has<JB_category>(event_job);
             if (has_category && has_category_id) {
                // the master category list (CT), fetched with its fixed type
                const auto master_category_list = ocs::DataStore::master_list<SGE_TYPE_CATEGORY>();
                // JB_category is a reference field, which takes the raw element
                lListElem *category = ocs::tom::find_first_rw<CT_id>(master_category_list, category_id);

                ocs::tom::set<JB_category>(event_job, category);
             }
          }
      }
   }

   if (sge_mirror_update_master_list(list, job, job_get_id_string(job_id, 0, nullptr, &id_dstring), action,
                                     event) != SGE_EM_OK) {
      DRETURN(SGE_EMA_FAILURE);
   }

   /* restore ja_task list after modify event */
   if (action == SGE_EMA_MOD && ja_tasks) {
      /* we have to search the replaced job */
      const auto new_job = ocs::tom::find_first_rw<JB_job_number>(*list, job_id);
      if (!new_job) {
         ERROR(MSG_JOB_CANTFINDJOBFORUPDATEIN_SS, job_get_id_string(job_id, 0, nullptr, &id_dstring),
               "job_update_master_list");
         DRETURN(SGE_EMA_FAILURE);
      }

      // the list the event brought in comes back and is freed with the handle
      ja_tasks = new_job.exchange<JB_ja_tasks>(std::move(ja_tasks));
      ja_tasks.reset();
   }

   DRETURN(SGE_EMA_OK);
}

/**
 * @brief Update the master list of scheduler job info
 *
 * The reason messages the scheduler produced for pending jobs.
 *
 * @param evc the event client the event arrived on
 * @param type event type; always SGE_TYPE_JOB_SCHEDD_INFO, the mirror table calls this function for it only
 * @param action action to perform
 * @param event the event (ET); its scheduler job info is taken over into the master list
 * @param clientdata client data
 *
 * @return always SGE_EMA_OK
 *
 * @note The function should only be called from the event mirror interface.
 */
sge_callback_result
job_schedd_info_update_master_list([[maybe_unused]] sge_evc_class_t *evc,
                                   [[maybe_unused]] const sge_object_type type,
                                   [[maybe_unused]] const sge_event_action action,
                                   const ocs::tom::Element<ocs::tom::ET> event, [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   // the event carries the whole list (one element) and the master list takes the event's list over; an event
   // without schedd info leaves no list
   auto info_list = ocs_mirror_exchange_event_objects<ocs::tom::SME>(event, ocs::tom::ListPtr<ocs::tom::SME>());
   if (info_list && info_list->empty()) {
      info_list.reset();
   }

   // the data store slot (never nullptr) gets the new list; the old one is freed
   ocs::DataStore::replace_master_list<SGE_TYPE_JOB_SCHEDD_INFO>(std::move(info_list));

   DRETURN(SGE_EMA_OK);
}
