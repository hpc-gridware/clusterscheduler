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
 * This code was generated from file source/libs/sgeobj/json/QRL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Queue Reference: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of QRL, Queue Reference
 *
 * The enumeration is the type of every field number of QRL, so a parameter
 * of this type takes the field numbers of QRL only.
 */
enum QRL_Field {
   /**
    * @brief Queue Instance
    *
    * Reference to the queue instance (`QU`). Not owned by this element.
    */
   QRL_queue = QRL_LOWERBOUND,
};

namespace ocs::tom {

   /**
    * @brief Queue Reference
    *
    * A bare reference to a queue instance, for building the scheduler's temporary candidate lists.
    */
   struct QRL {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {QRL_queue, "QRL_queue", FieldType::Ref, Flag::NoTransfer},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of QRL belongs to QRL */
   template <>
   struct TypeOf<QRL_Field> {
      using type = QRL;   ///< the object type
   };

} // namespace ocs::tom
