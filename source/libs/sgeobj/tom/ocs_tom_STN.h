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
 * This code was generated from file source/libs/sgeobj/json/STN.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Share Tree Node: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of STN, Share Tree Node
 *
 * The enumeration is the type of every field number of STN, so a parameter
 * of this type takes the field numbers of STN only.
 */
enum STN_Field {
   /**
    * @brief Node Name
    *
    * Node name (symbolic name or user or project name in the leafs)
    */
   STN_name = STN_LOWERBOUND,
   /**
    * @brief Node Type
    *
    * Kind of tree: STT_USER or STT_PROJECT.
    * All nodes in a tree have the same type.
    */
   STN_type,
   /**
    * @brief Node Id
    *
    * Only for editing and save/restore. May be different each time.
    */
   STN_id,
   /**
    * @brief Shares
    *
    * Configured shares for the node or subtree under a leaf node.
    */
   STN_shares,
   /**
    * @brief Children
    *
    * Configured childrens of this node. Same type, it is a recursive tree structure.
    */
   STN_children,
   /**
    * @brief Job Reference Count
    *
    * Number of running and pending jobs referencing this node (only used in scheduler thread).
    */
   STN_job_ref_count,
   /**
    * @brief Active Job Reference Count
    *
    * Number of running jobs referencing this node (only used in scheduler thread).
    */
   STN_active_job_ref_count,
   /**
    * @brief Is Project
    *
    * Set to 1 if this node is a project (only used in scheduler thread).
    */
   STN_project,
   /**
    * @brief Proportion
    *
    * Share proportion (only used in scheduler thread).
    */
   STN_proportion,
   /**
    * @brief Adjusted Proportion
    *
    * Share adjusted proportion (only used in scheduler thread).
    */
   STN_adjusted_proportion,
   /**
    * @brief Combined Usage
    *
    * Combined usage (only used in scheduler thread).
    */
   STN_combined_usage,
   /**
    * @brief Pass Two Sequence Number
    *
    * Sequence number for pass 2 of share calculation (only used in scheduler thread).
    */
   STN_pass2_seqno,
   /**
    * @brief Sum of Priorities
    *
    * Sum of job priorities (only used in scheduler thread).
    */
   STN_sum_priority,
   /**
    * @brief Actual Proportion
    *
    * Long term actual proportion.
    */
   STN_actual_proportion,
   /**
    * @brief M Share
    *
    * Dynamic long term targetted proportion.
    */
   STN_m_share,
   /**
    * @brief Last Actual Proportion
    *
    * Short term actual proportion.
    */
   STN_last_actual_proportion,
   /**
    * @brief Adjusted Current Proportion
    *
    * Short term targetted proportion.
    */
   STN_adjusted_current_proportion,
   /**
    * @brief Temporary Node
    *
    * Temporary Node used in scheduler only.
    */
   STN_temp,
   /**
    * @brief Short Term Targeted Proportion
    *
    * Short term targeted proportion of node as compared to sibling nodes.
    * Calculated during scheduling of pending of pending jobs.
    */
   STN_stt,
   /**
    * @brief Overall Short Term Targeted Proportion
    *
    * Overall short term targeted proportion of node as compared to all nodes.
    * Calculated during scheduling of pending of pending jobs.
    */
   STN_ostt,
   /**
    * @brief Long Term Targeted Proportion
    *
    * Long term targeted proportion of node as compared to all nodes.
    * Calculated during scheduling of pending of pending jobs.
    */
   STN_ltt,
   /**
    * @brief Overall Long Term Targeted Proportion
    *
    * Overall long term targeted proportion of node as compared to all nodes.
    * Calculated during scheduling of pending of pending jobs.
    */
   STN_oltt,
   /**
    * @brief Share
    *
    * Hierarchical calculated share node.
    * Calculated during scheduling of pending of pending jobs.
    */
   STN_shr,
   /**
    * @brief Sort Value
    *
    * Value for sorting jobs attached to a node.
    * Calculated during scheduling of pending of pending jobs.
    */
   STN_sort,
   /**
    * @brief Reference
    *
    * Temporary index reference back into the array of pending jobs.
    * Used during scheduling of pending of pending jobs.
    */
   STN_ref,
   /**
    * @brief Tickets
    *
    * Temporary storage of pending tickets from higher level policies.
    * Used during scheduling of pending of pending jobs.
    */
   STN_tickets,
   /**
    * @brief Job Id
    *
    * Job number of a temporary job node.
    * Used during scheduling of pending of pending jobs.
    */
   STN_jobid,
   /**
    * @brief Task Id
    *
    * Array task number of a temporary job node.
    * Used during scheduling of pending of pending jobs.
    */
   STN_taskid,
   /**
    * @brief Usage List
    *
    * Node usage list.
    * Used during scheduling of pending of pending jobs.
    */
   STN_usage_list,
   /**
    * @brief Share Tree Version
    *
    * Incremented on every change to the tree, and again by the timer thread when usage is recomputed. Lets a reader tell whether the tree it holds is still current.
    */
   STN_version,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   STN_joker,
};

