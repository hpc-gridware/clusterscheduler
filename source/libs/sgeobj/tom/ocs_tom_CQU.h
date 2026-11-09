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
 * This code was generated from file source/libs/sgeobj/json/CQU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Calendar Queue State Change: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of CQU, Calendar Queue State Change
 *
 * The enumeration is the type of every field number of CQU, so a parameter
 * of this type takes the field numbers of CQU only.
 */
enum CQU_Field {
   /**
    * @brief State
    *
    * The `QI_*` state to put the queue into - enabled, disabled or suspended.
    */
   CQU_state = CQU_LOWERBOUND,
   /**
    * @brief Until
    *
    * When this state ends and the next entry takes over.
    */
   CQU_till,
};

namespace ocs::tom {

   /**
    * @brief Calendar Queue State Change
    *
    * One state change a calendar schedules: the state to take, and until when.
    */
   struct CQU {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CQU_state, "CQU_state", FieldType::Uint32, Flag::None},
         {CQU_till, "CQU_till", FieldType::Uint64, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CQU belongs to CQU */
   template <>
   struct TypeOf<CQU_Field> {
      using type = CQU;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of CQU for the list library: a synonym for ocs::tom::CQU::descriptor */
inline constexpr const lDescr *CQU_Type = ocs::tom::CQU::descriptor.e;
