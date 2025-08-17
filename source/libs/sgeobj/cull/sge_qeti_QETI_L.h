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
 * This code was generated from file source/libs/sgeobj/json/QETI.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_DOUBLE(QETI_total) - @todo add summary
*    @todo add description
*
*    SGE_REF(QETI_resource_instance) - @todo add summary
*    @todo add description
*
*    SGE_REF(QETI_queue_end_next) - @todo add summary
*    @todo add description
*
*/

enum {
   QETI_total = QETI_LOWERBOUND,
   QETI_resource_instance,
   QETI_queue_end_next
};

LISTDEF(QETI_Type)
   SGE_DOUBLE(QETI_total, CULL_DEFAULT)
   SGE_REF(QETI_resource_instance, RUE_Type, CULL_DEFAULT)
   SGE_REF(QETI_queue_end_next, RDE_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(QETIN)
   NAME("QETI_total")
   NAME("QETI_resource_instance")
   NAME("QETI_queue_end_next")
NAMEEND

#define QETI_SIZE sizeof(QETIN)/sizeof(char *)


