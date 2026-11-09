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
 * This code was generated from file source/libs/sgeobj/json/UPP.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Project Usage: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of UPP, Project Usage
 *
 * The enumeration is the type of every field number of UPP, so a parameter
 * of this type takes the field numbers of UPP only.
 */
enum UPP_Field {
   /**
    * @brief Name
    *
    * Project name.
    */
   UPP_name = UPP_LOWERBOUND,
   /**
    * @brief Usage
    *
    * UA_Type; decayed usage.
    * Set and used by SGEEE scheduler stored to qmaster; spooled.
    */
   UPP_usage,
   /**
    * @brief Long Term Usage
    *
    * UA_Type; long term accumulated non-decayed usage.
    * Set by SGEEE scheduler stored to qmaster; spooled
    */
   UPP_long_term_usage,
};

namespace ocs::tom {

   struct UA;

   /**
    * @brief Project Usage
    *
    * This is the project usage list type we use to hold the usage for a user on a project basis.
    * Each entry contains a project name and a usage list.
    */
   struct UPP {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {UPP_name, "UPP_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {UPP_usage, "UPP_usage", FieldType::List, Flag::SpoolSublist},
         {UPP_long_term_usage, "UPP_long_term_usage", FieldType::List, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of UPP belongs to UPP */
   template <>
   struct TypeOf<UPP_Field> {
      using type = UPP;   ///< the object type
   };

   /** @brief UPP_usage holds elements of UA */
   template <>
   struct SubType<UPP_usage> {
      using type = UA;   ///< the sub-type
   };

   /** @brief UPP_long_term_usage holds elements of UA */
   template <>
   struct SubType<UPP_long_term_usage> {
      using type = UA;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of UPP for the list library: a synonym for ocs::tom::UPP::descriptor */
inline constexpr const lDescr *UPP_Type = ocs::tom::UPP::descriptor.e;
