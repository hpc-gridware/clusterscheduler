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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>
#include <ctime>

#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "cull/cull.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_order.h"

#include "sge_orders.h"
#include "suspend_thresholds.h"


static int select4suspension(lList *job_list, lListElem *queues, lListElem **jepp, lListElem **ja_taskp);
static int select4unsuspension(lList *job_list, lListElem *queues, lListElem **jepp, lListElem **ja_taskp);

/*
   select and suspend jobs in susp_queues 
*/
void 
suspend_job_in_queues( lList *susp_queues, lList *job_list, order_t *orders) 
{
   int i, found;
   lListElem *jep = nullptr, *ja_task = nullptr;
   lListElem *qep;

   DENTER(TOP_LAYER);

   u_long64 now = sge_get_gmt64();
   for_each_rw (qep, susp_queues) {
      u_long32 interval;      

      /* are suspend thresholds enabled? */
      parse_ulong_val(nullptr, &interval, TYPE_TIM, lGetString(qep, QU_suspend_interval), nullptr, 0);

      if (interval == 0
          || !lGetUlong(qep, QU_nsuspend)
          || !lGetList(qep, QU_suspend_thresholds)) {
         continue;
      } 

      /* check time stamp */
      if (lGetUlong64(qep, QU_last_suspend_threshold_ckeck) &&
         (lGetUlong64(qep, QU_last_suspend_threshold_ckeck) + sge_gmt32_to_gmt64(interval) > now)) {
         continue;
      }

      for (i = 0, found = 1; 
           i < (int) lGetUlong(qep, QU_nsuspend) && found; 
           i++) {
         found = 0;
         /* find one running job in suspend queue */
         if (select4suspension(job_list, qep, &jep, &ja_task))
            break;

         /* generate suspend order for found job */
         found = 1;
         orders->configOrderList = sge_create_orders(orders->configOrderList, 
                                                     ORT_suspend_on_threshold, 
                                                     jep, ja_task, nullptr, true);

         DPRINTF("++++ suspending job " sge_u32 "/" sge_u32 " on threshold\n",
                 lGetUlong(jep, JB_job_number), lGetUlong(ja_task, JAT_task_number));

         /* prevent multiple selection of this job */
         lSetUlong(ja_task, JAT_state, 
                   lGetUlong(ja_task, JAT_state) | JSUSPENDED_ON_THRESHOLD);
      }

      if (i==0 && !found) {
         DPRINTF("found no jobs for sot in queue %s\n", lGetString(qep, QU_full_name));
      }
   }
   
   DRETURN_VOID;
}


void 
unsuspend_job_in_queues( lList *queue_list, lList *job_list, order_t *orders) 
{
   int i, found;
   lListElem *jep = nullptr, *ja_task = nullptr;
   lListElem *qep;

   DENTER(TOP_LAYER);

   u_long64 now = sge_get_gmt64();
   for_each_rw (qep, queue_list) {
      u_long32 interval;

      /* are suspend thresholds enabled? */
      parse_ulong_val(nullptr, &interval, TYPE_TIM, lGetString(qep, QU_suspend_interval), nullptr, 0);

       if (interval == 0
           || !lGetUlong(qep, QU_nsuspend)
           || !lGetList(qep, QU_suspend_thresholds)) {
          continue;
       } 

      /* check time stamp */
      if (lGetUlong64(qep, QU_last_suspend_threshold_ckeck) &&
         (lGetUlong64(qep, QU_last_suspend_threshold_ckeck) + sge_gmt32_to_gmt64(interval) > now)) {
         if (DPRINTF_IS_ACTIVE) {
            DSTRING_STATIC(dstr1, 64);
            DSTRING_STATIC(dstr2, 64);
            DPRINTF("queue was last checked at %s (interval = %s, now = %s)\n",
                    sge_ctime64(lGetUlong64(qep, QU_last_suspend_threshold_ckeck), &dstr1),
                    lGetString(qep, QU_suspend_interval), sge_ctime64(now, &dstr2));
         }
         continue;
      }

      for (i = 0, found = 1; 
           i < (int) lGetUlong(qep, QU_nsuspend) && found; 
           i++) {
         found = 0;
         /* find one running job in suspend queue */
         if (select4unsuspension(job_list, qep, &jep, &ja_task)) {
            break;
         }   

         /* generate unsuspend order for found job */
         found = 1;
         orders->configOrderList = sge_create_orders(orders->configOrderList, 
                                                        ORT_unsuspend_on_threshold, 
                                                        jep, ja_task, nullptr, true);

         DPRINTF("---- unsuspending job " sge_u32 "/" sge_u32 " on threshold\n",
                 lGetUlong(jep, JB_job_number), lGetUlong(ja_task, JAT_task_number));

         /* prevent multiple selection of this job */
         lSetUlong(ja_task, JAT_state, lGetUlong(ja_task, JAT_state) & ~JSUSPENDED_ON_THRESHOLD);
      }
      
      if (i==0 && !found) {
         DPRINTF("found no jobs for usot in queue %s\n", lGetString(qep, QU_full_name));
      }
   }

   DRETURN_VOID;
}
   

