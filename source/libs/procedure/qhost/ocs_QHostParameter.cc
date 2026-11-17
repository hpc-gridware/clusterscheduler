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
 * @brief Parameters of `qhost`: what the call was asked to report
 */

#include <string>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "qhost/ocs_QHostParameter.h"

#include "ocs_ProcedureParameter.h"

/** @brief The process environment, which the parameter parsing reads */
extern char **environ;

/** @brief Build the parameters on the server, from a received bundle
 * @param bundle the marshalled parameters
 * @param packet the originating client packet
 */
ocs::QHostParameter::QHostParameter(lList *bundle, gdi::Packet *packet) : ProcedureParameter("", packet) {
   DENTER(TOP_LAYER);

   // initialize local member variables
   QHostParameter::set_bundle(bundle);

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
void ocs::QHostParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   // initialize parents member variables
   ProcedureParameter::set_bundle(bundle);

   // flags for used switches: -F -q -j -u
   ocs::tom::ConstElement<ocs::tom::SPP> show_param = ocs::tom::find_first<SPP_name>(bundle, SHOW);
   const lList *show_list = ocs::tom::get<SPP_value_list>(show_param);
   show_ = ocs::tom::get<ULNG_value>(lFirst(show_list));

   // -F
   ocs::tom::Element<ocs::tom::SPP> resource_visible_param =
      ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_VISIBLE_LIST);
   lList *resource_visible_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(resource_visible_param, &resource_visible_list_received);
   resource_visible_list_ = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(resource_visible_list_received);

   // -l
   ocs::tom::Element<ocs::tom::SPP> resource_match_param = ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_LIST);
   lList *resource_match_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(resource_match_param, &resource_match_list_received);
   resource_match_list_ = ocs::tom::ListPtr<ocs::tom::CE>::from_typed(resource_match_list_received);

   // -u
   ocs::tom::Element<ocs::tom::SPP> user_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, USER_NAME_LIST);
   lList *user_name_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(user_name_param, &user_name_list_received);
   user_name_list_ = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(user_name_list_received);

   // -h
   ocs::tom::Element<ocs::tom::SPP> host_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, HOSTNAME_LIST);
   lList *hostname_list_received = nullptr;
   ocs::tom::exchange<SPP_value_list>(host_name_param, &hostname_list_received);
   hostname_list_ = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(hostname_list_received);

   DRETURN_VOID;
}

/** @brief Marshal the parameters into a bundle for the server
 * @return the bundle, which the caller owns; the switch lists are copies
 */
lList *ocs::QHostParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Get the name-value-list that was initialized by the base class with the procedure name
   lList *bundle =  ProcedureParameter::get_bundle();
   //lList *name_value_list = lGetListRW(lGetElemStrRW(bundle, SPP_name, NAME_VALUE_LIST), SPP_value_list);

   // -h
   ocs::tom::Element<ocs::tom::SPP> ep = ocs::tom::emplace_back<SPP_name>(&bundle, HOSTNAME_LIST);
   ocs::tom::set<SPP_value_list>(ep, hostname_list_->clone());

   // -u
   ocs::tom::Element<ocs::tom::SPP> ep_spp = ocs::tom::emplace_back<SPP_name>(&bundle, USER_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp, user_name_list_->clone());

   // -l
   ocs::tom::Element<ocs::tom::SPP> ep_spp_2 = ocs::tom::emplace_back<SPP_name>(&bundle, RESOURCE_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_2, resource_match_list_->clone());

   // -F
   ocs::tom::Element<ocs::tom::SPP> ep_spp_3 = ocs::tom::emplace_back<SPP_name>(&bundle, RESOURCE_VISIBLE_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_3, resource_visible_list_->clone());

   // flags for used switches: -F -q -j -u
   lList *show_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&show_list, show_);
   ocs::tom::Element<ocs::tom::SPP> ep_spp_4 = ocs::tom::emplace_back<SPP_name>(&bundle, SHOW);
   ocs::tom::set<SPP_value_list>(ep_spp_4, show_list);

   DRETURN(bundle);
}

