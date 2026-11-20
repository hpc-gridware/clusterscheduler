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
 * This code was generated from file source/libs/sgeobj/json/US.json
 * DO NOT CHANGE
 */

/** @file
 * @brief User Set: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of US, User Set
 *
 * The enumeration is the type of every field number of US, so a parameter
 * of this type takes the field numbers of US only.
 */
enum US_Field {
   /**
    * @brief Name
    *
    * The set's name. `defaultdepartment` is the fallback department.
    */
   US_name = US_LOWERBOUND,
   /**
    * @brief Type
    *
    * `US_ACL`, `US_DEPT`, or both. See `sge_userset.h`.
    */
   US_type,
   /**
    * @brief Functional Share
    *
    * The department's share of functional tickets.
    */
   US_fshare,
   /**
    * @brief Override Tickets
    *
    * Override tickets granted to the department's jobs.
    */
   US_oticket,
   /**
    * @brief Running Job Count
    *
    * Jobs of this userset currently running, maintained by the scheduler for ticket calculation.
    */
   US_job_cnt,
   /**
    * @brief Pending Job Count
    *
    * Jobs of this userset waiting to run.
    */
   US_pending_job_cnt,
   /**
    * @brief Members
    *
    * The users and UNIX groups in the set (`UE`). A name starting with `@` is a group.
    */
   US_entries,
   /**
    * @brief Category Relevant
    *
    * The set affects which jobs can run where, so two jobs differing only in it must not share a scheduling category.
    */
   US_consider_with_categories,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   US_joker,
};

namespace ocs::tom {

   struct UE;
   struct VA;

   /**
    * @brief User Set
    *
    * A named set of users and groups, used either as an access list or as a department.
    * One type serves both: `US_type` says which. As an access list it decides who may use a queue, project or parallel environment; as a department it carries the share-tree and override tickets its members get. A userset may be both at once.
    */
   struct US {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {US_name, "US_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool | Flag::SpoolSublist},
         {US_type, "US_type", FieldType::Uint32, Flag::Spool},
         {US_fshare, "US_fshare", FieldType::Uint32, Flag::Spool},
         {US_oticket, "US_oticket", FieldType::Uint32, Flag::Spool},
         {US_job_cnt, "US_job_cnt", FieldType::Uint32, Flag::None},
         {US_pending_job_cnt, "US_pending_job_cnt", FieldType::Uint32, Flag::None},
         {US_entries, "US_entries", FieldType::List, Flag::Spool},
         {US_consider_with_categories, "US_consider_with_categories", FieldType::Bool, Flag::None},
         {US_joker, "US_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of US belongs to US */
   template <>
   struct TypeOf<US_Field> {
      using type = US;   ///< the object type
   };

   /** @brief US_entries holds elements of UE */
   template <>
   struct SubType<US_entries> {
      using type = UE;   ///< the sub-type
   };

   /** @brief US_joker holds elements of VA */
   template <>
   struct SubType<US_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
