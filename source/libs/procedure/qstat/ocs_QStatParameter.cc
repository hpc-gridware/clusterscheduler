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


#include "ocs_ProcedureParameter.h"
#include "ocs_QStatParameter.h"

#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/** @brief Free the parameters
 *
 * The lists of the switches are owning handles and free themselves.
 */
ocs::QStatParameter::~QStatParameter() {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/** @brief Build the parameters on the server, from a received bundle
 * @param bundle the marshalled parameters
 * @param packet the originating client packet
 */
ocs::QStatParameter::QStatParameter(const ocs::tom::ConstList<ocs::tom::SPP> bundle, gdi::Packet *packet)
   : ProcedureParameter("", packet) {
   DENTER(TOP_LAYER);

   // initialize local member variables with data received from client
   QStatParameter::set_bundle(bundle);

   DRETURN_VOID;
}

/** @brief Fill the parameters from a received bundle
 *
 * The lists of the switches are taken out of the bundle, which arrived over
 * the network as untyped value lists; they are wrapped without a check of
 * their type, as the old code used them, because an exception here would
 * terminate qmaster.
 *
 * @param bundle the marshalled parameters; the switch lists are moved out of it
 * @return none
 */
void ocs::QStatParameter::set_bundle(const ocs::tom::ConstList<ocs::tom::SPP> bundle) {
   DENTER(TOP_LAYER);

   // initialize parents member variables
   ProcedureParameter::set_bundle(bundle);

   // flags for used switches: -F -q -q ...
   const auto show_param = bundle.find_first<SPP_name>(SHOW);
   const auto show_list = value_list<ocs::tom::ULNG>(show_param);
   show_ = show_list.front().get<ULNG_value>();
   DPRINTF("show_: " sge_u32 "\n", show_);

   // -explain bits
   const auto explain_param = bundle.find_first<SPP_name>(EXPLAIN_BITS);
   const auto explain_list = value_list<ocs::tom::ULNG>(explain_param);
   explain_bits_ = explain_list.front().get<ULNG_value>();
   DPRINTF("explain_bits_: " sge_u32 "\n", explain_bits_);

   // output mode
   const auto output_param = bundle.find_first<SPP_name>(OUTPUT_MODE);
   const auto output_list = value_list<ocs::tom::ULNG>(output_param);
   const auto raw_output_mode = output_list.front().get<ULNG_value>();
   output_mode_ = raw_output_mode <= static_cast<uint32_t>(OutputMode::JOB_INFO)
                  ? static_cast<OutputMode>(raw_output_mode)
                  : OutputMode::QSTAT_DEFAULT;
   DPRINTF("output_mode_: " sge_u32 "\n", static_cast<uint32_t>(output_mode_));

   // need queues
   const auto need_queues_param = bundle.find_first<SPP_name>(NEED_QUEUES);
   const auto need_queues_list = value_list<ocs::tom::ULNG>(need_queues_param);
   need_queues_ = need_queues_list.front().get<ULNG_value>() > 0;
   DPRINTF("need_queues_: " sge_u32 "\n", need_queues_);

   // need jobs
   const auto need_jobs_param = bundle.find_first<SPP_name>(NEED_JOBS);
   const auto need_jobs_list = value_list<ocs::tom::ULNG>(need_jobs_param);
   need_job_list_ = need_jobs_list.front().get<ULNG_value>() > 0;
   DPRINTF("need_jobs_: " sge_u32 "\n", need_job_list_);

   // state filter
   const auto state_filter_param = bundle.find_first<SPP_name>(STATE_FILTER);
   const auto state_filter_list = value_list<ocs::tom::ULNG>(state_filter_param);
   state_filter_ = state_filter_list.front().get<ULNG_value>();
   DPRINTF("state_filter_: " sge_u32 "\n", state_filter_);

   // state string
   const auto state_string_param = bundle.find_first<SPP_name>(STATE_STRING);
   const auto state_string_list = value_list<ocs::tom::ST>(state_string_param);

   const char *value = state_string_list.front().get<ST_name>();
   state_filter_value_ = value != nullptr ? value : "";
   DPRINTF("state_filter_value_: %s\n", state_filter_value_.c_str());

   // queue state
   const auto queue_state_param = bundle.find_first<SPP_name>(QUEUE_STATE);
   const auto queue_state_list = value_list<ocs::tom::ULNG>(queue_state_param);
   queue_state_ = queue_state_list.front().get<ULNG_value>();
   DPRINTF("queue_state_: " sge_u32 "\n", queue_state_);

   // group opt
   const auto group_opt_param = bundle.find_first<SPP_name>(GROUP_OPT);
   const auto group_opt_list = value_list<ocs::tom::ULNG>(group_opt_param);
   group_opt_ = group_opt_list.front().get<ULNG_value>();
   DPRINTF("group_opt_: " sge_u32 "\n", group_opt_);

   // SGE_QSTAT_LOAD_AVG (CS-2387) — resolved on the client, marshalled here.
   // Missing from older client bundles is fine; we keep the default in that case.
   const auto load_avg_variable_param = bundle.find_first<SPP_name>(LOAD_AVG_VARIABLE);
   if (load_avg_variable_param != nullptr) {
      const auto load_avg_variable_list = value_list<ocs::tom::ST>(load_avg_variable_param);
      const char *value = load_avg_variable_list.front().get<ST_name>();
      if (value != nullptr && *value != '\0') {
         load_avg_variable_ = value;
      }
   }
   DPRINTF("load_avg_variable_: %s\n", load_avg_variable_.c_str());

   // -l
   resource_list_ = take_value_list<ocs::tom::CE>(ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_LIST));
   DPRINTF("resource_list_: " sge_u32 " elements\n", static_cast<uint32_t>(resource_list_->size()));

   // -F
   q_resource_list_ = take_value_list<ocs::tom::CE>(ocs::tom::find_first_rw<SPP_name>(bundle, Q_RESOURCE_LIST));
   DPRINTF("q_resource_list_: " sge_u32 " elements\n", static_cast<uint32_t>(q_resource_list_->size()));

   // -u
   queue_ref_list_ = take_value_list<ocs::tom::QR>(ocs::tom::find_first_rw<SPP_name>(bundle, QUEUE_REF_LIST));
   DPRINTF("queue_ref_list_: " sge_u32 " elements\n", static_cast<uint32_t>(queue_ref_list_->size()));

   // -pe
   pe_ref_list_ = take_value_list<ocs::tom::ST>(ocs::tom::find_first_rw<SPP_name>(bundle, PE_REF_LIST));
   DPRINTF("pe_ref_list_: " sge_u32 " elements\n", static_cast<uint32_t>(pe_ref_list_->size()));

   // -u
   user_list_ = take_value_list<ocs::tom::ST>(ocs::tom::find_first_rw<SPP_name>(bundle, USER_LIST));
   DPRINTF("user_list_: " sge_u32 " elements\n", static_cast<uint32_t>(user_list_->size()));

   // -U
   queue_user_list_ = take_value_list<ocs::tom::ST>(ocs::tom::find_first_rw<SPP_name>(bundle, QUEUE_USER_LIST));
   DPRINTF("queue_user_list_: " sge_u32 " elements\n", static_cast<uint32_t>(queue_user_list_->size()));

   // -j
   jid_list_ = take_value_list<ocs::tom::ST>(ocs::tom::find_first_rw<SPP_name>(bundle, JID_LIST));
   DPRINTF("jid_list_: " sge_u32 " elements\n", static_cast<uint32_t>(jid_list_->size()));

   DRETURN_VOID;
}

