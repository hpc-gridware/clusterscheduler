#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   JL_job_ID = 4950,
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
   JL_interactive
};

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

#define JL_ATTRIBUTES \
   {JL_job_ID, "JL_job_ID", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JL_OS_job_list, "JL_OS_job_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JL_state, "JL_state", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JL_tickets, "JL_tickets", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JL_share, "JL_share", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_ticket_share, "JL_ticket_share", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_timeslice, "JL_timeslice", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_usage, "JL_usage", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_old_usage_value, "JL_old_usage_value", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_adjusted_usage, "JL_adjusted_usage", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_last_usage, "JL_last_usage", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_old_usage, "JL_old_usage", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_proportion, "JL_proportion", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_adjusted_proportion, "JL_adjusted_proportion", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_adjusted_current_proportion, "JL_adjusted_current_proportion", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_actual_proportion, "JL_actual_proportion", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_diff_proportion, "JL_diff_proportion", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_last_proportion, "JL_last_proportion", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_curr_pri, "JL_curr_pri", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JL_pri, "JL_pri", AttributeStatic::LONG, AttributeStatic::NO_HASH}, \
   {JL_procfd, "JL_procfd", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JL_interactive, "JL_interactive", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

