/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

#include <fnmatch.h>

#include "uti/sge.h"
#include "uti/ocs_Pattern.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_parse_num_par.h"

#include "sgeobj/cull/sge_qinstance_QU_L.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_range.h"

#include "sched/sge_select_queue.h"

#include "ocs_QStatModelBase.h"

#include "msg_clients_common.h"
#include "ocs_client_cqueue.h"
#include "msg_qstat.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_qref.h"

ocs::QStatModelBase::~QStatModelBase() {
   lFreeList(&queue_list_);
   lFreeList(&centry_list_);
   lFreeList(&exechost_list_);
   lFreeList(&schedd_config_);
   lFreeList(&pe_list_);
   lFreeList(&ckpt_list_);
   lFreeList(&acl_list_);
   lFreeList(&job_list_);
   lFreeList(&hgrp_list_);
   lFreeList(&project_list_);
}

void ocs::QStatModelBase::apply_state_filter(QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (parameter.state_filter_) {
      DPRINTF("state filter string: %s\n", parameter.state_filter_value_.c_str());

      if (!parameter.state_filter_value_.empty()) {
         /*
          * list of options for the -s switch
          * when you add options, make sure that single byte options (e.g. "h")
          * come after multi byte options starting with the same character (e.g. "hs")!
          */
         static const char* flags[] = {
            "hu", "hs", "ho", "hd", "hj", "ha", "h", "p", "r", "s", "a", nullptr
         };
         static uint32_t bits[] = {
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
            (QSTAT_DISPLAY_PENDING|QSTAT_DISPLAY_RUNNING|QSTAT_DISPLAY_SUSPENDED),
            0
         };

         /* initialize bitmask */
         uint32_t rm_bits = 0;
         for (int i =0 ; flags[i] != 0; i++) {
            rm_bits |= bits[i];
         }
         parameter.show_ &= ~rm_bits;

         /*
          * search each 'flag' in argstr
          * if we find the whole string we will set the corresponding
          * bits in '*qstat_env->full_listing'
          */
         const char *s = parameter.state_filter_value_.c_str();
         while (*s != '\0') {
            for (int i = 0; flags[i] != nullptr; i++) {
               if (strncmp(s, flags[i], strlen(flags[i])) == 0) {
                  parameter.show_ |= bits[i];
                  s += strlen(flags[i]);
               }
            }
         }
      }
   }

   DRETURN_VOID;
}

void ocs::QStatModelBase::calc_longest_queue_length(QStatParameter &parameter) const {

   int name;
   if (parameter.output_mode_== QStatParameter::OutputMode::QSTAT_GROUP) {
      name = CQ_name;
   } else {
      name = QU_full_name;
   }

   // @todo Not available on server side
   if (const char *env = getenv("SGE_LONG_QNAMES"); env != nullptr){
      int queue_length;
      try {
         queue_length = std::stoi(env);
      } catch (std::invalid_argument &e) {
         queue_length = 30;
      } catch (std::out_of_range &e) {
         queue_length = 30;
      }
      if (queue_length == -1) {
         for_each_ep_lv(qep, queue_list_) {
            const char *queue_name = lGetString(qep, name);
            if (const int length = static_cast<int>(strlen(queue_name)); length > parameter.get_longest_queue_length()){
               queue_length = length;
            }
         }
      } else {
         if (queue_length < 10) {
            queue_length = 10;
         }
      }
      parameter.set_longest_queue_length(30);
   }
}

lEnumeration *
ocs::QStatModelBase::get_sub_job_filter(const QStatParameter &parameter) {
   lEnumeration *what_JB_Type = nullptr; // job

   lEnumeration *tmp_what = nullptr;
   constexpr int nm_JB_Type[] = {
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
      JB_ja_template,
      JB_execution_time,
      JB_request_set_list,
      JB_project,
      JB_jobshare,
      JB_checkpoint_name,
      NoName
   };
   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);
   if (parameter.show_ & QSTAT_DISPLAY_EXTENDED) {
      constexpr int nm_JB_Type_ext[] = {
         JB_department,
         JB_override_tickets,
         NoName
      };
      tmp_what = lIntVector2What(JB_Type, nm_JB_Type_ext);
      lMergeWhat(&what_JB_Type, &tmp_what);

   }
   if (parameter.show_ & QSTAT_DISPLAY_URGENCY) {
      constexpr int nm_JB_Type_urg[] = {
         JB_deadline,
         JB_nurg,
         JB_urg,
         JB_rrcontr,
         JB_dlcontr,
         JB_wtcontr,
         NoName
      };
      tmp_what = lIntVector2What(JB_Type, nm_JB_Type_urg);
      lMergeWhat(&what_JB_Type, &tmp_what);
   }
   if (parameter.show_ & QSTAT_DISPLAY_PRIORITY) {
      constexpr int nm_JB_Type_prio[] = {
         JB_nppri,
         JB_nurg,
         JB_priority,
         NoName
      };
      tmp_what = lIntVector2What(JB_Type, nm_JB_Type_prio);
      lMergeWhat(&what_JB_Type, &tmp_what);
   }
   if (parameter.show_ & QSTAT_DISPLAY_RESOURCES) {
      constexpr int nm_JB_Type_res[] = {
         JB_checkpoint_name,
         JB_request_set_list,
         JB_jid_request_list,
         JB_ja_ad_request_list,
         JB_binding,
         NoName
      };
      tmp_what = lIntVector2What(JB_Type, nm_JB_Type_res);
      lMergeWhat(&what_JB_Type, &tmp_what);
   }
   return what_JB_Type;
}

