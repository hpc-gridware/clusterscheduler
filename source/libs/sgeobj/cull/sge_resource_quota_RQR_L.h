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
 * This code was generated from file source/libs/sgeobj/json/RQR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(RQR_name) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(RQR_filter_users) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(RQR_filter_projects) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(RQR_filter_pes) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(RQR_filter_queues) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(RQR_filter_hosts) - @todo add summary
*    @todo add description
*
*    SGE_LIST(RQR_limit) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(RQR_level) - @todo add summary
*    @todo add description
*
*/

enum {
   RQR_name = RQR_LOWERBOUND,
   RQR_filter_users,
   RQR_filter_projects,
   RQR_filter_pes,
   RQR_filter_queues,
   RQR_filter_hosts,
   RQR_limit,
   RQR_level
};

LISTDEF(RQR_Type)
   SGE_STRING(RQR_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_OBJECT(RQR_filter_users, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_OBJECT(RQR_filter_projects, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_OBJECT(RQR_filter_pes, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_OBJECT(RQR_filter_queues, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_OBJECT(RQR_filter_hosts, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(RQR_limit, RQRL_Type, CULL_SPOOL)
   SGE_ULONG(RQR_level, CULL_DEFAULT)
LISTEND

NAMEDEF(RQRN)
   NAME("RQR_name")
   NAME("RQR_filter_users")
   NAME("RQR_filter_projects")
   NAME("RQR_filter_pes")
   NAME("RQR_filter_queues")
   NAME("RQR_filter_hosts")
   NAME("RQR_limit")
   NAME("RQR_level")
NAMEEND

#define RQR_SIZE sizeof(RQRN)/sizeof(char *)


