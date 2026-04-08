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
#include "ocs_client_cqueue.h"
#include "msg_qstat.h"

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
         parameter.full_listing_ &= ~rm_bits;

         /*
          * search each 'flag' in argstr
          * if we find the whole string we will set the corresponding
          * bits in '*qstat_env->full_listing'
          */
         const char *s = parameter.state_filter_value_.c_str();
         while (*s != '\0') {
            for (int i = 0; flags[i] != nullptr; i++) {
               if (strncmp(s, flags[i], strlen(flags[i])) == 0) {
                  parameter.full_listing_ |= bits[i];
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
      try {
         parameter.longest_queue_length = std::stoi(env);
      } catch (std::invalid_argument &e) {
         parameter.longest_queue_length = 30;
      } catch (std::out_of_range &e) {
         parameter.longest_queue_length = 30;
      }
      if (parameter.longest_queue_length == -1) {
         for_each_ep_lv(qep, queue_list_) {
            const char *queue_name = lGetString(qep, name);
            if (const int length = static_cast<int>(strlen(queue_name)); length > parameter.longest_queue_length){
               parameter.longest_queue_length = length;
            }
         }
      }
      else {
         if (parameter.longest_queue_length < 10) {
            parameter.longest_queue_length = 10;
         }
      }
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
   if (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) {
      constexpr int nm_JB_Type_ext[] = {
         JB_department,
         JB_override_tickets,
         NoName
      };
      tmp_what = lIntVector2What(JB_Type, nm_JB_Type_ext);
      lMergeWhat(&what_JB_Type, &tmp_what);

   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_URGENCY) {
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
   if (parameter.full_listing_ & QSTAT_DISPLAY_PRIORITY) {
      constexpr int nm_JB_Type_prio[] = {
         JB_nppri,
         JB_nurg,
         JB_priority,
         NoName
      };
      tmp_what = lIntVector2What(JB_Type, nm_JB_Type_prio);
      lMergeWhat(&what_JB_Type, &tmp_what);
   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_RESOURCES) {
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
   if (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) {
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
   if (parameter.full_listing_ & QSTAT_DISPLAY_PRIORITY) {
      constexpr int nm_JAT_Type_template_prio[] = {
         JAT_ntix,
         NoName
      };
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template_prio);
      lMergeWhat(&what_JAT_Type_template, &tmp_what);
   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_RESOURCES) {
      constexpr int nm_JAT_Type_template_res[] = {
         JAT_granted_pe,
         NoName
      };
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_template_res);
      lMergeWhat(&what_JAT_Type_template, &tmp_what);
   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_TASKS) {
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
   if (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) {
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
   if (parameter.full_listing_ & QSTAT_DISPLAY_PRIORITY) {
      constexpr int nm_JAT_Type_list_prio[] = {
         JAT_ntix,
         NoName
      };
      tmp_what = lIntVector2What(JAT_Type, nm_JAT_Type_list_prio);
      lMergeWhat(&what_JAT_Type_list, &tmp_what);
   }
   if (parameter.full_listing_ & QSTAT_DISPLAY_TASKS) {
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
   for_each_ep_lv(ep, parameter.user_list_) {
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
   if ((parameter.full_listing_ & QSTAT_DISPLAY_PENDING) == QSTAT_DISPLAY_PENDING) {
      constexpr uint32_t all_pending_flags = (QSTAT_DISPLAY_USERHOLD|QSTAT_DISPLAY_OPERATORHOLD|
                                              QSTAT_DISPLAY_SYSTEMHOLD|QSTAT_DISPLAY_JOBARRAYHOLD|QSTAT_DISPLAY_JOBHOLD|
                                              QSTAT_DISPLAY_STARTTIMEHOLD|QSTAT_DISPLAY_PEND_REMAIN);
      // Fine grained stated selection for pending jobs or simply all pending jobs
      if ((parameter.full_listing_ & all_pending_flags) == all_pending_flags || (parameter.full_listing_ & all_pending_flags) == 0) {
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
         if ((parameter.full_listing_ & QSTAT_DISPLAY_USERHOLD) == QSTAT_DISPLAY_USERHOLD) {
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
         if ((parameter.full_listing_ & QSTAT_DISPLAY_OPERATORHOLD) == QSTAT_DISPLAY_OPERATORHOLD) {
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
         if ((parameter.full_listing_ & QSTAT_DISPLAY_SYSTEMHOLD) == QSTAT_DISPLAY_SYSTEMHOLD) {
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
         if ((parameter.full_listing_ & QSTAT_DISPLAY_JOBARRAYHOLD) == QSTAT_DISPLAY_JOBARRAYHOLD) {
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
         if ((parameter.full_listing_ & QSTAT_DISPLAY_STARTTIMEHOLD) == QSTAT_DISPLAY_STARTTIMEHOLD) {
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
         if ((parameter.full_listing_ & QSTAT_DISPLAY_JOBHOLD) == QSTAT_DISPLAY_JOBHOLD) {
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
         if ((parameter.full_listing_ & QSTAT_DISPLAY_PEND_REMAIN) == QSTAT_DISPLAY_PEND_REMAIN) {
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
   if ((parameter.full_listing_ & QSTAT_DISPLAY_RUNNING) == QSTAT_DISPLAY_RUNNING) {
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
   if ((parameter.full_listing_ & QSTAT_DISPLAY_SUSPENDED) == QSTAT_DISPLAY_SUSPENDED) {
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
   // has to be overridden by derived class
   return true;
}


bool ocs::QStatModelBase::prepare_data(lList **alpp) {
   // has to be overridden by derived class
   return true;
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
   if (lGetNumberOfElem(parameter.user_list_)) {

      for_each_rw_lv(up, parameter.user_list_) {
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
   if (lGetNumberOfElem(parameter.peref_list_) || lGetNumberOfElem(parameter.queueref_list_) ||
       lGetNumberOfElem(parameter.resource_list_) || lGetNumberOfElem(parameter.queue_user_list_)) {

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
                                                   parameter.queue_user_list_, acl_list_, jep);

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
   if (lGetNumberOfElem(parameter.queueref_list_) > 0) {
      const int count = select_by_qref_list(queue_list_, hgrp_list_, parameter.queueref_list_);

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
   if (lGetNumberOfElem(parameter.queue_user_list_)>0) {
      const int count = select_by_queue_user_list(exechost_list_, queue_list_, parameter.queue_user_list_, acl_list_, project_list_);

      if (count < 0) {
         DRETURN(-1);
      }

      if (count == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-U");
         DRETURN(0);
      }
   }

   // unselect all queues not selected by a -pe (if exist)
   if (lGetNumberOfElem(parameter.peref_list_)>0) {
      const int count = select_by_pe_list(queue_list_, parameter.peref_list_, pe_list_);

      if (count < 0) {
         DRETURN(-1);
      }

      if (count == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-pe");
         DRETURN(0);
      }
   }

   // unselect all queues not selected by a -l
   if (lGetNumberOfElem(parameter.resource_list_)) {
      const int count = select_by_resource_list(parameter.resource_list_, exechost_list_, queue_list_,centry_list_, 1);
      if (count < 0) {
         DRETURN(-1);
      }
   }

   if (!is_cqueue_selected(queue_list_)) {
      DRETURN(0);
   }

   DRETURN(1);
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
