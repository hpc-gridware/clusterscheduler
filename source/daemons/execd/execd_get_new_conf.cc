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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"

#include "exec_job.h"
#include "execd_get_new_conf.h"
#include "sge_load_sensor.h"
#include "admin_mail.h"
#include "load_avg.h"
#include "msg_common.h"

#include <ocs_gdi_ClientExecd.h>
#include <reaper_execd.h>

#ifdef COMPILE_DC
#  include "ptf.h"
#endif

/*
** DESCRIPTION
**   retrieves new configuration from qmaster, very similar to what is
**   executed on startup. This function is triggered by the execd
**   dispatcher table when the tag TAG_GET_NEW_CONF is received.
*/
int do_get_new_conf(ocs::gdi::ClientServerBase::struct_msg_t *aMsg) {
   DENTER(TOP_LAYER);
   int ret;
   bool use_qidle = mconf_get_use_qidle();
   u_long32 dummy; /* always 0 */ 
   u_long32 old_reprioritization_enabled = mconf_get_reprioritize();

   unpackint(&(aMsg->buf), &dummy);

   const char *old_spool = mconf_get_execd_spool_dir();
   keep_active_t old_keep_active = mconf_get_keep_active();

   ret = ocs::gdi::ClientExecd::gdi_get_merged_configuration(&Execd_Config_List);
  
   const char *spool_dir = mconf_get_execd_spool_dir();
   if (strcmp(old_spool, spool_dir)) {
      WARNING(MSG_WARN_CHANGENOTEFFECTEDUNTILRESTARTOFEXECHOSTS, "execd_spool_dir");
   }

   // if the keep_active flag has changed, we need to enforce cleanup of old jobs
   // to get rid of old active jobs directories
   keep_active_t keep_active = mconf_get_keep_active();
   if (old_keep_active != keep_active) {
      set_enforce_cleanup_old_jobs();
   }

#ifdef COMPILE_DC
   if (old_reprioritization_enabled != mconf_get_reprioritize()) {
      /* Here we will make sure that each job which was started
         in SGEEE-Mode (reprioritization) will get its initial
         queue priority if this execd alternates to SGE-Mode */
      lListElem *job, *jatask, *petask;

      sge_switch2start_user();

      for_each_rw (job, *ocs::DataStore::get_master_list(SGE_TYPE_JOB)) {
         lListElem *master_queue;

         for_each_rw (jatask, lGetList(job, JB_ja_tasks)) {
            int priority;

            master_queue = responsible_queue(job, jatask, nullptr);
            priority = atoi(lGetString(master_queue, QU_priority));

            DPRINTF("Set priority of job " sge_uu32 "." sge_uu32 " running in queue  %s to %d\n",
            lGetUlong(job, JB_job_number), 
            lGetUlong(jatask, JAT_task_number),
            lGetString(master_queue, QU_full_name), priority);
            ptf_reinit_queue_priority(lGetUlong(job, JB_job_number),
                                      lGetUlong(jatask, JAT_task_number),
                                      nullptr, priority);

            for_each_rw(petask, lGetList(jatask, JAT_task_list)) {
               master_queue = responsible_queue(job, jatask, petask);
               priority = atoi(lGetString(master_queue, QU_priority));
               DPRINTF("Set priority of task " sge_uu32 "." sge_uu32 "-%s running in queue %s to %d\n",
               lGetUlong(job, JB_job_number), lGetUlong(jatask, JAT_task_number),
               lGetString(petask, PET_id), lGetString(master_queue, QU_full_name), priority);
               ptf_reinit_queue_priority(lGetUlong(job, JB_job_number),
                                         lGetUlong(jatask, JAT_task_number),
                                          lGetString(petask, PET_id),
                                          priority);
            }
         }
      }
      sge_switch2admin_user();
   }
#endif

   /*
   ** admin mail block is released on new conf
   */
   adm_mail_reset(BIT_ADM_NEW_CONF);

   sge_ls_qidle(use_qidle);
   DPRINTF("use_qidle: %d\n", use_qidle);

   sge_free(&old_spool);
   sge_free(&spool_dir);

   DRETURN(ret);
}


