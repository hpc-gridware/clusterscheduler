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
 * This code was generated from file source/libs/sgeobj/json/TE.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   TE_when = 5950,
   TE_type,
   TE_mode,
   TE_interval,
   TE_uval0,
   TE_uval1,
   TE_sval,
   TE_seqno
};

constexpr const int TE_Type[] = {
   TE_when,
   TE_type,
   TE_mode,
   TE_interval,
   TE_uval0,
   TE_uval1,
   TE_sval,
   TE_seqno,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define TE_ATTRIBUTES \
   {TE_when, "TE_when", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {TE_type, "TE_type", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TE_mode, "TE_mode", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TE_interval, "TE_interval", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {TE_uval0, "TE_uval0", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TE_uval1, "TE_uval1", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TE_sval, "TE_sval", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {TE_seqno, "TE_seqno", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

