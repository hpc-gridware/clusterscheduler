#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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
   {JSV_name, "JSV_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_context, "JSV_context", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_url, "JSV_url", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_type, "JSV_type", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_user, "JSV_user", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_command, "JSV_command", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_pid, "JSV_pid", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_in, "JSV_in", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_out, "JSV_out", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_err, "JSV_err", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_has_to_restart, "JSV_has_to_restart", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_last_mod, "JSV_last_mod", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_send_env, "JSV_send_env", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_old_job, "JSV_old_job", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_new_job, "JSV_new_job", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_restart, "JSV_restart", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_accept, "JSV_accept", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_done, "JSV_done", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_soft_shutdown, "JSV_soft_shutdown", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_test, "JSV_test", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_test_pos, "JSV_test_pos", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JSV_result, "JSV_result", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

