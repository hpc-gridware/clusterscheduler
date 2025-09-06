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
 * This code was generated from file source/libs/sgeobj/json/QIM.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Queue Instance Message
*
* Objects of this type hold messages attached to queues (QU_message_list).
* It is also used for ja_task specific messages (JAT_message_list), e.g. job error messages
*
*    SGE_ULONG(QIM_type) - Message Type
*    QI states defined in libs/sgeobj/sge_qinstance_state.h, e.g. QI_ERROR, QI_AMBIGUOUS, ...
*    Seems to be always 1 for ja_task messages.
*
*    SGE_STRING(QIM_message) - Message
*    Message as string.
*
*/

enum {
   QIM_type = QIM_LOWERBOUND,
   QIM_message
};

LISTDEF(QIM_Type)
   SGE_ULONG(QIM_type, CULL_SPOOL)
   SGE_STRING(QIM_message, CULL_SPOOL)
LISTEND

NAMEDEF(QIMN)
   NAME("QIM_type")
   NAME("QIM_message")
NAMEEND

#define QIM_SIZE sizeof(QIMN)/sizeof(char *)


