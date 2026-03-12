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

#include "uti/ocs_Pattern.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_log.h"

#include "cull/cull_what.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_Request.h"

#include "ocs_QStatModel.h"
#include "ocs_QStatParameter.h"

#include "msg_qstat.h"
#include "msg_clients_common.h"
#include "sge.h"

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

lEnumeration *
ocs::QStatModel::qstat_get_JB_Type_filter() {
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
ocs::QStatModel::qstat_get_JB_Type_selection(lList *user_list, u_long32 show)
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

int
ocs::QStatModel::qstat_env_get_all_lists(lList** alpp, QStatParameter &parameter) {
   lList **queue_l = parameter.need_queues_ ? &queue_list : nullptr;
   lList **job_l = parameter.need_job_list_ ? &job_list : nullptr;

   lList **centry_l = &centry_list;
   lList **exechost_l = &exechost_list;
   lList **sc_l = &schedd_config;
   lList **pe_l = &pe_list;
   lList **ckpt_l = &ckpt_list;
   lList **acl_l = &acl_list;
   lList **zombie_l = &zombie_list;
   lList **hgrp_l = &hgrp_list;
   lList **project_l = &project_list;
   lList *conf_l = nullptr;

   lList *user_list = parameter.user_list_;
   u_long32 show = parameter.full_listing_;

   lCondition *where= nullptr, *nw = nullptr;
   lCondition *zw = nullptr, *gc_where = nullptr;
   lEnumeration *q_all, *pe_all, *ckpt_all, *acl_all, *ce_all, *up_all;
   lEnumeration *eh_all, *sc_what, *gc_what, *hgrp_what;
   const lListElem *ep = nullptr;
   int q_id = 0, j_id = 0, pe_id = 0, ckpt_id = 0, acl_id = 0, z_id = 0, up_id = 0;
   int ce_id, eh_id, sc_id, gc_id, hgrp_id = 0;
   int show_zombies = (show & QSTAT_DISPLAY_ZOMBIES) ? 1 : 0;

   gdi::Request gdi_multi{};
   const char *cell_root = bootstrap_get_cell_root();
   u_long32 progid = component_get_component_id();

   DENTER(TOP_LAYER);

   if (queue_l) {
      DPRINTF("need queues\n");
      q_all = lWhat("%T(ALL)", CQ_Type);

      q_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::TargetValue::SGE_CQ_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, q_all, true);
      lFreeWhat(&q_all);

      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   } else {
      DPRINTF("queues not needed\n");
   }

   /*
   ** jobs
   */
   if (job_l) {
      lCondition *where = qstat_get_JB_Type_selection(user_list, show);
      lEnumeration *what = qstat_get_JB_Type_filter();

      //lEnumeration *what = lWhat("%T(ALL)", JB_Type);
      //lWriteWhereTo(where, stderr);
      //lWriteWhatTo(what, stderr);

      j_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_JB_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, where, what, true);
      lFreeWhere(&where);

      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /*
   ** job zombies
   */
   if (zombie_l && show_zombies) {
      for_each_ep(ep, user_list) {
         const char *user_name = lGetString(ep, ST_name);
         if (ocs::is_pattern(user_name)) {
            nw = lWhere("%T(%I p= %s)", JB_Type, JB_owner, user_name);
         } else {
            nw = lWhere("%T(%I == %s)", JB_Type, JB_owner, user_name);
         }
         if (!zw) {
            zw = nw;
         } else {
            zw = lOrWhere(zw, nw);
         }
      }

      z_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_ZOMBIE_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, zw, qstat_get_JB_Type_filter(), true);
      lFreeWhere(&zw);

      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /*
   ** complexes
   */
   ce_all = lWhat("%T(ALL)", CE_Type);
   ce_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_CE_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, ce_all, true);
   lFreeWhat(&ce_all);

   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }

   /*
   ** exechosts
   */
   where = lWhere("%T(%I!=%s)", EH_Type, EH_name, SGE_TEMPLATE_NAME);
   eh_all = lWhat("%T(ALL)", EH_Type);
   eh_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_EH_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, where, eh_all, true);
   lFreeWhat(&eh_all);
   lFreeWhere(&where);

   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }

   /*
   ** pe list
   */
   if (pe_l) {
      pe_all = lWhat("%T(%I%I%I%I%I)", PE_Type, PE_name, PE_slots, PE_job_is_first_task, PE_control_slaves, PE_urgency_slots);
      pe_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_PE_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, pe_all, true);
      lFreeWhat(&pe_all);

      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

  /*
   ** ckpt list
   */
   if (ckpt_l) {
      ckpt_all = lWhat("%T(%I)", CK_Type, CK_name);
      ckpt_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_CK_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, ckpt_all, true);
      lFreeWhat(&ckpt_all);

      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /*
   ** acl list
   */
   if (acl_l) {
      acl_all = lWhat("%T(ALL)", US_Type);
      acl_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_US_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, acl_all, true);
      lFreeWhat(&acl_all);

      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /*
   ** project list
   */
   if (project_l) {
      up_all = lWhat("%T(ALL)", PR_Type);
      up_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_PR_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, up_all, true);
      lFreeWhat(&up_all);

      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /*
   ** scheduler configuration
   */

   /* might be enough, but I am not sure */
   /*sc_what = lWhat("%T(%I %I)", SC_Type, SC_user_sort, SC_job_load_adjustments);*/
   sc_what = lWhat("%T(ALL)", SC_Type);

   sc_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_SC_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, sc_what, true);
   lFreeWhat(&sc_what);

   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }

   /*
   ** hgroup
   */
   hgrp_what = lWhat("%T(ALL)", HGRP_Type);
   hgrp_id = gdi_multi.request(alpp, Mode::RECORD, gdi::Target::SGE_HGRP_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, hgrp_what, true);
   lFreeWhat(&hgrp_what);

   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }

   /*
   ** global cluster configuration
   */
   gc_where = lWhere("%T(%I c= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME);
   gc_what = lWhat("%T(ALL)", CONF_Type);
   gc_id = gdi_multi.request(alpp, Mode::SEND, gdi::Target::SGE_CONF_LIST, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, gc_where, gc_what, true);
   gdi_multi.wait();
   lFreeWhat(&gc_what);
   lFreeWhere(&gc_where);

   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }

   /*
   ** handle results
   */
   if (queue_l) {
      /* --- queue */
      gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_CQ_LIST, q_id, queue_l);

      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /* --- job */
   if (job_l) {
      gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_JB_LIST, j_id, job_l);
      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }

      /*
       * debug output to perform testsuite tests
       */
      if (sge_getenv("_SGE_TEST_QSTAT_JOB_STATES") != nullptr) {
         fprintf(stderr, "_SGE_TEST_QSTAT_JOB_STATES: jobs_received=" sge_u32 "\n", lGetNumberOfElem(*job_l));
      }
   }

   /* --- job zombies */
   if (zombie_l && show_zombies) {
      gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_ZOMBIE_LIST, z_id, zombie_l);
      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /* --- complex */
   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_CE_LIST, ce_id, centry_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }

   /* --- exec host */
   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_EH_LIST, eh_id, exechost_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }

   /* --- pe */
   if (pe_l) {
      gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_PE_LIST, pe_id, pe_l);
      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /* --- ckpt */
   if (ckpt_l) {
      gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_CK_LIST, ckpt_id, ckpt_l);
      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /* --- acl */
   if (acl_l) {
      gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_US_LIST, acl_id, acl_l);
      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /* --- project */
   if (project_l) {
      gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_PR_LIST, up_id, project_l);
      if (answer_list_has_error(alpp)) {
         DRETURN(1);
      }
   }

   /* --- scheduler configuration */
   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_SC_LIST, sc_id, sc_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }

   /* --- hgrp */
   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_HGRP_LIST, hgrp_id, hgrp_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }


   /* -- apply global configuration for sge_hostcmp() scheme */
   gdi_multi.get_response(alpp, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_CONF_LIST, gc_id, &conf_l);
   if (answer_list_has_error(alpp)) {
      DRETURN(1);
   }

   if (lFirst(conf_l)) {
      lListElem *local = nullptr;
      merge_configuration(nullptr, progid, cell_root, lFirstRW(conf_l), local, nullptr);
   }
   lFreeList(&conf_l);

   DRETURN(0);
}

bool ocs::QStatModel::fetch_data(lList **answer_list, QStatParameter &parameter) {
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

   qstat_env_get_all_lists(answer_list, parameter);
   DRETURN(true);
}


bool ocs::QStatModel::make_snapshot(lList **answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   // @todo Should be combined with the other GDI requests

   if (!prepare_filter(answer_list, parameter)) {
      DRETURN(false);
   }

   if (!fetch_data(answer_list, parameter)) {
      DRETURN(false);
   }
   DRETURN(true);
}

void ocs::QStatModel::free_data() {
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
   lFreeList(&zombie_list);
   lFreeList(&job_list);
   lFreeList(&hgrp_list);
   lFreeList(&project_list);
}