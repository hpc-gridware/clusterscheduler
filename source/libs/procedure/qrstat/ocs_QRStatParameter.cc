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

#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ulong.h"

#include "ocs_QRStatParameter.h"

#include "qhost/ocs_QHostParameter.h"
#include "sgeobj/cull/sge_param_SPP_L.h"

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

void ocs::QRStatParameter::transform_user_list() {
   str_list_transform_user_list(&user_list_, nullptr,  user_);
}

void ocs::QRStatParameter::set_bundle(const lList *bundle) {
   // initialize parents member variables
   ProcedureParameter::set_bundle(bundle);

   // -explain
   const lListElem *explain_param = lGetElemStr(bundle, SPP_name, EXPLAIN);
   const lList *explain_list = lGetList(explain_param, SPP_value_list);
   is_explain_ = lGetUlong(lFirst(explain_list), ULNG_value);

   // show summary or individual AR
   const lListElem *summary_param = lGetElemStr(bundle, SPP_name, SUMMARY);
   const lList *summary_list = lGetList(summary_param, SPP_value_list);
   is_summary_ = lGetUlong(lFirst(summary_list), ULNG_value);

   // clients username
   const lListElem *username_param = lGetElemStr(bundle, SPP_name, USERNAME);
   const lList *username_list = lGetList(username_param, SPP_value_list);
   strncpy(user_, lGetString(lFirst(username_list), ST_name), sizeof(user_) - 1);

   // -u
   lListElem *user_name_param = lGetElemStrRW(bundle, SPP_name, QRStatParameter::USER_NAME_LIST);
   user_list_ = nullptr;
   lXchgList(user_name_param, SPP_value_list, &user_list_);

   // -ar
   lListElem *ar_id_param = lGetElemStrRW(bundle, SPP_name, AR_ID_LIST);
   ar_id_list_ = nullptr;
   lXchgList(ar_id_param, SPP_value_list, &ar_id_list_);
}

lList * ocs::QRStatParameter::get_bundle() {
   DENTER(TOP_LAYER);

   // Get the name-value-list that was initialized by the base class with the procedure name
   lList *bundle = ProcedureParameter::get_bundle();

   // -ar
   lListElem *ep = lAddElemStr(&bundle, SPP_name, AR_ID_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, lCopyList(nullptr, ar_id_list_));

   // -u
   ep = lAddElemStr(&bundle, SPP_name, USER_NAME_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, lCopyList(nullptr, user_list_));

   // show summary or individual AR
   lList *username_list = nullptr;
   lAddElemStr(&username_list, ST_name, user_, ST_Type);
   ep = lAddElemStr(&bundle, SPP_name, USERNAME, SPP_Type);
   lSetList(ep, SPP_value_list, username_list);

   // show summary or individual AR
   lList *show_list = nullptr;
   lAddElemUlong(&show_list, ULNG_value, is_summary_, ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, SUMMARY, SPP_Type);
   lSetList(ep, SPP_value_list, show_list);

   // -explain
   lList *explain_list = nullptr;
   lAddElemUlong(&explain_list, ULNG_value, is_explain(), ULNG_Type);
   ep = lAddElemStr(&bundle, SPP_name, EXPLAIN, SPP_Type);
   lSetList(ep, SPP_value_list, explain_list);

   DRETURN(bundle);
}
