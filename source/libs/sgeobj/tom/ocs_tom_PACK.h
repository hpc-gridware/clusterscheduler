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
 * This code was generated from file source/libs/sgeobj/json/PACK.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Packed Condition: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of PACK, Packed Condition
 *
 * The enumeration is the type of every field number of PACK, so a parameter
 * of this type takes the field numbers of PACK only.
 */
enum PACK_Field {
   /**
    * @brief Kind
    *
    * `SGE_WHERE` or `SGE_WHAT`, saying which of the two the packed data is.
    */
   PACK_id = PACK_LOWERBOUND,
   /**
    * @brief Packed Data
    *
    * The condition in packed form.
    */
   PACK_string,
};

namespace ocs::tom {

   /**
    * @brief Packed Condition
    *
    * A `where` or `what` condition in its packed form, so it can be sent over GDI.
    * The scheduler and the clients build `lCondition` and `lEnumeration` structures locally; those cannot be sent as they are, so they are packed into this and unpacked on the other side.
    */
   struct PACK {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PACK_id, "PACK_id", FieldType::Uint32, Flag::None},
         {PACK_string, "PACK_string", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PACK belongs to PACK */
   template <>
   struct TypeOf<PACK_Field> {
      using type = PACK;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of PACK for the list library: a synonym for ocs::tom::PACK::descriptor */
inline constexpr const lDescr *PACK_Type = ocs::tom::PACK::descriptor.e;
