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
#include "tom/ocs_Element.h"

/** @brief Build the parameters on the server, from a received bundle
 * @param bundle the marshalled parameters
 * @param packet the originating client packet
 */
ocs::QRStatParameter::QRStatParameter(lList *bundle, gdi::Packet *packet) : ProcedureParameter("", packet) {
   DENTER(TOP_LAYER);

   // initialize local member variables
   QRStatParameter::set_bundle(bundle);

   DRETURN_VOID;
}

ocs::QRStatParameter::~QRStatParameter() {
   DENTER(TOP_LAYER);
   lFreeList(&user_list_);
   lFreeList(&ar_id_list_);
   DRETURN_VOID;
}

/** @brief Resolve `-u *` and the empty user list into concrete names
 *
 * Called before the model runs, so that both execution contexts filter on
 * the same list. `*` becomes an empty list, meaning all users; an empty
 * list becomes the calling user.
 */
void ocs::QRStatParameter::transform_user_list() {
   str_list_transform_user_list(&user_list_, nullptr,  user_);
}

void ocs::QRStatParameter::set_bundle(const lList *bundle) {
   // initialize parents member variables
   ProcedureParameter::set_bundle(bundle);

   // -explain
   const lListElem *explain_param = ocs::tom::find_first<SPP_name>(bundle, EXPLAIN);
   const lList *explain_list = ocs::tom::get<SPP_value_list>(explain_param);
   is_explain_ = ocs::tom::get<ULNG_value>(lFirst(explain_list));

   // show summary or individual AR
   const lListElem *summary_param = ocs::tom::find_first<SPP_name>(bundle, SUMMARY);
   const lList *summary_list = ocs::tom::get<SPP_value_list>(summary_param);
   is_summary_ = ocs::tom::get<ULNG_value>(lFirst(summary_list));

   // clients username
   const lListElem *username_param = ocs::tom::find_first<SPP_name>(bundle, USERNAME);
   const lList *username_list = ocs::tom::get<SPP_value_list>(username_param);
   strncpy(user_, ocs::tom::get<ST_name>(lFirst(username_list)), sizeof(user_) - 1);

   // -u
   lListElem *user_name_param = ocs::tom::find_first_rw<SPP_name>(bundle, QRStatParameter::USER_NAME_LIST);
   user_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(user_name_param, &user_list_);

   // -ar
   lListElem *ar_id_param = ocs::tom::find_first_rw<SPP_name>(bundle, AR_ID_LIST);
   ar_id_list_ = nullptr;
   ocs::tom::exchange<SPP_value_list>(ar_id_param, &ar_id_list_);
}

lList * ocs::QRStatParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Get the name-value-list that was initialized by the base class with the procedure name
   lList *bundle = ProcedureParameter::get_bundle();

   // -ar
   lListElem *ep = ocs::tom::emplace_back<SPP_name>(&bundle, AR_ID_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, ar_id_list_));

   // -u
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, USER_NAME_LIST);
   ocs::tom::set<SPP_value_list>(ep, lCopyList(nullptr, user_list_));

   // show summary or individual AR
   lList *username_list = nullptr;
   ocs::tom::emplace_back<ST_name>(&username_list, user_);
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, USERNAME);
   ocs::tom::set<SPP_value_list>(ep, username_list);

   // show summary or individual AR
   lList *show_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&show_list, is_summary_);
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, SUMMARY);
   ocs::tom::set<SPP_value_list>(ep, show_list);

   // -explain
   lList *explain_list = nullptr;
   ocs::tom::emplace_back<ULNG_value>(&explain_list, is_explain());
   ep = ocs::tom::emplace_back<SPP_name>(&bundle, EXPLAIN);
   ocs::tom::set<SPP_value_list>(ep, explain_list);

   DRETURN(bundle);
}
