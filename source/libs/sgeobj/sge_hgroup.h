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

/** @file
 * @brief Declarations for host groups: named sets of hosts, written with a leading `@`
 *
 * @see sge_hgroup.cc
 */

#include <tuple>

#include "uti/sge_dstring.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_ST.h"

#include "sgeobj/ocs_Matcher.h"
#include "sgeobj/sge_answer.h"

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
/// The reserved host group holding every admin host
#define ADMIN_HOSTGROUP    "@admin_hosts"
/// The reserved host group holding every submit host
#define SUBMIT_HOSTGROUP   "@submit_hosts"
/// The reserved host group holding every execution host; read-only
#define EXEC_HOSTGROUP     "@exec_hosts"

/** @brief The prefix of a queue host group
 *
 * CS-2677. Every cluster queue owns one host group carrying its host list,
 * named after the queue: `@@all.q` for `all.q`. The prefix is unusable from
 * outside because hgroup_check_name() validates the name from its second
 * character and `@` is among the characters KEY_TABLE forbids -- which is what
 * makes it reservable. The qmaster creates these groups itself, with the
 * name check switched off (hgroup_create(..., is_name_validate = false)).
 */
#define QUEUE_HOSTGROUP_PREFIX  "@@"

bool hgroup_check_name(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char* name);

/* CS-2451: recompute HGRP_cached_hosts/HGRP_cache_version of one group, or of
 * every group in the list. Qmaster-side maintenance -- readers never compute. */
bool hgroup_update_cache(ocs::tom::Element<ocs::tom::HGRP> hgroup, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                         ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list);

