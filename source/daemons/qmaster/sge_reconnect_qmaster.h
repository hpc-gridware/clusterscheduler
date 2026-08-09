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
 * @brief Brokering a client's reconnect to a running interactive job
 */

#include <sys/types.h>
#include <cstddef>
#include <cstdint>

#include "cull/cull_list.h"

int qmaster_handle_reconnect_request(uint32_t job_id,
                                     const char *requester_user,
                                     const char *client_host,
                                     int client_port,
                                     const char *client_cred,
                                     char *out_token, size_t out_token_size,
                                     char *out_exec_host, size_t out_exec_host_size,
                                     lList **answer_list);

int sge_qmaster_send_reconnect_prepare(uint32_t job_id, uint32_t ja_task_id,
                                       const char *exec_host,
                                       const char *client_host, int client_port,
                                       const char *token,
                                       uid_t owner_uid, gid_t owner_gid,
                                       const char *client_cred);
