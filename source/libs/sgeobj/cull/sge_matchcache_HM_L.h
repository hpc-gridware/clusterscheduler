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
 * This code was generated from file source/libs/sgeobj/json/HM.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Match Cache Entry
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Match Cache Entry
*
* CS-2680: one host that a matcher of a host group has admitted.
*  
* A matcher describes a set of hosts instead of naming one, so a host it
* admits need not appear in any object of the cluster - and therefore
* cannot appear in the resolved host list either. For such a host every
* request misses the resolved list and would otherwise repeat the whole
* recursive matcher walk, on the entry path of the qmaster, per request.
*  
* An entry records that the walk has already said yes. It is process local:
* not spooled, and never handed to a reader or an event subscriber, so each
* data store holds what its own threads have learned. The entry cannot be
* a bare name because it expires by the time of its last use.
*
*    SGE_HOST(HM_name) - Name
*    The admitted host name.
*     
*    Hashed, so that the lookup in front of the matcher walk is a hash access
*    and so that an entry can be found again by name when it has to be
*    removed.
*
*    SGE_ULONG64(HM_last_used) - Time of Last Use
*    When this entry was last consulted, as a 64 bit timestamp - which
*    in this product means microseconds since the epoch, the convention every
*    other 64 bit timestamp of the object model follows.
*     
*    An entry expires when it has not been used for the configured period, so
*    the cache is bounded by the working set rather than by the uptime of the
*    qmaster - in an environment that does not reuse instance names it would
*    otherwise grow without bound.
*     
*    Written on every hit, and therefore written under no lock but the read
*    lock the caller already holds. It carries no invariant with any other
*    field: several threads writing "now" produce competing values, every one
*    of which is a valid recent time of use. What it needs is atomicity, not
*    mutual exclusion - see the atomic accessors of the object layer.
*
*/

enum {
   HM_name = HM_LOWERBOUND,   ///< Name
   HM_last_used   ///< Time of Last Use
};

LISTDEF(HM_Type)
   SGE_HOST(HM_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH)
   SGE_ULONG64(HM_last_used, CULL_DEFAULT)
LISTEND

NAMEDEF(HMN)
   NAME("HM_name")
   NAME("HM_last_used")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define HM_SIZE sizeof(HMN)/sizeof(char *)


