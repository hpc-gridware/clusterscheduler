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

/** @file
 * @brief Base model of `qstat`: the lists the report is built from
 */

#include <cerrno>
#include <fnmatch.h>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "uti/sge.h"
#include "uti/ocs_Pattern.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_parse_num_par.h"

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
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_ulong.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_MES.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_CQ.h"

#include "sched/sge_select_queue.h"

#include "ocs_QStatModelBase.h"
#include "msg_qstat.h"

/** @brief Free the fetched lists
 *
 * Every list is an owning handle and frees itself.
 */
ocs::QStatModelBase::~QStatModelBase() = default;

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
            "hu", "hs", "ho", "hd", "hj", "ha", "h", "p", "r", "s", "f", "a", nullptr
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
            QSTAT_DISPLAY_FINISHED,  /* CS-1908: qstat -s f = retained finished ja_tasks */
            /* CS-1908: `-s a` means "all states" and now includes the
             * QSTAT_DISPLAY_FINISHED retention bit. The plain `qstat` default
             * (QStatParameter::show_) still excludes finished so users opt
             * into the retention window either via `-s a` or `-s f`. */
            (QSTAT_DISPLAY_PENDING|QSTAT_DISPLAY_RUNNING|QSTAT_DISPLAY_SUSPENDED|QSTAT_DISPLAY_FINISHED),
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
            const char *s_before = s;
            for (int i = 0; flags[i] != nullptr; i++) {
               if (strncmp(s, flags[i], strlen(flags[i])) == 0) {
                  parameter.show_ |= bits[i];
                  s += strlen(flags[i]);
               }
            }
            if (s == s_before) {
               s++;
            }
         }
      }
   }

   DRETURN_VOID;
}

/** @brief Work out how wide the queue name column has to be
 *
 * The plain text listing sizes that column to the longest name actually
 * reported, so it can only be decided once the lists are filtered.
 *
 * @param parameter the parsed parameters, which receive the width
 * @return none
 */
void ocs::QStatModelBase::calc_longest_queue_length(QStatParameter &parameter) const {
   DENTER(TOP_LAYER);

   const char *env = parameter.get_variable("SGE_LONG_QNAMES");
   if (env == nullptr) {
      DRETURN_VOID;
   }

   // parse env value
   int queue_length;
   try {
      queue_length = std::stoi(env);
   } catch (std::invalid_argument &e) {
      queue_length = 30;
   } catch (std::out_of_range &e) {
      queue_length = 30;
   }

   // variable set but to -1 then find max length; -g c lists cluster queues, the others queue instances
   if (queue_length == -1) {
      if (parameter.output_mode_ == QStatParameter::OutputMode::QSTAT_GROUP) {
         for (const auto cqueue : *cqueue_list_) {
            if (const char *qname = ocs::tom::get<CQ_name>(cqueue); qname != nullptr) {
               queue_length = std::max(queue_length, static_cast<int>(strlen(qname)));
            }
         }
      } else {
         for (const auto qep : *qinstance_list_) {
            if (const char *qname = ocs::tom::get<QU_full_name>(qep); qname != nullptr) {
               queue_length = std::max(queue_length, static_cast<int>(strlen(qname)));
            }
         }
      }
   }

   // take a minimum of 10 characters
   if (queue_length < 10) {
      queue_length = 10;
   }
   parameter.set_longest_queue_length(queue_length);

   DRETURN_VOID;
}

/** @brief The job fields of the plain listing, without the sub-selections of the array tasks
 *
 * Which fields those are depends on the parameters - the extra priority,
 * urgency and resource columns are only fetched when they are printed.
 *
 * @param parameter the parsed parameters
 * @return the selection
 */
ocs::tom::What<ocs::tom::JB>
ocs::QStatModelBase::get_sub_job_filter(const QStatParameter &parameter) {
   auto what = ocs::tom::what<JB_job_number, JB_owner, JB_type, JB_pe, JB_jid_predecessor_list,
                              JB_ja_ad_predecessor_list, JB_job_name, JB_submission_time, JB_pe_range,
                              JB_ja_structure, JB_ja_tasks, JB_ja_n_h_ids, JB_ja_u_h_ids, JB_ja_o_h_ids,
                              JB_ja_s_h_ids, JB_ja_a_h_ids, JB_ja_template, JB_execution_time,
                              JB_request_set_list, JB_project, JB_jobshare, JB_checkpoint_name>();
   if (parameter.show_ & QSTAT_DISPLAY_EXTENDED) {
      what = std::move(what).merge(ocs::tom::what<JB_department, JB_override_tickets>());
   }
   if (parameter.show_ & QSTAT_DISPLAY_URGENCY) {
      what = std::move(what).merge(
         ocs::tom::what<JB_deadline, JB_nurg, JB_urg, JB_rrcontr, JB_dlcontr, JB_wtcontr>());
   }
   if (parameter.show_ & QSTAT_DISPLAY_PRIORITY) {
      what = std::move(what).merge(ocs::tom::what<JB_nppri, JB_nurg, JB_priority>());
   }
   if (parameter.show_ & QSTAT_DISPLAY_RESOURCES) {
      what = std::move(what).merge(ocs::tom::what<JB_checkpoint_name, JB_request_set_list, JB_jid_request_list,
                                                  JB_ja_ad_request_list, JB_binding>());
   }
   return what;
}

/** @brief The fields of the array task template of a job (`JB_ja_template`) the listing needs
 * @param parameter the parsed parameters
 * @return the selection
 */
ocs::tom::What<ocs::tom::JAT>
ocs::QStatModelBase::get_sub_ja_task_template_filter(const QStatParameter &parameter) {
   auto what = ocs::tom::what<JAT_task_number, JAT_prio, JAT_hold, JAT_state, JAT_status, JAT_job_restarted,
                              JAT_start_time>();
   if (parameter.show_ & QSTAT_DISPLAY_EXTENDED) {
      what = std::move(what).merge(ocs::tom::what<JAT_ntix, JAT_scaled_usage_list, JAT_granted_pe, JAT_tix,
                                                  JAT_oticket, JAT_fticket, JAT_sticket, JAT_share>());
   }
   if (parameter.show_ & QSTAT_DISPLAY_PRIORITY) {
      what = std::move(what).merge(ocs::tom::what<JAT_ntix>());
   }
   if (parameter.show_ & QSTAT_DISPLAY_RESOURCES) {
      what = std::move(what).merge(ocs::tom::what<JAT_granted_pe>());
   }
   if (parameter.show_ & QSTAT_DISPLAY_TASKS) {
      what = std::move(what).merge(ocs::tom::what<JAT_task_list, JAT_usage_list, JAT_scaled_usage_list>());
   }
   return what;
}

