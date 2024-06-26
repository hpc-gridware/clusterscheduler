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
 * This code was generated from file source/libs/sgeobj/json/CF.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Configuration Element
*
* A configuration element contains one line of a configuration, see qconf -sconf, e.g.
* - execd_spool_dir
* - mailer
* - xterm
* - load_sensor
* - ...
*
*    SGE_STRING(CF_name) - Configuration Name
*    Name of the configuration element (the left column of qconf -sconf).
*
*    SGE_STRING(CF_value) - Configuration Value
*    The value of a configuration element (the right column of qconf -sconf).
*
*    SGE_LIST(CF_sublist) - Configuration Sublist
*    Recursive Sublist of CF_Type. @todo is it still required? It is only used in libs/gdi/sge_qtcsh.cc.
*
*    SGE_ULONG(CF_local) - Local Configuration
*    Is it a local configuration (true) or the global configuration (false).
*
*/

enum {
   CF_name = CF_LOWERBOUND,
   CF_value,
   CF_sublist,
   CF_local
};

LISTDEF(CF_Type)
   SGE_STRING(CF_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(CF_value, CULL_SUBLIST)
   SGE_LIST(CF_sublist, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(CF_local, CULL_DEFAULT)
LISTEND

NAMEDEF(CFN)
   NAME("CF_name")
   NAME("CF_value")
   NAME("CF_sublist")
   NAME("CF_local")
NAMEEND

#define CF_SIZE sizeof(CFN)/sizeof(char *)