lEnumeration *
ocs::QStatModelBase::get_sub_ja_task_template_filter(const QStatParameter &parameter) {
   lEnumeration *what_JAT_Type_template = nullptr; // task template
   constexpr int nm_JAT_Type_template[] = {
      JAT_task_number,
      JAT_prio,
      JAT_hold,
      JAT_state,
      JAT_status,
      JAT_job_restarted,
      JAT_start_time,
      NoName
   };
   lEnumeration *tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template);
   lMergeWhat(&what_JAT_Type_template, &tmp_what);
   if (parameter.show_ & QSTAT_DISPLAY_EXTENDED) {
      constexpr int nm_JAT_Type_template_ext[] = {
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
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template_ext);
      lMergeWhat(&what_JAT_Type_template, &tmp_what);
   }
   if (parameter.show_ & QSTAT_DISPLAY_PRIORITY) {
      constexpr int nm_JAT_Type_template_prio[] = {
         JAT_ntix,
         NoName
      };
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template_prio);
      lMergeWhat(&what_JAT_Type_template, &tmp_what);
   }
   if (parameter.show_ & QSTAT_DISPLAY_RESOURCES) {
      constexpr int nm_JAT_Type_template_res[] = {
         JAT_granted_pe,
         NoName
      };
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template_res);
      lMergeWhat(&what_JAT_Type_template, &tmp_what);
   }
   if (parameter.show_ & QSTAT_DISPLAY_TASKS) {
      constexpr int nm_JAT_Type_template_task[] = {
         JAT_task_list,
         JAT_usage_list,
         JAT_scaled_usage_list,
         NoName
      };
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template_task);
      lMergeWhat(&what_JAT_Type_template, &tmp_what);
   }
   return what_JAT_Type_template;
}

lEnumeration *
ocs::QStatModelBase::get_sub_ja_task_filter(const QStatParameter &parameter) {
   lEnumeration *what_JAT_Type_list = nullptr; // task
   constexpr int nm_JAT_Type_list[] = {
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
   lEnumeration *tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_list);
   lMergeWhat(&what_JAT_Type_list, &tmp_what);
   if (parameter.show_ & QSTAT_DISPLAY_EXTENDED) {
      constexpr int nm_JAT_Type_list_ext[] = {
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
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_list_ext);
      lMergeWhat(&what_JAT_Type_list, &tmp_what);
   }
   if (parameter.show_ & QSTAT_DISPLAY_PRIORITY) {
      constexpr int nm_JAT_Type_list_prio[] = {
         JAT_ntix,
         NoName
      };
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_list_prio);
      lMergeWhat(&what_JAT_Type_list, &tmp_what);
   }
   if (parameter.show_ & QSTAT_DISPLAY_TASKS) {
      constexpr int nm_JAT_Type_list_task[] = {
         JAT_task_list,
         JAT_usage_list,
         JAT_scaled_usage_list,
         NoName
      };
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_list_task);
      lMergeWhat(&what_JAT_Type_list, &tmp_what);
   }
   return what_JAT_Type_list;
}

lEnumeration *
ocs::QStatModelBase::get_job_what(const QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   // job attribute filter
   lEnumeration *job_what = get_sub_job_filter(parameter);

   // add array task filter for enrolled tasks as sub filter
   lEnumeration *task_what = get_sub_ja_task_filter(parameter);
   if (task_what != nullptr) {
      lWhatSetSubWhat(job_what, JB_ja_tasks, &task_what);
   }

   // add array task template filter as sub filter
   lEnumeration *task_template_what = get_sub_ja_task_template_filter(parameter);
   if (task_template_what != nullptr) {
      lWhatSetSubWhat(job_what, JB_ja_template, &task_template_what);
   }

   DRETURN(job_what);
}

