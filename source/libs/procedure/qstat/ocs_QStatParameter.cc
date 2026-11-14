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

/** @file
 * @brief Parameters of `qstat`: what the call was asked to report
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/cull/sge_ulong_ULNG_L.h"

#include "ocs_ProcedureParameter.h"
#include "ocs_QStatParameter.h"

#include "sgeobj/cull/sge_str_ST_L.h"
#include "tom/ocs_Element.h"

ocs::QStatParameter::~QStatParameter() {
   DENTER(TOP_LAYER);
   // Data that was allocated
   lFreeList(&resource_list_);
   lFreeList(&q_resource_list_);
   lFreeList(&queue_ref_list_);
   lFreeList(&pe_ref_list_);
   lFreeList(&user_list_);
   lFreeList(&queue_user_list_);
   lFreeList(&jid_list_);

   DRETURN_VOID;
}

/** @brief Build the parameters on the server, from a received bundle
 * @param bundle the marshalled parameters
 * @param packet the originating client packet
 */
ocs::QStatParameter::QStatParameter(const lList *bundle, gdi::Packet *packet) : ProcedureParameter("", packet) {
   DENTER(TOP_LAYER);

   // initialize local member variables with data received from client
   QStatParameter::set_bundle(bundle);

   DRETURN_VOID;
}

