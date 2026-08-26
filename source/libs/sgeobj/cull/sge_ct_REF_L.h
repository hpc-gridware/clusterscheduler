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
 * This code was generated from file source/libs/sgeobj/json/REF.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Generic Reference (unused)
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Generic Reference (unused)
*
* A bare pointer to another element, for building temporary reference lists.
* @warning No attribute of this type is read or written anywhere in either repository, and `REF_Type` itself is referenced nowhere. Left in place because removing a type changes the spooled object model.
*
*    SGE_REF(REF_ref) - Referenced Element
*    Part of the unused REF object; see the object description.
*
*/

enum {
   REF_ref = REF_LOWERBOUND   ///< Referenced Element
};

LISTDEF(REF_Type)
   SGE_REF(REF_ref, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
LISTEND

NAMEDEF(REFN)
   NAME("REF_ref")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define REF_SIZE sizeof(REFN)/sizeof(char *)