lCondition *
ocs::QStatModelBase::get_job_where(const QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   lCondition *jw = nullptr;
   lCondition *nw = nullptr;

   // Retrieve jobs only for those users specified via -u switch
   for_each_ep_lv(ep, parameter.get_user_list()) {
      lCondition *tmp_nw = nullptr;

      if (const char *user = lGetString(ep, ST_name); is_pattern(user)) {
         tmp_nw = lWhere("%T(%I p= %s)", JB_Type, JB_owner, user);
      } else {
         tmp_nw = lWhere("%T(%I == %s)", JB_Type, JB_owner, user);
      }
      if (jw == nullptr) {
         jw = tmp_nw;
      } else {
         jw = lOrWhere(jw, tmp_nw);
      }
   }

   // Pending jobs (all that are not running)
   if ((parameter.show_ & QSTAT_DISPLAY_PENDING) == QSTAT_DISPLAY_PENDING) {
      constexpr uint32_t all_pending_flags = (QSTAT_DISPLAY_USERHOLD|QSTAT_DISPLAY_OPERATORHOLD|
                                              QSTAT_DISPLAY_SYSTEMHOLD|QSTAT_DISPLAY_JOBARRAYHOLD|QSTAT_DISPLAY_JOBHOLD|
                                              QSTAT_DISPLAY_STARTTIMEHOLD|QSTAT_DISPLAY_PEND_REMAIN);
      // Fine grained stated selection for pending jobs or simply all pending jobs
      if ((parameter.show_ & all_pending_flags) == all_pending_flags || (parameter.show_ & all_pending_flags) == 0) {
         /*
          * All jobs not running (= all pending)
          */
         lCondition *tmp_nw = lWhere("%T(!(%I -> %T((%I m= %u))))", JB_Type, JB_ja_tasks, JAT_Type, JAT_status, JRUNNING);
         if (nw == nullptr) {
            nw = tmp_nw;
         } else {
            nw = lOrWhere(nw, tmp_nw);
         }
         /*
          * Array Jobs with one or more tasks pending
          */
         tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_n_h_ids,
                     RN_Type, RN_min, 0);
         if (nw == nullptr) {
            nw = tmp_nw;
         } else {
            nw = lOrWhere(nw, tmp_nw);
         }
         tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_u_h_ids,
                     RN_Type, RN_min, 0);
         if (nw == nullptr) {
            nw = tmp_nw;
         } else {
            nw = lOrWhere(nw, tmp_nw);
         }
         tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_s_h_ids,
                     RN_Type, RN_min, 0);
         if (nw == nullptr) {
            nw = tmp_nw;
         } else {
            nw = lOrWhere(nw, tmp_nw);
         }
         tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_o_h_ids,
                     RN_Type, RN_min, 0);
         if (nw == nullptr) {
            nw = tmp_nw;
         } else {
            nw = lOrWhere(nw, tmp_nw);
         }
      } else {
         // User Hold
         if ((parameter.show_ & QSTAT_DISPLAY_USERHOLD) == QSTAT_DISPLAY_USERHOLD) {
            /* unenrolled jobs in user hold state ... */
            lCondition *tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_u_h_ids, RN_Type, RN_min, 0);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
            /* ... or enrolled jobs with an user  hold */
            tmp_nw = lWhere("%T((%I -> %T(%I m= %u)))", JB_Type,
                            JB_ja_tasks, JAT_Type, JAT_hold, MINUS_H_TGT_USER);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
         }
         // Operator Hold
         if ((parameter.show_ & QSTAT_DISPLAY_OPERATORHOLD) == QSTAT_DISPLAY_OPERATORHOLD) {
            lCondition *tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_o_h_ids, RN_Type, RN_min, 0);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
            tmp_nw = lWhere("%T((%I -> %T(%I m= %u)))", JB_Type,
                            JB_ja_tasks, JAT_Type, JAT_hold, MINUS_H_TGT_OPERATOR);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
         }
         // System Hold
         if ((parameter.show_ & QSTAT_DISPLAY_SYSTEMHOLD) == QSTAT_DISPLAY_SYSTEMHOLD) {
            lCondition *tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_s_h_ids, RN_Type, RN_min, 0);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
            tmp_nw = lWhere("%T((%I -> %T(%I m= %u)))", JB_Type,
                            JB_ja_tasks, JAT_Type, JAT_hold, MINUS_H_TGT_SYSTEM);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
         }
         // Job Array Dependency Hold
         if ((parameter.show_ & QSTAT_DISPLAY_JOBARRAYHOLD) == QSTAT_DISPLAY_JOBARRAYHOLD) {
            lCondition *tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_a_h_ids, RN_Type, RN_min, 0);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
            tmp_nw = lWhere("%T((%I -> %T(%I m= %u)))", JB_Type,
                            JB_ja_tasks, JAT_Type, JAT_hold, MINUS_H_TGT_JA_AD);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
         }
         /*
          * Start Time Hold
          */
         if ((parameter.show_ & QSTAT_DISPLAY_STARTTIMEHOLD) == QSTAT_DISPLAY_STARTTIMEHOLD) {
            lCondition *tmp_nw = lWhere("%T(%I > %lu)", JB_Type, JB_execution_time, 0);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
         }
         /*
          * Job Dependency Hold
          */
         if ((parameter.show_ & QSTAT_DISPLAY_JOBHOLD) == QSTAT_DISPLAY_JOBHOLD) {
            lCondition *tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_jid_predecessor_list, JRE_Type, JRE_job_number, 0);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
         }
         /*
          * Rescheduled and jobs in error state (not in hold/no start time/no dependency)
          * and regular pending jobs
          */
         if ((parameter.show_ & QSTAT_DISPLAY_PEND_REMAIN) == QSTAT_DISPLAY_PEND_REMAIN) {
            lCondition *tmp_nw = lWhere("%T(%I -> %T((%I != %u)))", JB_Type, JB_ja_tasks, JAT_Type, JAT_job_restarted, 0);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
            tmp_nw = lWhere("%T(%I -> %T((%I m= %u)))", JB_Type, JB_ja_tasks, JAT_Type, JAT_state, JERROR);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
            tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_n_h_ids, RN_Type, RN_min, 0);
            if (nw == nullptr) {
               nw = tmp_nw;
            } else {
               nw = lOrWhere(nw, tmp_nw);
            }
         }
      }
   }

   /*
    * Running jobs (which are not suspended)
    *
    * NOTE:
    *    This code is not quite correct. It select jobs
    *    which are running and not suspended (qmod -s)
    *
    *    Jobs which are suspended due to other mechanisms
    *    (suspend on subordinate, thresholds, calendar)
    *    should be rejected too, but this is not possible
    *    because this information is not stored within
    *    job or job array task.
    *
    *    As a result to many jobs will be requested by qsub.
    */
   if ((parameter.show_ & QSTAT_DISPLAY_RUNNING) == QSTAT_DISPLAY_RUNNING) {
      lCondition *tmp_nw = lWhere("%T(((%I -> %T(%I m= %u)) || (%I -> %T(%I m= %u))) && !(%I -> %T((%I m= %u))))", JB_Type,
                      JB_ja_tasks, JAT_Type, JAT_status, JRUNNING,
                      JB_ja_tasks, JAT_Type, JAT_status, JTRANSFERING,
                      JB_ja_tasks, JAT_Type, JAT_state, JSUSPENDED);
      if (nw == nullptr) {
         nw = tmp_nw;
      } else {
         nw = lOrWhere(nw, tmp_nw);
      }
   }

   // Suspended jobs
   if ((parameter.show_ & QSTAT_DISPLAY_SUSPENDED) == QSTAT_DISPLAY_SUSPENDED) {
      lCondition *tmp_nw = lWhere("%T((%I -> %T(%I m= %u)) || (%I -> %T(%I m= %u)) || (%I -> %T(%I m= %u)))", JB_Type,
                      JB_ja_tasks, JAT_Type, JAT_status, JRUNNING,
                      JB_ja_tasks, JAT_Type, JAT_status, JTRANSFERING,
                      JB_ja_tasks, JAT_Type, JAT_state, JSUSPENDED);
      if (nw == nullptr) {
         nw = tmp_nw;
      } else {
         nw = lOrWhere(nw, tmp_nw);
      }
   }

   if (nw != nullptr) {
      if (jw == nullptr) {
         jw = nw;
      } else {
         jw = lAndWhere(jw, nw);
      }
   }

   DRETURN(jw);
}

