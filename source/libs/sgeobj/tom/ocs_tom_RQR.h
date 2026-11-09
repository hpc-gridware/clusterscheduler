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
 * This code was generated from file source/libs/sgeobj/json/RQR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Rule: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of RQR, Resource Quota Rule
 *
 * The enumeration is the type of every field number of RQR, so a parameter
 * of this type takes the field numbers of RQR only.
 */
enum RQR_Field {
   /**
    * @brief Name
    *
    * Optional rule name, so `qquota` and the error messages can say which rule bit.
    */
   RQR_name = RQR_LOWERBOUND,
   /**
    * @brief User Filter
    *
    * Which submitting users the rule applies to (`RQRF_Type`).
    */
   RQR_filter_users,
   /**
    * @brief Project Filter
    *
    * Which projects the rule applies to.
    */
   RQR_filter_projects,
   /**
    * @brief Parallel Environment Filter
    *
    * Which parallel environments the rule applies to.
    */
   RQR_filter_pes,
   /**
    * @brief Queue Filter
    *
    * Which cluster queues the rule applies to.
    */
   RQR_filter_queues,
   /**
    * @brief Host Filter
    *
    * Which execution hosts the rule applies to.
    */
   RQR_filter_hosts,
   /**
    * @brief Limits
    *
    * What the rule limits and by how much (`RQRL_Type`), one entry per resource.
    */
   RQR_limit,
   /**
    * @brief Limit Level
    *
    * How far the limit is spread: shared by everything matched, or one limit each per host, cluster queue or queue instance. See the `RQR_*` values in `sge_resource_quota.h`.
    */
   RQR_level,
};

namespace ocs::tom {

   struct RQRL;

   /**
    * @brief Resource Quota Rule
    *
    * One rule of a resource quota set: what it matches, and what it then limits.
    * The five filters are ANDed. An empty filter matches everything, so a rule with no filters is the catch-all.
    */
   struct RQR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RQR_name, "RQR_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {RQR_filter_users, "RQR_filter_users", FieldType::Object, Flag::Spool},
         {RQR_filter_projects, "RQR_filter_projects", FieldType::Object, Flag::Spool},
         {RQR_filter_pes, "RQR_filter_pes", FieldType::Object, Flag::Spool},
         {RQR_filter_queues, "RQR_filter_queues", FieldType::Object, Flag::Spool},
         {RQR_filter_hosts, "RQR_filter_hosts", FieldType::Object, Flag::Spool},
         {RQR_limit, "RQR_limit", FieldType::List, Flag::Spool},
         {RQR_level, "RQR_level", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RQR belongs to RQR */
   template <>
   struct TypeOf<RQR_Field> {
      using type = RQR;   ///< the object type
   };

   /** @brief RQR_limit holds elements of RQRL */
   template <>
   struct SubType<RQR_limit> {
      using type = RQRL;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of RQR for the list library: a synonym for ocs::tom::RQR::descriptor */
inline constexpr const lDescr *RQR_Type = ocs::tom::RQR::descriptor.e;
