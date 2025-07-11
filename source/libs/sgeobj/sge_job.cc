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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2011-2012 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/                                   

#include <cstring>
#include <unistd.h>

#include "uti/sge.h"

#include "uti/sge_bitfield.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "symbols.h"
#include "msg_common.h"
#include "uti/sge_hostname.h"


/****** sgeobj/job/job_get_ja_task_template_pending() *************************
*  NAME
*     job_get_ja_task_template_pending() -- create a ja task template 
*
*  SYNOPSIS
*     lListElem* job_get_ja_task_template_pending(const lListElem *job, 
*                                                 u_long32 ja_task_id) 
*
*  FUNCTION
*     The function returns a pointer to a template array task element.
*     This task represents a currently submitted pending task 
*     (no hold state).
*
*     The task number (JAT_task_number) of this template task will be
*     initialized with the value given by 'ja_task_id'.
*
*  INPUTS
*     const lListElem *job - JB_Type
*     u_long32 ja_task_id  - array task number 
*
*  RESULT
*     lListElem* - template task (JAT_Type)
*
*  SEE ALSO
*     sgeobj/job/job_get_ja_task_template_pending()
*     sgeobj/job/job_get_ja_task_template_hold()
*     sgeobj/job/job_get_ja_task_template()
*
*  NOTES
*     MT-NOTE: job_get_ja_task_template_pending() is MT safe
*******************************************************************************/
lListElem *job_get_ja_task_template_pending(const lListElem *job,
                                            u_long32 ja_task_id)
{
   lListElem *template_task = nullptr;    /* JAT_Type */

   DENTER(BASIS_LAYER);

   template_task = lFirstRW(lGetList(job, JB_ja_template));

   if (!template_task) {
      ERROR("unable to retrieve template task");
   } else { 
      lSetUlong(template_task, JAT_state, JQUEUED | JWAITING);
      lSetUlong(template_task, JAT_task_number, ja_task_id);  
   }
   DRETURN(template_task);
}

   
/****** sgeobj/job/job_get_ja_task_template_hold() ****************************
*  NAME
*     job_get_ja_task_template_hold() -- create a ja task template 
*
*  SYNOPSIS
*     lListElem* job_get_ja_task_template_pending(const lListElem *job, 
*                                                 u_long32 ja_task_id) 
*
*  FUNCTION
*     The function returns a pointer to a template array task element.
*     This task represents a currently submitted pending task.
*
*     The task number (JAT_task_number) of this template task will be
*     initialized with the value given by 'ja_task_id'.
*
*     The hold state of the task (JAT_hold) will be initialized with
*     the 'hold_state' value. The function job_get_ja_task_hold_state()
*     may be used to get the current hold state of an unenrolled
*     pending task.
*
*  INPUTS
*     const lListElem *job - JB_Type
*     u_long32 ja_task_id  - array task number 
*
*  RESULT
*     lListElem* - template task (JAT_Type)
*
*  SEE ALSO
*     sgeobj/job/job_get_ja_task_template_pending()
*     sgeobj/job/job_get_ja_task_hold_state()
*     sgeobj/job/job_get_ja_task_template()
*******************************************************************************/
lListElem *job_get_ja_task_template_hold(const lListElem *job,
                                         u_long32 ja_task_id, 
                                         u_long32 hold_state)
{
   lListElem *template_task = nullptr;    /* JAT_Type */
 
   DENTER(BASIS_LAYER);
   template_task = job_get_ja_task_template_pending(job, ja_task_id);
   if (template_task) {
      u_long32 state;
 
      lSetUlong(template_task, JAT_task_number, ja_task_id);
      lSetUlong(template_task, JAT_hold, hold_state);
      lSetUlong(template_task, JAT_status, JIDLE);
      state = JQUEUED | JWAITING;
      if (lGetUlong(template_task, JAT_hold)) {
         state |= JHELD;
      }
      lSetUlong(template_task, JAT_state, state);
   }
   DRETURN(template_task);                                                        }

/****** sgeobj/job/job_is_zombie_job() ****************************************
*  NAME
*     job_is_zombie_job() -- Is 'job' a zombie job 
*
*  SYNOPSIS
*     bool job_is_zombie_job(const lListElem *job) 
*
*  FUNCTION
*     True will be returned if 'job' is a zombie job. 
*
*  INPUTS
*     const lListElem *job - JB_Type 
*
*  RESULT
*     bool - true or false 
*******************************************************************************/
bool job_is_zombie_job(const lListElem *job)
{
   return (lGetList(job, JB_ja_z_ids) != nullptr ? true : false);
}

/****** sgeobj/job/job_get_ja_task_template() *********************************
*  NAME
*     job_get_ja_task_template() -- create a ja task template 
*
*  SYNOPSIS
*     lListElem* job_get_ja_task_template_pending(const lListElem *job, 
*                                                 u_long32 ja_task_id) 
*
*  FUNCTION
*     The function returns a pointer to a template array task element.
*     This task represents a currently submitted pending task.
*
*  INPUTS
*     const lListElem *job - JB_Type
*     u_long32 ja_task_id  - array task number 
*
*  RESULT
*     lListElem* - template task (JAT_Type)
*
*  SEE ALSO
*     sgeobj/job/job_get_ja_task_template_pending()
*     sgeobj/job/job_get_ja_task_template_hold()
*     sgeobj/job/job_get_ja_task_hold_state()
*     sgeobj/job/job_get_ja_task_template()
*******************************************************************************/
lListElem *job_get_ja_task_template(const lListElem *job,
                                    u_long32 ja_task_id)
{
   u_long32 hold_state = job_get_ja_task_hold_state(job, ja_task_id);

   return job_get_ja_task_template_hold(job, ja_task_id, hold_state);
}

/****** sgeobj/job/job_get_ja_task_hold_state() *******************************
*  NAME
*     job_get_ja_task_hold_state() -- Hold state of unenrolled task
*
*  SYNOPSIS
*     u_long32 job_get_ja_task_hold_state(const lListElem *job, 
*                                         u_long32 ja_task_id) 
*
*  FUNCTION
*     Returns the hold state of a task which is not enrolled in 
*     the JB_ja_tasks list of 'job' 
*
*  INPUTS
*     const lListElem *job - JB_Type 
*     u_long32 ja_task_id  - valid ja task id  
*
*  RESULT
*     u_long32 - hold state 
*******************************************************************************/
u_long32 job_get_ja_task_hold_state(const lListElem *job,
                                     u_long32 ja_task_id)
{
   u_long32 ret = 0;

   DENTER(TOP_LAYER);
   if (range_list_is_id_within(lGetList(job, JB_ja_u_h_ids), ja_task_id)) {
      ret |= MINUS_H_TGT_USER;
   }
   if (range_list_is_id_within(lGetList(job, JB_ja_o_h_ids), ja_task_id)) {
      ret |= MINUS_H_TGT_OPERATOR;
   }
   if (range_list_is_id_within(lGetList(job, JB_ja_s_h_ids), ja_task_id)) {
      ret |= MINUS_H_TGT_SYSTEM;
   }
   if (range_list_is_id_within(lGetList(job, JB_ja_a_h_ids), ja_task_id)) {
      ret |= MINUS_H_TGT_JA_AD;
   }
   DRETURN(ret);
}

/****** sgeobj/job/job_create_hold_id_lists() *********************************
*  NAME
*     job_create_hold_id_lists() -- Lists for hold combinations
*
*  SYNOPSIS
*     void job_create_hold_id_lists(const lListElem *job, 
*                                   lList *id_list[16], 
*                                   u_long32 hold_state[16]) 
*
*  FUNCTION
*     This function creates sixteen 'id_lists'. Tasks whose id is 
*     contained in an id list has the hold state combination delivered 
*     by 'hold_state'.
*
*     After using 'id_list' the function job_destroy_hold_id_lists() 
*     has to be called to free allocated memory.
*
*  INPUTS
*     const lListElem *job    - JB_Type 
*     lList *id_list[16]      - nullptr initialized pointer array
*     u_long32 hold_state[16] - Array for hold state combinations 
*
*  SEE ALSO
*     sgeobj/job/job_destroy_hold_id_lists() 
*******************************************************************************/
void job_create_hold_id_lists(const lListElem *job, lList *id_list[16], 
                              u_long32 hold_state[16]) 
{
   int i;
   lList *list[24];

   DENTER(TOP_LAYER);

   hold_state[0] = 0;
   hold_state[1] = MINUS_H_TGT_USER;
   hold_state[2] = MINUS_H_TGT_OPERATOR;
   hold_state[3] = MINUS_H_TGT_SYSTEM;
   hold_state[4] = MINUS_H_TGT_JA_AD;
   hold_state[5] = MINUS_H_TGT_USER | MINUS_H_TGT_OPERATOR;
   hold_state[6] = MINUS_H_TGT_USER | MINUS_H_TGT_SYSTEM;
   hold_state[7] = MINUS_H_TGT_USER | MINUS_H_TGT_JA_AD;
   hold_state[8] = MINUS_H_TGT_OPERATOR | MINUS_H_TGT_SYSTEM;
   hold_state[9] = MINUS_H_TGT_OPERATOR | MINUS_H_TGT_JA_AD;
   hold_state[10] = MINUS_H_TGT_SYSTEM | MINUS_H_TGT_JA_AD;
   hold_state[11] = MINUS_H_TGT_USER | MINUS_H_TGT_OPERATOR |
                    MINUS_H_TGT_SYSTEM;
   hold_state[12] = MINUS_H_TGT_USER | MINUS_H_TGT_OPERATOR |
                    MINUS_H_TGT_JA_AD;
   hold_state[13] = MINUS_H_TGT_USER | MINUS_H_TGT_SYSTEM |
                    MINUS_H_TGT_JA_AD;
   hold_state[14] = MINUS_H_TGT_OPERATOR | MINUS_H_TGT_SYSTEM |
                    MINUS_H_TGT_JA_AD;
   hold_state[15] = MINUS_H_TGT_USER | MINUS_H_TGT_OPERATOR |
                    MINUS_H_TGT_SYSTEM | MINUS_H_TGT_JA_AD;

   for (i = 0; i < 24; i++) {
      list[i] = nullptr;
   }

   for (i = 0; i < 16; i++) {
      id_list[i] = nullptr;
   }

   /* uo, us, ua, os, oa, sa, uos, uoa, usa, osa */
   range_list_calculate_intersection_set(&list[0], nullptr,
                  lGetList(job, JB_ja_u_h_ids), lGetList(job, JB_ja_o_h_ids));
   range_list_calculate_intersection_set(&list[1], nullptr,
                  lGetList(job, JB_ja_u_h_ids), lGetList(job, JB_ja_s_h_ids));
   range_list_calculate_intersection_set(&list[2], nullptr,
                  lGetList(job, JB_ja_u_h_ids), lGetList(job, JB_ja_a_h_ids));
   range_list_calculate_intersection_set(&list[3], nullptr,
                  lGetList(job, JB_ja_o_h_ids), lGetList(job, JB_ja_s_h_ids));
   range_list_calculate_intersection_set(&list[4], nullptr,
                  lGetList(job, JB_ja_o_h_ids), lGetList(job, JB_ja_a_h_ids));
   range_list_calculate_intersection_set(&list[5], nullptr,
                  lGetList(job, JB_ja_s_h_ids), lGetList(job, JB_ja_a_h_ids));
   range_list_calculate_intersection_set(&list[6], nullptr, list[0], list[3]);
   range_list_calculate_intersection_set(&list[7], nullptr, list[0], list[4]);
   range_list_calculate_intersection_set(&list[8], nullptr, list[1], list[5]);
   range_list_calculate_intersection_set(&list[9], nullptr, list[3], list[5]);

   /* uosa -> 15 */
   range_list_calculate_intersection_set(&id_list[15], nullptr, list[6], list[7]);

   /* osaU -> 14 */
   range_list_calculate_difference_set(&id_list[14], nullptr, list[9], id_list[15]);

   /* usaO -> 13 */
   range_list_calculate_difference_set(&id_list[13], nullptr, list[8], id_list[15]);

   /* uoaS -> 12 */
   range_list_calculate_difference_set(&id_list[12], nullptr, list[7], id_list[15]);

   /* uosA -> 11 */
   range_list_calculate_difference_set(&id_list[11], nullptr, list[6], id_list[15]);

   /* saUO -> 10 */
   range_list_calculate_difference_set(&list[10], nullptr, list[5], list[8]);
   range_list_calculate_difference_set(&id_list[10], nullptr, list[10], id_list[14]);

   /* oaUS -> 9 */
   range_list_calculate_difference_set(&list[11], nullptr, list[4], list[7]);
   range_list_calculate_difference_set(&id_list[9], nullptr, list[11], id_list[14]);

   /* osUA -> 8 */
   range_list_calculate_difference_set(&list[12], nullptr, list[3], list[6]);
   range_list_calculate_difference_set(&id_list[8], nullptr, list[12], id_list[14]);

   /* uaOS -> 7 */
   range_list_calculate_difference_set(&list[13], nullptr, list[2], list[7]);
   range_list_calculate_difference_set(&id_list[7], nullptr, list[13], id_list[13]);

   /* usOA -> 6 */
   range_list_calculate_difference_set(&list[14], nullptr, list[1], list[6]);
   range_list_calculate_difference_set(&id_list[6], nullptr, list[14], id_list[13]);
   
   /* uoSA -> 5 */
   range_list_calculate_difference_set(&list[15], nullptr, list[0], list[6]);
   range_list_calculate_difference_set(&id_list[5], nullptr, list[15], id_list[12]);

   /* aUOS -> 4 */
   range_list_calculate_difference_set(&list[16], nullptr,
      lGetList(job, JB_ja_a_h_ids), list[2]);
   range_list_calculate_difference_set(&list[17], nullptr, list[16], list[11]);
   range_list_calculate_difference_set(&id_list[4], nullptr, list[17], id_list[10]);

   /* sUOA -> 3 */
   range_list_calculate_difference_set(&list[18], nullptr,
      lGetList(job, JB_ja_s_h_ids), list[1]);
   range_list_calculate_difference_set(&list[19], nullptr, list[18], list[12]);
   range_list_calculate_difference_set(&id_list[3], nullptr, list[19], id_list[10]);

   /* oUSA -> 2 */
   range_list_calculate_difference_set(&list[20], nullptr,
      lGetList(job, JB_ja_o_h_ids), list[0]);
   range_list_calculate_difference_set(&list[21], nullptr, list[20], list[12]);
   range_list_calculate_difference_set(&id_list[2], nullptr, list[21], id_list[9]);

   /* uOSA -> 1 */
   range_list_calculate_difference_set(&list[22], nullptr,
      lGetList(job, JB_ja_u_h_ids), list[0]);
   range_list_calculate_difference_set(&list[23], nullptr, list[22], list[14]);
   range_list_calculate_difference_set(&id_list[1], nullptr, list[23], id_list[7]);

   /* UOSA -> 0 */
   id_list[0] = lCopyList("task_id_range", lGetList(job, JB_ja_n_h_ids));

   for (i = 0; i < 24; i++) {
      lFreeList(&(list[i]));
   }

   DRETURN_VOID;
}

/****** sgeobj/job/job_destroy_hold_id_lists() ********************************
*  NAME
*     job_destroy_hold_id_lists() -- destroy hold combination lists
*
*  SYNOPSIS
*     void job_destroy_hold_id_lists(const lListElem *job, 
*                                    lList *id_list[16]) 
*
*  FUNCTION
*     This function frees all memory allocated by a previous call of 
*     job_create_hold_id_lists(). 
*
*  INPUTS
*     const lListElem *job - JB_Type 
*     lList *id_list[16]   - array of RN_Type lists
*
*  SEE ALSO
*     sgeobj/job/job_create_hold_id_lists 
******************************************************************************/
void job_destroy_hold_id_lists(const lListElem *job, lList *id_list[16]) 
{
   int i;

   DENTER(TOP_LAYER);
   for (i = 0; i < 16; i++) {
      lFreeList(&(id_list[i]));
   }
   DRETURN_VOID;
}

/****** sgeobj/job/job_is_enrolled() ******************************************
*  NAME
*     job_is_enrolled() -- Is a certain array task enrolled 
*
*  SYNOPSIS
*     bool job_is_enrolled(const lListElem *job, u_long32 task_number) 
*
*  FUNCTION
*     This function will return true (1) if the array task with 
*     'task_number' is not enrolled in the JB_ja_tasks sublist 
*     of 'job'.
*
*  INPUTS
*     const lListElem *job - JB_Type 
*     u_long32 task_number - task_number 
*
*  RESULT
*     bool - true or false 
******************************************************************************/
bool job_is_enrolled(const lListElem *job, u_long32 task_number)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   if (range_list_is_id_within(lGetList(job, JB_ja_n_h_ids), task_number) ||
       range_list_is_id_within(lGetList(job, JB_ja_u_h_ids), task_number) ||
       range_list_is_id_within(lGetList(job, JB_ja_o_h_ids), task_number) ||
       range_list_is_id_within(lGetList(job, JB_ja_s_h_ids), task_number) ||
       range_list_is_id_within(lGetList(job, JB_ja_a_h_ids), task_number)) {
      ret = false;
   }
   DRETURN(ret);
}

/****** sgeobj/job/job_is_ja_task_defined() ***********************************
*  NAME
*     job_is_ja_task_defined() -- was this task submitted 
*
*  SYNOPSIS
*     bool job_is_ja_task_defined(const lListElem *job, 
*                                 u_long32 ja_task_number) 
*
*  FUNCTION
*     This function will return true (1) if the task with 
*     'ja_task_number' is defined within the array 'job'. The task 
*     is defined when 'ja_task_number' is enclosed in the task id 
*     range which was specified during submit time (qsub -t). 
*
*  INPUTS
*     const lListElem *job    - JB_Type 
*     u_long32 ja_task_number - task number 
*
*  RESULT
*     bool - true or false 
*
*  NOTES
*     MT-NOTE: job_is_ja_task_defined() is MT safe
******************************************************************************/
bool job_is_ja_task_defined(const lListElem *job, u_long32 ja_task_number) 
{
   const lList *range_list = lGetList(job, JB_ja_structure);

   return range_list_is_id_within(range_list, ja_task_number);
}

/****** sgeobj/job/job_get_ja_tasks() *****************************************
*  NAME
*     job_get_ja_tasks() -- returns number of array tasks 
*
*  SYNOPSIS
*     u_long32 job_get_ja_tasks(const lListElem *job) 
*
*  FUNCTION
*     This function returns the overall amount of tasks in an array job. 
*
*  INPUTS
*     const lListElem *job - JB_Type 
*
*  RESULT
*     u_long32 - number of tasks 
*
*  SEE ALSO
*     sgeobj/job/job_get_ja_tasks() 
*     sgeobj/job/job_get_enrolled_ja_tasks() 
*     sgeobj/job/job_get_not_enrolled_ja_tasks() 
*     sgeobj/job/job_get_submit_ja_tasks() 
******************************************************************************/
u_long32 job_get_ja_tasks(const lListElem *job) 
{  
   u_long32 ret = 0;
   u_long32 n = 0;

   DENTER(TOP_LAYER);
   n = job_get_not_enrolled_ja_tasks(job);
   ret += n;
   DPRINTF("Not enrolled ja_tasks: " sge_u32 "\n", n);
   n = job_get_enrolled_ja_tasks(job);
   ret += n;
   DPRINTF("Enrolled ja_tasks: " sge_u32 "\n", n);
   DRETURN(ret);
}

