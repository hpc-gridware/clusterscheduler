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

#include "ocs_QStatModelServer.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_manop.h"

bool
ocs::QStatModelServer::fetch_data(lList **answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   is_manager_ = manop_is_manager(packet);
   const lList *master_job_list = *DataStore::get_master_list(SGE_TYPE_JOB);

   lEnumeration* sme_what = get_sme_what();
   const lList *master_sme_list = *DataStore::get_master_list(SGE_TYPE_JOB_SCHEDD_INFO);
   ilp = lSelect("", master_sme_list, nullptr, sme_what);
   lFreeWhat(&sme_what);

   if (parameter.get_jid_list() != nullptr) {
      lCondition *job_view_where = get_job_view_where(parameter.get_jid_list());
      lEnumeration *job_view_what = get_job_view_what();
      jlp = lSelect("", master_job_list, job_view_where, job_view_what);
      lFreeWhere(&job_view_where);
      lFreeWhat(&job_view_what);
   }

   if (parameter.need_queues_) {
      const lList *master_cqueue_list = *DataStore::get_master_list(SGE_TYPE_CQUEUE);
      lEnumeration *queue_what = get_queue_what();
      queue_list_ = lSelect("", master_cqueue_list, nullptr, queue_what);
      lFreeWhat(&queue_what);
   }

   if (parameter.need_job_list_) {
      lEnumeration *job_what = get_job_what(parameter);
      lCondition *job_where = get_job_where(parameter);
      job_list_ = lSelect("", master_job_list, job_where, job_what);
      lFreeWhere(&job_where);
      lFreeWhat(&job_what);
   }

   const lList *master_centry_list = *DataStore::get_master_list(SGE_TYPE_CENTRY);
   lEnumeration *centry_what = get_centry_what();
   centry_list_ = lSelect("", master_centry_list, nullptr, centry_what);
   lFreeWhat(&centry_what);

   const lList *master_ehost_list = *DataStore::get_master_list(SGE_TYPE_EXECHOST);
   lCondition *ehost_where = get_ehost_where();
   lEnumeration *ehost_what = get_ehost_what();
   exechost_list_ = lSelect("", master_ehost_list, ehost_where, ehost_what);
   lFreeWhat(&ehost_what);
   lFreeWhere(&ehost_where);

   const lList *master_pe_list = *DataStore::get_master_list(SGE_TYPE_PE);
   lEnumeration *pe_what = get_pe_what();
   pe_list_ = lSelect("", master_pe_list, nullptr, pe_what);
   lFreeWhat(&pe_what);

   const lList *master_ckpt_list = *DataStore::get_master_list(SGE_TYPE_CKPT);
   lEnumeration *ckpt_what = get_ckpt_what();
   ckpt_list_ = lSelect("", master_ckpt_list, nullptr, ckpt_what);
   lFreeWhat(&ckpt_what);

   const lList *master_uset_list = *DataStore::get_master_list(SGE_TYPE_USERSET);
   lEnumeration *uset_what = get_uset_what();
   acl_list_ = lSelect("", master_uset_list, nullptr, uset_what);
   lFreeWhat(&uset_what);

   const lList *master_prj_list = *DataStore::get_master_list(SGE_TYPE_PROJECT);
   lEnumeration *prj_what = get_prj_what();
   project_list_ = lSelect("", master_prj_list, nullptr, prj_what);
   lFreeWhat(&prj_what);

   const lList *master_sconf_list = *DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF);
   lEnumeration *sconf_what = get_sconf_what();
   schedd_config_ = lSelect("", master_sconf_list, nullptr, sconf_what);
   lFreeWhat(&sconf_what);

   const lList *master_hgrp_list = *DataStore::get_master_list(SGE_TYPE_HGROUP);
   lEnumeration *hgrp_what = get_hgrp_what();
   hgrp_list_ = lSelect("", master_hgrp_list, nullptr, hgrp_what);
   lFreeWhat(&hgrp_what);

   DRETURN(true);
}

