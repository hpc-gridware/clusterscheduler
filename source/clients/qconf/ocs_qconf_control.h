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
 * @brief The control switches of `qconf` (CS-2986)
 *
 * Shutting down daemons and event clients, starting and stopping qmaster
 * threads, cleaning queues and triggering a scheduler monitoring run. They
 * change no object data, so they stay GDI requests rather than procedures.
 */

#include "ocs_qconf_common.h"

/**
 * @brief Run one operation from qconf_control_table, if it is one
 *
 * @param op   the operation to run
 * @param alpp the answer list of the operation, receives every message
 * @return what became of the operation
 */
QConfOpResult
qconf_run_control(const qconf_op &op, lList **alpp);