/** @brief The fields of the enrolled array tasks of a job (`JB_ja_tasks`) the listing needs
 * @param parameter the parsed parameters
 * @return the selection
 */
ocs::tom::What<ocs::tom::JAT>
ocs::QStatModelBase::get_sub_ja_task_filter(const QStatParameter &parameter) {
   auto what = ocs::tom::what<JAT_task_number, JAT_status, JAT_granted_destin_identifier_list, JAT_suitable,
                              JAT_granted_pe, JAT_state, JAT_prio, JAT_hold, JAT_job_restarted, JAT_start_time>();
   if (parameter.show_ & QSTAT_DISPLAY_EXTENDED) {
      what = std::move(what).merge(ocs::tom::what<JAT_ntix, JAT_scaled_usage_list, JAT_task_list, JAT_tix,
                                                  JAT_oticket, JAT_fticket, JAT_sticket, JAT_share>());
   }
   if (parameter.show_ & QSTAT_DISPLAY_PRIORITY) {
      what = std::move(what).merge(ocs::tom::what<JAT_ntix>());
   }
   if (parameter.show_ & QSTAT_DISPLAY_TASKS) {
      what = std::move(what).merge(ocs::tom::what<JAT_task_list, JAT_usage_list, JAT_scaled_usage_list>());
   }
   return what;
}

/** @brief The job fields the report needs
 *
 * Which fields those are depends on the parameters - the extra priority
 * and usage columns are only fetched when they are going to be printed.
 *
 * @param parameter the parsed parameters
 * @return the selection, with the enrolled tasks and the task template narrowed to the fields needed
 */
ocs::tom::What<ocs::tom::JB>
ocs::QStatModelBase::get_job_what(const QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   // job attribute filter, with the array task filter for enrolled tasks and the one for the task template as
   // sub filters
   auto job_what = get_sub_job_filter(parameter)
                      .nest<JB_ja_tasks>(get_sub_ja_task_filter(parameter))
                      .nest<JB_ja_template>(get_sub_ja_task_template_filter(parameter));

   DRETURN(job_what);
}

/** @brief Build the condition selecting the jobs of the users given with `-u`
 *
 * The default `-u $user` and the wildcard `*` have already been resolved, see
 * str_list_transform_user_list(). A user name may be a wildcard pattern.
 *
 * @param user_list the users whose jobs are selected
 * @return the condition; empty if the job owner is not restricted
 */
ocs::tom::Where<ocs::tom::JB>
ocs::QStatModelBase::get_job_owner_where(const ocs::tom::ConstList<ocs::tom::ST> user_list) {
   DENTER(TOP_LAYER);
   ocs::tom::Where<ocs::tom::JB> where;

   for (const auto ep : user_list) {
      if (const char *user = ocs::tom::get<ST_name>(ep); is_pattern(user)) {
         where |= ocs::tom::field<JB_owner>().like(user);
      } else {
         where |= ocs::tom::field<JB_owner>() == user;
      }
   }
   DRETURN(where);
}

/** @brief Build the condition selecting the jobs to report
 * @param parameter the parsed parameters, holding the user and state filters
 * @return the condition; empty if neither users nor states restrict the jobs
 */
ocs::tom::Where<ocs::tom::JB>
ocs::QStatModelBase::get_job_where(const QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   ocs::tom::Where<ocs::tom::JB> nw;

   // Retrieve jobs only for those users specified via -u switch
   auto jw = get_job_owner_where(parameter.get_user_list());

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
         nw |= !ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_status>().has_bits(JRUNNING));
         /*
          * Array Jobs with one or more tasks pending
          */
         nw |= ocs::tom::field<JB_ja_n_h_ids>().any(ocs::tom::field<RN_min>() > 0U);
         nw |= ocs::tom::field<JB_ja_u_h_ids>().any(ocs::tom::field<RN_min>() > 0U);
         nw |= ocs::tom::field<JB_ja_s_h_ids>().any(ocs::tom::field<RN_min>() > 0U);
         nw |= ocs::tom::field<JB_ja_o_h_ids>().any(ocs::tom::field<RN_min>() > 0U);
      } else {
         // User Hold
         if ((parameter.show_ & QSTAT_DISPLAY_USERHOLD) == QSTAT_DISPLAY_USERHOLD) {
            /* unenrolled jobs in user hold state ... */
            nw |= ocs::tom::field<JB_ja_u_h_ids>().any(ocs::tom::field<RN_min>() > 0U);
            /* ... or enrolled jobs with an user  hold */
            nw |= ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_hold>().has_bits(MINUS_H_TGT_USER));
         }
         // Operator Hold
         if ((parameter.show_ & QSTAT_DISPLAY_OPERATORHOLD) == QSTAT_DISPLAY_OPERATORHOLD) {
            nw |= ocs::tom::field<JB_ja_o_h_ids>().any(ocs::tom::field<RN_min>() > 0U);
            nw |= ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_hold>().has_bits(MINUS_H_TGT_OPERATOR));
         }
         // System Hold
         if ((parameter.show_ & QSTAT_DISPLAY_SYSTEMHOLD) == QSTAT_DISPLAY_SYSTEMHOLD) {
            nw |= ocs::tom::field<JB_ja_s_h_ids>().any(ocs::tom::field<RN_min>() > 0U);
            nw |= ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_hold>().has_bits(MINUS_H_TGT_SYSTEM));
         }
         // Job Array Dependency Hold
         if ((parameter.show_ & QSTAT_DISPLAY_JOBARRAYHOLD) == QSTAT_DISPLAY_JOBARRAYHOLD) {
            nw |= ocs::tom::field<JB_ja_a_h_ids>().any(ocs::tom::field<RN_min>() > 0U);
            nw |= ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_hold>().has_bits(MINUS_H_TGT_JA_AD));
         }
         /*
          * Start Time Hold
          */
         if ((parameter.show_ & QSTAT_DISPLAY_STARTTIMEHOLD) == QSTAT_DISPLAY_STARTTIMEHOLD) {
            nw |= ocs::tom::field<JB_execution_time>() > std::uint64_t{0};
         }
         /*
          * Job Dependency Hold
          */
         if ((parameter.show_ & QSTAT_DISPLAY_JOBHOLD) == QSTAT_DISPLAY_JOBHOLD) {
            nw |= ocs::tom::field<JB_jid_predecessor_list>().any(ocs::tom::field<JRE_job_number>() > 0U);
         }
         /*
          * Rescheduled and jobs in error state (not in hold/no start time/no dependency)
          * and regular pending jobs
          */
         if ((parameter.show_ & QSTAT_DISPLAY_PEND_REMAIN) == QSTAT_DISPLAY_PEND_REMAIN) {
            nw |= ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_job_restarted>() != 0U);
            nw |= ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_state>().has_bits(JERROR));
            nw |= ocs::tom::field<JB_ja_n_h_ids>().any(ocs::tom::field<RN_min>() > 0U);
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
      nw |= (ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_status>().has_bits(JRUNNING)) ||
             ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_status>().has_bits(JTRANSFERING))) &&
            !ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_state>().has_bits(JSUSPENDED));
   }

   // Suspended jobs
   if ((parameter.show_ & QSTAT_DISPLAY_SUSPENDED) == QSTAT_DISPLAY_SUSPENDED) {
      nw |= ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_status>().has_bits(JRUNNING)) ||
            ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_status>().has_bits(JTRANSFERING)) ||
            ocs::tom::field<JB_ja_tasks>().any(ocs::tom::field<JAT_state>().has_bits(JSUSPENDED));
   }

   // the state condition restricts the owner condition; an empty one leaves it as it is
   jw &= std::move(nw);

   DRETURN(jw);
}

