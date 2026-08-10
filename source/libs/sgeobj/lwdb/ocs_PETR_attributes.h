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
 * This code was generated from file source/libs/sgeobj/json/PETR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief PE Task Request
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of PETR
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   PETR_jobid = 4150,   ///< Job ID
   PETR_jataskid,   ///< Array Task ID
   PETR_queuename,   ///< Queue Name
   PETR_owner,   ///< Owner
   PETR_cwd,   ///< Current Working Directory
   PETR_path_aliases,   ///< Path Aliases
   PETR_environment,   ///< Environment
   PETR_submission_time,   ///< Submission Time
   PETR_cred   ///< TLS Credentials
};

/** @brief The attribute ids of PETR, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int PETR_Type[] = {
   PETR_jobid,
   PETR_jataskid,
   PETR_queuename,
   PETR_owner,
   PETR_cwd,
   PETR_path_aliases,
   PETR_environment,
   PETR_submission_time,
   PETR_cred,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of PETR
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define PETR_ATTRIBUTES \
   {PETR_jobid, "PETR_jobid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PETR_jataskid, "PETR_jataskid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PETR_queuename, "PETR_queuename", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PETR_owner, "PETR_owner", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PETR_cwd, "PETR_cwd", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PETR_path_aliases, "PETR_path_aliases", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PETR_environment, "PETR_environment", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PETR_submission_time, "PETR_submission_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PETR_cred, "PETR_cred", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

