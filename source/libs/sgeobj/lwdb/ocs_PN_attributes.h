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
 * This code was generated from file source/libs/sgeobj/json/PN.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   PN_path = 2050,
   PN_host,
   PN_file_host,
   PN_file_staging
};

constexpr const int PN_Type[] = {
   PN_path,
   PN_host,
   PN_file_host,
   PN_file_staging,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define PN_ATTRIBUTES \
   {PN_path, "PN_path", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {PN_host, "PN_host", AttributeStatic::HOST, AttributeStatic::NO_HASH}, \
   {PN_file_host, "PN_file_host", AttributeStatic::HOST, AttributeStatic::NO_HASH}, \
   {PN_file_staging, "PN_file_staging", AttributeStatic::BOOL, AttributeStatic::NO_HASH} \

} // end namespace

