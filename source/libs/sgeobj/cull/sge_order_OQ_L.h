#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/OQ.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Queue Info in an Order
*
* Job start orders and also orders for reprioritization contain per queue information.
* One or more objects of type OrderQueue are contained in the queuelist attribute of an order.
*
*    SGE_ULONG(OQ_slots) - Slots
*    The number of slots a job occupies in the queue / queue instance.
*
*    SGE_STRING(OQ_dest_queue) - Qinstance Name
*    The full queue instance name (cluster queue name + @ + host name).
*
*    SGE_ULONG(OQ_dest_version) - Qinstance Version
*    Version of the queue instance. Is used to detect if the queue has been modified during scheduling.
*    In this case the scheduling result is skipped.
*
*    SGE_DOUBLE(OQ_ticket) - Total Tickets
*    Total SGEEE tickets for slots.
*
*    SGE_DOUBLE(OQ_oticket) - Override Tickets
*    Total SGEEE override tickets.
*
*    SGE_DOUBLE(OQ_fticket) - Functional Tickets
*    Total SGEEE functional tickets.
*
*    SGE_DOUBLE(OQ_sticket) - Sharetree Tickets
*    Total SGEEE sharetree tickets.
*
*    SGE_LIST(OQ_binding_to_use) - Binding that should be used
*    One entry for sequential jobs or host specific binding, multiple entries for PE jobs in case of task specific binding
*
*/

enum {
   OQ_slots = OQ_LOWERBOUND,
   OQ_dest_queue,
   OQ_dest_version,
   OQ_ticket,
   OQ_oticket,
   OQ_fticket,
   OQ_sticket,
   OQ_binding_to_use
};

LISTDEF(OQ_Type)
   SGE_ULONG(OQ_slots, CULL_DEFAULT)
   SGE_STRING(OQ_dest_queue, CULL_DEFAULT)
   SGE_ULONG(OQ_dest_version, CULL_DEFAULT)
   SGE_DOUBLE(OQ_ticket, CULL_DEFAULT)
   SGE_DOUBLE(OQ_oticket, CULL_DEFAULT)
   SGE_DOUBLE(OQ_fticket, CULL_DEFAULT)
   SGE_DOUBLE(OQ_sticket, CULL_DEFAULT)
   SGE_LIST(OQ_binding_to_use, ST_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(OQN)
   NAME("OQ_slots")
   NAME("OQ_dest_queue")
   NAME("OQ_dest_version")
   NAME("OQ_ticket")
   NAME("OQ_oticket")
   NAME("OQ_fticket")
   NAME("OQ_sticket")
   NAME("OQ_binding_to_use")
NAMEEND

#define OQ_SIZE sizeof(OQN)/sizeof(char *)


