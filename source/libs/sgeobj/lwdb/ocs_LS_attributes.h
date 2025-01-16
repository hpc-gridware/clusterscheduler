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
 * This code was generated from file source/libs/sgeobj/json/LS.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   LS_name = 7650,
   LS_command,
   LS_pid,
   LS_in,
   LS_out,
   LS_err,
   LS_has_to_restart,
   LS_tag,
   LS_incomplete,
   LS_complete,
   LS_last_mod
};

constexpr const int LS_Type[] = {
   LS_name,
   LS_command,
   LS_pid,
   LS_in,
   LS_out,
   LS_err,
   LS_has_to_restart,
   LS_tag,
   LS_incomplete,
   LS_complete,
   LS_last_mod,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define LS_ATTRIBUTES \
   {LS_name, "LS_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {LS_command, "LS_command", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {LS_pid, "LS_pid", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {LS_in, "LS_in", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {LS_out, "LS_out", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {LS_err, "LS_err", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {LS_has_to_restart, "LS_has_to_restart", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {LS_tag, "LS_tag", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {LS_incomplete, "LS_incomplete", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {LS_complete, "LS_complete", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {LS_last_mod, "LS_last_mod", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

