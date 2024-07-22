#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/MES.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Scheduling Message
*
* An individual scheduling message,
* e.g. explaining why a specific request of a certain job cannot be granted.
*
*    SGE_LIST(MES_job_number_list) - Job Number List
*    A list of job ids to which the message applies.
*    These are usually all jobs belonging to a specific job category (sharing the same requests).
*
*    SGE_ULONG(MES_message_number) - Message Number
*    Every possible scheduler message has an unique message number.
*
*    SGE_STRING(MES_message) - Message
*    The scheduler message as string.
*
*/

enum {
   MES_job_number_list = MES_LOWERBOUND,
   MES_message_number,
   MES_message
};

LISTDEF(MES_Type)
   SGE_LIST(MES_job_number_list, ULNG_Type, CULL_DEFAULT)
   SGE_ULONG(MES_message_number, CULL_DEFAULT)
   SGE_STRING(MES_message, CULL_DEFAULT)
LISTEND

NAMEDEF(MESN)
   NAME("MES_job_number_list")
   NAME("MES_message_number")
   NAME("MES_message")
NAMEEND

#define MES_SIZE sizeof(MESN)/sizeof(char *)


