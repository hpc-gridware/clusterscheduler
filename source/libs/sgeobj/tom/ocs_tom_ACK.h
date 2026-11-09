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
 * This code was generated from file source/libs/sgeobj/json/ACK.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Acknowledgement: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of ACK, Acknowledgement
 *
 * The enumeration is the type of every field number of ACK, so a parameter
 * of this type takes the field numbers of ACK only.
 */
enum ACK_Field {
   /**
    * @brief Acknowledgement Type
    *
    * What is being acknowledged, e.g. a job report or a signal. Dispatched on by the receiver.
    */
   ACK_type = ACK_LOWERBOUND,
   /**
    * @brief First Id
    *
    * Primary key of the acknowledged thing, usually a job id.
    */
   ACK_id,
   /**
    * @brief Second Id
    *
    * Secondary key, usually an array task id.
    */
   ACK_id2,
   /**
    * @brief Name
    *
    * String key, used where the acknowledged thing is named rather than numbered.
    */
   ACK_str,
};

namespace ocs::tom {

   /**
    * @brief Acknowledgement
    *
    * A short acknowledgement message, identifying what is being acknowledged rather than carrying data.
    */
   struct ACK {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ACK_type, "ACK_type", FieldType::Uint32, Flag::None},
         {ACK_id, "ACK_id", FieldType::Uint32, Flag::None},
         {ACK_id2, "ACK_id2", FieldType::Uint32, Flag::None},
         {ACK_str, "ACK_str", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ACK belongs to ACK */
   template <>
   struct TypeOf<ACK_Field> {
      using type = ACK;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of ACK for the list library: a synonym for ocs::tom::ACK::descriptor */
inline constexpr const lDescr *ACK_Type = ocs::tom::ACK::descriptor.e;