/** @brief The cluster queue fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::CQ> ocs::QStatModelBase::get_queue_what() {
   return ocs::tom::what_all<ocs::tom::CQ>();
}

/** @brief The complex entry fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::CE> ocs::QStatModelBase::get_centry_what() {
   return ocs::tom::what_all<ocs::tom::CE>();
}

/** @brief Build the condition selecting the execution hosts to fetch
 * @return the condition: every host but the template host
 */
ocs::tom::Where<ocs::tom::EH> ocs::QStatModelBase::get_ehost_where() {
   return ocs::tom::field<EH_name>() != SGE_TEMPLATE_NAME;
}

/** @brief The execution host fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::EH> ocs::QStatModelBase::get_ehost_what() {
   return ocs::tom::what_all<ocs::tom::EH>();
}

/** @brief The parallel environment fields the model needs
 * @return the selection
 */
ocs::tom::What<ocs::tom::PE> ocs::QStatModelBase::get_pe_what() {
   return ocs::tom::what<PE_name, PE_slots, PE_job_is_first_task, PE_control_slaves, PE_urgency_slots>();
}

/** @brief The checkpointing environment fields the model needs
 * @return the selection
 */
ocs::tom::What<ocs::tom::CK> ocs::QStatModelBase::get_ckpt_what() {
   return ocs::tom::what<CK_name>();
}

/** @brief The user set fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::US> ocs::QStatModelBase::get_uset_what() {
   return ocs::tom::what_all<ocs::tom::US>();
}

/** @brief The project fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::PR> ocs::QStatModelBase::get_prj_what() {
   return ocs::tom::what_all<ocs::tom::PR>();
}

/** @brief The scheduler configuration fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::SC> ocs::QStatModelBase::get_sconf_what() {
   return ocs::tom::what_all<ocs::tom::SC>();
}

/** @brief The host group fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::HGRP> ocs::QStatModelBase::get_hgrp_what() {
   return ocs::tom::what_all<ocs::tom::HGRP>();
}

/** @brief Build the condition selecting the configuration entries to fetch
 * @return the condition: the global configuration, its name compared without regard to case
 */
ocs::tom::Where<ocs::tom::CONF> ocs::QStatModelBase::get_conf_where() {
   return ocs::tom::field<CONF_name>().equals_nocase(SGE_GLOBAL_NAME);
}

/** @brief The configuration fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::CONF> ocs::QStatModelBase::get_conf_what() {
   return ocs::tom::what_all<ocs::tom::CONF>();
}

/** @brief Build the condition selecting the jobs of a `qstat -j` call
 * @param job_view_list the jobs the user named, by number or pattern
 * @param user_list the users given with `-u`, by default the current user, see get_job_owner_where()
 * @return the condition; empty if neither jobs nor users were named
 */
ocs::tom::Where<ocs::tom::JB>
ocs::QStatModelBase::get_job_view_where(const ocs::tom::ConstList<ocs::tom::ST> job_view_list,
                                        const ocs::tom::ConstList<ocs::tom::ST> user_list) {
   DENTER(TOP_LAYER);
   ocs::tom::Where<ocs::tom::JB> where;

   for (const auto j_elem : job_view_list) {
      const char *job_name = ocs::tom::get<ST_name>(j_elem);

      if (job_name != nullptr && isdigit(static_cast<unsigned char>(job_name[0]))) {
         char *end = nullptr;
         errno = 0;
         unsigned long raw = strtoul(job_name, &end, 10);
         if (errno == 0 && end != job_name && *end == '\0' && raw <= std::numeric_limits<uint32_t>::max()) {
            where |= ocs::tom::field<JB_job_number>() == static_cast<uint32_t>(raw);
         } else {
            where |= ocs::tom::field<JB_job_name>().like(job_name);
         }
      } else if (job_name != nullptr) {
         where |= ocs::tom::field<JB_job_name>().like(job_name);
      }
   }

   // like plain qstat, -j shows only the jobs of the users given with -u
   where &= get_job_owner_where(user_list);
   DRETURN(where);
}

/** @brief The scheduler job info fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::SME> ocs::QStatModelBase::get_sme_what() {
   return ocs::tom::what_all<ocs::tom::SME>();
}

/** @brief The `qstat -j` fields the model needs
 *
 * JB_ja_ad_request_list is named twice, as the selection always did; the
 * selection holds it twice, which selects it once.
 *
 * @return the selection, with the enrolled tasks and the job references narrowed to the fields needed
 */
ocs::tom::What<ocs::tom::JB> ocs::QStatModelBase::get_job_view_what() {
   return ocs::tom::what<JB_job_number, JB_ar, JB_exec_file, JB_submission_time,
                         JB_submission_command_line, JB_owner, JB_uid, JB_group, JB_gid,
                         JB_account, JB_merge_stderr, JB_mail_list, JB_project,
                         JB_department, JB_notify, JB_job_name, JB_stdout_path_list,
                         JB_jobshare, JB_request_set_list, JB_shell_list,
                         JB_env_list, JB_job_args, JB_script_file, JB_context,
                         JB_cwd, JB_stderr_path_list, JB_deadline,
                         JB_execution_time, JB_checkpoint_name, JB_checkpoint_attr, JB_checkpoint_interval,
                         JB_directive_prefix, JB_reserve, JB_mail_options, JB_stdin_path_list, JB_priority,
                         JB_restart, JB_verify, JB_script_size, JB_pe, JB_pe_range,
                         JB_jid_request_list, JB_ja_ad_request_list, JB_verify_suitable_queues,
                         JB_soft_wallclock_gmt, JB_hard_wallclock_gmt, JB_override_tickets, JB_version,
                         JB_ja_structure, JB_ja_n_h_ids, JB_ja_u_h_ids, JB_ja_s_h_ids, JB_ja_o_h_ids, JB_ja_a_h_ids,
                         JB_type, JB_binding, JB_ja_task_concurrency, JB_pty,
                         JB_grp_list, JB_sync_options, JB_category_id, JB_path_aliases, JB_ja_ad_predecessor_list,
                         JB_ja_ad_request_list, JB_ja_ad_successor_list>()
      .with<JB_ja_tasks>(ocs::tom::what<JAT_state, JAT_status, JAT_hold, JAT_task_number, JAT_scaled_usage_list,
                                        JAT_job_restarted, JAT_task_list, JAT_message_list, JAT_start_time,
                                        JAT_end_time, JAT_granted_resources_list,
                                        JAT_granted_destin_identifier_list>())
      .with<JB_jid_predecessor_list>(ocs::tom::what<JRE_job_number>())
      .with<JB_jid_successor_list>(ocs::tom::what<JRE_job_number>());
}