lEnumeration *ocs::QStatModelBase::get_queue_what() {
   return lWhat("%T(ALL)", CQ_Type);
}

lEnumeration *ocs::QStatModelBase::get_centry_what() {
   return lWhat("%T(ALL)", CE_Type);
}

lCondition *ocs::QStatModelBase::get_ehost_where() {
   return lWhere("%T(%I!=%s)", EH_Type, EH_name, SGE_TEMPLATE_NAME);
}

lEnumeration *ocs::QStatModelBase::get_ehost_what() {
   return lWhat("%T(ALL)", EH_Type);
}

lEnumeration *ocs::QStatModelBase::get_pe_what() {
   return lWhat("%T(%I%I%I%I%I)", PE_Type, PE_name, PE_slots, PE_job_is_first_task, PE_control_slaves, PE_urgency_slots);
}

lEnumeration *ocs::QStatModelBase::get_ckpt_what() {
   return lWhat("%T(%I)", CK_Type, CK_name);
}

lEnumeration *ocs::QStatModelBase::get_uset_what() {
   return lWhat("%T(ALL)", US_Type);
}

lEnumeration *ocs::QStatModelBase::get_prj_what() {
   return lWhat("%T(ALL)", PR_Type);
}

lEnumeration *ocs::QStatModelBase::get_sconf_what() {
   return lWhat("%T(ALL)", SC_Type);
}

lEnumeration *ocs::QStatModelBase::get_hgrp_what() {
   return lWhat("%T(ALL)", HGRP_Type);
}

