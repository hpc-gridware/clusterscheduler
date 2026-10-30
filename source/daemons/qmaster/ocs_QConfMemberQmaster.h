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
 * @brief The changing half of the access list membership procedure (CS-2959)
 *
 * Lives here for the reason the write provider does: the change goes through
 * `sge_gdi_add_mod_generic()`, which is the daemon's, and the procedure library
 * is linked into the clients too. Registered as the action
 * ocs::QConfActionParameter::USERSET_MEMBERS at startup (CS-2962).
 */

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "procedure/qconf/action/ocs_QConfActionServer.h"
#include "uti/sge_monitor.h"

/** @brief Add users to or remove them from access lists
 *
 * The ocs::qconf_action_provider of the action USERSET_MEMBERS.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param parameter the arguments ARG_USERS, ARG_LISTS and ARG_ADD
 * @param answer_list receives one answer per user and list
 * @return true when every user could be added or removed
 */
bool
qconf_member_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                            const ocs::QConfActionParameter &parameter, lList **answer_list);
