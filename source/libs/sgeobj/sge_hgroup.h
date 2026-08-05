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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sgeobj/cull/sge_hgroup_HGRP_L.h"

/* Reserved host groups backing the admin/submit host lists (CS-2438), the
 * counterpart to MANAGER_USERSET/OPERATOR_USERSET in sge_userset.h.
 *
 * The leading '@' is part of the stored HGRP_name: ocs::is_hgroup_name() is
 * name[0] == '@' (uti/ocs_Pattern.h), and hgroup_check_name() validates the
 * remainder from name[1] on.
 *
 * ADMIN and SUBMIT are maintained through the classic interface
 * (qconf -ah/-dh/-as/-ds) as well as the host group CLI; EXEC is derived from
 * the execution host list and is read-only for everyone, managers included.
 */
#define ADMIN_HOSTGROUP    "@admin_hosts"
#define SUBMIT_HOSTGROUP   "@submit_hosts"
#define EXEC_HOSTGROUP     "@exec_hosts"

bool hgroup_check_name(lList **answer_list, const char* name);

/* CS-2451: recompute HGRP_cached_hosts/HGRP_cache_version of one group, or of
 * every group in the list. Qmaster-side maintenance -- readers never compute. */
bool
hgroup_update_cache(lListElem *hgroup, lList **answer_list, const lList *master_hgroup_list);

bool
hgroup_list_update_caches(lList *master_hgroup_list, lList **answer_list);

/* CS-2451: consume the resolved host list cache.
 *
 * Correctness must never depend on the cache. A consumer asks
 * hgroup_has_host_cache() first and falls back to the tree walk when it says no
 * -- that covers a missed computation site, a rolling upgrade, and an element
 * that arrived through a GDI "what" filter without the fields.
 *
 * Two functions rather than one returning the list, because an EMPTY cache is
 * valid: a group can legitimately resolve to no hosts, and cull stores an empty
 * list as nullptr. "No hosts" and "no cache" must not collapse into one answer.
 */
bool
hgroup_has_host_cache(const lListElem *hgroup);

bool
hgroup_cache_contains_host(const lListElem *hgroup, const char *hostname);

/* true if name is one of the three reserved host groups above */
bool hgroup_is_reserved(const char *name);

/* true if name is a reserved group that no role may write (currently only
 * EXEC_HOSTGROUP, which the system maintains from the exec host list) */
bool hgroup_is_system_maintained(const char *name);

/* --- */

lListElem *
hgroup_create(lList **answer_list, const char *name, 
              lList *hostref_or_groupref, bool is_name_validate);

bool 
hgroup_add_references(lListElem *this_elem, lList **answer_list,
                      const lList *hostref_or_groupref);

bool 
hgroup_find_references(const lListElem *this_elem, lList **answer_list,
                       const lList *master_list, lList **used_hosts,
                       lList **used_groups);

bool 
hgroup_find_all_references(const lListElem *this_elem, lList **answer_list,
                           const lList *master_list, lList **used_hosts,
                           lList **used_groups);

bool 
hgroup_find_all_referencees(const lListElem *this_elem, 
                            lList **answer_list,
                            const lList *master_list, lList **used_groups);

bool
hgroup_find_referencees(const lListElem *this_elem,
                        lList **answer_list,
                        const lList *master_hgroup_list,
                        const lList *master_cqueue_list,
                        lList **occupants_groups,
                        lList **occupants_queues);


/* --- */

lListElem *
hgroup_list_locate(const lList *this_list, const char *group);

lList **
hgroup_list_get_master_list();

bool
hgroup_list_exists(const lList *this_list, lList **answer_list,
                   const lList *href_list);

bool
hgroup_list_find_matching_and_resolve(const lList *this_list,
                                      lList **answer_list,
                                      const char *hgroup_pattern,
                                      lList **used_hosts);

bool
hgroup_list_find_matching(const lList *this_list, lList **answer_list,
                          const char *hgroup_pattern, lList **used_hosts);
