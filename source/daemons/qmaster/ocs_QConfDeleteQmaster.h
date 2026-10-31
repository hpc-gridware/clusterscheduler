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
 * @brief The `qconf` delete action (CS-2983)
 *
 * Lives here for the reason every action does: the delete goes through
 * `sge_c_gdi_del_element()`, which is the daemon's. Registered as the action
 * ocs::QConfActionParameter::OBJECT_DELETE at startup.
 */

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "procedure/qconf/action/ocs_QConfActionServer.h"
#include "uti/sge_monitor.h"

/** @brief Delete the named objects of the list the request is addressed to
 *
 * The ocs::qconf_action_provider of the action OBJECT_DELETE.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered; its target is the object list
 * @param monitor for monitoring qmaster threads
 * @param parameter the names, their files, and whether a missing name is skipped and whether it is a dry run
 * @param answer_list receives what each delete has to say
 * @return true when no delete failed
 */
bool
qconf_delete_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                            const ocs::QConfActionParameter &parameter, lList **answer_list);
