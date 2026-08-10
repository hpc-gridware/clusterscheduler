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
 * @brief PTF Job
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of JL
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   JL_job_ID = 4550,   ///< Job Id
   JL_OS_job_list,   ///< OS Job List
   JL_state,   ///< State
   JL_tickets,   ///< Tickets
   JL_share,   ///< Share
   JL_ticket_share,   ///< Ticket Share
   JL_timeslice,   ///< Timeslice
   JL_usage,   ///< Usage
   JL_old_usage_value,   ///< Old Usage
   JL_adjusted_usage,   ///< Adjusted Usage
   JL_last_usage,   ///< Last Usage
   JL_old_usage,   ///< Old Usage
   JL_proportion,   ///< Proportion
   JL_adjusted_proportion,   ///< Adjusted Proportion
   JL_adjusted_current_proportion,   ///< Adjusted Current Proportion
   JL_actual_proportion,   ///< Actual Proportion
   JL_diff_proportion,   ///< Diff Proportion
   JL_last_proportion,   ///< Last Proportion
   JL_curr_pri,   ///< Current Priority
   JL_pri,   ///< Priority
   JL_procfd,   ///< Proc FD
   JL_interactive   ///< Interactive
};

/** @brief The attribute ids of JL, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int JL_Type[] = {
   JL_job_ID,
   JL_OS_job_list,
   JL_state,
   JL_tickets,
   JL_share,
   JL_ticket_share,
   JL_timeslice,
   JL_usage,
   JL_old_usage_value,
   JL_adjusted_usage,
   JL_last_usage,
   JL_old_usage,
   JL_proportion,
   JL_adjusted_proportion,
   JL_adjusted_current_proportion,
   JL_actual_proportion,
   JL_diff_proportion,
   JL_last_proportion,
   JL_curr_pri,
   JL_pri,
   JL_procfd,
   JL_interactive,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of JL
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define JL_ATTRIBUTES \
   {JL_job_ID, "JL_job_ID", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {JL_OS_job_list, "JL_OS_job_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_state, "JL_state", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_tickets, "JL_tickets", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_share, "JL_share", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_ticket_share, "JL_ticket_share", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_timeslice, "JL_timeslice", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_usage, "JL_usage", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_old_usage_value, "JL_old_usage_value", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_adjusted_usage, "JL_adjusted_usage", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_last_usage, "JL_last_usage", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_old_usage, "JL_old_usage", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_proportion, "JL_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_adjusted_proportion, "JL_adjusted_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_adjusted_current_proportion, "JL_adjusted_current_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_actual_proportion, "JL_actual_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_diff_proportion, "JL_diff_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_last_proportion, "JL_last_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_curr_pri, "JL_curr_pri", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_pri, "JL_pri", AttributeStatic::LONG, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_procfd, "JL_procfd", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JL_interactive, "JL_interactive", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

