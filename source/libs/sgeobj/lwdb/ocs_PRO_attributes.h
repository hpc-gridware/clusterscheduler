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
 * This code was generated from file source/libs/sgeobj/json/PRO.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   PRO_pid = 13000,
   PRO_utime,
   PRO_stime,
   PRO_vsize,
   PRO_rss,
   PRO_groups,
   PRO_rel,
   PRO_run,
   PRO_io
};

constexpr const int PRO_Type[] = {
   PRO_pid,
   PRO_utime,
   PRO_stime,
   PRO_vsize,
   PRO_rss,
   PRO_groups,
   PRO_rel,
   PRO_run,
   PRO_io,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define PRO_ATTRIBUTES \
   {PRO_pid, "PRO_pid", AttributeStatic::UINT32, AttributeStatic::UNORDERED_UNIQUE}, \
   {PRO_utime, "PRO_utime", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {PRO_stime, "PRO_stime", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {PRO_vsize, "PRO_vsize", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {PRO_rss, "PRO_rss", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {PRO_groups, "PRO_groups", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {PRO_rel, "PRO_rel", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PRO_run, "PRO_run", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PRO_io, "PRO_io", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

