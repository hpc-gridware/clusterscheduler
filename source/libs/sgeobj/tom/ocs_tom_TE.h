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
 * This code was generated from file source/libs/sgeobj/json/TE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Time Event: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of TE, Time Event
 *
 * The enumeration is the type of every field number of TE, so a parameter
 * of this type takes the field numbers of TE only.
 */
enum TE_Field {
   /**
    * @brief When
    *
    * Time in µs since epoch when this event must be delivered.
    */
   TE_when = TE_LOWERBOUND,
   /**
    * @brief Type
    *
    * Used to differ between different event categories from enum te_type_t, e.g.
    * TYPE_CALENDAR_EVENT, TYPE_SIGNAL_RESEND_EVENT, ...
    */
   TE_type,
   /**
    * @brief Mode
    *
    * one-time or recurring event (ONE_TIME_EVENT or RECURRING_EVENT).
    */
   TE_mode,
   /**
    * @brief Interval
    *
    * The event interval in µs, in case of recurring events.
    */
   TE_interval,
   /**
    * @brief UVal0
    *
    * 1st ulong key, e.g. the job number for a job specific event like a TYPE_JOB_RESEND_EVENT.
    */
   TE_uval0,
   /**
    * @brief UVal1
    *
    * 2nd ulong key, e.g. the array task number for a job/task specific event like a TYPE_JOB_RESEND_EVENT.
    */
   TE_uval1,
   /**
    * @brief String Key
    *
    * String key, e.g. for a queue instance specific event.
    */
   TE_sval,
   /**
    * @brief Sequence Number
    *
    * Every event is assigned an unique sequence number.
    */
   TE_seqno,
};

namespace ocs::tom {

   /**
    * @brief Time Event
    *
    * Time Event objects are used by the timed event thread in sge_qmaster.
    * Time Event Objects store one time or recurring events which trigger predefined actions.
    */
   struct TE {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {TE_when, "TE_when", FieldType::Uint64, Flag::None},
         {TE_type, "TE_type", FieldType::Uint32, Flag::None},
         {TE_mode, "TE_mode", FieldType::Uint32, Flag::None},
         {TE_interval, "TE_interval", FieldType::Uint64, Flag::None},
         {TE_uval0, "TE_uval0", FieldType::Uint32, Flag::None},
         {TE_uval1, "TE_uval1", FieldType::Uint32, Flag::None},
         {TE_sval, "TE_sval", FieldType::String, Flag::None},
         {TE_seqno, "TE_seqno", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of TE belongs to TE */
   template <>
   struct TypeOf<TE_Field> {
      using type = TE;   ///< the object type
   };

} // namespace ocs::tom
