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
 * This code was generated from file source/libs/sgeobj/json/XMLE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief XML Element: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of XMLE, XML Element
 *
 * The enumeration is the type of every field number of XMLE, so a parameter
 * of this type takes the field numbers of XMLE only.
 */
enum XMLE_Field {
   /**
    * @brief Attributes
    *
    * The element's XML attributes (`XMLA`).
    */
   XMLE_Attribute = XMLE_LOWERBOUND,
   /**
    * @brief Print
    *
    * Whether to emit this element. A node can be built and then suppressed, so the tree does not have to be pruned.
    */
   XMLE_Print,
   /**
    * @brief Value
    *
    * The element's own value, when it is a leaf.
    */
   XMLE_Element,
   /**
    * @brief Children
    *
    * The child elements (`XMLE`), when it is not.
    */
   XMLE_List,
};

namespace ocs::tom {

   struct XMLA;

   /**
    * @brief XML Element
    *
    * One node of the XML document being built: its attributes, its children, and whether it is printed at all.
    */
   struct XMLE {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {XMLE_Attribute, "XMLE_Attribute", FieldType::List, Flag::None},
         {XMLE_Print, "XMLE_Print", FieldType::Bool, Flag::None},
         {XMLE_Element, "XMLE_Element", FieldType::Object, Flag::None},
         {XMLE_List, "XMLE_List", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of XMLE belongs to XMLE */
   template <>
   struct TypeOf<XMLE_Field> {
      using type = XMLE;   ///< the object type
   };

   /** @brief XMLE_Attribute holds elements of XMLA */
   template <>
   struct SubType<XMLE_Attribute> {
      using type = XMLA;   ///< the sub-type
   };

} // namespace ocs::tom