lCondition *ocs::QStatModelBase::get_conf_where() {
   return lWhere("%T(%I c= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME);
}

lEnumeration *ocs::QStatModelBase::get_conf_what() {
   return lWhat("%T(ALL)", CONF_Type);
}

void ocs::QStatModelBase::prepare_filter(QStatParameter &parameter) {
   // set the full_listing flags that will influence job/task filters
   apply_state_filter(parameter);
}

bool ocs::QStatModelBase::fetch_data(lList **alpp, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   // has to be overridden by derived class
   DRETURN(true);
}

bool ocs::QStatModelBase::prepare_data(lList **alpp) {
   DENTER(TOP_LAYER);
   // has to be overridden by derived class
   DRETURN(true);
}

/*
   untag all queues not in a specific state

   returns
      0 ok
      -1 error

*/
int ocs::QStatModelBase::select_by_queue_state(uint32_t queue_states, lList *exechost_list, lList *queue_list, lList *centry_list) {
   DENTER(TOP_LAYER);

   /* only show queues in the requested state */
   /* make it possible to display any load value in qstat output */
   const char *load_avg_str = getenv("SGE_LOAD_AVG");
   if (load_avg_str == nullptr || !strlen(load_avg_str)) {
      load_avg_str = LOAD_ATTR_LOAD_AVG;
   }

   for_each_ep_lv(cqueue, queue_list) {
      for_each_rw_lv(qep, lGetList(cqueue, CQ_qinstances)) {
         bool has_value_from_object;
         double load_avg;
         uint32_t interval;

         /* compute the load and suspend alarm */
         sge_get_double_qattr(&load_avg, load_avg_str, qep, exechost_list, centry_list, &has_value_from_object);
         if (sge_load_alarm(nullptr, 0, qep, lGetList(qep, QU_load_thresholds), exechost_list, centry_list, nullptr, true)) {
            qinstance_state_set_alarm(qep, true);
         }
         parse_ulong_val(nullptr, &interval, ocs::CEntry::Type::TIME, lGetString(qep, QU_suspend_interval), nullptr, 0);
         if (lGetUlong(qep, QU_nsuspend) != 0 && interval != 0 &&
             sge_load_alarm(nullptr, 0, qep, lGetList(qep, QU_suspend_thresholds), exechost_list, centry_list, nullptr, false)) {
            qinstance_state_set_suspend_alarm(qep, true);
             }

         if (!qinstance_has_state(qep, queue_states)) {
            lSetUlong(qep, QU_tag, 0);
         }
      }
   }
   DRETURN(0);
}

int ocs::QStatModelBase::select_by_qref_list(lList *cqueue_list, const lList *hgrp_list, const lList *qref_list) {
   int ret = 0;
   lList *queueref_list = nullptr;

   DENTER(TOP_LAYER);

   /*
    * Resolve queue pattern
    */
   {
      lList *tmp_list = nullptr;
      bool found_something = true;
      queueref_list = lCopyList("", qref_list);

      qref_list_resolve(queueref_list, nullptr, &tmp_list, &found_something, cqueue_list, hgrp_list, true, true);
      if (!found_something) {
         lFreeList(&queueref_list);
         DRETURN(-1);
      }
      lFreeList(&queueref_list);
      queueref_list = tmp_list;
      tmp_list = nullptr;
   }

   if (cqueue_list != nullptr && queueref_list != nullptr) {
      for_each_ep_lv(qref, queueref_list) {
         dstring cqueue_buffer = DSTRING_INIT;
         dstring hostname_buffer = DSTRING_INIT;
         const char *full_name = nullptr;

         full_name = lGetString(qref, QR_name);

         if (cqueue_name_split(full_name, &cqueue_buffer, &hostname_buffer, nullptr, nullptr)) {
            const char *cqueue_name = sge_dstring_get_string(&cqueue_buffer);
            const char *hostname = sge_dstring_get_string(&hostname_buffer);
            const lListElem *cqueue = lGetElemStr(cqueue_list, CQ_name, cqueue_name);
            const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
            lListElem *qinstance = lGetElemHostRW(qinstance_list, QU_qhostname, hostname);

            uint32_t tag = lGetUlong(qinstance, QU_tag);
            lSetUlong(qinstance, QU_tag, tag | TAG_SELECT_IT);
         }

         sge_dstring_free(&cqueue_buffer);
         sge_dstring_free(&hostname_buffer);
      }

      for_each_ep_lv(cqueue, cqueue_list) {
         const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
         for_each_rw_lv(qinstance, qinstance_list) {
            uint32_t tag = lGetUlong(qinstance, QU_tag);
            bool selected = ((tag & TAG_SELECT_IT) != 0) ? true : false;

            if (!selected) {
               tag &= ~(TAG_SELECT_IT | TAG_SHOW_IT);
               lSetUlong(qinstance, QU_tag, tag);
            } else {
               ret++;
            }
         }
      }
   }

   lFreeList(&queueref_list);
   DRETURN(ret);
}

void ocs::QStatModelBase::filter_jobs(const QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   // select all jobs which are not finished
   for_each_rw_lv(jep, job_list_) {
      for_each_rw_lv(jatep, lGetList(jep, JB_ja_tasks)) {
         if (!(lGetUlong(jatep, JAT_status) & JFINISHED))
            lSetUlong(jatep, JAT_suitable, TAG_SHOW_IT);
      }
   }

   // untag all jobs which do not fit to the user list (-u)
   if (lGetNumberOfElem(parameter.get_user_list())) {
      for_each_rw_lv(up, parameter.get_user_list()) {
         const char *user = lGetString(up, ST_name);
         if (user == nullptr) {
            break;
         }

         const bool is_pattern = ocs::is_pattern(user);
         for_each_rw_lv(jep, job_list_) {
            const char *owner = lGetString(jep, JB_owner);

            int match;
            if (is_pattern) {
               match = fnmatch(user, owner, 0);
            } else {
               match = sge_strnullcmp(user, owner);
            }
            if (match == 0) {
               for_each_rw_lv(jatep, lGetList(jep, JB_ja_tasks)) {
                  lSetUlong(jatep, JAT_suitable, lGetUlong(jatep, JAT_suitable) | TAG_SHOW_IT | TAG_SELECT_IT);
               }
            }
         }
      }
   }

   // untag all jobs which do not fit to the queue selection (-pe -l -q -U)
   if (lGetNumberOfElem(parameter.get_pe_ref_list()) || lGetNumberOfElem(parameter.get_queue_ref_list()) ||
       lGetNumberOfElem(parameter.get_resource_list()) || lGetNumberOfElem(parameter.get_queue_user_list())) {

      // @todo Will this call cause an issue if executed in the reader thread pool
      // do not debit for running jobs
      sconf_set_qs_state(QS_STATE_EMPTY);

      // unselect all pending jobs that fit in none of the selected queues
      for_each_rw_lv(jep, job_list_) {
         bool show_job = false;
         for_each_ep_lv(cqueue, queue_list_) {
            for_each_rw_lv(qep, lGetList(cqueue, CQ_qinstances)) {

               if (!(lGetUlong(qep, QU_tag) & TAG_SHOW_IT)) {
                  continue;
               }

               // find if the queue instance is in the list of selected queues and if the job fits into this queue instance
               if (lListElem *host = host_list_locate(exechost_list_, lGetHost(qep, QU_qhostname)); host != nullptr) {
                  const int ret = sge_select_queue(job_get_hard_resource_listRW(jep), qep,
                                                   host, exechost_list_, centry_list_,true, 1,
                                                   parameter.get_queue_user_list(), acl_list_, jep);

                  if (ret==1) {
                     show_job = true;
                     break;
                  }
               }
            }
         }

         for_each_rw_lv(jatep, lGetList(jep, JB_ja_tasks)) {
            if (!show_job && !(lGetUlong(jatep, JAT_status) == JRUNNING || (lGetUlong(jatep, JAT_status) == JTRANSFERING))) {
               DPRINTF("show task " sge_u32"." sge_u32"\n", lGetUlong(jep, JB_job_number), lGetUlong(jatep, JAT_task_number));
               lSetUlong(jatep, JAT_suitable, lGetUlong(jatep, JAT_suitable) & ~TAG_SHOW_IT);
            }
         }
         if (!show_job) {
            lSetList(jep, JB_ja_n_h_ids, nullptr);
            lSetList(jep, JB_ja_u_h_ids, nullptr);
            lSetList(jep, JB_ja_o_h_ids, nullptr);
            lSetList(jep, JB_ja_s_h_ids, nullptr);
         }
      }

      // @todo Will this call cause an issue if executed in the reader thread pool
      // re-enable debit for running jobs
      sconf_set_qs_state(QS_STATE_FULL);
   }

   // prepare queues for output
   if (parameter.output_mode_== QStatParameter::OutputMode::QSTAT_GROUP) {

      // sort cluster queues for grouped output
      lPSortList(queue_list_, "%I+ ", CQ_name);
   } else {

      // cluster queues are not required. We need to reconstruct the queue list with the queue instances only.
      lList *tmp_queue_list = lCreateList("", QU_Type);
      for_each_rw_lv(cqueue, queue_list_) {
         lList *qinstances = nullptr;
         lXchgList(cqueue, CQ_qinstances, &qinstances);
         lAddList(tmp_queue_list, &qinstances);
      }
      lFreeList(&queue_list_);
      queue_list_ = tmp_queue_list;

      // sort queue instances
      lPSortList(queue_list_, "%I+ %I+ %I+", QU_seq_no, QU_qname, QU_qhostname);
   }
   DRETURN_VOID;
}

int ocs::QStatModelBase::filter_queues(lList **answer_list, const QStatParameter &parameter) const {
   DENTER(TOP_LAYER);

   centry_list_init_double(centry_list_);

   // all queues are selected
   cqueue_list_set_tag(queue_list_, TAG_SHOW_IT, true);

   // unselect all queues not selected by a -q
   if (lGetNumberOfElem(parameter.get_queue_ref_list()) > 0) {
      const int count = select_by_qref_list(queue_list_, hgrp_list_, parameter.get_queue_ref_list());

      if (count < 0) {
         DRETURN(-1);
      }

      if (count == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-q");
         DRETURN(0);
      }
   }

   // unselect all queues not selected by -qs
   select_by_queue_state(parameter.queue_state_, exechost_list_, queue_list_, centry_list_);

   // unselect all queues not selected by a -U (if exist)
   if (lGetNumberOfElem(parameter.get_queue_user_list())>0) {
      const int count = select_by_queue_user_list(exechost_list_, queue_list_, parameter.get_queue_user_list(), acl_list_, project_list_);

      if (count < 0) {
         DRETURN(-1);
      }

      if (count == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-U");
         DRETURN(0);
      }
   }

   // unselect all queues not selected by a -pe (if exist)
   if (lGetNumberOfElem(parameter.get_pe_ref_list())>0) {
      const int count = select_by_pe_list(queue_list_, parameter.get_pe_ref_list(), pe_list_);

      if (count < 0) {
         DRETURN(-1);
      }

      if (count == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-pe");
         DRETURN(0);
      }
   }

   // unselect all queues not selected by a -l
   if (lGetNumberOfElem(parameter.get_resource_list())) {
      const int count = select_by_resource_list(parameter.get_resource_list(), exechost_list_, queue_list_, centry_list_, 1);
      if (count < 0) {
         DRETURN(-1);
      }
   }

   if (!is_cqueue_selected(queue_list_)) {
      DRETURN(0);
   }

   DRETURN(1);
}

bool ocs::QStatModelBase::is_cqueue_selected(lList *queue_list) {
   DENTER(TOP_LAYER);

   bool a_qinstance_is_selected = false;
   bool a_cqueue_is_selected = false;

   for_each_rw_lv(cqueue, queue_list) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      bool tmp_a_qinstance_is_selected = false;

      for_each_ep_lv(qep, qinstance_list) {
         if (lGetUlong(qep, QU_tag) & TAG_SHOW_IT) {
            tmp_a_qinstance_is_selected = true;
            break;
         }
      }
      a_qinstance_is_selected |= tmp_a_qinstance_is_selected;
      if (!tmp_a_qinstance_is_selected && (lGetNumberOfElem(lGetList(cqueue, CQ_qinstances)) > 0)) {
         lSetUlong(cqueue, CQ_tag, TAG_DEFAULT);
      } else {
         a_cqueue_is_selected |= true;
      }
   }

   DRETURN(a_cqueue_is_selected);
}