/****** sgeobj/job/job_get_not_enrolled_ja_tasks() ****************************
*  NAME
*     job_get_not_enrolled_ja_tasks() -- num. of unenrolled tasks 
*
*  SYNOPSIS
*     u_long32 job_get_not_enrolled_ja_tasks(const lListElem *job) 
*
*  FUNCTION
*     This function returns the amount of tasks not enrolled in the
*     JB_ja_tasks sublist. 
*
*  INPUTS
*     const lListElem *job - JB_Type 
*
*  RESULT
*     u_long32 - number of tasks 
*
*  SEE ALSO
*     sgeobj/job/job_get_ja_tasks() 
*     sgeobj/job/job_get_enrolled_ja_tasks() 
*     sgeobj/job/job_get_not_enrolled_ja_tasks() 
*     sgeobj/job/job_get_submit_ja_tasks() 
******************************************************************************/
u_long32 job_get_not_enrolled_ja_tasks(const lListElem *job) 
{
   lList *answer_list = nullptr;
   lList *uosa_ids = nullptr;
   lList *uos_ids = nullptr;
   lList *uo_ids = nullptr;
   u_long32 ret = 0;

   DENTER(TOP_LAYER);     

   range_list_calculate_union_set(&uo_ids, &answer_list,
                                  lGetList(job, JB_ja_u_h_ids),
                                  lGetList(job, JB_ja_o_h_ids));
   range_list_calculate_union_set(&uos_ids, &answer_list, uo_ids, 
                                  lGetList(job, JB_ja_s_h_ids));
   range_list_calculate_union_set(&uosa_ids, &answer_list, uos_ids, 
                                  lGetList(job, JB_ja_a_h_ids));

   ret += range_list_get_number_of_ids(lGetList(job, JB_ja_n_h_ids));
   ret += range_list_get_number_of_ids(uosa_ids);

   lFreeList(&uosa_ids);
   lFreeList(&uos_ids);
   lFreeList(&uo_ids);

   DRETURN(ret);
}

/****** sgeobj/job/job_get_enrolled_ja_tasks() ********************************
*  NAME
*     job_get_enrolled_ja_tasks() -- num. of enrolled array tasks 
*
*  SYNOPSIS
*     u_long32 job_get_not_enrolled_ja_tasks(const lListElem *job) 
*
*  FUNCTION
*     This function returns the amount of tasks enrolled in the
*     JB_ja_tasks sublist. 
*
*  INPUTS
*     const lListElem *job - JB_Type 
*
*  RESULT
*     u_long32 - number of tasks 
*
*  SEE ALSO
*     sgeobj/job/job_get_ja_tasks() 
*     sgeobj/job/job_get_enrolled_ja_tasks() 
*     sgeobj/job/job_get_not_enrolled_ja_tasks() 
*     sgeobj/job/job_get_submit_ja_tasks() 
******************************************************************************/
u_long32 job_get_enrolled_ja_tasks(const lListElem *job) 
{
   return lGetNumberOfElem(lGetList(job, JB_ja_tasks));
}

/****** sgeobj/job/job_get_submit_ja_tasks() **********************************
*  NAME
*     job_get_submit_ja_tasks() -- array size during job submittion 
*
*  SYNOPSIS
*     u_long32 job_get_submit_ja_tasks(const lListElem *job) 
*
*  FUNCTION
*     The function returns the ammount of tasks the job had during
*     it's submittion 
*
*  INPUTS
*     const lListElem *job - JB_Type 
*
*  RESULT
*     u_long32 - number of tasks
*
*  SEE ALSO
*     sgeobj/job/job_get_ja_tasks() 
*     sgeobj/job/job_get_enrolled_ja_tasks() 
*     sgeobj/job/job_get_not_enrolled_ja_tasks() 
*     sgeobj/job/job_get_submit_ja_tasks() 
******************************************************************************/
u_long32 job_get_submit_ja_tasks(const lListElem *job)
{
   u_long32 start, end, step;
 
   job_get_submit_task_ids(job, &start, &end, &step);
   return ((end - start) / step + 1); 
}
 
/** @brief Enrolls a task into the JB_ja_tasks list of a job
 *
 * @param job the job
 * @param answer_list the answer list
 * @param ja_task_number the task number
 *
 * @return the enrolled task
 */
lListElem *
job_enroll(lListElem *job, lList **answer_list, u_long32 ja_task_number) {
   DENTER(TOP_LAYER);

   object_delete_range_id(job, answer_list, JB_ja_n_h_ids, ja_task_number);
   lListElem *ja_task = lGetSubUlongRW(job, JAT_task_number, ja_task_number, JB_ja_tasks);
   if (ja_task == nullptr) {
      lList *ja_task_list = lGetListRW(job, JB_ja_tasks);
      lListElem *template_task = job_get_ja_task_template_pending(job, ja_task_number);

      if (ja_task_list == nullptr) {
         ja_task_list = lCreateList("ulong_sublist", lGetElemDescr(template_task) );
         lSetList(job, JB_ja_tasks, ja_task_list);
      }
      ja_task = lCopyElem(template_task);
      lAppendElem(ja_task_list, ja_task); 
   }

   DRETURN(ja_task);
}

/** @brief Unenrolls a task from the JB_ja_tasks list of a job
 *
 * The task ID is added to the list of unenrolled tasks not in hold state (n_h)
 *
 * @param job the job
 * @param answer_list the answer list
 * @param ja_task the task to unenroll
 */
void
job_unenroll(lListElem *job, lList **answer_list, lListElem **ja_task) {
   DENTER(TOP_LAYER);
   if (job == nullptr || ja_task == nullptr || *ja_task == nullptr) {
      // nothing to do
      DRETURN_VOID;
   }

   lList *ja_task_list = lGetListRW(job, JB_ja_tasks);
   if (ja_task_list == nullptr) {
      // no enrolled task - should never happen
      DRETURN_VOID;
   }

   // add the task id to the list of unenrolled tasks
   u_long32 ja_task_id = lGetUlong(*ja_task, JAT_task_number);
   lList *ids = lGetListRW(job, JB_ja_n_h_ids);
   const lList *before_ids = ids;
   range_list_insert_id(&ids, answer_list, ja_task_id);
   range_list_compress(ids);
   if (before_ids == nullptr) {
      lSetList(job, JB_ja_n_h_ids, ids);
   }

   // remove the enrolled task
   lRemoveElem(ja_task_list, ja_task);

   // trash the list if it was the last task
   if (lGetNumberOfElem(ja_task_list) == 0) {
      lSetList(job, JB_ja_tasks, nullptr);
   }
   DRETURN_VOID;
}

/****** sge_job/job_count_rescheduled_ja_tasks() *******************************
*  NAME
*     job_count_rescheduled_ja_tasks() -- count rescheduled tasks
*
*  SYNOPSIS
*     static int job_count_rescheduled_ja_tasks(lListElem *job, bool count_all)
*
*  FUNCTION
*     Returns number of rescheduled tasks in JB_ja_tasks of a job. The
*     'count_all' flag can be used to cause a quick exit if merely the
*     existence of rescheduled tasks is of interest.
*
*  INPUTS
*     lListElem *job - the job (JB_Type)
*     bool count_all - quick exit flag
*
*  RESULT
*     static int - number of tasks resp. 0/1
*
*  NOTES
*     MT-NOTE: job_count_rescheduled_ja_tasks() is MT safe
*******************************************************************************/
static int job_count_rescheduled_ja_tasks(const lListElem *job, bool count_all)
{
   const lListElem *ja_task;
   u_long32 state;
   int n = 0;

   for_each_ep(ja_task, lGetList(job, JB_ja_tasks)) {
      state = lGetUlong(ja_task, JAT_state);
      if ((lGetUlong(ja_task, JAT_status) == JIDLE) && ((state & JQUEUED) != 0) && ((state & JWAITING) != 0)) {
         n++;
         if (!count_all)
            break;
      }
   }
   return n;
}

/****** sgeobj/job/job_count_pending_tasks() ********************************************
*  NAME
*     job_count_pending_tasks() -- Count number of pending tasks
*
*  SYNOPSIS
*     bool job_count_pending_tasks(lListElem *job, bool count_all)
*
*  FUNCTION
*     This function returns the number of pending tasks of a job.
*
*  INPUTS
*     lListElem *job - JB_Type
*     bool           - number of tasks or simply 0/1 if count_all is 'false'
*
*  RESULT
*     int - number of tasks or simply 0/1 if count_all is 'false'
******************************************************************************/
int job_count_pending_tasks(const lListElem *job, bool count_all)
{
   int n = 0;

   DENTER(TOP_LAYER);

   if (count_all) {
      n = range_list_get_number_of_ids(lGetList(job, JB_ja_n_h_ids));
      n += job_count_rescheduled_ja_tasks(job, true);
   } else {
      if (lGetList(job, JB_ja_n_h_ids) || job_count_rescheduled_ja_tasks(job, false))
         n = 1;
   }

   DRETURN(n);
}


/****** sgeobj/job/job_delete_not_enrolled_ja_task() **************************
*  NAME
*     job_delete_not_enrolled_ja_task() -- remove unenrolled task 
*
*  SYNOPSIS
*     void job_delete_not_enrolled_ja_task(lListElem *job, 
*                                          lList **answer_list, 
*                                          u_long32 ja_task_number) 
*
*  FUNCTION
*     Removes an unenrolled pending task from the id lists.
*
*  INPUTS
*     lListElem *job          - JB_Type 
*     lList **answer_list     - AN_Type 
*     u_long32 ja_task_number - Task to be removed 
*
*  SEE ALSO
*     sgeobj/job/job_add_as_zombie()
******************************************************************************/
void job_delete_not_enrolled_ja_task(lListElem *job, lList **answer_list, 
                                     u_long32 ja_task_number) 
{
   const int attributes = 5;
   const int attribute[] = {JB_ja_n_h_ids, JB_ja_u_h_ids, JB_ja_o_h_ids,
                            JB_ja_s_h_ids, JB_ja_a_h_ids};
   int i;

   DENTER(TOP_LAYER);
   for (i = 0; i < attributes; i++) { 
      object_delete_range_id(job, answer_list, attribute[i], ja_task_number);
   }
   DRETURN_VOID;
}

/****** sgeobj/job/job_add_as_zombie() ****************************************
*  NAME
*     job_add_as_zombie() -- add task into zombie id list 
*
*  SYNOPSIS
*     void job_add_as_zombie(lListElem *zombie, lList **answer_list, 
*                            u_long32 ja_task_id) 
*
*  FUNCTION
*     Adds a task into the zombie id list (JB_ja_z_ids)
*
*  INPUTS
*     lListElem *zombie    - JB_Type 
*     lList **answer_list  - AN_Type 
*     u_long32 ja_task_id  - Task id to be inserted
*
*  SEE ALSO
*     sgeobj/job/job_delete_not_enrolled_ja_task()
******************************************************************************/
void job_add_as_zombie(lListElem *zombie, lList **answer_list, 
                       u_long32 ja_task_id) 
{
   lList *z_ids = nullptr;    /* RN_Type */

   DENTER(TOP_LAYER);
   lXchgList(zombie, JB_ja_z_ids, &z_ids);
   range_list_insert_id(&z_ids, nullptr, ja_task_id);
   range_list_compress(z_ids);
   lXchgList(zombie, JB_ja_z_ids, &z_ids);    
   DRETURN_VOID;
}

/****** sgeobj/job/job_has_soft_requests() ********************************
*  NAME
*     job_has_soft_requests() -- Has the job soft requests?
*
*  SYNOPSIS
*     bool job_has_soft_requests(lListElem *job) 
*
*  FUNCTION
*     True (1) will be returned if the job has soft requests.
*
*  INPUTS
*     lListElem *job - JB_Type 
*
*  RESULT
*     bool - true or false 
*
*  NOTES
*     MT-NOTES: job_has_soft_requests() is MT safe
*******************************************************************************/
bool job_has_soft_requests(lListElem *job) 
{
   bool ret = false;
   const lListElem *jrs;
   for_each_ep(jrs, lGetList(job, JB_request_set_list)) {
      if (lGetList(jrs, JRS_soft_resource_list) != nullptr ||
          lGetList(jrs, JRS_soft_queue_list) != nullptr) {
         ret = true;
         break;
      }
   }
   
   return ret;
}

/****** sgeobj/job/job_set_hold_state() ***************************************
*  NAME
*     job_set_hold_state() -- Changes the hold state of a task.
*
*  SYNOPSIS
*     void job_set_hold_state(lListElem *job, lList **answer_list, 
*                             u_long32 ja_task_id, 
*                             u_long32 new_hold_state) 
*
*  FUNCTION
*     This function changes the hold state of a job/array-task.
*
*  INPUTS
*     lListElem *job          - JB_Type 
*     lList **answer_list     - AN_Type 
*     u_long32 ja_task_id     - Array task id 
*     u_long32 new_hold_state - hold state (see MINUS_H_TGT_*)
******************************************************************************/
void job_set_hold_state(lListElem *job, lList **answer_list, 
                        u_long32 ja_task_id,
                        u_long32 new_hold_state)
{
   DENTER(TOP_LAYER);
   if (!job_is_enrolled(job, ja_task_id)) {
      const int lists = 5;
      const u_long32 mask[] = {MINUS_H_TGT_ALL, MINUS_H_TGT_USER, 
                               MINUS_H_TGT_OPERATOR, MINUS_H_TGT_SYSTEM,
                               MINUS_H_TGT_JA_AD};
      const int attribute[] = {JB_ja_n_h_ids, JB_ja_u_h_ids, JB_ja_o_h_ids, 
                               JB_ja_s_h_ids, JB_ja_a_h_ids}; 
      const range_remove_insert_t if_function[] = {range_list_remove_id,
                                                   range_list_insert_id,
                                                   range_list_insert_id,
                                                   range_list_insert_id,
                                                   range_list_insert_id}; 
      const range_remove_insert_t else_function[] = {range_list_insert_id,
                                                     range_list_remove_id,
                                                     range_list_remove_id,
                                                     range_list_remove_id,
                                                     range_list_remove_id};
      int i;

      for (i = 0; i < lists; i++) {
         lList *range_list = nullptr;

         if (new_hold_state & mask[i]) {
            lXchgList(job, attribute[i], &range_list);
            if_function[i](&range_list, answer_list, ja_task_id);
            lXchgList(job, attribute[i], &range_list); 
         } else {
            lXchgList(job, attribute[i], &range_list);
            else_function[i](&range_list, answer_list, ja_task_id);
            lXchgList(job, attribute[i], &range_list);
         }
         range_list_compress(lGetListRW(job, attribute[i]));
      }
   } else {
      lListElem *ja_task = job_search_task(job, nullptr, ja_task_id);

      if (ja_task != nullptr) {
         lSetUlong(ja_task, JAT_hold, new_hold_state); 
         if (new_hold_state) {
            lSetUlong(ja_task, JAT_state, lGetUlong(ja_task, JAT_state) | JHELD);
         } else {
            lSetUlong(ja_task, JAT_state, lGetUlong(ja_task, JAT_state) & ~JHELD);
         }
      }
   }
   DRETURN_VOID;
}

/****** sgeobj/job/job_get_hold_state() ***************************************
*  NAME
*     job_get_hold_state() -- Returns the hold state of a task
*
*  SYNOPSIS
*     u_long32 job_get_hold_state(lListElem *job, u_long32 ja_task_id) 
*
*  FUNCTION
*     Returns the hold state of a job/array-task 
*
*  INPUTS
*     lListElem *job      - JB_Type 
*     u_long32 ja_task_id - array task id 
*
*  RESULT
*     u_long32 - hold state (see MINUS_H_TGT_*)
******************************************************************************/
u_long32 job_get_hold_state(lListElem *job, u_long32 ja_task_id)
{
   u_long32 ret = 0;

   DENTER(TOP_LAYER);
   if (job_is_enrolled(job, ja_task_id)) {
      lListElem *ja_task = job_search_task(job, nullptr, ja_task_id);
  
      if (ja_task != nullptr) {
         ret = lGetUlong(ja_task, JAT_hold) & MINUS_H_TGT_ALL;
      } else {
         ret = 0;
      }
   } else {
      int attribute[4] = {JB_ja_u_h_ids, JB_ja_o_h_ids,
                          JB_ja_s_h_ids, JB_ja_a_h_ids };
      u_long32 hold_flag[4] = {MINUS_H_TGT_USER, MINUS_H_TGT_OPERATOR,
                               MINUS_H_TGT_SYSTEM, MINUS_H_TGT_JA_AD};
      int i;

      for (i = 0; i < 4; i++) {
         const lList *hold_list = lGetList(job, attribute[i]);

         if (range_list_is_id_within(hold_list, ja_task_id)) {
            ret |= hold_flag[i];
         }
      }
   }
   DRETURN(ret);
}

/****** sgeobj/job/job_search_task() ******************************************
*  NAME
*     job_search_task() -- Search an array task
*
*  SYNOPSIS
*     lListElem* job_search_task(const lListElem *job, 
*                                lList **answer_list, 
*                                u_long32 ja_task_id)
*
*  FUNCTION
*     This function return the array task with the id 'ja_task_id' if 
*     it exists in the JB_ja_tasks-sublist of 'job'. If the task 
*     is not found in the sublist, nullptr is returned.
*
*  INPUTS
*     const lListElem *job       - JB_Type 
*     lList **answer_list        - AN_Type 
*     u_long32 ja_task_id        - array task id 
*
*  RESULT
*     lListElem* - JAT_Type element
*
*  NOTES
*     In case of errors, the function should return a message in a
*     given answer_list (answer_list != nullptr).
*     MT-NOTE: job_search_task() is MT safe
******************************************************************************/
lListElem *job_search_task(const lListElem *job, lList **answer_list,
                           u_long32 ja_task_id)
{
   lListElem *ja_task = nullptr;

   DENTER(TOP_LAYER);
   if (job != nullptr) {
      ja_task = lGetSubUlongRW(job, JAT_task_number, ja_task_id, JB_ja_tasks);
   }
   DRETURN(ja_task);
}

/****** sgeobj/job/job_create_task() ******************************************
*  NAME
*     job_create_task() -- Create an array task
*
*  SYNOPSIS
*     lListElem* job_create_task(lListElem *job, lList **answer_list, 
*                                u_long32 ja_task_id)
*
*  FUNCTION
*     This function return the array task with the id 'ja_task_id' if 
*     it exists in the JB_ja_tasks-sublist of 'job'.
*     A new element will be created in the sublist
*     of 'job' and a pointer to the new element will be returned.
*     Errors may be found in the 'answer_list'
*
*  INPUTS
*     lListElem *job             - JB_Type 
*     lList **answer_list        - AN_Type 
*     u_long32 ja_task_id        - array task id 
*
*  RESULT
*     lListElem* - JAT_Type element
*
*  NOTES
*     In case of errors, the function should return a message in a
*     given answer_list (answer_list != nullptr).
******************************************************************************/
lListElem *job_create_task(lListElem *job, lList **answer_list, u_long32 ja_task_id)
{
   lListElem *ja_task = nullptr;

   DENTER(TOP_LAYER);

   if (job != nullptr && job_is_ja_task_defined(job, ja_task_id)) {
      ja_task = job_enroll(job, answer_list, ja_task_id);
   }

   DRETURN(ja_task);
}

