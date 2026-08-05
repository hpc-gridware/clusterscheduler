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
 * This code was generated from file source/libs/sgeobj/json/HGRP.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   HGRP_name = 7150,
   HGRP_host_list,
   HGRP_cqueue_list,
   HGRP_joker,
   HGRP_cached_hosts,
   HGRP_cache_version
};

constexpr const int HGRP_Type[] = {
   HGRP_name,
   HGRP_host_list,
   HGRP_cqueue_list,
   HGRP_joker,
   HGRP_cached_hosts,
   HGRP_cache_version,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define HGRP_ATTRIBUTES \
   {HGRP_name, "HGRP_name", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {HGRP_host_list, "HGRP_host_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {HGRP_cqueue_list, "HGRP_cqueue_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {HGRP_joker, "HGRP_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {HGRP_cached_hosts, "HGRP_cached_hosts", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {HGRP_cache_version, "HGRP_cache_version", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

