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
 * @brief The action switches of `qconf` (CS-2972)
 *
 * The switches that change part of an object in qmaster through the action
 * stored procedure: purge, membership, share tree nodes, clear usage.
 */

#include "procedure/qconf/ocs_QConfParameterClient.h"
#include "ocs_qconf_common.h"

/**
 * @brief Run one operation from qconf_action_table, if it is one
 *
 * In the order the blocks had: a short command line is reported, the
 * arguments are built (which may refuse the
 * command line with a message of its own), the client execution context is
 * declined - an action is a write - and the action runs in qmaster. What it has
 * to say comes back in the answer list and is surfaced the way every qconf
 * write surfaces it; a warning is not a failure.
 *
 * @param param    the invocation, for its execution context
 * @param op       the operation to run
 * @param alpp     the answer list of the operation, receives every message (CS-2971)
 * @return what became of the operation
 */
QConfOpResult
qconf_run_action(const ocs::QConfParameterClient &param, const qconf_op &op, lList **alpp);