/** @brief Normalise parameter flags before any data is fetched.
 *
 * @param parameter the parsed parameters, adjusted in place
 */
void ocs::QStatModelBase::prepare_filter(QStatParameter &parameter) {
   // set the full_listing flags that will influence job/task filters
   apply_state_filter(parameter);
}

/** @brief Fetch raw CULL lists into the member variables.
 *
 * Overridden by QStatModelClient (GDI calls) and QStatModelServer (master lists).
 *
 * @param answer_list receives error messages
 * @param parameter the parsed parameters
 * @return true when everything could be fetched
 */
bool ocs::QStatModelBase::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   // has to be overridden by derived class
   DRETURN(true);
}

/** @brief Post-process fetched lists (e.g. resolve hostgroup references).
 *
 * For XML output of named jobs, the messages of the scheduler are reduced to those jobs. Without named jobs
 * nothing is reduced.
 *
 * @param answer_list receives error messages
 * @param parameter the parsed parameters
 * @return true when the lists could be prepared
 */
bool ocs::QStatModelBase::prepare_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   // report an error if response does not contain all information for the job view
   const auto jid_list = parameter.get_jid_list();
   if (jlp->empty() && !jid_list.empty()) {

      // remove all pattern
      bool removed_pattern = false;
      auto next_elem = jid_list.front();
      while (next_elem != nullptr) {
         const auto elem = next_elem;
         next_elem = elem.next();

         if (is_pattern(ocs::tom::get<ST_name>(elem))) {
            jid_list.erase(elem);
            removed_pattern = true;
         }
      }

      // if there is still something missing then report an error
      std::stringstream ss;
      if (!jid_list.empty()) {
         ss << MSG_QSTAT_FOLLOWINGDONOTEXIST;
         bool first_time = true;
         for (const auto elem : jid_list) {
            if (!first_time) {
               ss << ", ";
            }
            first_time = false;
            ss << ocs::tom::get<ST_name>(elem);
         }
      } else {
         if (removed_pattern) {
            ss << MSG_QSTAT_FOLLOWINGDONOTEXIST;
         }
      }
      answer_list_add(answer_list, ss.str().c_str(), STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // the job list exists only when jobs were named; without them (-j alone) every message is shown, as in plain
   if (parameter.get_output_format() != QStatParameter::OutputFormat::XML || jid_list == nullptr) {
      DRETURN(true);
   }

   /* filter the message list to contain only jobs that have been requested.
      First remove all entries in the job_number_list that are not in the
      jbList. Then remove all entries (job_number_list, message_number and
      message) from the message_list that have no jobs in them.
   */
   for (const auto sme : *ilp) {
      const auto msg_list = ocs::tom::get<SME_message_list>(sme);

      auto next_msg = msg_list.front();
      while (next_msg != nullptr) {
         const auto msg_elem = next_msg;
         next_msg = msg_elem.next();

         const auto jb_list = ocs::tom::get<MES_job_number_list>(msg_elem);
         auto next_jb = jb_list.front();
         while (next_jb != nullptr) {
            const auto jb_elem = next_jb;
            next_jb = jb_elem.next();
            if (jlp->find_first<JB_job_number>(ocs::tom::get<ULNG_value>(jb_elem)) == nullptr) {
               jb_list.erase(jb_elem);
            }
         }
         if (ocs::tom::get<MES_job_number_list>(msg_elem).empty()) {
            msg_list.erase(msg_elem);
         }
      }
   }
   DRETURN(true);
}

/** @brief Untag the queue instances that are not in one of the requested states
 *
 * Sets the load and suspend alarm of each instance first, so that `-qs a`
 * and `-qs A` see them.
 *
 * @param queue_states the states the user asked for
 * @param exechost_list the execution hosts, for the load values
 * @param queue_list the cluster queues, whose instances are tagged in place
 * @param centry_list the complex entries, to interpret the load
 * @param load_adjustments the job load adjustments of the scheduler configuration
 * @return 0
 */
int ocs::QStatModelBase::select_by_queue_state(const uint32_t queue_states,
                                               const ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                                               const ocs::tom::List<ocs::tom::CQ> queue_list,
                                               const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                                               const ocs::tom::ConstList<ocs::tom::CE> load_adjustments) {
   DENTER(TOP_LAYER);

   /* only show queues in the requested state */
   /* make it possible to display any load value in qstat output */
   const char *load_avg_str = getenv("SGE_LOAD_AVG");
   static const char *valid_load_attrs[] = {
      LOAD_ATTR_LOAD_AVG, LOAD_ATTR_LOAD_SHORT, LOAD_ATTR_LOAD_MEDIUM, LOAD_ATTR_LOAD_LONG,
      LOAD_ATTR_NP_LOAD_AVG, LOAD_ATTR_NP_LOAD_SHORT, LOAD_ATTR_NP_LOAD_MEDIUM, LOAD_ATTR_NP_LOAD_LONG,
      nullptr
   };
   bool valid = false;
   for (int i = 0; valid_load_attrs[i] != nullptr; i++) {
      if (load_avg_str != nullptr && strcmp(load_avg_str, valid_load_attrs[i]) == 0) {
         valid = true;
         break;
      }
   }
   if (!valid) {
      load_avg_str = LOAD_ATTR_LOAD_AVG;
   }

   for (const auto cqueue : queue_list) {
      for (const auto qep : ocs::tom::get<CQ_qinstances>(cqueue)) {
         bool has_value_from_object;
         double load_avg;
         uint32_t interval;

         /* compute the load and suspend alarm */
         sge_get_double_qattr(&load_avg, load_avg_str, qep, exechost_list, centry_list, load_adjustments,
                              &has_value_from_object);
         if (sge_load_alarm(nullptr, 0, qep, ocs::tom::get<QU_load_thresholds>(qep), exechost_list, centry_list,
                            ocs::tom::ConstList<ocs::tom::CE>(), load_adjustments, true)) {
            qinstance_state_set_alarm(qep, true);
         }
         parse_ulong_val(nullptr, &interval, ocs::CEntry::Type::TIME, ocs::tom::get<QU_suspend_interval>(qep),
                         nullptr, 0);
         if (ocs::tom::get<QU_nsuspend>(qep) != 0 && interval != 0 &&
             sge_load_alarm(nullptr, 0, qep, ocs::tom::get<QU_suspend_thresholds>(qep), exechost_list, centry_list,
                            ocs::tom::ConstList<ocs::tom::CE>(), load_adjustments, false)) {
            qinstance_state_set_suspend_alarm(qep, true);
         }

         if (!qinstance_has_state(qep, queue_states)) {
            ocs::tom::set<QU_tag>(qep, 0);
         }
      }
   }
   DRETURN(0);
}

/** @brief Tag the queue instances named by the requested queue references
 * @param cqueue_list the cluster queues, tagged in place
 * @param hgrp_list the host groups, to resolve `@group` references
 * @param qref_list the queue references the user gave
 * @return the number of queue instances that matched, -1 if a reference matches nothing
 */
int ocs::QStatModelBase::select_by_qref_list(const ocs::tom::List<ocs::tom::CQ> cqueue_list,
                                             const ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list,
                                             const ocs::tom::ConstList<ocs::tom::QR> qref_list) {
   DENTER(TOP_LAYER);
   int ret = 0;

   // Resolve queue pattern
   lList *resolved = nullptr;
   bool found_something = true;
   {
      const auto queueref_copy = qref_list.clone("");
      qref_list_resolve(*queueref_copy, nullptr, &resolved, &found_something, cqueue_list, hgrp_list, true, true);
   }
   const auto queueref_list = ocs::tom::ListPtr<ocs::tom::QR>::from_typed(resolved);
   if (!found_something) {
      DRETURN(-1);
   }

   if (cqueue_list != nullptr && queueref_list != nullptr) {
      for (const auto qref : *queueref_list) {
         dstring cqueue_buffer = DSTRING_INIT;
         dstring hostname_buffer = DSTRING_INIT;

         if (const char *full_name = ocs::tom::get<QR_name>(qref);
             cqueue_name_split(full_name, &cqueue_buffer, &hostname_buffer, nullptr, nullptr)) {
            const char *cqueue_name = sge_dstring_get_string(&cqueue_buffer);
            const char *hostname = sge_dstring_get_string(&hostname_buffer);

            if (const auto cqueue = cqueue_list.find_first<CQ_name>(cqueue_name); cqueue != nullptr) {
               const auto qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);
               if (const auto qinstance = qinstance_list.find_first<QU_qhostname>(ocs::tom::Host(hostname));
                   qinstance != nullptr) {
                  const uint32_t tag = ocs::tom::get<QU_tag>(qinstance);
                  ocs::tom::set<QU_tag>(qinstance, tag | TAG_SELECT_IT);
               }
            }
         }

         sge_dstring_free(&cqueue_buffer);
         sge_dstring_free(&hostname_buffer);
      }

      for (const auto cqueue : cqueue_list) {
         for (const auto qinstance : ocs::tom::get<CQ_qinstances>(cqueue)) {
            uint32_t tag = ocs::tom::get<QU_tag>(qinstance);

            if (const bool selected = (tag & TAG_SELECT_IT) != 0; !selected) {
               tag &= ~(TAG_SELECT_IT | TAG_SHOW_IT);
               ocs::tom::set<QU_tag>(qinstance, tag);
            } else {
               ret++;
            }
         }
      }
   }

   DRETURN(ret);
}

