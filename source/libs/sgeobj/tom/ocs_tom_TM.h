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
 * This code was generated from file source/libs/sgeobj/json/TM.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Date Time: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of TM, Date Time
 *
 * The enumeration is the type of every field number of TM, so a parameter
 * of this type takes the field numbers of TM only.
 */
enum TM_Field {
   /**
    * @brief Month Day
    *
    * Day of the month, 1-31 (@todo 32 in the old documentation, does it have a special meaning?).
    */
   TM_mday = TM_LOWERBOUND,
   /**
    * @brief Month
    *
    * Month, 0-11.
    */
   TM_mon,
   /**
    * @brief Year
    *
    * The number of years since 1900.
    */
   TM_year,
   /**
    * @brief Seconds
    *
    * Seconds, 0-59
    */
   TM_sec,
   /**
    * @brief Minute
    *
    * Minutes, 0-59
    */
   TM_min,
   /**
    * @brief Hor
    *
    * Hour, 0-23
    */
   TM_hour,
   /**
    * @brief Week Day
    *
    * Week day, 0-6.
    */
   TM_wday,
   /**
    * @brief Year Day
    *
    * Day in the year, 0-365.
    */
   TM_yday,
   /**
    * @brief Is Daylight Saving Time
    *
    * Whether daylight saving time is in effect (0 = false, 1 = true).
    */
   TM_isdst,
};

namespace ocs::tom {

   /**
    * @brief Date Time
    *
    * An object of this type represents a timestamp (date and time). It mostly corresponds to the struct tm, see man.
    */
   struct TM {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {TM_mday, "TM_mday", FieldType::Uint32, Flag::None},
         {TM_mon, "TM_mon", FieldType::Uint32, Flag::None},
         {TM_year, "TM_year", FieldType::Uint32, Flag::None},
         {TM_sec, "TM_sec", FieldType::Uint32, Flag::None},
         {TM_min, "TM_min", FieldType::Uint32, Flag::None},
         {TM_hour, "TM_hour", FieldType::Uint32, Flag::None},
         {TM_wday, "TM_wday", FieldType::Uint32, Flag::None},
         {TM_yday, "TM_yday", FieldType::Uint32, Flag::None},
         {TM_isdst, "TM_isdst", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of TM belongs to TM */
   template <>
   struct TypeOf<TM_Field> {
      using type = TM;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of TM for the list library: a synonym for ocs::tom::TM::descriptor */
inline constexpr const lDescr *TM_Type = ocs::tom::TM::descriptor.e;
