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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Cluster queues, and the queue instances they expand into
 */

#include "sge_c_gdi.h"
#include "uti/sge_monitor.h"
#include "sgeobj/sge_daemonize.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/sge_answer.h"

bool
cqueue_mod_qinstances(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::CQ> cqueue,
                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *reduced_elem,
                      bool refresh_all_values, bool is_startup,
                      monitoring_t *monitor, ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                      ocs::tom::List<ocs::tom::CQ> master_cqueue_list);

bool
cqueue_handle_qinstances(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::CQ> cqueue,
                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *reduced_elem,
                         ocs::tom::ConstList<ocs::tom::HR> add_hosts, ocs::tom::ConstList<ocs::tom::HR> rem_hosts,
                         bool refresh_all_values, monitoring_t *monitor,
                         ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                         ocs::tom::List<ocs::tom::CQ> master_cqueue_list);

void
cqueue_commit(ocs::tom::Element<ocs::tom::CQ> cqueue, uint64_t gdi_session);

/** @brief Discard the pending changes of a cluster queue
 *
 * Counterpart of #cqueue_commit for the failure path of a queue modification.
 *
 * @note This function is declared here but defined nowhere in the source tree,
 *       and nothing calls it. The rollback of a failed queue modification is
 *       done by discarding the working copy of the queue instead.
 *
 * @param cqueue the cluster queue (`CQ_Type`) whose changes are discarded
 */
void
cqueue_rollback(lListElem *cqueue);

int
cqueue_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList,
               monitoring_t *monitor);

int
cqueue_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *modp,
           lListElem *ep, int add, const char *ruser,
           const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor);

int
cqueue_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
             lListElem *this_elem, gdi_object_t *object);

int
cqueue_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
           ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost);

bool
cqueue_del_all_orphaned(ocs::tom::Element<ocs::tom::CQ> this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const char *ehname, uint64_t gdi_session);

bool
cqueue_list_del_all_orphaned(ocs::tom::List<ocs::tom::CQ> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                             const char *cqname, const char *ehname, uint64_t gdi_session);

void
cqueue_list_set_unknown_state(ocs::tom::List<ocs::tom::CQ> this_list, const char *hostname,
                              bool send_events, bool is_unknown, uint64_t gdi_session);

void cqueue_diff_projects(ocs::tom::ConstElement<ocs::tom::CQ> new_cqueue,
                          ocs::tom::ConstElement<ocs::tom::CQ> old_cqueue, lList **new_prj, lList **old_prj);

void cqueue_diff_usersets(ocs::tom::ConstElement<ocs::tom::CQ> new_cqueue,
                          ocs::tom::ConstElement<ocs::tom::CQ> old_cqueue, lList **new_acl, lList **old_acl);