/** @brief Drop the jobs the user did not ask about and prepare the queues for the output
 *
 * For the `-g c` listing the cluster queues are sorted by name. For every
 * other listing the queue instances are moved out of their cluster queues
 * into #qinstance_list_, sorted by sequence number, queue and host, and the
 * cluster queues are freed.
 *
 * @param parameter the parsed parameters
 * @return none
 */
void ocs::QStatModelBase::filter_jobs(const QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   const auto job_list = *job_list_;

   // select all jobs which are not finished
   for (const auto jep : job_list) {
      for (const auto jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
         if (!(ocs::tom::get<JAT_status>(jatep) & JFINISHED))
            ocs::tom::set<JAT_suitable>(jatep, TAG_SHOW_IT);
      }
   }

   // untag all jobs which do not fit to the user list (-u)
   if (!parameter.get_user_list().empty()) {
      for (const auto up : parameter.get_user_list()) {
         const char *user = ocs::tom::get<ST_name>(up);
         if (user == nullptr) {
            break;
         }

         const bool is_pattern = ocs::is_pattern(user);
         for (const auto jep : job_list) {
            const char *owner = ocs::tom::get<JB_owner>(jep);

            int match;
            if (is_pattern) {
               match = fnmatch(user, owner, 0);
            } else {
               match = sge_strnullcmp(user, owner);
            }
            if (match == 0) {
               for (const auto jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
                  ocs::tom::set<JAT_suitable>(jatep,
                                              ocs::tom::get<JAT_suitable>(jatep) | TAG_SHOW_IT | TAG_SELECT_IT);
               }
            }
         }
      }
   }

   // untag all jobs which do not fit to the queue selection (-pe -l -q -U)
   if (!parameter.get_pe_ref_list().empty() || !parameter.get_queue_ref_list().empty() ||
       !parameter.get_resource_list().empty() || !parameter.get_queue_user_list().empty()) {

      // @todo Will this call cause an issue if executed in the reader thread pool
      // do not debit for running jobs
      sconf_set_qs_state(QS_STATE_EMPTY);

      // unselect all pending jobs that fit in none of the selected queues
      for (const auto jep : job_list) {
         bool show_job = false;
         for (const auto cqueue : *cqueue_list_) {
            for (const auto qep : ocs::tom::get<CQ_qinstances>(cqueue)) {

               if (!(ocs::tom::get<QU_tag>(qep) & TAG_SHOW_IT)) {
                  continue;
               }

               // find if the queue instance is in the list of selected queues and if the job fits into this queue
               // instance
               if (const auto host = ocs::tom::ConstElement<ocs::tom::EH>::from_typed(
                      host_list_locate(*exechost_list_, ocs::tom::get<QU_qhostname>(qep)));
                   host != nullptr) {
                  const auto hard_resources =
                     ocs::tom::List<ocs::tom::CE>::from_typed(job_get_hard_resource_listRW(jep));
                  const int ret = sge_select_queue(hard_resources, qep, host, *exechost_list_, *centry_list_, true, 1,
                                                   parameter.get_queue_user_list(), *acl_list_, jep);

                  if (ret==1) {
                     show_job = true;
                     break;
                  }
               }
            }
         }

         for (const auto jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
            if (!show_job && !(ocs::tom::get<JAT_status>(jatep) == JRUNNING ||
                               (ocs::tom::get<JAT_status>(jatep) == JTRANSFERING))) {
               DPRINTF("show task " sge_u32"." sge_u32"\n", ocs::tom::get<JB_job_number>(jep),
                       ocs::tom::get<JAT_task_number>(jatep));
               ocs::tom::set<JAT_suitable>(jatep, ocs::tom::get<JAT_suitable>(jatep) & ~TAG_SHOW_IT);
            }
         }
         if (!show_job) {
            ocs::tom::set<JB_ja_n_h_ids>(jep, nullptr);
            ocs::tom::set<JB_ja_u_h_ids>(jep, nullptr);
            ocs::tom::set<JB_ja_o_h_ids>(jep, nullptr);
            ocs::tom::set<JB_ja_s_h_ids>(jep, nullptr);
         }
      }

      // @todo Will this call cause an issue if executed in the reader thread pool
      // re-enable debit for running jobs
      sconf_set_qs_state(QS_STATE_FULL);
   }

   // prepare queues for output
   if (parameter.output_mode_== QStatParameter::OutputMode::QSTAT_GROUP) {

      // sort cluster queues for grouped output
      cqueue_list_->sort<ocs::tom::asc<CQ_name>>();
   } else {

      // cluster queues are not required. We need to reconstruct the queue list with the queue instances only.
      auto qinstance_list = ocs::tom::ListPtr<ocs::tom::QU>::create("");
      for (const auto cqueue : *cqueue_list_) {
         lList *qinstances = nullptr;
         ocs::tom::exchange<CQ_qinstances>(cqueue, &qinstances);
         qinstance_list->splice(ocs::tom::ListPtr<ocs::tom::QU>::from_typed(qinstances));
      }
      cqueue_list_.reset();
      qinstance_list_ = std::move(qinstance_list);

      // sort queue instances
      qinstance_list_->sort<ocs::tom::asc<QU_seq_no>, ocs::tom::asc<QU_qname>, ocs::tom::asc<QU_qhostname>>();
   }
   DRETURN_VOID;
}

