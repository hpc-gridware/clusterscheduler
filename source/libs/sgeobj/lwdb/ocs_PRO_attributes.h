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
 * This code was generated from file source/libs/sgeobj/json/PRO.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Process Element
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of PRO
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   PRO_pid = 12500,   ///< Pid
   PRO_utime,   ///< User Time
   PRO_stime,   ///< System Time
   PRO_vsize,   ///< Virtual Memory
   PRO_rss,   ///< Resident Set Size
   PRO_groups,   ///< Groups
   PRO_rel,   ///< Related to Cluster Scheduler Job
   PRO_run,   ///< Running
   PRO_io,   ///< IO
   PRO_ioops,   ///< IOOPS
   PRO_iow   ///< IOW
};

/** @brief The attribute ids of PRO, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int PRO_Type[] = {
   PRO_pid,
   PRO_utime,
   PRO_stime,
   PRO_vsize,
   PRO_rss,
   PRO_groups,
   PRO_rel,
   PRO_run,
   PRO_io,
   PRO_ioops,
   PRO_iow,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of PRO
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define PRO_ATTRIBUTES \
   {PRO_pid, "PRO_pid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {PRO_utime, "PRO_utime", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PRO_stime, "PRO_stime", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PRO_vsize, "PRO_vsize", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PRO_rss, "PRO_rss", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PRO_groups, "PRO_groups", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PRO_rel, "PRO_rel", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PRO_run, "PRO_run", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PRO_io, "PRO_io", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PRO_ioops, "PRO_ioops", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PRO_iow, "PRO_iow", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

