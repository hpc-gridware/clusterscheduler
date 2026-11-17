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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 *
 *   Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations and state constants of the queue instance object
 *
 * @see sge_qinstance.cc
 */

#include "uti/sge_dstring.h"

#include "gdi/ocs_gdi_Packet.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_QU.h"

#include "sgeobj/cull/sge_qinstance_QU_L.h"

/**
 * @brief Values for `QU_qtype`
 *
 * Only these two are stored. A queue also counts as parallel or
 * checkpointing, but that is derived from whether it references a PE or a
 * checkpointing environment rather than stored as a bit.
 */
enum {
   BQ = 0x01,                ///< batch queue
   IQ = 0x02                 ///< interactive queue
};

/// Flags for queue instance modification requests
enum {
   GDI_DO_LATER = 0x01       ///< apply the change when the queue next becomes idle, not now
};

bool qinstance_validate(ocs::tom::Element<ocs::tom::QU> this_elem, lList **answer_list,
                        ocs::tom::ConstList<ocs::tom::EH> master_exechost_list,
                        ocs::tom::ConstList<ocs::tom::CE> centry_master_list);
OCS_TRANSITIONAL bool qinstance_validate(lListElem *this_elem, lList **answer_list, const lList *master_exechost_list,
                                         const lList *centry_master_list);

bool qinstance_list_validate(ocs::tom::List<ocs::tom::QU> this_list, lList **answer_list,
                             ocs::tom::ConstList<ocs::tom::EH> master_exechost_list,
                             ocs::tom::ConstList<ocs::tom::CE> centry_master_list);
OCS_TRANSITIONAL bool qinstance_list_validate(lList *this_list, lList **answer_list, const lList *master_exechost_list,
                                              const lList *centry_master_list);

void qinstance_set_full_name(ocs::tom::Element<ocs::tom::QU> this_elem);
OCS_TRANSITIONAL void qinstance_set_full_name(lListElem *this_elem);

ocs::tom::Element<ocs::tom::QU> qinstance_list_locate(ocs::tom::ConstList<ocs::tom::QU> this_list, const char *hostname,
                                                      const char *cqueue_name);
OCS_TRANSITIONAL lListElem *qinstance_list_locate(const lList *this_list, const char *hostname,
                                                   const char *cqueue_name);

ocs::tom::Element<ocs::tom::QU> qinstance_list_locate2(ocs::tom::ConstList<ocs::tom::QU> qinstance_list,
                                                       const char *full_name);

const char *qinstance_get_name(ocs::tom::ConstElement<ocs::tom::QU> this_elem, dstring *string_buffer);
OCS_TRANSITIONAL const char *qinstance_get_name(const lListElem *this_elem, dstring *string_buffer);

void qinstance_list_set_tag(ocs::tom::List<ocs::tom::QU> this_list, uint32_t tag_value, int tag_nm = QU_tag);

void qinstance_increase_qversion(ocs::tom::Element<ocs::tom::QU> this_elem);
OCS_TRANSITIONAL void qinstance_increase_qversion(lListElem *this_elem);

bool qinstance_is_owner(const ocs::gdi::Packet *packet, ocs::tom::ConstElement<ocs::tom::QU> this_elem);
OCS_TRANSITIONAL bool qinstance_is_owner(const ocs::gdi::Packet *packet, const lListElem *this_elem);

bool qinstance_is_pe_referenced(ocs::tom::ConstElement<ocs::tom::QU> this_elem,
                                ocs::tom::ConstElement<ocs::tom::PE> pe);

bool qinstance_is_a_pe_referenced(ocs::tom::ConstElement<ocs::tom::QU> this_elem);

bool qinstance_is_ckpt_referenced(ocs::tom::ConstElement<ocs::tom::QU> this_elem,
                                  ocs::tom::ConstElement<ocs::tom::CK> ckpt);

bool qinstance_is_a_ckpt_referenced(ocs::tom::ConstElement<ocs::tom::QU> this_elem);

bool qinstance_is_centry_a_complex_value(ocs::tom::ConstElement<ocs::tom::QU> this_elem,
                                         ocs::tom::ConstElement<ocs::tom::CE> centry);

void qinstance_set_slots_used(ocs::tom::Element<ocs::tom::QU> this_elem, int new_slots);
OCS_TRANSITIONAL void qinstance_set_slots_used(lListElem *this_elem, int new_slots);

int qinstance_slots_used(ocs::tom::ConstElement<ocs::tom::QU> this_elem);
OCS_TRANSITIONAL int qinstance_slots_used(const lListElem *this_elem);

uint32_t qinstance_slots_reserved(ocs::tom::ConstElement<ocs::tom::QU> this_elem);
OCS_TRANSITIONAL uint32_t qinstance_slots_reserved(const lListElem *this_elem);

void qinstance_set_conf_slots_used(ocs::tom::Element<ocs::tom::QU> this_elem);
OCS_TRANSITIONAL void qinstance_set_conf_slots_used(lListElem *this_elem);

int qinstance_debit_consumable(ocs::tom::Element<ocs::tom::QU> this_elem, ocs::tom::ConstElement<ocs::tom::JB> job,
                               ocs::tom::ConstElement<ocs::tom::PE> pe, ocs::tom::ConstList<ocs::tom::CE> centry_list,
                               int slots, bool is_master_task, bool do_per_host_booking, bool *just_check);
OCS_TRANSITIONAL int qinstance_debit_consumable(lListElem *this_elem, const lListElem *job, const lListElem *pe,
                                                const lList *centry_list, int slots, bool is_master_task,
                                                bool do_per_host_booking, bool *just_check);

bool qinstance_message_add(ocs::tom::Element<ocs::tom::QU> this_elem, uint32_t type, const char *message);
OCS_TRANSITIONAL bool qinstance_message_add(lListElem *this_elem, uint32_t type, const char *message);

bool qinstance_message_trash_all_of_type_X(ocs::tom::Element<ocs::tom::QU> this_elem, uint32_t type);
OCS_TRANSITIONAL bool qinstance_message_trash_all_of_type_X(lListElem *this_elem, uint32_t type);

int rc_debit_consumable(ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::PE> pe,
                        lListElem *ep, ocs::tom::ConstList<ocs::tom::CE> centry_list, int slots, int config_nm,
                        int actual_nm, const char *obj_name, bool is_master_task, bool do_per_host_booking,
                        bool *just_check);

//lListElem *
//explicit_job_request(lListElem *jep, const char *name);

[[nodiscard]] ocs::tom::ListPtr<ocs::tom::QR> qinstance_list_find_matching(ocs::tom::ConstList<ocs::tom::QU> this_list,
                                                                         lList **answer_list,
                                                                         const char *hostname_pattern);

bool qinstance_verify(ocs::tom::ConstElement<ocs::tom::QU> qep, lList **answer_list);
OCS_TRANSITIONAL bool qinstance_verify(const lListElem *qep, lList **answer_list);

bool
qinstance_verify_full_name(lList **answer_list, const char *full_name);

void qinstance_set_error(ocs::tom::Element<ocs::tom::QU> qinstance, uint32_t type, const char *message,
                         bool set_error);
OCS_TRANSITIONAL void qinstance_set_error(lListElem *qinstance, uint32_t type, const char *message, bool set_error);