/** @brief Drop the queue instances the user did not ask about
 * @param answer_list receives error messages
 * @param parameter the parsed parameters
 * @return 1 when queue instances remain, 0 when none do, -1 on an error
 */
int ocs::QStatModelBase::filter_queues(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                       const QStatParameter &parameter) const {
   DENTER(TOP_LAYER);

   centry_list_init_double(*centry_list_);

   // all queues are selected
   cqueue_list_set_tag(*cqueue_list_, TAG_SHOW_IT, true);

   // unselect all queues not selected by a -q
   if (!parameter.get_queue_ref_list().empty()) {
      const int count = select_by_qref_list(*cqueue_list_, *hgrp_list_, parameter.get_queue_ref_list());

      if (count < 0) {
         DRETURN(-1);
      }

      if (count == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-q");
         DRETURN(0);
      }
   }

   // unselect all queues not selected by -qs
   select_by_queue_state(parameter.queue_state_, *exechost_list_, *cqueue_list_, *centry_list_,
                         *job_load_adjustments_);

   // unselect all queues not selected by a -U (if exist)
   if (!parameter.get_queue_user_list().empty()) {
      const int count = select_by_queue_user_list(*exechost_list_, *cqueue_list_, parameter.get_queue_user_list(),
                                                  *acl_list_, *project_list_, *config_user_lists_,
                                                  *config_xuser_lists_);

      if (count < 0) {
         DRETURN(-1);
      }

      if (count == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-U");
         DRETURN(0);
      }
   }

   // unselect all queues not selected by a -pe (if exist)
   if (!parameter.get_pe_ref_list().empty()) {
      const int count = select_by_pe_list(*cqueue_list_, parameter.get_pe_ref_list(), *pe_list_);

      if (count < 0) {
         DRETURN(-1);
      }

      if (count == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S, "-pe");
         DRETURN(0);
      }
   }

   // unselect all queues not selected by a -l
   if (!parameter.get_resource_list().empty()) {
      const int count = select_by_resource_list(parameter.get_resource_list(), *exechost_list_, *cqueue_list_,
                                                *centry_list_, 1);
      if (count < 0) {
         DRETURN(-1);
      }
   }

   if (!is_cqueue_selected(*cqueue_list_)) {
      DRETURN(0);
   }

   DRETURN(1);
}

/** @brief Whether any cluster queue survived the tagging
 *
 * A cluster queue none of whose instances is tagged any more is untagged too.
 *
 * @param queue_list the cluster queues
 * @return true when at least one is still selected
 */
bool ocs::QStatModelBase::is_cqueue_selected(const ocs::tom::List<ocs::tom::CQ> queue_list) {
   DENTER(TOP_LAYER);
   bool a_cqueue_is_selected = false;

   //bool a_qinstance_is_selected = false;
   for (const auto cqueue : queue_list) {
      const auto qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);
      bool tmp_a_qinstance_is_selected = false;

      for (const auto qep : qinstance_list) {
         if (ocs::tom::get<QU_tag>(qep) & TAG_SHOW_IT) {
            tmp_a_qinstance_is_selected = true;
            break;
         }
      }
      //a_qinstance_is_selected |= tmp_a_qinstance_is_selected;
      if (!tmp_a_qinstance_is_selected && !ocs::tom::get<CQ_qinstances>(cqueue).empty()) {
         ocs::tom::set<CQ_tag>(cqueue, TAG_DEFAULT);
      } else {
         a_cqueue_is_selected |= true;
      }
   }

   DRETURN(a_cqueue_is_selected);
}

/** @brief Untag the queue instances that do not satisfy the requested resources
 * @param resource_list the resources the user asked for, completed in place from the complex entries
 * @param exechost_list the execution hosts
 * @param queue_list the cluster queues, whose instances are tagged in place
 * @param centry_list the complex entries
 * @param empty_qs whether the queue instances are judged as if they were empty
 * @return 0 when the instances were untagged as necessary, -1 if the request is invalid
 */
