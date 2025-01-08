#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/SO.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief SubordinateQueue
*
* Subordinate (queue instances) are defined in a sub list of a queue instance.
* We want to use the configuration parameter subordinate_list
* for both the classic queue instance-wise suspend on subordinate
* and the slot-wise suspend on subordinate.
* The fields SO_name and SO_threshold are used by the queue instance-wise
* suspend on subordinate, SO_name, SO_slots_sum, SO_seq_no and SO_action
* are used by the slot-wise suspend on subordinate.
* If SO_slots_sum is 0, it's queue instance-wise, otherwise slot-wise
* suspend on subordinate that is configured.
*
*    SGE_STRING(SO_name) - Subordinate Queue Name
*    Name of the subordinate queue.
*
*    SGE_ULONG(SO_threshold) - Threshold
*    The threshold (slots) defines when the subordination action (suspend) will be triggered.
*
*    SGE_ULONG(SO_slots_sum) - Slots Sum
*    Used for slot-wise SOS.
*
*    SGE_ULONG(SO_seq_no) - Sequence Number
*    Used for slot-wise SOS.
*
*    SGE_ULONG(SO_action) - Action
*    Subordination action, ussed for slot-wise SOS:
*    - SO_ACTION_SR: suspend the task with the shortest runtime
*    - SO_ACTION_LR: suspend the task with the longest runtime
*
*/

enum {
   SO_name = SO_LOWERBOUND,
   SO_threshold,
   SO_slots_sum,
   SO_seq_no,
   SO_action
};

LISTDEF(SO_Type)
   SGE_STRING(SO_name, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_ULONG(SO_threshold, CULL_SUBLIST)
   SGE_ULONG(SO_slots_sum, CULL_SUBLIST)
   SGE_ULONG(SO_seq_no, CULL_SUBLIST)
   SGE_ULONG(SO_action, CULL_SUBLIST)
LISTEND

NAMEDEF(SON)
   NAME("SO_name")
   NAME("SO_threshold")
   NAME("SO_slots_sum")
   NAME("SO_seq_no")
   NAME("SO_action")
NAMEEND

#define SO_SIZE sizeof(SON)/sizeof(char *)