/** @brief Marshal the parameters into a bundle for the server
 * @return the bundle, which the caller owns; the switch lists are copies
 */
ocs::tom::ListPtr<ocs::tom::SPP> ocs::QStatParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Get the name-value-list that was initialized by the base class with the procedure name
   auto bundle = ProcedureParameter::get_bundle();

   // -j
   DPRINTF("jid_list_: " sge_u32 " elements\n", static_cast<uint32_t>(jid_list_->size()));
   add_parameter_bundle(*bundle, JID_LIST, jid_list_->clone());

   // -U
   DPRINTF("queue_user_list_: " sge_u32 " element\n", static_cast<uint32_t>(queue_user_list_->size()));
   add_parameter_bundle(*bundle, QUEUE_USER_LIST, queue_user_list_->clone());

   // -u
   DPRINTF("user_list_: " sge_u32 " elements\n", static_cast<uint32_t>(user_list_->size()));
   add_parameter_bundle(*bundle, USER_LIST, user_list_->clone());

   // -pe
   DPRINTF("pr_ref_list_: " sge_u32 " elements\n", static_cast<uint32_t>(pe_ref_list_->size()));
   add_parameter_bundle(*bundle, PE_REF_LIST, pe_ref_list_->clone());

   // -u
   DPRINTF("queue_ref_list_: " sge_u32 " elements\n", static_cast<uint32_t>(queue_ref_list_->size()));
   add_parameter_bundle(*bundle, QUEUE_REF_LIST, queue_ref_list_->clone());

   // -F
   DPRINTF("q_resource_list_: " sge_u32 " elements\n", static_cast<uint32_t>(q_resource_list_->size()));
   add_parameter_bundle(*bundle, Q_RESOURCE_LIST, q_resource_list_->clone());

   // -l
   DPRINTF("resource_list_: " sge_u32 " elements\n", static_cast<uint32_t>(resource_list_->size()));
   add_parameter_bundle(*bundle, RESOURCE_LIST, resource_list_->clone());

   // output mode
   add_parameter_bundle(*bundle, OUTPUT_MODE, ulong_value_list(static_cast<uint32_t>(output_mode_)));
   DPRINTF("output_mode_=%u\n", static_cast<uint32_t>(output_mode_));

   // flags for used switches ...
   add_parameter_bundle(*bundle, SHOW, ulong_value_list(show_));
   DPRINTF("show_=%u\n", static_cast<uint32_t>(show_));

   // explain bits
   add_parameter_bundle(*bundle, EXPLAIN_BITS, ulong_value_list(explain_bits_));
   DPRINTF("explain_bits_=%u\n", static_cast<uint32_t>(explain_bits_));

   // need queues
   add_parameter_bundle(*bundle, NEED_QUEUES, ulong_value_list(need_queues_));
   DPRINTF("need_queues_=%s\n", need_queues_ ? "true" : "false");

   // need jobs
   add_parameter_bundle(*bundle, NEED_JOBS, ulong_value_list(need_job_list_));
   DPRINTF("need_jobs_=%s\n", need_job_list_ ? "true" : "false");

   // state filter
   add_parameter_bundle(*bundle, STATE_FILTER, ulong_value_list(state_filter_));
   DPRINTF("state_filter_=%u\n", static_cast<uint32_t>(state_filter_));

   // state string
   add_parameter_bundle(*bundle, STATE_STRING, string_value_list(state_filter_value_.c_str()));
   DPRINTF("state_filer_value_=%s (%p)\n", state_filter_value_.c_str(), state_filter_value_.c_str());

   // queue state
   add_parameter_bundle(*bundle, QUEUE_STATE, ulong_value_list(queue_state_));
   DPRINTF("queue_state_=%u\n", static_cast<uint32_t>(queue_state_));

   // group opt
   add_parameter_bundle(*bundle, GROUP_OPT, ulong_value_list(group_opt_));
   DPRINTF("group_opt_=%u\n", static_cast<uint32_t>(group_opt_));

   // SGE_QSTAT_LOAD_AVG (CS-2387)
   add_parameter_bundle(*bundle, LOAD_AVG_VARIABLE, string_value_list(load_avg_variable_.c_str()));
   DPRINTF("load_avg_variable_=%s\n", load_avg_variable_.c_str());

   DRETURN(bundle);
}



