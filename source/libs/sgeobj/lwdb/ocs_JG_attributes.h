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
 * This code was generated from file source/libs/sgeobj/json/JG.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   JG_qname = 3250,
   JG_qversion,
   JG_qhostname,
   JG_slots,
   JG_queue,
   JG_tag_slave_job,
   JG_ticket,
   JG_oticket,
   JG_fticket,
   JG_sticket,
   JG_jcoticket,
   JG_jcfticket,
   JG_processors
};

constexpr const int JG_Type[] = {
   JG_qname,
   JG_qversion,
   JG_qhostname,
   JG_slots,
   JG_queue,
   JG_tag_slave_job,
   JG_ticket,
   JG_oticket,
   JG_fticket,
   JG_sticket,
   JG_jcoticket,
   JG_jcfticket,
   JG_processors,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define JG_ATTRIBUTES \
   {JG_qname, "JG_qname", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {JG_qversion, "JG_qversion", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JG_qhostname, "JG_qhostname", AttributeStatic::HOST, AttributeStatic::UNORDERED_UNIQUE}, \
   {JG_slots, "JG_slots", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JG_queue, "JG_queue", AttributeStatic::OBJECT, AttributeStatic::NO_HASH}, \
   {JG_tag_slave_job, "JG_tag_slave_job", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JG_ticket, "JG_ticket", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JG_oticket, "JG_oticket", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JG_fticket, "JG_fticket", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JG_sticket, "JG_sticket", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JG_jcoticket, "JG_jcoticket", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JG_jcfticket, "JG_jcfticket", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JG_processors, "JG_processors", AttributeStatic::STRING, AttributeStatic::NO_HASH} \

} // end namespace

