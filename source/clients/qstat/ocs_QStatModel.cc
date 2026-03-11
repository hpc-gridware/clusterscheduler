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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_log.h"

#include "cull/cull_what.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_conf.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_QStatModel.h"
#include "ocs_QStatParameter.h"

#include "msg_qstat.h"
#include "msg_clients_common.h"

void
ocs::QStatModel::qstat_filter_add_core_attributes(ocs::QStatParameter &parameter) {
   lEnumeration *tmp_what = nullptr;
   const int nm_JB_Type[] = {
      JB_job_number,
      JB_owner,
      JB_type,
      JB_pe,
      JB_jid_predecessor_list,
      JB_ja_ad_predecessor_list,
      JB_job_name,
      JB_submission_time,
      JB_pe_range,
      JB_ja_structure,
      JB_ja_tasks,
      JB_ja_n_h_ids,
      JB_ja_u_h_ids,
      JB_ja_o_h_ids,
      JB_ja_s_h_ids,
      JB_ja_a_h_ids,
      JB_ja_z_ids,
      JB_ja_template,
      JB_execution_time,
      JB_request_set_list,
      JB_project,
      JB_jobshare,
      NoName
   };
   const int nm_JAT_Type_template[] = {
      JAT_task_number,
      JAT_prio,
      JAT_hold,
      JAT_state,
      JAT_status,
      JAT_job_restarted,
      JAT_start_time,
      NoName
   };
   const int nm_JAT_Type_list[] = {
      JAT_task_number,
      JAT_status,
      JAT_granted_destin_identifier_list,
      JAT_suitable,
      JAT_granted_pe,
      JAT_state,
      JAT_prio,
      JAT_hold,
      JAT_job_restarted,
      JAT_start_time,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);

   tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template);
   lMergeWhat(&what_JAT_Type_template, &tmp_what);

   tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_list);
   lMergeWhat(&what_JAT_Type_list, &tmp_what);
}

/*-------------------------------------------------------------------------*
 * NAME
 *   build_job_state_filter - set the full_listing flags in the qstat_env
 *                            according to job_state
 *
 * PARAMETER
 *  qstat_env - the qstat_env
 *  job_state - the job_state
 *  alpp      - answer list for error reporting
 *
 * RETURN
 *  0    - full_listing flags in qstat_env set
 *  else - error, reason has been reported in alpp.
 *
 * DESCRIPTION
 *-------------------------------------------------------------------------*/
int ocs::QStatModel::build_job_state_filter(lList **alpp, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   int ret = 0;

   DPRINTF("state filter string: %s\n", parameter.state_filter_value_.c_str());

   if (!parameter.state_filter_value_.empty()) {
      /*
       * list of options for the -s switch
       * when you add options, make sure that single byte options (e.g. "h")
       * come after multi byte options starting with the same character (e.g. "hs")!
       */
      static const char* flags[] = {
         "hu", "hs", "ho", "hd", "hj", "ha", "h", "p", "r", "s", "z", "a", nullptr
      };
      static u_long32 bits[] = {
         (QSTAT_DISPLAY_USERHOLD|QSTAT_DISPLAY_PENDING),
         (QSTAT_DISPLAY_SYSTEMHOLD|QSTAT_DISPLAY_PENDING),
         (QSTAT_DISPLAY_OPERATORHOLD|QSTAT_DISPLAY_PENDING),
         (QSTAT_DISPLAY_JOBARRAYHOLD|QSTAT_DISPLAY_PENDING),
         (QSTAT_DISPLAY_JOBHOLD|QSTAT_DISPLAY_PENDING),
         (QSTAT_DISPLAY_STARTTIMEHOLD|QSTAT_DISPLAY_PENDING),
         (QSTAT_DISPLAY_HOLD|QSTAT_DISPLAY_PENDING),
         QSTAT_DISPLAY_PENDING,
         QSTAT_DISPLAY_RUNNING,
         QSTAT_DISPLAY_SUSPENDED,
         QSTAT_DISPLAY_ZOMBIES,
         (QSTAT_DISPLAY_PENDING|QSTAT_DISPLAY_RUNNING|QSTAT_DISPLAY_SUSPENDED),
         0
      };
      int i;
      const char *s;
      u_long32 rm_bits = 0;

      /* initialize bitmask */
      for (i =0 ; flags[i] != 0; i++) {
         rm_bits |= bits[i];
      }
      parameter.full_listing_ &= ~rm_bits;

      /*
       * search each 'flag' in argstr
       * if we find the whole string we will set the corresponding
       * bits in '*qstat_env->full_listing'
       */
      s = parameter.state_filter_value_.c_str();
      while (*s != '\0') {
         bool matched = false;
         for (i = 0; flags[i] != nullptr; i++) {
            if (strncmp(s, flags[i], strlen(flags[i])) == 0) {
               parameter.full_listing_ |= bits[i];
               s += strlen(flags[i]);
               matched = true;
            }
         }

         if (!matched) {
            answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    "%s", MSG_OPTIONS_WRONGARGUMENTTOSOPT);
            ret = -1;
            break;
         }
      }
   }

   DRETURN(ret);
}

