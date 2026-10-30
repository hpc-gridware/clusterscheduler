#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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

/** @file
 * @brief Localisable message catalogue of what several procedures share (CS-2970)
 */

#include <cinttypes>

// clang-format off

#define MSG_COMMON_format_OPT_USAGE                   _MESSAGE(13000, _("specifies the display output format"))
#define MSG_COMMON_exec_ctx_OPT_USAGE                 _MESSAGE(13001, _("specifies the execution context for the command"))

// clang-format on
