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
 * @brief Base model of `qhost`: the lists the report is built from
 */

#include "uti/ocs_Pattern.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"
#include "uti/sge_hostname.h"

#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_userset.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"
#include "sgeobj/tom/ocs_tom_JAT.h"

#include "sched/sge_select_queue.h"

#include "qstat/ocs_QStatParameter.h"
#include "ocs_QHostModelBase.h"
#include "sgeobj/sge_answer.h"


/** @brief Fetch raw CULL lists into the member variables.
 *
 * Overridden by QHostModelClient (GDI calls) and QHostModelServer (master lists).
 *
 * @param answer_list receives error messages
 * @param hostname_list the hosts the user asked for; all hosts when empty
 * @param user_name_list the users the user asked for; all users when empty
 * @param show which of queues, jobs and resources were requested
 * @return true when everything could be fetched
 */
bool ocs::QHostModelBase::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                     const ocs::tom::ConstList<ocs::tom::ST> hostname_list,
                                     const ocs::tom::ConstList<ocs::tom::ST> user_name_list, const uint32_t show) {
   DENTER(TOP_LAYER);
   // Nothing common to do
   DRETURN(true);
}

/** @brief Post-process fetched lists (e.g. resolve hostgroup references).
 *
 * @param answer_list receives error messages
 * @param[in,out] resource_match_list the resource filter the user gave; its entries are filled in place for
 *        filter_data()
 * @param show which of queues, jobs and resources were requested
 * @return true when the lists could be prepared
 */
bool ocs::QHostModelBase::prepare_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                       const ocs::tom::List<ocs::tom::CE> resource_match_list,
                                       const uint32_t show) const {
   DENTER(TOP_LAYER);
   // tag jobs for printing if they should be shown
   if ((show & QHOST_DISPLAY_JOBS) == QHOST_DISPLAY_JOBS) {
      for (const auto job : *job_list_) {
         for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
            ocs::tom::set<JAT_suitable>(ja_task, TAG_SHOW_IT);
         }
      }
   }

   // init steps required only if there is a resource match request (-l)
   if (!resource_match_list.empty()) {

      // prepare complex attribute list for matching
      centry_list_init_double(*centry_list_);
      if (centry_list_fill_request(resource_match_list, answer_list, *centry_list_, true, true, false)) {
         DRETURN(false);
      }

      // initialize tag field for hosts
      for (const auto host : *exec_host_list_) {
         ocs::tom::set<EH_tagged>(host, 0);
      }
   }
   DRETURN(true);
}

/** @brief Apply the resource-match filter to the exec host list.
 *
 * Only the hosts that match the filter stay in the list; a list left empty
 * is freed, as lSplit() freed it.
 *
 * @param resource_match_list the resource filter the user gave
 * @return none
 */
void ocs::QHostModelBase::filter_data(const ocs::tom::ConstList<ocs::tom::CE> resource_match_list) {
   DENTER(TOP_LAYER);

   // early exit if there is no resource match request
   if (resource_match_list.empty()) {
      DRETURN_VOID;
   }

   // we will modify the resource_match_list, so we need to work on a copy of it
   const auto tmp_resource_list = resource_match_list.clone();
   const auto global = exec_host_list_->find_first<EH_name>(ocs::tom::Host(SGE_GLOBAL_NAME));
   bool is_selected = sge_select_queue(*tmp_resource_list, ocs::tom::ConstElement<ocs::tom::QU>(), global,
                                       *exec_host_list_, *centry_list_, true, -1,
                                       ocs::tom::ConstList<ocs::tom::ST>(), ocs::tom::ConstList<ocs::tom::US>(),
                                       ocs::tom::ConstElement<ocs::tom::JB>());
   if (is_selected) {
      for (const auto host : *exec_host_list_) {
         ocs::tom::set<EH_tagged>(host, 1);
      }
   } else {
      // If there is hostname request, remove it as we cannot match hostname in sge_select_queue()
      // We will process this tmp_resource_list separately!
      ocs::tom::ElementPtr<ocs::tom::CE> host_match_elem;
      if (const auto host_match = tmp_resource_list->find_first<CE_name>("hostname"); host_match != nullptr) {
         host_match_elem = tmp_resource_list->extract(host_match);
      }

      for (const auto host : *exec_host_list_) {
         const char *hostname = ocs::tom::get<EH_name>(host);

         // try to find a matching attribute
         is_selected = sge_select_queue(*tmp_resource_list, ocs::tom::ConstElement<ocs::tom::QU>(), host,
                                        *exec_host_list_, *centry_list_, true, -1,
                                        ocs::tom::ConstList<ocs::tom::ST>(), ocs::tom::ConstList<ocs::tom::US>(),
                                        ocs::tom::ConstElement<ocs::tom::JB>());
         if (is_selected) {
            // this host is selected, tag it for output
            ocs::tom::set<EH_tagged>(host, 1);

            // if there is hostname request, check it here as sge_select_queue cannot handle it
            if (host_match_elem != nullptr &&
                sge_hostcmp(ocs::tom::get<CE_stringval>(*host_match_elem), hostname) != 0) {
               ocs::tom::set<EH_tagged>(host, 0);
            }
         } else {
            ocs::tom::set<EH_tagged>(host, 0);
         }
      }
   }

   // Reduce the hostlist, only the tagged ones survive; the others are freed with the list taken out
   {
      const auto untagged = exec_host_list_->split_off(ocs::tom::field<EH_tagged>() == 1u);
   }
   if (exec_host_list_ != nullptr && exec_host_list_->empty()) {
      exec_host_list_.reset();
   }

   DRETURN_VOID;
}

