/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <sstream>

#include "cull/cull_list.h"

#include "uti/ocs_Pattern.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_answer.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_QStatJobModel.h"
#include "msg_qstat.h"

void ocs::QStatJobModel::free_data() {
   lFreeList(&ilp);
   lFreeList(&jlp);
}

bool ocs::QStatJobModel::fetch_data(lList **alpp, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (!gdi::Client::sge_gdi_get_permission(alpp, &is_manager_, nullptr, nullptr, nullptr)) {
      DRETURN(false);
   }

   lEnumeration* what = lWhat("%T(ALL)", SME_Type);
   *alpp = gdi::Client::sge_gdi(gdi::Target::TargetValue::SGE_SME_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, &ilp, nullptr, what);
   lFreeWhat(&what);

   if (parameter.jid_list_ != nullptr) {
      const lListElem *j_elem = nullptr;

      lCondition *where = nullptr;
      for_each_ep(j_elem, parameter.jid_list_) {
         const char *job_name = lGetString(j_elem, ST_name);
         lCondition *new_where;

         if (isdigit(job_name[0])) {
            u_long32 jid = atol(lGetString(j_elem, ST_name));
            new_where = lWhere("%T(%I==%u)", JB_Type, JB_job_number, jid);
         } else {
            new_where = lWhere("%T(%I p= %s)", JB_Type, JB_job_name, job_name);
         }
         if (new_where) {
            if (!where) {
               where = new_where;
            } else {
               where = lOrWhere(where, new_where);
            }
         }
      }
      what = lWhat("%T(%I%I%I%I"
                       "%I%I%I%I%I"
                       "%I%I%I%I"
                       "%I%I%I%I"
                       "%I%I%I"

                       "%I%I%I"
                       "%I->%T(%I%I%I"
                       "%I%I%I%I"
                       "%I%I%I"
                       "%I)%I"

                       "%I%I%I->%T"
                       "(%I)%I->%T(%I)%I"
                       "%I%I%I%I%I"
                       "%I%I%I%I"
                       "%I%I%I%I%I"

                       "%I%I%I"
                       "%I%I%I%I%I"
                       "%I%I%I%I"
                       "%I%I%I)",

               JB_Type, JB_job_number, JB_ar, JB_exec_file, JB_submission_time,
               JB_submission_command_line, JB_owner, JB_uid, JB_group, JB_gid,
               JB_account, JB_merge_stderr, JB_mail_list, JB_project,
               JB_department, JB_notify, JB_job_name, JB_stdout_path_list,
               JB_jobshare, JB_request_set_list, JB_shell_list,

               JB_env_list, JB_job_args, JB_script_file,
               JB_ja_tasks, JAT_Type, JAT_state, JAT_status, JAT_hold,
               JAT_task_number, JAT_scaled_usage_list, JAT_job_restarted, JAT_task_list,
               JAT_message_list, JAT_start_time, JAT_granted_resources_list,
               JAT_granted_destin_identifier_list, JB_context,

               JB_cwd, JB_stderr_path_list, JB_jid_predecessor_list, JRE_Type,
               JRE_job_number, JB_jid_successor_list, JRE_Type, JRE_job_number, JB_deadline,
               JB_execution_time, JB_checkpoint_name, JB_checkpoint_attr, JB_checkpoint_interval, JB_directive_prefix,
               JB_reserve, JB_mail_options, JB_stdin_path_list, JB_priority,
               JB_restart, JB_verify, JB_script_size, JB_pe, JB_pe_range,

               JB_jid_request_list, JB_ja_ad_request_list, JB_verify_suitable_queues,
               JB_soft_wallclock_gmt, JB_hard_wallclock_gmt, JB_override_tickets, JB_version, JB_ja_structure,
               JB_type, JB_binding, JB_ja_task_concurrency, JB_pty,
               JB_grp_list, JB_sync_options, JB_category_id);

      /* get job list */
      *alpp = gdi::Client::sge_gdi(gdi::Target::TargetValue::SGE_JB_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, &jlp, where, what);
      lFreeWhere(&where);
      lFreeWhat(&what);
   }

   DRETURN(true);
}

bool ocs::QStatJobModel::prepare_data(lList **alpp, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   // report an error if response does not contain all information
   if (lGetNumberOfElem(jlp) == 0 && lGetNumberOfElem(parameter.jid_list_) != 0) {

      // remove all pattern
      bool removed_pattern = false;
      lListElem *elem1;
      lListElem *elem2 = lFirstRW(parameter.jid_list_);
      while ((elem1 = elem2) != nullptr) {
         elem2 = lNextRW(elem1);

         if (is_pattern(lGetString(elem1, ST_name))) {
            lDechainElem(parameter.jid_list_, elem1);
            removed_pattern = true;
         }
      }

      // if there is still something missing then report an error
      std::stringstream ss;
      if (lGetNumberOfElem(parameter.jid_list_) > 0) {
         bool first_time = true;
         ss << MSG_QSTAT_FOLLOWINGDONOTEXIST;
         for_each_rw(elem1, parameter.jid_list_) {
            if (!first_time) {
               ss << ", ";
            }
            first_time = false;
            ss << lGetString(elem1, ST_name);
         }
      } else {
         if (removed_pattern) {
            ss << MSG_QSTAT_FOLLOWINGDONOTEXIST;
         }
      }
      answer_list_add(alpp, ss.str().c_str(), STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if (parameter.output_format_ != QStatParameter::OutputFormat::XML) {
      DRETURN(true);
   }

   /* filter the message list to contain only jobs that have been requested.
      First remove all entries in the job_number_list that are not in the
      jbList. Then remove all entries (job_number_list, message_number and
      message) from the message_list that have no jobs in them.
   */
   const lListElem *tmpElem;
   for_each_ep(tmpElem, ilp) {
      lList *msgList = lGetListRW(tmpElem, SME_message_list);
      lListElem *msgElem = lFirstRW(msgList);
      while (msgElem) {

         lListElem *tmp_msgElem = lNextRW(msgElem);
         lList *jbList = lGetListRW(msgElem, MES_job_number_list);
         lListElem *jbElem = lFirstRW(jbList);

         while (jbElem) {
            lListElem *tmp_jbElem = lNextRW(jbElem);
            if (lGetElemUlong(jlp, JB_job_number, lGetUlong(jbElem, ULNG_value)) == nullptr) {
               lRemoveElem(jbList, &jbElem);
            }
            jbElem = tmp_jbElem;
         }
         if (lGetNumberOfElem(lGetList(msgElem, MES_job_number_list)) == 0) {
            lRemoveElem(msgList, &msgElem);
         }
         msgElem = tmp_msgElem;
      }
   }
   DRETURN(true);
}

bool ocs::QStatJobModel::make_snapshot(lList **answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (!fetch_data(answer_list, parameter)) {
      DRETURN(false);
   }
   if (!prepare_data(answer_list, parameter)) {
      DRETURN(false);
   }

   DRETURN(true);
}