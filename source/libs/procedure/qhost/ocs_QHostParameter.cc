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
ocs::QHostParameter::QHostParameter(const ocs::tom::ConstList<ocs::tom::SPP> bundle, gdi::Packet *packet)
   : ProcedureParameter("", packet) {
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
void ocs::QHostParameter::set_bundle(const ocs::tom::ConstList<ocs::tom::SPP> bundle) {
   DENTER(TOP_LAYER);

   // initialize parents member variables
   ProcedureParameter::set_bundle(bundle);

   // flags for used switches: -F -q -j -u
   const auto show_param = bundle.find_first<SPP_name>(SHOW);
   const auto show_list = value_list<ocs::tom::ULNG>(show_param);
   show_ = show_list.front().get<ULNG_value>();

   // -F
   resource_visible_list_ =
           take_value_list<ocs::tom::ST>(ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_VISIBLE_LIST));

   // -l
   resource_match_list_ = take_value_list<ocs::tom::CE>(ocs::tom::find_first_rw<SPP_name>(bundle, RESOURCE_LIST));

   // -u
   user_name_list_ = take_value_list<ocs::tom::ST>(ocs::tom::find_first_rw<SPP_name>(bundle, USER_NAME_LIST));

   // -h
   hostname_list_ = take_value_list<ocs::tom::ST>(ocs::tom::find_first_rw<SPP_name>(bundle, HOSTNAME_LIST));

   DRETURN_VOID;
}

/** @brief Marshal the parameters into a bundle for the server
 * @return the bundle, which the caller owns; the switch lists are copies
 */
ocs::tom::ListPtr<ocs::tom::SPP> ocs::QHostParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Get the name-value-list that was initialized by the base class with the procedure name
   auto bundle = ProcedureParameter::get_bundle();

   // -h
   add_parameter_bundle(*bundle, HOSTNAME_LIST, hostname_list_->clone());

   // -u
   add_parameter_bundle(*bundle, USER_NAME_LIST, user_name_list_->clone());

   // -l
   add_parameter_bundle(*bundle, RESOURCE_LIST, resource_match_list_->clone());

   // -F
   add_parameter_bundle(*bundle, RESOURCE_VISIBLE_LIST, resource_visible_list_->clone());

   // flags for used switches: -F -q -j -u
   add_parameter_bundle(*bundle, SHOW, ulong_value_list(show_));

   DRETURN(bundle);
}

