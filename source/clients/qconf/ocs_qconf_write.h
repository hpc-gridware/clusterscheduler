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
 * @brief The add and modify switches of `qconf` (CS-2972)
 *
 * The write stored procedure - from a file, a directory or the editor -
 * and the table that maps the switches onto it.
 */

#include <ostream>
#include <sys/types.h>

#include "procedure/qconf/ocs_QConfParameterClient.h"
#include "ocs_qconf_common.h"

/**
 * @brief Run one add or modify operation from qconf_write_table, if it is one
 *
 * Makes the row's client-side check (which exits the client on a refusal, as
 * the blocks did), then calls the engine with the operation's argument, or for
 * an EDIT row without one, with the row's default name. Either engine failing
 * fails the operation.
 *
 * @param os       the stream the output is written to
 * @param param    the invocation, for the procedure read of an EDIT row
 * @param op       the operation to run
 * @param username the calling user, for the client-side check
 * @param hostname the calling host, for the client-side check
 * @param uid      uid to run the editor as
 * @param gid      gid to run the editor as
 * @return what became of the operation
 */
QConfOpResult
qconf_run_write(std::ostream &os, ocs::QConfParameterClient &param, const qconf_op &op,
                const char *username, const char *hostname, uid_t uid, gid_t gid);
