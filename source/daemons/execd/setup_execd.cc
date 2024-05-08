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

#include "uti/sge_bootstrap_files.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "cull/cull_file.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_binding.h"
#include "sgeobj/sge_utility.h"

#include "spool/classic/read_write_job.h"

#include "job_report_execd.h"
#include "execd_ck_to_do.h"
#include "setup_execd.h"
#include "reaper_execd.h"
#include "execution_states.h"
#include "sge.h"
#include "msg_common.h"
#include "msg_daemons_common.h"
#include "msg_execd.h"
#include "execd.h"

extern char execd_spool_dir[SGE_PATH_MAX];
extern lList *jr_list;

static char execd_messages_file[SGE_PATH_MAX];

/*-------------------------------------------------------------------*/
void sge_setup_sge_execd_before_register() {
   char err_str[MAX_STRING_SIZE];
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

   DRETURN_VOID;
}

void
execd_reread_act_qmaster() {
   static u_long32 last_qmaster_file_read = 0;

   DENTER(TOP_LAYER);

   u_long32 now = sge_get_gmt();
   if (now - last_qmaster_file_read >= 30) {
      gdi_get_act_master_host(true);
      DPRINTF("re-read actual qmaster file\n");
      last_qmaster_file_read = now;
   }

   DRETURN_VOID;
}

static int gdi_wait_for_conf(lList **conf_list) {
   lListElem *global = nullptr;
   lListElem *local = nullptr;
   int ret_val;
   int ret;
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *cell_root = bootstrap_get_cell_root();
   u_long32 progid = component_get_component_id();

   /* TODO: move this function to execd */
   DENTER(GDI_LAYER);
   /*
    * for better performance retrieve 2 configurations
    * in one gdi call
    */
   DPRINTF("qualified hostname: %s\n", qualified_hostname);

   while ((ret = gdi_get_configuration(qualified_hostname, &global, &local))) {
      if (ret == -6 || ret == -7) {
         /* confict: endpoint not unique or no permission to get config */
         DRETURN(-1);
      }

      if (ret == -8) {
         /* access denied */
         sge_get_com_error_flag(progid, SGE_COM_ACCESS_DENIED, true);
         sleep(30);
      }

      DTRACE;
      cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
      ret_val = cl_commlib_trigger(handle, 1);
      switch (ret_val) {
         case CL_RETVAL_SELECT_TIMEOUT:
            sleep(1);  /* If we could not establish the connection */
            break;
         case CL_RETVAL_OK:
            break;
         default:
            sleep(1);  /* for other errors */
            break;
      }

      execd_reread_act_qmaster();
   }

   ret = merge_configuration(nullptr, progid, cell_root, global, local, nullptr);
   if (ret) {
      DPRINTF("Error %d merging configuration \"%s\"\n", ret, qualified_hostname);
   }

   /*
    * we don't keep all information, just the name and the version
    * the entries are freed
    */
   lSetList(global, CONF_entries, nullptr);
   lSetList(local, CONF_entries, nullptr);
   lFreeList(conf_list);
   *conf_list = lCreateList("config list", CONF_Type);
   lAppendElem(*conf_list, global);
   lAppendElem(*conf_list, local);
   DRETURN(0);
}


void sge_setup_execd_after_register(const char *tmp_err_file_name) {
   int allowed_get_conf_errors = 5;
   char *spool_dir = nullptr;
   const char *unqualified_hostname = component_get_unqualified_hostname();

   DENTER(TOP_LAYER);
   while (gdi_wait_for_conf(&Execd_Config_List)) {
      if (allowed_get_conf_errors-- <= 0) {
         CRITICAL(SFNMAX, MSG_EXECD_CANT_GET_CONFIGURATION_EXIT);
         /* TODO: remove */
         sge_exit(1);
      }
      sleep(1);
      gdi_get_act_master_host(true);
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
   if (tmp_err_file_name != nullptr) {
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

#if defined(OGE_HWLOC)
   /* initialize processor topology */
   if (!initialize_topology()) {
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
            execd_job_run_failure(job, ja_task, nullptr, "lost active dir of running "
                                  "job", GFSTATE_HOST);
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
               ERROR(MSG_JOB_XREGISTERINGJOBYATPTFDURINGSTARTUP_SU, (ret == 1 ? MSG_DELAYED : MSG_FAILED), sge_u32c(job_id));
            }
         }
         for_each_ep(pe_task, lGetList(ja_task, JAT_task_list)) {
            if (lGetUlong(pe_task, PET_status) == JRUNNING) {
               ret=register_at_ptf(job, ja_task, pe_task);
               if (ret) {
                  ERROR(MSG_JOB_XREGISTERINGJOBYTASKZATPTFDURINGSTARTUP_SUS, (ret == 1 ? MSG_DELAYED : MSG_FAILED), sge_u32c(job_id), lGetString(pe_task, PET_id));
               }
            }
         }
      }
#endif
   }     
   DRETURN(0);           
}
