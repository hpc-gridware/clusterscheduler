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
 *  Copyright: 2008 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cerrno>

#include "sge.h"

#include "uti/sge_bootstrap_env.h"
#include "uti/sge_log.h"
#include "uti/sge_monitor.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_security.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_suser.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/sge_conf.h"

#include "sge_userprj_qmaster.h"
#include "sge_userset_qmaster.h"
#include "sge_job_qmaster.h"
#include "symbols.h"
#include "msg_common.h"
#include "msg_qmaster.h"

#include <sge_str.h>

static bool
sge_job_verify_global_master_slave_queues(lList **alpp, const lListElem *jep) {
   bool ret = true;

   const lList *global_queue_requests = job_get_hard_queue_list(jep);
   if (global_queue_requests != nullptr) {
      if (job_get_hard_queue_list(jep, JRS_SCOPE_MASTER) != nullptr) {
         ERROR(MSG_JOB_GLOBALMASTERSLAVEQ_S, "master");
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
      if (ret && job_get_hard_queue_list(jep, JRS_SCOPE_SLAVE) != nullptr) {
         ERROR(MSG_JOB_GLOBALMASTERSLAVEQ_S, "slave");
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

   return ret;
}

static bool
sge_job_verify_global_master_slave_requests(lList **alpp, const lListElem *jep, bool soft) {
   bool ret = true;

   const lList *global_requests;
   if (soft) {
      global_requests = job_get_soft_resource_list(jep);
   } else {
      global_requests = job_get_hard_resource_list(jep);
   }

   if (global_requests != nullptr) {
      const lList *master_requests = job_get_hard_resource_list(jep, JRS_SCOPE_MASTER);
      const lList *slave_requests = job_get_hard_resource_list(jep, JRS_SCOPE_SLAVE);
      const lListElem *ep;
      for_each_ep(ep, global_requests) {
         const char *name = lGetString(ep, CE_name);
         if (centry_list_locate(master_requests, name) != nullptr) {
            ERROR(MSG_JOB_GLOBALMASTERSLAVE_SSS, soft ? "soft" : "hard", name, "master");
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
         if (centry_list_locate(slave_requests, name) != nullptr) {
            ERROR(MSG_JOB_GLOBALMASTERSLAVE_SSS, soft ? "soft" : "hard", name, "slave");
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
      }
   }

   return ret;
}

bool
sge_job_verify_slave_per_job_requests(lList **alpp, const lListElem *jep, const lList *centry_list) {
   bool ret = true;

   // we only allow slave hard requests, no soft requests
   const lList *request_list = job_get_hard_resource_list(jep, JRS_SCOPE_SLAVE);
   const lListElem *request;
   for_each_ep(request, request_list) {
      const char *name = lGetString(request, CE_name);
      if (name != nullptr) {
         const lListElem *centry = centry_list_locate(centry_list, name);
         if (centry != nullptr) {
            u_long32 consumable = lGetUlong(centry, CE_consumable);
            if (consumable == CONSUMABLE_JOB) {
               ERROR(MSG_JOB_SLAVEPERJOBREQUEST_S, name);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
         }
      }
   }


   return ret;
}

bool
sge_job_verify_per_host_requests(lList **alpp, const lListElem *jep, const lList *master_centry_list) {
   bool ret = true;

   const lList *master_request_list = job_get_hard_resource_list(jep, JRS_SCOPE_MASTER);
   const lList *slave_request_list = job_get_hard_resource_list(jep, JRS_SCOPE_SLAVE);
   if (master_request_list != nullptr && slave_request_list != nullptr) {
      const lListElem *master_request;
      for_each_ep (master_request, master_request_list) {
         const char *name = lGetString(master_request, CE_name);
         if (name != nullptr) {
            const lListElem *centry = centry_list_locate(master_centry_list, name);
            if (centry != nullptr) {
               u_long32 consumable = lGetUlong(centry, CE_consumable);
               if (consumable == CONSUMABLE_HOST) {
                  if (lGetElemStr(slave_request_list, CE_name, name) != nullptr) {
                     ERROR(MSG_JOB_PERHOSTINBOTHMASTERSLAVE_S, name);
                     answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                     ret = false;
                     break;
                  }
               }
            }
         }
      }
   }

   return ret;
}

static bool job_verify_soft_master_slave_requests(lList **alpp, const lListElem *jep) {
   bool ret = true;

   const lList *jrs_list = lGetList(jep, JB_request_set_list);
   if (jrs_list != nullptr) {
      const lListElem *jrs;
      for_each_ep(jrs, jrs_list) {
         u_long32 scope = lGetUlong(jrs, JRS_scope);

         // we do not allow master and slave soft queue requests
         if (scope == JRS_SCOPE_MASTER || scope == JRS_SCOPE_SLAVE) {
            if (lGetList(jrs, JRS_soft_queue_list) != nullptr) {
               ERROR(SFNMAX, MSG_JOB_MASTERSLAVESOFTQUEUE);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
            // we do not allow master and slave soft resource requests
            if (lGetList(jrs, JRS_soft_resource_list) != nullptr) {
               ERROR(SFNMAX, MSG_JOB_MASTERSLAVESOFTREQ);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
         }
      }
   }

   return ret;
}

static bool
job_verify_non_pe_soft_master_slave_requests(lList **alpp, const lListElem *jep) {
   bool ret = true;

   if (lGetString(jep, JB_pe) == nullptr) {
      const lList *jrs_list = lGetList(jep, JB_request_set_list);
      if (jrs_list != nullptr) {
         const lListElem *jrs;
         for_each_ep(jrs, jrs_list) {
            if (lGetUlong(jrs, JRS_scope) != JRS_SCOPE_GLOBAL) {
               ERROR(SFNMAX, MSG_JOB_MASTERSLAVENONPE);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               ret = false;
               break;
            }
         }
      }
   }
   return ret;
}

/**
 * @brief Do all request set related checks
 *
 * Calls all the request set verification functions above:
 * - sge_job_verify_global_master_slave_queues()
 * - sge_job_verify_global_master_slave_requests()
 * - sge_job_verify_slave_per_job_requests()
 * - sge_job_verify_per_host_requests()
 * - job_verify_soft_master_slave_requests()
 * - job_verify_non_pe_soft_master_slave_requests()
 *
 * @param alpp answer list which is filled in case of errors
 * @param jep job containing the request set
 * @param master_centry_list list of centry definitions
 */
bool
job_verify_adjust_request_set(lList **alpp, const lListElem *jep, const lList *master_centry_list) {
   bool ret = true;

   /* check for non-parallel job that define master or slave requests */
   if (ret) {
      ret = job_verify_non_pe_soft_master_slave_requests(alpp, jep);
   }

   // check for soft master or slave requests - we don't allow them (for now)
   if (ret) {
      ret = job_verify_soft_master_slave_requests(alpp, jep);
   }

   // verify that the there are no requests on the same variable in global scope and one of master or slave
   if (ret) {
      if (!sge_job_verify_global_master_slave_requests(alpp, jep, false) ||
          !sge_job_verify_global_master_slave_requests(alpp, jep, true)) {
         ret = false;
      }
   }

   // check for slave requests of per job consumables (which are only granted to the master task)
   if (ret) {
      ret = sge_job_verify_slave_per_job_requests(alpp, jep, master_centry_list);
   }

   // verify that there are no hard queue requests in the global scope and one of master or slave
   if (ret) {
      ret = sge_job_verify_global_master_slave_queues(alpp, jep);
   }

   // verify that per host requests are not in both master and slave requests
   if (ret) {
      ret = sge_job_verify_per_host_requests(alpp, jep, master_centry_list);
   }

   return ret;
}

int
sge_job_verify_adjust(lListElem *jep, lList **alpp, lList **lpp,
                      ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                      monitoring_t *monitor) {
   int ret = STATUS_OK;

   DENTER(TOP_LAYER);

   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   const lList *master_operator_list = *ocs::DataStore::get_master_list(SGE_TYPE_OPERATOR);
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   const lList *master_ckpt_list = *ocs::DataStore::get_master_list(SGE_TYPE_CKPT);
   const lList *master_user_list = *ocs::DataStore::get_master_list(SGE_TYPE_USER);
   const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);
   lList *master_suser_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_SUSER);

   if (jep == nullptr) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = STATUS_EUNKNOWN;
   }

   /* check min_uid */
   if (ret == STATUS_OK) {
      if (packet->uid < mconf_get_min_uid()) {
         ERROR(MSG_JOB_UID2LOW_II, (int) packet->uid, (int) mconf_get_min_uid());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /* check min_gid */
   if (ret == STATUS_OK) {
      if (packet->gid < mconf_get_min_gid()) {
         ERROR(MSG_JOB_GID2LOW_II, (int) packet->gid, (int) mconf_get_min_gid());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /*
    * adjust user and group
    *
    * we cannot rely on the information we got from the client
    * therefore we fill in the data we got from communication
    * library.
    */
   if (ret == STATUS_OK) {
      if (!job_set_owner_and_group(jep, packet->uid, packet->gid, packet->user, packet->group, packet->amount, packet->grp_array)) {
         ret = STATUS_EUNKNOWN;
      }
   }

   // Check if there are binding parameters that are required but unset
   ocs::Job::binding_set_missing_defaults(jep);

   /*
    * fill name and shortcut for all requests
    * fill numeric values for all bool, time, memory and int type requests
    * use the master_CEntry_list for all fills
    * JB_hard/soft_resource_list points to a CE_Type list
    */
   lListElem *jrs;
   for_each_rw(jrs, lGetList(jep, JB_request_set_list)) {
      u_long32 scope = lGetUlong(jrs, JRS_scope);
      DPRINTF("request set of scope %s\n", job_scope_name(scope));

      lList *hard_resource_list = lGetListRW(jrs, JRS_hard_resource_list);
      if (centry_list_fill_request(hard_resource_list, alpp, master_centry_list, false, true, false)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (compress_ressources(alpp, hard_resource_list, SGE_OBJ_JOB)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (!centry_list_is_correct(hard_resource_list, alpp)) {
         ret = STATUS_EUNKNOWN;
         break;
      }

      lList *soft_resource_list = lGetListRW(jrs, JRS_soft_resource_list);
      if (centry_list_fill_request(soft_resource_list, alpp, master_centry_list, false, true, false)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (compress_ressources(alpp, soft_resource_list, SGE_OBJ_JOB)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (deny_soft_consumables(alpp, soft_resource_list, master_centry_list)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      if (!centry_list_is_correct(soft_resource_list, alpp)) {
         ret = STATUS_EUNKNOWN;
         break;
      }

      lList *queue_list = lGetListRW(jrs, JRS_hard_queue_list);
      if (!qref_list_is_valid(queue_list, alpp, master_cqueue_list, master_hgroup_list, master_centry_list)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
      queue_list = lGetListRW(jrs, JRS_soft_queue_list);
      if (!qref_list_is_valid(queue_list, alpp, master_cqueue_list, master_hgroup_list, master_centry_list)) {
         ret = STATUS_EUNKNOWN;
         break;
      }
   }

   if (ret == STATUS_OK) {
      if (!job_verify_adjust_request_set(alpp, jep, master_centry_list)) {
         ret = STATUS_EUNKNOWN;
      }
   }

   /* check for qsh without DISPLAY set */
   if (ret == STATUS_OK) {
      if (JOB_TYPE_IS_QSH(lGetUlong(jep, JB_type))) {
         ret = job_check_qsh_display(jep, alpp, false);
      }
   }

   /*
    * verify and adjust array job ids:
    *    JB_ja_structure, JB_ja_n_h_ids, JB_ja_u_h_ids, 
    *    JB_ja_s_h_ids, JB_ja_o_h_ids, JB_ja_a_h_ids, JB_ja_z_ids
    */
   if (ret == STATUS_OK) {
      job_check_correct_id_sublists(jep, alpp);
      if (answer_list_has_error(alpp)) {
         ret = STATUS_EUNKNOWN;
      }
   }

   /*
    * resolve host names contained in path names
    */
   if (ret == STATUS_OK) {
      int s1, s2, s3, s4;

      s1 = job_resolve_host_for_path_list(jep, alpp, JB_stdout_path_list);
      s2 = job_resolve_host_for_path_list(jep, alpp, JB_stdin_path_list);
      s3 = job_resolve_host_for_path_list(jep, alpp, JB_shell_list);
      s4 = job_resolve_host_for_path_list(jep, alpp, JB_stderr_path_list);
      if (s1 != STATUS_OK || s2 != STATUS_OK || s3 != STATUS_OK || s4 != STATUS_OK) {
         ret = STATUS_EUNKNOWN;
      }
   }

   /* take care that non-binary jobs have a script */
   if (ret == STATUS_OK) {
      if ((!JOB_TYPE_IS_BINARY(lGetUlong(jep, JB_type)) &&
           !lGetString(jep, JB_script_ptr) && lGetString(jep, JB_script_file))) {
         ERROR(SFNMAX, MSG_JOB_NOSCRIPT);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /* set the jobs submission time */
   if (ret == STATUS_OK) {
      lSetUlong64(jep, JB_submission_time, sge_get_gmt64());
   }

   /* initialize the task template element and other sublists */
   if (ret == STATUS_OK) {
      lSetList(jep, JB_ja_tasks, nullptr);
      lSetList(jep, JB_jid_successor_list, nullptr);
      lSetList(jep, JB_ja_ad_successor_list, nullptr);
      if (lGetList(jep, JB_ja_template) == nullptr) {
         lAddSubUlong(jep, JAT_task_number, 0, JB_ja_template, JAT_Type);
      }
   }

   /* verify or set the account string */
   if (ret == STATUS_OK) {
      if (!lGetString(jep, JB_account)) {
         lSetString(jep, JB_account, DEFAULT_ACCOUNT);
      } else {
         if (verify_str_key(alpp, lGetString(jep, JB_account), MAX_VERIFY_STRING,
                            "account string", QSUB_TABLE) != STATUS_OK) {
            ret = STATUS_EUNKNOWN;
         }
      }
   }

   /* verify the job name */
   if (ret == STATUS_OK) {
      if (object_verify_name(jep, alpp, JB_job_name)) {
         ret = STATUS_EUNKNOWN;
      }
   }

   /* is the max. size of array jobs exceeded? */
   if (ret == STATUS_OK) {
      u_long32 max_aj_tasks = mconf_get_max_aj_tasks();

      if (max_aj_tasks > 0) {
         const lList *range_list = lGetList(jep, JB_ja_structure);
         u_long32 submit_size = range_list_get_number_of_ids(range_list);

         if (submit_size > max_aj_tasks) {
            ERROR(MSG_JOB_MORETASKSTHAN_U, max_aj_tasks);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
      }
   }

   /* 
    * JB_context contains a raw context list, which needs to be transformed into
    * a real context. For that, we have to take out the raw context and add it back
    * again, processed. 
    */
   if (ret == STATUS_OK) {
      lList *temp = nullptr;

      lXchgList(jep, JB_context, &temp);
      set_context(temp, jep);
      lFreeList(&temp);
   }

#if 0
   // if verification failed so far, we don't need to continue
   if (ret != STATUS_OK) {
      //DRETURN(ret);
   }
#endif

   /*
    * Following block should only be executed once, when the job has no job id.
    *
    * At first, we try to find a job id which is not yet used. AFTER that we need
    * to set the submission time. Only this makes wure that forced separation 
    * in time is effective in case of job ID rollover.
    */
   /* ORDER IS IMPORTANT */
   if (lGetUlong(jep, JB_job_number) == 0) {
      u_long32 jid;

      do {
         jid = sge_get_job_number(monitor);
      } while (lGetElemUlong(master_job_list, JB_job_number, jid));
      lSetUlong(jep, JB_job_number, jid);
      lSetUlong64(jep, JB_submission_time, sge_get_gmt64());
   }

   /*      
    * with interactive jobs, JB_exec_file is not set
    */
   if (lGetString(jep, JB_script_file)) {
      dstring string = DSTRING_INIT;
      sge_dstring_sprintf(&string, "%s/" sge_u32, EXEC_DIR, lGetUlong(jep, JB_job_number));
      lSetString(jep, JB_exec_file, sge_dstring_get_string(&string));
      sge_dstring_free(&string);
   }

   /* check max_jobs */
   if (job_list_register_new_job(master_job_list, mconf_get_max_jobs(), 0)) {
      INFO(MSG_JOB_ALLOWEDJOBSPERCLUSTER, mconf_get_max_jobs());
      answer_list_add(alpp, SGE_EVENT, STATUS_NOTOK_DOAGAIN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_NOTOK_DOAGAIN);
   }

   if (lGetUlong(jep, JB_verify_suitable_queues) != JUST_VERIFY &&
       lGetUlong(jep, JB_verify_suitable_queues) != POKE_VERIFY) {
      if (suser_check_new_job(jep, mconf_get_max_u_jobs(), master_suser_list) != 0) {
         INFO(MSG_JOB_ALLOWEDJOBSPERUSER_UU, mconf_get_max_u_jobs(), suser_job_count(jep, master_suser_list));
         answer_list_add(alpp, SGE_EVENT, STATUS_NOTOK_DOAGAIN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_NOTOK_DOAGAIN);
      }
   }

   {
      lList *user_lists = mconf_get_user_lists();
      lList *xuser_lists = mconf_get_xuser_lists();
      lList *grp_list = grp_list_array2list(packet->amount, packet->grp_array);

      if (!sge_has_access_(packet->user, packet->group, grp_list, user_lists, xuser_lists, master_userset_list)) {
         ERROR(MSG_JOB_NOPERMS_SS, packet->user, packet->host);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         lFreeList(&user_lists);
         lFreeList(&xuser_lists);
         lFreeList(&grp_list);
         DRETURN(STATUS_EUNKNOWN);
      }
      lFreeList(&user_lists);
      lFreeList(&xuser_lists);
      lFreeList(&grp_list);
   }

   /*
    * here we test (if requested) the parallel environment exists.
    * if not the job is refused
    */
   {
      const char *pe_name = nullptr;
      lList *pe_range = nullptr;

      pe_name = lGetString(jep, JB_pe);
      if (pe_name) {
         const lListElem *pep;

         pep = pe_list_find_matching(master_pe_list, pe_name);
         if (!pep) {
            ERROR(MSG_JOB_PEUNKNOWN_S, pe_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
         /* check pe_range */
         pe_range = lGetListRW(jep, JB_pe_range);
         if (object_verify_pe_range(alpp, pe_name, pe_range, SGE_OBJ_JOB) != STATUS_OK) {
            DRETURN(STATUS_EUNKNOWN);
         }
      }
   }

   {
      u_long32 ckpt_attr = lGetUlong(jep, JB_checkpoint_attr);
      u_long32 ckpt_inter = lGetUlong(jep, JB_checkpoint_interval);
      const char *ckpt_name = lGetString(jep, JB_checkpoint_name);
      lListElem *ckpt_ep;
      int ckpt_err = 0;

      /* request for non-existing ckpt object will be refused */
      if ((ckpt_name != nullptr)) {
         if (!(ckpt_ep = ckpt_list_locate(master_ckpt_list, ckpt_name)))
            ckpt_err = 1;
         else if (!ckpt_attr) {
            ckpt_attr = sge_parse_checkpoint_attr(lGetString(ckpt_ep, CK_when));
            lSetUlong(jep, JB_checkpoint_attr, ckpt_attr);
         }
      }

      if (!ckpt_err) {
         if ((ckpt_attr & NO_CHECKPOINT) && (ckpt_attr & ~NO_CHECKPOINT)) {
            ckpt_err = 2;
         } else if (ckpt_name && (ckpt_attr & NO_CHECKPOINT)) {
            ckpt_err = 3;
         } else if ((!ckpt_name && (ckpt_attr & ~NO_CHECKPOINT))) {
            ckpt_err = 4;
         } else if (!ckpt_name && ckpt_inter) {
            ckpt_err = 5;
         }
      }

      if (ckpt_err) {
         switch (ckpt_err) {
            case 1:
               ERROR(MSG_JOB_CKPTUNKNOWN_S, ckpt_name);
               break;
            case 2:
            case 3:
               ERROR("%s", MSG_JOB_CKPTMINUSC);
               break;
            case 4:
            case 5:
               ERROR("%s", MSG_JOB_CKPTDENIED);
               break;
            default:
               ERROR("%s", MSG_JOB_CKPTDENIED);
               break;
         }
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   /* first check user permissions */
   {
      const lListElem *cqueue = nullptr;
      int has_permissions = 0;
      lList *grp_list = grp_list_array2list(packet->amount, packet->grp_array);

      for_each_ep(cqueue, master_cqueue_list) {
         const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
         const lListElem *qinstance = nullptr;

         for_each_ep(qinstance, qinstance_list) {
            if (sge_has_access(packet->user, packet->group, grp_list, qinstance, master_userset_list)) {
               DPRINTF("job has access to queue " SFQ "\n", lGetString(qinstance, QU_qname));
               has_permissions = 1;
               break;
            }
         }
         if (has_permissions == 1) {
            break;
         }
      }
      lFreeList(&grp_list);
      if (has_permissions == 0) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_JOB_NOTINANYQ_S, packet->user);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      }
   }

   /* if enforce_user flag is "auto", add or update the user */
   {
      char *enforce_user = mconf_get_enforce_user();

      if (enforce_user && !strcasecmp(enforce_user, "auto")) {
         int status = sge_add_auto_user(packet, task, packet->user, alpp, monitor);

         if (status != STATUS_OK) {
            sge_free(&enforce_user);
            DRETURN(status);
         }
      }

      /* ensure user exists if enforce_user flag is set */
      if (enforce_user && !strcasecmp(enforce_user, "true") &&
          !user_list_locate(master_user_list, packet->user)) {
         ERROR(MSG_JOB_USRUNKNOWN_S, packet->user);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         sge_free(&enforce_user);
         DRETURN(STATUS_EUNKNOWN);
      }
      sge_free(&enforce_user);
   }

   /* set default project */
   if (!lGetString(jep, JB_project) && master_user_list) {
      lListElem *uep = nullptr;
      if ((uep = user_list_locate(master_user_list, packet->user))) {
         lSetString(jep, JB_project, lGetString(uep, UU_default_project));
      }
   }

   /* project */
   {
      int local_ret = job_verify_project(jep, alpp, packet->user, packet->group, lGetList(jep, JB_grp_list));
      if (local_ret != STATUS_OK) {
         DRETURN(local_ret);
      }
   }

   // check the given department or find a department for the user if none was specified
   if (const char *dept_name = lGetString(jep, JB_department); dept_name != nullptr) {
      if (!job_is_valid_department(jep, alpp, dept_name, master_userset_list)) {
         DRETURN(STATUS_EUNKNOWN);
      }
   } else {
      if (!job_set_department(jep, alpp, master_userset_list)) {
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* 
    * If it is a deadline job the user has to be a deadline user
    */
   if (lGetUlong64(jep, JB_deadline) > 0) {
      if (!user_is_deadline_user(packet, master_userset_list)) {
         ERROR(MSG_JOB_NODEADLINEUSER_S, packet->user);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* Verify existence of ar, if ar exists */
   {
      u_long32 ar_id = lGetUlong(jep, JB_ar);

      if (ar_id != 0) {
         lListElem *ar;
         u_long64 ar_start_time, ar_end_time, job_duration;
         u_long64 now_time, job_execution_time;

         DPRINTF("job -ar " sge_u32"\n", ar_id);

         ar = ar_list_locate(master_ar_list, ar_id);
         if (ar == nullptr) {
            ERROR(MSG_JOB_NOAREXISTS_U, ar_id);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         } else if ((lGetUlong(ar, AR_state) == AR_DELETED) ||
                    (lGetUlong(ar, AR_state) == AR_EXITED)) {
            ERROR(MSG_JOB_ARNOLONGERAVAILABE_U, ar_id);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
         /* fill the job and ar values */
         ar_start_time = lGetUlong64(ar, AR_start_time);
         ar_end_time = lGetUlong64(ar, AR_end_time);
         now_time = sge_get_gmt64();
         job_execution_time = lGetUlong64(jep, JB_execution_time);

         /* execution before now is set to at least now */
         if (job_execution_time < now_time) {
            job_execution_time = now_time;
         }

         /* to be sure the execution time is NOT before AR start time */
         if (job_execution_time < ar_start_time) {
            job_execution_time = ar_start_time;
         }

         /* hard_resources h_rt limit */
         if (job_get_wallclock_limit(&job_duration, jep)) {
            DPRINTF("job -ar " sge_u64", ar_start_time " sge_u64", ar_end_time " sge_u64
                    ", job_execution_time " sge_u64", job duration " sge_u64" \n",
                    ar_id, ar_start_time, ar_end_time,
                    job_execution_time, job_duration);

            /* fit the timeframe */
            if (job_duration > (ar_end_time - ar_start_time)) {
               ERROR(MSG_JOB_HRTLIMITTOOLONG_U, ar_id);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_DENIED);
            }
            if ((job_execution_time + job_duration) > ar_end_time) {
               ERROR(MSG_JOB_HRTLIMITOVEREND_U, ar_id);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_DENIED);
            }
         }
      }
   }

   /* verify schedulability */
   {
      int ret = verify_suitable_queues(alpp, jep, nullptr, false);
      if (lGetUlong(jep, JB_verify_suitable_queues) == JUST_VERIFY ||
          lGetUlong(jep, JB_verify_suitable_queues) == POKE_VERIFY || ret != 0) {
         DRETURN(ret);
      }
   }

   /*
    * only operators and managers are allowed to submit
    * jobs with higher priority than 0 (=BASE_PRIORITY)
    */
   if (lGetUlong(jep, JB_priority) > BASE_PRIORITY &&
       !manop_is_operator(packet, master_manager_list, master_operator_list)) {
      ERROR(SFNMAX, MSG_JOB_NONADMINPRIO);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* checks on -hold_jid */
   if (job_verify_predecessors(jep, alpp)) {
      DRETURN(STATUS_EUNKNOWN);
   }

   /* checks on -hold_jid_ad */
   if (job_verify_predecessors_ad(jep, alpp, packet->gdi_session)) {
      DRETURN(STATUS_EUNKNOWN);
   }

   /*
   ** security hook
   **
   ** Execute command to store the client's DCE or Kerberos credentials.
   ** This also creates a forwardable credential for the user.
   */
   if (mconf_get_do_credentials()) {
      const char *sge_root = bootstrap_get_sge_root();

      if (store_sec_cred(sge_root, jep, mconf_get_do_authentication(), alpp) != 0) {
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   job_suc_pre(jep);

   job_suc_pre_ad(jep);

   DRETURN(ret);
}