/****** sgeobj/job/job_get_shell_start_mode() *********************************
*  NAME
*     job_get_shell_start_mode() -- get shell start mode for 'job' 
*
*  SYNOPSIS
*     const char* job_get_shell_start_mode(const lListElem *job, 
*                                        const lListElem *queue,
*                             const char *conf_shell_start_mode) 
*
*  FUNCTION
*     Returns a string identifying the shell start mode for 'job'.
*
*  INPUTS
*     const lListElem *job              - JB_Type element 
*     const lListElem *queue            - QU_Type element
*     const char *conf_shell_start_mode - shell start mode of 
*                                         configuration
*
*  RESULT
*     const char* - shell start mode
******************************************************************************/
const char *job_get_shell_start_mode(const lListElem *job,
                                     const lListElem *queue,
                                     const char *conf_shell_start_mode) 
{
   const char *ret;
   const char *queue_start_mode = lGetString(queue, QU_shell_start_mode);

   if (queue_start_mode && strcasecmp(queue_start_mode, "none")) {
      ret = queue_start_mode;
   } else {
      ret = conf_shell_start_mode;
   }
   return ret;
}

/****** sgeobj/job/job_list_add_job() *****************************************
*  NAME
*     job_list_add_job() -- Creates a joblist and adds an job into it 
*
*  SYNOPSIS
*     int job_list_add_job(lList **job_list, const char *name, 
*                          lListElem *job, int check) 
*
*  FUNCTION
*     A 'job_list' will be created by this function if it does not 
*     already exist and 'job' will be inserted into this 'job_list'. 
*     'name' will be the name of the new list.
*
*     If 'check' is true (1) than the function will test whether 
*     there is already an element in 'job_list' which has the same 
*     'JB_job_number' like 'job'. If this is true than -1 will be 
*     returned by this function.
*      
*
*  INPUTS
*     lList **job_list - JB_Type
*     const char *name - name of the list
*     lListElem *job   - JB_Type element 
*     int check        - Does the element already exist? 
*
*  RESULT
*     int - error code
*           1 => invalid parameter
*          -1 => check failed: element already exists
*           0 => OK
******************************************************************************/
int job_list_add_job(lList **job_list, const char *name, lListElem *job, 
                     int check) {
   DENTER(TOP_LAYER);

   if (!job_list) {
      ERROR(SFNMAX, MSG_JOB_JLPPNULL);
      DRETURN(1);
   }
   if (!job) {
      ERROR(SFNMAX, MSG_JOB_JEPNULL);
      DRETURN(1);
   }

   if(!*job_list) {
      *job_list = lCreateList(name, JB_Type);
   }

   if (check && *job_list && lGetElemUlong(*job_list, JB_job_number, lGetUlong(job, JB_job_number))) {
      dstring id_dstring = DSTRING_INIT;
      ERROR(MSG_JOB_JOBALREADYEXISTS_S, job_get_id_string(lGetUlong(job, JB_job_number), 0, nullptr, &id_dstring));
      sge_dstring_free(&id_dstring);
      DRETURN(-1);
   }

   lAppendElem(*job_list, job);

   DRETURN(0);
}     

/****** sgeobj/job/job_is_array() *********************************************
*  NAME
*     job_is_array() -- Is "job" an array job or not? 
*
*  SYNOPSIS
*     bool job_is_array(const lListElem *job) 
*
*  FUNCTION
*     The function returns true (1) if "job" is an array job with more 
*     than one task. 
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*
*  RESULT
*     bool - true or false
*
*  SEE ALSO
*     sgeobj/job/job_is_parallel()
*     sgeobj/job/job_is_tight_parallel()
******************************************************************************/
bool job_is_array(const lListElem *job)
{
   u_long32 job_type = lGetUlong(job, JB_type);

   return JOB_TYPE_IS_ARRAY(job_type) ? true : false;
}  

/****** sgeobj/job/job_is_parallel() ******************************************
*  NAME
*     job_is_parallel() -- Is "job" a parallel job? 
*
*  SYNOPSIS
*     bool job_is_parallel(const lListElem *job) 
*
*  FUNCTION
*     This function returns true if "job" is a parallel job
*     (requesting a parallel environment). 
*
*  INPUTS
*     const lListELem *job - JB_Type element 
*
*  RESULT
*     bool - true or false
*
*  SEE ALSO
*     sgeobj/job/job_is_array() 
*     sgeobj/job/job_is_tight_parallel()
******************************************************************************/
bool job_is_parallel(const lListElem *job)
{
   return (lGetString(job, JB_pe) != nullptr ? true : false);
} 

/****** sgeobj/job/job_is_tight_parallel() ************************************
*  NAME
*     job_is_tight_parallel() -- Is "job" a tightly integrated par. job?
*
*  SYNOPSIS
*     bool job_is_tight_parallel(const lListElem *job, 
*                                const lList *pe_list) 
*
*  FUNCTION
*     This function returns true if "job" is really a tightly 
*     integrated parallel job. 
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*     const lList *pe_list - PE_Type list with all existing PEs 
*
*  RESULT
*     bool - true or false
*
*  SEE ALSO
*     sgeobj/job/job_is_array()
*     sgeobj/job/job_is_parallel()
*     sgeobj/job/job_might_be_tight_parallel()
******************************************************************************/
bool job_is_tight_parallel(const lListElem *job, const lList *pe_list)
{
   bool ret = false;
   const char *pe_name = nullptr;

   DENTER(TOP_LAYER);
   pe_name = lGetString(job, JB_pe);
   if (pe_name != nullptr) {
      int found_pe = 0;
      int all_are_tight = 1;
      const lListElem *pe;

      for_each_ep(pe, pe_list) {
         if (pe_is_matching(pe, pe_name)) {
            found_pe = 1;
            all_are_tight &= lGetBool(pe, PE_control_slaves);
         }
      }
   
      if (found_pe && all_are_tight) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/****** sgeobj/job/job_might_be_tight_parallel() ******************************
*  NAME
*     job_might_be_tight_parallel() -- Tightly integrated job? 
*
*  SYNOPSIS
*     bool job_might_be_tight_parallel(const lListElem *job, 
*                                      const lList *pe_list) 
*
*  FUNCTION
*     This functions returns true  if "job" might be a tightly 
*     integrated job. True will be returned if (at least one) pe 
*     matching the requested (wildcard) pe of a job has 
*     "contol_slaves=true" in its configuration.
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*     const lList *pe_list - PE_Type list with all existing PEs 
*
*  RESULT
*     bool - true or false
*
*  SEE ALSO
*     sgeobj/job/job_is_array()
*     sgeobj/job/job_is_parallel()
*     sgeobj/job/job_is_tight_parallel()
*     sgeobj/job/job_might_be_tight_parallel()
******************************************************************************/
bool job_might_be_tight_parallel(const lListElem *job, const lList *pe_list)
{
   bool ret = false;
   const char *pe_name = nullptr;

   DENTER(TOP_LAYER);

   pe_name = lGetString(job, JB_pe);
   if (pe_name != nullptr) {
      const lListElem *pe;

      for_each_ep(pe, pe_list) {
         if (pe_is_matching(pe, pe_name) && lGetBool(pe, PE_control_slaves)) {
            ret = true;
            break;
         }
      }
   }
   DRETURN(ret);
}

/****** sgeobj/job/job_get_submit_task_ids() **********************************
*  NAME
*     job_get_submit_task_ids() -- Submit time task specification 
*
*  SYNOPSIS
*     void job_get_submit_task_ids(const lListElem *job, 
*                                  u_long32 *start, 
*                                  u_long32 *end, 
*                                  u_long32 *step) 
*
*  FUNCTION
*     The function returns the "start", "end" and "step" numbers 
*     which where used to create "job" (qsub -t <range>).
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*     u_long32 *start      - first id 
*     u_long32 *end        - last id 
*     u_long32 *step       - step size (>=1)
******************************************************************************/
void job_get_submit_task_ids(const lListElem *job, u_long32 *start, 
                             u_long32 *end, u_long32 *step)
{
   const lListElem *range_elem = lFirst(lGetList(job, JB_ja_structure));
   if (range_elem) {
      u_long32 tmp_step;
 
      *start = lGetUlong(range_elem, RN_min);
      *end = lGetUlong(range_elem, RN_max);
      tmp_step = lGetUlong(range_elem, RN_step);
      *step = tmp_step ? tmp_step : 1;
   } else {
      *start = *end = *step = 1;
   }
}      

/****** sgeobj/job/job_set_submit_task_ids() **********************************
*  NAME
*     job_set_submit_task_ids() -- store the initial range ids in "job"
*
*  SYNOPSIS
*     int job_set_submit_task_ids(lListElem *job, u_long32 start, 
*                                 u_long32 end, u_long32 step) 
*
*  FUNCTION
*     The function stores the initial range id values ("start", "end" 
*     and "step") in "job". It should only be used in functions 
*     initializing new jobs.
*
*  INPUTS
*     lListElem *job - JB_Type job 
*     u_long32 start - first id 
*     u_long32 end   - last id 
*     u_long32 step  - step size 
*
*  RESULT
*     int - 0 -> OK
*           1 -> no memory
*
*  NOTES
*     MT-NOTE: job_set_submit_task_ids() is MT safe
******************************************************************************/
int job_set_submit_task_ids(lListElem *job, u_long32 start, u_long32 end,
                            u_long32 step)
{
   return object_set_range_id(job, JB_ja_structure, start, end, step);
}          

/****** sgeobj/job/job_get_smallest_unenrolled_task_id() **********************
*  NAME
*     job_get_smallest_unenrolled_task_id() -- get smallest id 
*
*  SYNOPSIS
*     u_long32 job_get_smallest_unenrolled_task_id(const lListElem *job) 
*
*  FUNCTION
*     Returns the smallest task id currently existing in a job 
*     which is not enrolled in the JB_ja_tasks sublist of 'job'.
*     If all tasks are enrolled 0 will be returned. 
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*
*  RESULT
*     u_long32 - task id or 0
******************************************************************************/
u_long32 job_get_smallest_unenrolled_task_id(const lListElem *job)
{
   u_long32 n_h_id, u_h_id, o_h_id, s_h_id, a_h_id;
   u_long32 ret = 0;

   n_h_id = range_list_get_first_id(lGetList(job, JB_ja_n_h_ids), nullptr);
   u_h_id = range_list_get_first_id(lGetList(job, JB_ja_u_h_ids), nullptr);
   o_h_id = range_list_get_first_id(lGetList(job, JB_ja_o_h_ids), nullptr);
   s_h_id = range_list_get_first_id(lGetList(job, JB_ja_s_h_ids), nullptr);
   a_h_id = range_list_get_first_id(lGetList(job, JB_ja_a_h_ids), nullptr);
   ret = n_h_id;
   if (ret > 0 && u_h_id > 0) {
      ret = MIN(ret, u_h_id);
   } else if (u_h_id > 0) {
      ret = u_h_id;
   }
   if (ret > 0 && o_h_id > 0) {
      ret = MIN(ret, o_h_id);
   } else if (o_h_id > 0) {
      ret = o_h_id;
   }
   if (ret > 0 && s_h_id > 0)  {
      ret = MIN(ret, s_h_id);
   } else if (s_h_id > 0){
      ret = s_h_id;
   }
   if (ret == 0 && a_h_id > 0)  {
      ret = MIN(ret, a_h_id);
   } else if (a_h_id > 0){
      ret = a_h_id;
   }
   return ret;
}

/****** sgeobj/job/job_get_smallest_enrolled_task_id() ************************
*  NAME
*     job_get_smallest_enrolled_task_id() -- find smallest enrolled tid 
*
*  SYNOPSIS
*     u_long32 job_get_smallest_enrolled_task_id(const lListElem *job) 
*
*  FUNCTION
*     Returns the smallest task id currently existing in a job 
*     which is enrolled in the JB_ja_tasks sublist of 'job'.
*     If no task is enrolled 0 will be returned.
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*
*  RESULT
*     u_long32 - task id or 0
******************************************************************************/
u_long32 job_get_smallest_enrolled_task_id(const lListElem *job)
{
   const lListElem *ja_task;        /* JAT_Type */
   const lListElem *nxt_ja_task;    /* JAT_Type */
   u_long32 ret = 0; 

   /*
    * initialize ret
    */
   ja_task = lFirst(lGetList(job, JB_ja_tasks)); 
   nxt_ja_task = lNext(ja_task);
   if (ja_task != nullptr) {
      ret = lGetUlong(ja_task, JAT_task_number);
   }

   /*
    * try to find a smaller task id
    */
   while ((ja_task = nxt_ja_task)) {
      nxt_ja_task = lNext(ja_task);

      ret = MIN(ret, lGetUlong(ja_task, JAT_task_number));
   }
   return ret;
}

/****** sgeobj/job/job_get_biggest_unenrolled_task_id() ***********************
*  NAME
*     job_get_biggest_unenrolled_task_id() -- find biggest unenrolled id 
*
*  SYNOPSIS
*     u_long32 job_get_biggest_unenrolled_task_id(const lListElem *job) 
*
*  FUNCTION
*     Returns the biggest task id currently existing in a job 
*     which is not enrolled in the JB_ja_tasks sublist of 'job'.
*     If no task is enrolled 0 will be returned.
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*
*  RESULT
*     u_long32 - task id or 0
******************************************************************************/
u_long32 job_get_biggest_unenrolled_task_id(const lListElem *job)
{
   u_long32 n_h_id, u_h_id, o_h_id, s_h_id, a_h_id;
   u_long32 ret = 0;
 
   n_h_id = range_list_get_last_id(lGetList(job, JB_ja_n_h_ids), nullptr);
   u_h_id = range_list_get_last_id(lGetList(job, JB_ja_u_h_ids), nullptr);
   o_h_id = range_list_get_last_id(lGetList(job, JB_ja_o_h_ids), nullptr);
   s_h_id = range_list_get_last_id(lGetList(job, JB_ja_s_h_ids), nullptr);
   a_h_id = range_list_get_last_id(lGetList(job, JB_ja_a_h_ids), nullptr);
   ret = n_h_id;
   if (ret > 0 && u_h_id > 0) {
      ret = MAX(ret, u_h_id);
   } else if (u_h_id > 0) {
      ret = u_h_id;
   }
   if (ret > 0 && o_h_id > 0) {
      ret = MAX(ret, o_h_id);
   } else if (o_h_id > 0) {
      ret = o_h_id;
   }
   if (ret > 0 && s_h_id > 0)  {
      ret = MAX(ret, s_h_id);
   } else if (s_h_id > 0 ){
      ret = s_h_id; 
   }
   if (ret == 0 && a_h_id > 0)  {
      ret = MAX(ret, a_h_id);
   } else if (a_h_id > 0 ){
      ret = a_h_id; 
   }
   return ret;
}

/****** sgeobj/job/job_get_biggest_enrolled_task_id() *************************
*  NAME
*     job_get_biggest_enrolled_task_id() -- find biggest enrolled tid 
*
*  SYNOPSIS
*     u_long32 job_get_biggest_enrolled_task_id(const lListElem *job) 
*
*  FUNCTION
*     Returns the biggest task id currently existing in a job 
*     which is enrolled in the JB_ja_tasks sublist of 'job'.
*     If no task is enrolled 0 will be returned.
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*
*  RESULT
*     u_long32 - task id or 0
******************************************************************************/
u_long32 job_get_biggest_enrolled_task_id(const lListElem *job)
{
   const lListElem *ja_task;        /* JAT_Type */
   const lListElem *nxt_ja_task;    /* JAT_Type */
   u_long32 ret = 0; 

   /*
    * initialize ret
    */
   ja_task = lLast(lGetList(job, JB_ja_tasks)); 
   nxt_ja_task = lPrev(ja_task);
   if (ja_task != nullptr) {
      ret = lGetUlong(ja_task, JAT_task_number);
   }

   /*
    * try to find a smaller task id
    */
   while ((ja_task = nxt_ja_task)) {
      nxt_ja_task = lPrev(ja_task);

      ret = MAX(ret, lGetUlong(ja_task, JAT_task_number));
   }
   return ret;
}
 
/****** sgeobj/job/job_list_register_new_job() ********************************
*  NAME
*     job_list_register_new_job() -- try to register a new job
*
*  SYNOPSIS
*     int job_list_register_new_job(const lList *job_list, 
*                                   u_long32 max_jobs,
*                                   int force_registration)
*
*  FUNCTION
*     This function checks whether a new job would exceed the maximum
*     of allowed jobs per cluster ("max_jobs"). If the limit would be
*     exceeded then the function will return 1 otherwise 0. In some
*     situations it may be necessary to force the registration
*     of a new job (reading jobs from spool area). This may be done
*     with "force_registration".
*
*
*  INPUTS
*     const lListElem *job   - JB_Type element
*     u_long32 max_jobs      - maximum number of allowed jobs per user
*     int force_registration - force job registration
*
*  RESULT
*     int - 1 => limit would be exceeded
*           0 => otherwise
*
*  SEE ALSO
*     sgeobj/suser/suser_register_new_job()
******************************************************************************/
int job_list_register_new_job(const lList *job_list, u_long32 max_jobs, int force_registration)
{
   int ret = 1;
 
   DENTER(TOP_LAYER);
   if (max_jobs > 0 && !force_registration && max_jobs <= lGetNumberOfElem(job_list)) {
      ret = 1;
   } else {
      ret = 0;
   }
   DRETURN(ret);
}                


/****** sgeobj/job/job_initialize_id_lists() **********************************
*  NAME
*     job_initialize_id_lists() -- initialize task id range lists 
*
*  SYNOPSIS
*     void job_initialize_id_lists(lListElem *job, lList **answer_list) 
*
*  FUNCTION
*     Initialize the task id range lists within "job". All tasks within
*     the JB_ja_structure element of job will be added to the
*     JB_ja_n_h_ids list. All other id lists stored in the "job" will
*     be deleted. 
*
*  INPUTS
*     lListElem *job      - JB_Type element 
*     lList **answer_list - AN_Type list pointer 
*
*  RESULT
*     int - return state
*        -1 - error
*         0 - OK
*
*  SEE ALSO
*     sgeobj/range/RN_Type
*
*  NOTES
*     MT-NOTE: job_initialize_id_lists() is MT safe
******************************************************************************/
int job_initialize_id_lists(lListElem *job, lList **answer_list)
{
   lList *n_h_list = nullptr;    /* RN_Type */

   DENTER(TOP_LAYER);
   n_h_list = lCopyList("task_id_range", lGetList(job, JB_ja_structure));
   if (n_h_list == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                              MSG_MEM_MEMORYALLOCFAILED_S, __func__);
      DRETURN(-1);
   } else {
      lSetList(job, JB_ja_n_h_ids, n_h_list);
      lSetList(job, JB_ja_u_h_ids, nullptr);
      lSetList(job, JB_ja_o_h_ids, nullptr);
      lSetList(job, JB_ja_s_h_ids, nullptr);
      lSetList(job, JB_ja_a_h_ids, nullptr);
   }
   DRETURN(0);
}

/****** sgeobj/job/job_initialize_env() ***************************************
*  NAME
*     job_initialize_env() -- initialize environment (partially) 
*
*  SYNOPSIS
*     void job_initialize_env(lListElem *job, lList **answer_list, 
*                             const lList* path_alias_list) 
*
*  FUNCTION
*     Initialize the environment sublist (JB_env_list) of "job".
*     Path aliasing ("path_alias_list") has to be initialized before 
*     this function might be called. 
*
*     Following enironment variables will be added:
*        <VAR_PREFIX>O_HOME
*        <VAR_PREFIX>O_LOGNAME
*        <VAR_PREFIX>O_PATH
*        <VAR_PREFIX>O_SHELL
*        <VAR_PREFIX>O_TZ
*        <VAR_PREFIX>O_HOST
*        <VAR_PREFIX>O_WORKDIR
*        <VAR_PREFIX>O_MAIL
*
*     This function will be used in SGE/EE client applications.
*     Clients do not know which prefix should be used for job
*     environment variables ("SGE_" or other). Therefore 
*     we use the define <VAR_PREFIX> which will be replaced shortly 
*     before the job is started.
*
*  INPUTS
*     lListElem *job               - JB_Type element 
*     lList **answer_list          - AN_Type list pointer 
*     const lList* path_alias_list - PA_Type list 
******************************************************************************/
void job_initialize_env(lListElem *job, lList **answer_list, 
                        const lList* path_alias_list,
                        const char *unqualified_hostname,
                        const char *qualified_hostname)
{
   lList *env_list = nullptr;
   dstring buffer = DSTRING_INIT;
   DENTER(TOP_LAYER);  
    
   lXchgList(job, JB_env_list, &env_list);
   {   
      int i = -1;
      const char* env_name[] = {"HOME", "LOGNAME", "PATH", 
                                "SHELL", "TZ", "MAIL", nullptr};

      while (env_name[++i] != nullptr) {
         const char *env_value = sge_getenv(env_name[i]);

         sge_dstring_sprintf(&buffer, "%s%s%s", VAR_PREFIX, "O_",
                             env_name[i]);
         var_list_set_string(&env_list, sge_dstring_get_string(&buffer),
                             env_value);
      }
   }
   {
      const char* host = sge_getenv("HOST"); /* ??? */

      if (host == nullptr) {
         host = unqualified_hostname;
      }
      var_list_set_string(&env_list, VAR_PREFIX "O_HOST", host);
   } 
   {
      char tmp_str[SGE_PATH_MAX + 1];

      if (!getcwd(tmp_str, sizeof(tmp_str))) {
         answer_list_add(answer_list, MSG_ANSWER_GETCWDFAILED, 
                         STATUS_EDISK, ANSWER_QUALITY_ERROR);
         goto error;
      }
      path_alias_list_get_path(path_alias_list, nullptr,
                               tmp_str, qualified_hostname,
                               &buffer);
      var_list_set_string(&env_list, VAR_PREFIX "O_WORKDIR", 
                          sge_dstring_get_string(&buffer));
   }

error:
   sge_dstring_free(&buffer);
   lXchgList(job, JB_env_list, &env_list);
   DRETURN_VOID;
}

/****** sgeobj/job/job_get_env_string() ***************************************
*  NAME
*     job_get_env_string() -- get value of certain job env variable 
*
*  SYNOPSIS
*     const char* job_get_env_string(const lListElem *job, 
*                                    const char *variable) 
*
*  FUNCTION
*     Return the string value of the job environment "variable". 
*
*     Please note: The "*_O_*" env variables get their final
*                  name shortly before job execution. Find more 
*                  information in the comment of
*                  job_initialize_env()
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*     const char* variable - environment variable name 
*
*  RESULT
*     const char* - value of "variable"
*
*  SEE ALSO
*     sgeobj/job/job_initialize_env()
*     sgeobj/job/job_set_env_string() 
******************************************************************************/
const char *job_get_env_string(const lListElem *job, const char *variable)
{
   const char *ret = nullptr;
   DENTER(TOP_LAYER);
   ret = var_list_get_string(lGetList(job, JB_env_list), variable);
   DRETURN(ret);
}

/****** sgeobj/job/job_set_env_string() ***************************************
*  NAME
*     job_set_env_string() -- set value of certain job env variable 
*
*  SYNOPSIS
*     void job_set_env_string(lListElem *job, 
*                             const char* variable, 
*                             const char *value) 
*
*  FUNCTION
*     Set the string "value" of the job environment "variable". 
*
*     Please note: The "*_O_*" env variables get their final
*                  name shortly before job execution. Find more 
*                  information in the comment of
*                  job_initialize_env()
*
*  INPUTS
*     lListElem *job       - JB_Type element 
*     const char* variable - environment variable name 
*     const char* value    - new value 
*
*  SEE ALSO
*     sgeobj/job/job_initialize_env()
*     sgeobj/job/job_get_env_string()
******************************************************************************/
void job_set_env_string(lListElem *job, const char* variable, const char* value)
{
   lList *env_list = nullptr;
   DENTER(TOP_LAYER);  

   lXchgList(job, JB_env_list, &env_list);
   var_list_set_string(&env_list, variable, value);
   lXchgList(job, JB_env_list, &env_list);
   DRETURN_VOID; 
}

/****** sgeobj/job/job_check_correct_id_sublists() ****************************
*  NAME
*     job_check_correct_id_sublists() -- test JB_ja_* sublists 
*
*  SYNOPSIS
*     void 
*     job_check_correct_id_sublists(lListElem *job, lList **answer_list) 
*
*  FUNCTION
*     Test following elements of "job" whether they are correct:
*        JB_ja_structure, JB_ja_n_h_ids, JB_ja_u_h_ids, 
*        JB_ja_s_h_ids, JB_ja_o_h_ids, JB_ja_a_h_ids, JB_ja_z_ids
*     The function will try to correct errors within this lists. If
*     this is not possible an error will be returned in "answer_list".
*      
*
*  INPUTS
*     lListElem *job      - JB_Type element 
*     lList **answer_list - AN_Type list 
*
*  RESULT
*     void - none
******************************************************************************/
void job_check_correct_id_sublists(lListElem *job, lList **answer_list)
{
   DENTER(TOP_LAYER);
   /*
    * Is 0 contained in one of the range lists
    */
   {
      const int field[] = {
         JB_ja_structure,
         JB_ja_n_h_ids,
         JB_ja_u_h_ids,
         JB_ja_s_h_ids,
         JB_ja_o_h_ids,
         JB_ja_a_h_ids,
         JB_ja_z_ids,
         -1
      };
      int i = -1;

      while (field[++i] != -1) {
         const lList *range_list = lGetList(job, field[i]);
         lListElem *range = nullptr;

         for_each_rw(range, range_list) {
            if (field[i] != JB_ja_structure)
               range_correct_end(range);
            if (range_is_id_within(range, 0)) {
               ERROR(SFNMAX, MSG_JOB_NULLNOTALLOWEDT);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN_VOID;
            }
         }
      }
   }    
 
   /*
    * JB_ja_structure and one of the JB_ja_?_h_ids has
    * to comprise at least one id.
    */
   {
      const int field[] = {
         JB_ja_n_h_ids,
         JB_ja_u_h_ids,
         JB_ja_s_h_ids,
         JB_ja_o_h_ids,
         JB_ja_a_h_ids,
         -1
      };
      int has_structure = 0;
      int has_x_ids = 0;
      int i = -1;

      while (field[++i] != -1) {
         const lList *range_list = lGetList(job, field[i]);

         if (!range_list_is_empty(range_list)) {
            has_x_ids = 1;
         }
      }
      has_structure = !range_list_is_empty(lGetList(job, JB_ja_structure));
      if (!has_structure) {
         ERROR(SFNMAX, MSG_JOB_NOIDNOTALLOWED);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN,
                         ANSWER_QUALITY_ERROR);
         DRETURN_VOID;
      } else if (!has_x_ids) {
         job_initialize_id_lists(job, answer_list);
      }
   }
  
   DRETURN_VOID; 
}