bool hgroup_list_update_caches(ocs::tom::List<ocs::tom::HGRP> master_hgroup_list,
                               ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

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
bool hgroup_has_host_cache(ocs::tom::ConstElement<ocs::tom::HGRP> hgroup);

bool hgroup_cache_contains_host(ocs::tom::ConstElement<ocs::tom::HGRP> hgroup, const char *hostname);

/* CS-2438: does this group contain the host, directly or through nesting?
 *
 * The two functions above, composed the way every consumer wants them: cache
 * when there is one, tree walk when there is not. Prefer this over calling them
 * by hand -- an unguarded hgroup_cache_contains_host() answers "no" on an
 * element without a cache, which on the permission path denies a GDI request. */
bool hgroup_contains_host(ocs::tom::ConstElement<ocs::tom::HGRP> hgroup, const char *hostname,
                          ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list);

/**
 * @brief By which route is a host a member of a host group? (CS-2680, N-I-7)
 *
 * The question `qconf -shgrp_why` asks. It walks the **definition**, never the
 * resolved membership: a host admitted through a matcher is not in the resolved
 * membership at all, and one that is there gives no hint of what put it there.
 *
 * The order is that of the membership test - the literal members of the group
 * itself, then the groups it references, then the matchers - and the first route
 * found is the answer. A host somebody entered by name is therefore reported as
 * entered even when a matcher would fit it too.
 *
 * @param hgroup the group to ask
 * @param hostname the host in question, already resolved by the caller
 * @param master_hgroup_list the groups its references resolve against
 * @param[out] group receives the group the entry was found in, empty when that
 *        is @p hgroup itself
 * @param[out] detail receives the matcher on the matcher route, empty otherwise
 *
 * @return the route, or ocs::Matcher::Route::NOT_A_MEMBER
 */
ocs::Matcher::Route hgroup_why(ocs::tom::ConstElement<ocs::tom::HGRP> hgroup, const char *hostname,
                               ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, dstring *group,
                               dstring *detail);

/**
 * @brief Split a member list into its three classes (CS-2680, N-I-14)
 *
 * The definition of a host group holds literal host names, references to other
 * groups and matchers as alike strings in one list. This takes them apart, in
 * the order they are stored, for a consumer that should not have to tell them
 * apart by their text.
 *
 * Nothing is resolved and no reference is followed: this is the definition of
 * the one group, not its membership.
 *
 * @param hgroup the group whose member list is split; an empty handle gives false and three empty handles
 *
 * @return a tuple of
 *         - true when the group carries at least one matcher - the case in which
 *           the split says something the leading '@' does not;
 *         - the literal host names (`HR`), nullptr when none;
 *         - the group references (`HR`), nullptr when none;
 *         - the matchers (`HR`), nullptr when none
 */
[[nodiscard]] std::tuple<bool, ocs::tom::ListPtr<ocs::tom::HR>, ocs::tom::ListPtr<ocs::tom::HR>,
                         ocs::tom::ListPtr<ocs::tom::HR>>
hgroup_split_members(ocs::tom::ConstElement<ocs::tom::HGRP> hgroup);

/* true if name is one of the three reserved host groups above */
bool hgroup_is_reserved(const char *name);

/* true if name is a reserved group that no role may write (currently only
 * EXEC_HOSTGROUP, which the system maintains from the exec host list) */
bool hgroup_is_system_maintained(const char *name);

/* CS-2677: is this the host group carrying the host list of a cluster queue?
 *
 * Deliberately NOT hgroup_is_system_maintained(): that one refuses every write,
 * and the member list of a queue host group is exactly what an administrator
 * edits -- through the queue (qconf -mq) or through the group itself
 * (qconf -mhgrp @@all.q). What the system owns here is the lifecycle, not the
 * content: the group is created with its queue and removed with it, and this
 * predicate is what refuses an administrative create or delete. */
bool hgroup_is_queue_group(const char *name);

/* CS-2677: the name of the host group carrying the host list of "cqueue_name",
 * written into "buffer", which is also returned */
const char *
hgroup_queue_group_name(const char *cqueue_name, dstring *buffer);

/* CS-2677: the cluster queue a queue host group belongs to, or nullptr when
 * "name" is not one. The relation is carried by the name and by nothing else --
 * no field on either object refers to the other. */
const char *
hgroup_queue_of_group_name(const char *name);

/* --- */

[[nodiscard]] ocs::tom::ElementPtr<ocs::tom::HGRP>
hgroup_create(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name,
              ocs::tom::ListPtr<ocs::tom::HR> &&hostref_or_groupref, bool is_name_validate);

bool hgroup_add_references(ocs::tom::Element<ocs::tom::HGRP> this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                           ocs::tom::ConstList<ocs::tom::HR> hostref_or_groupref);

bool hgroup_find_all_references(ocs::tom::ConstElement<ocs::tom::HGRP> this_elem,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                ocs::tom::ConstList<ocs::tom::HGRP> master_list,
                                ocs::tom::ListPtr<ocs::tom::HR> *used_hosts,
                                ocs::tom::ListPtr<ocs::tom::HR> *used_groups,
                                ocs::tom::ListPtr<ocs::tom::HR> *used_matchers = nullptr,
                                bool expand_matchers = true);

bool hgroup_find_all_referencees(ocs::tom::ConstElement<ocs::tom::HGRP> this_elem,
                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 ocs::tom::ConstList<ocs::tom::HGRP> master_list,
                                 ocs::tom::ListPtr<ocs::tom::HR> *used_groups);

bool hgroup_find_referencees(ocs::tom::ConstElement<ocs::tom::HGRP> this_elem,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                             ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                             ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
                             ocs::tom::ListPtr<ocs::tom::HR> *occupants_groups,
                             ocs::tom::ListPtr<ocs::tom::ST> *occupants_queues);

/* --- */

ocs::tom::Element<ocs::tom::HGRP> hgroup_list_locate(ocs::tom::ConstList<ocs::tom::HGRP> this_list,
                                                     const char *group);

bool hgroup_list_exists(ocs::tom::ConstList<ocs::tom::HGRP> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        ocs::tom::ConstList<ocs::tom::HR> href_list);

bool hgroup_list_find_matching_and_resolve(ocs::tom::ConstList<ocs::tom::HGRP> this_list,
                                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                           const char *hgroup_pattern, ocs::tom::ListPtr<ocs::tom::HR> *used_hosts);

bool hgroup_list_find_matching(ocs::tom::ConstList<ocs::tom::HGRP> this_list,
                               ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                               const char *hgroup_pattern, ocs::tom::ListPtr<ocs::tom::HR> *href_list);
