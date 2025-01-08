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
 * This code was generated from file source/libs/sgeobj/json/PA.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Path Alias
*
* Object holding information necessary to realize path aliasing.
* Path aliasing is configured in $SGE_ROOT/$SGE_CELL/sge_aliases and/or $HOME/.sge_aliases
* See man page sge_aliases.5
*
*    SGE_STRING(PA_origin) - Original Path
*    The original path before applying aliasing.
*
*    SGE_HOST(PA_submit_host) - Submit Host
*    The host from which the job was submitted.
*
*    SGE_HOST(PA_exec_host) - Exec Host
*    The destination execution host.
*
*    SGE_STRING(PA_translation) - Translation
*    The path after applying path aliasing.
*
*/

enum {
   PA_origin = PA_LOWERBOUND,
   PA_submit_host,
   PA_exec_host,
   PA_translation
};

LISTDEF(PA_Type)
   SGE_STRING(PA_origin, CULL_PRIMARY_KEY | CULL_SPOOL)
   SGE_HOST(PA_submit_host, CULL_SPOOL)
   SGE_HOST(PA_exec_host, CULL_SPOOL)
   SGE_STRING(PA_translation, CULL_SPOOL)
LISTEND

NAMEDEF(PAN)
   NAME("PA_origin")
   NAME("PA_submit_host")
   NAME("PA_exec_host")
   NAME("PA_translation")
NAMEEND

#define PA_SIZE sizeof(PAN)/sizeof(char *)