/****** sgeobj/job/job_get_id_string() ****************************************
*  NAME
*     job_get_id_string() -- get an id string for a job/jatask/petask
*
*  SYNOPSIS
*     const char *
*     job_get_id_string(u_long32 job_id, u_long32 ja_task_id, 
*                       const char *pe_task_id) 
*
*  FUNCTION
*     Returns an id string for a certain job, ja task or pe task.
*     The function should be used in any code that outputs ids, e.g. 
*     in error strings to ensure we have the same output format 
*     everywhere. If the ja_task_id is 0, only the job id is output.
*
*  INPUTS
*     u_long32 job_id        - the job id
*     u_long32 ja_task_id    - the ja task id or 0 to output only 
*                              job_id
*     const char *pe_task_id - optionally the pe task id
*     dstring *buffer        - a buffer to be used for printing the id string
*
*  RESULT
*     const char* - pointer to a static buffer. It is valid until the 
*                   next call of the function.
*
*  NOTES
*     MT-NOTE: job_get_id_string() is MT safe
******************************************************************************/
const char *job_get_id_string(u_long32 job_id, u_long32 ja_task_id, 
                              const char *pe_task_id, dstring *buffer)
{
   DENTER(TOP_LAYER);

   if (job_id == 0) {
      sge_dstring_sprintf(buffer, "");
   } else {
      if (ja_task_id == 0) {
         sge_dstring_sprintf(buffer, MSG_JOB_JOB_ID_U, job_id);
      } else {
         if (pe_task_id == nullptr) {
            sge_dstring_sprintf(buffer, MSG_JOB_JOB_JATASK_ID_UU, job_id, ja_task_id);
         } else {
            sge_dstring_sprintf(buffer, MSG_JOB_JOB_JATASK_PETASK_ID_UUS, job_id, ja_task_id, pe_task_id);
         }
      }
   }   
   
   DRETURN(sge_dstring_get_string(buffer));
}

/****** sgeobj/job/job_is_pe_referenced() *************************************
*  NAME
*     job_is_pe_referenced() -- Does job reference the given PE? 
*
*  SYNOPSIS
*     bool job_is_pe_referenced(const lListElem *job, 
*                              const lListElem *pe) 
*
*  FUNCTION
*     The function returns true if "job" references the "pe". 
*     This is also the case if job requests a wildcard PE and 
*     the wildcard name matches the given pe name. 
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*     const lListElem *pe  - PE_Type object 
*
*  RESULT
*     int - true or false 
******************************************************************************/
bool job_is_pe_referenced(const lListElem *job, const lListElem *pe)
{
   const char *ref_pe_name = lGetString(job, JB_pe);
   bool ret = false;

   if(ref_pe_name != nullptr) {
      if (pe_is_matching(pe, ref_pe_name)) {
         ret = true;
      }
   }
   return ret;
}

/****** sgeobj/job/job_is_ckpt_referenced() ***********************************
*  NAME
*     job_is_ckpt_referenced() -- Does job reference the given CKPT? 
*
*  SYNOPSIS
*     bool job_is_ckpt_referenced(const lListElem *job, 
*                                 const lListELem *ckpt) 
*
*  FUNCTION
*     The function returns true if "job" references the 
*     checkpointing object "ckpt". 
*
*  INPUTS
*     const lListElem *job  - JB_Type element 
*     const lListElem *ckpt - CK_Type object 
*
*  RESULT
*     bool - true or false 
******************************************************************************/
bool job_is_ckpt_referenced(const lListElem *job, const lListElem *ckpt)
{
   const char *ckpt_name = lGetString(ckpt, CK_name);
   const char *ref_ckpt_name = lGetString(job, JB_checkpoint_name);
   bool ret = false;

   if(ckpt_name != nullptr && ref_ckpt_name != nullptr) {
      if (!strcmp(ref_ckpt_name, ckpt_name)) {
         ret = true;
      }
   }
   return ret;
}

/****** sgeobj/job/job_get_state_string() *************************************
*  NAME
*     job_get_state_string() -- write job state flags into a string 
*
*  SYNOPSIS
*     void job_get_state_string(char *str, u_long32 op) 
*
*  FUNCTION
*     This function writes the state flags given by 'op' into the 
*     string 'str'
*
*  INPUTS
*     char *str   - containes the state flags for 'qstat'/'qhost' 
*     u_long32 op - job state bitmask 
******************************************************************************/
/* JG: TODO: use dstring! */
void job_get_state_string(char *str, u_long32 op)
{
   int count = 0;

   DENTER(TOP_LAYER);

   if (VALID(JDELETED, op)) {
      str[count++] = DISABLED_SYM;
   }

   if (VALID(JERROR, op)) {
      str[count++] = ERROR_SYM;
   }

   if (VALID(JSUSPENDED_ON_SUBORDINATE, op) ||
       VALID(JSUSPENDED_ON_SLOTWISE_SUBORDINATE, op)) {
      str[count++] = SUSPENDED_ON_SUBORDINATE_SYM;
   }
   
   if (VALID(JSUSPENDED_ON_THRESHOLD, op)) {
      str[count++] = SUSPENDED_ON_THRESHOLD_SYM;
   }

   if (VALID(JHELD, op)) {
      str[count++] = HELD_SYM;
   }

   if (VALID(JMIGRATING, op)) {
      str[count++] = RESTARTING_SYM;
   }

   if (VALID(JQUEUED, op)) {
      str[count++] = QUEUED_SYM;
   }

   if (VALID(JRUNNING, op)) {
      str[count++] = RUNNING_SYM;
   }

   if (VALID(JSUSPENDED, op)) {
      str[count++] = SUSPENDED_SYM;
   }

   if (VALID(JTRANSFERING, op)) {
      str[count++] = TRANSISTING_SYM;
   }

   if (VALID(JWAITING, op)) {
      str[count++] = WAITING_SYM;
   }

   if (VALID(JEXITING, op)) { 
      str[count++] = EXITING_SYM;
   }

   str[count++] = '\0';

   DRETURN_VOID;
}

/****** sgeobj/job/job_add_parent_id_to_context() *****************************
*  NAME
*     job_add_parent_id_to_context() -- add parent jobid to job context  
*
*  SYNOPSIS
*     void job_add_parent_id_to_context(lListElem *job) 
*
*  FUNCTION
*     If we have JOB_ID in environment implicitly put it into the 
*     job context variable PARENT if was not explicitly set using 
*     "-sc PARENT=$JOBID". By doing this we preserve information 
*     about the relationship between these two jobs. 
*
*  INPUTS
*     lListElem *job - JB_Type element 
*
*  RESULT
*     void - None
******************************************************************************/
void job_add_parent_id_to_context(lListElem *job) 
{
   const char *job_id_string = sge_getenv("JOB_ID");
   lListElem *context_parent = lGetSubStrRW(job, VA_variable, CONTEXT_PARENT, JB_context);
   if (job_id_string != nullptr && context_parent == nullptr) {
      context_parent = lAddSubStr(job, VA_variable, CONTEXT_PARENT, JB_context, VA_Type);
      lSetString(context_parent, VA_value, job_id_string);
   }
}

/****** sgeobj/job/job_check_qsh_display() ************************************
*  NAME
*     job_check_qsh_display() -- check DISPLAY variable for qsh jobs 
*
*  SYNOPSIS
*     int 
*     job_check_qsh_display(const lListElem *job, lList **answer_list, 
*                           bool output_warning) 
*
*  FUNCTION
*     Checks the DISPLAY variable for qsh jobs:
*     - existence
*     - empty string
*     - local variable
*
*     In each error cases, an appropriate error message is generated.
*     If output_warning is set to true, an error message is output.
*     In each case, an error message is written into answer_list.
*
*  INPUTS
*     const lListElem *job - the job to check
*     lList **answer_list  - answer list to take error messages, if 
*                            nullptr, no answer is passed back.
*     bool output_warning  - output error messages to stderr?
*
*  RESULT
*     int - STATUS_OK, if function call succeeds,
*           else STATUS_EUNKNOWN.
*
*  NOTES
*     To fully hide the data representation of the DISPLAY settings, 
*     functions job_set_qsh_display and job_get_qsh_display would
*     be usefull.
******************************************************************************/
int job_check_qsh_display(const lListElem *job, lList **answer_list, 
                          bool output_warning)
{
   const lListElem *display_ep;
   const char *display;

   DENTER(TOP_LAYER);

   /* check for existence of DISPLAY */
   display_ep = lGetElemStr(lGetList(job, JB_env_list), VA_variable, "DISPLAY");
   if(display_ep == nullptr) {
      dstring id_dstring = DSTRING_INIT;
      if(output_warning) {
         WARNING(MSG_JOB_NODISPLAY_S, job_get_id_string(lGetUlong(job, JB_job_number), 0, nullptr, &id_dstring));
      } else {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_NODISPLAY_S, job_get_id_string(lGetUlong(job, JB_job_number), 0, nullptr, &id_dstring));
      }
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&id_dstring);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* check value of display variable, if it is an empty string,
    * it is useless in a grid environment.
    */
   display = lGetString(display_ep, VA_value);
   if(display == nullptr || strlen(display) == 0) {
      dstring id_dstring = DSTRING_INIT;
      if(output_warning) {
         WARNING(MSG_JOB_EMPTYDISPLAY_S, job_get_id_string(lGetUlong(job, JB_job_number), 0, nullptr, &id_dstring));
      } else {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_EMPTYDISPLAY_S, job_get_id_string(lGetUlong(job, JB_job_number), 0, nullptr, &id_dstring));
      }
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&id_dstring);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* check value of display variable, if it has the form :<id> (local display)
    * it is useless in a grid environment.
    */
   if(*display == ':') {
      dstring id_dstring = DSTRING_INIT;
      if(output_warning) {
         WARNING(MSG_JOB_LOCALDISPLAY_SS, display, job_get_id_string(lGetUlong(job, JB_job_number), 0, nullptr, &id_dstring));
      } else {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_LOCALDISPLAY_SS, display, job_get_id_string(lGetUlong(job, JB_job_number), 0, nullptr, &id_dstring));
      }
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&id_dstring);
      DRETURN(STATUS_EUNKNOWN);
   }

   DRETURN(STATUS_OK);
}

/****** sgeobj/job/job_check_owner() ******************************************
*  NAME
*     job_check_owner() -- check the owner of a job
*
*  SYNOPSIS
*     int job_check_owner(const char *user_name, u_long32 job_id) 
*
*  FUNCTION
*     Checks if the owner of the job specified by job_id is the
*     user given by user_name.
*
*  INPUTS
*     const char *user_name      - the user name 
*     u_long32   job_id          - the job number
*     lList      master_job_list - a ref to the master job list
*
*  RESULT
*     int - -1, if the job cannot be found
*            0, if the user is the job owner
*            1, if the user is not the job owner
******************************************************************************/
int job_check_owner(const ocs::gdi::Packet *packet, u_long32 job_id, lList *master_job_list, const lList *master_manager_list, const lList *master_operator_list)
{
   const lListElem *job;

   DENTER(TOP_LAYER);

   if (manop_is_operator(packet, master_manager_list, master_operator_list)) {
      DRETURN(0);
   }

   job = lGetElemUlong(master_job_list, JB_job_number, job_id);
   if (job == nullptr) {
      DRETURN(-1);
   }

   if (strcmp(packet->user, lGetString(job, JB_owner)) != 0) {
      DRETURN(1);
   }

   DRETURN(0);
}

/****** sgeobj/job/job_get_job_key() **********************************************
*  NAME
*     job_get_job_key() -- create a unique key 
*
*  SYNOPSIS
*     const char* job_get_job_key(u_long32 job_id)
*
*  FUNCTION
*     Creates a unique key consisting of the job_id.
*     The job id can reread by calling job_parse_key().
*
*  INPUTS
*     u_long32 job_id        - job id
*
*  RESULT
*     const char* - pointer to a static buffer containing the key.
*                   The result is only valid until the next call of the 
*                   function.
*
*  NOTES
*     MT-NOTE: job_get_job_key() is MT safe
*
*  SEE ALSO
*     sgeobj/job/job_get_key()
*     sgeobj/job/job_parse_key()
******************************************************************************/
const char *job_get_job_key(u_long32 job_id, dstring *buffer)
{
   const char *ret = nullptr;
   DENTER(TOP_LAYER);
   if (buffer != nullptr) {
      ret = sge_dstring_sprintf(buffer, "%d", job_id);
   }

   DRETURN(ret);
}