static int 
select4suspension(lList *job_list, lListElem *qep, lListElem **jepp, 
                  lListElem **ja_taskp) 
{
   u_long32 jstate;
   lListElem *jep, *ja_task;
   lListElem *jshortest = nullptr, *shortest = nullptr;
   const char *qnm;

   DENTER(TOP_LAYER);

   qnm = lGetString(qep, QU_full_name);
   if (qinstance_state_is_manual_suspended(qep) ||
       qinstance_state_is_susp_on_sub(qep) ||
       qinstance_state_is_cal_suspended(qep)) {
      DRETURN(-1);
   }
  
   for_each_rw (jep, job_list) {

      /* job running */ 
      for_each_rw (ja_task, lGetList(jep, JB_ja_tasks)) {
         jstate = lGetUlong(ja_task, JAT_state);
         if (!(jstate & JRUNNING) || 
             (jstate & JSUSPENDED) || (jstate & JSUSPENDED_ON_THRESHOLD)) {
            continue;
         }

         /*
         ** if the current task is
         **    a job / one task of an array-job
         **    a master-task of a pe-job with sub-tasks in this queue
         ** then it is a potential candidate which we could suspend
         */
         if (lGetSubStr(ja_task, JG_qname, qnm, JAT_granted_destin_identifier_list) == nullptr) {
            continue;
         }

         /* select job that runs shortest time for suspension */
         if (shortest == nullptr ||
             // @todo: CS-353 the condition looks incorrect
             lGetUlong64(shortest, JAT_start_time) < lGetUlong64(ja_task, JAT_start_time)) {
            shortest = ja_task;
            jshortest = jep;
         }
      }
   }

   if (shortest) {
      *jepp = jshortest; 
      *ja_taskp = shortest; 
   }

   DRETURN(shortest?0:1);
}

static int select4unsuspension(
lList *job_list,
lListElem *qep,
lListElem **jepp,
lListElem **ja_taskp 
) {
   u_long32 jstate;
   lListElem *jep, *jlongest = nullptr, *longest = nullptr, *ja_task;
   const char *qnm;

   DENTER(TOP_LAYER);

   qnm = lGetString(qep, QU_full_name);

   for_each_rw (jep, job_list) {
      for_each_rw (ja_task, lGetList(jep, JB_ja_tasks)) {
         /* job must be suspended */ 
         jstate = lGetUlong(ja_task, JAT_state);
         if (!(jstate & JSUSPENDED_ON_THRESHOLD)) {
            continue;
         }

         /* is this the master queue of this job ? */
         if (strcmp(qnm, lGetString(lFirst(lGetList(ja_task, 
               JAT_granted_destin_identifier_list)), JG_qname))) {
            DTRACE;
            continue;
         }
         
         /* select task that runs longest time for unsuspension */
         if (longest == nullptr ||
             lGetUlong64(longest, JAT_start_time) > lGetUlong64(ja_task, JAT_start_time)) {
            longest = ja_task;
            jlongest = jep;
         }
      }
   }

   if (longest) {
      *jepp = jlongest; 
      *ja_taskp = longest; 
   }

   DRETURN(longest?0:1);
}

