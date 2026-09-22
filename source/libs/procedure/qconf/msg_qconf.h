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
 * @brief Messages of the `qconf` stored procedure
 *
 * The ids stay in the `qconf` range: the code moved out of the client
 * (CS-2799), the messages did not change. `MSG_ROLE_DOESNOTEXIST_S` came from
 * `clients/qconf/msg_qconf.h`, where its six siblings for the other object
 * types never lived - those are in `common/msg_common.h`.
 */

#include <cinttypes>

// clang-format off

#define MSG_ROLE_DOESNOTEXIST_S                       _MESSAGE(5253, _(SFQ " is not a role"))

// clang-format on
