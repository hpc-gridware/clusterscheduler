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

bool
qinstance_validate(lListElem *this_elem, lList **answer_list, const lList *master_exechost_list,
                   const lList *centry_master_list);

bool
qinstance_list_validate(lList *this_list, lList **answer_list, const lList *master_exechost_list,
                        const lList *centry_master_list);

void
qinstance_set_full_name(lListElem *this_elem);

lListElem *
qinstance_list_locate(const lList *this_list, const char *hostname,
                      const char *cqueue_name);

lListElem *
qinstance_list_locate2(const lList *qinstance_list, const char *full_name);

const char *
qinstance_get_name(const lListElem *this_elem, dstring *string_buffer);

void
qinstance_list_set_tag(lList *this_list, uint32_t tag_value, int tag_nm = QU_tag);

void
qinstance_increase_qversion(lListElem *this_elem);

bool
qinstance_is_owner(const ocs::gdi::Packet *packet, const lListElem *queue);

bool
qinstance_is_pe_referenced(const lListElem *this_elem,
                           const lListElem *pe);

bool
qinstance_is_a_pe_referenced(const lListElem *this_elem);

bool
qinstance_is_ckpt_referenced(const lListElem *this_elem,
                             const lListElem *ckpt);

bool
qinstance_is_a_ckpt_referenced(const lListElem *this_elem);

bool
qinstance_is_centry_a_complex_value(const lListElem *this_elem,
                                    const lListElem *name);

void
qinstance_set_slots_used(lListElem *this_elem, int new_slots);

int
qinstance_slots_used(const lListElem *this_elem);

uint32_t
qinstance_slots_reserved(const lListElem *this_elem);

void
qinstance_set_conf_slots_used(lListElem *this_elem);

bool
qinstance_is_calendar_referenced(const lListElem *this_elem,
                                 const lListElem *calendar);

int
qinstance_debit_consumable(lListElem *this_elem, const lListElem *job, const lListElem *pe, const lList *centry_list,
                           int slots, bool is_master_task, bool do_per_host_booking, bool *just_check);

bool
qinstance_message_add(lListElem *this_elem, uint32_t type, const char *message);

bool
qinstance_message_trash_all_of_type_X(lListElem *this_elem, uint32_t type);

/**
 * @brief Do all queues named in a reference list exist?
 *
 * @param[out] alpp receives the name of the first queue that does not exist
 * @param qr_list the queue references to check
 * @param attr_name the attribute the list belongs to, used in the message
 * @param obj_descr the kind of object holding the list, used in the message
 * @param obj_name the object's name, used in the message
 * @return 0 when every referenced queue exists
 *
 * @todo Rename from `queue` to `qinstance`.
 *
 * @warning Declared here but defined nowhere in the tree, and never called.
 */
int queue_reference_list_validate(lList **alpp, lList *qr_list,
                                  const char *attr_name, const char *obj_descr,
                                  const char *obj_name);

int
rc_debit_consumable(const lListElem *jep, const lListElem *pe, lListElem *ep, const lList *centry_list, int slots,
                    int config_nm, int actual_nm, const char *obj_name, bool is_master_task,
                    bool do_per_host_booking, bool *just_check);

//lListElem *
//explicit_job_request(lListElem *jep, const char *name);

bool
qinstance_list_find_matching(const lList *this_list, lList **answer_list,
                             const char *hostname_pattern, lList **qref_list);

bool
qinstance_list_verify_execd_job(const lList *queue_list, lList **answer_list);

bool
qinstance_verify(const lListElem *qep, lList **answer_list);

bool
qinstance_verify_full_name(lList **answer_list, const char *full_name);

void
qinstance_set_error(lListElem *qinstance, uint32_t type, const char *message, bool set_error);