/** @brief Sort the exec host list for deterministic view output: the global host first, then by name
 * @return none
 */
void ocs::QHostModelBase::sort_data() {
   DENTER(TOP_LAYER);

   // Find and de-chain the global host
   ocs::tom::ElementPtr<ocs::tom::EH> global_host;
   if (const auto global = exec_host_list_->find_first<EH_name>(ocs::tom::Host(SGE_GLOBAL_NAME)); global != nullptr) {
      global_host = exec_host_list_->extract(global);
   }

   // Sort remaining elements alphabetically
   exec_host_list_->sort<ocs::tom::asc<EH_name>>();

   // Re-insert global host at the beginning
   if (global_host != nullptr) {
      exec_host_list_->push_front(std::move(global_host));
   }

   DRETURN_VOID;
}

/** @brief Run the full pipeline: fetch_data → prepare_data → filter_data → sort_data.
 *
 * @param answer_list  Receives error messages on failure.
 * @param parameter    Parsed qhost parameters.
 * @return true if all pipeline steps succeeded.
 */
bool
ocs::QHostModelBase::make_snapshot(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QHostParameter &parameter) {
   DENTER(TOP_LAYER);

   const auto hostname_list = parameter.get_hostname_list();
   const auto user_name_list = parameter.get_user_name_list();
   const auto resource_match_list = parameter.get_resource_match_list_rw();
   const uint32_t show = parameter.get_show();

   // fetch data from qmaster
   if (!fetch_data(answer_list, hostname_list, user_name_list, show)) {
      DRETURN(false);
   }

   // init raw data received from qmaster
   if (!prepare_data(answer_list, resource_match_list, show)) {
      DRETURN(false);
   }

   // filter and sort data for output
   filter_data(resource_match_list);
   sort_data();

   DRETURN(true);
}

/** @brief Returns a hostname filter.
 *
 * @param hostname_list the hosts the user asked for
 * @return the condition: the hosts named and the global host, never the template host
 */
ocs::tom::Where<ocs::tom::EH>
ocs::QHostModelBase::get_host_where(const ocs::tom::ConstList<ocs::tom::ST> hostname_list) {
   DENTER(TOP_LAYER);
   ocs::tom::Where<ocs::tom::EH> where;

   for (const auto host : hostname_list) {
      where |= ocs::tom::field<EH_name>().same_host(ocs::tom::get<ST_name>(host));
   }

   // without host request from the user we include the global host, too.
   if (where) {
      where |= ocs::tom::field<EH_name>() == SGE_GLOBAL_NAME;
   }

   // But we do not want the template host
   where &= ocs::tom::field<EH_name>() != SGE_TEMPLATE_NAME;

   DRETURN(where);
}

/** @brief The execution host fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::EH> ocs::QHostModelBase::get_host_what() {
   return ocs::tom::what_all<ocs::tom::EH>();
}

/** @brief The cluster queue fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::CQ> ocs::QHostModelBase::get_queue_what() {
   return ocs::tom::what_all<ocs::tom::CQ>();
}

/** @brief Build the condition selecting the jobs to report
 * @param user_name_list the users the user asked for; all users when empty
 * @param show which of queues, jobs and resources were requested
 * @return the condition; empty when neither users nor a state restrict the jobs
 */
ocs::tom::Where<ocs::tom::JB> ocs::QHostModelBase::get_job_where(const ocs::tom::ConstList<ocs::tom::ST> user_name_list,
                                                                 const uint32_t show) {
   DENTER(TOP_LAYER);
   ocs::tom::Where<ocs::tom::JB> where;

   for (const auto user : user_name_list) {
      if (const char *user_name = ocs::tom::get<ST_name>(user); is_pattern(user_name)) {
         where |= ocs::tom::field<JB_owner>().like(user_name);
      } else {
         where |= ocs::tom::field<JB_owner>() == user_name;
      }
   }

   if (!(show & QSTAT_DISPLAY_PENDING)) {
      where &= ocs::tom::field<JB_ja_tasks>().any(!ocs::tom::field<JAT_state>().has_bits(JQUEUED));
   }

   DRETURN(where);
}

/** @brief The job fields the model needs
 * @return the selection
 */
ocs::tom::What<ocs::tom::JB> ocs::QHostModelBase::get_job_what() {
   return ocs::tom::what<JB_job_number, JB_script_file, JB_owner, JB_group, JB_type,
                         JB_pe, JB_checkpoint_name, JB_jid_predecessor_list, JB_env_list, JB_priority,
                         JB_jobshare, JB_job_name, JB_project, JB_department, JB_submission_time,
                         JB_deadline, JB_override_tickets, JB_pe_range, JB_request_set_list, JB_ja_structure,
                         JB_ja_tasks, JB_ja_n_h_ids, JB_ja_u_h_ids, JB_ja_s_h_ids, JB_ja_o_h_ids,
                         JB_ja_a_h_ids>();
}

/** @brief The complex entry fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::CE> ocs::QHostModelBase::get_centry_what() {
   return ocs::tom::what_all<ocs::tom::CE>();
}

/** @brief The parallel environment fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::PE> ocs::QHostModelBase::get_pe_what() {
   return ocs::tom::what_all<ocs::tom::PE>();
}

/** @brief The user set fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::US> ocs::QHostModelBase::get_user_set_what() {
   return ocs::tom::what_all<ocs::tom::US>();
}