void
ocs::QStatModel::qstat_filter_add_l_attributes() {
   lEnumeration *tmp_what = nullptr;
   const int nm_JB_Type[] = {
      JB_checkpoint_name,
      JB_request_set_list,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);
}

void
ocs::QStatModel::qstat_filter_add_ext_attributes() {
   lEnumeration *tmp_what = nullptr;
   const int nm_JB_Type[] = {
      JB_department,
      JB_override_tickets,
      NoName
   };
   const int nm_JAT_Type_template[] = {
      JAT_ntix,
      JAT_scaled_usage_list,
      JAT_granted_pe,
      JAT_tix,
      JAT_oticket,
      JAT_fticket,
      JAT_sticket,
      JAT_share,
      NoName
   };
   const int nm_JAT_Type_list[] = {
      JAT_ntix,
      JAT_scaled_usage_list,
      JAT_task_list,
      JAT_tix,
      JAT_oticket,
      JAT_fticket,
      JAT_sticket,
      JAT_share,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);

   tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template);
   lMergeWhat(&what_JAT_Type_template, &tmp_what);

   tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_list);
   lMergeWhat(&what_JAT_Type_list, &tmp_what);
}

void
ocs::QStatModel::qstat_filter_add_urg_attributes() {
   lEnumeration *tmp_what = nullptr;
   const int nm_JB_Type[] = {
      JB_deadline,
      JB_nurg,
      JB_urg,
      JB_rrcontr,
      JB_dlcontr,
      JB_wtcontr,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);
}

void
ocs::QStatModel::qstat_filter_add_pri_attributes() {
   lEnumeration *tmp_what = nullptr;
   const int nm_JB_Type[] = {
      JB_nppri,
      JB_nurg,
      JB_priority,
      NoName
   };
   const int nm_JAT_Type_template[] = {
      JAT_ntix,
      NoName
   };
   const int nm_JAT_Type_list[] = {
      JAT_ntix,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);
   tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template);
   lMergeWhat(&what_JAT_Type_template, &tmp_what);
   tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_list);
   lMergeWhat(&what_JAT_Type_list, &tmp_what);
}

void
ocs::QStatModel::qstat_filter_add_r_attributes() {
   lEnumeration *tmp_what = nullptr;
   const int nm_JB_Type[] = {
      JB_checkpoint_name,
      JB_request_set_list,
      JB_jid_request_list,
      JB_ja_ad_request_list,
      JB_binding,
      NoName
   };
   const int nm_JAT_Type_template[] = {
      JAT_granted_pe,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);

   tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template);
   lMergeWhat(&what_JAT_Type_template, &tmp_what);
}