/****** sgeobj/job/job_get_key() **********************************************
*  NAME
*     job_get_key() -- create a unique key
*
*  SYNOPSIS
*     const char* job_get_key(u_long32 job_id, u_long32 ja_task_id,
*                             const char *pe_task_id, dstring *buffer)
*
*  FUNCTION
*     Creates a unique key consisting of job_id, ja_task_id and
*     pe_task_id. This key can again be split into its components
*     by a call to job_parse_key().
*
*  INPUTS
*     u_long32 job_id        - job id
*     u_long32 ja_task_id    - ja task id
*     const char *pe_task_id - pe task id
*     dstring *buffer        - dstring buffer used to generate the key
*
*  RESULT
*     const char* - pointer to the key, stored within buffer
*
*  NOTES
*     MT-NOTE: job_get_key() is MT safe
*
*  SEE ALSO
*     sgeobj/job/job_parse_key()
******************************************************************************/
const char *job_get_key(u_long32 job_id, u_long32 ja_task_id,
                        const char *pe_task_id, dstring *buffer)
{
   const char *ret = nullptr;
   DENTER(TOP_LAYER);
   if (buffer != nullptr) {
      if (ja_task_id == 0) {
         ret = sge_dstring_sprintf(buffer, "%d", job_id);
      } else if (pe_task_id != nullptr) {
         ret = sge_dstring_sprintf(buffer, "%d.%d %s",
                                   job_id, ja_task_id, pe_task_id);
      } else {
         ret = sge_dstring_sprintf(buffer, "%d.%d",
                                   job_id, ja_task_id);
      }
   }

   DRETURN(ret);
}

/****** sgeobj/job/job_parse_key() ********************************************
*  NAME
*     job_parse_key() -- parse a key generated by job_get_key()
*
*  SYNOPSIS
*     bool 
*     job_parse_key(char *key, u_long32 *job_id, u_long32 *ja_task_id, 
*                   char **pe_task_id, bool *only_job) 
*
*  FUNCTION
*     Parse a key generated by job_get_key() and split it into its 
*     components.
*
*  INPUTS
*     char *key            - key to be parsed
*     u_long32 *job_id     - pointer to job_id
*     u_long32 *ja_task_id - pointer to ja_task_id
*     char **pe_task_id    - pointer to pe_task_id
*     bool *only_job       - true, if only a job id is contained in 
*                            key, else false.
*
*  RESULT
*     bool - true, if the key could be parsed, else false
*
*  NOTES
*     MT-NOTE: job_get_key() is MT safe
*
*     The pe_task_id is only valid until the passed key is deleted!
*
*  SEE ALSO
*     sgeobj/job/job_get_key()
******************************************************************************/
bool job_parse_key(char *key, u_long32 *job_id, u_long32 *ja_task_id,
                   char **pe_task_id, bool *only_job)
{
   const char *ja_task_id_str;
   char *lasts = nullptr;

   DENTER(TOP_LAYER);
   *job_id = atoi(strtok_r(key, ".", &lasts));
   ja_task_id_str = strtok_r(nullptr, " ", &lasts);
   if (ja_task_id_str == nullptr) {
      *ja_task_id = 0;
      *pe_task_id = nullptr;
      *only_job  = true;
   } else {
      *ja_task_id = atoi(ja_task_id_str);
      *pe_task_id = strtok_r(nullptr, " ", &lasts);
      *only_job = false;
   }

   if(*pe_task_id != nullptr && strlen(*pe_task_id) == 0) {
      *pe_task_id = nullptr;
   }

   DRETURN(true);
}

/****** sgeobj/job/jobscript_get_key() **********************************************
*  NAME
*     jobscript_get_key() -- create a unique key 
*
*  SYNOPSIS
*     const char* jobscript_get_key(lListElem *jep, dstring *buffer)
*
*  FUNCTION
*     Creates a unique key consisting of job_id and jobscript name
*     This key can again be split into its components 
*     by a call to jobscript_parse_key()
*
*  INPUTS
*     u_long32 job_id        - job id
*     dstring *buffer        - buffer
*
*  RESULT
*     const char* - pointer to a static buffer containing the key.
*     
*                   The result is only valid until the next call of the 
*                   function.
*
*  NOTES
*     MT-NOTE: jobscript_get_key() is MT safe
*
*  SEE ALSO
*     sgeobj/job/jobscript_parse_key()
*
******************************************************************************/
const char *jobscript_get_key(const lListElem *jep, dstring *buffer)
{
   const char *ret = nullptr;

   DENTER(TOP_LAYER);
   if (buffer != nullptr) {
      ret = sge_dstring_sprintf(buffer, "%s:" sge_u32 ".%s",
                                object_type_get_name(SGE_TYPE_JOBSCRIPT), 
                                lGetUlong(jep, JB_job_number), lGetString(jep, JB_exec_file));
   }
   DRETURN(ret);
}


/****** sgeobj/job/jobscript_parse_key() ********************************************
*  NAME
*     jobscript_parse_key() -- parse a key generated by job_get_key()
*
*  SYNOPSIS
*     const char *  job_parse_key(char *key, const char **exec_file) 
*
*  FUNCTION
*     Parse a key generated by jobscript_get_key() 
*
*  INPUTS
*     char *key                 - key to be parsed
*     onst char **exec_file     - exec_file name to unlink
*
*  RESULT
*     const char * the database job key
*
*  NOTES
*     MT-NOTE: jobscript_parse_key() is MT safe
*
*
*  SEE ALSO
*     sgeobj/job/jobscript_get_key()
******************************************************************************/
char *jobscript_parse_key(char *key, const char **exec_file)
{
   char *lasts = nullptr;
   char *ret = nullptr;
   DENTER(TOP_LAYER);
   ret = strtok_r(key, ".", &lasts);
   *exec_file = strtok_r(nullptr, ".", &lasts);
   DRETURN(ret);
}

/****** sgeobj/job/job_resolve_host_for_path_list() ***************************
*  NAME
*     job_resolve_host_for_path_list() -- resolves hostnames in path lists 
*
*  SYNOPSIS
*     int 
*     job_resolve_host_for_path_list(const lListElem *job, 
*                                    lList **answer_list, int name) 
*
*  FUNCTION
*     Resolves hostnames in path lists. 
*
*  INPUTS
*     const lListElem *job - the submited cull list 
*     lList **answer_list  - AN_Type element
*     int name             - a JB_Type (JB_stderr_path_list or 
*                                       JB_stout_path_list)
*
*  RESULT
*     int - error code (STATUS_OK, or ...) 
*******************************************************************************/
int job_resolve_host_for_path_list(const lListElem *job, lList **answer_list, 
                                   int name)
{
   bool ret_error=false;
   lListElem *ep;

   DENTER(TOP_LAYER);

   for_each_rw(ep, lGetList(job, name)){
      int res = sge_resolve_host(ep, PN_host);
      DPRINTF("after sge_resolve_host() which returned %s\n", cl_get_error_text(res));
      if (res != CL_RETVAL_OK) { 
         const char *hostname = lGetHost(ep, PN_host);
         if (hostname != nullptr) {
            ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, hostname);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret_error=true;
         } else if (res != CL_RETVAL_PARAMS) {
            ERROR(SFNMAX, MSG_PARSE_NULLPOINTERRECEIVED);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret_error=true;
         }
      } 
      DPRINTF("after sge_resolve_host() - II\n");

      /* ensure, that each hostname is only specified once */
      if( !ret_error ){
         const char *hostname = lGetHost(ep, PN_host);       
         const lListElem *temp;         

         for (temp= lPrev(ep); temp; temp =lPrev(temp)){
            const char *temp_hostname = lGetHost(temp, PN_host);

            if(hostname == nullptr){
               if(temp_hostname == nullptr){
                  ERROR(SFNMAX, MSG_PARSE_DUPLICATEHOSTINFILESPEC);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                  ret_error=true;
               }
            } 
            else if( temp_hostname && strcmp(hostname, temp_hostname)==0){
               ERROR(SFNMAX, MSG_PARSE_DUPLICATEHOSTINFILESPEC);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret_error=true;
            }
            if(ret_error)
               break;
         }/* end for */ 
      }

      if(ret_error) {
         break;
      }
   }/*end for*/

   if(ret_error) {
      DRETURN(STATUS_EUNKNOWN);
   } else {
      DRETURN(STATUS_OK);
   }     
}

/****** sgeobj/job/job_get_request() ******************************************
*  NAME
*     job_get_request() -- Returns the requested centry name 
*
*  SYNOPSIS
*     lListElem * 
*     job_get_request(const lListElem *this_elem, const char **centry_name) 
*
*  FUNCTION
*     Returns the requested centry name if it is requested by the give
*     job (JB_Type). 
*
*  INPUTS
*     const lListElem *this_elem - JB_Type element 
*     const char *centry_name    - name 
*
*  RESULT
*     lListElem * - CE_Type element
*
*  NOTES
*     MT-NOTE: job_get_request() is MT safe 
*******************************************************************************/
const lListElem *
job_get_request(const lListElem *job, const char *centry_name)
{
   DENTER(TOP_LAYER);

   const lListElem *ret;

   const lList *hard_centry_list = job_get_hard_resource_list(job);
   ret = lGetElemStr(hard_centry_list, CE_name, centry_name);
   if (ret == nullptr) {
      const lList *soft_centry_list = job_get_soft_resource_list(job);
      ret = lGetElemStr(soft_centry_list, CE_name, centry_name);
   }
   DRETURN(ret);
}

const lListElem *
job_get_hard_request(const lListElem *job, const char *name, bool is_master_task) {
   DENTER(TOP_LAYER);
   const lListElem *ret = nullptr;

   // a request can be either global
   const lList *request_list = job_get_hard_resource_list(job, JRS_SCOPE_GLOBAL);
   if (request_list != nullptr) {
      ret = lGetElemStr(request_list, CE_name, name);
   }

   // or (only for pe-jobs) a master or slave request
   if (ret == nullptr) {
      request_list = job_get_hard_resource_list(job, is_master_task ? JRS_SCOPE_MASTER : JRS_SCOPE_SLAVE);
      if (request_list != nullptr) {
         // @todo can we rely on CE_name or can it be the shortcut? Use centry_list_locate?
         ret = lGetElemStr(request_list, CE_name, name);
      }
   }

   DRETURN(ret);
}

const lListElem *
job_get_hard_request(const lListElem *job, const char *name, u_long32 scope) {
   DENTER(TOP_LAYER);

   const lListElem *ret = nullptr;

   // a request can be either global
   const lList *request_list = job_get_hard_resource_list(job, scope);
   if (request_list != nullptr) {
      // @todo can we rely on CE_name or can it be the shortcut? Use centry_list_locate?
      ret = lGetElemStr(request_list, CE_name, name);
   }

   DRETURN(ret);
}

bool
job_get_contribution(const lListElem *job, lList **answer_list, const char *name, double *value,
                     const lListElem *complex_definition, bool is_master_task)
{
   bool ret = true;
   const lListElem *centry = nullptr;
   const char *value_string = nullptr;
   char error_str[256];

   DENTER(TOP_LAYER);

   // we only consider *hard* requests (consumables), there are no soft consumables
   centry = job_get_hard_request(job, name, is_master_task);
   if (centry != nullptr) {
      // @todo CS-537 could we rely on CE_doubleval? Would spare us the string parsing below.
      value_string = lGetString(centry, CE_stringval);
   } else {
      // if the job did not request the consumable, there might still be a default request
      // @todo CE-459 if there was a CE_default_doubleval we wouldn't have to parse the string
      value_string = lGetString(complex_definition, CE_defaultval);
   }
   if (!(parse_ulong_val(value, nullptr, TYPE_INT, value_string,
                         error_str, sizeof(error_str)-1))) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_ATTRIB_PARSATTRFAILED_SS, name, error_str);
      ret = false;
   }

   DRETURN(ret);
}

bool
job_get_contribution_by_scope(const lListElem *job, lList **answer_list, const char *name, double *value,
                              const lListElem *complex_definition, u_long32 scope)
{
   bool ret = true;
   const lListElem *centry = nullptr;
   const char *value_string = nullptr;
   char error_str[256];
   bool is_default_request = false;
   
   DENTER(TOP_LAYER);

   // we only consider *hard* requests (consumables), there are no soft consumables
   centry = job_get_hard_request(job, name, scope);
   if (centry != nullptr) {
      // @todo CS-537 could we rely on CE_doubleval? Would spare us the string parsing below.
      value_string = lGetString(centry, CE_stringval);
   } else {
      // if the job did not request the consumable, there might still be a default request
      // @todo CE-459 if there was a CE_default_doubleval we wouldn't have to parse the string
      value_string = lGetString(complex_definition, CE_defaultval);
      is_default_request = true;
   }
   if (!(parse_ulong_val(value, nullptr, TYPE_INT, value_string, error_str, sizeof(error_str)-1))) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_ATTRIB_PARSATTRFAILED_SS, name, error_str); 
      ret = false; 
   }
   if (is_default_request && *value == 0) {
      DPRINTF("job_get_contribution_by_scope: default request for %s is 0, ignoring\n", name);
      ret = false;
   }
   
   DRETURN(ret);
}

// adjust the slot count used for debiting of slave tasks
// called when we just debited the master task
// we need to reduce the slot count by one
// exception:
//    - the pe setting job_is_first_task = false (in this case there was no slot for the master task)
//    - unless the slot count is already +-1, then we had a single master task without slave task
//      @todo really? What if there is the master task and one slave task in the qinstance?
//        unless slots == +-1, then we are only booking the master task here
//        ==> reason for the alleged bug CS-547 we will always have a slave task with the master task
//            when job_is_first_task = false?
// for JOB and HOST variables debit_slots was already +-1, so we will not book them for slave again
void
adjust_slave_task_debit_slots(const lListElem *pe, int &slave_debit_slots) {
   bool job_is_first_task = true;
   if (pe != nullptr) {
      job_is_first_task = lGetBool(pe, PE_job_is_first_task);
   }

   if (job_is_first_task /* || abs(slave_debit_slots) == 1 */) {
      if (slave_debit_slots > 0) {
         slave_debit_slots--;
      } else {
         slave_debit_slots++;
      }
   }
}

/****** sge_job/sge_unparse_acl_dstring() **************************************
*  NAME
*     sge_unparse_acl_dstring() -- creates a string from the access lists and user
*
*  SYNOPSIS
*     bool sge_unparse_acl_dstring(dstring *category_str, const char *owner, 
*     const char *group, const lList *acl_list, const char *option) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     dstring *category_str - target string
*     const char *owner     - job owner
*     const char *group     - group owner
*     const lList *acl_list - a list of all access lists
*     const char *option    - string option to put in infront of the generated string
*
*  RESULT
*     bool - true, if everything was fine
*
*  NOTES
*     MT-NOTE: sge_unparse_acl_dstring() is MT safe 
*
*******************************************************************************/
bool sge_unparse_acl_dstring(dstring *category_str, const char *owner, const char *group, const lList *grp_list,
                             const lList *acl_list, const char *option) 
{
   bool first = true;
   const lListElem *elem = nullptr;
  
   DENTER(TOP_LAYER);  

   for_each_ep(elem, acl_list) {
      if (lGetBool(elem, US_consider_with_categories) &&
          sge_contained_in_access_list(owner, group, grp_list, elem)) {
         if (first) {      
            sge_dstring_append(category_str, option);
            sge_dstring_append_char(category_str, ' ');
            first = false;
         } else {
            sge_dstring_append_char(category_str, ',');
         }
         sge_dstring_append(category_str, lGetString(elem, US_name));
      }
   }
   if (!first) {
      sge_dstring_append_char(category_str, ' ');
   }

   DRETURN(true);
}


/****** sge_job/sge_unparse_queue_list_dstring() *******************************
*  NAME
*     sge_unparse_queue_list_dstring() -- creates a string from a queue name list
*
*  SYNOPSIS
*     bool sge_unparse_queue_list_dstring(dstring *category_str, const 
*     lListElem *job_elem, int nm, const char *option) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     dstring *category_str     - target string
*     const lListElem *job_elem - a job structure
*     int nm                    - position in of the queue list attribute in the job 
*     const char *option        - string option to put in infront of the generated string
*
*  RESULT
*     bool - true, if everything was fine
*
*  NOTES
*     MT-NOTE: sge_unparse_queue_list_dstring() is MT safe 
*
*******************************************************************************/
bool sge_unparse_queue_list_dstring(dstring *category_str, lList *queue_list, const char *option)
{
   DENTER(TOP_LAYER);
  
   if (queue_list != nullptr) {
      lPSortList(queue_list, "%I+", QR_name);

      bool first = true;
      const lListElem *sub_elem;
      for_each_ep(sub_elem, queue_list) {
         if (first) {      
            sge_dstring_append(category_str, option);
            sge_dstring_append_char(category_str, ' ');
            first = false;
         } else {
            sge_dstring_append_char(category_str, ',');
         }
         sge_dstring_append(category_str, lGetString(sub_elem, QR_name));
      }
      sge_dstring_append_char(category_str, ' ');
   }

   DRETURN(true);
}

/****** sge_job/sge_unparse_resource_list_dstring() ****************************
*  NAME
*     sge_unparse_resource_list_dstring() -- creates a string from resource requests
*
*  SYNOPSIS
*     bool sge_unparse_resource_list_dstring(dstring *category_str, lListElem 
*     *job_elem, int nm, const char *option) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     dstring *category_str - target string
*     lListElem *job_elem   - a job structure
*     int nm                - position of the resource list attribute in the job
*     const char *option    - string option to put in infront of the generated string
*
*  RESULT
*     bool - true, if everything was fine
*
*  NOTES
*     MT-NOTE: sge_unparse_resource_list_dstring() is MT safe 
*
*******************************************************************************/
bool sge_unparse_resource_list_dstring(dstring *category_str, lList *resource_list, const char *option)
{
   DENTER(TOP_LAYER);

   if (resource_list != nullptr) {
      lPSortList(resource_list, "%I+", CE_name);

       bool first = true;
       const lListElem *sub_elem;
       for_each_ep(sub_elem, resource_list) {
         if (first) {
            sge_dstring_append(category_str, option);
            sge_dstring_append(category_str, " ");
            first = false;
         } else {
            sge_dstring_append(category_str, ",");
         }
         sge_dstring_append(category_str, lGetString(sub_elem, CE_name));
         sge_dstring_append(category_str, "=");
         sge_dstring_append(category_str, lGetString(sub_elem, CE_stringval));
      }
      sge_dstring_append(category_str, " ");
   }
   
   DRETURN(true);
}

/****** sge_job/sge_unparse_pe_dstring() ***************************************
*  NAME
*     sge_unparse_pe_dstring() -- creates a string from a pe request
*
*  SYNOPSIS
*     bool sge_unparse_pe_dstring(dstring *category_str, const lListElem 
*     *job_elem, int pe_pos, int range_pos, const char *option) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     dstring *category_str     - target string
*     const lListElem *job_elem - a job structure
*     int pe_pos                - position of the pe name attribute in the job
*     int range_pos             - position of the pe range request attribute in the job
*     const char *option        - string option to put in infront of the generated string
*
*  RESULT
*     bool - true, if everything was fine
*
*  NOTES
*     MT-NOTE: sge_unparse_pe_dstring() is MT safe 
*
*******************************************************************************/
bool sge_unparse_pe_dstring(dstring *category_str, const lListElem *job_elem, int pe_pos, int range_pos,
                            const char *option) 
{
   const lList *range_list = nullptr;
   
   DENTER(TOP_LAYER); 

   if (lGetPosString(job_elem, pe_pos) != nullptr) {
      if ((range_list = lGetPosList(job_elem, range_pos)) == nullptr) {
         DPRINTF("Job has parallel environment with no ranges\n");
         DRETURN(false);
      } else {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(range_list, &range_string, true, false, false);
         sge_dstring_append(category_str, option);
         sge_dstring_append_char(category_str, ' ');
         sge_dstring_append(category_str, lGetString(job_elem, JB_pe));
         sge_dstring_append_char(category_str, ' ');
         sge_dstring_append_dstring(category_str, &range_string);
         sge_dstring_append_char(category_str, ' ');

         sge_dstring_free(&range_string);
      }
      
   }

   DRETURN(true);
}

