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
 * @brief The `qconf -clearusage` action (CS-2963)
 *
 * Lives here for the reason every action does: the change goes through
 * `sge_gdi_add_mod_generic()`, which is the daemon's. Registered as the action
 * ocs::QConfActionParameter::USAGE_CLEAR at startup.
 */

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "procedure/qconf/action/ocs_QConfActionServer.h"
#include "uti/sge_monitor.h"

/** @brief Clear the share tree usage of every user and every project
 *
 * The ocs::qconf_action_provider of the action USAGE_CLEAR.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param parameter the action; it takes no arguments
 * @param answer_list receives the reason of every write that failed
 * @return true when the usage of every user and project was cleared
 */
bool
qconf_usage_clear_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                                 const ocs::QConfActionParameter &parameter, lList **answer_list);
