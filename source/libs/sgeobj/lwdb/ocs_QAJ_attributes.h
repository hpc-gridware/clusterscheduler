#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   QAJ_host = 3450,
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
   QAJ_arid
};

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

