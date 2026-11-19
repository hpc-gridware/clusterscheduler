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
 * @brief Server side model of `qhost`: reads the lists inside qmaster
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_schedd_conf.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_SC.h"

#include "qstat/ocs_QStatParameter.h"
#include "ocs_QHostModelBase.h"
#include "ocs_QHostModelServer.h"
#include "procedure/ocs_ProcedureModel.h"
#include "sgeobj/sge_answer.h"


/** @brief Fetch the lists from the master lists of qmaster
 *
 * Every list is a typed selection of a master list, with the results
 * lSelect() gave: an empty handle when nothing was selected, see
 * ocs_procedure_select().
 *
 * @param answer_list receives error messages
 * @param hostname_list the hosts the user asked for; all hosts when empty
 * @param user_name_list the users the user asked for; all users when empty
 * @param show which of queues, jobs and resources were requested
 * @return true
 */
bool ocs::QHostModelServer::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                       const ocs::tom::ConstList<ocs::tom::ST> hostname_list,
                                       const ocs::tom::ConstList<ocs::tom::ST> user_name_list, const uint32_t show) {
   DENTER(TOP_LAYER);

   // Fetch either all host or only the selected ones
   const auto master_host_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
   exec_host_list_ = ocs_procedure_select(master_host_list, get_host_where(hostname_list), get_host_what());

   if (show & QHOST_DISPLAY_JOBS || show & QHOST_DISPLAY_QUEUES) {
      const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
      queue_list_ = ocs_procedure_select(master_cqueue_list, ocs::tom::Where<ocs::tom::CQ>(), get_queue_what());
   }

   if ((show & QHOST_DISPLAY_JOBS) == QHOST_DISPLAY_JOBS) {
      const auto master_job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();
      job_list_ = ocs_procedure_select(master_job_list, get_job_where(user_name_list, show), get_job_what());
   }

   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   centry_list_ = ocs_procedure_select(master_centry_list, ocs::tom::Where<ocs::tom::CE>(), get_centry_what());

   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   pe_list_ = ocs_procedure_select(master_pe_list, ocs::tom::Where<ocs::tom::PE>(), get_pe_what());

   const auto sc = ocs::DataStore::master_list<SGE_TYPE_SCHEDD_CONF>().front();
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
