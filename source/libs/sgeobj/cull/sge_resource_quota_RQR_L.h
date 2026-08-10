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
 * This code was generated from file source/libs/sgeobj/json/RQR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Rule
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource Quota Rule
*
* One rule of a resource quota set: what it matches, and what it then limits.
* The five filters are ANDed. An empty filter matches everything, so a rule with no filters is the catch-all.
*
*    SGE_STRING(RQR_name) - Name
*    Optional rule name, so `qquota` and the error messages can say which rule bit.
*
*    SGE_OBJECT(RQR_filter_users) - User Filter
*    Which submitting users the rule applies to (`RQRF_Type`).
*
*    SGE_OBJECT(RQR_filter_projects) - Project Filter
*    Which projects the rule applies to.
*
*    SGE_OBJECT(RQR_filter_pes) - Parallel Environment Filter
*    Which parallel environments the rule applies to.
*
*    SGE_OBJECT(RQR_filter_queues) - Queue Filter
*    Which cluster queues the rule applies to.
*
*    SGE_OBJECT(RQR_filter_hosts) - Host Filter
*    Which execution hosts the rule applies to.
*
*    SGE_LIST(RQR_limit) - Limits
*    What the rule limits and by how much (`RQRL_Type`), one entry per resource.
*
*    SGE_ULONG(RQR_level) - Limit Level
*    How far the limit is spread: shared by everything matched, or one limit each per host, cluster queue or queue instance. See the `RQR_*` values in `sge_resource_quota.h`.
*
*/

enum {
   RQR_name = RQR_LOWERBOUND,   ///< Name
   RQR_filter_users,   ///< User Filter
   RQR_filter_projects,   ///< Project Filter
   RQR_filter_pes,   ///< Parallel Environment Filter
   RQR_filter_queues,   ///< Queue Filter
   RQR_filter_hosts,   ///< Host Filter
   RQR_limit,   ///< Limits
   RQR_level   ///< Limit Level
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

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define RQR_SIZE sizeof(RQRN)/sizeof(char *)


