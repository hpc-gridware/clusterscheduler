#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief TODO describe this file
 */

#include "sge_c_gdi.h"
#include "uti/sge_monitor.h"
#include "sgeobj/sge_daemonize.h"

int
hgroup_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList, monitoring_t *monitor);

/* CS-2451: refresh the resolved-host cache of a group and of every group that
 * references it, then let the caller send one HGROUP_MOD per referencee. Used by
 * hgroup_success() and by the @exec_hosts sync in sge_host_qmaster.cc (CS-2438
 * chunk 7), which changes a host group without going through the GDI path. */
void
hgroup_refresh_caches(lListElem *hgroup, lList *master_hgroup_list, lList **referencees);

void
hgroup_send_referencee_events(const lList *referencees, lList *master_hgroup_list, uint64_t gdi_session);

/* CS-2438 chunk 7: install the queue copies a membership change produced, and
 * discard them if the change is abandoned. Shared with the @exec_hosts sync. */
void
hgroup_commit(lListElem *hgroup, uint64_t gdi_session);

void
hgroup_rollback_cqueues(lListElem *hgroup);

int
hgroup_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *modp, lListElem *ep, int add, const char *ruser, const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor);

int
hgroup_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *upe, gdi_object_t *object);

int
hgroup_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *cep, lList **alpp, char *ruser, char *rhost);
