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
 * This code was generated from file source/libs/sgeobj/json/CONF.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Host Configuration
*
* Contains configuration options for hosts (execution hosts but also a global configuration for the sge_qmaster).
* Host specific configurations inherit values from the global configuration.
* @todo there is an overlap with the exec host type (EH_Type), can this be unified?
*
*    SGE_HOST(CONF_name) - Host Name
*    Name of the host the configuration object refers to, or global for the global configuration.
*
*    SGE_ULONG(CONF_version) - Configuration Version
*    Each configuration object has a version number which is increased with every change.
*
*    SGE_LIST(CONF_entries) - Configuration Entries
*    A configuration consists of multiple configuration entries of CF_Type.
*
*/

enum {
   CONF_name = CONF_LOWERBOUND,
   CONF_version,
   CONF_entries
};

LISTDEF(CONF_Type)
   SGE_HOST(CONF_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_ULONG(CONF_version, CULL_SPOOL)
   SGE_LIST(CONF_entries, CF_Type, CULL_SPOOL)
LISTEND

NAMEDEF(CONFN)
   NAME("CONF_name")
   NAME("CONF_version")
   NAME("CONF_entries")
NAMEEND

#define CONF_SIZE sizeof(CONFN)/sizeof(char *)


