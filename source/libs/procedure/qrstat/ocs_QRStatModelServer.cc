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

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"

#include "ocs_QRStatModelServer.h"

#include "msg_common.h"

bool ocs::QRStatModelServer::fetch_data(lList **answer_list, QRStatParameter& parameter) {
   DENTER(TOP_LAYER);
   const lList *master_ar_list = *DataStore::get_master_list(SGE_TYPE_AR);

   lEnumeration *ar_what = get_ar_what(parameter);
   lCondition *ar_where = get_ar_where(parameter);
   ar_list_ = lSelect("", master_ar_list, ar_where, ar_what);
   lFreeWhat(&ar_what);
   lFreeWhere(&ar_where);

   if (!parameter.is_summary() && lGetNumberOfElem(ar_list_) == 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_GDI_AR_DOES_NOT_EXIT);
      DRETURN(false);
   }

   DRETURN(true);
}
