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
 * This code was generated from file source/libs/sgeobj/json/CE.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Complex Entry
*
* A complex entry contains a complex variable, both its definition as well as a possible current value
* A complex variable is used for defining all types of attributes in Cluster Scheduler
* as well as defining fixed values of resources and the capacity of consumable resources.
* @todo should be we better split definition and values into two objects?
*
*    SGE_STRING(CE_name) - Full Name
*    Full name of a complex variable.
*
*    SGE_STRING(CE_shortcut) - Shortcut Name
*    Shortcut for the complex variable name which can be used as an alternative to the name,
*    e.g. in job submission with qsub or when querying resources with qstat -F.
*
*    SGE_ULONG(CE_valtype) - Variable Type
*    Type of the complex variable defined in common/basis_types.h, e.g.
*      - TYPE_INT
*      - TYPE_STR
*      - TYPE_TIM
*      - TYPE_MEM
*      - ...
*    @todo instead of defines, should we better use an enum?
*
*    SGE_STRING(CE_stringval) - String Value
*    Value of the complex variable as string, from old docs: non overwritten value.
*
*    SGE_DOUBLE(CE_doubleval) - Double Value
*    Value of the complex variable as double, from old docs: parsed CE_stringval
*
*    SGE_ULONG(CE_relop) - Relational Operator
*    Relational operator used in comparison of complex variables (e.g. against requests).
*    Defined in libs/sgeobj/sge_centry.h, e.g.
*    CMPLXEQ_OP
*    CMPLXGE_OP
*    CMPLXGT_OP
*    ...
*
*    SGE_ULONG(CE_consumable) - Consumable Flag
*    Defines if a complex variable is consumable and if it is a per job or per slot consumable.
*    Defined in libs/sgeobj/sge_centry.h, possible values are
*    CONSUMABLE_NO
*    CONSUMABLE_YES
*    CONSUMABLE_JOB
*    CONSUMABLE_HOST
*
*    SGE_STRING(CE_defaultval) - Default Value
*    Default value (default request) as string.
*
*    SGE_ULONG(CE_dominant) - Monitoring Facility
*    @todo add description
*
*    SGE_STRING(CE_pj_stringval) - Per Job String Value
*    Per job string value, @todo add more information
*
*    SGE_DOUBLE(CE_pj_doubleval) - Per Job Double Value
*    Per job double values, parsed from CE_stringval (?)
*
*    SGE_ULONG(CE_pj_dominant) - Per Job Monitoring Facility
*    @todo add description
*
*    SGE_ULONG(CE_requestable) - @todo add summary
*    Defines if a complex variable can be requested and if it is a forced variable (must be requested)
*    Defined in libs/sgeobj/sge_centry.h, possible values are
*    REQU_NO
*    REQU_YES
*    REQU_FORCED
*
*    SGE_ULONG(CE_tagged) - Variable Is Tagged
*    Used for tagging variables, e.g. during the scheduling process.
*
*    SGE_STRING(CE_urgency_weight) - Urgency Weighting Factor
*    Static Urgency Weighting Factor.
*
*    SGE_LIST(CE_resource_map_list) - Resource Map List
*    on host level only: RSMAP id list
*
*/

enum {
   CE_name = CE_LOWERBOUND,
   CE_shortcut,
   CE_valtype,
   CE_stringval,
   CE_doubleval,
   CE_relop,
   CE_consumable,
   CE_defaultval,
   CE_dominant,
   CE_pj_stringval,
   CE_pj_doubleval,
   CE_pj_dominant,
   CE_requestable,
   CE_tagged,
   CE_urgency_weight,
   CE_resource_map_list
};

LISTDEF(CE_Type)
   SGE_STRING(CE_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_STRING(CE_shortcut, CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_ULONG(CE_valtype, CULL_SPOOL)
   SGE_STRING(CE_stringval, CULL_SPOOL | CULL_SUBLIST)
   SGE_DOUBLE(CE_doubleval, CULL_DEFAULT)
   SGE_ULONG(CE_relop, CULL_SPOOL)
   SGE_ULONG(CE_consumable, CULL_SPOOL)
   SGE_STRING(CE_defaultval, CULL_SPOOL)
   SGE_ULONG(CE_dominant, CULL_DEFAULT)
   SGE_STRING(CE_pj_stringval, CULL_DEFAULT)
   SGE_DOUBLE(CE_pj_doubleval, CULL_DEFAULT)
   SGE_ULONG(CE_pj_dominant, CULL_DEFAULT)
   SGE_ULONG(CE_requestable, CULL_SPOOL)
   SGE_ULONG(CE_tagged, CULL_DEFAULT)
   SGE_STRING(CE_urgency_weight, CULL_SPOOL)
   SGE_LIST(CE_resource_map_list, RESL_Type, CULL_SPOOL)
LISTEND

NAMEDEF(CEN)
   NAME("CE_name")
   NAME("CE_shortcut")
   NAME("CE_valtype")
   NAME("CE_stringval")
   NAME("CE_doubleval")
   NAME("CE_relop")
   NAME("CE_consumable")
   NAME("CE_defaultval")
   NAME("CE_dominant")
   NAME("CE_pj_stringval")
   NAME("CE_pj_doubleval")
   NAME("CE_pj_dominant")
   NAME("CE_requestable")
   NAME("CE_tagged")
   NAME("CE_urgency_weight")
   NAME("CE_resource_map_list")
NAMEEND

#define CE_SIZE sizeof(CEN)/sizeof(char *)