void ocs::QStatParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   // initialize parents member variables
   ProcedureParameter::set_bundle(bundle);

   // flags for used switches: -F -q -q ...
   const lListElem *show_param = ocs::tom::find_first<SPP_name>(bundle, SHOW);
   const lList *show_list = ocs::tom::get<SPP_value_list>(show_param);
   show_ = ocs::tom::get<ULNG_value>(lFirst(show_list));
   DPRINTF("show_: " sge_u32 "\n", show_);

   // -explain bits
   const lListElem *explain_param = ocs::tom::find_first<SPP_name>(bundle, EXPLAIN_BITS);
   const lList *explain_list = ocs::tom::get<SPP_value_list>(explain_param);
   explain_bits_ = ocs::tom::get<ULNG_value>(lFirst(explain_list));
   DPRINTF("explain_bits_: " sge_u32 "\n", explain_bits_);

   // output mode
   const lListElem *output_param = ocs::tom::find_first<SPP_name>(bundle, OUTPUT_MODE);
   const lList *output_list = ocs::tom::get<SPP_value_list>(output_param);
   const auto raw_output_mode = ocs::tom::get<ULNG_value>(lFirst(output_list));
   output_mode_ = raw_output_mode <= static_cast<uint32_t>(OutputMode::JOB_INFO)
                  ? static_cast<OutputMode>(raw_output_mode)
                  : OutputMode::QSTAT_DEFAULT;
   DPRINTF("output_mode_: " sge_u32 "\n", static_cast<uint32_t>(output_mode_));

   // need queues
   const lListElem *need_queues_param = ocs::tom::find_first<SPP_name>(bundle, NEED_QUEUES);
   const lList *need_queues_list = ocs::tom::get<SPP_value_list>(need_queues_param);
   need_queues_ = ocs::tom::get<ULNG_value>(lFirst(need_queues_list)) > 0;
   DPRINTF("need_queues_: " sge_u32 "\n", need_queues_);

   // need jobs
   const lListElem *need_jobs_param = ocs::tom::find_first<SPP_name>(bundle, NEED_JOBS);
   const lList *need_jobs_list = ocs::tom::get<SPP_value_list>(need_jobs_param);
   need_job_list_ = ocs::tom::get<ULNG_value>(lFirst(need_jobs_list)) > 0;
   DPRINTF("need_jobs_: " sge_u32 "\n", need_job_list_);

   // state filter
   const lListElem *state_filter_param = ocs::tom::find_first<SPP_name>(bundle, STATE_FILTER);
   const lList *state_filter_list = ocs::tom::get<SPP_value_list>(state_filter_param);
   state_filter_ = ocs::tom::get<ULNG_value>(lFirst(state_filter_list));
   DPRINTF("state_filter_: " sge_u32 "\n", state_filter_);

   // state string
   const lListElem *state_string_param = ocs::tom::find_first<SPP_name>(bundle, STATE_STRING);
   const lList *state_string_list = ocs::tom::get<SPP_value_list>(state_string_param);

   const char *value = ocs::tom::get<ST_name>(lFirst(state_string_list));
   state_filter_value_ = value != nullptr ? value : "";
   DPRINTF("state_filter_value_: %s\n", state_filter_value_.c_str());

   // queue state
   const lListElem *queue_state_param = ocs::tom::find_first<SPP_name>(bundle, QUEUE_STATE);
   const lList *queue_state_list = ocs::tom::get<SPP_value_list>(queue_state_param);
   queue_state_ = ocs::tom::get<ULNG_value>(lFirst(queue_state_list));
   DPRINTF("queue_state_: " sge_u32 "\n", queue_state_);

   // group opt
   const lListElem *group_opt_param = ocs::tom::find_first<SPP_name>(bundle, GROUP_OPT);
   const lList *group_opt_list = ocs::tom::get<SPP_value_list>(group_opt_param);
   group_opt_ = ocs::tom::get<ULNG_value>(lFirst(group_opt_list));
   DPRINTF("group_opt_: " sge_u32 "\n", group_opt_);

   // SGE_QSTAT_LOAD_AVG (CS-2387) — resolved on the client, marshalled here.
   // Missing from older client bundles is fine; we keep the default in that case.
   const lListElem *load_avg_variable_param = ocs::tom::find_first<SPP_name>(bundle, LOAD_AVG_VARIABLE);
   if (load_avg_variable_param != nullptr) {
      const lList *load_avg_variable_list = ocs::tom::get<SPP_value_list>(load_avg_variable_param);
      const char *value = ocs::tom::get<ST_name>(lFirst(load_avg_variable_list));
      if (value != nullptr && *value != '\0') {
         load_avg_variable_ = value;
      }
   }
   DPRINTF("load_avg_variable_: %s\n", load_avg_variable_.c_str());

   // -l
   lListElem *resource_param = ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_LIST);
   resource_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(resource_param, &resource_list_);
   DPRINTF("resource_list_: " sge_u32 " elements\n", lGetNumberOfElem(resource_list_));

   // -F
   lListElem *q_resource_param = ocs::tom::find_first_rw<SPP_name>(bundle, Q_RESOURCE_LIST);
   q_resource_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(q_resource_param, &q_resource_list_);
   DPRINTF("q_resource_list_: " sge_u32 " elements\n", lGetNumberOfElem(q_resource_list_));

   // -u
   lListElem *queue_ref_param = ocs::tom::find_first_rw<SPP_name>(bundle, QUEUE_REF_LIST);
   queue_ref_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(queue_ref_param, &queue_ref_list_);
   DPRINTF("queue_ref_list_: " sge_u32 " elements\n", lGetNumberOfElem(queue_ref_list_));

   // -pe
   lListElem *pe_ref_param = ocs::tom::find_first_rw<SPP_name>(bundle, PE_REF_LIST);
   pe_ref_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(pe_ref_param, &pe_ref_list_);
   DPRINTF("pe_ref_list_: " sge_u32 " elements\n", lGetNumberOfElem(pe_ref_list_));

   // -u
   lListElem *user_param = ocs::tom::find_first_rw<SPP_name>(bundle, USER_LIST);
   user_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(user_param, &user_list_);
   DPRINTF("user_list_: " sge_u32 " elements\n", lGetNumberOfElem(user_list_));

   // -U
   lListElem *queue_user_param = ocs::tom::find_first_rw<SPP_name>(bundle, QUEUE_USER_LIST);
   queue_user_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(queue_user_param, &queue_user_list_);
   DPRINTF("queue_user_list_: " sge_u32 " elements\n", lGetNumberOfElem(queue_user_list_));

   // -j
   lListElem *jid_param = ocs::tom::find_first_rw<SPP_name>(bundle, JID_LIST);
   jid_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(jid_param, &jid_list_);
   DPRINTF("jid_list_: " sge_u32 " elements\n", lGetNumberOfElem(jid_list_));

   DRETURN_VOID;
}