int ocs::QStatModelBase::select_by_resource_list(const ocs::tom::List<ocs::tom::CE> resource_list,
                                                 const ocs::tom::List<ocs::tom::EH> exechost_list,
                                                 const ocs::tom::List<ocs::tom::CQ> queue_list,
                                                 const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                                                 const uint32_t empty_qs) {
   DENTER(TOP_LAYER);

   if (centry_list_fill_request(resource_list, nullptr, centry_list, true, true, false)) {
      /*
      ** error message gets written by centry_list_fill_request into
      ** SGE_EVENT
      */
      DRETURN(-1);
   }

   /* prepare request */
   for (const auto cqueue : queue_list) {
      for (const auto qep : ocs::tom::get<CQ_qinstances>(cqueue)) {
         if (empty_qs) {
            sconf_set_qs_state(QS_STATE_EMPTY);
         }

         const bool selected = sge_select_queue(resource_list, qep, ocs::tom::ConstElement<ocs::tom::EH>(),
                                                exechost_list, centry_list, true, -1,
                                                ocs::tom::ConstList<ocs::tom::ST>(),
                                                ocs::tom::ConstList<ocs::tom::US>(),
                                                ocs::tom::ConstElement<ocs::tom::JB>());
         if (empty_qs) {
            sconf_set_qs_state(QS_STATE_FULL);
         }

         if (!selected) {
            DPRINTF("unselecting queue " SFQ "\n", ocs::tom::get<QU_full_name>(qep));
            ocs::tom::set<QU_tag>(qep, 0);
         }
      }
   }

   DRETURN(0);
}

/** @brief Untag the queue instances that offer none of the requested parallel environments
 *
 * Non-parallel queue instances are untagged in any case.
 *
 * @param queue_list the cluster queues, whose instances are tagged in place
 * @param peref_list the parallel environments the user asked for
 * @param pe_list the configured parallel environments
 * @return the number of queue instances that matched, -1 if none of the requested ones exists
 */
int ocs::QStatModelBase::select_by_pe_list(const ocs::tom::List<ocs::tom::CQ> queue_list,
                                           const ocs::tom::ConstList<ocs::tom::ST> peref_list,
                                           const ocs::tom::ConstList<ocs::tom::PE> pe_list) {
   DENTER(TOP_LAYER);
   int nqueues = 0;

   /*
    * iterate through peref_list and build up a new pe_list
    * containing only those pe's referenced in peref_list;
    * a name that is not configured adds nothing
    */
   ocs::tom::ListPtr<ocs::tom::PE> pe_selected;
   for (const auto pe : peref_list) {
      const auto ref_pe = pe_list.find_first<PE_name>(ocs::tom::get<ST_name>(pe));
      if (ref_pe == nullptr) {
         continue;
      }
      if (pe_selected == nullptr) {
         // the selected PEs keep the reduced layout the PE list was fetched with
         pe_selected = ocs::tom::ListPtr<ocs::tom::PE>::create_like("", ref_pe);
      }
      pe_selected->push_back(ref_pe.clone());
   }
   if (pe_selected->empty()) {
      fprintf(stderr, "%s\n", MSG_PE_NOSUCHPARALLELENVIRONMENT);
      DRETURN(-1);
   }

   /*
    * untag all non-parallel queues and queues not referenced
    * by a pe in the selected pe list entry of a queue
    */
   for (const auto cqueue : queue_list) {
      for (const auto qep : ocs::tom::get<CQ_qinstances>(cqueue)) {
         ocs::tom::ConstElement<ocs::tom::ST> found;

         if (!qinstance_is_parallel_queue(qep)) {
            ocs::tom::set<QU_tag>(qep, 0);
            continue;
         }
         for (const auto pe : *pe_selected) {
            const char *pe_name = ocs::tom::get<PE_name>(pe);

            found = ocs::tom::get<QU_pe_list>(qep).find_first<ST_name>(pe_name);
            if (found != nullptr) {
               break;
            }
         }
         if (found == nullptr) {
            ocs::tom::set<QU_tag>(qep, 0);
         } else {
            nqueues++;
         }
      }
   }

   DRETURN(nqueues);
}

/** @brief Untag the queue instances that none of the requested users may run in
 *
 * A user may run in a queue instance when the access lists and projects of the
 * instance, of its host, of the global host and of the global configuration all
 * grant access. A name starting with `@` is a group.
 *
 * @param exechost_list the execution hosts
 * @param cqueue_list the cluster queues, whose instances are tagged in place
 * @param queue_user_list the users the user asked about
 * @param acl_list the access control lists
 * @param project_list the projects; with an empty handle the projects are not checked
 * @param config_acl the `user_lists` of the global configuration
 * @param config_xacl the `xuser_lists` of the global configuration
 * @return the number of queue instances that matched
 */
