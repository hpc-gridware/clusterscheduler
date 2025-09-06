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
 * This code was generated from file source/libs/sgeobj/json/EVR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(EVR_operation) - @todo add summary
*    @todo add description
*
*    SGE_ULONG64(EVR_timestamp) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EVR_event_client_id) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(EVR_event_number) - @todo add summary
*    @todo add description
*
*    SGE_STRING(EVR_session) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(EVR_event_client) - @todo add summary
*    @todo add description
*
*    SGE_LIST(EVR_event_list) - @todo add summary
*    @todo add description
*
*/

enum {
   EVR_operation = EVR_LOWERBOUND,
   EVR_timestamp,
   EVR_event_client_id,
   EVR_event_number,
   EVR_session,
   EVR_event_client,
   EVR_event_list
};

LISTDEF(EVR_Type)
   SGE_ULONG(EVR_operation, CULL_DEFAULT)
   SGE_ULONG64(EVR_timestamp, CULL_DEFAULT)
   SGE_ULONG(EVR_event_client_id, CULL_DEFAULT)
   SGE_ULONG(EVR_event_number, CULL_DEFAULT)
   SGE_STRING(EVR_session, CULL_DEFAULT)
   SGE_OBJECT(EVR_event_client, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_LIST(EVR_event_list, ET_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(EVRN)
   NAME("EVR_operation")
   NAME("EVR_timestamp")
   NAME("EVR_event_client_id")
   NAME("EVR_event_number")
   NAME("EVR_session")
   NAME("EVR_event_client")
   NAME("EVR_event_list")
NAMEEND

#define EVR_SIZE sizeof(EVRN)/sizeof(char *)


