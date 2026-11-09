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
 * This code was generated from file source/libs/sgeobj/json/XMLS.json
 * DO NOT CHANGE
 */

/** @file
 * @brief XML Stylesheet: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of XMLS, XML Stylesheet
 *
 * The enumeration is the type of every field number of XMLS, so a parameter
 * of this type takes the field numbers of XMLS only.
 */
enum XMLS_Field {
   /**
    * @brief Name
    *
    * The instruction's attribute name, e.g. `href`.
    */
   XMLS_Name = XMLS_LOWERBOUND,
   /**
    * @brief Value
    *
    * Its value, e.g. the stylesheet URL.
    */
   XMLS_Value,
   /**
    * @brief Version
    *
    * The stylesheet version.
    */
   XMLS_Version,
};

namespace ocs::tom {

   /**
    * @brief XML Stylesheet
    *
    * One stylesheet processing instruction for the document header.
    */
   struct XMLS {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {XMLS_Name, "XMLS_Name", FieldType::String, Flag::None},
         {XMLS_Value, "XMLS_Value", FieldType::String, Flag::None},
         {XMLS_Version, "XMLS_Version", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of XMLS belongs to XMLS */
   template <>
   struct TypeOf<XMLS_Field> {
      using type = XMLS;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of XMLS for the list library: a synonym for ocs::tom::XMLS::descriptor */
inline constexpr const lDescr *XMLS_Type = ocs::tom::XMLS::descriptor.e;