/****** sge_job/sge_unparse_string_option_dstring() ****************************
*  NAME
*     sge_unparse_string_option_dstring() -- copies a string into a dstring
*
*  SYNOPSIS
*     bool sge_unparse_string_option_dstring(dstring *category_str, const 
*     lListElem *job_elem, int nm, char *option) 
*
*  FUNCTION
*     Copies a string into a dstring. Used for category string building
*
*  INPUTS
*     dstring *category_str     - target string
*     const lListElem *job_elem - a job structure
*     int nm                    - position of the string attribute in the job
*     char *option              - string option to put in in front of the generated string
*
*  RESULT
*     bool - always true
*
*  NOTES
*     MT-NOTE: sge_unparse_string_option_dstring() is MT safe 
*
*  SEE ALSO
*     sge_job/sge_unparse_ulong_option_dstring()
*******************************************************************************/
bool sge_unparse_string_option_dstring(dstring *category_str, const lListElem *job_elem, int nm, const char *option) {
   DENTER(TOP_LAYER);
   if (const char *string = lGetPosString(job_elem, nm); string != nullptr) {
      sge_dstring_append(category_str, option);
      sge_dstring_append_char(category_str, ' ');
      sge_dstring_append(category_str, string);
      sge_dstring_append_char(category_str, ' ');
   }
   DRETURN(true);
}

/****** sge_job/sge_unparse_ulong_option_dstring() *****************************
*  NAME
*     sge_unparse_ulong_option_dstring() -- copies a string into a dstring
*
*  SYNOPSIS
*     bool sge_unparse_ulong_option_dstring(dstring *category_str, const 
*     lListElem *job_elem, int nm, char *option) 
*
*  FUNCTION
*     Copies a string into a dstring. Used for category string building
*
*  INPUTS
*     dstring *category_str     - target string
*     const lListElem *job_elem - a job structure
*     int nm                    - position of the string attribute in the job
*     char *option              - string option to put in front of the generated string
*
*  RESULT
*     bool - always true
*
*  EXAMPLE
*     ??? 
*
*  NOTES
*     MT-NOTE: sge_unparse_ulong_option_dstring() is MT safe 
*
*  SEE ALSO
*     sge_job/sge_unparse_string_option_dstring()
*******************************************************************************/
bool
sge_unparse_ulong_option_dstring(dstring *category_str, const lListElem *job_elem, int nm, const char *option) {
   DENTER(TOP_LAYER);
   
   if (const u_long32 ul = lGetPosUlong(job_elem, nm); ul != 0) {
      sge_dstring_append(category_str, option);
      sge_dstring_append_char(category_str, ' ');
      sge_dstring_sprintf_append(category_str, sge_u32, ul);
      sge_dstring_append_char(category_str, ' ');
   }
   DRETURN(true);
}

/****** sge_job/job_verify() ***************************************************
*  NAME
*     job_verify() -- verify a job object
*
*  SYNOPSIS
*     bool 
*     job_verify(const lListElem *job, lList **answer_list) 
*
*  FUNCTION
*     Verifies structure and contents of a job object.
*     As a job object may look quite different depending on its state,
*     additional functions are provided, calling this function doing 
*     general tests and doing additional verification themselves.
*
*  INPUTS
*     const lListElem *job - the job object to verify
*     lList **answer_list  - answer list to pass back error messages
*     bool do_cull_verify  - do cull verification against the JB_Type descriptor.
*
*  RESULT
*     bool - true on success,
*            false on error with error message in answer_list
*
*  NOTES
*     MT-NOTE: job_verify() is MT safe 
*
*  BUGS
*     The function is far from being complete.
*     Currently, only the CULL structure is verified, not the contents.
*
*  SEE ALSO
*     sge_object/object_verify_cull()
*     sge_job/job_verify_submitted_job()
*     sge_job/job_verify_execd_job()
*******************************************************************************/
bool 
job_verify(const lListElem *job, lList **answer_list, bool do_cull_verify)
{
   bool ret = true;

   DENTER(TOP_LAYER);

   if (job == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_NULLELEMENTPASSEDTO_S, "job_verify");
      DRETURN(false);
   }

   if (ret && do_cull_verify) {
      if (!object_verify_cull(job, JB_Type)) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_OBJECT_STRUCTURE_ERROR);
         ret = false;
      }
   }

   if (ret) {
      const char *name = lGetString(job, JB_job_name);
      if (name != nullptr) {
         if (strlen(name) >= MAX_VERIFY_STRING) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                    MSG_JOB_NAMETOOLONG_I, MAX_VERIFY_STRING);
            ret = false;
         }
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_JOB_NOJOBNAME);
         ret = false;
      }
   }

   if (ret) {
      const char *cwd = lGetString(job, JB_cwd);

      if (cwd != nullptr) {
         /*
          * cwd needn't be an absolute path, we also accept 
          * relative paths, e.g. via -wd switch, 
          * or even pseudo variables, e.g. $HOME used by drmaa submit
          */
         ret = path_verify(cwd, answer_list, "cwd", false);
      }
   }

   if (ret) {
      const lList *path_aliases = lGetList(job, JB_path_aliases);

      if (path_aliases != nullptr) {
         ret = path_alias_verify(path_aliases, answer_list);
      }
   } 

   if (ret) {
      const lList *env_list = lGetList(job, JB_env_list);

      if (env_list != nullptr) {
         ret = var_list_verify(env_list, answer_list);
      }
   } 

   if (ret) {
      const lList *context_list = lGetList(job, JB_context);

      if (context_list != nullptr) {
         ret = var_list_verify(context_list, answer_list);
      }
   }

   if (ret) {
      ret = path_list_verify(lGetList(job, JB_stdout_path_list), answer_list, "stdout path");
   }

   if (ret) {
      ret = path_list_verify(lGetList(job, JB_stderr_path_list), answer_list, "stderr path");
   }

   if (ret) {
      ret = path_list_verify(lGetList(job, JB_stdin_path_list), answer_list, "stdin path");
   }

   DRETURN(ret);
}

/****** sge_job/job_verify_submitted_job() *************************************
*  NAME
*     job_verify_submitted_job() -- verify a just submitted job
*
*  SYNOPSIS
*     bool 
*     job_verify_submitted_job(const lListElem *job, lList **answer_list) 
*
*  FUNCTION
*     Verifies a just submitted job object.
*     Does generic tests by calling job_verify, like verifying the cull
*     structure, and makes sure a number of job attributes are set
*     correctly.
*
*  INPUTS
*     const lListElem *job - the job to verify
*     lList **answer_list  - answer list to pass back error messages
*
*  RESULT
*     bool - true on success,
*            false on error with error message in answer_list
*
*  NOTES
*     MT-NOTE: job_verify_submitted_job() is MT safe 
*
*  BUGS
*     The function is far from being complete.
*     Currently, only the CULL structure is verified, not the contents.
*
*  SEE ALSO
*     sge_job/job_verify()
*******************************************************************************/
bool 
job_verify_submitted_job(lListElem *job, lList **answer_list)
{
   bool ret = true;

   DENTER(TOP_LAYER);

   ret = job_verify(job, answer_list, true);

   /* JB_job_number must be 0 */
   if (ret) {
      ret = object_verify_ulong_null(job, answer_list, JB_job_number);
   }

   /* JB_version must be 0 */
   if (ret) {
      ret = object_verify_ulong_null(job, answer_list, JB_version);
   }

   /* TODO: JB_jid_request_list */
   /* TODO: JB_jid_predecessor_l */
   /* TODO: JB_jid_sucessor_list */

   /* JB_session must be a valid string */
   if (ret) {
      const char *name = lGetString(job, JB_session);
      if (name != nullptr) {
         if (verify_str_key(
               answer_list, name, MAX_VERIFY_STRING,
               lNm2Str(JB_session), KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      } 
   }

   /* JB_project must be a valid string */
   if (ret) {
      const char *name = lGetString(job, JB_project);
      if (name != nullptr) {
         if (verify_str_key(
            answer_list, name, MAX_VERIFY_STRING,
            lNm2Str(JB_project), KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      } 
   }

   /* JB_department must be a valid string */
   if (ret) {
      const char *name = lGetString(job, JB_department);
      if (name != nullptr) {
         if (verify_str_key(
            answer_list, name, MAX_VERIFY_STRING,
            lNm2Str(JB_department), KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      } 
   }

   /* TODO: JB_directive_prefix can be any string, verify_str_key is too restrictive */

   /* JB_exec_file must be a valid directory string */
   if (ret) {
      const char *name = lGetString(job, JB_exec_file);
      if (name != nullptr) {
         ret = path_verify(name, answer_list, "exec_file", false);
      } 
   }

   /* JB_script_file must be a string and a valid directory string */
   if (ret) {
      const char *name = lGetString(job, JB_script_file);
      if (name != nullptr) {
         ret = path_verify(name, answer_list, "script_file", false);
      } 
   }
   
   /* JB_script_ptr must be any string */
   if (ret) {
      const char *name = lGetString(job, JB_script_ptr);
      if (name == nullptr) {
         /* JB_script_size must not 0 */
         ret = object_verify_ulong_null(job, answer_list, JB_script_size);
      } else {
         /* JB_script_size must be size of JB_script_ptr */
         /* TODO: define a max script size */
         if (strlen(name) != lGetUlong(job, JB_script_size)) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                    MSG_JOB_SCRIPTLENGTHDOESNOTMATCH);
            ret = false;            
         }
      }
   }

   /* JB_submission_time is overwritten by qmaster */

   /* JB_execution_time can be any value */

   /* JB_deadline can be any value */

   /* JB_owner is overwritten by qmaster */

   /* JB_uid is overwritten by qmaster */

   /* JB_group is overwritten by qmaster */

   /* JB_gid is overwritten by qmaster */

   /* JB_account must be a valid string */
   if (ret) {
      const char *name = lGetString(job, JB_account);
      if (name != nullptr) {
         if(verify_str_key(
               answer_list, name, MAX_VERIFY_STRING,
               lNm2Str(JB_account), QSUB_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* JB_notify boolean value */
   /* JB_type any ulong value */
   /* JB_reserve boolean value */

   /* 
    * Job priority has a valid range from -1023 to 1024.
    * As JB_priority is an unsigned long, it is raised by BASE_PRIORITY at
    * job submission time.
    * Therefore a valid range is from 1 to 2 * BASE_PRIORITY.
    */
   if (ret) {
      u_long32 priority = lGetUlong(job, JB_priority);
      if (priority < 1 || priority > 2 * BASE_PRIORITY) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_PARSE_INVALIDPRIORITYMUSTBEINNEG1023TO1024);
         ret = false;
      }
      job_normalize_priority(job, priority);
   }

   /* JB_jobshare any ulong value */

   /* TODO JB_shell_list */
   /* JB_verify any ulong value */
   /* TODO JB_job_args */
   /* JB_checkpoint_attr any ulong */

   /* JB_checkpoint_name */
   if (ret) {
      const char *name = lGetString(job, JB_checkpoint_name);
      if (name != nullptr) {
         if (verify_str_key(
               answer_list, name, MAX_VERIFY_STRING,
               lNm2Str(JB_checkpoint_name), KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* JB_checkpoint_object */
   if (ret) {
      if (lGetObject(job, JB_checkpoint_object) != nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                              MSG_INVALIDJOB_REQUEST_S, "checkpoint object");
            ret = false;
      }
   }

   /* JB_checkpoint_interval any ulong */

   /* JB_restart can be 0, 1 or 2 */
   if (ret) {
      u_long32 value = lGetUlong(job, JB_restart);
      if (value != 0 && value != 1 && value != 2) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                              MSG_INVALIDJOB_REQUEST_S, "restart");
            ret = false; 
      }
   }

   /* TODO: JB_stdout_path_list */
   /* TODO: JB_stderr_path_list */
   /* TODO: JB_stdin_path_list */
   /* JB_merge_stderr boolean value */
   /* TODO: JB_hard_resource_list */
   /* TODO: JB_soft_resource_list */
   /* TODO: JB_hard_queue_list */
   /* TODO: JB_soft_queue_list */
   /* TODO: JB_mail_options */
   /* JB_mail_list any ulong */

   /* JB_pe must be a valid string */
   if (ret) {
      const char *name = lGetString(job,JB_pe );
      if (name != nullptr) {
         if (verify_str_key(
               answer_list, name, MAX_VERIFY_STRING,
               lNm2Str(JB_pe), KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      }
   }

   /* TODO: JB_pe_range */
   /* TODO: JB_master_hard_queue */

   /* JB_tgt can be any string value */
   /* JB_cred can be any string value */
  
   /* TODO: JB_ja_structure */
   /* TODO: JB_ja_n_h_ids */
   /* TODO: JB_ja_u_h_ids */
   /* TODO: JB_ja_s_h_ids */
   /* TODO: JB_ja_o_h_ids */
   /* TODO: JB_ja_a_h_ids */
   /* TODO: JB_ja_z_ids */
   /* TODO: JB_ja_template */
   /* TODO: JB_ja_tasks */

   /* JB_host must be nullptr */
   if (ret) {
      if (lGetHost(job, JB_host) != nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                              MSG_INVALIDJOB_REQUEST_S, "host");
            ret = false;
      }
   }

   /* TODO: JB_category */
   /* TODO: JB_user_list */
   /* TODO: JB_job_identifier_list */

   /* JB_verify_suitable_queues must be in range of OPTION_VERIFY_STR */
   if (ret) {
      if (lGetUlong(job, JB_verify_suitable_queues) >= (sizeof(OPTION_VERIFY_STR)/sizeof(char)-1)) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                              MSG_INVALIDJOB_REQUEST_S, "verify");
            ret = false;
      }
   }

   /* JB_soft_wallclock_gmt must be 0 */
   if (ret) {
      ret = object_verify_ulong64_null(job, answer_list, JB_soft_wallclock_gmt);
    }

   /* JB_hard_wallclock_gmt must be 0 */
   if (ret) {
      ret = object_verify_ulong64_null(job, answer_list, JB_hard_wallclock_gmt);
    }

   /* JB_override_tickets must be 0 */
   if (ret) {
      ret = object_verify_ulong_null(job, answer_list, JB_override_tickets);
    }

   /* TODO: JB_qs_args */
   /* JB_urg must be 0 */
   if (ret) {
      ret = object_verify_double_null(job, answer_list, JB_urg);
    }
   /* JB_nurg must be 0 */
   if (ret) {
      ret = object_verify_double_null(job, answer_list, JB_nurg);
   }
// will be overwritten anyways
#if 0
   /* JB_nppri must be 0 */
   if (ret) {
      ret = object_verify_double_null(job, answer_list, JB_nppri);
    }
#endif
   /* JB_rrcontr must be 0 */
   if (ret) {
      ret = object_verify_double_null(job, answer_list, JB_rrcontr);
    }
   /* JB_dlcontr must be 0 */
   if (ret) {
      ret = object_verify_double_null(job, answer_list, JB_dlcontr);
    }
   /* JB_wtcontr must be 0 */
   if (ret) {
      ret = object_verify_double_null(job, answer_list, JB_wtcontr);
    }
   /* JB_ja_task_concurrency must be nullptr */
   if (ret) {
      u_long32 task_concurrency = lGetUlong(job, JB_ja_task_concurrency);
      if (task_concurrency > 0 && !job_is_array(job)) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALIDJOB_REQUEST_S, "task concurrency");
         ret = false;
      }
   }

   /* JB_env_list
    * Filter potentially dangerous environment variables, see also Issue GE-3761.
    */
   if (ret) {
      lList *env_list = lGetListRW(job, JB_env_list);
      if (env_list != nullptr) {
         var_list_filter_env_list(env_list, answer_list);
      }
   }

   DRETURN(ret);
}

/****** sge_job/job_get_wallclock_limit() **************************************
*  NAME
*     job_get_wallclock_limit() -- Computes jobs wallclock limit
*
*  SYNOPSIS
*     bool job_get_wallclock_limit(u_long32 *limit, const lListElem *jep) 
*
*  FUNCTION
*     Compute the jobs wallclock limit depending on requested h_rt, s_rt.
*     If no limit was requested the maximal ulong32 value is returned
*
*  INPUTS
*     u_long64 *limit      - store for the value
*     const lListElem *jep - jobs ep
*
*  RESULT
*     bool - true on success
*            false if no value was requested
*
*  NOTES
*     MT-NOTE: job_get_wallclock_limit() is not MT safe 
*
*******************************************************************************/
bool job_get_wallclock_limit(u_long64 *limit, const lListElem *jep) {
   const lListElem *ep;
   double d_ret = 0, d_tmp;
   const char *s;
   bool got_duration = false;
   char error_str[1024];

   DENTER(TOP_LAYER);

   const lList *hard_resource_list = job_get_hard_resource_list(jep);
   if ((ep=lGetElemStr(hard_resource_list, CE_name, SGE_ATTR_H_RT))) {
      if (parse_ulong_val(&d_tmp, nullptr, TYPE_TIM, (s=lGetString(ep, CE_stringval)), error_str, sizeof(error_str)-1)==0) {
         ERROR(MSG_CPLX_WRONGTYPE_SSS, SGE_ATTR_H_RT, s, error_str);
         DRETURN(false);
      }
      d_ret = d_tmp;
      got_duration = true;
   }
   
   if ((ep=lGetElemStr(hard_resource_list, CE_name, SGE_ATTR_S_RT))) {
      if (parse_ulong_val(&d_tmp, nullptr, TYPE_TIM, (s=lGetString(ep, CE_stringval)), error_str, sizeof(error_str)-1)==0) {
         ERROR(MSG_CPLX_WRONGTYPE_SSS, SGE_ATTR_H_RT, s, error_str);
         DRETURN(false);
      }

      if (got_duration) {
         d_ret = MIN(d_ret, d_tmp);
      } else {
         d_ret = d_tmp;
         got_duration = true;
      }
   }

   if (got_duration) {
      if (d_ret > (double)U_LONG32_MAX) {
         *limit = U_LONG64_MAX;
      } else {
         *limit = sge_gmt32_to_gmt64(d_ret);
      }
   } else {
      *limit = U_LONG64_MAX;
   }

   DRETURN(got_duration);
}

/****** sgeobj/job_is_binary() ******************************************
*  NAME
*     job_is_binary() -- Was "job" job submitted with -b y? 
*
*  SYNOPSIS
*     bool job_is_binary(const lListElem *job) 
*
*  FUNCTION
*     This function returns true if "job" is a "binary" job
*     which was e.g. submitted with qsub -b y 
*
*  INPUTS
*     const lListELem *job - JB_Type element 
*
*  RESULT
*     bool - true or false
*
*  SEE ALSO
*     sgeobj/job/job_is_array() 
*     sgeobj/job/job_is_binary() 
*     sgeobj/job/job_is_tight_parallel()
******************************************************************************/
bool 
job_is_binary(const lListElem *job) {
   return (JOB_TYPE_IS_BINARY(lGetUlong(job, JB_type)) ? true : false);
}

/* TODO: EB: JSV: add doc */
bool
job_set_binary(lListElem *job, bool is_binary) {
   bool ret = true;
   u_long32 type = lGetUlong(job, JB_type);
  
   if (is_binary) { 
      JOB_TYPE_SET_BINARY(type);
   } else {
      JOB_TYPE_CLEAR_BINARY(type);
   }
   lSetUlong(job, JB_type, type);
   return ret;
}

/* TODO: EB: JSV: add doc */
bool 
job_is_no_shell(const lListElem *job) {
   return (JOB_TYPE_IS_NO_SHELL(lGetUlong(job, JB_type)) ? true : false);
}

/* TODO: EB: JSV: add doc */
bool
job_set_no_shell(lListElem *job, bool is_binary) {
   bool ret = true;
   u_long32 type = lGetUlong(job, JB_type);
  
   if (is_binary) { 
      JOB_TYPE_SET_NO_SHELL(type);
   } else {
      JOB_TYPE_CLEAR_NO_SHELL(type);
   }
   lSetUlong(job, JB_type, type);
   return ret;
}


/* TODO: EB: JSV: add doc */
bool
job_set_owner_and_group(lListElem *job, u_long32 uid, u_long32 gid,
                        const char *user, const char *group, int amount, ocs_grp_elem_t *grp_array) {
   bool ret = true;

   DENTER(TOP_LAYER);
   lSetString(job, JB_owner, user);
   lSetUlong(job, JB_uid, uid);
   lSetString(job, JB_group, group);
   lSetUlong(job, JB_gid, gid);
   lSetList(job, JB_grp_list, grp_list_array2list(amount, grp_array));

   DRETURN(ret);
}

/* The context comes as a VA_Type list with certain groups of
** elements: A group starts with either:
** (+, ): All following elements are appended to the job's
**        current context values, or replaces the current value
** (-, ): The following context values are removed from the
**        job's current list of values
** (=, ): The following elements replace the job's current
**        context values.
** Any combination of groups is possible.
** To ensure portablity with common ocs::gdi::Client::sge_gdi, (=, ) is the default
** when no group tag is given at the beginning of the incoming list
*/
/* jbctx - VA_Type; job - JB_Type */
void 
set_context(lList *jbctx, lListElem *job) 
{
   lList* newjbctx = nullptr;
   const lListElem* jbctxep;
   lListElem* temp;
   char   mode = '+';
   
   newjbctx = lGetListRW(job, JB_context);

   /* if the incoming list is empty, then simply clear the context */
   if (!jbctx || !lGetNumberOfElem(jbctx)) {
      lSetList(job, JB_context, nullptr);
      newjbctx = nullptr;
   }
   else {
      /* if first element contains no tag => assume (=, ) */
      switch(*lGetString(lFirst(jbctx), VA_variable)) {
         case '+':
         case '-':
         case '=':
            break;
         default:
            lSetList(job, JB_context, nullptr);
            newjbctx = nullptr;
            break;
      }
   }

   for_each_ep(jbctxep, jbctx) {
      switch(*(lGetString(jbctxep, VA_variable))) {
         case '+':
            mode = '+';
            break;
         case '-':
            mode = '-';
            break;
         case '=':
            lSetList(job, JB_context, nullptr);
            newjbctx = nullptr;
            mode = '+';
            break;
         default:
            switch(mode) {
               case '+':
                  if (!newjbctx)
                     lSetList(job, JB_context, newjbctx = lCreateList("context_list", VA_Type));
                  if ((temp = lGetElemStrRW(newjbctx, VA_variable, lGetString(jbctxep, VA_variable))))
                     lSetString(temp, VA_value, lGetString(jbctxep, VA_value));
                  else
                     lAppendElem(newjbctx, lCopyElem(jbctxep));
                  break;
               case '-':

                  lDelSubStr(job, VA_variable, lGetString(jbctxep, VA_variable), JB_context); 
                  /* WARNING: newjbctx is not valid when complete list was deleted */
                  break;
            }
            break;
      }
   }
}

bool
job_get_ckpt_attr(int op, dstring *string)
{
   bool success = true;

   DENTER(TOP_LAYER);
   if (VALID(CHECKPOINT_AT_MINIMUM_INTERVAL, op)) {
      sge_dstring_append_char(string, CHECKPOINT_AT_MINIMUM_INTERVAL_SYM);
   }
   if (VALID(CHECKPOINT_AT_SHUTDOWN, op)) {
      sge_dstring_append_char(string, CHECKPOINT_AT_SHUTDOWN_SYM);
   }
   if (VALID(CHECKPOINT_SUSPEND, op)) {
      sge_dstring_append_char(string, CHECKPOINT_SUSPEND_SYM);
   }
   if (VALID(NO_CHECKPOINT, op)) {
      sge_dstring_append_char(string, NO_CHECKPOINT_SYM);
   }
   DRETURN(success);
}

bool
job_get_verify_attr(u_long32 op, dstring *string)
{
   bool success = true;

   DENTER(TOP_LAYER);
   if (ERROR_VERIFY == op) {
      sge_dstring_append_char(string, 'e');
   } else if (WARNING_VERIFY == op) {
      sge_dstring_append_char(string, 'w');
   } else if (JUST_VERIFY == op) {
      sge_dstring_append_char(string, 'v');
   } else if (POKE_VERIFY == op) {
      sge_dstring_append_char(string, 'p');
   } else {
      sge_dstring_append_char(string, 'n');
   }
   DRETURN(success);
}

bool 
job_parse_validation_level(int *level, const char *input, int prog_number, lList **answer_list) 
{
   bool ret = true;

   DENTER(TOP_LAYER);
   if (strcmp("e", input) == 0) {
      if (prog_number == QRSUB) {
         *level = AR_ERROR_VERIFY;
      } else {
         *level = ERROR_VERIFY;
      }
   } else if (strcmp("w", input) == 0) {
      if (prog_number == QRSUB) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, input);
         ret = false;
      } else {
         *level = WARNING_VERIFY; 
      }
   } else if (strcmp("n", input) == 0) {
      if (prog_number == QRSUB) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, input);
         ret = false;
      } else {
         *level = SKIP_VERIFY;
      }
   } else if (strcmp("v", input) == 0) {
      if (prog_number == QRSUB) {
         *level = AR_JUST_VERIFY;
      } else {
         *level = JUST_VERIFY;
      }
   } else if (strcmp("p", input) == 0) {
      if (prog_number == QRSUB) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, input); 
         ret = false;
      } else {
         *level = POKE_VERIFY;
      }
   } else {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, input);
      ret = false;
   }
   DRETURN(ret);
}

