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
 * This code was generated from file source/libs/sgeobj/json/GRU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Granted Resource: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of GRU, Granted Resource
 *
 * The enumeration is the type of every field number of GRU, so a parameter
 * of this type takes the field numbers of GRU only.
 */
enum GRU_Field {
   /**
    * @brief Type
    *
    * Type of the resource:
    * GRU_HARD_REQUEST_TYPE
    * GRU_SOFT_REQUEST_TYPE
    * GRU_RESOURCE_MAP_TYPE
    */
   GRU_type = GRU_LOWERBOUND,
   /**
    * @brief Name
    *
    * Name of the resource (complex variable name).
    */
   GRU_name,
   /**
    * @brief Granted Amount
    *
    * How much of the resource has been granted.
    */
   GRU_amount,
   /**
    * @brief Resource Map List
    *
    * In case of resource maps: Which Ids and how much per Id has been granted
    * For RSMAPs: Which ids have been granted.
    */
   GRU_resource_map_list,
   /**
    * @brief Grant Binding In Use
    *
    * Topology strings that show the granted core binding cores/threads.
    */
   GRU_binding_inuse,
   /**
    * @brief Host
    *
    * Host on which the resource has been granted (required in case of parallel jobs).
    */
   GRU_host,
};

namespace ocs::tom {

   struct RESL;
   struct ST;

   /**
    * @brief Granted Resource
    *
    * An object of this type represents a granted resource.
    * A granted resource defines the amount of a resource which was requested by and granted to a job.
    * Currently only used for RSMAPs.
    * @todo Use it for all granted resources (e.g. prerequisit for soft consumables)
    * @todo Don't we have to store it per petask?
    * @todo we don't have a primary key and cannot search by index. Primary key would be name + host.
    */
   struct GRU {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {GRU_type, "GRU_type", FieldType::Uint32, Flag::Spool},
         {GRU_name, "GRU_name", FieldType::String, Flag::Spool},
         {GRU_amount, "GRU_amount", FieldType::Double, Flag::Spool},
         {GRU_resource_map_list, "GRU_resource_map_list", FieldType::List, Flag::Spool},
         {GRU_binding_inuse, "GRU_binding_inuse", FieldType::List, Flag::Spool},
         {GRU_host, "GRU_host", FieldType::Host, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of GRU belongs to GRU */
   template <>
   struct TypeOf<GRU_Field> {
      using type = GRU;   ///< the object type
   };

   /** @brief GRU_resource_map_list holds elements of RESL */
   template <>
   struct SubType<GRU_resource_map_list> {
      using type = RESL;   ///< the sub-type
   };

   /** @brief GRU_binding_inuse holds elements of ST */
   template <>
   struct SubType<GRU_binding_inuse> {
      using type = ST;   ///< the sub-type
   };

} // namespace ocs::tom
