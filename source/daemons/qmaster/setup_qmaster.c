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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  
#include <errno.h>

#include "sge.h"
#include "sge_conf.h"
#include "commlib.h"
#include "subordinate_qmaster.h"
#include "sge_calendar_qmaster.h"
#include "sge_sched.h"
#include "sge_all_listsL.h"
#include "sge_host.h"
#include "sge_host_qmaster.h"
#include "sge_pe_qmaster.h"
#include "sge_queue_qmaster.h"
#include "sge_manop_qmaster.h"
#include "slots_used.h"
#include "sge_job_qmaster.h"
#include "configuration_qmaster.h"
#include "qmaster_heartbeat.h"
#include "qm_name.h"
#include "sched_conf_qmaster.h"
#include "sge_sharetree.h"
#include "sge_sharetree_qmaster.h"
#include "sge_userset.h"
#include "sge_feature.h"
#include "sge_userset_qmaster.h"
#include "sge_ckpt_qmaster.h"
#include "sge_utility.h"
#include "setup.h"
#include "setup_qmaster.h"
#include "sge_prog.h"
#include "sgermon.h"
#include "sge_log.h"
#include "sge_host.h"
#include "config_file.h"
#include "sge_qmod_qmaster.h"
#include "time_event.h"
#include "sge_give_jobs.h"
#include "setup_path.h"
#include "reschedule.h"
#include "msg_daemons_common.h"
#include "msg_qmaster.h"
#include "reschedule.h"
#include "sge_job.h"
#include "sge_spool.h"
#include "sge_unistd.h"
#include "sge_uidgid.h"
#include "sge_io.h"
#include "sge_sharetree_qmaster.h"
#include "sge_answer.h"
#include "sge_pe.h"
#include "sge_queue.h"
#include "sge_ckpt.h"
#include "sge_userprj.h"
#include "sge_manop.h"
#include "sge_calendar.h"
#include "sge_sharetree.h"
#include "sge_hgroup.h"
#include "sge_cuser.h"
#include "sge_centry.h"

#include "spool/sge_spooling.h"
#include "spool/dynamic/sge_spooling_loader.h"

#include "msg_common.h"

static int 
remove_invalid_job_references(int user);

static int 
debit_all_jobs_from_qs(void);

static bool 
init_spooling_params(const char **shlib, const char **args);

static bool
init_admin_user(void);

