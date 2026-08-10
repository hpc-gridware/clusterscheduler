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
 * This code was generated from file source/libs/sgeobj/json/JG.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Granted Destination Identifier
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of JG
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   JG_qname = 2850,   ///< Queue Instance Name
   JG_qversion,   ///< Queue Version
   JG_qhostname,   ///< Qualified Hostname
   JG_slots,   ///< Number of Slots
   JG_queue,   ///< Queue Object
   JG_tag_slave_job,   ///< Tag for Slave Job Delivery
   JG_ticket,   ///< Total Tickets
   JG_oticket,   ///< Override Tickets
   JG_fticket,   ///< Functional Tickets
   JG_sticket,   ///< Sharetree Tickets
   JG_jcoticket,   ///< Job Class Override Tickets
   JG_jcfticket,   ///< Job Class Functional Tickets
   JG_processors,   ///< Processor Set
   JG_binding_to_use,   ///< Binding that should be used
   JG_granted_rsmaps   ///< Granted RSMAP IDs
};

/** @brief The attribute ids of JG, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
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
   JG_binding_to_use,
   JG_granted_rsmaps,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of JG
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define JG_ATTRIBUTES \
   {JG_qname, "JG_qname", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {JG_qversion, "JG_qversion", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_qhostname, "JG_qhostname", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, false}, \
   {JG_slots, "JG_slots", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_queue, "JG_queue", AttributeStatic::OBJECT, nullptr, 0, AttributeStatic::NO_HASH, false, false}, \
   {JG_tag_slave_job, "JG_tag_slave_job", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_ticket, "JG_ticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_oticket, "JG_oticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_fticket, "JG_fticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_sticket, "JG_sticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_jcoticket, "JG_jcoticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_jcfticket, "JG_jcfticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_processors, "JG_processors", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_binding_to_use, "JG_binding_to_use", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JG_granted_rsmaps, "JG_granted_rsmaps", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

