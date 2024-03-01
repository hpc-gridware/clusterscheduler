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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"

#include "mir/sge_mirror.h"
#include "mir/msg_mirlib.h"
#include "mir/sge_ja_task_mirror.h"

/****** Eventmirror/ja_task/ja_task_update_master_list_usage() *****************
*  NAME
*     ja_task_update_master_list_usage() -- update an array tasks usage
*
*  SYNOPSIS
*     bool 
*     ja_task_update_master_list_usage(lList *job_list, lListElem *event)
*
*  FUNCTION
*     Updates the scaled usage of an array task (also task data structure
*     of a non array job).
*
*  INPUTS
*     lListElem *event - event object containing the new usage list
*     lList *job_list  - master job list
*
*  RESULT
*     bool - true, if the operation succeeds, else false
*
*  SEE ALSO
*     Eventmirror/job/job_update_master_list_usage()
*     Eventmirror/ja_task/pe_task_update_master_list_usage()
*******************************************************************************/
sge_callback_result
ja_task_update_master_list_usage(lList *job_list, lListElem *event)
{
   lList *tmp = nullptr;
   u_long32 job_id, ja_task_id;
   lListElem *job, *ja_task;

   DENTER(TOP_LAYER);

   job_id = lGetUlong(event, ET_intkey);
   ja_task_id = lGetUlong(event, ET_intkey2);

   job = lGetElemUlongRW(job_list, JB_job_number, job_id);

   if (job == nullptr) {
      dstring id_dstring = DSTRING_INIT;
      ERROR(MSG_JOB_CANTFINDJOBFORUPDATEIN_SS, job_get_id_string(job_id, 0, nullptr, &id_dstring), __func__);
      sge_dstring_free(&id_dstring);
      DRETURN(SGE_EMA_FAILURE);
   }

   ja_task = job_search_task(job, nullptr, ja_task_id);
   if (ja_task == nullptr) {
      dstring id_dstring = DSTRING_INIT;
      ERROR(MSG_JOB_CANTFINDJATASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, nullptr, &id_dstring), __func__);
      sge_dstring_free(&id_dstring);
      DRETURN(SGE_EMA_FAILURE);
   }

   lXchgList(event, ET_new_version, &tmp);
   lXchgList(ja_task, JAT_scaled_usage_list, &tmp);
   lXchgList(event, ET_new_version, &tmp);

   DRETURN(SGE_EMA_OK);
}

