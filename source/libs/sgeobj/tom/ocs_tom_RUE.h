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
 * This code was generated from file source/libs/sgeobj/json/RUE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Utilization: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of RUE, Resource Utilization
 *
 * The enumeration is the type of every field number of RUE, so a parameter
 * of this type takes the field numbers of RUE only.
 */
enum RUE_Field {
   /**
    * @brief Resource Name
    *
    * The name of the resource (= the name of the complex variable, e.g. slots)
    */
   RUE_name = RUE_LOWERBOUND,
   /**
    * @brief Utilized Now
    *
    * Currently used amount.
    */
   RUE_utilized_now,
   /**
    * @brief Utilized Now Resource Map List
    *
    * Currently used amount of Resource Maps
    */
   RUE_utilized_now_resource_map_list,
   /**
    * @brief Utilized Now Binding In Use
    *
    * Topology string with lowercase core/threads that are currently in use.
    */
   RUE_utilized_now_binding_inuse,
   /**
    * @brief Utilized
    *
    * A resource diagram indicating future utilization.
    */
   RUE_utilized,
   /**
    * @brief Utilized Now Non-Exclusive
    *
    * Currently used amount of implicitly used exclusive resources.
    */
   RUE_utilized_now_nonexclusive,
   /**
    * @brief Utilized Non-Exclusive
    *
    * A resource diagram indicating future utilization of implicitly used exclusive resources.
    */
   RUE_utilized_nonexclusive,
};

namespace ocs::tom {

   struct RDE;
   struct RESL;

   /**
    * @brief Resource Utilization
    *
    * Utilization of a certain resource.
    * Both the current utilization can be stored, as well as
    * future utilization (due to currently running jobs,
    * advance reservations and resource reservations).
    */
   struct RUE {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RUE_name, "RUE_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool | Flag::SpoolSublist},
         {RUE_utilized_now, "RUE_utilized_now", FieldType::Double, Flag::None},
         {RUE_utilized_now_resource_map_list, "RUE_utilized_now_resource_map_list", FieldType::List, Flag::None},
         {RUE_utilized_now_binding_inuse, "RUE_utilized_now_binding_inuse", FieldType::String, Flag::None},
         {RUE_utilized, "RUE_utilized", FieldType::List, Flag::None},
         {RUE_utilized_now_nonexclusive, "RUE_utilized_now_nonexclusive", FieldType::Double, Flag::None},
         {RUE_utilized_nonexclusive, "RUE_utilized_nonexclusive", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RUE belongs to RUE */
   template <>
   struct TypeOf<RUE_Field> {
      using type = RUE;   ///< the object type
   };

   /** @brief RUE_utilized_now_resource_map_list holds elements of RESL */
   template <>
   struct SubType<RUE_utilized_now_resource_map_list> {
      using type = RESL;   ///< the sub-type
   };

   /** @brief RUE_utilized holds elements of RDE */
   template <>
   struct SubType<RUE_utilized> {
      using type = RDE;   ///< the sub-type
   };

   /** @brief RUE_utilized_nonexclusive holds elements of RDE */
   template <>
   struct SubType<RUE_utilized_nonexclusive> {
      using type = RDE;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of RUE for the list library: a synonym for ocs::tom::RUE::descriptor */
inline constexpr const lDescr *RUE_Type = ocs::tom::RUE::descriptor.e;
