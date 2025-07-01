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

#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/sge_utility.h"

#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_ClientExecd.h"

#include "spool/classic/read_write_job.h"

#include "job_report_execd.h"
#include "execd_ck_to_do.h"
#include "setup_execd.h"
#include "reaper_execd.h"
#include "execution_states.h"
#include "sge.h"
#include "msg_daemons_common.h"
#include "msg_execd.h"

extern char execd_spool_dir[SGE_PATH_MAX];
extern lList *jr_list;

static char execd_messages_file[SGE_PATH_MAX];

/*-------------------------------------------------------------------*/
void sge_setup_sge_execd(const char* tmp_err_file_name)
{
   char err_str[MAX_STRING_SIZE];
   int allowed_get_conf_errors     = 5;
   char* spool_dir = nullptr;
   const char *unqualified_hostname = component_get_unqualified_hostname();
   const char *admin_user = bootstrap_get_admin_user();

   DENTER(TOP_LAYER);

   /* TODO: is this the right place to switch the user ?
            ports below 1024 ok */
   /*
   ** switch to admin user
   */
   if (sge_set_admin_username(admin_user, err_str, sizeof(err_str))) {
      CRITICAL(SFNMAX, err_str);
      /* TODO: remove */
      sge_exit(1);
   }

   if (sge_switch2admin_user()) {
      CRITICAL(SFNMAX, MSG_ERROR_CANTSWITCHTOADMINUSER);
      /* TODO: remove */
      sge_exit(1);
   }

   while (ocs::gdi::ClientExecd::gdi_wait_for_conf(&Execd_Config_List)) {
      if (allowed_get_conf_errors-- <= 0) {
         CRITICAL(SFNMAX, MSG_EXECD_CANT_GET_CONFIGURATION_EXIT);
         /* TODO: remove */
         sge_exit(1);
      }
      sleep(1);
      ocs::gdi::ClientBase::gdi_get_act_master_host(true);
   }
   sge_show_conf();         


   /* get aliased hostname */
   /* TODO: is this call needed ? */
   reresolve_qualified_hostname();
   spool_dir = mconf_get_execd_spool_dir();

   DPRINTF("chdir(\"/\")----------------------------\n");
   sge_chdir_exit("/",1);
   DPRINTF("Making directories----------------------------\n");
   sge_mkdir(spool_dir, 0755, true, false);
   DPRINTF("chdir(\"%s\")----------------------------\n", spool_dir);
   sge_chdir_exit(spool_dir,1);
   sge_mkdir(unqualified_hostname, 0755, true, false);
   DPRINTF("chdir(\"%s\",me.unqualified_hostname)--------------------------\n", unqualified_hostname);
   sge_chdir_exit(unqualified_hostname, 1); 
   /* having passed the  previous statement we may 
      log messages into the ERR_FILE  */
   if ( tmp_err_file_name != nullptr) {
      sge_copy_append((char*)tmp_err_file_name, ERR_FILE, SGE_MODE_APPEND);
   }
   sge_switch2start_user();
   if ( tmp_err_file_name != nullptr) {
      unlink(tmp_err_file_name);
   }
   sge_switch2admin_user();
   log_state_set_log_as_admin_user(1);
   snprintf(execd_messages_file, sizeof(execd_messages_file), "%s/%s/%s", spool_dir, unqualified_hostname, ERR_FILE);
   log_state_set_log_file(execd_messages_file);
   snprintf(execd_spool_dir, sizeof(execd_spool_dir), "%s/%s", spool_dir, unqualified_hostname);
   
   DPRINTF("Making directories----------------------------\n");
   sge_mkdir(EXEC_DIR, 0775, true, false);
   sge_mkdir(JOB_DIR, 0775, true, false);
   sge_mkdir(ACTIVE_DIR,  0775, true, false);

#if defined(OCS_HWLOC) || defined(BINDING_SOLARIS)
   /* initialize processor topology */
   if (initialize_topology() != true) {
      DPRINTF("Couldn't initialize topology-----------------------\n");
   }
#endif

   sge_free(&spool_dir);
   DRETURN_VOID;
}

int job_initialize_job(lListElem *job)
{
   u_long32 job_id;
   lListElem *ja_task;
   const lListElem *pe_task;
   DENTER(TOP_LAYER);

   job_id = lGetUlong(job, JB_job_number); 
   for_each_rw (ja_task, lGetList(job, JB_ja_tasks)) {
      u_long32 ja_task_id;

      ja_task_id = lGetUlong(ja_task, JAT_task_number);

      add_job_report(job_id, ja_task_id, nullptr, job);
                                                                                      /* add also job reports for tasks */
      for_each_ep(pe_task, lGetList(ja_task, JAT_task_list)) {
         add_job_report(job_id, ja_task_id, lGetString(pe_task, PET_id), job);
      }

      if (mconf_get_simulate_jobs()) {
         /* nothing to do for simulated jobs */
         continue;
      }

      /* does active dir exist ? */
      if (lGetUlong(ja_task, JAT_status) == JRUNNING ||
          lGetUlong(ja_task, JAT_status) == JWAITING4OSJID) {
         SGE_STRUCT_STAT stat_buffer;
         stringT active_dir = "";

         sge_get_file_path(active_dir, sizeof(active_dir), JOB_ACTIVE_DIR, FORMAT_DEFAULT, SPOOL_WITHIN_EXECD, job_id, ja_task_id, nullptr);
         if (SGE_STAT(active_dir, &stat_buffer)) {
            /* lost active directory - initiate cleanup for job */
            execd_job_run_failure(job, ja_task, nullptr, "lost active dir of running " "job", GFSTATE_HOST);
            continue;
         }
      }

#ifdef COMPILE_DC
      {
         int ret;
         /* register still running jobs at ptf */
         if (lGetUlong(ja_task, JAT_status) == JRUNNING) {
            ret = register_at_ptf(job, ja_task, nullptr);
            if (ret) {
               ERROR(MSG_JOB_XREGISTERINGJOBYATPTFDURINGSTARTUP_SU, (ret == 1 ? MSG_DELAYED : MSG_FAILED), job_id);
            }
         }
         for_each_ep(pe_task, lGetList(ja_task, JAT_task_list)) {
            if (lGetUlong(pe_task, PET_status) == JRUNNING) {
               ret=register_at_ptf(job, ja_task, pe_task);
               if (ret) {
                  ERROR(MSG_JOB_XREGISTERINGJOBYTASKZATPTFDURINGSTARTUP_SUS, (ret == 1 ? MSG_DELAYED : MSG_FAILED), job_id, lGetString(pe_task, PET_id));
               }
            }
         }
      }
#endif
   }     
   DRETURN(0);           
}
