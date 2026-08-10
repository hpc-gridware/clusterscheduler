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
 * This code was generated from file source/libs/sgeobj/json/QETI.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Queue End Time Iterator
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Queue End Time Iterator
*
* Walks the points in time at which a resource frees up, newest first.
* Reservation scheduling has to answer "when could this job start". Rather than sampling time, the iterator visits only the instants at which some booking ends, because the answer can only change there.
*
*    SGE_DOUBLE(QETI_total) - Total Capacity
*    The resource's full capacity, the value utilization is compared against.
*
*    SGE_REF(QETI_resource_instance) - Resource Instance
*    Reference to the resource being walked (`RUE_Type`), whose utilization diagram holds the bookings.
*
*    SGE_REF(QETI_queue_end_next) - Next Position
*    Cursor into that utilization diagram: the next booking end this iterator will report.
*
*/

enum {
   QETI_total = QETI_LOWERBOUND,   ///< Total Capacity
   QETI_resource_instance,   ///< Resource Instance
   QETI_queue_end_next   ///< Next Position
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

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define QETI_SIZE sizeof(QETIN)/sizeof(char *)


