#pragma once
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

/*
 * This code was generated from file source/libs/sgeobj/json/QIM.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Queue Instance Message: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of QIM, Queue Instance Message
 *
 * The enumeration is the type of every field number of QIM, so a parameter
 * of this type takes the field numbers of QIM only.
 */
enum QIM_Field {
   /**
    * @brief Message Type
    *
    * QI states defined in libs/sgeobj/sge_qinstance_state.h, e.g. QI_ERROR, QI_AMBIGUOUS, ...
    * Seems to be always 1 for ja_task messages.
    */
   QIM_type = QIM_LOWERBOUND,
   /**
    * @brief Message
    *
    * Message as string.
    */
   QIM_message,
};

namespace ocs::tom {

   /**
    * @brief Queue Instance Message
    *
    * Objects of this type hold messages attached to queues (QU_message_list).
    * It is also used for ja_task specific messages (JAT_message_list), e.g. job error messages
    */
   struct QIM {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {QIM_type, "QIM_type", FieldType::Uint32, Flag::Spool},
         {QIM_message, "QIM_message", FieldType::String, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of QIM belongs to QIM */
   template <>
   struct TypeOf<QIM_Field> {
      using type = QIM;   ///< the object type
   };

} // namespace ocs::tom
