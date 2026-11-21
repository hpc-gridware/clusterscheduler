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
 * This code was generated from file source/libs/sgeobj/json/AN.json
 * DO NOT CHANGE
 */

/** @file
 * @brief answer element: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of AN, answer element
 *
 * The enumeration is the type of every field number of AN, so a parameter
 * of this type takes the field numbers of AN only.
 */
enum AN_Field {
   /**
    * @brief answer status
    *
    * status of an answer: a high level error code, e.g.
    *  - STATUS_OK
    *  - STATUS_ESEMANTIC
    *  - STATUS_EEXIST
    *  - STATUS_EUNKNOWN
    *  - ...
    */
   AN_status = AN_LOWERBOUND,
   /**
    * @brief answer text
    *
    * printable answer text, e.g. an error message
    */
   AN_text,
   /**
    * @brief answer quality
    *
    * answer quality (level, severity):
    *  - ANSWER_QUALITY_CRITICAL
    *  - ANSWER_QUALITY_ERROR
    *  - ANSWER_QUALITY_WARNING
    *  - ANSWER_QUALITY_INFO
    */
   AN_quality,
};

namespace ocs::tom {

   /**
    * @brief answer element
    *
    * element holding information for an answer to a request
    */
   struct AN {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {AN_status, "AN_status", FieldType::Uint32, Flag::None},
         {AN_text, "AN_text", FieldType::String, Flag::None},
         {AN_quality, "AN_quality", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of AN belongs to AN */
   template <>
   struct TypeOf<AN_Field> {
      using type = AN;   ///< the object type
   };

} // namespace ocs::tom
