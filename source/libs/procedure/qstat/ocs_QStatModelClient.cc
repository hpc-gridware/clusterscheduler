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
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_log.h"
#include "uti/sge.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_what.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_schedd_conf.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_Request.h"

#include "ocs_QStatModelClient.h"
#include "ocs_QStatParameter.h"


bool ocs::QStatModelClient::fetch_data(lList **answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   gdi::Request gdi_multi{};

   if (!gdi::Client::sge_gdi_get_permission(answer_list, &is_manager_, nullptr, nullptr, nullptr)) {
      DRETURN(false);
   }

   int q_id = -1;
   if (parameter.need_queues_) {
      lEnumeration *queue_what = get_queue_what();
      q_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::CQ_LIST, gdi::Command::GET,
                               gdi::SubCommand::NONE, nullptr, nullptr, queue_what, true);
      lFreeWhat(&queue_what);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   int j_id = -1;
   if (parameter.need_job_list_) {
      lEnumeration *job_what = get_job_what(parameter);
      lCondition *job_where = get_job_where(parameter);
      j_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::JB_LIST, gdi::Command::GET,
                               gdi::SubCommand::NONE, nullptr, job_where, job_what, true);
      lFreeWhere(&job_where);
      lFreeWhat(&job_what);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   lEnumeration *centry_what = get_centry_what();
   const int ce_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::CE_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, nullptr, centry_what, true);
   lFreeWhat(&centry_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   lCondition *ehost_where = get_ehost_where();
   lEnumeration *ehost_what = get_ehost_what();
   const int eh_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::EH_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, ehost_where, ehost_what, true);
   lFreeWhat(&ehost_what);
   lFreeWhere(&ehost_where);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   lEnumeration *pe_what = get_pe_what();
   const int pe_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::PE_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, nullptr, pe_what, true);
   lFreeWhat(&pe_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   lEnumeration *ckpt_what = get_ckpt_what();
   const int ckpt_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::CK_LIST, gdi::Command::GET,
                                         gdi::SubCommand::NONE, nullptr, nullptr, ckpt_what, true);
   lFreeWhat(&ckpt_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   lEnumeration *uset_what = get_uset_what();
   const int acl_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::US_LIST, gdi::Command::GET,
                                        gdi::SubCommand::NONE, nullptr, nullptr, uset_what, true);
   lFreeWhat(&uset_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   lEnumeration *prj_what = get_prj_what();
   const int up_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::PR_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, nullptr, prj_what, true);
   lFreeWhat(&prj_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   lEnumeration *sconf_what = get_sconf_what();
   const int sc_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::SC_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, nullptr, sconf_what, true);
   lFreeWhat(&sconf_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   lEnumeration *hgrp_what = get_hgrp_what();
   int hgrp_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::HGRP_LIST, gdi::Command::GET,
                                   gdi::SubCommand::NONE, nullptr, nullptr, hgrp_what, true);
   lFreeWhat(&hgrp_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   lCondition *conf_where = get_conf_where();
   lEnumeration *conf_what = get_conf_what();
   const int gc_id = gdi_multi.request(answer_list, gdi::Mode::SEND, gdi::Target::CONF_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, conf_where, conf_what, true);
   lFreeWhat(&conf_what);
   lFreeWhere(&conf_where);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   gdi_multi.wait();
   // Start fetching the lists

   if (parameter.need_queues_) {
      gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::CQ_LIST, q_id, &queue_list_);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   if (parameter.need_job_list_) {
      gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::JB_LIST, j_id, &job_list_);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }

      // @todo this will not work as stored procedure
      // debug output to perform testsuite tests
      if (sge_getenv("_SGE_TEST_QSTAT_JOB_STATES") != nullptr) {
         fprintf(stderr, "_SGE_TEST_QSTAT_JOB_STATES: jobs_received=" sge_u32 "\n", lGetNumberOfElem(job_list_));
      }
   }

   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::CE_LIST, ce_id, &centry_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::EH_LIST, eh_id, &exechost_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::PE_LIST, pe_id, &pe_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::CK_LIST, ckpt_id, &ckpt_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::US_LIST, acl_id, &acl_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::PR_LIST, up_id, &project_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::SC_LIST, sc_id, &schedd_config_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::HGRP_LIST, hgrp_id, &hgrp_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   lList *conf_l = nullptr;
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::CONF_LIST, gc_id, &conf_l);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   if (lFirst(conf_l) != nullptr) {
      const lListElem *local = nullptr;
      const char *cell_root = bootstrap_get_cell_root();
      const uint32_t progid = component_get_component_id();
      merge_configuration(nullptr, progid, cell_root, lFirstRW(conf_l), local, nullptr);
   }
   lFreeList(&conf_l);

   DRETURN(true);
}

bool ocs::QStatModelClient::prepare_data(lList **answer_list) {
   DENTER(TOP_LAYER);

   // init the sconf module
   if (!sconf_set_config(&schedd_config_, answer_list)) {
      DRETURN(false);
   }

   // make the centry list ready to get used
   centry_list_init_double(centry_list_);

   DRETURN(true);
}
