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

#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/cull/sge_ulong_ULNG_L.h"

#include "ocs_ProcedureParameter.h"
#include "ocs_QStatParameter.h"

#include "symbols.h"
#include "sgeobj/cull/sge_str_ST_L.h"

ocs::QStatParameter::~QStatParameter() {
   lFreeList(&resource_list_);
   lFreeList(&q_resource_list_);
   lFreeList(&queue_ref_list_);
   lFreeList(&pe_ref_list_);
   lFreeList(&user_list_);
   lFreeList(&queue_user_list_);
   lFreeList(&jid_list_);
}

ocs::QStatParameter::QStatParameter(lList **bundle) : ProcedureParameter("") {
   DENTER(TOP_LAYER);

   // initialize local member variables
   ocs::QStatParameter::set_bundle(*bundle);

   // free the bundle
   lFreeList(bundle);

   DRETURN_VOID;
}

void ocs::QStatParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   // initialize parents member variables
   ProcedureParameter::set_bundle(bundle);

   // flags for used switches: -F -q -q ...
   const lListElem *show_param = lGetElemStr(bundle, SPP_name, SHOW);
   const lList *show_list = lGetList(show_param, SPP_value_list);
   show_ = lGetUlong(lFirst(show_list), ULNG_value);
   DPRINTF("show_: " sge_u32 "\n", show_);

   // output mode
   const lListElem *output_param = lGetElemStr(bundle, SPP_name, OUTPUT_MODE);
   const lList *output_list = lGetList(output_param, SPP_value_list);
   output_mode_ = static_cast<OutputMode>(lGetUlong(lFirst(output_list), ULNG_value));
   DPRINTF("output_mode_: " sge_u32 "\n", static_cast<uint32_t>(output_mode_));

   // need queues
   const lListElem *need_queues_param = lGetElemStr(bundle, SPP_name, NEED_QUEUES);
   const lList *need_queues_list = lGetList(need_queues_param, SPP_value_list);
   need_queues_ = lGetUlong(lFirst(need_queues_list), ULNG_value) > 0;
   DPRINTF("need_queues_: " sge_u32 "\n", need_queues_);

   // need jobs
   const lListElem *need_jobs_param = lGetElemStr(bundle, SPP_name, NEED_JOBS);
   const lList *need_jobs_list = lGetList(need_jobs_param, SPP_value_list);
   need_job_list_ = lGetUlong(lFirst(need_jobs_list), ULNG_value) > 0;
   DPRINTF("need_jobs_: " sge_u32 "\n", need_job_list_);

   // state filter
   const lListElem *state_filter_param = lGetElemStr(bundle, SPP_name, STATE_FILTER);
   const lList *state_filter_list = lGetList(state_filter_param, SPP_value_list);
   state_filter_ = lGetUlong(lFirst(state_filter_list), ULNG_value);
   DPRINTF("state_filter_: " sge_u32 "\n", state_filter_);

   // state string
   const lListElem *state_string_param = lGetElemStr(bundle, SPP_name, STATE_STRING);
   const lList *state_string_list = lGetList(state_string_param, SPP_value_list);

   const char *value = lGetString(lFirst(state_string_list), ST_name);
   state_filter_value_ = value != nullptr ? value : "";
   DPRINTF("state_filter_value_: %s\n", state_filter_value_.c_str());

   // queue state
   const lListElem *queue_state_param = lGetElemStr(bundle, SPP_name, QUEUE_STATE);
   const lList *queue_state_list = lGetList(queue_state_param, SPP_value_list);
   queue_state_ = lGetUlong(lFirst(queue_state_list), ULNG_value);
   DPRINTF("queue_state_: " sge_u32 "\n", queue_state_);

   // group opt
   const lListElem *group_opt_param = lGetElemStr(bundle, SPP_name, GROUP_OPT);
   const lList *group_opt_list = lGetList(group_opt_param, SPP_value_list);
   group_opt_ = lGetUlong(lFirst(group_opt_list), ULNG_value);
   DPRINTF("group_opt_: " sge_u32 "\n", group_opt_);

   // -l
   lListElem *resource_param = lGetElemStrRW(bundle, SPP_name, RESOURCE_LIST);
   resource_list_ = nullptr;
   lXchgList(resource_param, SPP_value_list, &resource_list_);
   DPRINTF("resource_list_: " sge_u32 " elements\n", lGetNumberOfElem(resource_list_));

   // -F
   lListElem *q_resource_param = lGetElemStrRW(bundle, SPP_name, Q_RESOURCE_LIST);
   q_resource_list_ = nullptr;
   lXchgList(q_resource_param, SPP_value_list, &q_resource_list_);
   DPRINTF("q_resource_list_: " sge_u32 " elements\n", lGetNumberOfElem(q_resource_list_));

   // -u
   lListElem *queue_ref_param = lGetElemStrRW(bundle, SPP_name, QUEUE_REF_LIST);
   queue_ref_list_ = nullptr;
   lXchgList(queue_ref_param, SPP_value_list, &queue_ref_list_);
   DPRINTF("queue_ref_list_: " sge_u32 " elements\n", lGetNumberOfElem(queue_ref_list_));

   // -pe
   lListElem *pe_ref_param = lGetElemStrRW(bundle, SPP_name, PE_REF_LIST);
   pe_ref_list_ = nullptr;
   lXchgList(pe_ref_param, SPP_value_list, &pe_ref_list_);
   DPRINTF("pe_ref_list_: " sge_u32 " elements\n", lGetNumberOfElem(pe_ref_list_));

   // -u
   lListElem *user_param = lGetElemStrRW(bundle, SPP_name, USER_LIST);
   user_list_ = nullptr;
   lXchgList(user_param, SPP_value_list, &user_list_);
   DPRINTF("user_list_: " sge_u32 " elements\n", lGetNumberOfElem(user_list_));

   // -U
   lListElem *queue_user_param = lGetElemStrRW(bundle, SPP_name, QUEUE_USER_LIST);
   queue_user_list_ = nullptr;
   lXchgList(queue_user_param, SPP_value_list, &queue_user_list_);
   DPRINTF("queue_user_list_: " sge_u32 " elements\n", lGetNumberOfElem(queue_user_list_));

   // -j
   lListElem *jid_param = lGetElemStrRW(bundle, SPP_name, JID_LIST);
   jid_list_ = nullptr;
   lXchgList(jid_param, SPP_value_list, &jid_list_);
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
   ep = lAddElemStr(&bundle, SPP_name, JID_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, jid_list_);
   DPRINTF("jid_list_: " sge_u32 " elements\n", lGetNumberOfElem(jid_list_));

   // -U
   ep = lAddElemStr(&bundle, SPP_name, QUEUE_USER_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, queue_user_list_);
   DPRINTF("queue_user_list_: " sge_u32 " element\n", lGetNumberOfElem(queue_user_list_));

   // -u
   ep = lAddElemStr(&bundle, SPP_name, USER_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, user_list_);
   DPRINTF("user_list_: " sge_u32 " elements\n", lGetNumberOfElem(user_list_));

   // -pe
   ep = lAddElemStr(&bundle, SPP_name, PE_REF_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, pe_ref_list_);
   DPRINTF("pr_ref_list_: " sge_u32 " elements\n", lGetNumberOfElem(pe_ref_list_));

   // -u
   ep = lAddElemStr(&bundle, SPP_name, QUEUE_REF_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, queue_ref_list_);
   DPRINTF("queue_ref_list_: " sge_u32 " elements\n", lGetNumberOfElem(queue_ref_list_));

   // -F
   ep = lAddElemStr(&bundle, SPP_name, Q_RESOURCE_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, q_resource_list_);
   DPRINTF("q_resource_list_: " sge_u32 " elements\n", lGetNumberOfElem(q_resource_list_));

   // -l
   ep = lAddElemStr(&bundle, SPP_name, RESOURCE_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, resource_list_);
   DPRINTF("resource_list_: " sge_u32 " elements\n", lGetNumberOfElem(resource_list_));

   // output mode
   lList *output_list = nullptr;
   lAddElemUlong(&output_list, ULNG_value, static_cast<uint32_t>(output_mode_), ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, OUTPUT_MODE, SPP_Type);
   lSetList(ep, SPP_value_list, output_list);
   DPRINTF("output_mode_=%u\n", static_cast<uint32_t>(output_mode_));

   // flags for used switches ...
   lList *show_list = nullptr;
   lAddElemUlong(&show_list, ULNG_value, show_, ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, SHOW, SPP_Type);
   lSetList(ep, SPP_value_list, show_list);
   DPRINTF("show_=%u\n", static_cast<uint32_t>(show_));

   // need queues
   lList *need_queues_list = nullptr;
   lAddElemUlong(&need_queues_list, ULNG_value, need_queues_, ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, NEED_QUEUES, SPP_Type);
   lSetList(ep, SPP_value_list, need_queues_list);
   DPRINTF("need_queues_=%s\n", need_queues_ ? "true" : "false");

   // need jobs
   lList *need_jobs_list = nullptr;
   lAddElemUlong(&need_jobs_list, ULNG_value, need_job_list_, ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, NEED_JOBS, SPP_Type);
   lSetList(ep, SPP_value_list, need_jobs_list);
   DPRINTF("need_jobs_=%s\n", need_job_list_ ? "true" : "false");

   // state filter
   lList *state_filter_list = nullptr;
   lAddElemUlong(&state_filter_list, ULNG_value, state_filter_, ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, STATE_FILTER, SPP_Type);
   lSetList(ep, SPP_value_list, state_filter_list);
   DPRINTF("state_filter_=%u\n", static_cast<uint32_t>(state_filter_));

   // state string
   lList *state_filter_value_list = nullptr;
   lAddElemStr(&state_filter_value_list, ST_name, state_filter_value_.c_str(), ST_Type);
   ep = lAddElemStr(&bundle, SPP_name, STATE_STRING, SPP_Type);
   lSetList(ep, SPP_value_list, state_filter_value_list);
   DPRINTF("state_filer_value_=%s (%p)\n", state_filter_value_.c_str(), state_filter_value_.c_str());

   // queue state
   lList *queue_state_list = nullptr;
   lAddElemUlong(&queue_state_list, ULNG_value, queue_state_, ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, QUEUE_STATE, SPP_Type);
   lSetList(ep, SPP_value_list, queue_state_list);
   DPRINTF("queue_state_=%u\n", static_cast<uint32_t>(queue_state_));

   // explain bits
   lList *explain_bits_list = nullptr;
   lAddElemUlong(&explain_bits_list, ULNG_value, explain_bits_, ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, EXPLAIN_BITS, SPP_Type);
   lSetList(ep, SPP_value_list, explain_bits_list);
   DPRINTF("explain_bits_=%u\n", static_cast<uint32_t>(explain_bits_));

   // group opt
   lList *group_opt_list = nullptr;
   lAddElemUlong(&group_opt_list, ULNG_value, group_opt_, ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, GROUP_OPT, SPP_Type);
   lSetList(ep, SPP_value_list, group_opt_list);
   DPRINTF("group_opt_=%u\n", static_cast<uint32_t>(group_opt_));

   DRETURN(bundle);
}



