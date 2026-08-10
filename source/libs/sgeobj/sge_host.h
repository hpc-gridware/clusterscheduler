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
 * Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations of the host object and the standard load value names
 *
 * @see sge_host.cc
 */

#include <string>

#include "sgeobj/cull/sge_host_EH_L.h"
#include "sgeobj/cull/sge_host_RU_L.h"
#include "sgeobj/cull/sge_host_HL_L.h"
#include "sgeobj/cull/sge_host_HS_L.h"
#include "sgeobj/cull/sge_host_RESL_L.h"

/**
 * @name Standard load value names
 *
 * Use these names rather than the string literals: an execution host reports
 * its load under exactly these keys, and a load formula or a resource request
 * refers to the same key.
 * @{
 */
/// The load value reporting the host architecture, e.g. `lx-amd64`
#define LOAD_ATTR_ARCH           "arch"
/// The load value reporting how many processors the host has
#define LOAD_ATTR_NUM_PROC       "num_proc"

/// The load value reporting the raw one minute load average
#define LOAD_ATTR_LOAD_SHORT     "load_short"
/// The load value reporting the raw five minute load average
#define LOAD_ATTR_LOAD_MEDIUM    "load_medium"
/// The load value reporting the raw fifteen minute load average
#define LOAD_ATTR_LOAD_LONG      "load_long"
/// The load value reporting the raw load average the scheduler sorts by
#define LOAD_ATTR_LOAD_AVG       "load_avg"

/// The load value reporting the one minute load average divided by the processor count
#define LOAD_ATTR_NP_LOAD_SHORT  "np_load_short"
/// The load value reporting the five minute load average divided by the processor count
#define LOAD_ATTR_NP_LOAD_MEDIUM "np_load_medium"
/// The load value reporting the fifteen minute load average divided by the processor count
#define LOAD_ATTR_NP_LOAD_LONG   "np_load_long"
/// The load value reporting the load average divided by the processor count
#define LOAD_ATTR_NP_LOAD_AVG    "np_load_avg"
/// The load value reporting unused physical memory
#define LOAD_ATTR_MEM_FREE       "mem_free"
/// The load value reporting unused swap space
#define LOAD_ATTR_SWAP_FREE      "swap_free"
/// The load value reporting unused memory and swap together
#define LOAD_ATTR_VIRTUAL_FREE   "virtual_free"
/// The load value reporting installed physical memory
#define LOAD_ATTR_MEM_TOTAL      "mem_total"
/// The load value reporting configured swap space
#define LOAD_ATTR_SWAP_TOTAL     "swap_total"
/// The load value reporting physical memory and swap together
#define LOAD_ATTR_VIRTUAL_TOTAL  "virtual_total"
/// The load value reporting physical memory in use
#define LOAD_ATTR_MEM_USED       "mem_used"
/// The load value reporting swap space in use
#define LOAD_ATTR_SWAP_USED      "swap_used"
/// The load value reporting memory and swap in use together
#define LOAD_ATTR_VIRTUAL_USED   "virtual_used"
/// The load value reporting swap space reserved but not yet written
#define LOAD_ATTR_SWAP_RSVD      "swap_rsvd"

/// The load value reporting the host's CPU topology string; see @ref ocs::TopologyString
#define LOAD_ATTR_TOPOLOGY       "m_topology"
/// The load value reporting how many sockets the host has
#define LOAD_ATTR_SOCKETS        "m_socket"
/// The load value reporting how many cores the host has
#define LOAD_ATTR_CORES          "m_core"
/// The load value reporting how many hardware threads the host has
#define LOAD_ATTR_THREADS        "m_thread"

/// The load value reporting the devices the shepherd's systemd `DeviceAllow` is filled from (CS-2462); populated on the qmaster side from the `devices` characteristic of an RSMAP granted to the job
#define LOAD_ATTR_DEVICES        "devices"
/** @} */

bool host_is_referenced(const lListElem *host, lList **answer_list,
                        const lList *queue_list, const lList *hgrp_list);

const char *host_get_load_value(lListElem *host, const char *name);

int sge_resolve_host(lListElem *ep, int nm);

int sge_resolve_hostname(const char *hostname, char *unique, int nm);

bool
host_is_centry_referenced(const lListElem *this_elem, const lListElem *centry);

bool
host_is_centry_a_complex_value(const lListElem *this_elem,
                               const lListElem *centry);

lListElem *
host_list_locate(const lList *this_list, const char *hostname);

/* CS-2438: is this host an admin / submit host?
 *
 * The one place the question is answered, so that moving the answer from the
 * classic AH_LIST/SH_LIST to the reserved @admin_hosts/@submit_hosts groups is
 * a change to two function bodies rather than to a dozen call sites on the GDI
 * permission path. Mirrors manop_is_manager() in sge_manop.h, which plays the
 * same role for the reserved manager/operator usersets.
 *
 * Still backed by the classic lists -- see the note at the implementation. */
bool
host_is_admin_host(const char *hostname);

bool
host_is_submit_host(const char *hostname);

bool
host_list_merge(lList *this_list);

bool
host_merge(lListElem *host, const lListElem *global_host);

int
host_debit_rsmap(lListElem *host, const char *ce_name, const lListElem *resl, int slots, bool *just_check);

int
host_debit_binding(lListElem *host, const char *ce_name, const lListElem *resl, int slots, bool *just_check);

bool host_do_per_host_booking(const char **last_hostname, const char *hostname);

bool
host_is_visible(const lListElem *hep, bool is_manager, bool dept_view, const lList *acl_list);

std::string
host_get_topology_in_use(const lListElem *host);
