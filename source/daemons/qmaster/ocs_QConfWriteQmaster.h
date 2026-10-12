#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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
 * @brief The writing half of the qconf write procedure (CS-2822)
 *
 * The procedure library holds the model, the parameters and the view; what it
 * cannot hold is the write itself, because `sge_gdi_add_mod_generic()` is the
 * daemon's and the library is linked into the clients too. So this function
 * lives here and is handed over at startup - see ocs::set_qconf_write_provider().
 */

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"
#include "procedure/qconf/ocs_QConfWriteServer.h"
#include "uti/sge_monitor.h"

/** @brief Apply a set of objects, parsing each one first
 *
 * The ocs::qconf_write_provider of this daemon. Two phases, in this order and
 * for one reason: `-strict` means "one item that cannot be read rejects the
 * batch", and that verdict can only be reached once every item has been read.
 * Applying as it goes would make the mode a lie, since an object already
 * written cannot be taken back.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param obj which object type is written
 * @param parameter the items and the batch policy
 * @param answer_list receives one entry per item, plus whatever the writes produced
 * @param result receives how many items were applied and how many could not be read
 * @return true when the batch was applied as the policy demands
 */
bool
qconf_write_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                           QConfObj obj, const ocs::QConfWriteParameter &parameter,
                           lList **answer_list, ocs::QConfWriteResult *result);