namespace ocs::tom {

   struct UA;
   struct VA;

   /**
    * @brief Share Tree Node
    *
    * Represents one node of the share tree.
    */
   struct STN {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {STN_name, "STN_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {STN_type, "STN_type", FieldType::Uint32, Flag::Spool},
         {STN_id, "STN_id", FieldType::Uint32, Flag::None},
         {STN_shares, "STN_shares", FieldType::Uint32, Flag::Spool},
         {STN_children, "STN_children", FieldType::List, Flag::Spool},
         {STN_job_ref_count, "STN_job_ref_count", FieldType::Uint32, Flag::None},
         {STN_active_job_ref_count, "STN_active_job_ref_count", FieldType::Uint32, Flag::None},
         {STN_project, "STN_project", FieldType::Uint32, Flag::None},
         {STN_proportion, "STN_proportion", FieldType::Double, Flag::None},
         {STN_adjusted_proportion, "STN_adjusted_proportion", FieldType::Double, Flag::None},
         {STN_combined_usage, "STN_combined_usage", FieldType::Double, Flag::None},
         {STN_pass2_seqno, "STN_pass2_seqno", FieldType::Uint32, Flag::None},
         {STN_sum_priority, "STN_sum_priority", FieldType::Uint32, Flag::None},
         {STN_actual_proportion, "STN_actual_proportion", FieldType::Double, Flag::None},
         {STN_m_share, "STN_m_share", FieldType::Double, Flag::None},
         {STN_last_actual_proportion, "STN_last_actual_proportion", FieldType::Double, Flag::None},
         {STN_adjusted_current_proportion, "STN_adjusted_current_proportion", FieldType::Double, Flag::None},
         {STN_temp, "STN_temp", FieldType::Uint32, Flag::None},
         {STN_stt, "STN_stt", FieldType::Double, Flag::None},
         {STN_ostt, "STN_ostt", FieldType::Double, Flag::None},
         {STN_ltt, "STN_ltt", FieldType::Double, Flag::None},
         {STN_oltt, "STN_oltt", FieldType::Double, Flag::None},
         {STN_shr, "STN_shr", FieldType::Double, Flag::None},
         {STN_sort, "STN_sort", FieldType::Double, Flag::None},
         {STN_ref, "STN_ref", FieldType::Uint32, Flag::None},
         {STN_tickets, "STN_tickets", FieldType::Double, Flag::None},
         {STN_jobid, "STN_jobid", FieldType::Uint32, Flag::None},
         {STN_taskid, "STN_taskid", FieldType::Uint32, Flag::None},
         {STN_usage_list, "STN_usage_list", FieldType::List, Flag::None},
         {STN_version, "STN_version", FieldType::Uint32, Flag::None},
         {STN_joker, "STN_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of STN belongs to STN */
   template <>
   struct TypeOf<STN_Field> {
      using type = STN;   ///< the object type
   };

   /** @brief STN_children holds elements of STN */
   template <>
   struct SubType<STN_children> {
      using type = STN;   ///< the sub-type
   };

   /** @brief STN_usage_list holds elements of UA */
   template <>
   struct SubType<STN_usage_list> {
      using type = UA;   ///< the sub-type
   };

   /** @brief STN_joker holds elements of VA */
   template <>
   struct SubType<STN_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
