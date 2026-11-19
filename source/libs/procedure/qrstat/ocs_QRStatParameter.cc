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
 * @brief Parameters of `qrstat`: what the call was asked to report
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ulong.h"

#include "ocs_QRStatParameter.h"

#include "qhost/ocs_QHostParameter.h"
#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/** @brief Build the parameters on the server, from a received bundle
 * @param bundle the marshalled parameters
 * @param packet the originating client packet
 */
ocs::QRStatParameter::QRStatParameter(const ocs::tom::ConstList<ocs::tom::SPP> bundle, gdi::Packet *packet)
   : ProcedureParameter("", packet) {
   DENTER(TOP_LAYER);

   // initialize local member variables
   QRStatParameter::set_bundle(bundle);

   DRETURN_VOID;
}

/** @brief Resolve `-u *` and the empty user list into concrete names
 *
 * Called before the model runs, so that both execution contexts filter on
 * the same list. `*` becomes an empty list, meaning all users; an empty
 * list becomes the calling user.
 *
 * @return none
 */
void ocs::QRStatParameter::transform_user_list() {
   DENTER(TOP_LAYER);
   str_list_transform_user_list(user_list_, nullptr, user_);
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
void ocs::QRStatParameter::set_bundle(const ocs::tom::ConstList<ocs::tom::SPP> bundle) {
   DENTER(TOP_LAYER);

   // initialize parents member variables
   ProcedureParameter::set_bundle(bundle);

   // -explain
   const auto explain_param = bundle.find_first<SPP_name>(EXPLAIN);
   const auto explain_list = value_list<ocs::tom::ULNG>(explain_param);
   is_explain_ = ocs::tom::get<ULNG_value>(explain_list.front());

   // show summary or individual AR
   const auto summary_param = bundle.find_first<SPP_name>(SUMMARY);
   const auto summary_list = value_list<ocs::tom::ULNG>(summary_param);
   is_summary_ = ocs::tom::get<ULNG_value>(summary_list.front());

   // clients username
   const auto username_param = bundle.find_first<SPP_name>(USERNAME);
   const auto username_list = value_list<ocs::tom::ST>(username_param);
   strncpy(user_, ocs::tom::get<ST_name>(username_list.front()), sizeof(user_) - 1);

   // -u
   user_list_ =
           take_value_list<ocs::tom::ST>(ocs::tom::find_first_rw<SPP_name>(bundle, QRStatParameter::USER_NAME_LIST));

   // -ar
   ar_id_list_ = take_value_list<ocs::tom::ULNG>(ocs::tom::find_first_rw<SPP_name>(bundle, AR_ID_LIST));

   DRETURN_VOID;
}

/** @brief Marshal the parameters into a bundle for the server
 * @return the bundle, which the caller owns; the switch lists are copies
 */
ocs::tom::ListPtr<ocs::tom::SPP> ocs::QRStatParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Get the name-value-list that was initialized by the base class with the procedure name
   auto bundle = ProcedureParameter::get_bundle();

   // -ar
   add_parameter_bundle(*bundle, AR_ID_LIST, ar_id_list_->clone());

   // -u
   add_parameter_bundle(*bundle, USER_NAME_LIST, user_list_->clone());

   // show summary or individual AR
   add_parameter_bundle(*bundle, USERNAME, string_value_list(user_));

   // show summary or individual AR
   add_parameter_bundle(*bundle, SUMMARY, ulong_value_list(is_summary_));

   // -explain
   add_parameter_bundle(*bundle, EXPLAIN, ulong_value_list(is_explain()));

   DRETURN(bundle);
}
