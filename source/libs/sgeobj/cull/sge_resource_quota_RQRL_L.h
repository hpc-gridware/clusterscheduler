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
 * This code was generated from file source/libs/sgeobj/json/RQRL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Rule Limit
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource Quota Rule Limit
*
* One resource a rule limits, the configured ceiling, and what is currently booked against it.
*
*    SGE_STRING(RQRL_name) - Resource Name
*    The complex attribute being limited, e.g. `slots`.
*
*    SGE_STRING(RQRL_value) - Configured Value
*    The limit as configured, still as text. For a dynamic limit this is the formula rather than a number.
*
*    SGE_ULONG(RQRL_type) - Value Type
*    The complex attribute's type, copied from `CE_valtype`, which decides how RQRL_value is parsed.
*
*    SGE_DOUBLE(RQRL_dvalue) - Evaluated Value
*    The limit as a number. For a static limit this is RQRL_value parsed; for a dynamic one it is the formula evaluated against the host currently being considered.
*
*    SGE_LIST(RQRL_usage) - Usage
*    What is booked against this limit right now (`RUE_Type`), one entry per name when the filter expands.
*
*    SGE_BOOL(RQRL_dynamic) - Dynamic
*    The limit is a formula over host complexes rather than a constant, so RQRL_dvalue has to be recomputed per host instead of read once.
*
*/

enum {
   RQRL_name = RQRL_LOWERBOUND,   ///< Resource Name
   RQRL_value,   ///< Configured Value
   RQRL_type,   ///< Value Type
   RQRL_dvalue,   ///< Evaluated Value
   RQRL_usage,   ///< Usage
   RQRL_dynamic   ///< Dynamic
};

LISTDEF(RQRL_Type)
   SGE_STRING(RQRL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_STRING(RQRL_value, CULL_SPOOL)
   SGE_ULONG(RQRL_type, CULL_SPOOL)
   SGE_DOUBLE(RQRL_dvalue, CULL_SPOOL)
   SGE_LIST(RQRL_usage, RUE_Type, CULL_DEFAULT)
   SGE_BOOL(RQRL_dynamic, CULL_DEFAULT)
LISTEND

NAMEDEF(RQRLN)
   NAME("RQRL_name")
   NAME("RQRL_value")
   NAME("RQRL_type")
   NAME("RQRL_dvalue")
   NAME("RQRL_usage")
   NAME("RQRL_dynamic")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define RQRL_SIZE sizeof(RQRLN)/sizeof(char *)


