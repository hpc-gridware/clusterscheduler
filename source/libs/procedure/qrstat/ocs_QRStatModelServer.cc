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
 * @brief Server side model of `qrstat`: reads the lists inside qmaster
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"

#include "ocs_QRStatModelServer.h"

#include "msg_qrstat.h"

/** @brief Fetch the advance reservations from the master list of qmaster
 *
 * The list is a selection of the master list, of a fixed type, so it is
 * wrapped with a check of its type.
 *
 * @param answer_list receives error messages
 * @param parameter the parsed parameters
 * @return true when reservations were found or a summary was requested
 */
bool ocs::QRStatModelServer::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QRStatParameter& parameter) {
   DENTER(TOP_LAYER);
   const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();

   lEnumeration *ar_what = get_ar_what(parameter);
   lCondition *ar_where = get_ar_where(parameter);
   ar_list_ = ocs::tom::ListPtr<ocs::tom::AR>(lSelect("", master_ar_list, ar_where, ar_what));
   lFreeWhat(&ar_what);
   lFreeWhere(&ar_where);

   if (!parameter.is_summary() && ar_list_->empty()) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_GDI_AR_DOES_NOT_EXIT);
      DRETURN(false);
   }

   DRETURN(true);
}