/*------------------------------------------------------------*/
int sge_setup_qmaster()
{
   lListElem *jep, *ep, *tmpqep;
   static bool first = true;
   int ret;
   lListElem *lep = NULL;
   char err_str[1024];
   u_long32 state;
#ifdef PW   
   extern u_long32 pw_num_submit;
#endif   
   extern int new_config;
   lListElem *spooling_context = NULL;
   lList *answer_list = NULL;

   DENTER(TOP_LAYER, "sge_setup_qmaster");

   if (first)
      first = false;
   else {
      CRITICAL((SGE_EVENT, MSG_SETUP_SETUPMAYBECALLEDONLYATSTARTUP));
      DEXIT;
      return -1;
   }   

   if (!init_admin_user()) {
      CRITICAL((SGE_EVENT, "cannot determine admin_user\n"));
      SGE_EXIT(1);
   }

   if (sge_switch2admin_user()) {
      CRITICAL((SGE_EVENT, MSG_ERROR_CANTSWITCHTOADMINUSER));
      SGE_EXIT(1);
   }

   /* register our error function for use in replace_params() */
   config_errfunc = set_error;

   /*
    * Initialize Master lists and hash tables, if necessary 
    */
   if (Master_Job_List == NULL) {
      Master_Job_List = lCreateList("Master_Job_List", JB_Type);
   }
   cull_hash_new(Master_Job_List, JB_owner, 0);

   /* create spooling context */
   {
      const char *shlib;
      const char *args;

      if (!init_spooling_params(&shlib, &args)) {
         CRITICAL((SGE_EVENT, "unable to initialize spooling\n"));
         SGE_EXIT(1);
      }
      
      spooling_context = spool_create_dynamic_context(&answer_list, 
                                                      shlib, args);
      answer_list_output(&answer_list);
      if (spooling_context == NULL) {
         CRITICAL((SGE_EVENT, "unable to create spooling context\n"));
         SGE_EXIT(1);
      }

      if (!spool_startup_context(&answer_list, spooling_context, true)) {
         CRITICAL((SGE_EVENT, "unable to startup spooling context\n"));
         SGE_EXIT(1);
      }
      answer_list_output(&answer_list);

      spool_set_default_context(spooling_context);
   }

   /*
   ** get cluster configuration
   */
   spool_read_list(&answer_list, spooling_context, &Master_Config_List, SGE_TYPE_CONFIG);
   answer_list_output(&answer_list);

   ret = select_configuration(uti_state_get_qualified_hostname(), Master_Config_List, &lep);
   if (ret) {
      if (ret == -3)
         WARNING((SGE_EVENT, MSG_CONFIG_FOUNDNOLOCALCONFIGFORQMASTERHOST_S,
                 uti_state_get_qualified_hostname()));
      else {           
         ERROR((SGE_EVENT, MSG_CONFIG_ERRORXSELECTINGCONFIGY_IS, ret, uti_state_get_qualified_hostname()));
         return -1;
      }   
   }
   ret = merge_configuration( lGetElemHost(Master_Config_List, CONF_hname, SGE_GLOBAL_NAME), lep, &conf, NULL);
   if (ret) {
      ERROR((SGE_EVENT, MSG_CONFIG_ERRORXMERGINGCONFIGURATIONY_IS, ret, uti_state_get_qualified_hostname()));
      return -1;
   }
   sge_show_conf();         
   new_config = 1;

   /* pass max unheard to commlib */
   set_commlib_param(CL_P_LT_HEARD_FROM_TIMEOUT, conf.max_unheard, NULL, NULL);

   /* get aliased hostname from commd */
   reresolve_me_qualified_hostname();

   /*
   ** build and change to master spool dir
   */
   DPRINTF(("chdir(\"/\")----------------------------\n"));
   sge_chdir_exit("/", 1);

   DPRINTF(("Making directories----------------------------\n"));
   sge_mkdir(conf.qmaster_spool_dir, 0755, 1, 0);

   DPRINTF(("chdir("SFQ")----------------------------\n", conf.qmaster_spool_dir));
   sge_chdir_exit(conf.qmaster_spool_dir, 1);

   /* 
   ** we are in the master spool dir now 
   ** log messages into ERR_FILE in master spool dir 
   */
   sge_copy_append(TMP_ERR_FILE_QMASTER, ERR_FILE, SGE_MODE_APPEND);
   sge_switch2start_user();
   unlink(TMP_ERR_FILE_QMASTER);   
   sge_switch2admin_user();
   log_state_set_log_as_admin_user(1);
   log_state_set_log_file(ERR_FILE);

   /* 
   ** increment the heartbeat as early as possible 
   ** and write our name to the act_qmaster file
   ** the lock file will be removed as late as possible
   */
   inc_qmaster_heartbeat(QMASTER_HEARTBEAT_FILE);

   /* 
   ** write our host name to the act_qmaster file 
   */
   if (write_qm_name(uti_state_get_qualified_hostname(), path_state_get_act_qmaster_file(), err_str)) {
      ERROR((SGE_EVENT, "%s\n", err_str));
      SGE_EXIT(1);
   }

   /*
   ** read in all objects and check for correctness
   */
   DPRINTF(("Complex Attributes----------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_CEntry_List, SGE_TYPE_CENTRY);
   answer_list_output(&answer_list);

   DPRINTF(("host_list----------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Exechost_List, SGE_TYPE_EXECHOST);
   spool_read_list(&answer_list, spooling_context, &Master_Adminhost_List, SGE_TYPE_ADMINHOST);
   spool_read_list(&answer_list, spooling_context, &Master_Submithost_List, SGE_TYPE_SUBMITHOST);
   answer_list_output(&answer_list);

   if (!host_list_locate(Master_Exechost_List, SGE_TEMPLATE_NAME)) {
      /* add an exec host "template" */
      if (sge_add_host_of_type(SGE_TEMPLATE_NAME, SGE_EXECHOST_LIST))
         ERROR((SGE_EVENT, MSG_CONFIG_ADDINGHOSTTEMPLATETOEXECHOSTLIST));
   }

   /* add host "global" to Master_Exechost_List as an exec host */
   if (!host_list_locate(Master_Exechost_List, SGE_GLOBAL_NAME)) {
      /* add an exec host "global" */
      if (sge_add_host_of_type(SGE_GLOBAL_NAME, SGE_EXECHOST_LIST))
         ERROR((SGE_EVENT, MSG_CONFIG_ADDINGHOSTGLOBALTOEXECHOSTLIST));
   }

   /* add qmaster host to Master_Adminhost_List as an administrativ host */
   if (!host_list_locate(Master_Adminhost_List, uti_state_get_qualified_hostname())) {
      if (sge_add_host_of_type(uti_state_get_qualified_hostname(), SGE_ADMINHOST_LIST)) {
         DEXIT;
         return -1;
      }
   }

#ifdef PW
   /* check for licensed # of submit hosts */
   if ((ret=sge_count_uniq_hosts(Master_Adminhost_List, 
         Master_Submithost_List)) < 0) {
      /* s.th.'s wrong, but we can't blame it on the user so we
       * keep truckin'
       */
      ERROR((SGE_EVENT, MSG_SGETEXT_CANTCOUNT_HOSTS_S, SGE_FUNC));
   } else {
      if (pw_num_submit < ret) {
         /* we've a license violation */
         ERROR((SGE_EVENT, MSG_SGETEXT_TOOFEWSUBMHLIC_II, (int) pw_num_submit, ret));
         return -1;
      }
   }
#endif

   DPRINTF(("manager_list----------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Manager_List, SGE_TYPE_MANAGER);
   answer_list_output(&answer_list);
   if (!manop_is_manager("root")) {
      ep = lAddElemStr(&Master_Manager_List, MO_name, "root", MO_Type);

      if (!spool_write_object(&answer_list, spooling_context, ep, "root", SGE_TYPE_MANAGER)) {
         answer_list_output(&answer_list);
         CRITICAL((SGE_EVENT, MSG_CONFIG_CANTWRITEMANAGERLIST)); 
         return -1;
      }
   }
   for_each(ep, Master_Manager_List) 
      DPRINTF(("%s\n", lGetString(ep, MO_name)));

   DPRINTF(("host group definitions-----------\n"));
   spool_read_list(&answer_list, spooling_context, hgroup_list_get_master_list(), 
                   SGE_TYPE_HGROUP);
   answer_list_output(&answer_list);

   DPRINTF(("operator_list----------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Operator_List, SGE_TYPE_OPERATOR);
   answer_list_output(&answer_list);
   if (!manop_is_operator("root")) {
      ep = lAddElemStr(&Master_Operator_List, MO_name, "root", MO_Type);

      if (!spool_write_object(&answer_list, spooling_context, ep, "root", SGE_TYPE_OPERATOR)) {
         answer_list_output(&answer_list);
         CRITICAL((SGE_EVENT, MSG_CONFIG_CANTWRITEOPERATORLIST)); 
         return -1;
      }
   }
   for_each(ep, Master_Operator_List) 
      DPRINTF(("%s\n", lGetString(ep, MO_name)));


   DPRINTF(("userset_list------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Userset_List, SGE_TYPE_USERSET);
   answer_list_output(&answer_list);

   DPRINTF(("calendar list ------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Calendar_List, SGE_TYPE_CALENDAR);
   answer_list_output(&answer_list);

#ifndef __SGE_NO_USERMAPPING__
   DPRINTF(("administrator user mapping-----------\n"));
   spool_read_list(&answer_list, spooling_context, cuser_list_get_master_list(), SGE_TYPE_CUSER);
   answer_list_output(&answer_list);
#endif

   DPRINTF(("queue_list---------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Queue_List, SGE_TYPE_QUEUE);
   answer_list_output(&answer_list);
   queue_list_set_unknown_state_to(Master_Queue_List, NULL, 0, 1);


   DPRINTF(("pe_list---------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Pe_List, SGE_TYPE_PE);
   answer_list_output(&answer_list);

   DPRINTF(("ckpt_list---------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Ckpt_List, SGE_TYPE_CKPT);
   answer_list_output(&answer_list);

   DPRINTF(("job_list-----------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Job_List, SGE_TYPE_JOB);
   answer_list_output(&answer_list);

   for_each(jep, Master_Job_List) {
      DPRINTF(("JOB "u32" PRIORITY %d\n", lGetUlong(jep, JB_job_number), 
            (int)lGetUlong(jep, JB_priority) - BASE_PRIORITY));

      /* doing this operation we need the complete job list read in */
      job_suc_pre(jep);
   }

   /* 
      if the job is in state running 
      we have to register each slot 
      in a queue and in the parallel 
      environment if the job is a 
      parallel one
   */
   debit_all_jobs_from_qs(); 
   debit_all_jobs_from_pes(Master_Pe_List); 
         
   /* clear suspend on subordinate flag in QU_state */ 
   for_each(tmpqep, Master_Queue_List) {
      state = lGetUlong(tmpqep, QU_state);
      CLEARBIT(QSUSPENDED_ON_SUBORDINATE, state);
      lSetUlong(tmpqep, QU_state, state);
   }

   /* recompute suspend on subordinate caching fields */
   /* here we assume that all jobs are debited on all queues */
   for_each(tmpqep, Master_Queue_List) {
      lList *to_suspend;

      if (check_subordinate_list(NULL, lGetString(tmpqep, QU_qname), lGetHost(tmpqep, QU_qhostname), 
            lGetUlong(tmpqep, QU_job_slots), lGetList(tmpqep, QU_subordinate_list), 
            CHECK4SETUP)!=STATUS_OK) {
         DEXIT; /* inconsistent subordinates */
         return -1;
      }
      to_suspend = NULL;
      copy_suspended(&to_suspend, lGetList(tmpqep, QU_subordinate_list), 
         qslots_used(tmpqep), lGetUlong(tmpqep, QU_job_slots), 0);
      suspend_all(to_suspend, 1); /* just recompute */
      lFreeList(to_suspend);
   }

   /* calendar */
   {
      lListElem *cep;
      for_each (cep, Master_Calendar_List) 
         calendar_update_queue_states(cep, NULL, NULL);
   }

   /* rebuild signal resend events */
   rebuild_signal_events();

   /* scheduler configuration stuff */
   DPRINTF(("scheduler config -----------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Sched_Config_List, SGE_TYPE_SCHEDD_CONF);
   /* JG: TODO: reading the schedd configuration may fail, 
    * as it is not created at install time.
    * The corresponding error message is confusing, so do not output the error.
    * Better: Create config at install time (trough spooldefaults)
   answer_list_output(&answer_list);
    */
   if (lGetNumberOfElem(Master_Sched_Config_List) == 0) {
      lListElem *ep = schedd_conf_create_default();

      if (Master_Sched_Config_List == NULL) {
         Master_Sched_Config_List = lCreateList("schedd config list", SC_Type);
      }
      
      lAppendElem(Master_Sched_Config_List, ep);
      spool_write_object(&answer_list, spool_get_default_context(), ep, NULL, SGE_TYPE_SCHEDD_CONF);
      answer_list_output(&answer_list);
   }

   if (feature_is_enabled(FEATURE_SGEEE)) {

      /* SGEEE: read user list */
      spool_read_list(&answer_list, spooling_context, &Master_User_List, SGE_TYPE_USER);
      answer_list_output(&answer_list);

      remove_invalid_job_references(1);

      /* SGE: read project list */
      spool_read_list(&answer_list, spooling_context, &Master_Project_List, SGE_TYPE_PROJECT);
      answer_list_output(&answer_list);

      remove_invalid_job_references(0);
   }
   
   if (feature_is_enabled(FEATURE_SGEEE)) {
      /* SGEEE: read share tree */
      spool_read_list(&answer_list, spooling_context, &Master_Sharetree_List, SGE_TYPE_SHARETREE);
      answer_list_output(&answer_list);
      ep = lFirst(Master_Sharetree_List);
      if (ep) {
         lList *alp = NULL;
         lList *found = NULL;
         ret = check_sharetree(&alp, ep, Master_User_List, Master_Project_List, 
               NULL, &found);
         found = lFreeList(found);
         alp = lFreeList(alp); 
      }
   }

   /* RU: */
   /* initiate timer for all hosts because they start in 'unknown' state */ 
   if (Master_Exechost_List) {
      lListElem *host               = NULL;
      lListElem *global_host_elem   = NULL;
      lListElem *template_host_elem = NULL;

      /* get "global" element pointer */
      global_host_elem   = host_list_locate(Master_Exechost_List, SGE_GLOBAL_NAME);   

      /* get "template" element pointer */
      template_host_elem = host_list_locate(Master_Exechost_List, SGE_TEMPLATE_NAME);
  
      for_each(host, Master_Exechost_List) {
         if ( (host != global_host_elem ) && (host != template_host_elem ) ) {
            reschedule_add_additional_time(load_report_interval(host));
            reschedule_unknown_trigger(host);
            reschedule_add_additional_time(0); 
         }
      }
   }

   DEXIT;
   return 0;
}

/* get rid of still debited per job usage contained 
   in user or project object if the job is no longer existing */
static int remove_invalid_job_references(
int user 
) {
   lListElem *up, *upu, *next;
   u_long32 jobid;

   DENTER(TOP_LAYER, "remove_invalid_job_references");

   for_each (up, user?Master_User_List:Master_Project_List) {

      int spool_me = 0;
      next = lFirst(lGetList(up, UP_debited_job_usage));
      while ((upu=next)) {
         next = lNext(upu);

         jobid = lGetUlong(upu, UPU_job_number);
         if (!job_list_locate(Master_Job_List, jobid)) {
            lRemoveElem(lGetList(up, UP_debited_job_usage), upu);
            WARNING((SGE_EVENT, "removing reference to no longer existing job "u32" of %s "SFQ"\n",
                           jobid, user?"user":"project", lGetString(up, UP_name)));
            spool_me = 1;
         }
      }

      if (spool_me) {
         lList *answer_list = NULL;
         spool_write_object(&answer_list, spool_get_default_context(), up, 
                            lGetString(up, UP_name), user ? SGE_TYPE_USER : 
                                                            SGE_TYPE_PROJECT);
         answer_list_output(&answer_list);
      }
   }

   DEXIT;
   return 0;
}

static int debit_all_jobs_from_qs()
{
   lListElem *gdi;
   u_long32 slots, jid, tid;
   const char *queue_name;
   lListElem *hep, *master_hep, *next_jep, *jep, *qep, *next_jatep, *jatep;
   int ret = 0;

   DENTER(TOP_LAYER, "debit_all_jobs_from_qs");

   next_jep = lFirst(Master_Job_List);
   while ((jep=next_jep)) {
   
      /* may be we have to delete this job */   
      next_jep = lNext(jep);
      jid = lGetUlong(jep, JB_job_number);
      
      next_jatep = lFirst(lGetList(jep, JB_ja_tasks));
      while ((jatep = next_jatep)) {
         next_jatep = lNext(jatep);
         tid = lGetUlong(jatep, JAT_task_number);

         /* don't look at states - we only trust in 
            "granted destin. ident. list" */

         master_hep = NULL;
         for_each (gdi, lGetList(jatep, JAT_granted_destin_identifier_list)) {

            queue_name = lGetString(gdi, JG_qname);
            slots = lGetUlong(gdi, JG_slots);
            
            if (!(qep = queue_list_locate(Master_Queue_List, queue_name))) {
               ERROR((SGE_EVENT, MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU,  
                  queue_name, u32c(lGetUlong(jep, JB_job_number))));
               lRemoveElem(lGetList(jep, JB_ja_tasks), jatep);   
            }

            /* debit in all layers */
            debit_host_consumable(jep, host_list_locate(Master_Exechost_List,
                                  "global"), Master_CEntry_List, slots);
            debit_host_consumable(jep, hep = host_list_locate(
                     Master_Exechost_List, lGetHost(qep, QU_qhostname)), 
                     Master_CEntry_List, slots);
            debit_queue_consumable(jep, qep, Master_CEntry_List, slots);
            if (!master_hep)
               master_hep = hep;
         }

         /* check for resend jobs */
         if (lGetUlong(jatep, JAT_status) == JTRANSFERING) {
            trigger_job_resend(lGetUlong(jatep, JAT_start_time), master_hep, 
                  jid, tid);
         }
      }
   }

   DEXIT;
   return ret;
}

static bool 
init_spooling_params(const char **shlib, const char **args)
{
   static dstring args_out = DSTRING_INIT;
   const char *name[1] = { "qmaster_spool_dir" };
   char value[1][1025];

   DENTER(TOP_LAYER, "init_spooling_params");

   if (sge_get_confval_array(path_state_get_conf_file(), 1, name, value)) {
      ERROR((SGE_EVENT, "cannot read spooling parameters\n"));
      DEXIT;
      return false;
   }

   sge_dstring_sprintf(&args_out, "%s/%s;%s", 
                       path_state_get_cell_root(), COMMON_DIR, value[0]);
   
   *shlib = "none";
   *args  = sge_dstring_get_string(&args_out);

   DEXIT;
   return true;
}

static bool
init_admin_user(void)
{
   const char *name[1] = { "admin_user" };
   char value[1][1025];
   int ret;
   char err_str[MAX_STRING_SIZE];

   DENTER(TOP_LAYER, "init_admin_user");

   if (sge_get_confval_array(path_state_get_conf_file(), 1, name, value)) {
      ERROR((SGE_EVENT, "cannot read admin_user parameter\n"));
      DEXIT;
      return false;
   }

   ret = sge_set_admin_username(value[0], err_str);
   if (ret == -1) {
      ERROR((SGE_EVENT, err_str));
      DEXIT;
      return false;
   }

   DEXIT;
   return true;
}
