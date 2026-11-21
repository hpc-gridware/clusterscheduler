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

#include "sgeobj/tom/ocs_tom_SPP.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/** @brief Build the parameters on the server, from a received bundle
 * @param bundle the marshalled parameters
 * @param packet the originating client packet
 */
ocs::QQuotaParameter::QQuotaParameter(const ocs::tom::List<ocs::tom::SPP> bundle, gdi::Packet *packet)
   : ProcedureParameter("", packet) {
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
void ocs::QQuotaParameter::set_bundle(const ocs::tom::List<ocs::tom::SPP> bundle) {
   DENTER(TOP_LAYER);

   // procedure name, output format, ...
   ProcedureParameter::set_bundle(bundle);

   // -q
   queue_name_list_ = take_value_list<ocs::tom::ST>(bundle.find_first<SPP_name>(QUEUE_NAME_LIST));

   // -h
   host_name_list_ = take_value_list<ocs::tom::ST>(bundle.find_first<SPP_name>(HOSTNAME_LIST));

   // -pe
   pe_name_list_ = take_value_list<ocs::tom::ST>(bundle.find_first<SPP_name>(PE_NAME_LIST));

   // -P
   project_name_list_ = take_value_list<ocs::tom::ST>(bundle.find_first<SPP_name>(PROJECT_NAME_LIST));

   // -l
   resource_match_list_ =
           take_value_list<ocs::tom::CE>(bundle.find_first<SPP_name>(RESOURCE_MATCH_LIST));

   // -u
   user_name_list = take_value_list<ocs::tom::ST>(bundle.find_first<SPP_name>(USER_NAME_LIST));

   DRETURN_VOID;
}

/** @brief Marshal the parameters into a bundle for the server
 * @return the bundle, which the caller owns; the switch lists are copies
 */
ocs::tom::ListPtr<ocs::tom::SPP> ocs::QQuotaParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Parent: -fmt, procedure name
   auto bundle = ProcedureParameter::get_bundle();

   // -q
   add_parameter_bundle(*bundle, QUEUE_NAME_LIST, queue_name_list_->clone());

   // -h
   add_parameter_bundle(*bundle, HOSTNAME_LIST, host_name_list_->clone());

   // -pe
   add_parameter_bundle(*bundle, PE_NAME_LIST, pe_name_list_->clone());

   // -P
   add_parameter_bundle(*bundle, PROJECT_NAME_LIST, project_name_list_->clone());

   // -l
   add_parameter_bundle(*bundle, RESOURCE_MATCH_LIST, resource_match_list_->clone());

   // -u
   add_parameter_bundle(*bundle, USER_NAME_LIST, user_name_list->clone());

   DRETURN(bundle);
}
