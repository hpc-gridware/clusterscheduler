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
 * This code was generated from file source/libs/sgeobj/json/CK.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   CK_name = 5350,
   CK_interface,
   CK_ckpt_command,
   CK_migr_command,
   CK_rest_command,
   CK_ckpt_dir,
   CK_when,
   CK_signal,
   CK_job_pid,
   CK_clean_command,
   CK_joker
};

constexpr const int CK_Type[] = {
   CK_name,
   CK_interface,
   CK_ckpt_command,
   CK_migr_command,
   CK_rest_command,
   CK_ckpt_dir,
   CK_when,
   CK_signal,
   CK_job_pid,
   CK_clean_command,
   CK_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define CK_ATTRIBUTES \
   {CK_name, "CK_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {CK_interface, "CK_interface", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CK_ckpt_command, "CK_ckpt_command", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CK_migr_command, "CK_migr_command", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CK_rest_command, "CK_rest_command", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CK_ckpt_dir, "CK_ckpt_dir", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CK_when, "CK_when", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CK_signal, "CK_signal", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CK_job_pid, "CK_job_pid", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CK_clean_command, "CK_clean_command", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CK_joker, "CK_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

