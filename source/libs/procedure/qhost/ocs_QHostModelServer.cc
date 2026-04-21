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

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_manop.h"

#include "qstat/ocs_QStatParameter.h"
#include "ocs_QHostModelBase.h"
#include "ocs_QHostModelServer.h"


bool ocs::QHostModelServer::fetch_data(lList **answer_list, const lList *hostname_list, const lList *user_name_list, uint32_t show) {
   DENTER(TOP_LAYER);

   // Fetch either all host or only the selected ones
   const lList *master_host_list = *DataStore::get_master_list(SGE_TYPE_EXECHOST);
   lEnumeration *host_what = get_host_what();
   lCondition *host_where = get_host_where(hostname_list);
   exec_host_list_ = lSelect("", master_host_list, host_where, host_what);
   lFreeWhat(&host_what);
   lFreeWhere(&host_where);

   if (show & QHOST_DISPLAY_JOBS || show & QHOST_DISPLAY_QUEUES) {
      const lList *master_cqueue_list = *DataStore::get_master_list(SGE_TYPE_CQUEUE);
      lEnumeration *queue_what = get_queue_what();
      queue_list_ = lSelect("", master_cqueue_list, nullptr, queue_what);
      lFreeWhat(&queue_what);
   }

   if ((show & QHOST_DISPLAY_JOBS) == QHOST_DISPLAY_JOBS) {
      const lList *master_job_list = *DataStore::get_master_list(SGE_TYPE_JOB);
      lEnumeration *job_what = get_job_what();
      lCondition *job_where = get_job_where(user_name_list, show);
      job_list_ = lSelect("", master_job_list, job_where, job_what);
      lFreeWhere(&job_where);
      lFreeWhat(&job_what);
   }

   const lList *master_centry_list = *DataStore::get_master_list(SGE_TYPE_CENTRY);
   lEnumeration *centry_what = get_centry_what();
   centry_list_ = lSelect("", master_centry_list, nullptr, centry_what);
   lFreeWhat(&centry_what);

   lWriteListTo(centry_list_, stderr);

   const lList *master_pe_list = *DataStore::get_master_list(SGE_TYPE_PE);
   lEnumeration *pe_what = get_pe_what();
   pe_list_ = lSelect("", master_pe_list, nullptr, pe_what);
   lFreeWhat(&pe_what);

   DRETURN(true);
}
