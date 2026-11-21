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
 * This code was generated from file source/libs/sgeobj/json/MA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Multi Gdi Id: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of MA, Multi Gdi Id
 *
 * The enumeration is the type of every field number of MA, so a parameter
 * of this type takes the field numbers of MA only.
 */
enum MA_Field {
   /**
    * @brief Id
    *
    * Id of the individual request to associate request with result.
    */
   MA_id = MA_LOWERBOUND,
   /**
    * @brief Objects
    *
    * A list of the requested type containing the result of the request.
    */
   MA_objects,
   /**
    * @brief Answers
    *
    * A list of answers. This can be error, warning or info messages from processing the request.
    */
   MA_answers,
};

namespace ocs::tom {

   struct AN;

   /**
    * @brief Multi Gdi Id
    *
    * Multiple requests can be sent by a client to sge_qmaster in a single message.
    * Every such request gets a unique number.
    * A list of MA is used to return the answers to a multi GDI request.
    */
   struct MA {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {MA_id, "MA_id", FieldType::Uint32, Flag::None},
         {MA_objects, "MA_objects", FieldType::List, Flag::None},
         {MA_answers, "MA_answers", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of MA belongs to MA */
   template <>
   struct TypeOf<MA_Field> {
      using type = MA;   ///< the object type
   };

   /** @brief MA_answers holds elements of AN */
   template <>
   struct SubType<MA_answers> {
      using type = AN;   ///< the sub-type
   };

} // namespace ocs::tom
