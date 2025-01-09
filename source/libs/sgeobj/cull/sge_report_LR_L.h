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
 * This code was generated from file source/libs/sgeobj/json/LR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Load Report
*
* A LoadReport object represents the value for a single load variable on a specific host.
*
*    SGE_STRING(LR_name) - Load Variable Name
*    Name of the load variable. In order for the load value to be processed in sge_qmaster
*    a complex variable with this name must have been configured.
*
*    SGE_STRING(LR_value) - Load Variable Value
*    Value of the variable on a specific host.
*
*    SGE_ULONG(LR_global) - Is Global
*    Specifies if it is a global load variable.
*    1 means that it is a global load value (host is global in this case),
*    0 means that it is a host specific load value.
*    @todo: make it a boolean
*
*    SGE_ULONG(LR_is_static) - Is Static
*    Specifies if it is a static load variable.
*    Static load variables represent seldomly changing variables, e.g. arch, n_proc, mem_total.
*    0 means a non static load value
*    1 means a static load value
*    2 is a special internal value: remove the load value
*
*    SGE_HOST(LR_host) - Host Name
*    Name of the host on which the load value is valid. Specific host name or keyword global for global load values.
*
*/

enum {
   LR_name = LR_LOWERBOUND,
   LR_value,
   LR_global,
   LR_is_static,
   LR_host
};

LISTDEF(LR_Type)
   SGE_STRING(LR_name, CULL_HASH)
   SGE_STRING(LR_value, CULL_DEFAULT)
   SGE_ULONG(LR_global, CULL_DEFAULT)
   SGE_ULONG(LR_is_static, CULL_DEFAULT)
   SGE_HOST(LR_host, CULL_HASH)
LISTEND

NAMEDEF(LRN)
   NAME("LR_name")
   NAME("LR_value")
   NAME("LR_global")
   NAME("LR_is_static")
   NAME("LR_host")
NAMEEND

#define LR_SIZE sizeof(LRN)/sizeof(char *)