/****** Eventmirror/ja_task/ja_task_update_master_list() ***********************
*  NAME
*     ja_task_update_master_list() -- update array tasks of a job
*
*  SYNOPSIS
*     bool 
*     ja_task_update_master_list(sge_object_type type, sge_event_action action,
*                                lListElem *event, void *clientdata)
*
*  FUNCTION
*     Update the list of array tasks of a job
*     based on an event.
*     The function is called from the event mirroring interface.
*
*     An array tasks list of parallel tasks and the
*     scaled usage list are not updated in this function,
*     as this data is maintained by separate events.
*
*  INPUTS
*     sge_object_type type     - event type
*     sge_event_action action - action to perform
*     lListElem *event        - the raw event
*     void *clientdata        - client data
*
*  RESULT
*     bool - true, if update is successful, else false
*
*  NOTES
*     The function should only be called from the event mirror interface.
*
*  SEE ALSO
*     Eventmirror/--Eventmirror
*     Eventmirror/sge_mirror_update_master_list()
*******************************************************************************/
sge_callback_result
ja_task_update_master_list(sge_evc_class_t *evc, sge_object_type type, 
                           sge_event_action action, lListElem *event, void *clientdata)
{
   u_long32 job_id = 0; 
   lListElem *job = nullptr;

   u_long32 ja_task_id = 0;
   lListElem *ja_task = nullptr;
   lList *ja_task_list = nullptr;
   const lDescr *ja_task_descr = nullptr;

   lList **list = nullptr;
   lList *pe_tasks = nullptr;
   lList *usage = nullptr;

   char id_buffer[MAX_STRING_SIZE];
   dstring id_dstring;

   DENTER(TOP_LAYER);

   sge_dstring_init(&id_dstring, id_buffer, MAX_STRING_SIZE);

   list = object_type_get_master_list_rw(SGE_TYPE_JOB); 
   
   job_id = lGetUlong(event, ET_intkey);
   ja_task_id = lGetUlong(event, ET_intkey2);

   job = lGetElemUlongRW(*list, JB_job_number, job_id);
   if (job == nullptr) {
      ERROR(MSG_JOB_CANTFINDJOBFORUPDATEIN_SS, job_get_id_string(job_id, 0, nullptr, &id_dstring), __func__);
      DRETURN(SGE_EMA_FAILURE);
   }

   ja_task = job_search_task(job, nullptr, ja_task_id);
   ja_task_list = lGetListRW(job, JB_ja_tasks);
   ja_task_descr = lGetListDescr(lGetList(event, ET_new_version));

   if (action == SGE_EMA_MOD) {
      /* modify event for ja_task.
       * we may not update
       * - JAT_task_list - it is maintained by PETASK events
       * - JAT_scaled_usage - it is maintained by JOB_USAGE events
       */
      if (ja_task == nullptr) {
         ERROR(MSG_JOB_CANTFINDJATASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, nullptr, &id_dstring), __func__);
         DRETURN(SGE_EMA_FAILURE);
      }

      lXchgList(ja_task, JAT_task_list, &pe_tasks);
      lXchgList(ja_task, JAT_scaled_usage_list, &usage);
   }

   /* If an array job is deleted, a delete event is sent for
    * each ja_task. If it is not yet enrolled,
    * sge_mirror_update_master_list will fail.
    * If it is not enrolled, but in the range list
    * for pending tasks, remove it from ranges.
    */
   if (action == SGE_EMA_DEL) {
      if (ja_task == nullptr &&
          job_is_ja_task_defined(job, ja_task_id) &&
          (!job_is_enrolled(job, ja_task_id))
         ) {
         job_delete_not_enrolled_ja_task(job, nullptr, ja_task_id);
         DRETURN(SGE_EMA_OK);
      }
   }

   if (sge_mirror_update_master_list(&ja_task_list, ja_task_descr, ja_task, 
                                     job_get_id_string(job_id, ja_task_id, 
                                                       nullptr, &id_dstring),
                                     action, event) != SGE_EM_OK) {
      lFreeList(&pe_tasks);
      lFreeList(&usage);
      DRETURN(SGE_EMA_FAILURE);
   }

   /* restore pe_task list after modify event */
   if (action == SGE_EMA_MOD) {
      /* we have to search the replaced ja_task */
      ja_task = job_search_task(job, nullptr, ja_task_id);
      if (ja_task == nullptr) {
         ERROR(MSG_JOB_CANTFINDJATASKFORUPDATEIN_SS, job_get_id_string(job_id, ja_task_id, nullptr, &id_dstring), __func__);
         lFreeList(&pe_tasks);
         lFreeList(&usage);
         DRETURN(SGE_EMA_FAILURE);
      }

      lXchgList(ja_task, JAT_task_list, &pe_tasks);
      lXchgList(ja_task, JAT_scaled_usage_list, &usage);
      lFreeList(&pe_tasks);
      lFreeList(&usage);
   }

   if (action == SGE_EMA_ADD) {
      /* first jatask add event could have created new ja_task list for job */
      if (lGetList(job, JB_ja_tasks) == nullptr && ja_task_list != nullptr) {
         lSetList(job, JB_ja_tasks, ja_task_list);
      }
      /* we must enroll the task to have it removed in the pending range list */
      job_enroll(job, nullptr, ja_task_id);
   }

   DRETURN(SGE_EMA_OK);
}
