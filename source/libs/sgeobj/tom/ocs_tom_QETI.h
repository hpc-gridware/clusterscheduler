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
 * This code was generated from file source/libs/sgeobj/json/QETI.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Queue End Time Iterator: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of QETI, Queue End Time Iterator
 *
 * The enumeration is the type of every field number of QETI, so a parameter
 * of this type takes the field numbers of QETI only.
 */
enum QETI_Field {
   /**
    * @brief Total Capacity
    *
    * The resource's full capacity, the value utilization is compared against.
    */
   QETI_total = QETI_LOWERBOUND,
   /**
    * @brief Resource Instance
    *
    * Reference to the resource being walked (`RUE`), whose utilization diagram holds the bookings.
    */
   QETI_resource_instance,
   /**
    * @brief Next Position
    *
    * Cursor into that utilization diagram: the next booking end this iterator will report.
    */
   QETI_queue_end_next,
};

namespace ocs::tom {

   /**
    * @brief Queue End Time Iterator
    *
    * Walks the points in time at which a resource frees up, newest first.
    * Reservation scheduling has to answer "when could this job start". Rather than sampling time, the iterator visits only the instants at which some booking ends, because the answer can only change there.
    */
   struct QETI {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {QETI_total, "QETI_total", FieldType::Double, Flag::None},
         {QETI_resource_instance, "QETI_resource_instance", FieldType::Ref, Flag::NoTransfer},
         {QETI_queue_end_next, "QETI_queue_end_next", FieldType::Ref, Flag::NoTransfer},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of QETI belongs to QETI */
   template <>
   struct TypeOf<QETI_Field> {
      using type = QETI;   ///< the object type
   };

} // namespace ocs::tom
