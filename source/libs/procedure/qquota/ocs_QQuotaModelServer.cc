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
#include "sgeobj/sge_schedd_conf.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_SC.h"

#include "ocs_QQuotaModelServer.h"
#include "sgeobj/sge_answer.h"

/** @brief Fetch the lists from the master lists of qmaster
 *
 * Every list is a selection of a master list of a fixed type, so it is
 * wrapped with a check of its type.
 *
 * @param answer_list receives error messages
 * @param host_name_list the hosts the user asked for; all hosts when empty
 * @return true
 */
bool ocs::QQuotaModelServer::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                        const ocs::tom::ConstList<ocs::tom::ST> host_name_list) {
   DENTER(TOP_LAYER);

   // Fetch all resource quota sets
   const lList *master_rqs_list = *DataStore::get_master_list(SGE_TYPE_RQS);
   lEnumeration *rqs_what = get_rqs_what();
   rqs_list_ = ocs::tom::ListPtr<ocs::tom::RQS>(lSelect("", master_rqs_list, nullptr, rqs_what));
   lFreeWhat(&rqs_what);
   DPRINTF("rqs: " sge_u32"\n", static_cast<uint32_t>(rqs_list_->size()));

   const lList *master_ce_list = *DataStore::get_master_list(SGE_TYPE_CENTRY);
   lEnumeration *ce_what = get_centry_what();
   centry_list_ = ocs::tom::ListPtr<ocs::tom::CE>(lSelect("", master_ce_list, nullptr, ce_what));
   lFreeWhat(&ce_what);
   DPRINTF("complex entries: " sge_u32"\n", static_cast<uint32_t>(centry_list_->size()));

   const lList *master_user_set_list = *DataStore::get_master_list(SGE_TYPE_USERSET);
   lEnumeration *user_set_what = get_user_set_what();
   user_set_list_ = ocs::tom::ListPtr<ocs::tom::US>(lSelect("", master_user_set_list, nullptr, user_set_what));
   lFreeWhat(&user_set_what);
   DPRINTF("user sets: " sge_u32"\n", static_cast<uint32_t>(user_set_list_->size()));

   const lList *master_hgroup_list = *DataStore::get_master_list(SGE_TYPE_HGROUP);
   lEnumeration *hgroup_what = get_hgroup_what();
   hgroup_list_ = ocs::tom::ListPtr<ocs::tom::HGRP>(lSelect("", master_hgroup_list, nullptr, hgroup_what));
   lFreeWhat(&hgroup_what);
   DPRINTF("host groups: " sge_u32"\n", static_cast<uint32_t>(hgroup_list_->size()));

   const lList *master_exec_host_list = *DataStore::get_master_list(SGE_TYPE_EXECHOST);
   lEnumeration *exec_host_what = get_host_what();
   lCondition *exec_host_where = get_host_where(host_name_list);
   exec_host_list_ =
      ocs::tom::ListPtr<ocs::tom::EH>(lSelect("", master_exec_host_list, exec_host_where, exec_host_what));
   lFreeWhat(&exec_host_what);
   lFreeWhere(&exec_host_where);
   DPRINTF("execution hosts: " sge_u32"\n", static_cast<uint32_t>(exec_host_list_->size()))

   // QHostModelClient fetches the configuration additionally which is not required in the server context
   ;

   const auto sc = ocs::tom::ConstList<ocs::tom::SC>(*DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF)).front();
   if (sc != nullptr) {
      job_load_adjustments_ = ocs::tom::get<SC_job_load_adjustments>(sc).clone("load_adj_copy");
   }
   // An empty list, not nullptr, when none are configured: the sched helpers read nullptr as "fetch them
   // from the data store of the calling thread" (LOAD_ADJUSTMENTS_LATE_FETCH in get_attribute()), which
   // after the early release of the reader lock would read the data store unprotected (CS-3008).
   if (job_load_adjustments_ == nullptr) {
      job_load_adjustments_ = ocs::tom::ListPtr<ocs::tom::CE>::create("load_adj_copy");
   }

   DRETURN(true);
}
