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
#include "tom/ocs_List.h"

/** @brief Build the parameters on the server, from a received bundle
 * @param bundle the marshalled parameters
 * @param packet the originating client packet
 */
ocs::QQuotaParameter::QQuotaParameter(lList *bundle, gdi::Packet *packet) : ProcedureParameter("", packet) {
   DENTER(TOP_LAYER);

   QQuotaParameter::set_bundle(bundle);

   DRETURN_VOID;
}

/** @brief Fill the parameters from a received bundle
 *
 * The lists of the switches are taken out of the bundle, an untyped value
 * list, so they are not checked, as before.
 *
 * @param bundle the marshalled parameters; the switch lists are moved out of it
 * @return none
 */
void ocs::QQuotaParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   // procedure name, output format, ...
   ProcedureParameter::set_bundle(bundle);

   // -q
   ocs::tom::Element<ocs::tom::SPP> queue_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, QUEUE_NAME_LIST);
   lList *queue_name_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(queue_name_param, &queue_name_list_received);
   queue_name_list_ = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(queue_name_list_received);

   // -h
   ocs::tom::Element<ocs::tom::SPP> host_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, HOSTNAME_LIST);
   lList *host_name_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(host_name_param, &host_name_list_received);
   host_name_list_ = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(host_name_list_received);

   // -pe
   ocs::tom::Element<ocs::tom::SPP> pe_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, PE_NAME_LIST);
   lList *pe_name_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(pe_name_param, &pe_name_list_received);
   pe_name_list_ = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(pe_name_list_received);

   // -P
   ocs::tom::Element<ocs::tom::SPP> prj_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, PROJECT_NAME_LIST);
   lList *project_name_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(prj_name_param, &project_name_list_received);
   project_name_list_ = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(project_name_list_received);

   // -l
   ocs::tom::Element<ocs::tom::SPP> resource_match_param =
      ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_MATCH_LIST);
   lList *resource_match_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(resource_match_param, &resource_match_list_received);
   resource_match_list_ = ocs::tom::ListPtr<ocs::tom::CE>::from_typed(resource_match_list_received);

   // -u
   ocs::tom::Element<ocs::tom::SPP> user_match_param = ocs::tom::find_first_rw<SPP_name>(bundle, USER_NAME_LIST);
   lList *user_name_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(user_match_param, &user_name_list_received);
   user_name_list = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(user_name_list_received);

   DRETURN_VOID;
}

/** @brief Marshal the parameters into a bundle for the server
 * @return the bundle, which the caller owns; the switch lists are copies
 */
lList *ocs::QQuotaParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Parent: -fmt, procedure name
   lList *bundle =  ProcedureParameter::get_bundle();

   // -q
   ocs::tom::Element<ocs::tom::SPP> ep = ocs::tom::emplace_back<SPP_name>(&bundle, QUEUE_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep, queue_name_list_->clone());

   // -h
   ocs::tom::Element<ocs::tom::SPP> ep_spp = ocs::tom::emplace_back<SPP_name>(&bundle, HOSTNAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp, host_name_list_->clone());

   // -pe
   ocs::tom::Element<ocs::tom::SPP> ep_spp_2 = ocs::tom::emplace_back<SPP_name>(&bundle, PE_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_2, pe_name_list_->clone());

   // -P
   ocs::tom::Element<ocs::tom::SPP> ep_spp_3 = ocs::tom::emplace_back<SPP_name>(&bundle, PROJECT_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_3, project_name_list_->clone());

   // -l
   ocs::tom::Element<ocs::tom::SPP> ep_spp_4 = ocs::tom::emplace_back<SPP_name>(&bundle, RESOURCE_MATCH_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_4, resource_match_list_->clone());

   // -u
   ocs::tom::Element<ocs::tom::SPP> ep_spp_5 = ocs::tom::emplace_back<SPP_name>(&bundle, USER_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_5, user_name_list->clone());

   DRETURN(bundle);
}
