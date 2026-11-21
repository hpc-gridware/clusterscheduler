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
 * This code was generated from file source/libs/sgeobj/json/XMLA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief XML Attribute: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of XMLA, XML Attribute
 *
 * The enumeration is the type of every field number of XMLA, so a parameter
 * of this type takes the field numbers of XMLA only.
 */
enum XMLA_Field {
   /**
    * @brief Name
    *
    * The attribute name.
    */
   XMLA_Name = XMLA_LOWERBOUND,
   /**
    * @brief Value
    *
    * Its value. Escaped when the document is written, so plain text is passed in.
    */
   XMLA_Value,
};

namespace ocs::tom {

   /**
    * @brief XML Attribute
    *
    * One name/value attribute on an XML element.
    */
   struct XMLA {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {XMLA_Name, "XMLA_Name", FieldType::String, Flag::None},
         {XMLA_Value, "XMLA_Value", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of XMLA belongs to XMLA */
   template <>
   struct TypeOf<XMLA_Field> {
      using type = XMLA;   ///< the object type
   };

} // namespace ocs::tom
