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

#include "sge_c_gdi.h"
#include "uti/sge_monitor.h"

int
role_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *new_ep, lListElem *ep, int add,
         const char *ruser, const char *rhost, gdi_object_t *object,
         ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
         monitoring_t *monitor);

int
role_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *ep, gdi_object_t *object);

int
role_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object,
             lList **ppList, monitoring_t *monitor);

int
sge_del_role(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **alpp, char *ruser, char *rhost);