/*
   untag all queues not covered by -l

   returns
      0  successfully untagged qinstances if necessary
     -1  error

*/
int ocs::QStatModelBase::select_by_resource_list(lList *resource_list, lList *exechost_list, lList *queue_list, lList *centry_list, uint32_t empty_qs) {
   DENTER(TOP_LAYER);

   if (centry_list_fill_request(resource_list, nullptr, centry_list, true, true, false)) {
      /*
      ** error message gets written by centry_list_fill_request into
      ** SGE_EVENT
      */
      DRETURN(-1);
   }

   /* prepare request */
   for_each_ep_lv(cqueue, queue_list) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      for_each_rw_lv(qep, qinstance_list) {
         if (empty_qs) {
            sconf_set_qs_state(QS_STATE_EMPTY);
         }

         const bool selected = sge_select_queue(resource_list, qep, nullptr, exechost_list,
                                                centry_list, true, -1, nullptr,
                                                nullptr, nullptr);
         if (empty_qs) {
            sconf_set_qs_state(QS_STATE_FULL);
         }

         if (!selected) {
            DPRINTF("unselecting queue " SFQ "\n", lGetString(qep, QU_full_name));
            lSetUlong(qep, QU_tag, 0);
         }
      }
   }

   DRETURN(0);
}

/*
   untag all queues not selected by a -pe

   returns
      0 ok
      -1 error

*/
int ocs::QStatModelBase::select_by_pe_list(lList *queue_list, lList *peref_list, lList *pe_list) {
   DENTER(TOP_LAYER);
   int nqueues = 0;
   lList *pe_selected = nullptr;

   /*
    * iterate through peref_list and build up a new pe_list
    * containing only those pe's referenced in peref_list
    */
   for_each_ep_lv(pe, peref_list) {
      lListElem *ref_pe;  /* PE_Type */
      lListElem *copy_pe; /* PE_Type */

      ref_pe = pe_list_locate(pe_list, lGetString(pe, ST_name));
      copy_pe = lCopyElem(ref_pe);
      if (pe_selected == nullptr) {
         const lDescr *descriptor = lGetElemDescr(ref_pe);

         pe_selected = lCreateList("", descriptor);
      }
      lAppendElem(pe_selected, copy_pe);
   }
   if (lGetNumberOfElem(pe_selected) == 0) {
      fprintf(stderr, "%s\n", MSG_PE_NOSUCHPARALLELENVIRONMENT);
      return -1;
   }

   /*
    * untag all non-parallel queues and queues not referenced
    * by a pe in the selected pe list entry of a queue
    */
   for_each_ep_lv(cqueue, queue_list) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      for_each_rw_lv(qep, qinstance_list) {
         const lListElem *found = nullptr;

         if (!qinstance_is_parallel_queue(qep)) {
            lSetUlong(qep, QU_tag, 0);
            continue;
         }
         for_each_ep_lv(pe, pe_selected) {
            const char *pe_name = lGetString(pe, PE_name);

            found = lGetSubStr(qep, ST_name, pe_name, QU_pe_list);
            if (found != nullptr) {
               break;
            }
         }
         if (found == nullptr) {
            lSetUlong(qep, QU_tag, 0);
         } else {
            nqueues++;
         }
      }
   }

   if (pe_selected != nullptr) {
      lFreeList(&pe_selected);
   }
   DRETURN(nqueues);
}


