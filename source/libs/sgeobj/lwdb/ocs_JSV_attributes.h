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
 * This code was generated from file source/libs/sgeobj/json/JSV.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   JSV_name = 12800,
   JSV_context,
   JSV_url,
   JSV_type,
   JSV_user,
   JSV_command,
   JSV_pid,
   JSV_in,
   JSV_out,
   JSV_err,
   JSV_has_to_restart,
   JSV_last_mod,
   JSV_send_env,
   JSV_old_job,
   JSV_new_job,
   JSV_restart,
   JSV_accept,
   JSV_done,
   JSV_soft_shutdown,
   JSV_test,
   JSV_test_pos,
   JSV_result
};

constexpr const int JSV_Type[] = {
   JSV_name,
   JSV_context,
   JSV_url,
   JSV_type,
   JSV_user,
   JSV_command,
   JSV_pid,
   JSV_in,
   JSV_out,
   JSV_err,
   JSV_has_to_restart,
   JSV_last_mod,
   JSV_send_env,
   JSV_old_job,
   JSV_new_job,
   JSV_restart,
   JSV_accept,
   JSV_done,
   JSV_soft_shutdown,
   JSV_test,
   JSV_test_pos,
   JSV_result,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define JSV_ATTRIBUTES \
   {JSV_name, "JSV_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JSV_context, "JSV_context", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JSV_url, "JSV_url", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JSV_type, "JSV_type", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JSV_user, "JSV_user", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JSV_command, "JSV_command", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JSV_pid, "JSV_pid", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JSV_in, "JSV_in", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {JSV_out, "JSV_out", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {JSV_err, "JSV_err", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {JSV_has_to_restart, "JSV_has_to_restart", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JSV_last_mod, "JSV_last_mod", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JSV_send_env, "JSV_send_env", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JSV_old_job, "JSV_old_job", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {JSV_new_job, "JSV_new_job", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {JSV_restart, "JSV_restart", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JSV_accept, "JSV_accept", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JSV_done, "JSV_done", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JSV_soft_shutdown, "JSV_soft_shutdown", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JSV_test, "JSV_test", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JSV_test_pos, "JSV_test_pos", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JSV_result, "JSV_result", AttributeStatic::STRING, AttributeStatic::NO_HASH} \

} // end namespace

