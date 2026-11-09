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
 * This code was generated from file source/libs/sgeobj/json/PR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Project: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of PR, Project
 *
 * The enumeration is the type of every field number of PR, so a parameter
 * of this type takes the field numbers of PR only.
 */
enum PR_Field {
   /**
    * @brief Project Name
    *
    * A unique name of the project.
    */
   PR_name = PR_LOWERBOUND,
   /**
    * @brief Override Tickets
    *
    * Configured override tickets.
    */
   PR_oticket,
   /**
    * @brief Functional Shares
    *
    * Configured functional shares.
    */
   PR_fshare,
   /**
    * @brief Job Count
    *
    * Job counter, used in scheduler thread only.
    */
   PR_job_cnt,
   /**
    * @brief Pending Job Count
    *
    * Pending job counter, used in scheduler thread only.
    */
   PR_pending_job_cnt,
   /**
    * @brief Usage
    *
    * List of UA_Type objects storing accumulated and decayed usage of jobs belonging to this project.
    */
   PR_usage,
   /**
    * @brief Usage Time Stamp
    *
    * Time stamp of last decay. Set when usage changes.
    */
   PR_usage_time_stamp,
   /**
    * @brief Usage Sequence Number
    *
    * Usage sequence number used in scheduler thread only.
    */
   PR_usage_seqno,
   /**
    * @brief Long Term Usage
    *
    * List of UA_Type objects holding long term accumulated non-decayed usage.
    */
   PR_long_term_usage,
   /**
    * @brief Project Usage
    *
    * Usage on a project basis (?).
    */
   PR_project,
   /**
    * @brief Access List
    *
    * US_Type but only names are filled configured excluded user access list used.
    */
   PR_acl,
   /**
    * @brief No Access List
    *
    * US_Type but only names are filled configured excluded user access list used.
    */
   PR_xacl,
   /**
    * @brief Debited Job Usage
    *
    * List of UPU_Type, still debited usage per job.
    */
   PR_debited_job_usage,
   /**
    * @brief Project Version
    *
    * project version, increments when usage is updated
    */
   PR_version,
   /**
    * @brief Consider With Categories
    *
    * True, if project plays role with categories.
    */
   PR_consider_with_categories,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   PR_joker,
};

namespace ocs::tom {

   struct UA;
   struct UPP;
   struct UPU;
   struct US;
   struct VA;

   /**
    * @brief Project
    *
    * An object of this type holds project configuration data as well as usage accumulators.
    */
   struct PR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PR_name, "PR_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool | Flag::SpoolSublist},
         {PR_oticket, "PR_oticket", FieldType::Uint32, Flag::Spool},
         {PR_fshare, "PR_fshare", FieldType::Uint32, Flag::Spool},
         {PR_job_cnt, "PR_job_cnt", FieldType::Uint32, Flag::None},
         {PR_pending_job_cnt, "PR_pending_job_cnt", FieldType::Uint32, Flag::None},
         {PR_usage, "PR_usage", FieldType::List, Flag::Spool},
         {PR_usage_time_stamp, "PR_usage_time_stamp", FieldType::Uint64, Flag::Spool},
         {PR_usage_seqno, "PR_usage_seqno", FieldType::Uint32, Flag::None},
         {PR_long_term_usage, "PR_long_term_usage", FieldType::List, Flag::Spool},
         {PR_project, "PR_project", FieldType::List, Flag::Spool},
         {PR_acl, "PR_acl", FieldType::List, Flag::SpoolProject},
         {PR_xacl, "PR_xacl", FieldType::List, Flag::SpoolProject},
         {PR_debited_job_usage, "PR_debited_job_usage", FieldType::List, Flag::Spool},
         {PR_version, "PR_version", FieldType::Uint32, Flag::None},
         {PR_consider_with_categories, "PR_consider_with_categories", FieldType::Bool, Flag::None},
         {PR_joker, "PR_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PR belongs to PR */
   template <>
   struct TypeOf<PR_Field> {
      using type = PR;   ///< the object type
   };

   /** @brief PR_usage holds elements of UA */
   template <>
   struct SubType<PR_usage> {
      using type = UA;   ///< the sub-type
   };

   /** @brief PR_long_term_usage holds elements of UA */
   template <>
   struct SubType<PR_long_term_usage> {
      using type = UA;   ///< the sub-type
   };

   /** @brief PR_project holds elements of UPP */
   template <>
   struct SubType<PR_project> {
      using type = UPP;   ///< the sub-type
   };

   /** @brief PR_acl holds elements of US */
   template <>
   struct SubType<PR_acl> {
      using type = US;   ///< the sub-type
   };

   /** @brief PR_xacl holds elements of US */
   template <>
   struct SubType<PR_xacl> {
      using type = US;   ///< the sub-type
   };

   /** @brief PR_debited_job_usage holds elements of UPU */
   template <>
   struct SubType<PR_debited_job_usage> {
      using type = UPU;   ///< the sub-type
   };

   /** @brief PR_joker holds elements of VA */
   template <>
   struct SubType<PR_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of PR for the list library: a synonym for ocs::tom::PR::descriptor */
inline constexpr const lDescr *PR_Type = ocs::tom::PR::descriptor.e;