lList *ocs::QStatParameter::get_bundle() {
   DENTER(TOP_LAYER);
   lListElem *ep = nullptr;

   // Get the name-value-list that was initialized by the base class with the procedure name
   lList *bundle =  ProcedureParameter::get_bundle();
   //lList *name_value_list = lGetListRW(lGetElemStrRW(bundle, SPP_name, NAME_VALUE_LIST), SPP_value_list);

   // -j
   DPRINTF("jid_list_: " sge_u32 " elements\n", lGetNumberOfElem(jid_list_));
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, JID_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, jid_list_));

   // -U
   DPRINTF("queue_user_list_: " sge_u32 " element\n", lGetNumberOfElem(queue_user_list_));
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, QUEUE_USER_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, queue_user_list_));

   // -u
   DPRINTF("user_list_: " sge_u32 " elements\n", lGetNumberOfElem(user_list_));
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, USER_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, user_list_));

   // -pe
   DPRINTF("pr_ref_list_: " sge_u32 " elements\n", lGetNumberOfElem(pe_ref_list_));
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, PE_REF_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, pe_ref_list_));

   // -u
   DPRINTF("queue_ref_list_: " sge_u32 " elements\n", lGetNumberOfElem(queue_ref_list_));
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, QUEUE_REF_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, queue_ref_list_));

   // -F
   DPRINTF("q_resource_list_: " sge_u32 " elements\n", lGetNumberOfElem(q_resource_list_));
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, Q_RESOURCE_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, q_resource_list_));

   // -l
   DPRINTF("resource_list_: " sge_u32 " elements\n", lGetNumberOfElem(resource_list_));
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, RESOURCE_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, resource_list_));

   // output mode
   lList *output_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&output_list, static_cast<uint32_t>(output_mode_));
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, OUTPUT_MODE);
   ocs::tom::set<SPP_value_list>(ep, output_list);
   DPRINTF("output_mode_=%u\n", static_cast<uint32_t>(output_mode_));

   // flags for used switches ...
   lList *show_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&show_list, show_);
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, SHOW);
   ocs::tom::set<SPP_value_list>(ep, show_list);
   DPRINTF("show_=%u\n", static_cast<uint32_t>(show_));

   // explain bits
   lList *explain_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&explain_list, explain_bits_);
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, EXPLAIN_BITS);
   ocs::tom::set<SPP_value_list>(ep, explain_list);
   DPRINTF("explain_bits_=%u\n", static_cast<uint32_t>(explain_bits_));

   // need queues
   lList *need_queues_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&need_queues_list, need_queues_);
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, NEED_QUEUES);
   ocs::tom::set<SPP_value_list>(ep, need_queues_list);
   DPRINTF("need_queues_=%s\n", need_queues_ ? "true" : "false");

   // need jobs
   lList *need_jobs_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&need_jobs_list, need_job_list_);
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, NEED_JOBS);
   ocs::tom::set<SPP_value_list>(ep, need_jobs_list);
   DPRINTF("need_jobs_=%s\n", need_job_list_ ? "true" : "false");

   // state filter
   lList *state_filter_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&state_filter_list, state_filter_);
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, STATE_FILTER);
   ocs::tom::set<SPP_value_list>(ep, state_filter_list);
   DPRINTF("state_filter_=%u\n", static_cast<uint32_t>(state_filter_));

   // state string
   lList *state_filter_value_list = nullptr;
   ocs::tom::emplace_back<ST_name>(&state_filter_value_list, state_filter_value_.c_str());
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, STATE_STRING);
   ocs::tom::set<SPP_value_list>(ep, state_filter_value_list);
   DPRINTF("state_filer_value_=%s (%p)\n", state_filter_value_.c_str(), state_filter_value_.c_str());

   // queue state
   lList *queue_state_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&queue_state_list, queue_state_);
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, QUEUE_STATE);
   ocs::tom::set<SPP_value_list>(ep, queue_state_list);
   DPRINTF("queue_state_=%u\n", static_cast<uint32_t>(queue_state_));

   // group opt
   lList *group_opt_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&group_opt_list, group_opt_);
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, GROUP_OPT);
   ocs::tom::set<SPP_value_list>(ep, group_opt_list);
   DPRINTF("group_opt_=%u\n", static_cast<uint32_t>(group_opt_));

   // SGE_QSTAT_LOAD_AVG (CS-2387)
   lList *load_avg_variable_list = nullptr;
   ocs::tom::emplace_back<ST_name>(&load_avg_variable_list, load_avg_variable_.c_str());
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, LOAD_AVG_VARIABLE);
   ocs::tom::set<SPP_value_list>(ep, load_avg_variable_list);
   DPRINTF("load_avg_variable_=%s\n", load_avg_variable_.c_str());

   DRETURN(bundle);
}



