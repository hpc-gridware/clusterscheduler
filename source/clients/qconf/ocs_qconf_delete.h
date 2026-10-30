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
 * @brief The delete switches of `qconf` (CS-2972)
 *
 * Deleting by name, by name list and by the objects a file or directory
 * names, and the table that maps the switches onto it.
 */

#include <ostream>

#include "ocs_qconf_common.h"

/**
 * @brief Run one delete operation from qconf_delete_table, if it is one
 *
 * Makes the row's client-side check (which exits the client on a refusal, as
 * the blocks did), then calls the engine with the operation's argument -
 * "-dstree" has none and passes nullptr, as its block did. Either engine
 * reporting a failure fails the operation.
 *
 * @param os       the stream the output is written to
 * @param op       the operation to run
 * @param username the calling user, for the client-side check
 * @param hostname the calling host, for the client-side check
 * @return what became of the operation
 */
QConfOpResult
qconf_run_delete(std::ostream &os, const qconf_op &op, const char *username,
                 const char *hostname);