/****** sgeobj/job_is_requesting_consumable() ******************************************
*  NAME
*     job_is_requesting_consumable() -- Is job requesting resources of type 
*                                       CONSUMABLE_JOB?
*
*  SYNOPSIS
*     bool job_is_requesting_consumable(lListElem *jep, const char *resoure_name) 
*
*  FUNCTION
*     This function returns true if "job" is requesting a resource with type
*     CONSUMABLE_JOB.
*
*  INPUTS
*     lListELem *jep - JB_Type element 
*     const char *resource_name - Name of resource
*
*  RESULT
*     bool - true or false
*
*  SEE ALSO
******************************************************************************/
bool
job_is_requesting_consumable(lListElem *jep, const char *resource_name)
{
   lListElem *cep = nullptr;
   u_long32 consumable;
   const lList *request_list = job_get_hard_resource_list(jep);

   if (request_list != nullptr) {
      cep = centry_list_locate(request_list, resource_name);
      if (cep != nullptr) {
         consumable = lGetUlong(cep, CE_consumable);
         if (consumable == CONSUMABLE_JOB) {
            return true;
         }
      }
   }
   return false;
}

bool
job_init_binding_elem(lListElem *jep) 
{
   bool ret = true;
   lList *binding_list = lCreateList("", BN_Type);
   lListElem *binding_elem = lCreateElem(BN_Type); 

   if (binding_elem != nullptr && binding_list != nullptr) {
      lAppendElem(binding_list, binding_elem);
      lSetList(jep, JB_binding, binding_list);

      lSetString(binding_elem, BN_strategy, "no_job_binding");
      lSetUlong(binding_elem, BN_type, BINDING_TYPE_NONE);
      lSetUlong(binding_elem, BN_parameter_n, 0);
      lSetUlong(binding_elem, BN_parameter_socket_offset, 0);
      lSetUlong(binding_elem, BN_parameter_core_offset, 0);
      lSetUlong(binding_elem, BN_parameter_striding_step_size, 0);
      lSetString(binding_elem, BN_parameter_explicit, "no_explicit_binding");
   } else {
      ret = false;
   }
   return ret;
}

bool job_parse_scope_string(const char *scope, char &scope_id) {
   bool ret = true;

   if (sge_strnullcasecmp(scope, "global") == 0) {
      scope_id = JRS_SCOPE_GLOBAL;
   } else if (sge_strnullcasecmp(scope, "master") == 0) {
      scope_id = JRS_SCOPE_MASTER;
   } else if (sge_strnullcasecmp(scope, "slave") == 0) {
      scope_id = JRS_SCOPE_SLAVE;
   } else {
      scope_id = JRS_SCOPE_GLOBAL;
      ret = false;
   }

   return ret;
}

const lListElem *job_get_request_set(const lListElem *job, u_long32 scope) {
   // job might be reduced
   if (const int pos = lGetPosViaElem(job, JB_request_set_list, false); pos == NoName) {
      return nullptr;
   } else {
      return lGetSubUlong(job, JRS_scope, scope, JB_request_set_list);
   }
}

lListElem *job_get_request_setRW(lListElem *job, u_long32 scope) {
   // job might be reduced
   if (const int pos = lGetPosViaElem(job, JB_request_set_list, false); pos == NoName) {
      return nullptr;
   } else {
      return lGetSubUlongRW(job, JRS_scope, scope, JB_request_set_list);
   }
}

lListElem *job_get_or_create_request_setRW(lListElem *job, u_long32 scope) {
   lListElem *jrs = lGetSubUlongRW(job, JRS_scope, scope, JB_request_set_list);
   if (jrs == nullptr) {
      jrs = lAddSubUlong(job, JRS_scope, scope, JB_request_set_list, JRS_Type);
   }

   return jrs;
}

/**
 * Remove duplicate resource requests from the job request set list.
 *
 * @param job - the job to work on
 * @return true if the job has any resource requests, else false
 */
bool job_request_set_remove_duplicates(lListElem *job) {
   bool requests_found = false;

   lListElem *jrs;
   for_each_rw (jrs, lGetListRW(job, JB_request_set_list)) {
      lList *lp = lGetListRW(jrs, JRS_hard_resource_list);
      if (lp != nullptr) {
         requests_found = true;
         centry_list_remove_duplicates(lp);
      }
      lp = lGetListRW(jrs, JRS_soft_resource_list);
      if (lp != nullptr) {
         requests_found = true;
         centry_list_remove_duplicates(lp);
      }
   }

   return requests_found;
}

bool job_request_set_has_queue_requests(const lListElem *job) {
   bool ret = false;
   lListElem *jrs;
   for_each_rw (jrs, lGetListRW(job, JB_request_set_list)) {
      if (lGetList(jrs, JRS_hard_queue_list) != nullptr || lGetList(jrs, JRS_soft_queue_list) != nullptr) {
         ret = true;
         break;
      }
   }

   return ret;
}

const lListElem *job_get_highest_hard_request(const lListElem *job, const char *request_name) {
   const lListElem *ret = nullptr;
   double max_request = 0.0;

   const lListElem *jrs;
   for_each_ep (jrs, lGetList(job, JB_request_set_list)) {
      const lListElem *request = lGetSubStr(jrs, CE_name, request_name, JRS_hard_resource_list);
      if (request != nullptr) {
         double request_value = lGetDouble(request, CE_doubleval);
         if (request_value > max_request) {
            ret = request;
            max_request = request_value;
         }
      }
   }

   return ret;
}

const lList *job_get_resource_list(const lListElem *job, u_long32 scope, bool hard) {
   const lList *ret = nullptr;
   const lListElem *jrs = job_get_request_set(job, scope);
   const int nm = hard ? JRS_hard_resource_list : JRS_soft_resource_list;
   if (jrs != nullptr) {
      ret = lGetList(jrs, nm);
   }
   return ret;
}

const lList *job_get_hard_resource_list(const lListElem *job) {
   return job_get_resource_list(job, JRS_SCOPE_GLOBAL, true);
}

const lList *job_get_hard_resource_list(const lListElem *job, u_long32 scope) {
   return job_get_resource_list(job, scope, true);
}

const lList *job_get_soft_resource_list(const lListElem *job) {
   return job_get_resource_list(job, JRS_SCOPE_GLOBAL, false);
}

const lList *job_get_soft_resource_list(const lListElem *job, u_long32 scope) {
   return job_get_resource_list(job, scope, false);
}

const lList *job_get_hard_queue_list(const lListElem *job) {
   return job_get_hard_queue_list(job, JRS_SCOPE_GLOBAL);
}

const lList *job_get_queue_list(const lListElem *job, u_long32 scope, bool hard) {
   const lList *ret = nullptr;
   const lListElem *jrs = job_get_request_set(job, scope);
   const int nm = hard ? JRS_hard_queue_list : JRS_soft_queue_list;

   if (jrs != nullptr) {
      ret = lGetList(jrs, nm);
   }
   return ret;
}

const lList *job_get_hard_queue_list(const lListElem *job, u_long32 scope) {
   return job_get_queue_list(job, scope, true);
}

const lList *job_get_soft_queue_list(const lListElem *job) {
   return job_get_queue_list(job, JRS_SCOPE_GLOBAL, false);
}
const lList *job_get_soft_queue_list(const lListElem *job, u_long32 scope) {
   return job_get_queue_list(job, scope, false);
}

const lList *job_get_master_hard_queue_list(const lListElem *job) {
   return job_get_queue_list(job, JRS_SCOPE_MASTER, true);
}

lList *job_get_resource_listRW(lListElem *job, u_long32 scope, bool hard) {
   lList *ret = nullptr;
   lListElem *jrs = job_get_request_setRW(job, scope);
   const int nm = hard ? JRS_hard_resource_list : JRS_soft_resource_list;
   if (jrs != nullptr) {
      ret = lGetListRW(jrs, nm);
   }
   return ret;
}

lList *job_get_hard_resource_listRW(lListElem *job) {
   return job_get_resource_listRW(job, JRS_SCOPE_GLOBAL, true);
}

lList *job_get_hard_resource_listRW(lListElem *job, u_long32 scope) {
   return job_get_resource_listRW(job, scope, true);
}

lList *job_get_soft_resource_listRW(lListElem *job) {
   return job_get_resource_listRW(job, JRS_SCOPE_GLOBAL, false);
}
lList *job_get_soft_resource_listRW(lListElem *job, u_long32 scope) {
   return job_get_resource_listRW(job, scope, false);
}

lList *job_get_queue_listRW(lListElem *job, u_long32 scope, bool hard) {
   lList *ret = nullptr;
   lListElem *jrs = job_get_request_setRW(job, scope);
   const int nm = hard ? JRS_hard_queue_list : JRS_soft_queue_list;
   if (jrs != nullptr) {
      ret = lGetListRW(jrs, nm);
   }
   return ret;
}

lList *job_get_hard_queue_listRW(lListElem *job) {
   return job_get_queue_listRW(job, JRS_SCOPE_GLOBAL, true);
}
lList *job_get_hard_queue_listRW(lListElem *job, u_long32 scope) {
   return job_get_queue_listRW(job, scope, true);
}

lList *job_get_soft_queue_listRW(lListElem *job) {
   return job_get_queue_listRW(job, JRS_SCOPE_GLOBAL, false);
}
lList *job_get_soft_queue_listRW(lListElem *job, u_long32 scope) {
   return job_get_queue_listRW(job, scope, false);
}

lList *job_get_master_hard_queue_listRW(lListElem *job) {
   return job_get_queue_listRW(job, JRS_SCOPE_MASTER, false);
}

void job_set_resource_list(lListElem *job, lList *resource_list, u_long32 scope, bool hard) {
   lListElem *jrs = job_get_or_create_request_setRW(job, scope);
   if (jrs != nullptr) {
      int nm = hard ? JRS_hard_resource_list : JRS_soft_resource_list;
      lSetList(jrs, nm, resource_list);
   }
}

void job_set_hard_resource_list(lListElem *job, lList *resource_list) {
   job_set_resource_list(job, resource_list, JRS_SCOPE_GLOBAL, true);
}

void job_set_hard_resource_list(lListElem *job, lList *resource_list, u_long32 scope) {
   job_set_resource_list(job, resource_list, scope, true);
}

void job_set_soft_resource_list(lListElem *job, lList *resource_list) {
   job_set_resource_list(job, resource_list, JRS_SCOPE_GLOBAL, false);
}

void job_set_soft_resource_list(lListElem *job, lList *resource_list, u_long32 scope) {
   job_set_resource_list(job, resource_list, scope, false);
}

void job_set_queue_list(lListElem *job, lList *queue_list, u_long32 scope, bool hard) {
   lListElem *jrs = job_get_or_create_request_setRW(job, scope);
   if (jrs != nullptr) {
      int nm = hard ? JRS_hard_queue_list : JRS_soft_queue_list;
      lSetList(jrs, nm, queue_list);
   }
}
void job_set_hard_queue_list(lListElem *job, lList *queue_list) {
   job_set_queue_list(job, queue_list, JRS_SCOPE_GLOBAL, true);
}

void job_set_hard_queue_list(lListElem *job, lList *queue_list, u_long32 scope) {
   job_set_queue_list(job, queue_list, scope, true);
}

void job_set_soft_queue_list(lListElem *job, lList *queue_list) {
   job_set_queue_list(job, queue_list, JRS_SCOPE_GLOBAL, false);
}

void job_set_soft_queue_list(lListElem *job, lList *queue_list, u_long32 scope) {
   job_set_queue_list(job, queue_list, scope, false);
}

void job_set_master_hard_queue_list(lListElem *job, lList *queue_list) {
   job_set_queue_list(job, queue_list, JRS_SCOPE_MASTER, true);
}

static void
job_add_str_to_command_line(dstring *dstr, const char *str) {
   if (str != nullptr) {
      bool do_quote = sge_has_whitespace(str) || sge_is_pattern(str);
      if (do_quote) {
         sge_dstring_append_char(dstr, '\'');
      }
      sge_dstring_append(dstr, str);
      if (do_quote) {
         sge_dstring_append_char(dstr, '\'');
      }
   }
}

static void
job_add_opt_to_comand_line(dstring *dstr, const char *opt, const char *value) {
   if (opt != nullptr) {
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
   }
   if (value != nullptr) {
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, value);
   }
}

static bool
job_add_bool_opt_to_command_line(const lListElem *job, dstring *dstr, const char *opt, int nm, bool print_value) {
   bool ret = false;
   bool b = lGetBool(job, nm);
   if (b == print_value) {
      ret = true;
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, b ? "yes" : "no");
   }

   return ret;
}

static bool
job_add_ulong_opt_to_command_line(const lListElem *job, dstring *dstr, const char *opt, int nm, long offset) {
   bool ret = false;
   long l = lGetUlong(job, nm) + offset;
   if (l != 0) {
      ret = true;
      if (opt != nullptr) {
         sge_dstring_append_char(dstr, ' ');
         sge_dstring_append(dstr, opt);
      }
      sge_dstring_sprintf_append(dstr, " %ld", l);
   }

   return ret;
}

