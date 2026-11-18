/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Client side model of `qrstat`: fetches the lists over GDI
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_QRStatModelClient.h"

#include "msg_qrstat.h"
#include "ocs_QRStatParameterClient.h"

/** @brief Fetch the advance reservations from qmaster with one GDI request
 *
 * A GET of the AR list answers with a list of that type, so the list is
 * wrapped with a check of its type.
 *
 * @param answer_list receives error messages
 * @param parameter the parsed parameters
 * @return true when reservations were found or a summary was requested
 */
bool ocs::QRStatModelClient::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QRStatParameter& parameter) {
   DENTER(TOP_LAYER);

   lEnumeration *what_ar = get_ar_what(parameter);
   lCondition *where_ar = get_ar_where(parameter);
   lList *ar_list = nullptr;
   *answer_list = gdi::Client::sge_gdi(gdi::Target::AR_LIST, gdi::Command::GET, gdi::SubCommand::NONE, &ar_list,
                                       where_ar, what_ar);
   ar_list_ = ocs::tom::ListPtr<ocs::tom::AR>(ar_list);
   lFreeWhere(&where_ar);
   lFreeWhat(&what_ar);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   if (answer_list != nullptr) {
      answer_list->reset();
   }

   if (!parameter.is_summary() && ar_list_->empty()) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_GDI_AR_DOES_NOT_EXIT);
      DRETURN(false);
   }

   DRETURN(true);
}
