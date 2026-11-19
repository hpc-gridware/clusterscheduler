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
 * @brief Server side model of `qstat`: reads the lists inside qmaster
 */

#include "uti/sge_rmon_macros.h"

#include "ocs_QStatModelServer.h"
#include "procedure/ocs_ProcedureModel.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_schedd_conf.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/sge_answer.h"

/** @brief Fetch the lists from the master lists of qmaster
 *
 * Every list is a typed selection of a master list, with the results
 * lSelect() gave: an empty handle when nothing was selected, see
 * ocs_procedure_select().
 *
 * @param answer_list receives error messages
 * @param parameter the parsed parameters
 * @return true
 */
bool
ocs::QStatModelServer::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   is_manager_ = manop_is_manager(packet);
   const auto master_job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();

   const auto master_sme_list = ocs::DataStore::master_list<SGE_TYPE_JOB_SCHEDD_INFO>();
   ilp = ocs_procedure_select(master_sme_list, ocs::tom::Where<ocs::tom::SME>(), get_sme_what());

   if (parameter.get_jid_list() != nullptr) {
      jlp = ocs_procedure_select(master_job_list,
                                 get_job_view_where(parameter.get_jid_list(), parameter.get_user_list()),
                                 get_job_view_what());
   }

   if (parameter.need_queues_) {
      const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
      cqueue_list_ = ocs_procedure_select(master_cqueue_list, ocs::tom::Where<ocs::tom::CQ>(), get_queue_what());
   }

   if (parameter.need_job_list_) {
      job_list_ = ocs_procedure_select(master_job_list, get_job_where(parameter), get_job_what(parameter));
   }

   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   centry_list_ = ocs_procedure_select(master_centry_list, ocs::tom::Where<ocs::tom::CE>(), get_centry_what());

   const auto master_ehost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
   exechost_list_ = ocs_procedure_select(master_ehost_list, get_ehost_where(), get_ehost_what());

   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   pe_list_ = ocs_procedure_select(master_pe_list, ocs::tom::Where<ocs::tom::PE>(), get_pe_what());

   const auto master_ckpt_list = ocs::DataStore::master_list<SGE_TYPE_CKPT>();
   ckpt_list_ = ocs_procedure_select(master_ckpt_list, ocs::tom::Where<ocs::tom::CK>(), get_ckpt_what());

   const auto master_uset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   acl_list_ = ocs_procedure_select(master_uset_list, ocs::tom::Where<ocs::tom::US>(), get_uset_what());

   const auto master_prj_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();
   project_list_ = ocs_procedure_select(master_prj_list, ocs::tom::Where<ocs::tom::PR>(), get_prj_what());

   const auto master_sconf_list = ocs::DataStore::master_list<SGE_TYPE_SCHEDD_CONF>();
   schedd_config_ = ocs_procedure_select(master_sconf_list, ocs::tom::Where<ocs::tom::SC>(), get_sconf_what());
   if (const auto sc = schedd_config_->front(); sc != nullptr) {
      job_load_adjustments_ = ocs::tom::get<SC_job_load_adjustments>(sc).clone("load_adj_copy");
   }
   // An empty list, not nullptr, when none are configured: the sched helpers read nullptr as "fetch them
   // from the data store of the calling thread" (LOAD_ADJUSTMENTS_LATE_FETCH in get_attribute()), which
   // after the early release of the reader lock would read the data store unprotected (CS-3008).
   if (job_load_adjustments_ == nullptr) {
      job_load_adjustments_ = ocs::tom::ListPtr<ocs::tom::CE>::create("load_adj_copy");
   }

   // the access lists of the global configuration of qmaster
   config_user_lists_ = mconf_get_user_lists();
   config_xuser_lists_ = mconf_get_xuser_lists();

   const auto master_hgrp_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();
   hgrp_list_ = ocs_procedure_select(master_hgrp_list, ocs::tom::Where<ocs::tom::HGRP>(), get_hgrp_what());

   DRETURN(true);
}