void
ocs::QStatModel::qstat_filter_add_t_attributes() {
   lEnumeration *tmp_what = nullptr;

   const int nm_JAT_Type_list[] = {
      JAT_task_list,
      JAT_usage_list,
      JAT_scaled_usage_list,
      NoName
   };

   const int nm_JAT_Type_template[] = {
      JAT_task_list,
      JAT_usage_list,
      JAT_scaled_usage_list,
      NoName
   };

   tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_list);
   lMergeWhat(&what_JAT_Type_list, &tmp_what);

   tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template);
   lMergeWhat(&what_JAT_Type_template, &tmp_what);
}

void
ocs::QStatModel::qstat_filter_add_U_attributes()
{
   lEnumeration *tmp_what = nullptr;

   const int nm_JB_Type[] = {
      JB_checkpoint_name,
      JB_request_set_list,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);
}

void
ocs::QStatModel::qstat_filter_add_pe_attributes() {
   lEnumeration *tmp_what = nullptr;
   const int nm_JB_Type[] = {
      JB_checkpoint_name,
      JB_request_set_list,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);
}

void
ocs::QStatModel::qstat_filter_add_q_attributes() {
   lEnumeration *tmp_what = nullptr;
   const int nm_JB_Type[] = {
      JB_checkpoint_name,
      JB_request_set_list,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);
}


bool ocs::QStatModel::prepare_filter(lList **answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   qstat_filter_add_core_attributes(parameter);
   if (parameter.state_filter_) {
      if (build_job_state_filter(answer_list, parameter)) {
         //if (!usageshowed) {
         //   qstat_usage(stderr, nullptr);
         //}
         DRETURN(false);
      }
   }
   // when -l was specified
   if (1) {
      qstat_filter_add_l_attributes();
   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) {
      qstat_filter_add_ext_attributes();
   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_URGENCY) {
      qstat_filter_add_urg_attributes();
   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_PRIORITY) {
      qstat_filter_add_pri_attributes();
   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_RESOURCES) {
      qstat_filter_add_r_attributes();
   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_TASKS) {
      qstat_filter_add_t_attributes();
   }
   qstat_filter_add_U_attributes(); // -U queue_user_list
   qstat_filter_add_pe_attributes(); // -pe peref_list
   qstat_filter_add_q_attributes(); // -q queueref_list
   DRETURN(true);
}

bool ocs::QStatModel::fetch_data(lList **answer_list) {
   DENTER(TOP_LAYER);
   if (!gdi::Client::sge_gdi_get_permission(answer_list, &is_manager_, nullptr, nullptr, nullptr)) {
      DRETURN(false);
   }

   // get configuration from qmaster - from now on it is possible to use the mconf_get-functions
   lListElem *global = nullptr;
   lListElem *local = nullptr;
   lList *conf_list = nullptr;

   const char *qualified_hostname = component_get_qualified_hostname();
   u_long32 progid = component_get_component_id();
   const char *cell_root = bootstrap_get_cell_root();
   if (gdi::Client::gdi_get_configuration(qualified_hostname, &global, &local) ||
      merge_configuration(nullptr, progid, cell_root, global, local, &conf_list)) {
      ERROR(SFNMAX, MSG_CONFIG_CANTGETCONFIGURATIONFROMQMASTER);
      lFreeList(&conf_list);
      lFreeElem(&global);
      lFreeElem(&local);
      sge_exit(1);
      }
   DRETURN(true);
}

bool ocs::QStatModel::make_snapshot(lList **answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   // @todo Should be combined with the other GDI requests

   if (!prepare_filter(answer_list, parameter)) {
      DRETURN(false);
   }

   if (!fetch_data(answer_list)) {
      DRETURN(false);
   }
   DRETURN(true);
}

void ocs::QStatModel::free_data() {
   lFreeWhat(&what_JB_Type);
   lFreeWhat(&what_JAT_Type_template);
   lFreeWhat(&what_JAT_Type_list);
}