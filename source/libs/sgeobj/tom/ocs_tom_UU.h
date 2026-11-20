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
 * This code was generated from file source/libs/sgeobj/json/UU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief User: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of UU, User
 *
 * The enumeration is the type of every field number of UU, so a parameter
 * of this type takes the field numbers of UU only.
 */
enum UU_Field {
   /**
    * @brief User Name
    *
    * A unique name of the user.
    */
   UU_name = UU_LOWERBOUND,
   /**
    * @brief Override Tickets
    *
    * Configured override tickets.
    */
   UU_oticket,
   /**
    * @brief Functional Shares
    *
    * Configured functional shares.
    */
   UU_fshare,
   /**
    * @brief Delete Time
    *
    * If an user submits a job for whom no user object exists, a user object is created.
    * This user object will automatically be deleted at the delete time.
    * Whenever a user submits a job the delete time is updated.
    * When set to 0 (via qconf) the user object is kept forever.
    */
   UU_delete_time,
   /**
    * @brief Job Count
    *
    * Job counter, used in scheduler thread only.
    */
   UU_job_cnt,
   /**
    * @brief Pending Job Count
    *
    * Pending job counter, used in scheduler thread only.
    */
   UU_pending_job_cnt,
   /**
    * @brief Usage
    *
    * List of UA objects storing accumulated and decayed usage of jobs belonging to this user.
    */
   UU_usage,
   /**
    * @brief Usage Time Stamp
    *
    * Time stamp of last decay. Set when usage changes.
    */
   UU_usage_time_stamp,
   /**
    * @brief Usage Sequence Number
    *
    * Usage sequence number used in scheduler thread only.
    */
   UU_usage_seqno,
   /**
    * @brief Long Term Usage
    *
    * List of UA objects holding long term accumulated non-decayed usage.
    */
   UU_long_term_usage,
   /**
    * @brief Project
    *
    * UPP list @todo add description.
    */
   UU_project,
   /**
    * @brief Debited Job Usage
    *
    * List of UPU, still debited usage per job.
    */
   UU_debited_job_usage,
   /**
    * @brief Default Project
    *
    * The default project is the project to which jobs of the user are assigned
    * if no project is specified at job submission time.
    */
   UU_default_project,
   /**
    * @brief User Version
    *
    * Version of the user object, increments when usage is updated
    */
   UU_version,
   /**
    * @brief Consider With Categories
    *
    * True, if user plays role with categories.
    */
   UU_consider_with_categories,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   UU_joker,
};

namespace ocs::tom {

   struct UA;
   struct UPP;
   struct UPU;
   struct VA;

   /**
    * @brief User
    *
    * An object of this type holds user configuration data as well as usage accumulators.
    */
   struct UU {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {UU_name, "UU_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool | Flag::SpoolSublist},
         {UU_oticket, "UU_oticket", FieldType::Uint32, Flag::Spool},
         {UU_fshare, "UU_fshare", FieldType::Uint32, Flag::Spool},
         {UU_delete_time, "UU_delete_time", FieldType::Uint64, Flag::Spool},
         {UU_job_cnt, "UU_job_cnt", FieldType::Uint32, Flag::None},
         {UU_pending_job_cnt, "UU_pending_job_cnt", FieldType::Uint32, Flag::None},
         {UU_usage, "UU_usage", FieldType::List, Flag::Spool},
         {UU_usage_time_stamp, "UU_usage_time_stamp", FieldType::Uint64, Flag::Spool},
         {UU_usage_seqno, "UU_usage_seqno", FieldType::Uint32, Flag::None},
         {UU_long_term_usage, "UU_long_term_usage", FieldType::List, Flag::Spool},
         {UU_project, "UU_project", FieldType::List, Flag::Spool},
         {UU_debited_job_usage, "UU_debited_job_usage", FieldType::List, Flag::Spool},
         {UU_default_project, "UU_default_project", FieldType::String, Flag::Spool},
         {UU_version, "UU_version", FieldType::Uint32, Flag::None},
         {UU_consider_with_categories, "UU_consider_with_categories", FieldType::Bool, Flag::None},
         {UU_joker, "UU_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of UU belongs to UU */
   template <>
   struct TypeOf<UU_Field> {
      using type = UU;   ///< the object type
   };

   /** @brief UU_usage holds elements of UA */
   template <>
   struct SubType<UU_usage> {
      using type = UA;   ///< the sub-type
   };

   /** @brief UU_long_term_usage holds elements of UA */
   template <>
   struct SubType<UU_long_term_usage> {
      using type = UA;   ///< the sub-type
   };

   /** @brief UU_project holds elements of UPP */
   template <>
   struct SubType<UU_project> {
      using type = UPP;   ///< the sub-type
   };

   /** @brief UU_debited_job_usage holds elements of UPU */
   template <>
   struct SubType<UU_debited_job_usage> {
      using type = UPU;   ///< the sub-type
   };

   /** @brief UU_joker holds elements of VA */
   template <>
   struct SubType<UU_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
