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
 * This code was generated from file source/libs/sgeobj/json/RTIC.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Ticket Host Entry: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of RTIC, Resource Ticket Host Entry
 *
 * The enumeration is the type of every field number of RTIC, so a parameter
 * of this type takes the field numbers of RTIC only.
 */
enum RTIC_Field {
   /**
    * @brief Host
    *
    * The execution host the tickets belong to.
    */
   RTIC_host = RTIC_LOWERBOUND,
   /**
    * @brief Tickets
    *
    * The per-job ticket values for that host.
    */
   RTIC_tickets,
};

namespace ocs::tom {

   struct OR;

   /**
    * @brief Resource Ticket Host Entry
    *
    * The tickets granted on one host, as the scheduler reports them back with its orders.
    */
   struct RTIC {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RTIC_host, "RTIC_host", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash},
         {RTIC_tickets, "RTIC_tickets", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RTIC belongs to RTIC */
   template <>
   struct TypeOf<RTIC_Field> {
      using type = RTIC;   ///< the object type
   };

   /** @brief RTIC_tickets holds elements of OR */
   template <>
   struct SubType<RTIC_tickets> {
      using type = OR;   ///< the sub-type
   };

} // namespace ocs::tom
