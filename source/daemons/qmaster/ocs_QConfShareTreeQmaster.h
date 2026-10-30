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
 * @brief The share tree node action of `qconf -astnode/-mstnode/-dstnode` (CS-2966)
 *
 * Lives here for the reason every action does: the tree is written through
 * `sge_mod_sharetree()`, which is the daemon's. Registered as the action
 * ocs::QConfActionParameter::SHARETREE_NODES at startup.
 */

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "procedure/qconf/action/ocs_QConfActionServer.h"
#include "uti/sge_monitor.h"

/** @brief Add, modify or delete share tree nodes
 *
 * The ocs::qconf_action_provider of the action SHARETREE_NODES.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param parameter the arguments ARG_NODES and ARG_OPERATION
 * @param answer_list receives one answer per line the client prints
 * @return true when at least one node was changed and the tree was written
 */
bool
qconf_sharetree_nodes_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                                     const ocs::QConfActionParameter &parameter, lList **answer_list);
