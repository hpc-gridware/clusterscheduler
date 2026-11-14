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
 * @brief Parameters of `qquota`: what the call was asked to report
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_var.h"

#include "ocs_QQuotaParameter.h"

#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "tom/ocs_Element.h"

ocs::QQuotaParameter::~QQuotaParameter() {
   DENTER(TOP_LAYER);
   lFreeList(&queue_name_list_);
   lFreeList(&host_name_list_);
   lFreeList(&pe_name_list_);
   lFreeList(&project_name_list_);
   lFreeList(&resource_match_list_);
   lFreeList(&user_name_list);
   DRETURN_VOID;
}

/** @brief Build the parameters on the server, from a received bundle
 * @param bundle the marshalled parameters
 * @param packet the originating client packet
 */
ocs::QQuotaParameter::QQuotaParameter(lList *bundle, gdi::Packet *packet) : ProcedureParameter("", packet) {
   DENTER(TOP_LAYER);

   QQuotaParameter::set_bundle(bundle);

   DRETURN_VOID;
}

void ocs::QQuotaParameter::set_bundle(const lList *bundle) {
   // procedure name, output format, ...
   ProcedureParameter::set_bundle(bundle);

   // -q
   ocs::tom::Element<ocs::tom::SPP> queue_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, QUEUE_NAME_LIST);
   queue_name_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(queue_name_param, &queue_name_list_);

   // -h
   ocs::tom::Element<ocs::tom::SPP> host_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, HOSTNAME_LIST);
   host_name_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(host_name_param, &host_name_list_);

   // -pe
   ocs::tom::Element<ocs::tom::SPP> pe_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, PE_NAME_LIST);
   pe_name_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(pe_name_param, &pe_name_list_);

   // -P
   ocs::tom::Element<ocs::tom::SPP> prj_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, PROJECT_NAME_LIST);
   project_name_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(prj_name_param, &project_name_list_);

   // -l
   ocs::tom::Element<ocs::tom::SPP> resource_match_param = ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_MATCH_LIST);
   resource_match_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(resource_match_param, &resource_match_list_);

   // -u
   ocs::tom::Element<ocs::tom::SPP> user_match_param = ocs::tom::find_first_rw<SPP_name>(bundle, USER_NAME_LIST);
   user_name_list = nullptr;
   ocs::tom::exchange<SPP_value_list>(user_match_param, &user_name_list);
}

lList *ocs::QQuotaParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Parent: -fmt, procedure name
   lList *bundle =  ProcedureParameter::get_bundle();

   // -q
   ocs::tom::Element<ocs::tom::SPP> ep = ocs::tom::emplace_back<SPP_name>(&bundle, QUEUE_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, queue_name_list_));

   // -h
   ocs::tom::Element<ocs::tom::SPP> ep_spp = ocs::tom::emplace_back<SPP_name>(&bundle, HOSTNAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp, lCopyList(nullptr, host_name_list_));

   // -pe
   ocs::tom::Element<ocs::tom::SPP> ep_spp_2 = ocs::tom::emplace_back<SPP_name>(&bundle, PE_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_2, lCopyList(nullptr, pe_name_list_));

   // -P
   ocs::tom::Element<ocs::tom::SPP> ep_spp_3 = ocs::tom::emplace_back<SPP_name>(&bundle, PROJECT_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_3, lCopyList(nullptr, project_name_list_));

   // -l
   ocs::tom::Element<ocs::tom::SPP> ep_spp_4 = ocs::tom::emplace_back<SPP_name>(&bundle, RESOURCE_MATCH_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_4, lCopyList(nullptr, resource_match_list_));

   // -u
   ocs::tom::Element<ocs::tom::SPP> ep_spp_5 = ocs::tom::emplace_back<SPP_name>(&bundle, USER_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_5, lCopyList(nullptr, user_name_list));

   DRETURN(bundle);
}
