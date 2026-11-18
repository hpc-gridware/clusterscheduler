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
 * @brief Advance reservations: booking resources ahead of the jobs that will use them
 */

#include "uti/sge_monitor.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"

#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_ST.h"

#include "sgeobj/sge_daemonize.h"

#include "sge_qmaster_timed_event.h"
#include "sge_c_gdi.h"
#include "sgeobj/sge_answer.h"

/* funtions called from within gdi framework in qmaster, the signatures are fixed by the gdi_object_t table */
int
ar_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *new_ar_ep,
       lListElem *ar_ep, int add, const char *ruser, const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd,
       ocs::gdi::SubCommand sub_command, monitoring_t *monitor);

int
ar_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *pep,
         gdi_object_t *object);

int
ar_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object,
           lList **ppList, monitoring_t *monitor);

/* funtions called via gdi and inside the qmaster */
int
ar_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ConstElement<ocs::tom::ID> ep,
       ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::List<ocs::tom::AR> master_ar_list, monitoring_t *monitor);

void
sge_store_ar_id(te_event_t anEvent, monitoring_t *monitor);

void
sge_init_ar_id();

int
ar_do_reservation(ocs::tom::Element<ocs::tom::AR> ar, bool incslots, uint64_t gdi_session);

void
ar_initialize_resource_booking(ocs::tom::Element<ocs::tom::AR> ar);

void
sge_ar_event_handler(te_event_t anEvent, monitoring_t *monitor);

bool
ar_list_has_reservation_due_to_ckpt(ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    const char *qinstance_name, ocs::tom::ConstList<ocs::tom::ST> ckpt_string_list);

bool
ar_list_has_reservation_due_to_pe(ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                  ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                  const char *qinstance_name, ocs::tom::ConstList<ocs::tom::ST> pe_string_list);

bool
ar_list_has_reservation_for_pe_with_slots(ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                          const char *pe_name, uint32_t new_slots);

bool
sge_ar_remove_all_jobs(uint32_t ar_id, int forced, monitoring_t *monitor, uint64_t gdi_session);

bool
sge_ar_list_conflicts_with_calendar(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *qinstance_name,
                                    ocs::tom::ConstElement<ocs::tom::CAL> cal_ep,
                                    ocs::tom::ConstList<ocs::tom::AR> master_ar_list);

void
sge_ar_state_set_running(ocs::tom::Element<ocs::tom::AR> ar);

void
sge_ar_state_set_waiting(ocs::tom::Element<ocs::tom::AR> ar);

void
sge_ar_state_set_deleted(ocs::tom::Element<ocs::tom::AR> ar);

void
sge_ar_state_set_exited(ocs::tom::Element<ocs::tom::AR> ar);

void
sge_ar_list_set_error_state(ocs::tom::List<ocs::tom::AR> ar_list, const char *qname, uint32_t error_type,
                            bool set_error, uint64_t gdi_session);

bool
ar_list_has_reservation_due_to_qinstance_complex_attr(ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                      ocs::tom::Element<ocs::tom::QU> qinstance,
                                                      ocs::tom::ConstList<ocs::tom::CE> ce_master_list);

bool
ar_list_has_reservation_due_to_host_complex_attr(ocs::tom::ConstList<ocs::tom::AR> ar_master_list,
                                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                 ocs::tom::ConstElement<ocs::tom::EH> host,
                                                 ocs::tom::ConstList<ocs::tom::CE> ce_master_list);

void
ar_initialize_timer(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, monitoring_t *monitor, uint64_t gdi_session);
