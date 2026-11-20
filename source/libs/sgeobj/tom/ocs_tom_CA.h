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
 * This code was generated from file source/libs/sgeobj/json/CA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Parsed Calendar: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of CA, Parsed Calendar
 *
 * The enumeration is the type of every field number of CA, so a parameter
 * of this type takes the field numbers of CA only.
 */
enum CA_Field {
   /**
    * @brief YearDay Range List
    *
    * TMR with begin/end using TM_mday/TM_mon/TM_year.
    */
   CA_yday_range_list = CA_LOWERBOUND,
   /**
    * @brief WeekDay Range List
    *
    * TMR with begin/end using TM_wday.
    */
   CA_wday_range_list,
   /**
    * @brief DayTime Range List
    *
    * TMR with begin/end using TM_sec/TM_min/TM_hour.
    */
   CA_daytime_range_list,
   /**
    * @brief State
    *
    * Queue state in the given time range, e.g. QI_DO_NOTHING, QI_DO_DISABLE, QI_DO_SUSPEND, ...
    */
   CA_state,
};

namespace ocs::tom {

   struct TMR;

   /**
    * @brief Parsed Calendar
    *
    * This data structure is used for storing a parsed calendar.
    */
   struct CA {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CA_yday_range_list, "CA_yday_range_list", FieldType::List, Flag::None},
         {CA_wday_range_list, "CA_wday_range_list", FieldType::List, Flag::None},
         {CA_daytime_range_list, "CA_daytime_range_list", FieldType::List, Flag::None},
         {CA_state, "CA_state", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CA belongs to CA */
   template <>
   struct TypeOf<CA_Field> {
      using type = CA;   ///< the object type
   };

   /** @brief CA_yday_range_list holds elements of TMR */
   template <>
   struct SubType<CA_yday_range_list> {
      using type = TMR;   ///< the sub-type
   };

   /** @brief CA_wday_range_list holds elements of TMR */
   template <>
   struct SubType<CA_wday_range_list> {
      using type = TMR;   ///< the sub-type
   };

   /** @brief CA_daytime_range_list holds elements of TMR */
   template <>
   struct SubType<CA_daytime_range_list> {
      using type = TMR;   ///< the sub-type
   };

} // namespace ocs::tom
