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
#include "sgeobj/sge_answer.h"


/** @brief Fetch the lists from the master lists of qmaster
 *
 * Every list is a selection of a master list of a fixed type, so it is
 * wrapped with a check of its type.
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
   lEnumeration *host_what = get_host_what();
   lCondition *host_where = get_host_where(hostname_list);
   exec_host_list_ = ocs::tom::ListPtr<ocs::tom::EH>(lSelect("", master_host_list, host_where, host_what));
   lFreeWhat(&host_what);
   lFreeWhere(&host_where);

   if (show & QHOST_DISPLAY_JOBS || show & QHOST_DISPLAY_QUEUES) {
      const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
      lEnumeration *queue_what = get_queue_what();
      queue_list_ = ocs::tom::ListPtr<ocs::tom::CQ>(lSelect("", master_cqueue_list, nullptr, queue_what));
      lFreeWhat(&queue_what);
   }

   if ((show & QHOST_DISPLAY_JOBS) == QHOST_DISPLAY_JOBS) {
      const auto master_job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();
      lEnumeration *job_what = get_job_what();
      lCondition *job_where = get_job_where(user_name_list, show);
      job_list_ = ocs::tom::ListPtr<ocs::tom::JB>(lSelect("", master_job_list, job_where, job_what));
      lFreeWhere(&job_where);
      lFreeWhat(&job_what);
   }

   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   lEnumeration *centry_what = get_centry_what();
   centry_list_ = ocs::tom::ListPtr<ocs::tom::CE>(lSelect("", master_centry_list, nullptr, centry_what));
   lFreeWhat(&centry_what);

   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   lEnumeration *pe_what = get_pe_what();
   pe_list_ = ocs::tom::ListPtr<ocs::tom::PE>(lSelect("", master_pe_list, nullptr, pe_what));
   lFreeWhat(&pe_what);

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
