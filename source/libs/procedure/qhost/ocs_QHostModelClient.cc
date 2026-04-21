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

#include "uti/sge_bootstrap_files.h"
#include "uti/sge_hostname.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"

#include "cull/cull.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_Request.h"

#include "qstat/ocs_QStatParameter.h"
#include "qhost/ocs_QHostModelClient.h"

bool
ocs::QHostModelClient::fetch_data(lList **answer_list, const lList *hostname_list, const lList *user_name_list, uint32_t show) {
   DENTER(TOP_LAYER);

   // @todo Should be combined with the other GDI requests
   if (!gdi::Client::sge_gdi_get_permission(answer_list, &is_manager_, nullptr, nullptr, nullptr)) {
      DRETURN(false);
   }

   // execution hosts
   gdi::Request gdi_multi{};
   int eh_id;
   {
      lCondition *where = get_host_where(hostname_list);
      lEnumeration *what = get_host_what();

      eh_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::EH_LIST,
                                gdi::Command::GET, gdi::SubCommand::NONE,
                                nullptr, where, what, true);
      lFreeWhat(&what);
      lFreeWhere(&where);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   // Queues
   int q_id = 0;
   if (show & QHOST_DISPLAY_JOBS || show & QHOST_DISPLAY_QUEUES) {
      lEnumeration *what = get_queue_what();

      q_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::CQ_LIST,
                               gdi::Command::GET, gdi::SubCommand::NONE,
                               nullptr, nullptr, what, true);
      lFreeWhat(&what);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   // Jobs
   int j_id = 0;
   if ((show & QHOST_DISPLAY_JOBS) == QHOST_DISPLAY_JOBS) {
      lCondition *where = get_job_where(user_name_list, show);
      lEnumeration *what = get_job_what();

      j_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::JB_LIST, gdi::Command::GET,
                               gdi::SubCommand::NONE, nullptr, where, what, true);
      lFreeWhat(&what);
      lFreeWhere(&where);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   // Complexes
   int ce_id;
   {
      lEnumeration *what = get_centry_what();
      ce_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::CE_LIST, gdi::Command::GET,
                                gdi::SubCommand::NONE, nullptr, nullptr, what, true);
      lFreeWhat(&what);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   // Parallel environments
   int pe_id;
   {
      lEnumeration *what = get_pe_what();
      pe_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::PE_LIST, gdi::Command::GET,
                                gdi::SubCommand::NONE, nullptr, nullptr, what, true);
      lFreeWhat(&what);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   /*
   ** user list
   */
   int acl_id;
   {
      lEnumeration *what = get_user_set_what();
      acl_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::US_LIST, gdi::Command::GET,
                                 gdi::SubCommand::NONE, nullptr, nullptr, what, true);
      lFreeWhat(&what);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   // Configuration
   int gc_id;
   {
      lCondition *where = lWhere("%T(%I c= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME);
      lEnumeration *what = lWhat("%T(ALL)", CONF_Type);

      gc_id = gdi_multi.request(answer_list, gdi::Mode::SEND, gdi::Target::CONF_LIST, gdi::Command::GET,
                                gdi::SubCommand::NONE, nullptr, where, what, true);
      gdi_multi.wait();
      lFreeWhat(&what);
      lFreeWhere(&where);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   // Delete the ok message
   lFreeList(answer_list);

   /*
   ** handle results
   */

   // Execution hosts
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::EH_LIST, eh_id, &exec_host_list_);
   if (answer_list_has_error(answer_list)) {
      answer_list_output(answer_list);
      DRETURN(false);
   }

   // Cluster queues
   if (show & QHOST_DISPLAY_JOBS || show & QHOST_DISPLAY_QUEUES) {
      gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::CQ_LIST, q_id, &queue_list_);
      if (answer_list_has_error(answer_list)) {
         answer_list_output(answer_list);
         DRETURN(false);
      }
   }

   // Jobs
   if ((show & QHOST_DISPLAY_JOBS) == QHOST_DISPLAY_JOBS) {
      gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::JB_LIST, j_id, &job_list_);
      if (answer_list_has_error(answer_list)) {
         answer_list_output(answer_list);
         DRETURN(false);
      }
   }

   // complexes
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::CE_LIST, ce_id, &centry_list_);
   if (answer_list_has_error(answer_list)) {
      answer_list_output(answer_list);
      DRETURN(false);
   }

   // PE's
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::PE_LIST, pe_id, &pe_list_);
   if (answer_list_has_error(answer_list)) {
      answer_list_output(answer_list);
      DRETURN(false);
   }

   // ACL's
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::US_LIST, acl_id, &acl_list_);
   if (answer_list_has_error(answer_list)) {
      answer_list_output(answer_list);
      DRETURN(false);
   }

   // get configuration
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::CONF_LIST, gc_id, &config_list_);
   if (answer_list_has_error(answer_list)) {
      answer_list_output(answer_list);
      DRETURN(false);
   }

   DRETURN(true);
}

bool
ocs::QHostModelClient::prepare_data(lList **answer_list, const lList *resource_match_list, const uint32_t show) const {
   DENTER(TOP_LAYER);

   // Common things that need to be done on client and server side
   QHostModelBase::prepare_data(answer_list, resource_match_list, show);

   // Prepare the configuration
   if (lFirst(config_list_)) {
      const char *cell_root = bootstrap_get_cell_root();
      const uint32_t progid = component_get_component_id();

      merge_configuration(nullptr, progid, cell_root, lFirstRW(config_list_), nullptr, nullptr);
   }

   DRETURN(true);
}
