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
 * This code was generated from file source/libs/sgeobj/json/JL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief PTF Job: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of JL, PTF Job
 *
 * The enumeration is the type of every field number of JL, so a parameter
 * of this type takes the field numbers of JL only.
 */
enum JL_Field {
   /**
    * @brief Job Id
    *
    * The Job Id.
    */
   JL_job_ID = JL_LOWERBOUND,
   /**
    * @brief OS Job List
    *
    * List of OS jobs (JO_Type). There should be one per running Array/PE task having one os job id each.
    */
   JL_OS_job_list,
   /**
    * @brief State
    *
    * Job State, one of JL_JOB_ACTIVE, JL_JOB_COMPLETE, JL_JOB_DELETED.
    */
   JL_state,
   /**
    * @brief Tickets
    *
    * Job Tickets. Used for reprioritization.
    */
   JL_tickets,
   /**
    * @brief Share
    *
    * ptf interval share
    */
   JL_share,
   /**
    * @brief Ticket Share
    *
    * ptf job ticket share
    */
   JL_ticket_share,
   /**
    * @brief Timeslice
    *
    * ptf calculated timeslice (SX)
    */
   JL_timeslice,
   /**
    * @brief Usage
    *
    * ptf interval combined usage
    */
   JL_usage,
   /**
    * @brief Old Usage
    *
    * ptf interval combined usage
    */
   JL_old_usage_value,
   /**
    * @brief Adjusted Usage
    *
    * ptf interval adjusted usage
    */
   JL_adjusted_usage,
   /**
    * @brief Last Usage
    *
    * ptf last interval combined usage
    */
   JL_last_usage,
   /**
    * @brief Old Usage
    *
    * prev interval combined usage
    */
   JL_old_usage,
   /**
    * @brief Proportion
    *
    * @todo add description
    */
   JL_proportion,
   /**
    * @brief Adjusted Proportion
    *
    * @todo add description
    */
   JL_adjusted_proportion,
   /**
    * @brief Adjusted Current Proportion
    *
    * @todo add description
    */
   JL_adjusted_current_proportion,
   /**
    * @brief Actual Proportion
    *
    * @todo add description
    */
   JL_actual_proportion,
   /**
    * @brief Diff Proportion
    *
    * @todo add description
    */
   JL_diff_proportion,
   /**
    * @brief Last Proportion
    *
    * @todo add description
    */
   JL_last_proportion,
   /**
    * @brief Current Priority
    *
    * @todo add description
    */
   JL_curr_pri,
   /**
    * @brief Priority
    *
    * @todo add description
    */
   JL_pri,
   /**
    * @brief Proc FD
    *
    * proc file descriptor
    */
   JL_procfd,
   /**
    * @brief Interactive
    *
    * interactive flag
    */
   JL_interactive,
};

namespace ocs::tom {

   struct JO;

   /**
    * @brief PTF Job
    *
    * This is the list type we use to hold the jobs which are running under the local execution daemon.
    * @todo This is a very old module and probably noone really knows its details any more. Add better documentation.
    */
   struct JL {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JL_job_ID, "JL_job_ID", FieldType::Uint32, Flag::PrimaryKey | Flag::Unique | Flag::Hash},
         {JL_OS_job_list, "JL_OS_job_list", FieldType::List, Flag::None},
         {JL_state, "JL_state", FieldType::Uint32, Flag::None},
         {JL_tickets, "JL_tickets", FieldType::Uint32, Flag::None},
         {JL_share, "JL_share", FieldType::Double, Flag::None},
         {JL_ticket_share, "JL_ticket_share", FieldType::Double, Flag::None},
         {JL_timeslice, "JL_timeslice", FieldType::Double, Flag::None},
         {JL_usage, "JL_usage", FieldType::Double, Flag::None},
         {JL_old_usage_value, "JL_old_usage_value", FieldType::Double, Flag::None},
         {JL_adjusted_usage, "JL_adjusted_usage", FieldType::Double, Flag::None},
         {JL_last_usage, "JL_last_usage", FieldType::Double, Flag::None},
         {JL_old_usage, "JL_old_usage", FieldType::Double, Flag::None},
         {JL_proportion, "JL_proportion", FieldType::Double, Flag::None},
         {JL_adjusted_proportion, "JL_adjusted_proportion", FieldType::Double, Flag::None},
         {JL_adjusted_current_proportion, "JL_adjusted_current_proportion", FieldType::Double, Flag::None},
         {JL_actual_proportion, "JL_actual_proportion", FieldType::Double, Flag::None},
         {JL_diff_proportion, "JL_diff_proportion", FieldType::Double, Flag::None},
         {JL_last_proportion, "JL_last_proportion", FieldType::Double, Flag::None},
         {JL_curr_pri, "JL_curr_pri", FieldType::Double, Flag::None},
         {JL_pri, "JL_pri", FieldType::Int64, Flag::None},
         {JL_procfd, "JL_procfd", FieldType::Uint32, Flag::None},
         {JL_interactive, "JL_interactive", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JL belongs to JL */
   template <>
   struct TypeOf<JL_Field> {
      using type = JL;   ///< the object type
   };

   /** @brief JL_OS_job_list holds elements of JO */
   template <>
   struct SubType<JL_OS_job_list> {
      using type = JO;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of JL for the list library: a synonym for ocs::tom::JL::descriptor */
inline constexpr const lDescr *JL_Type = ocs::tom::JL::descriptor.e;
