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
 * @brief Client side model of `qquota`: fetches the lists over GDI
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"

#include "gdi/ocs_gdi_Request.h"

#include "ocs_QQuotaModelClient.h"

#include "ocs_QQuotaParameter.h"

bool ocs::QQuotaModelClient::fetch_data(lList **answer_list, const lList *host_list) {
   DENTER(TOP_LAYER);

   gdi::Request gdi_multi{};

   // RQS
   lEnumeration *rqs_what = get_rqs_what();
   const int rqs_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::RQS_LIST,
                                        gdi::Command::GET, gdi::SubCommand::NONE,
                                        nullptr, nullptr, rqs_what, true);
   lFreeWhat(&rqs_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   // CEntry
   lEnumeration *ce_what = get_centry_what();
   const int ce_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::CE_LIST,
                                       gdi::Command::GET, gdi::SubCommand::NONE,
                                       nullptr, nullptr, ce_what, true);
   lFreeWhat(&ce_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   // User Set
   lEnumeration *user_set_what = get_user_set_what();
   const int userset_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::US_LIST,
                                            gdi::Command::GET, gdi::SubCommand::NONE,
                                            nullptr, nullptr, user_set_what, true);
   lFreeWhat(&user_set_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   // Hostgroup
   lEnumeration *hgroup_what = get_hgroup_what();
   const int hgroup_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::HGRP_LIST,
                                           gdi::Command::GET, gdi::SubCommand::NONE,
                                           nullptr, nullptr, hgroup_what, true);
   lFreeWhat(&hgroup_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }


   // Hosts (fetch all specified + global host but not the template host)
   lCondition *eh_where = get_host_where(host_list);
   lEnumeration *eh_what = get_host_what();
   const int eh_id = gdi_multi.request(answer_list, gdi::Mode::SEND, gdi::Target::EH_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, eh_where, eh_what, true);
   lFreeWhat(&eh_what);
   lFreeWhere(&eh_where);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   gdi_multi.wait();

   /* --- resource quota sets */
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::RQS_LIST, rqs_id, &rqs_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /* --- complex attribute */
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::CE_LIST, ce_id, &centry_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   /* --- usersets */
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::US_LIST, userset_id, &user_set_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   /* --- hostgroups */
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::HGRP_LIST, hgroup_id, &hgroup_list_);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   /* --- exec hosts*/
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::GET, gdi::SubCommand::NONE, gdi::Target::EH_LIST, eh_id, &hgroup_list_);

   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   DRETURN(true);
}