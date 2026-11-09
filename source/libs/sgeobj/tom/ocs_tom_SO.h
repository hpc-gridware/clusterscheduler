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
 * This code was generated from file source/libs/sgeobj/json/SO.json
 * DO NOT CHANGE
 */

/** @file
 * @brief SubordinateQueue: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SO, SubordinateQueue
 *
 * The enumeration is the type of every field number of SO, so a parameter
 * of this type takes the field numbers of SO only.
 */
enum SO_Field {
   /**
    * @brief Subordinate Queue Name
    *
    * Name of the subordinate queue.
    */
   SO_name = SO_LOWERBOUND,
   /**
    * @brief Threshold
    *
    * The threshold (slots) defines when the subordination action (suspend) will be triggered.
    */
   SO_threshold,
   /**
    * @brief Slots Sum
    *
    * Used for slot-wise SOS.
    */
   SO_slots_sum,
   /**
    * @brief Sequence Number
    *
    * Used for slot-wise SOS.
    */
   SO_seq_no,
   /**
    * @brief Action
    *
    * Subordination action, ussed for slot-wise SOS:
    * - SO_ACTION_SR: suspend the task with the shortest runtime
    * - SO_ACTION_LR: suspend the task with the longest runtime
    */
   SO_action,
};

namespace ocs::tom {

   /**
    * @brief SubordinateQueue
    *
    * Subordinate (queue instances) are defined in a sub list of a queue instance.
    * We want to use the configuration parameter subordinate_list
    * for both the classic queue instance-wise suspend on subordinate
    * and the slot-wise suspend on subordinate.
    * The fields SO_name and SO_threshold are used by the queue instance-wise
    * suspend on subordinate, SO_name, SO_slots_sum, SO_seq_no and SO_action
    * are used by the slot-wise suspend on subordinate.
    * If SO_slots_sum is 0, it's queue instance-wise, otherwise slot-wise
    * suspend on subordinate that is configured.
    */
   struct SO {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SO_name, "SO_name", FieldType::String, Flag::PrimaryKey | Flag::SpoolSublist},
         {SO_threshold, "SO_threshold", FieldType::Uint32, Flag::SpoolSublist},
         {SO_slots_sum, "SO_slots_sum", FieldType::Uint32, Flag::SpoolSublist},
         {SO_seq_no, "SO_seq_no", FieldType::Uint32, Flag::SpoolSublist},
         {SO_action, "SO_action", FieldType::Uint32, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SO belongs to SO */
   template <>
   struct TypeOf<SO_Field> {
      using type = SO;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of SO for the list library: a synonym for ocs::tom::SO::descriptor */
inline constexpr const lDescr *SO_Type = ocs::tom::SO::descriptor.e;
