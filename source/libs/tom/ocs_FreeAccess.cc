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
 * @brief The cold paths of the free forms of the keyed operations (CS-3045): the old messages and the soft paths
 */

#include "tom/ocs_FreeAccess.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "tom/msg_tom.h"
#include "tom/ocs_Model.h"

namespace ocs::tom {

   void report_missing_field(const char *function, const int nm, const ocs::tom::FieldDescr *descr) noexcept {
      // a reduced descriptor has a position for the field, but no value; any other descriptor is of another type
      const SoftPathReason reason =
         field_pos(descr, nm) >= 0 ? SoftPathReason::MissingField : SoftPathReason::ForeignField;
      report_soft_path(function, nm, reason);
   }

   void missing_key_field(const KeyOp op, const char *function, const int nm, const ocs::tom::FieldDescr *descr) {
      DENTER(TOM_LAYER);
      report_missing_field(function, nm, descr);
      switch (op) {
         case KeyOp::FindStr:
         case KeyOp::FindLike:
            CRITICAL(MSG_TOM_XNOTFOUNDINELEMENT_S, ocs::tom::field_name(nm));
            break;
         case KeyOp::FindHost:
            CRITICAL(MSG_TOM_GETELEMHOSTERRORXRUNTIMETYPE_S, ocs::tom::field_name(nm));
            break;
         case KeyOp::FindUlong:
            CRITICAL(MSG_TOM_GETELEMULONGERRORXRUNTIMETYPE_S, ocs::tom::field_name(nm));
            break;
         case KeyOp::FindUlong64:
            CRITICAL(MSG_TOM_GETELEMULONG64ERRORXRUNTIMETYPE_S, ocs::tom::field_name(nm));
            break;
         case KeyOp::FindNocase:
            CRITICAL(MSG_TOM_GETELEMCASESTRERRORXRUNTIMETYPE_S, ocs::tom::field_name(nm));
            break;
         case KeyOp::AddStr:
            CRITICAL(MSG_TOM_ADDELEMSTRERRORXRUNTIMETYPE_S, ocs::tom::field_name(nm));
            break;
         case KeyOp::AddHost:
            CRITICAL(MSG_TOM_ADDELEMHOSTERRORXRUNTIMETYPE_S, ocs::tom::field_name(nm));
            break;
         case KeyOp::AddUlong:
            CRITICAL(MSG_TOM_ADDELEMULONGERRORXRUNTIMETYPE_S, ocs::tom::field_name(nm));
            break;
         case KeyOp::AddUlong64:
            CRITICAL(MSG_TOM_ADDELEMULONG64ERRORXRUNTIMETYPE_S, ocs::tom::field_name(nm));
            break;
      }
      DRETURN_VOID;
   }

} // namespace ocs::tom