/*
   untag all queues not selected by a -pe

   returns
      0 ok
      -1 error

*/
int ocs::QStatModelBase::select_by_queue_user_list(lList *exechost_list, lList *cqueue_list, lList *queue_user_list, lList *acl_list, lList *project_list) {
   DENTER(TOP_LAYER);
   int nqueues = 0;
   lListElem *ehep = nullptr;
   const lList *h_acl = nullptr;
   const lList *h_xacl = nullptr;
   const lList *global_acl = nullptr;
   const lList *global_xacl = nullptr;
   lList *config_acl = nullptr;
   lList *config_xacl = nullptr;
   const lList *prj = nullptr;
   const lList *xprj = nullptr;
   const lList *h_prj = nullptr;
   const lList *h_xprj = nullptr;
   const lList *global_prj = nullptr;
   const lList *global_xprj = nullptr;

   /* untag all queues where no of the users has access */

   ehep = host_list_locate(exechost_list, "global");
   global_acl = lGetList(ehep, EH_acl);
   global_xacl = lGetList(ehep, EH_xacl);
   global_prj = lGetList(ehep, EH_prj);
   global_xprj = lGetList(ehep, EH_xprj);

   config_acl = mconf_get_user_lists();
   config_xacl = mconf_get_xuser_lists();

   for_each_ep_lv(cqueue, cqueue_list) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      for_each_rw_lv(qep, qinstance_list) {
         int access = 0;
         const char *host_name = nullptr;

         prj = lGetList(qep, QU_projects);
         xprj = lGetList(qep, QU_xprojects);

         /* get exec host list element for current queue
            and its access lists */
         host_name = lGetHost(qep, QU_qhostname);
         ehep = host_list_locate(exechost_list, host_name);
         if (ehep != nullptr) {
            h_acl = lGetList(ehep, EH_acl);
            h_xacl = lGetList(ehep, EH_xacl);
            h_prj = lGetList(ehep, EH_prj);
            h_xprj = lGetList(ehep, EH_xprj);
         }

         for_each_ep_lv(qu, queue_user_list) {
            int q_access = 0;
            int h_access = 0;
            int gh_access = 0;
            int conf_access = 0;

            const char *name = lGetString(qu, ST_name);
            if (name == nullptr)
               continue;

            DPRINTF("-----> checking queue user: %s\n", name);

            DPRINTF("testing queue access lists\n");
            q_access = (name[0] == '@') ? sge_has_access(nullptr, &name[1], nullptr, qep, acl_list)
                                        : sge_has_access(name, nullptr, nullptr, qep, acl_list);
            if (!q_access) {
               DPRINTF("no access\n");
            } else {
               DPRINTF("ok\n");
            }
            if (project_list != nullptr) {
               DPRINTF("testing queue projects lists\n");
               for_each_ep_lv(pep, prj) {
                  const char *prj_name;
                  lListElem *prj_elem;
                  if ((prj_name = lGetString(pep, PR_name)) != nullptr) {
                     if ((prj_elem = prj_list_locate(project_list, prj_name)) != nullptr) {
                        q_access &= (name[0] == '@') ? sge_has_access_(nullptr, &name[1], nullptr, lGetList(prj_elem, PR_acl),
                                                                       lGetList(prj_elem, PR_xacl), acl_list)
                                                     : sge_has_access_(name, nullptr, nullptr, lGetList(prj_elem, PR_acl),
                                                                       lGetList(prj_elem, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               for_each_ep_lv(pep, xprj) {
                  if (const char *prj_name = lGetString(pep, PR_name); prj_name != nullptr) {
                     if (lListElem *prj_elem = prj_list_locate(project_list, prj_name); prj_elem != nullptr) {
                        q_access &= (name[0] == '@') ? !sge_has_access_(nullptr, &name[1], nullptr, lGetList(prj_elem, PR_acl),
                                                                        lGetList(prj_elem, PR_xacl), acl_list)
                                                     : !sge_has_access_(name, nullptr, nullptr, lGetList(prj_elem, PR_acl),
                                                                        lGetList(prj_elem, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               if (!q_access) {
                  DPRINTF("no access\n");
               } else {
                  DPRINTF("ok\n");
               }
            }

            DPRINTF("testing host access lists\n");
            h_access = (name[0] == '@') ? sge_has_access_(nullptr, &name[1], nullptr, h_acl, h_xacl, acl_list)
                                        : sge_has_access_(name, nullptr, nullptr, h_acl, h_xacl, acl_list);
            if (!h_access) {
               DPRINTF("no access\n");
            } else {
               DPRINTF("ok\n");
            }
            if (project_list != nullptr) {
               DPRINTF("testing host projects lists\n");
               for_each_ep_lv(pep, h_prj) {
                  const char *prj_name;
                  lListElem *prj_elem;
                  if ((prj_name = lGetString(pep, PR_name)) != nullptr) {
                     if ((prj_elem = prj_list_locate(project_list, prj_name)) != nullptr) {
                        q_access &= (name[0] == '@') ? sge_has_access_(nullptr, &name[1], nullptr, lGetList(prj_elem, PR_acl),
                                                                       lGetList(prj_elem, PR_xacl), acl_list)
                                                     : sge_has_access_(name, nullptr, nullptr, lGetList(prj_elem, PR_acl),
                                                                       lGetList(prj_elem, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               for_each_ep_lv(pep, h_xprj) {
                  if (const char *prj_name = lGetString(pep, PR_name); prj_name != nullptr) {
                     if (lListElem *prj_elem = prj_list_locate(project_list, prj_name); prj_elem != nullptr) {
                        q_access &= (name[0] == '@') ? !sge_has_access_(nullptr, &name[1], nullptr, lGetList(prj_elem, PR_acl),
                                                                        lGetList(prj_elem, PR_xacl), acl_list)
                                                     : !sge_has_access_(name, nullptr, nullptr, lGetList(prj_elem, PR_acl),
                                                                        lGetList(prj_elem, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               if (!q_access) {
                  DPRINTF("no access\n");
               } else {
                  DPRINTF("ok\n");
               }
            }

            DPRINTF("testing global host access lists\n");
            gh_access = (name[0] == '@') ? sge_has_access_(nullptr, &name[1], nullptr, global_acl, global_xacl, acl_list)
                                         : sge_has_access_(name, nullptr, nullptr, global_acl, global_xacl, acl_list);
            if (!gh_access) {
               DPRINTF("no access\n");
            } else {
               DPRINTF("ok\n");
            }
            if (project_list != nullptr) {
               DPRINTF("testing host projects lists\n");
               for_each_ep_lv(pep, global_prj) {
                  if (const char *prj_name = lGetString(pep, PR_name); prj_name != nullptr) {
                     if (lListElem *prj_elem = prj_list_locate(project_list, prj_name); prj_elem != nullptr) {
                        q_access &= (name[0] == '@') ? sge_has_access_(nullptr, &name[1], nullptr, lGetList(prj_elem, PR_acl),
                                                                       lGetList(prj_elem, PR_xacl), acl_list)
                                                     : sge_has_access_(name, nullptr, nullptr, lGetList(prj_elem, PR_acl),
                                                                       lGetList(prj_elem, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               for_each_ep_lv(pep, global_xprj) {
                  const char *prj_name;
                  lListElem *prj_elem;
                  if ((prj_name = lGetString(pep, PR_name)) != nullptr) {
                     if ((prj_elem = prj_list_locate(project_list, prj_name)) != nullptr) {
                        q_access &= (name[0] == '@') ? !sge_has_access_(nullptr, &name[1], nullptr, lGetList(prj_elem, PR_acl),
                                                                        lGetList(prj_elem, PR_xacl), acl_list)
                                                     : !sge_has_access_(name, nullptr, nullptr, lGetList(prj_elem, PR_acl),
                                                                        lGetList(prj_elem, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               if (!q_access) {
                  DPRINTF("no access\n");
               } else {
                  DPRINTF("ok\n");
               }
            }

            DPRINTF("testing cluster config access lists\n");
            if (name[0] == '@') {
               conf_access = sge_has_access_(nullptr, &name[1], nullptr, config_acl, config_xacl, acl_list);
            } else {
               conf_access = sge_has_access_(name, nullptr, nullptr, config_acl, config_xacl, acl_list);
            }
            if (!conf_access) {
               DPRINTF("no access\n");
            } else {
               DPRINTF("ok\n");
            }

            access = q_access && h_access && gh_access && conf_access;
            if (!access) {
               break;
            }
         }
         if (!access) {
            DPRINTF("no access for queue %s\n", lGetString(qep, QU_qname));
            lSetUlong(qep, QU_tag, 0);
         } else {
            DPRINTF("access for queue %s\n", lGetString(qep, QU_qname));
            nqueues++;
         }
      }
   }

   lFreeList(&config_acl);
   lFreeList(&config_xacl);
   DRETURN(nqueues);
}

bool ocs::QStatModelBase::filter_data(lList **alpp, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   // filter queues according to given parameters
   if (filter_queues(alpp, parameter) < 0) {
      DPRINTF("qstat_env_filter_queues failed\n");
      DRETURN(false);
   }

   // if output mode is qselect, we do not filter the jobs
   if (parameter.output_mode_!= QStatParameter::OutputMode::QSELECT) {

      // filter jobs according to given parameters (might also unselect queues)
      filter_jobs(parameter);
   }
   DRETURN(true);
}

bool ocs::QStatModelBase::make_snapshot(lList **answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   prepare_filter(parameter);

   if (!fetch_data(answer_list, parameter)) {
      DRETURN(false);
   }

   if (!prepare_data(answer_list)) {
      DRETURN(false);
   }

   if (!filter_data(answer_list, parameter)) {
      DRETURN(false);
   }

   DRETURN(true);
}
