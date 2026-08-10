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
 * This code was generated from file source/libs/sgeobj/json/RQS.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Set
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource Quota Set
*
* A named, enableable set of rules that limit resource use across the object model.
* Where a queue or host limits what it can give, a resource quota set limits what a *combination* may take - "this project may use 100 slots in total, wherever they are". The rules are tried in order and the first that matches decides.
*
*    SGE_STRING(RQS_name) - Name
*    The set's name, as used by `qconf -mrqs`.
*
*    SGE_STRING(RQS_description) - Description
*    Free text describing what the set is for.
*
*    SGE_BOOL(RQS_enabled) - Enabled
*    Whether the set is applied. A disabled set stays configured but limits nothing.
*
*    SGE_LIST(RQS_rule) - Rules
*    The rules (`RQR_Type`), in the order they are tried.
*
*    SGE_LIST(RQS_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   RQS_name = RQS_LOWERBOUND,   ///< Name
   RQS_description,   ///< Description
   RQS_enabled,   ///< Enabled
   RQS_rule,   ///< Rules
   RQS_joker   ///< Joker
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

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define RQS_SIZE sizeof(RQSN)/sizeof(char *)