static bool
job_add_time_opt_to_command_line(const lListElem *job, dstring *dstr, const char *opt, int nm) {
   bool ret = true;
   u_long64 time64 = lGetUlong64(job, nm);
   if (time64 != 0) {
      ret = true;
      if (opt != nullptr) {
         sge_dstring_append_char(dstr, ' ');
         sge_dstring_append(dstr, opt);
      }
      sge_dstring_append_char(dstr, ' ');
      DSTRING_STATIC(dstr_time, 64);
      sge_dstring_append(dstr, sge_ctime64_date_time(time64, &dstr_time));
   }

   return ret;
}

static bool
job_add_str_opt_to_command_line(const lListElem *job, dstring *dstr, const char *opt, int nm) {
   bool ret = false;
   const char *str = lGetString(job, nm);
   if (str != nullptr) {
      ret = true;
      if (opt != nullptr) {
         sge_dstring_append_char(dstr, ' ');
         sge_dstring_append(dstr, opt);
      }
      sge_dstring_append_char(dstr, ' ');
      job_add_str_to_command_line(dstr, str);
   }

   return ret;
}

static bool
job_add_ce_list_opt_to_command_line(const lListElem *job, dstring *dstr, const char *opt, int nm) {
   bool ret = false;
   const lList *ce_list = lGetList(job, nm);
   if (ce_list != nullptr) {
      ret = true;
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
      sge_dstring_append_char(dstr, ' ');
      dstring local_dstr = DSTRING_INIT;
      job_add_str_to_command_line(dstr, centry_list_append_to_dstring(ce_list, &local_dstr));
      sge_dstring_free(&local_dstr);
   }

   return ret;
}

static bool
job_add_list_opt_to_command_line(const lListElem *job, dstring *dstr, const char *opt, int nm, int lnm) {
   bool ret = false;
   const lList *lp = lGetList(job, nm);
   if (lp != nullptr) {
      ret = true;
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
      sge_dstring_append_char(dstr, ' ');
      const lListElem *ep;
      bool first = true;
      for_each_ep(ep, lp) {
         if (first) {
            first = false;
         } else {
            sge_dstring_append_char(dstr, ',');
         }
         sge_dstring_append(dstr, lGetString(ep, lnm));
      }
   }

   return ret;
}

static bool
job_add_host_str_list_opt_to_command_line(const lListElem *job, dstring *dstr, const char *opt, int nm, int hnm,
                                          int snm) {
   bool ret = false;
   const lList *lp = lGetList(job, nm);
   if (lp != nullptr) {
      ret = true;
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, opt);
      sge_dstring_append_char(dstr, ' ');
      const lListElem *ep;
      bool first = true;
      for_each_ep(ep, lp) {
         if (first) {
            first = false;
         } else {
            sge_dstring_append_char(dstr, ',');
         }
         const char *host = lGetHost(ep, hnm);
         const char *str = lGetString(ep, snm);
         if (str != nullptr) {
            if (host != nullptr) {
               sge_dstring_append(dstr, host);
               sge_dstring_append_char(dstr, ':');
            }
            sge_dstring_append(dstr, str);
         }
      }
   }

   return ret;
}

static bool
job_add_name_value_list_opt_to_command_line(const lListElem *job, dstring *dstr, const char *opt, int nm, int nnm,
                                            int vnm, const char *filter = nullptr) {
   bool ret = false;
   const lList *lp = lGetList(job, nm);
   if (lp != nullptr) {
      const lListElem *ep;
      bool first = true;
      for_each_ep(ep, lp) {
         const char *name = lGetString(ep, nnm);
         // apply filter to name, if name starts with filter, then skip this entry
         if (filter != nullptr && strstr(name, filter) == name) {
            continue;
         }
         if (first) {
            first = false;
            // if we get here for the first time then print the option string
            sge_dstring_append_char(dstr, ' ');
            sge_dstring_append(dstr, opt);
            sge_dstring_append_char(dstr, ' ');
            ret = true;
         } else {
            // additional variable, print the separator
            sge_dstring_append_char(dstr, ',');
         }
         if (name != nullptr) {
            sge_dstring_append(dstr, name);
            sge_dstring_append_char(dstr, '=');

            const char *value = lGetString(ep, vnm);
            if (value != nullptr) {
               sge_dstring_append(dstr, value);
            }
         }
      }
   }

   return ret;
}

static void
job_add_resource_set_list_scope_to_command_line(const lListElem *scope_ep, dstring *dstr) {
   job_add_ce_list_opt_to_command_line(scope_ep, dstr, "-hard -l", JRS_hard_resource_list);
   job_add_ce_list_opt_to_command_line(scope_ep, dstr, "-soft -l", JRS_soft_resource_list);
   job_add_list_opt_to_command_line(scope_ep, dstr, "-hard -q", JRS_hard_queue_list, QR_name);
   job_add_list_opt_to_command_line(scope_ep, dstr, "-soft -q", JRS_soft_queue_list, QR_name);
}

const char *job_scope_name(u_long32 scope_id) {
   const char *ret = "unknown";

   switch (scope_id) {
      case JRS_SCOPE_GLOBAL:
         ret = "global";
         break;
      case JRS_SCOPE_MASTER:
         ret = "master";
         break;
      case JRS_SCOPE_SLAVE:
         ret = "slave";
         break;
   }

   return ret;
}

const char *
job_scope_name(const lListElem *scope_ep) {
   const char *ret = "unknown";

   if (scope_ep != nullptr) {
      ret = job_scope_name(lGetUlong(scope_ep, JRS_scope));
   }

   return ret;
}

static void
job_add_resource_set_list_to_command_line(const lListElem *job, dstring *dstr) {
   const lList *request_set_list = lGetList(job, JB_request_set_list);
   if (request_set_list != nullptr) {
      const lListElem *scope_ep;
      for_each_ep (scope_ep, request_set_list) {
         job_add_opt_to_comand_line(dstr, "-scope", job_scope_name(scope_ep));
         job_add_resource_set_list_scope_to_command_line(scope_ep, dstr);
      }
   }
}

/**
 * Based on the attributes of the job object the function generates a command line
 * which can be used to submit an identical job (if no sge_request files and/or JSV are active).
 * The command line is stored in the given dstring.
 *
 * @param[in] job      job used as template for the command line
 * @param[in] dstr     dstring which will contain the command line
 * @param[in] client   client name, this will become the first word in the command line
 * @return the command line as string
 */
const char *
job_get_effective_command_line(const lListElem *job, dstring *dstr, const char *client) {
   // the submit client name
   sge_dstring_copy_string(dstr, client);

   // the submit options
   job_add_str_opt_to_command_line(job, dstr, "-A", JB_account);
   job_add_time_opt_to_command_line(job, dstr, "-a", JB_execution_time);
   // -ac is covered by -sc
   job_add_ulong_opt_to_command_line(job, dstr, "-ar", JB_ar, 0);
   if (JOB_TYPE_IS_BINARY(lGetUlong(job, JB_type))) {
      job_add_opt_to_comand_line(dstr, "-b", "yes");
   }
   // -binding
   const lListElem *binding;
   for_each_ep(binding, lGetList(job, JB_binding)) {
      sge_dstring_append(dstr, " -binding ");
      ocs::BindingIo::binding_print_to_string(binding, dstr);
   }

   job_add_str_opt_to_command_line(job, dstr, "-C", JB_directive_prefix);

   // checkpointing
   if (job_add_str_opt_to_command_line(job, dstr, "-ckpt", JB_checkpoint_name)) {
      u_long ckpt_attr = lGetUlong(job, JB_checkpoint_attr);
      u_long ckpt_interval = lGetUlong(job, JB_checkpoint_interval);
      if (ckpt_attr != 0 || ckpt_interval != 0) {
         sge_dstring_append(dstr, " -c ");
         if (ckpt_attr != 0) {
            job_get_ckpt_attr(ckpt_attr, dstr);
         }
         if (ckpt_interval != 0) {
            sge_dstring_sprintf_append(dstr, sge_u32, ckpt_interval);
         }
      }
   }

   // -clear is not reflected in the job object
   // -cwd is covered by -wd
   // -dc is not reflected in the job object
   job_add_time_opt_to_command_line(job, dstr, "-dl", JB_deadline);
   job_add_host_str_list_opt_to_command_line(job, dstr, "-e", JB_stderr_path_list, PN_host, PN_path);
   job_add_host_str_list_opt_to_command_line(job, dstr, "-o", JB_stdout_path_list, PN_host, PN_path);
   job_add_host_str_list_opt_to_command_line(job, dstr, "-i", JB_stdin_path_list, PN_host, PN_path);
   job_add_list_opt_to_command_line(job, dstr, "-hold_jid", JB_jid_request_list, JRE_job_name);
   job_add_list_opt_to_command_line(job, dstr, "-hold_jid_ad", JB_ja_ad_request_list, JRE_job_name);
   job_add_bool_opt_to_command_line(job, dstr, "-j", JB_merge_stderr, true);
   job_add_ulong_opt_to_command_line(job, dstr, "-js", JB_jobshare, 0);
   // -jsv is not reflected in the job object

   u_long32 mailopt = lGetUlong(job, JB_mail_options);
   if (mailopt > 0) {
      sge_dstring_sprintf_append(dstr, " -m ");
      sge_dstring_append_mailopt(dstr, mailopt);
   }
   const lList *mail_list = lGetList(job, JB_mail_list);
   if (mail_list != nullptr) {
      sge_dstring_append_char(dstr, ' ');
      sge_dstring_append(dstr, "-M");
      sge_dstring_append_char(dstr, ' ');
      const lListElem *ep;
      bool first = true;
      for_each_ep(ep, mail_list) {
         if (first) {
            first = false;
         } else {
            sge_dstring_append_char(dstr, ',');
         }
         sge_dstring_sprintf_append(dstr, "%s@%s", lGetString(ep, MR_user), lGetHost(ep, MR_host));
      }
   }

   if (lGetBool(job, JB_notify)) {
      job_add_opt_to_comand_line(dstr, "-notify", nullptr);
   }
   if (JOB_TYPE_IS_IMMEDIATE(lGetUlong(job, JB_type))) {
      job_add_opt_to_comand_line(dstr, "-now", "yes");
   }
   job_add_str_opt_to_command_line(job, dstr, "-N", JB_job_name);
   job_add_str_opt_to_command_line(job, dstr, "-P", JB_project);
   job_add_ulong_opt_to_command_line(job, dstr, "-p", JB_priority, -1024);
   if (job_add_str_opt_to_command_line(job, dstr, "-pe", JB_pe)) {
      sge_dstring_append_char(dstr, ' ');
      DSTRING_STATIC(dstr_pe, 64);
      range_list_print_to_string(lGetList(job, JB_pe_range), &dstr_pe, true, false, false);
      sge_dstring_append_dstring(dstr, &dstr_pe);
   }
   u_long32 pty = lGetUlong(job, JB_pty);
   if (pty < 2) { // 2 means: do not specify it but use the default for the client
      job_add_opt_to_comand_line(dstr, "-pty", pty == 0 ? "no" : "yes");
   }

   // -sync <options_flags>
   // -sync n which is the default will be silently ignored
   u_long32 sync_options = lGetUlong(job, JB_sync_options);
   if (sync_options != SYNC_NO) {
      std::string sync_flags = job_get_sync_options_string(job);
      job_add_opt_to_comand_line(dstr, "-sync", sync_flags.c_str());
   }

   job_add_bool_opt_to_command_line(job, dstr, "-R", JB_reserve, true);
   if (lGetUlong(job, JB_restart) == 2) {
      job_add_opt_to_comand_line(dstr, "-r", "no");
   } else {
      job_add_opt_to_comand_line(dstr, "-r", "yes");
   }
   job_add_name_value_list_opt_to_command_line(job, dstr, "-sc", JB_context, VA_variable, VA_value);
   job_add_host_str_list_opt_to_command_line(job, dstr, "-shell", JB_shell_list, PN_host, PN_path);
   // -sync is not part of the job - it is only client behaviour
   // -t option
   if (job_is_array(job)) {
      u_long32 start, end, step;
      job_get_submit_task_ids(job, &start, &end, &step);
      sge_dstring_sprintf_append(dstr, " -t " sge_u32 "-" sge_u32 ":" sge_u32, start, end, step);
   }
   job_add_ulong_opt_to_command_line(job, dstr, "-tc", JB_ja_task_concurrency, 0);
   job_add_name_value_list_opt_to_command_line(job, dstr, "-v", JB_env_list, VA_variable, VA_value, VAR_PREFIX);
   // -w has no effect on the later job execution
   job_add_str_opt_to_command_line(job, dstr, "-wd", JB_cwd);
   // -@ is not reflected in the job object

   // requests
   job_add_resource_set_list_to_command_line(job, dstr);

   // command line to execute
   job_add_str_opt_to_command_line(job, dstr, nullptr, JB_script_file);
   const lListElem *ep;
   for_each_ep (ep, lGetList(job, JB_job_args)) {
      sge_dstring_append_char(dstr, ' ');
      job_add_str_to_command_line(dstr, lGetString(ep, ST_name));
   }

   return sge_dstring_get_string(dstr);
}

/**
 * Generates the effective command line from the job attributes
 * and stores it in the job attribute JB_submission_command_line.
 *
 * @param[in] job      job used as template for the command line
 * @param[in] client   client name, this will become the first word in the command line
 */
void job_set_command_line(lListElem *job, const char *client) {
   dstring dstr = DSTRING_INIT;
   lSetString(job, JB_submission_command_line, job_get_effective_command_line(job, &dstr, client));
   sge_dstring_free(&dstr);
}

/**
 * Formats the command line given as argument count and argument vector into one string
 * and stores it in the job attribute JB_submission_command_line.
 *
 * @param[in] job    we store the result here
 * @param[in] argc   the argument count
 * @param[in] argv   the argument vector
 */
void
job_set_command_line(lListElem *job, int argc, const char *argv[]) {
   dstring dstr = DSTRING_INIT;
   lSetString(job, JB_submission_command_line, sge_dstring_from_argv(&dstr, argc, argv, true, true));
   sge_dstring_free(&dstr);
}

/** @brief Sets the sync options for a job.
 *
 * @param job
 * @param sync_options
 */
void
job_set_sync_options(lListElem *job, u_long32 sync_options) {
   lSetUlong(job, JB_sync_options, sync_options);
}

/** @brief get the letter combination that represents the sync options
 *
 * @param job
 * @return the letter combination
 */
std::string
job_get_sync_options_string(const lListElem *job) {
   DENTER(TOP_LAYER);

   u_long32 sync_options = lGetUlong(job, JB_sync_options);
   if (sync_options == SYNC_NO) {
      DRETURN("n");
   }

   std::string ret = "";
   if (sync_options == SYNC_JOB_START) {
      ret += "r";
   }
   if (sync_options == SYNC_JOB_END) {
      ret += "x";
   }
   DRETURN(ret);
}

/** @brief Checks if a job should be visible to the current user.
 *
 * @param owner the owner of the job
 * @param is_manager true if the current user is a manager
 * @param show_department_view true if the department view should be shown
 * @param acl_list the ACL list
 * @return true if the job should be visible, false otherwise
 */
bool
job_is_visible(const char *owner, const bool is_manager, const bool show_department_view, const lList *acl_list) {
   DENTER(BASIS_LAYER);

   // manager can see everything as well as all users if -sdv was not specified
   if (is_manager || !show_department_view) {
      DRETURN(true);
   }

   // if the qstat-user is the owner of the job, show the data
   const char *username = component_get_username();
   if (strcmp(username, owner) == 0) {
      DRETURN(true);
   }

   // check all departments
   const lListElem *acl_dep;
   for_each_ep(acl_dep, acl_list) {
      // skip non-departmental acl's
      if (const u_long32 type = lGetUlong(acl_dep, US_type); (type & US_DEPT) == 0) {
         continue;
      }

      // if the qstat-user and owner are in the same department, show the data
      const lList *entries = lGetList(acl_dep, US_entries);
      const lListElem *owner_elem = lGetElemStr(entries, UE_name, owner);
      const lListElem *user_elem = lGetElemStr(entries, UE_name, username);
      if (owner_elem != nullptr && user_elem != nullptr) {
         DRETURN(true);
      }
   }
   DRETURN(false);
}

void job_normalize_priority(lListElem *jep, u_long32 priority)
{
   constexpr double min_priority = 0.0;
   constexpr double max_priority = 2048.0;

   lSetUlong(jep, JB_priority, priority);
   lSetDouble(jep, JB_nppri, sge_normalize_value(priority, min_priority, max_priority));
}

/** @brief Summarizes the slots of the given gdil list.
 *
 * The function will return a new list with one entry for each host.
 * The slots of all entries for the same host will be accumulated.
 *
 * @param gdil_in the input list
 * @return a new list with summarized entries. Caller is responsible for freeing the list.
 */
lList *
gdil_make_host_unique(const lList *gdil_in) {
   DENTER(TOP_LAYER);

   // no input -> nothing to do
   if (gdil_in == nullptr) {
      DRETURN(nullptr);
   }

   // just one entry -> return a copy of the input
   lList *gdil_out = lCopyList("gdil_summarize_hosts", gdil_in);
   if (lGetNumberOfElem(gdil_out) == 1) {
      DRETURN(gdil_out);
   }

   // not required as long as only master host has a separate entry
   // not to sort also ensures that the first entry shows the master host
#if 0
   // make entries for same host to appear in the list as consecutive entries
   lPSortList(gdil_out, "%I+", JG_qhostname);
#endif

   // first gdil that we will use to summarize the slots and the hostname
   lListElem *first_gdil = lFirstRW(gdil_out);
   const char *first_hostname = lGetHost(first_gdil, JG_qhostname);

   // iterate over the rest of the list and summarize the slots for entries with the same hostname
   lListElem *next_gdil = lNextRW(first_gdil);
   lListElem *gdil;
   while ((gdil = next_gdil) != nullptr) {
      next_gdil = lNextRW(gdil);

      // if the next gdil has a different hostname then it is the first gdil for this new hostname
      // otherwise we summarize the slots of the current gdil into the first gdil
      if (const char *next_gdil_hostname = lGetHost(gdil, JG_qhostname); sge_hostcmp(first_hostname, next_gdil_hostname) != 0) {
         first_gdil = gdil;
         first_hostname = next_gdil_hostname;
      } else {
         lAddUlong(first_gdil, JG_slots, lGetUlong(gdil, JG_slots));
         lRemoveElem(gdil_out, &gdil);
      }
   }

   DRETURN(gdil_out);
}

u_long32
jatask_combine_state_and_status_for_output(const lListElem *job, const lListElem *jatep) {
   DENTER(TOP_LAYER);

   u_long32 status = lGetUlong(jatep, JAT_status);
   u_long32 state = lGetUlong(jatep, JAT_state);
   if (status == JRUNNING) {
      state |= JRUNNING;
      state &= ~JTRANSFERING;
   } else if (status == JTRANSFERING) {
      state |= JTRANSFERING;
      state &= ~JRUNNING;
   } else if (status == JFINISHED) {
      state |= JEXITING;
      state &= ~(JRUNNING | JTRANSFERING);
   }

   // correct running state if the job is suspended (remove the 'r')
   if (ISSET(state, JSUSPENDED_ON_SUBORDINATE) ||
       ISSET(state, JSUSPENDED) ||
       ISSET(state, JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
      state &= ~JRUNNING;
   }

   // show hold state if the job is held or has predecessors
   if (lGetList(job, JB_jid_predecessor_list) ||
       lGetUlong(jatep, JAT_hold)) {
      state |= JHELD;
   }

   // show 'R' state if the job is restarted
   if (lGetUlong(jatep, JAT_job_restarted)) {
      state &= ~JWAITING;
      state |= JMIGRATING;
   }
   DRETURN(state);
}
