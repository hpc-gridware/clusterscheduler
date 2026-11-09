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
 * This code was generated from file source/libs/sgeobj/json/CTI.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Category Ignore Entry: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of CTI, Category Ignore Entry
 *
 * The enumeration is the type of every field number of CTI, so a parameter
 * of this type takes the field numbers of CTI only.
 */
enum CTI_Field {
   /**
    * @brief Name
    *
    * The cluster queue or host to skip for this category.
    */
   CTI_name = CTI_LOWERBOUND,
};

namespace ocs::tom {

   /**
    * @brief Category Ignore Entry
    *
    * One name a category has already been rejected by, held in the ignore lists of `CCT_Type`.
    */
   struct CTI {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CTI_name, "CTI_name", FieldType::String, Flag::Unique | Flag::Hash},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CTI belongs to CTI */
   template <>
   struct TypeOf<CTI_Field> {
      using type = CTI;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of CTI for the list library: a synonym for ocs::tom::CTI::descriptor */
inline constexpr const lDescr *CTI_Type = ocs::tom::CTI::descriptor.e;
