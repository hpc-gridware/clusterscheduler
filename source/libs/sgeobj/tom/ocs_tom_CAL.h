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
 * This code was generated from file source/libs/sgeobj/json/CAL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Calendar: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of CAL, Calendar
 *
 * The enumeration is the type of every field number of CAL, so a parameter
 * of this type takes the field numbers of CAL only.
 */
enum CAL_Field {
   /**
    * @brief Name
    *
    * The calendar name.
    * See man page sge_calendar_conf.5 for detailed information about the attributes.
    */
   CAL_name = CAL_LOWERBOUND,
   /**
    * @brief Year Calendar
    *
    * A year calendar as string, e.g. 12.03.2004=12-11=off.
    */
   CAL_year_calendar,
   /**
    * @brief Week Calendar
    *
    * A week calendar as string, e.g. mon-fri=6-20=suspended.
    */
   CAL_week_calendar,
   /**
    * @brief Parsed Year Calendar
    *
    * The year calendar parsed to internal data structures.
    */
   CAL_parsed_year_calendar,
   /**
    * @brief Parsed Week Calendar
    *
    * The week calendar parsed to internal data structures.
    */
   CAL_parsed_week_calendar,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   CAL_joker,
};

namespace ocs::tom {

   struct CA;
   struct VA;

   /**
    * @brief Calendar
    *
    * This data structure represents the SGE calendar object.
    */
   struct CAL {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CAL_name, "CAL_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {CAL_year_calendar, "CAL_year_calendar", FieldType::String, Flag::Spool},
         {CAL_week_calendar, "CAL_week_calendar", FieldType::String, Flag::Spool},
         {CAL_parsed_year_calendar, "CAL_parsed_year_calendar", FieldType::List, Flag::None},
         {CAL_parsed_week_calendar, "CAL_parsed_week_calendar", FieldType::List, Flag::None},
         {CAL_joker, "CAL_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CAL belongs to CAL */
   template <>
   struct TypeOf<CAL_Field> {
      using type = CAL;   ///< the object type
   };

   /** @brief CAL_parsed_year_calendar holds elements of CA */
   template <>
   struct SubType<CAL_parsed_year_calendar> {
      using type = CA;   ///< the sub-type
   };

   /** @brief CAL_parsed_week_calendar holds elements of CA */
   template <>
   struct SubType<CAL_parsed_week_calendar> {
      using type = CA;   ///< the sub-type
   };

   /** @brief CAL_joker holds elements of VA */
   template <>
   struct SubType<CAL_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of CAL for the list library: a synonym for ocs::tom::CAL::descriptor */
inline constexpr const lDescr *CAL_Type = ocs::tom::CAL::descriptor.e;
