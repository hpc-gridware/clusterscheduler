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
 * This code was generated from file source/libs/sgeobj/json/TMR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Time Range: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of TMR, Time Range
 *
 * The enumeration is the type of every field number of TMR, so a parameter
 * of this type takes the field numbers of TMR only.
 */
enum TMR_Field {
   /**
    * @brief Begin
    *
    * Begin time stamp.
    */
   TMR_begin = TMR_LOWERBOUND,
   /**
    * @brief End
    *
    * End time stamp.
    */
   TMR_end,
};

namespace ocs::tom {

   struct TM;

   /**
    * @brief Time Range
    *
    * Used for time ranges represented by two (begin and end) timestamps (TM).
    */
   struct TMR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {TMR_begin, "TMR_begin", FieldType::List, Flag::None},
         {TMR_end, "TMR_end", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of TMR belongs to TMR */
   template <>
   struct TypeOf<TMR_Field> {
      using type = TMR;   ///< the object type
   };

   /** @brief TMR_begin holds elements of TM */
   template <>
   struct SubType<TMR_begin> {
      using type = TM;   ///< the sub-type
   };

   /** @brief TMR_end holds elements of TM */
   template <>
   struct SubType<TMR_end> {
      using type = TM;   ///< the sub-type
   };

} // namespace ocs::tom
