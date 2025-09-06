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
 * This code was generated from file source/libs/sgeobj/json/CCT.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   CCT_pe_name = 11500,
   CCT_ignore_queues,
   CCT_ignore_hosts,
   CCT_job_messages,
   CCT_pe_job_slots,
   CCT_pe_job_slot_count
};

constexpr const int CCT_Type[] = {
   CCT_pe_name,
   CCT_ignore_queues,
   CCT_ignore_hosts,
   CCT_job_messages,
   CCT_pe_job_slots,
   CCT_pe_job_slot_count,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define CCT_ATTRIBUTES \
   {CCT_pe_name, "CCT_pe_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_ignore_queues, "CCT_ignore_queues", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_ignore_hosts, "CCT_ignore_hosts", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_job_messages, "CCT_job_messages", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_pe_job_slots, "CCT_pe_job_slots", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_pe_job_slot_count, "CCT_pe_job_slot_count", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

