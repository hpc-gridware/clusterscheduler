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

#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_var.h"

#include "ocs_QQuotaParameter.h"

#include "sgeobj/cull/sge_param_SPP_L.h"

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

ocs::QQuotaParameter::QQuotaParameter(lList *bundle, gdi::Packet *packet) : ProcedureParameter("", packet) {
   DENTER(TOP_LAYER);

   QQuotaParameter::set_bundle(bundle);

   DRETURN_VOID;
}

void ocs::QQuotaParameter::set_bundle(const lList *bundle) {
   // procedure name, output format, ...
   ProcedureParameter::set_bundle(bundle);

   // -q
   lListElem *queue_name_param = lGetElemStrRW(bundle, SPP_name, QUEUE_NAME_LIST);
   queue_name_list_ = nullptr;
   lXchgList(queue_name_param, SPP_value_list, &queue_name_list_);

   // -h
   lListElem *host_name_param = lGetElemStrRW(bundle, SPP_name, HOSTNAME_LIST);
   host_name_list_ = nullptr;
   lXchgList(host_name_param, SPP_value_list, &host_name_list_);

   // -pe
   lListElem *pe_name_param = lGetElemStrRW(bundle, SPP_name, PE_NAME_LIST);
   pe_name_list_ = nullptr;
   lXchgList(pe_name_param, SPP_value_list, &pe_name_list_);

   // -P
   lListElem *prj_name_param = lGetElemStrRW(bundle, SPP_name, PROJECT_NAME_LIST);
   project_name_list_ = nullptr;
   lXchgList(prj_name_param, SPP_value_list, &project_name_list_);

   // -l
   lListElem *resource_match_param = lGetElemStrRW(bundle, SPP_name, RESOURCE_MATCH_LIST);
   resource_match_list_ = nullptr;
   lXchgList(resource_match_param, SPP_value_list, &resource_match_list_);

   // -u
   lListElem *user_match_param = lGetElemStrRW(bundle, SPP_name, USER_NAME_LIST);
   user_name_list = nullptr;
   lXchgList(user_match_param, SPP_value_list, &user_name_list);
}

lList *ocs::QQuotaParameter::get_bundle() {
   DENTER(TOP_LAYER);
   lListElem *ep = nullptr;

   // Parent: -fmt, procedure name
   lList *bundle =  ProcedureParameter::get_bundle();

   // -q
   ep = lAddElemStr(&bundle, SPP_name, QUEUE_NAME_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, queue_name_list_);

   // -h
   ep = lAddElemStr(&bundle, SPP_name, HOSTNAME_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, host_name_list_);

   // -pe
   ep = lAddElemStr(&bundle, SPP_name, PE_NAME_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, pe_name_list_);

   // -P
   ep = lAddElemStr(&bundle, SPP_name, PROJECT_NAME_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, project_name_list_);

   // -l
   ep = lAddElemStr(&bundle, SPP_name, RESOURCE_MATCH_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, resource_match_list_);

   // -u
   ep = lAddElemStr(&bundle, SPP_name, USER_NAME_LIST, SPP_Type);
   lSetList(ep, SPP_value_list, user_name_list);

   DRETURN(bundle);
}