int ocs::QStatModelBase::select_by_queue_user_list(const ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                                                   const ocs::tom::List<ocs::tom::CQ> cqueue_list,
                                                   const ocs::tom::ConstList<ocs::tom::ST> queue_user_list,
                                                   const ocs::tom::ConstList<ocs::tom::US> acl_list,
                                                   const ocs::tom::ConstList<ocs::tom::PR> project_list,
                                                   const ocs::tom::ConstList<ocs::tom::US> config_acl,
                                                   const ocs::tom::ConstList<ocs::tom::US> config_xacl) {
   DENTER(TOP_LAYER);
   int nqueues = 0;
   ocs::tom::ConstList<ocs::tom::US> h_acl = nullptr;
   ocs::tom::ConstList<ocs::tom::US> h_xacl = nullptr;
   ocs::tom::ConstList<ocs::tom::US> global_acl = nullptr;
   ocs::tom::ConstList<ocs::tom::US> global_xacl = nullptr;
   ocs::tom::ConstList<ocs::tom::PR> h_prj = nullptr;
   ocs::tom::ConstList<ocs::tom::PR> h_xprj = nullptr;
   ocs::tom::ConstList<ocs::tom::PR> global_prj = nullptr;
   ocs::tom::ConstList<ocs::tom::PR> global_xprj = nullptr;

   /* untag all queues where no of the users has access */

   auto ehep = ocs::tom::ConstElement<ocs::tom::EH>::from_typed(host_list_locate(exechost_list, "global"));
   if (ehep != nullptr) {
      global_acl  = ocs::tom::get<EH_acl>(ehep);
      global_xacl = ocs::tom::get<EH_xacl>(ehep);
      global_prj  = ocs::tom::get<EH_prj>(ehep);
      global_xprj = ocs::tom::get<EH_xprj>(ehep);
   }

   for (const auto cqueue : cqueue_list) {
      for (const auto qep : ocs::tom::get<CQ_qinstances>(cqueue)) {
         int access = 0;

         const auto prj = ocs::tom::get<QU_projects>(qep);
         const auto xprj = ocs::tom::get<QU_xprojects>(qep);

         /* get exec host list element for current queue
            and its access lists */
         const char *host_name = ocs::tom::get<QU_qhostname>(qep);
         ehep = ocs::tom::ConstElement<ocs::tom::EH>::from_typed(host_list_locate(exechost_list, host_name));
         if (ehep != nullptr) {
            h_acl = ocs::tom::get<EH_acl>(ehep);
            h_xacl = ocs::tom::get<EH_xacl>(ehep);
            h_prj = ocs::tom::get<EH_prj>(ehep);
            h_xprj = ocs::tom::get<EH_xprj>(ehep);
         }

         for (const auto qu : queue_user_list) {
            int q_access = 0;
            int h_access = 0;
            int gh_access = 0;
            int conf_access = 0;

            const char *name = ocs::tom::get<ST_name>(qu);
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
               for (const auto pep : prj) {
                  if (const char *prj_name = ocs::tom::get<PR_name>(pep); prj_name != nullptr) {
                     if (const auto prj_elem = project_list.find_first<PR_name>(prj_name); prj_elem != nullptr) {
                        const auto prj_acl = ocs::tom::get<PR_acl>(prj_elem);
                        const auto prj_xacl = ocs::tom::get<PR_xacl>(prj_elem);
                        q_access &= (name[0] == '@')
                                    ? sge_has_access_(nullptr, &name[1], nullptr, prj_acl, prj_xacl, acl_list)
                                    : sge_has_access_(name, nullptr, nullptr, prj_acl, prj_xacl, acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               for (const auto pep : xprj) {
                  if (const char *prj_name = ocs::tom::get<PR_name>(pep); prj_name != nullptr) {
                     if (const auto prj_elem = project_list.find_first<PR_name>(prj_name); prj_elem != nullptr) {
                        const auto prj_acl = ocs::tom::get<PR_acl>(prj_elem);
                        const auto prj_xacl = ocs::tom::get<PR_xacl>(prj_elem);
                        q_access &= (name[0] == '@')
                                    ? !sge_has_access_(nullptr, &name[1], nullptr, prj_acl, prj_xacl, acl_list)
                                    : !sge_has_access_(name, nullptr, nullptr, prj_acl, prj_xacl, acl_list);
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
               for (const auto pep : h_prj) {
                  if (const char *prj_name = ocs::tom::get<PR_name>(pep); prj_name != nullptr) {
                     if (const auto prj_elem = project_list.find_first<PR_name>(prj_name); prj_elem != nullptr) {
                        const auto prj_acl = ocs::tom::get<PR_acl>(prj_elem);
                        const auto prj_xacl = ocs::tom::get<PR_xacl>(prj_elem);
                        q_access &= (name[0] == '@')
                                    ? sge_has_access_(nullptr, &name[1], nullptr, prj_acl, prj_xacl, acl_list)
                                    : sge_has_access_(name, nullptr, nullptr, prj_acl, prj_xacl, acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               for (const auto pep : h_xprj) {
                  if (const char *prj_name = ocs::tom::get<PR_name>(pep); prj_name != nullptr) {
                     if (const auto prj_elem = project_list.find_first<PR_name>(prj_name); prj_elem != nullptr) {
                        const auto prj_acl = ocs::tom::get<PR_acl>(prj_elem);
                        const auto prj_xacl = ocs::tom::get<PR_xacl>(prj_elem);
                        q_access &= (name[0] == '@')
                                    ? !sge_has_access_(nullptr, &name[1], nullptr, prj_acl, prj_xacl, acl_list)
                                    : !sge_has_access_(name, nullptr, nullptr, prj_acl, prj_xacl, acl_list);
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
            gh_access = (name[0] == '@')
                        ? sge_has_access_(nullptr, &name[1], nullptr, global_acl, global_xacl, acl_list)
                        : sge_has_access_(name, nullptr, nullptr, global_acl, global_xacl, acl_list);
            if (!gh_access) {
               DPRINTF("no access\n");
            } else {
               DPRINTF("ok\n");
            }
            if (project_list != nullptr) {
               DPRINTF("testing host projects lists\n");
               for (const auto pep : global_prj) {
                  if (const char *prj_name = ocs::tom::get<PR_name>(pep); prj_name != nullptr) {
                     if (const auto prj_elem = project_list.find_first<PR_name>(prj_name); prj_elem != nullptr) {
                        const auto prj_acl = ocs::tom::get<PR_acl>(prj_elem);
                        const auto prj_xacl = ocs::tom::get<PR_xacl>(prj_elem);
                        q_access &= (name[0] == '@')
                                    ? sge_has_access_(nullptr, &name[1], nullptr, prj_acl, prj_xacl, acl_list)
                                    : sge_has_access_(name, nullptr, nullptr, prj_acl, prj_xacl, acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               for (const auto pep : global_xprj) {
                  if (const char *prj_name = ocs::tom::get<PR_name>(pep); prj_name != nullptr) {
                     if (const auto prj_elem = project_list.find_first<PR_name>(prj_name); prj_elem != nullptr) {
                        const auto prj_acl = ocs::tom::get<PR_acl>(prj_elem);
                        const auto prj_xacl = ocs::tom::get<PR_xacl>(prj_elem);
                        q_access &= (name[0] == '@')
                                    ? !sge_has_access_(nullptr, &name[1], nullptr, prj_acl, prj_xacl, acl_list)
                                    : !sge_has_access_(name, nullptr, nullptr, prj_acl, prj_xacl, acl_list);
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
            DPRINTF("no access for queue %s\n", ocs::tom::get<QU_qname>(qep));
            ocs::tom::set<QU_tag>(qep, 0);
         } else {
            DPRINTF("access for queue %s\n", ocs::tom::get<QU_qname>(qep));
            nqueues++;
         }
      }
   }

   DRETURN(nqueues);
}

/** @brief Apply user-specified filters (queue state, resource, PE, queue ref …).
 *
 * @param alpp receives error messages
 * @param parameter the parsed parameters
 * @return true when the filters could be applied
 */
bool ocs::QStatModelBase::filter_data(ocs::tom::ListPtr<ocs::tom::AN> *alpp, QStatParameter &parameter) {
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

/** @brief Run the full pipeline: prepare_filter → fetch_data → prepare_data → filter_data.
 *
 * @param answer_list  Receives error messages on failure.
 * @param parameter    Parsed qstat parameters.
 * @return true if all pipeline steps succeeded.
 */
bool ocs::QStatModelBase::make_snapshot(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   prepare_filter(parameter);

   if (!fetch_data(answer_list, parameter)) {
      DRETURN(false);
   }
   if (!prepare_data(answer_list, parameter)) {
      DRETURN(false);
   }

   if (!filter_data(answer_list, parameter)) {
      DRETURN(false);
   }

   DRETURN(true);
}
