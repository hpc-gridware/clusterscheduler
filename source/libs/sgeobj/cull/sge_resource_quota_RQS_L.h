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
 * This code was generated from file source/libs/sgeobj/json/RQS.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(RQS_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(RQS_description) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(RQS_enabled) - @todo add summary
*    @todo add description
*
*    SGE_LIST(RQS_rule) - @todo add summary
*    @todo add description
*
*    SGE_LIST(RQS_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   RQS_name = RQS_LOWERBOUND,
   RQS_description,
   RQS_enabled,
   RQS_rule,
   RQS_joker
};

LISTDEF(RQS_Type)
   SGE_STRING(RQS_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_STRING(RQS_description, CULL_SPOOL)
   SGE_BOOL(RQS_enabled, CULL_SPOOL)
   SGE_LIST(RQS_rule, RQR_Type, CULL_SPOOL)
   SGE_LIST(RQS_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(RQSN)
   NAME("RQS_name")
   NAME("RQS_description")
   NAME("RQS_enabled")
   NAME("RQS_rule")
   NAME("RQS_joker")
NAMEEND

#define RQS_SIZE sizeof(RQSN)/sizeof(char *)


