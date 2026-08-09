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
 * @brief Server side model of `qquota`: reads the lists inside qmaster
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_DataStore.h"

#include "ocs_QQuotaModelServer.h"

bool ocs::QQuotaModelServer::fetch_data(lList **answer_list, const lList *host_name_list) {
   DENTER(TOP_LAYER);

   // Fetch all resource quota sets
   const lList *master_rqs_list = *DataStore::get_master_list(SGE_TYPE_RQS);
   lEnumeration *rqs_what = get_rqs_what();
   rqs_list_ = lSelect("", master_rqs_list, nullptr, rqs_what);
   lFreeWhat(&rqs_what);
   DPRINTF("rqs: " sge_u32"\n", lGetNumberOfElem(rqs_list_));

   const lList *master_ce_list = *DataStore::get_master_list(SGE_TYPE_CENTRY);
   lEnumeration *ce_what = get_centry_what();
   centry_list_ = lSelect("", master_ce_list, nullptr, ce_what);
   lFreeWhat(&ce_what);
   DPRINTF("complex entries: " sge_u32"\n", lGetNumberOfElem(centry_list_));

   const lList *master_user_set_list = *DataStore::get_master_list(SGE_TYPE_USERSET);
   lEnumeration *user_set_what = get_user_set_what();
   user_set_list_ = lSelect("", master_user_set_list, nullptr, user_set_what);
   lFreeWhat(&user_set_what);
   DPRINTF("user sets: " sge_u32"\n", lGetNumberOfElem(user_set_list_));

   const lList *master_hgroup_list = *DataStore::get_master_list(SGE_TYPE_HGROUP);
   lEnumeration *hgroup_what = get_hgroup_what();
   hgroup_list_ = lSelect("", master_hgroup_list, nullptr, hgroup_what);
   lFreeWhat(&hgroup_what);
   DPRINTF("host groups: " sge_u32"\n", lGetNumberOfElem(hgroup_list_));

   const lList *master_exec_host_list = *DataStore::get_master_list(SGE_TYPE_EXECHOST);
   lEnumeration *exec_host_what = get_host_what();
   lCondition *exec_host_where = get_host_where(host_name_list);
   exec_host_list_ = lSelect("", master_exec_host_list, exec_host_where, exec_host_what);
   lFreeWhat(&exec_host_what);
   lFreeWhere(&exec_host_where);
   DPRINTF("execution hosts: " sge_u32"\n", lGetNumberOfElem(exec_host_list_))

   // QHostModelClient fetches the configuration additionally which is not required in the server context
   ;

   DRETURN(true);
}
