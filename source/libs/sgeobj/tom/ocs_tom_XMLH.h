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
 * This code was generated from file source/libs/sgeobj/json/XMLH.json
 * DO NOT CHANGE
 */

/** @file
 * @brief XML Document Header: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of XMLH, XML Document Header
 *
 * The enumeration is the type of every field number of XMLH, so a parameter
 * of this type takes the field numbers of XMLH only.
 */
enum XMLH_Field {
   /**
    * @brief XML Version
    *
    * The version string written into the XML declaration.
    */
   XMLH_Version = XMLH_LOWERBOUND,
   /**
    * @brief Root Element Name
    *
    * Name of the document's root element.
    */
   XMLH_Name,
   /**
    * @brief Stylesheets
    *
    * Stylesheet processing instructions (`XMLS`) emitted before the root element.
    */
   XMLH_Stylesheet,
   /**
    * @brief Root Attributes
    *
    * Attributes (`XMLA`) placed on the root element, such as the schema location.
    */
   XMLH_Attribute,
   /**
    * @brief Content
    *
    * The document body (`XMLE`).
    */
   XMLH_Element,
};

namespace ocs::tom {

   struct XMLA;
   struct XMLS;

   /**
    * @brief XML Document Header
    *
    * The head of a generated XML document: its version, root element name, stylesheets and attributes.
    * Used by the clients' `-xml` output. See `sge_cull_xml.cc`.
    */
   struct XMLH {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {XMLH_Version, "XMLH_Version", FieldType::String, Flag::None},
         {XMLH_Name, "XMLH_Name", FieldType::String, Flag::None},
         {XMLH_Stylesheet, "XMLH_Stylesheet", FieldType::List, Flag::None},
         {XMLH_Attribute, "XMLH_Attribute", FieldType::List, Flag::None},
         {XMLH_Element, "XMLH_Element", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of XMLH belongs to XMLH */
   template <>
   struct TypeOf<XMLH_Field> {
      using type = XMLH;   ///< the object type
   };

   /** @brief XMLH_Stylesheet holds elements of XMLS */
   template <>
   struct SubType<XMLH_Stylesheet> {
      using type = XMLS;   ///< the sub-type
   };

   /** @brief XMLH_Attribute holds elements of XMLA */
   template <>
   struct SubType<XMLH_Attribute> {
      using type = XMLA;   ///< the sub-type
   };

} // namespace ocs::tom
