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
 * This code was generated from file source/libs/sgeobj/json/HL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief host load value: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of HL, host load value
 *
 * The enumeration is the type of every field number of HL, so a parameter
 * of this type takes the field numbers of HL only.
 */
enum HL_Field {
   /**
    * @brief name of the load variable
    *
    * this is the name of the complex variable representing the load value
    */
   HL_name = HL_LOWERBOUND,
   /**
    * @brief value of the load variable
    *
    * value of the load variable as string
    */
   HL_value,
   /**
    * @brief date/time of last update
    *
    * timestamp (seconds since epoch) when the load value was last updated
    */
   HL_last_update,
   /**
    * @brief is it a static load value?
    *
    * true if it is a static load value, else false
    * a static load value is a value which is unlikely to change, e.g.
    *  - arch
    *  - num_proc
    *  - mem_total
    * static load values are spooled and therefore are available even if an execution host is down
    */
   HL_is_static,
};

namespace ocs::tom {

   /**
    * @brief host load value
    *
    * an object of this type represents a single load value
    */
   struct HL {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {HL_name, "HL_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {HL_value, "HL_value", FieldType::String, Flag::SpoolSublist},
         {HL_last_update, "HL_last_update", FieldType::Uint64, Flag::None},
         {HL_is_static, "HL_is_static", FieldType::Bool, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of HL belongs to HL */
   template <>
   struct TypeOf<HL_Field> {
      using type = HL;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of HL for the list library: a synonym for ocs::tom::HL::descriptor */
inline constexpr const lDescr *HL_Type = ocs::tom::HL::descriptor.e;
