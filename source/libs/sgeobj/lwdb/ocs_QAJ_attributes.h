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
 * This code was generated from file source/libs/sgeobj/json/QAJ.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Accounting Summary
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of QAJ
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   QAJ_host = 3050,   ///< Host Name
   QAJ_queue,   ///< Cluster Queue Name
   QAJ_group,   ///< User Group
   QAJ_owner,   ///< Owner
   QAJ_project,   ///< Project
   QAJ_department,   ///< Department
   QAJ_ru_wallclock,   ///< Rusage Wallclock
   QAJ_ru_utime,   ///< Rusage User Time
   QAJ_ru_stime,   ///< Rusage System Time
   QAJ_ru_maxrss,   ///< Rusage Maximum RSS
   QAJ_ru_inblock,   ///< Rusage Block Input
   QAJ_granted_pe,   ///< Granted Parallel Environment
   QAJ_slots,   ///< Slots
   QAJ_cpu,   ///< Cpu Usage
   QAJ_mem,   ///< Integral Memory Usage
   QAJ_io,   ///< IO Usage
   QAJ_iow,   ///< IO Wait Time
   QAJ_maxvmem,   ///< Maximum Virtual Memory
   QAJ_arid   ///< AR Id
};

/** @brief The attribute ids of QAJ, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int QAJ_Type[] = {
   QAJ_host,
   QAJ_queue,
   QAJ_group,
   QAJ_owner,
   QAJ_project,
   QAJ_department,
   QAJ_ru_wallclock,
   QAJ_ru_utime,
   QAJ_ru_stime,
   QAJ_ru_maxrss,
   QAJ_ru_inblock,
   QAJ_granted_pe,
   QAJ_slots,
   QAJ_cpu,
   QAJ_mem,
   QAJ_io,
   QAJ_iow,
   QAJ_maxvmem,
   QAJ_arid,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of QAJ
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define QAJ_ATTRIBUTES \
   {QAJ_host, "QAJ_host", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_queue, "QAJ_queue", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_group, "QAJ_group", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_owner, "QAJ_owner", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_project, "QAJ_project", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_department, "QAJ_department", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_ru_wallclock, "QAJ_ru_wallclock", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_ru_utime, "QAJ_ru_utime", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_ru_stime, "QAJ_ru_stime", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_ru_maxrss, "QAJ_ru_maxrss", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_ru_inblock, "QAJ_ru_inblock", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_granted_pe, "QAJ_granted_pe", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_slots, "QAJ_slots", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_cpu, "QAJ_cpu", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_mem, "QAJ_mem", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_io, "QAJ_io", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_iow, "QAJ_iow", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_maxvmem, "QAJ_maxvmem", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QAJ_arid, "QAJ_arid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

