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

#include <fnmatch.h>

#include "uti/ocs_Pattern.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_log.h"
#include "uti/sge_string.h"
#include "uti/sge.h"

#include "cull/cull_what.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_Request.h"

#include "sched/sge_select_queue.h"

#include "ocs_QStatGenericModel.h"
#include "ocs_QStatParameter.h"
#include "ocs_client_cqueue.h"

#include "msg_qstat.h"

void
ocs::QStatGenericModel::qstat_filter_add_core_attributes(ocs::QStatParameter &parameter) {
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
int ocs::QStatGenericModel::build_job_state_filter(lList **alpp, QStatParameter &parameter) {
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
         "hu", "hs", "ho", "hd", "hj", "ha", "h", "p", "r", "s", "a", nullptr
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
ocs::QStatGenericModel::qstat_filter_add_l_attributes() {
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
ocs::QStatGenericModel::qstat_filter_add_ext_attributes() {
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
ocs::QStatGenericModel::qstat_filter_add_urg_attributes() {
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
ocs::QStatGenericModel::qstat_filter_add_pri_attributes() {
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
ocs::QStatGenericModel::qstat_filter_add_r_attributes() {
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
ocs::QStatGenericModel::qstat_filter_add_t_attributes() {
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
ocs::QStatGenericModel::qstat_filter_add_U_attributes()
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
ocs::QStatGenericModel::qstat_filter_add_pe_attributes() {
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
ocs::QStatGenericModel::qstat_filter_add_q_attributes() {
   lEnumeration *tmp_what = nullptr;
   const int nm_JB_Type[] = {
      JB_checkpoint_name,
      JB_request_set_list,
      NoName
   };

   tmp_what = lIntVector2What(JB_Type, nm_JB_Type);
   lMergeWhat(&what_JB_Type, &tmp_what);
}


bool ocs::QStatGenericModel::prepare_filter(lList **answer_list, QStatParameter &parameter) {
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

lEnumeration *
ocs::QStatGenericModel::qstat_get_JB_Type_filter() {
   DENTER(TOP_LAYER);

   if (what_JAT_Type_template != nullptr) {
      lWhatSetSubWhat(what_JB_Type, JB_ja_template, &what_JAT_Type_template);
   }
   if (what_JAT_Type_list != nullptr) {
      lWhatSetSubWhat(what_JB_Type, JB_ja_tasks, &what_JAT_Type_list);
   }

   DRETURN(what_JB_Type);
}

/* ----------- functions from qstat_filter ---------------------------------- */
lCondition *
ocs::QStatGenericModel::qstat_get_JB_Type_selection(lList *user_list, u_long32 show)
{
   lCondition *jw = nullptr;
   lCondition *nw = nullptr;

   DENTER(TOP_LAYER);

   /*
    * Retrieve jobs only for those users specified via -u switch
    */
   {
      const lListElem *ep = nullptr;
      lCondition *tmp_nw = nullptr;

      for_each_ep(ep, user_list) {
         const char *user = lGetString(ep, ST_name);
         if (ocs::is_pattern(user)) {
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
   }

   /*
    * Select jobs according to current state
    */
   {
      lCondition *tmp_nw = nullptr;

      /*
       * Pending jobs (all that are not running)
       */
      if ((show & QSTAT_DISPLAY_PENDING) == QSTAT_DISPLAY_PENDING) {
         const u_long32 all_pending_flags = (QSTAT_DISPLAY_USERHOLD|QSTAT_DISPLAY_OPERATORHOLD|
                    QSTAT_DISPLAY_SYSTEMHOLD|QSTAT_DISPLAY_JOBARRAYHOLD|QSTAT_DISPLAY_JOBHOLD|
                    QSTAT_DISPLAY_STARTTIMEHOLD|QSTAT_DISPLAY_PEND_REMAIN);
         /*
          * Fine grained stated selection for pending jobs
          * or simply all pending jobs
          */
         if (((show & all_pending_flags) == all_pending_flags) ||
             ((show & all_pending_flags) == 0)) {
            /*
             * All jobs not running (= all pending)
             */
            tmp_nw = lWhere("%T(!(%I -> %T((%I m= %u))))", JB_Type, JB_ja_tasks,
                        JAT_Type, JAT_status, JRUNNING);
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
            /*
             * User Hold
             */
            if ((show & QSTAT_DISPLAY_USERHOLD) == QSTAT_DISPLAY_USERHOLD) {
               /* unenrolled jobs in user hold state ... */
               tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_u_h_ids,
                           RN_Type, RN_min, 0);
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
            /*
             * Operator Hold
             */
            if ((show & QSTAT_DISPLAY_OPERATORHOLD) == QSTAT_DISPLAY_OPERATORHOLD) {
               tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_o_h_ids,
                           RN_Type, RN_min, 0);
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
            /*
             * System Hold
             */
            if ((show & QSTAT_DISPLAY_SYSTEMHOLD) == QSTAT_DISPLAY_SYSTEMHOLD) {
               tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_s_h_ids,
                           RN_Type, RN_min, 0);
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
            /*
             * Job Array Dependency Hold
             */
            if ((show & QSTAT_DISPLAY_JOBARRAYHOLD) == QSTAT_DISPLAY_JOBARRAYHOLD) {
               tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_a_h_ids,
                           RN_Type, RN_min, 0);
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
            if ((show & QSTAT_DISPLAY_STARTTIMEHOLD) == QSTAT_DISPLAY_STARTTIMEHOLD) {
               tmp_nw = lWhere("%T(%I > %lu)", JB_Type, JB_execution_time, 0);
               if (nw == nullptr) {
                  nw = tmp_nw;
               } else {
                  nw = lOrWhere(nw, tmp_nw);
               }
            }
            /*
             * Job Dependency Hold
             */
            if ((show & QSTAT_DISPLAY_JOBHOLD) == QSTAT_DISPLAY_JOBHOLD) {
               tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_jid_predecessor_list, JRE_Type, JRE_job_number, 0);
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
            if ((show & QSTAT_DISPLAY_PEND_REMAIN) == QSTAT_DISPLAY_PEND_REMAIN) {
               tmp_nw = lWhere("%T(%I -> %T((%I != %u)))", JB_Type, JB_ja_tasks,
                           JAT_Type, JAT_job_restarted, 0);
               if (nw == nullptr) {
                  nw = tmp_nw;
               } else {
                  nw = lOrWhere(nw, tmp_nw);
               }
               tmp_nw = lWhere("%T(%I -> %T((%I m= %u)))", JB_Type, JB_ja_tasks,
                           JAT_Type, JAT_state, JERROR);
               if (nw == nullptr) {
                  nw = tmp_nw;
               } else {
                  nw = lOrWhere(nw, tmp_nw);
               }
               tmp_nw = lWhere("%T(%I -> %T((%I > %u)))", JB_Type, JB_ja_n_h_ids,
                           RN_Type, RN_min, 0);
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
      if ((show & QSTAT_DISPLAY_RUNNING) == QSTAT_DISPLAY_RUNNING) {
         tmp_nw = lWhere("%T(((%I -> %T(%I m= %u)) || (%I -> %T(%I m= %u))) && !(%I -> %T((%I m= %u))))", JB_Type,
                         JB_ja_tasks, JAT_Type, JAT_status, JRUNNING,
                         JB_ja_tasks, JAT_Type, JAT_status, JTRANSFERING,
                         JB_ja_tasks, JAT_Type, JAT_state, JSUSPENDED);
         if (nw == nullptr) {
            nw = tmp_nw;
         } else {
            nw = lOrWhere(nw, tmp_nw);
         }
      }

      /*
       * Suspended jobs
       *
       * NOTE:
       *    see comment above
       */
      if ((show & QSTAT_DISPLAY_SUSPENDED) == QSTAT_DISPLAY_SUSPENDED) {
         tmp_nw = lWhere("%T((%I -> %T(%I m= %u)) || (%I -> %T(%I m= %u)) || (%I -> %T(%I m= %u)))", JB_Type,
                         JB_ja_tasks, JAT_Type, JAT_status, JRUNNING,
                         JB_ja_tasks, JAT_Type, JAT_status, JTRANSFERING,
                         JB_ja_tasks, JAT_Type, JAT_state, JSUSPENDED);
         if (nw == nullptr) {
            nw = tmp_nw;
         } else {
            nw = lOrWhere(nw, tmp_nw);
         }
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

void ocs::QStatGenericModel::calc_longest_queue_length(QStatParameter &parameter) {
   u_long32 name;
   char *env;
   const lListElem *qep = nullptr;

   if (parameter.output_mode_== ocs::QStatParameter::OutputMode::QSTAT_GROUP) {
      name = CQ_name;
   } else {
      name = QU_full_name;
   }
   if ((env = getenv("SGE_LONG_QNAMES")) != nullptr){
      parameter.longest_queue_length = atoi(env);
      if (parameter.longest_queue_length == -1) {
         for_each_ep(qep, queue_list) {
            int length;
            const char *queue_name =lGetString(qep, name);
            if ((length = strlen(queue_name)) > parameter.longest_queue_length){
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


bool ocs::QStatGenericModel::fetch_data(lList **alpp, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   int q_id, j_id, pe_id, ckpt_id, acl_id, up_id, ce_id, eh_id, sc_id, gc_id, hgrp_id;
   lCondition *where = nullptr;
   lEnumeration *what = nullptr;
   gdi::Request gdi_multi{};

   if (!gdi::Client::sge_gdi_get_permission(alpp, &is_manager_, nullptr, nullptr, nullptr)) {
      DRETURN(false);
   }

   if (parameter.need_queues_) {
      DPRINTF("need queues\n");
      what = lWhat("%T(ALL)", CQ_Type);
      q_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::TargetValue::SGE_CQ_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
      lFreeWhat(&what);
      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }

   if (parameter.need_job_list_) {
      what = qstat_get_JB_Type_filter();
      where = qstat_get_JB_Type_selection(parameter.user_list_, parameter.full_listing_);
      j_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_JB_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, where, what, true);
      lFreeWhere(&where);
      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }

   what = lWhat("%T(ALL)", CE_Type);
   ce_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_CE_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   where = lWhere("%T(%I!=%s)", EH_Type, EH_name, SGE_TEMPLATE_NAME);
   what = lWhat("%T(ALL)", EH_Type);
   eh_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_EH_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, where, what, true);
   lFreeWhat(&what);
   lFreeWhere(&where);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   what = lWhat("%T(%I%I%I%I%I)", PE_Type, PE_name, PE_slots, PE_job_is_first_task, PE_control_slaves, PE_urgency_slots);
   pe_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_PE_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   what = lWhat("%T(%I)", CK_Type, CK_name);
   ckpt_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_CK_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   what = lWhat("%T(ALL)", US_Type);
   acl_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_US_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   what = lWhat("%T(ALL)", PR_Type);
   up_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_PR_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   what = lWhat("%T(ALL)", SC_Type);
   sc_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_SC_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   what = lWhat("%T(ALL)", HGRP_Type);
   hgrp_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_HGRP_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   lFreeWhat(&what);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   where = lWhere("%T(%I c= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME);
   what = lWhat("%T(ALL)", CONF_Type);
   gc_id = gdi_multi.request(alpp, Mode::SEND, gdi::Target::SGE_CONF_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, where, what, true);
   gdi_multi.wait();
   lFreeWhat(&what);
   lFreeWhere(&where);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   // Start fetching the lists

   if (parameter.need_queues_) {
      gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_CQ_LIST, q_id, &queue_list);
      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }

   if (parameter.need_job_list_) {
      gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_JB_LIST, j_id, &job_list);
      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }

      // @todo this will not work as stored procedure
      // debug output to perform testsuite tests
      if (sge_getenv("_SGE_TEST_QSTAT_JOB_STATES") != nullptr) {
         fprintf(stderr, "_SGE_TEST_QSTAT_JOB_STATES: jobs_received=" sge_u32 "\n", lGetNumberOfElem(job_list));
      }
   }

   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_CE_LIST, ce_id, &centry_list);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_EH_LIST, eh_id, &exechost_list);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_PE_LIST, pe_id, &pe_list);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_CK_LIST, ckpt_id, &ckpt_list);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_US_LIST, acl_id, &acl_list);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_PR_LIST, up_id, &project_list);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_SC_LIST, sc_id, &schedd_config);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_HGRP_LIST, hgrp_id, &hgrp_list);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   lList *conf_l = nullptr;
   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_CONF_LIST, gc_id, &conf_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }
   if (lFirst(conf_l) != nullptr) {
      lListElem *local = nullptr;
      const char *cell_root = bootstrap_get_cell_root();
      u_long32 progid = component_get_component_id();
      merge_configuration(nullptr, progid, cell_root, lFirstRW(conf_l), local, nullptr);
   }
   lFreeList(&conf_l);

   DRETURN(true);
}

bool ocs::QStatGenericModel::prepare_data(lList **alpp) {
   DENTER(TOP_LAYER);

   if (!sconf_set_config(&schedd_config, alpp)) {
      DRETURN(false);
   }

   centry_list_init_double(centry_list);

   DRETURN(true);
}


/*-------------------------------------------------------------------------*/
int ocs::QStatGenericModel::qstat_env_filter_queues(lList **alpp, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   centry_list_init_double(centry_list);

   /* all queues are selected */
   cqueue_list_set_tag(queue_list, TAG_SHOW_IT, true);

   /* unseclect all queues not selected by a -q (if exist) */
   int nqueues = 0;
   if (lGetNumberOfElem(parameter.queueref_list_)>0) {

      if ((nqueues=select_by_qref_list(queue_list, hgrp_list, parameter.queueref_list_))<0) {
         DRETURN(-1);
      }

      if (nqueues==0) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-q");
         DRETURN(0);
      }
   }

   /* unselect all queues not selected by -qs */
   select_by_queue_state(parameter.queue_state_, exechost_list, queue_list, centry_list);

   /* unselect all queues not selected by a -U (if exist) */
   if (lGetNumberOfElem(parameter.queue_user_list_)>0) {
      if ((nqueues=select_by_queue_user_list(exechost_list, queue_list, parameter.queue_user_list_, acl_list, project_list))<0) {
         DRETURN(-1);
      }

      if (nqueues==0) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-U");
         DRETURN(0);
      }
   }

   /* unselect all queues not selected by a -pe (if exist) */
   if (lGetNumberOfElem(parameter.peref_list_)>0) {
      if ((nqueues=select_by_pe_list(queue_list, parameter.peref_list_, pe_list))<0) {
         DRETURN(-1);
      }

      if (nqueues==0) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-pe");
         DRETURN(0);
      }
   }
   /* unselect all queues not selected by a -l (if exist) */
   if (lGetNumberOfElem(parameter.resource_list_)) {
      u_long32 empty_qs = 1;
      if (select_by_resource_list(parameter.resource_list_, exechost_list, queue_list,centry_list, empty_qs)<0) {
         DRETURN(-1);
      }
   }

   if (rmon_mlgetl(&RMON_DEBUG_ON, GDI_LAYER) & INFOPRINT) {
      const lListElem *cqueue;
      for_each_ep(cqueue, queue_list) {
         const lListElem *qep;
         const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

         for_each_ep(qep, qinstance_list) {
            if ((lGetUlong(qep, QU_tag) & TAG_SHOW_IT) != 0) {
               DPRINTF("++ %s\n", lGetString(qep, QU_full_name));
            } else {
               DPRINTF("-- %s\n", lGetString(qep, QU_full_name));
            }
         }
      }
   }


   if (!is_cqueue_selected(queue_list)) {
      DRETURN(0);
   }

   DRETURN(1);
}

int ocs::QStatGenericModel::filter_jobs(QStatParameter &parameter) {

   lListElem *jep = nullptr;
   lListElem *jatep = nullptr;
   const lListElem *up = nullptr;

   DENTER(TOP_LAYER);

   // select all jobs which are not finished
   for_each_rw (jep, job_list) {
      for_each_rw(jatep, lGetList(jep, JB_ja_tasks)) {
         if (!(lGetUlong(jatep, JAT_status) & JFINISHED))
            lSetUlong(jatep, JAT_suitable, TAG_SHOW_IT);
      }
   }

   // untag all jobs which do not fit to the user list (-u)
   if (lGetNumberOfElem(parameter.user_list_)) {

      for_each_rw(up, parameter.user_list_) {
         const char *user = lGetString(up, ST_name);
         if (user == nullptr) {
            break;
         }

         const bool is_pattern = ocs::is_pattern(user);
         for_each_rw (jep, job_list) {
            const char *owner = lGetString(jep, JB_owner);

            int match;
            if (is_pattern) {
               match = fnmatch(user, owner, 0);
            } else {
               match = sge_strnullcmp(user, owner);
            }
            if (match == 0) {
               for_each_rw(jatep, lGetList(jep, JB_ja_tasks)) {
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
      for_each_rw(jep, job_list) {
         bool show_job = false;
         const lListElem *cqueue = nullptr;

         for_each_ep(cqueue, queue_list) {
            const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

            lListElem *qep = nullptr;
            for_each_rw(qep, qinstance_list) {

               if (!(lGetUlong(qep, QU_tag) & TAG_SHOW_IT)) {
                  continue;
               }

               // find if the queue instance is in the list of selected queues and if the job fits into this queue instance
               if (lListElem *host = host_list_locate(exechost_list, lGetHost(qep, QU_qhostname)); host != nullptr) {
                  const int ret = sge_select_queue(job_get_hard_resource_listRW(jep), qep,
                                                   host, exechost_list, centry_list,true, 1,
                                                   parameter.queue_user_list_, acl_list, jep);

                  if (ret==1) {
                     show_job = true;
                     break;
                  }
               }
            }
         }

         for_each_rw(jatep, lGetList(jep, JB_ja_tasks)) {
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
      lPSortList(queue_list, "%I+ ", CQ_name);
   } else {

      // cluster queues are not required. We need to reconstruct the queue list with the queue instances only.
      lList *tmp_queue_list = lCreateList("", QU_Type);
      lListElem *cqueue = nullptr;
      for_each_rw(cqueue, queue_list) {
         lList *qinstances = nullptr;
         lXchgList(cqueue, CQ_qinstances, &qinstances);
         lAddList(tmp_queue_list, &qinstances);
      }
      lFreeList(&queue_list);
      queue_list = tmp_queue_list;

      // sort queue instances
      lPSortList(queue_list, "%I+ %I+ %I+", QU_seq_no, QU_qname, QU_qhostname);
   }
   DRETURN(0);
}


bool ocs::QStatGenericModel::filter_data(lList **alpp, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   // filter queues according to given parameters
   if (qstat_env_filter_queues(alpp, parameter) < 0) {
      DPRINTF("qstat_env_filter_queues failed\n");
      DRETURN(false);
   }

   // if output mode is qselect, we do not filter the jobs
   if (parameter.output_mode_!= QStatParameter::OutputMode::QSELECT) {

      // filter jobs according to given parameters (might also unselect queues)
      if (filter_jobs(parameter) != 0) {
         DPRINTF("filter_jobs failed\n");
         DRETURN(false);
      }
   }
   DRETURN(true);
}

bool ocs::QStatGenericModel::make_snapshot(lList **answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (!prepare_filter(answer_list, parameter)) {
      DRETURN(false);
   }

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

void ocs::QStatGenericModel::free_data() {
   lFreeWhat(&what_JB_Type);
   lFreeWhat(&what_JAT_Type_template);
   lFreeWhat(&what_JAT_Type_list);

   lFreeList(&queue_list);
   lFreeList(&centry_list);
   lFreeList(&exechost_list);
   lFreeList(&schedd_config);
   lFreeList(&pe_list);
   lFreeList(&ckpt_list);
   lFreeList(&acl_list);
   lFreeList(&job_list);
   lFreeList(&hgrp_list);
   lFreeList(&project_list);
}