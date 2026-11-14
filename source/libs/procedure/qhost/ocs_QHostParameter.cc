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

#include "qhost/ocs_QHostParameter.h"

#include "ocs_ProcedureParameter.h"

/** @brief The process environment, which the parameter parsing reads */
extern char **environ;

ocs::QHostParameter::~QHostParameter() {
   lFreeList(&hostname_list_);
   lFreeList(&user_name_list_);
   lFreeList(&resource_match_list_);
   lFreeList(&resource_visible_list_);
}

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

void ocs::QHostParameter::set_bundle(const lList *bundle) {
   DENTER(TOP_LAYER);

   // initialize parents member variables
   ProcedureParameter::set_bundle(bundle);

   // flags for used switches: -F -q -j -u
   ocs::tom::ConstElement<ocs::tom::SPP> show_param = ocs::tom::find_first<SPP_name>(bundle, SHOW);
   const lList *show_list = ocs::tom::get<SPP_value_list>(show_param);
   show_ = ocs::tom::get<ULNG_value>(lFirst(show_list));

   // -F
   ocs::tom::Element<ocs::tom::SPP> resource_visible_param = ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_VISIBLE_LIST);
   resource_visible_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(resource_visible_param, &resource_visible_list_);

   // -l
   ocs::tom::Element<ocs::tom::SPP> resource_match_param = ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_LIST);
   resource_match_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(resource_match_param, &resource_match_list_);

   // -u
   ocs::tom::Element<ocs::tom::SPP> user_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, USER_NAME_LIST);
   user_name_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(user_name_param, &user_name_list_);

   // -h
   ocs::tom::Element<ocs::tom::SPP> host_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, HOSTNAME_LIST);
   hostname_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(host_name_param, &hostname_list_);

   DRETURN_VOID;
}

lList *ocs::QHostParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Get the name-value-list that was initialized by the base class with the procedure name
   lList *bundle =  ProcedureParameter::get_bundle();
   //lList *name_value_list = lGetListRW(lGetElemStrRW(bundle, SPP_name, NAME_VALUE_LIST), SPP_value_list);

   // -h
   ocs::tom::Element<ocs::tom::SPP> ep = ocs::tom::emplace_back<SPP_name>(&bundle, HOSTNAME_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, hostname_list_));

   // -u
   ocs::tom::Element<ocs::tom::SPP> ep_spp = ocs::tom::emplace_back<SPP_name>(&bundle, USER_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp, lCopyList(nullptr, user_name_list_));

   // -l
   ocs::tom::Element<ocs::tom::SPP> ep_spp_2 = ocs::tom::emplace_back<SPP_name>(&bundle, RESOURCE_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_2, lCopyList(nullptr, resource_match_list_));

   // -F
   ocs::tom::Element<ocs::tom::SPP> ep_spp_3 = ocs::tom::emplace_back<SPP_name>(&bundle, RESOURCE_VISIBLE_LIST);
   ocs::tom::set<SPP_value_list>(ep_spp_3, lCopyList(nullptr, resource_visible_list_));

   // flags for used switches: -F -q -j -u
   lList *show_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&show_list, show_);
   ocs::tom::Element<ocs::tom::SPP> ep_spp_4 = ocs::tom::emplace_back<SPP_name>(&bundle, SHOW);
   ocs::tom::set<SPP_value_list>(ep_spp_4, show_list);

   DRETURN(bundle);
}

