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
 * This code was generated from file source/libs/sgeobj/json/CT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Category: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of CT, Job Category
 *
 * The enumeration is the type of every field number of CT, so a parameter
 * of this type takes the field numbers of CT only.
 */
enum CT_Field {
   /**
    * @brief Category ID
    *
    * Unique ID of a category
    */
   CT_id = CT_LOWERBOUND,
   /**
    * @brief Category String
    *
    * String holding all elements of a category (requests, user, project, ...).
    */
   CT_str,
   /**
    * @brief Reference Count
    *
    * Number of jobs referencing this category.
    */
   CT_refcount,
   /**
    * @brief Rejected
    *
    * Has this category been rejected as it can not be dispatched now?
    */
   CT_rejected,
   /**
    * @brief Cache
    *
    * Stores all info, which cannot run this job category.
    */
   CT_cache,
   /**
    * @brief Messages Added
    *
    * If true, the scheduler info messages have been added for this category.
    */
   CT_messages_added,
   /**
    * @brief Resource Contribution
    *
    * Resource request dependent contribution on urgency.
    * This value is common for all jobs of a category.
    */
   CT_resource_contribution,
   /**
    * @brief Resource Contribution valid
    *
    * Indicates whether the cached CT_resource_contribution is valid.
    */
   CT_rc_valid,
   /**
    * @brief Reservation Rejected
    *
    * Has this category been rejected as it can not be reserved?
    */
   CT_reservation_rejected,
};

namespace ocs::tom {

   struct CCT;

   /**
    * @brief Job Category
    *
    * An object of this type describes a category of jobs.
    */
   struct CT {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CT_id, "CT_id", FieldType::Uint32, Flag::Unique | Flag::Hash},
         {CT_str, "CT_str", FieldType::String, Flag::Unique | Flag::Hash},
         {CT_refcount, "CT_refcount", FieldType::Uint32, Flag::None},
         {CT_rejected, "CT_rejected", FieldType::Bool, Flag::None},
         {CT_cache, "CT_cache", FieldType::List, Flag::NoTransfer},
         {CT_messages_added, "CT_messages_added", FieldType::Bool, Flag::None},
         {CT_resource_contribution, "CT_resource_contribution", FieldType::Double, Flag::None},
         {CT_rc_valid, "CT_rc_valid", FieldType::Bool, Flag::None},
         {CT_reservation_rejected, "CT_reservation_rejected", FieldType::Bool, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CT belongs to CT */
   template <>
   struct TypeOf<CT_Field> {
      using type = CT;   ///< the object type
   };

   /** @brief CT_cache holds elements of CCT */
   template <>
   struct SubType<CT_cache> {
      using type = CCT;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of CT for the list library: a synonym for ocs::tom::CT::descriptor */
inline constexpr const lDescr *CT_Type = ocs::tom::CT::descriptor.e;
