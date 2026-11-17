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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Host groups: named sets of hosts, written with a leading `@`
 *
 * A group may contain other groups, so resolving one is recursive. Three
 * group names are reserved and maintained by the daemons rather than by an
 * administrator - see #ADMIN_HOSTGROUP and its siblings.
 *
 * @see sge_hgroup.h
 */
/*
  This module is used for group building
 */

#include <cstdio>
#include <cstring>
#include <tuple>
#include <utility>

#include "uti/ocs_Pattern.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "comm/commlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"

#include <cinttypes>
#include "msg_common.h"

/// Debug layer the host group traces are written to
#define HGROUP_LAYER TOP_LAYER

/**
 * @brief Refresh the resolved host list of one group
 *
 * CS-2451: resolves the nested references of @p hgroup once and stores the
 * flat result in HGRP_cached_hosts, so a membership test becomes a hash
 * lookup on HR_name instead of a walk of the tree.
 * Qmaster-side maintenance only. Readers never compute a cache; they use it
 * when HGRP_cache_version is non-zero and fall back to the walk when it is 0.
 * The version counter is a PRESENCE FLAG, not a stamp to compare against
 * anything (see HGRP.json). It only has to be non-zero and never 0, which is
 * why the counter starts at 1 and skips 0 on wrap.
 *
 * @param hgroup the group to refresh (HGRP_Type); an empty handle gives false
 * @param answer_list for returning errors
 * @param master_hgroup_list list the references are resolved against; an empty handle gives false
 *
 * @return true on success; on failure the cache is left INVALID (version 0) rather than stale, so
 *         consumers fall back to the walk
 *
 * @note MT-NOTE: hgroup_update_cache() is not MT safe -- call it under the write
 *       lock, like every other writer of the HGRP master list
 */
bool hgroup_update_cache(const ocs::tom::Element<ocs::tom::HGRP> hgroup, lList **answer_list,
                         const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   DENTER(HGROUP_LAYER);

   static lUlong next_version = 1;

   if (!hgroup || !master_hgroup_list) {
      DRETURN(false);
   }

   ocs::tom::ListPtr<ocs::tom::HR> used_hosts;

   // resolve the whole nested tree once
   const bool ret = hgroup_find_all_references(hgroup, answer_list, master_hgroup_list, &used_hosts, nullptr);

   if (ret) {
      ocs::tom::set<HGRP_cached_hosts>(hgroup, std::move(used_hosts));   // takes ownership
      if (++next_version == 0) {
         next_version = 1;                               // 0 means "not computed"
      }
      ocs::tom::set<HGRP_cache_version>(hgroup, next_version);
   } else {
      // Leave no half-resolved cache behind: an invalid cache must look
      // uncomputed, never plausible-but-wrong. used_hosts is freed on return.
      ocs::tom::set<HGRP_cached_hosts>(hgroup, nullptr);
      ocs::tom::set<HGRP_cache_version>(hgroup, 0);
   }

   DRETURN(ret);
}

/**
 * @brief hgroup_update_cache() for an untyped caller
 *
 * @param hgroup the group to refresh (HGRP_Type), checked; nullptr gives false
 * @param answer_list for returning errors
 * @param master_hgroup_list HGRP_Type list the references are resolved against, checked; nullptr gives false
 *
 * @return as hgroup_update_cache()
 */
bool hgroup_update_cache(lListElem *hgroup, lList **answer_list, const lList *master_hgroup_list) {
   return hgroup_update_cache(ocs::tom::Element<ocs::tom::HGRP>(hgroup), answer_list,
                              ocs::tom::ConstList<ocs::tom::HGRP>(master_hgroup_list));
}

/**
 * @brief Refresh every group's resolved host list
 *
 * Used at qmaster startup, after the host group list has been read from the
 * spool area. Every group is resolved independently, so the order in the
 * list does not matter.
 *
 * @param master_hgroup_list the HGRP_Type list to refresh in place, may be an empty handle
 * @param answer_list for returning errors
 *
 * @return true if every group could be resolved
 *
 * @note MT-NOTE: hgroup_list_update_caches() is not MT safe
 */
bool hgroup_list_update_caches(const ocs::tom::List<ocs::tom::HGRP> master_hgroup_list, lList **answer_list) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   for (const auto hgroup : master_hgroup_list) {
      ret &= hgroup_update_cache(hgroup, answer_list, master_hgroup_list);
   }
   DRETURN(ret);
}

/**
 * @brief hgroup_list_update_caches() for an untyped caller
 *
 * @param master_hgroup_list the HGRP_Type list to refresh in place, checked; may be nullptr
 * @param answer_list for returning errors
 *
 * @return as hgroup_list_update_caches()
 */
bool hgroup_list_update_caches(lList *master_hgroup_list, lList **answer_list) {
   return hgroup_list_update_caches(ocs::tom::List<ocs::tom::HGRP>(master_hgroup_list), answer_list);
}

/**
 * @brief Does this group carry a usable resolved list?
 *
 * CS-2451. True when HGRP_cache_version is non-zero, i.e. qmaster has
 * resolved this group and the result travelled with the element.
 * The version is what makes the answer possible at all: cull stores an empty
 * list as nullptr, so HGRP_cached_hosts alone cannot distinguish a group that
 * resolves to no hosts from one that was never resolved.
 *
 * @param hgroup HGRP_Type object, may be an empty handle
 *
 * @return true if hgroup_cache_contains_host() may be used on this element
 *
 * @note MT-NOTE: hgroup_has_host_cache() is MT safe -- but it reads the master list of the
 *       calling thread's data store, whose elements the mirror thread frees while
 *       merging events (CS-2633). The caller must hold that store's READ lock;
 *       on the GDI permission path that is LOCK_LISTENER.
 *
 *       HGRP_cached_hosts and HGRP_cache_version are a UNIT. A GDI "what" filter
 *       that selects the version without the list would present an empty cache as
 *       valid, and every membership test against that group would answer "no".
 *       Every descriptor in the tree is currently lWhat("%T(ALL)"); if a reduced
 *       one is ever introduced -- RBAC output reduction is the likely reason --
 *       it must take both fields or neither.
 */
bool hgroup_has_host_cache(const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup) {
   return hgroup && ocs::tom::get<HGRP_cache_version>(hgroup) != 0;
}

/**
 * @brief hgroup_has_host_cache() for an untyped caller
 *
 * @param hgroup HGRP_Type object, checked; may be nullptr
 *
 * @return as hgroup_has_host_cache()
 */
bool hgroup_has_host_cache(const lListElem *hgroup) {
   return hgroup_has_host_cache(ocs::tom::ConstElement<ocs::tom::HGRP>(hgroup));
}

/**
 * @brief Is host a member, according to the cache?
 *
 * CS-2451. Answers the membership question with a single hash lookup instead
 * of a walk of the nested group tree.
 * The lookup is equivalent to the walk's sge_hostcmp() comparison, not merely
 * similar: cull normalises host keys with sge_hostcpy() + sge_strtoupper()
 * (cull_multitype.cc), and sge_hostcpy() honours the same ignore_fqdn and
 * default_domain rules as sge_hostcmp() (sge_hostname.cc). HR_name carries
 * the HASH flag, so the lookup is O(1).
 *
 * @param hgroup HGRP_Type object with a valid cache; an empty handle gives false
 * @param hostname host to look for; nullptr gives false
 *
 * @return true if the host is in the group, directly or through nesting
 *
 * @note MT-NOTE: hgroup_cache_contains_host() is MT safe -- but it reads the master list of the
 *       calling thread's data store, whose elements the mirror thread frees while
 *       merging events (CS-2633). The caller must hold that store's READ lock;
 *       on the GDI permission path that is LOCK_LISTENER.
 *
 *       Only meaningful when hgroup_has_host_cache() is true; on an element without
 *       a cache it reports false, which is the WRONG answer rather than a safe one.
 *       Always guard the call.
 */
bool hgroup_cache_contains_host(const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup, const char *hostname) {
   if (!hgroup || hostname == nullptr) {
      return false;
   }
   return ocs::tom::get<HGRP_cached_hosts>(hgroup).find_first<HR_name>(ocs::tom::Host(hostname)) != nullptr;
}

/**
 * @brief Is the host a member, directly or through nesting?
 *
 * CS-2438. The two functions above composed the way consumers actually want
 * them: use the cache when the element carries one, resolve the nested tree
 * when it does not.
 * This exists so that the guard is written once. hgroup_cache_contains_host()
 * on an element without a cache answers "no", which is the wrong answer
 * rather than a safe one -- and its callers are the GDI permission path
 * (host_is_admin_host()) and the delete-semantics check in the qmaster, where
 * a wrong "no" denies a request or reports a member as absent.
 *
 * @param hgroup HGRP_Type object, may be an empty handle
 * @param hostname host to look for
 * @param master_hgroup_list HGRP_Type list the nested references resolve against
 *
 * @return true if the host is in the group, directly or transitively
 *
 * @note MT-NOTE: hgroup_contains_host() is MT safe -- but it reads the master list of the
 *       calling thread's data store, whose elements the mirror thread frees while
 *       merging events (CS-2633). The caller must hold that store's READ lock;
 *       on the GDI permission path that is LOCK_LISTENER.
 *
 *       The fallback allocates and walks; it is not the hot path and must not
 *       become one. If a caller finds itself here on every request, the cache is
 *       not being maintained -- fix that rather than optimising this.
 */
bool hgroup_contains_host(const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup, const char *hostname,
                          const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   DENTER(HGROUP_LAYER);

   if (!hgroup || hostname == nullptr) {
      DRETURN(false);
   }

   if (hgroup_has_host_cache(hgroup)) {
      DRETURN(hgroup_cache_contains_host(hgroup, hostname));
   }

   lList *answer_list = nullptr;
   ocs::tom::ListPtr<ocs::tom::HR> used_hosts;
   bool ret = false;

   const bool resolved = hgroup_find_all_references(hgroup, &answer_list, master_hgroup_list, &used_hosts, nullptr);

   if (resolved) {
      ret = used_hosts->find_first<HR_name>(ocs::tom::Host(hostname)) != nullptr;
   }
   lFreeList(&answer_list);

   DRETURN(ret);
}

/**
 * @brief hgroup_contains_host() for an untyped caller
 *
 * @param hgroup HGRP_Type object, checked; may be nullptr
 * @param hostname host to look for
 * @param master_hgroup_list HGRP_Type list the nested references resolve against, checked
 *
 * @return as hgroup_contains_host()
 */
bool hgroup_contains_host(const lListElem *hgroup, const char *hostname, const lList *master_hgroup_list) {
   return hgroup_contains_host(ocs::tom::ConstElement<ocs::tom::HGRP>(hgroup), hostname,
                               ocs::tom::ConstList<ocs::tom::HGRP>(master_hgroup_list));
}

/// How deep hgroup_why() follows group references before it gives up
static constexpr int HGROUP_WHY_MAX_NESTING = 32;

/**
 * @brief Find a host named literally in this group or in one it references
 *
 * @param hgroup the group to search (HGRP_Type), may be an empty handle
 * @param hostname the host in question
 * @param master_hgroup_list the groups its references resolve against (HGRP_Type)
 * @param depth how deep the recursion already is
 * @param[out] group receives the name of the group the entry was found in
 *
 * @return true when the host is named somewhere below @p hgroup
 */
static bool
hgroup_why_named(const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup, const char *hostname,
                 const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, const int depth, dstring *group) {
   if (!hgroup || depth > HGROUP_WHY_MAX_NESTING) {
      // A cycle is refused when a group is written, so this is a guard and not
      // a case - but a diagnostic command must return whatever it is given.
      return false;
   }

   const auto member_list = ocs::tom::get<HGRP_host_list>(hgroup);

   // the names of this group before those of the groups it references, so that
   // the nearest explanation is the one reported
   for (const auto member : member_list) {
      const char *name = ocs::tom::get<HR_name>(member);

      if (ocs::classify_member(name) == ocs::MemberClass::LITERAL_HOST &&
          sge_hostcmp(name, hostname) == 0) {
         sge_dstring_copy_string(group, ocs::tom::get<HGRP_name>(hgroup));
         return true;
      }
   }

   for (const auto member : member_list) {
      const char *name = ocs::tom::get<HR_name>(member);

      if (ocs::classify_member(name) == ocs::MemberClass::GROUP_REFERENCE &&
          hgroup_why_named(hgroup_list_locate(master_hgroup_list, name), hostname,
                           master_hgroup_list, depth + 1, group)) {
         return true;
      }
   }

   return false;
}

/**
 * @brief By which route is a host a member of a host group?
 *
 * @see sge_hgroup.h for what the routes mean and why the order is what it is.
 *
 * @param hgroup the group to ask (HGRP_Type), may be an empty handle
 * @param hostname the host in question, already resolved by the caller
 * @param master_hgroup_list the groups its references resolve against (HGRP_Type)
 * @param[out] group receives the group the entry was found in, empty when that
 *        is @p hgroup itself
 * @param[out] detail receives the matcher on the matcher route, empty otherwise
 *
 * @return the route, or ocs::Matcher::Route::NOT_A_MEMBER
 */
ocs::Matcher::Route
hgroup_why(const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup, const char *hostname,
           const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, dstring *group, dstring *detail) {
   DENTER(HGROUP_LAYER);

   if (!hgroup || hostname == nullptr || group == nullptr || detail == nullptr) {
      DRETURN(ocs::Matcher::Route::NOT_A_MEMBER);
   }

   sge_dstring_clear(group);
   sge_dstring_clear(detail);

   if (hgroup_why_named(hgroup, hostname, master_hgroup_list, 0, group)) {
      const char *found_in = sge_dstring_get_string(group);
      const char *asked = ocs::tom::get<HGRP_name>(hgroup);

      // the group is reported only when it is not the one that was asked
      if (found_in != nullptr && asked != nullptr && strcmp(found_in, asked) == 0) {
         sge_dstring_clear(group);
         DRETURN(ocs::Matcher::Route::LITERAL);
      }
      DRETURN(ocs::Matcher::Route::GROUP_REFERENCE);
   }

   // Only now the matchers. A host that is named somewhere was put there by
   // somebody, and that is the more informative answer even when a matcher would
   // fit it as well.
   const ocs::Matcher::Route route =
           ocs::Matcher::why(hgroup, hostname, master_hgroup_list, group, detail);

   if (route == ocs::Matcher::Route::MATCHER) {
      const char *found_in = sge_dstring_get_string(group);
      const char *asked = ocs::tom::get<HGRP_name>(hgroup);

      if (found_in != nullptr && asked != nullptr && strcmp(found_in, asked) == 0) {
         sge_dstring_clear(group);
      }
   }

   DRETURN(route);
}

/**
 * @brief hgroup_why() for an untyped caller
 *
 * @param hgroup the group to ask (HGRP_Type), checked; may be nullptr
 * @param hostname the host in question, already resolved by the caller
 * @param master_hgroup_list the groups its references resolve against (HGRP_Type), checked
 * @param[out] group receives the group the entry was found in, empty when that
 *        is @p hgroup itself
 * @param[out] detail receives the matcher on the matcher route, empty otherwise
 *
 * @return as hgroup_why()
 */
ocs::Matcher::Route
hgroup_why(const lListElem *hgroup, const char *hostname, const lList *master_hgroup_list, dstring *group,
           dstring *detail) {
   return hgroup_why(ocs::tom::ConstElement<ocs::tom::HGRP>(hgroup), hostname,
                     ocs::tom::ConstList<ocs::tom::HGRP>(master_hgroup_list), group, detail);
}

/**
 * @brief Split a member list into its three classes
 *
 * @see sge_hgroup.h
 *
 * @param hgroup the group whose member list is split; an empty handle gives false and three empty handles
 *
 * @return a tuple of: true when the group carries at least one matcher; the literal host names; the group
 *         references; the matchers - each list `HR_Type` and nullptr when none
 */
std::tuple<bool, ocs::tom::ListPtr<ocs::tom::HR>, ocs::tom::ListPtr<ocs::tom::HR>, ocs::tom::ListPtr<ocs::tom::HR>>
hgroup_split_members(const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup) {
   DENTER(HGROUP_LAYER);

   bool has_matcher = false;
   // created on the first entry, as before
   ocs::tom::ListPtr<ocs::tom::HR> hosts;
   ocs::tom::ListPtr<ocs::tom::HR> groups;
   ocs::tom::ListPtr<ocs::tom::HR> matchers;

   if (hgroup) {
      for (const auto member : ocs::tom::get<HGRP_host_list>(hgroup)) {
         const char *name = ocs::tom::get<HR_name>(member);

         switch (ocs::classify_member(name)) {
            case ocs::MemberClass::LITERAL_HOST:
               ocs_href_list_emplace(hosts, name);
               break;
            case ocs::MemberClass::GROUP_REFERENCE:
               ocs_href_list_emplace(groups, name);
               break;
            case ocs::MemberClass::MATCHER:
               ocs_href_list_emplace(matchers, name);
               has_matcher = true;
               break;
         }
      }
   }

   auto split = std::make_tuple(has_matcher, std::move(hosts), std::move(groups), std::move(matchers));
   DRETURN(split);
}

/**
 * @brief Is this one of the reserved host groups?
 *
 * True for `@admin_hosts`, `@submit_hosts` and `@exec_hosts` (CS-2438). These
 * back what used to be the AH_LIST/SH_LIST data models and the execution
 * host list, so they may not be deleted and carry extra rules on write.
 * Comparison is case-INsensitive (CS-2763), because `HGRP_name` is an
 * `SGE_HOST` field: the object layer folds it, so `@ADMIN_HOSTS` and
 * `@admin_hosts` are one and the same group and a predicate that answered
 * differently for the two would disagree with the lookup that finds them.
 *
 * It used to be `strcmp()`, justified by the parallel to the reserved usersets
 * in sge_userset_qmaster.cc. That parallel does not hold: `US_name` is an
 * `SGE_STRING` field, where case-sensitivity is right, and `HGRP_name` is not.
 * The consequence was measurable -- `qconf -dhgrp @ADMIN_HOSTS` walked past
 * this guard, found the group, and was stopped only by the spool file carrying
 * the stored spelling.
 *
 * sge_hostcmp() is the comparison the object layer itself uses, and it already
 * handles a group name as a group name: `@`-prefixed operands are compared
 * case-insensitively with no domain normalisation applied.
 *
 * @param name host group name including the leading '@'
 *
 * @return true if reserved
 *
 * @note MT-NOTE: hgroup_is_reserved() is MT safe
 */
bool hgroup_is_reserved(const char *name) {
   return name != nullptr &&
          (sge_hostcmp(name, ADMIN_HOSTGROUP) == 0 ||
           sge_hostcmp(name, SUBMIT_HOSTGROUP) == 0 ||
           sge_hostcmp(name, EXEC_HOSTGROUP) == 0);
}

/**
 * @brief May nobody write this group?
 *
 * True only for `@exec_hosts`, which the qmaster derives from the execution
 * host list. Write access is refused for every role including manager --
 * the spec states this independently of RBAC (04_Logical_View.md, "Protected
 * Object Keys"), because a hand-edited copy would silently disagree with the
 * exec host list it is supposed to mirror.
 *
 * Case-insensitive for the reason given at hgroup_is_reserved() (CS-2763):
 * `HGRP_name` is an `SGE_HOST` field, so `@EXEC_HOSTS` names this very group.
 *
 * @param name host group name including the leading '@'
 *
 * @return true if the group is maintained by the system
 *
 * @note MT-NOTE: hgroup_is_system_maintained() is MT safe
 */
bool hgroup_is_system_maintained(const char *name) {
   return name != nullptr && sge_hostcmp(name, EXEC_HOSTGROUP) == 0;
}

/**
 * @brief Is this the host group carrying the host list of a cluster queue?
 *
 * CS-2677. Queue host groups are named `@@<queue>`. The prefix is reserved
 * rather than merely conventional: hgroup_check_name() validates a name from
 * its second character on and `@` is among the characters KEY_TABLE forbids,
 * so no administrator can write one.
 *
 * No case folding is needed here, unlike the two predicates above (CS-2763):
 * the test is for the literal prefix `@@`, which has no case.
 *
 * This is deliberately not part of hgroup_is_system_maintained(): that
 * predicate refuses every write, and the member list of a queue host group is
 * exactly what an administrator edits. What the system owns is the lifecycle.
 *
 * @param name host group name including the leading '@'
 *
 * @return true if the group belongs to a cluster queue
 *
 * @note MT-NOTE: hgroup_is_queue_group() is MT safe
 */
bool hgroup_is_queue_group(const char *name) {
   return name != nullptr && strncmp(name, QUEUE_HOSTGROUP_PREFIX,
                                     sizeof(QUEUE_HOSTGROUP_PREFIX) - 1) == 0;
}

/**
 * @brief The name of the host group carrying a cluster queue's host list
 *
 * CS-2677. The name is derived and stored nowhere: neither object carries a
 * field pointing at the other, which is what keeps the two from disagreeing.
 *
 * @param cqueue_name name of the cluster queue
 * @param buffer receives the name
 *
 * @return the name, or nullptr if either argument is missing
 *
 * @note MT-NOTE: hgroup_queue_group_name() is MT safe
 */
const char *
hgroup_queue_group_name(const char *cqueue_name, dstring *buffer) {
   if (cqueue_name == nullptr || buffer == nullptr) {
      return nullptr;
   }
   sge_dstring_clear(buffer);
   sge_dstring_append(buffer, QUEUE_HOSTGROUP_PREFIX);
   sge_dstring_append(buffer, cqueue_name);

   return sge_dstring_get_string(buffer);
}

/**
 * @brief The cluster queue a queue host group belongs to
 *
 * CS-2677, the other direction of hgroup_queue_group_name(). N-T-5 rests on
 * it: a host group change finds the queues it affects by asking each of them
 * whether it names the group, and a queue host group is named by no host list,
 * its own included. The relation is carried by the name, and the lookup has to
 * be told so.
 *
 * @param name host group name including the leading '@'
 *
 * @return a pointer into "name" past the prefix, or nullptr if "name" is not a queue host group
 *
 * @note MT-NOTE: hgroup_queue_of_group_name() is MT safe
 */
const char *
hgroup_queue_of_group_name(const char *name) {
   if (!hgroup_is_queue_group(name)) {
      return nullptr;
   }
   const char *queue_name = name + sizeof(QUEUE_HOSTGROUP_PREFIX) - 1;

   return (*queue_name != '\0') ? queue_name : nullptr;
}

/**
 * @brief Determine if the name is a valid hgroup name
 *
 * Determine if the given name is a valid hostgroup name. If not
 * add an approbiate error to the answer_list
 *
 * @param answer_list answer list where errors are stored
 * @param name name of the hostgroup
 *
 * @return result true  -  name contains a valid name for a hostgroup false - name is not a valid name for a hostrgroup
 *
 * @note MT-NOTE: check_hgroup_name() is not MT safe
 */
bool hgroup_check_name(lList **answer_list, const char *name) {
   if (!ocs::is_hgroup_name(name)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_HGRP_INVALIDHOSTGROUPNAME_S, name);
      return false;
   }
   if (verify_obj_name(
      answer_list,&name[1], MAX_VERIFY_STRING,
      "hostgroup") != STATUS_OK) {
      return false;
   }
   return true;
}

/**
 * @brief Find a group by name
 *
 * Find a 'group' in 'this_list'.
 *
 * @param this_list HGRP_Type list, may be an empty handle
 * @param group group name
 *
 * @return found element or an empty handle
 */
ocs::tom::Element<ocs::tom::HGRP>
hgroup_list_locate(const ocs::tom::ConstList<ocs::tom::HGRP> this_list, const char *group) {
   DENTER(HGROUP_LAYER);

   const auto ret = ocs::tom::find_first_rw<HGRP_name>(this_list, group);
   DRETURN(ret);
}

/**
 * @brief hgroup_list_locate() for an untyped caller
 *
 * @param this_list HGRP_Type list, checked; may be nullptr
 * @param group group name
 *
 * @return found element or nullptr
 */
lListElem *hgroup_list_locate(const lList *this_list, const char *group) {
   return hgroup_list_locate(ocs::tom::ConstList<ocs::tom::HGRP>(this_list), group).raw();
}


/**
 * @brief Create a new hgroup
 *
 * Create a new hostgroup.
 *
 * @param answer_list AN_Type list
 * @param name name
 * @param href_or_groupref list of hosts for this hgroup (HR_Type), may be empty; taken over only when the group
 *        is created, otherwise it stays with the caller
 * @param is_name_validate if true, the hgrp name is validated. Should be done all the time, there is only one
 *        case in qconf in that the name has to be ignored.
 *
 * @return new element or an empty handle
 */
ocs::tom::ElementPtr<ocs::tom::HGRP>
hgroup_create(lList **answer_list, const char *name, ocs::tom::ListPtr<ocs::tom::HR> &&href_or_groupref,
              const bool is_name_validate) {
   DENTER(HGROUP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::HGRP> ret = nullptr;

   if (name != nullptr) {
      if (!is_name_validate || hgroup_check_name(answer_list, name)) {
         ret = ocs::tom::ElementPtr<ocs::tom::HGRP>::create();
         if (ret) {
            ocs::tom::set<HGRP_name>(ret, name);
            ocs::tom::set<HGRP_host_list>(ret, std::move(href_or_groupref));
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_MEM_MEMORYALLOCFAILED_S, __func__);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
         }
      }
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   }
   DRETURN(ret);
}

/**
 * @brief hgroup_create() for an untyped caller
 *
 * @param answer_list AN_Type list
 * @param name name
 * @param href_or_groupref list of hosts for this hgroup (HR_Type), checked; may be nullptr; taken over only when
 *        the group is created, otherwise it stays with the caller
 * @param is_name_validate if true, the hgrp name is validated
 *
 * @return new element or nullptr
 */
lListElem *hgroup_create(lList **answer_list, const char *name, lList *href_or_groupref, bool is_name_validate) {
   auto hrefs = ocs::tom::ListPtr<ocs::tom::HR>(href_or_groupref);
   auto ret = hgroup_create(answer_list, name, std::move(hrefs), is_name_validate);

   // not taken over: the list stays with the caller, as before
   [[maybe_unused]] const lList *const not_taken = hrefs.release();
   return ret.release();
}

/**
 * @brief Add a host or group reference
 *
 * Add a host or group reference.
 *
 * @param this_elem HGRP_Type elem
 * @param answer_list AN_Type list
 * @param href_or_groupref HR_Type list, may be an empty handle
 *
 * @return error state true  - Success false - Error
 */
bool hgroup_add_references(const ocs::tom::Element<ocs::tom::HGRP> this_elem, lList **answer_list,
                           const ocs::tom::ConstList<ocs::tom::HR> href_or_groupref) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_elem && href_or_groupref) {
      // taken out of the element for href_list_add(), handed back below
      auto href_list = this_elem.exchange<HGRP_host_list>(nullptr);

      for (const auto href : href_or_groupref) {
         const char *name = ocs::tom::get<HR_name>(href);

         ret &= href_list_add(href_list, answer_list, name);
         if (!ret) {
            break;
         }
      }
      static_cast<void>(this_elem.exchange<HGRP_host_list>(std::move(href_list)));
   } else {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_INAVLID_PARAMETER_IN_S, __func__);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Find referenced host and groups
 *
 * Find directly or indirectly referenced hgroup names.
 * 'master_list' has to be the list of all existing hgroups.
 * 'used_hosts' and 'used_groups' will contain the names of
 * hosts and groups referenced by 'this_elem'.
 *
 * @param this_elem HGRP_Type, may be an empty handle
 * @param answer_list AN_Type list
 * @param master_list HGRP_Type list, may be an empty handle
 * @param used_hosts in-out: HR_Type list, or nullptr
 * @param used_groups in-out: HR_Type list, or nullptr
 * @param used_matchers in-out: HR_Type list receiving the matchers met, or nullptr
 * @param expand_matchers whether the hosts the matchers capture are added to 'used_hosts'
 *
 * @return error state true  - Success false - Error
 *
 * @note Extremely poor performance. Try not to use this function.
 */
bool hgroup_find_all_references(const ocs::tom::ConstElement<ocs::tom::HGRP> this_elem, lList **answer_list,
                                const ocs::tom::ConstList<ocs::tom::HGRP> master_list,
                                ocs::tom::ListPtr<ocs::tom::HR> *used_hosts,
                                ocs::tom::ListPtr<ocs::tom::HR> *used_groups,
                                ocs::tom::ListPtr<ocs::tom::HR> *used_matchers, const bool expand_matchers) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_elem && master_list) {
      ocs::tom::ListPtr<ocs::tom::HR> href_list;   // created by href_list_add()

      const char *name = ocs::tom::get<HGRP_name>(this_elem);
      ret &= href_list_add(href_list, answer_list, name);

      if (ret) {
         ret &= href_list_find_all_references(*href_list, answer_list, master_list, used_hosts, used_groups,
                                              used_matchers, expand_matchers);
      }
   }
   DRETURN(ret);
}

/**
 * @brief hgroup_find_all_references() for an untyped caller
 *
 * @param this_elem HGRP_Type, checked; may be nullptr
 * @param answer_list AN_Type list
 * @param master_list HGRP_Type list, checked; may be nullptr
 * @param used_hosts in-out: HR_Type list, or nullptr
 * @param used_groups in-out: HR_Type list, or nullptr
 * @param used_matchers in-out: HR_Type list receiving the matchers met, or nullptr
 * @param expand_matchers whether the hosts the matchers capture are added to 'used_hosts'
 *
 * @return as hgroup_find_all_references()
 */
bool hgroup_find_all_references(const lListElem *this_elem, lList **answer_list, const lList *master_list,
                                lList **used_hosts, lList **used_groups, lList **used_matchers,
                                const bool expand_matchers) {
   ocs::UntypedHrefList typed_used_hosts(used_hosts);
   ocs::UntypedHrefList typed_used_groups(used_groups);
   ocs::UntypedHrefList typed_used_matchers(used_matchers);

   return hgroup_find_all_references(ocs::tom::ConstElement<ocs::tom::HGRP>(this_elem), answer_list,
                                     ocs::tom::ConstList<ocs::tom::HGRP>(master_list), typed_used_hosts.get(),
                                     typed_used_groups.get(), typed_used_matchers.get(), expand_matchers);
}

/**
 * @brief Find groups refering to this group
 *
 * Find all hostgroups from 'master_list' which reference the
 * hostgroup 'this_elem'. The name of these hostgroups will be
 * returned in the hreference list 'occupants_groups'.
 * 'answer_list' will contain error messages if the function is
 * not successful
 *
 * @param this_elem HGRP_Type element, may be an empty handle
 * @param answer_list AN_Type list
 * @param master_list list of all existing HGRP_Type elements
 * @param occupants_groups in-out: HR_Type list
 *
 * @return exit state true  - Success false - Error
 */
bool hgroup_find_all_referencees(const ocs::tom::ConstElement<ocs::tom::HGRP> this_elem, lList **answer_list,
                                 const ocs::tom::ConstList<ocs::tom::HGRP> master_list,
                                 ocs::tom::ListPtr<ocs::tom::HR> *occupants_groups) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_elem && occupants_groups != nullptr) {
      ocs::tom::ListPtr<ocs::tom::HR> href_list;   // created by href_list_add()
      const char *name = ocs::tom::get<HGRP_name>(this_elem);

      ret &= href_list_add(href_list, answer_list, name);

      if (ret) {
         ret &= href_list_find_all_referencees(*href_list, answer_list,
                                               master_list, occupants_groups);
      }
   }
   DRETURN(ret);
}

/**
 * @brief hgroup_find_all_referencees() for an untyped caller
 *
 * @param this_elem HGRP_Type element, checked; may be nullptr
 * @param answer_list AN_Type list
 * @param master_list list of all existing HGRP_Type elements, checked
 * @param occupants_groups in-out: HR_Type list
 *
 * @return as hgroup_find_all_referencees()
 */
bool hgroup_find_all_referencees(const lListElem *this_elem, lList **answer_list, const lList *master_list,
                                 lList **occupants_groups) {
   ocs::UntypedHrefList typed_occupants_groups(occupants_groups);

   return hgroup_find_all_referencees(ocs::tom::ConstElement<ocs::tom::HGRP>(this_elem), answer_list,
                                      ocs::tom::ConstList<ocs::tom::HGRP>(master_list),
                                      typed_occupants_groups.get());
}

/**
 * @brief Find groups refering to this group
 *
 * Find all hostgroups from 'master_list' which reference the
 * hostgroup 'this_elem'. The name of these hostgroups will be
 * returned in the hreference list 'occupants_groups'.
 * 'answer_list' will contain error messages if the function is
 * not successful
 *
 * @param this_elem HGRP_Type, may be an empty handle
 * @param answer_list AN_Type
 * @param master_hgroup_list HGRP_Type master list
 * @param master_cqueue_list CQ_Type master list
 * @param occupants_groups in-out: HR_Type list, or nullptr
 * @param occupants_queues in-out: ST_Type list, or nullptr
 *
 * @return Error state true  - Success false - Error
 */
bool hgroup_find_referencees(const ocs::tom::ConstElement<ocs::tom::HGRP> this_elem, lList **answer_list,
                             const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list,
                             const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
                             ocs::tom::ListPtr<ocs::tom::HR> *occupants_groups, lList **occupants_queues) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_elem) {
      if (occupants_groups != nullptr) {
         const char *name = ocs::tom::get<HGRP_name>(this_elem);
         ocs::tom::ListPtr<ocs::tom::HR> href_list;   // created by href_list_add()

         ret &= href_list_add(href_list, answer_list, name);

         if (ret) {
            ret &= href_list_find_referencees(*href_list, answer_list,
                                              master_hgroup_list,
                                              occupants_groups);
         }
      }
      if (ret && occupants_queues != nullptr) {
         ret &= cqueue_list_find_hgroup_references(master_cqueue_list, 
                                                   answer_list,
                                                   this_elem, 
                                                   occupants_queues);
      }
   }
   DRETURN(ret);
}

/**
 * @brief hgroup_find_referencees() for an untyped caller
 *
 * @param this_elem HGRP_Type, checked; may be nullptr
 * @param answer_list AN_Type
 * @param master_hgroup_list HGRP_Type master list, checked
 * @param master_cqueue_list CQ_Type master list, checked
 * @param occupants_groups in-out: HR_Type list, or nullptr
 * @param occupants_queues in-out: ST_Type list, or nullptr
 *
 * @return as hgroup_find_referencees()
 */
bool hgroup_find_referencees(const lListElem *this_elem, lList **answer_list, const lList *master_hgroup_list,
                             const lList *master_cqueue_list, lList **occupants_groups,
                             lList **occupants_queues) {
   ocs::UntypedHrefList typed_occupants_groups(occupants_groups);

   return hgroup_find_referencees(ocs::tom::ConstElement<ocs::tom::HGRP>(this_elem), answer_list,
                                  ocs::tom::ConstList<ocs::tom::HGRP>(master_hgroup_list),
                                  ocs::tom::ConstList<ocs::tom::CQ>(master_cqueue_list),
                                  typed_occupants_groups.get(), occupants_queues);
}

/**
 * @brief Do hostgroups really exist
 *
 * This functions returns true if all hostgroups given by the parameter
 * "href_list" exist in "this_list". If one or more objects are missing
 * a corresponding error message will be returned via "answer_list".
 *
 * @param this_list HGRP_Type list, may be an empty handle
 * @param answer_list AN_Type
 * @param href_list HR_Type list, may be an empty handle
 *
 * @return true or false; true if one of the lists is an empty handle
 */
bool hgroup_list_exists(const ocs::tom::ConstList<ocs::tom::HGRP> this_list, lList **answer_list,
                        const ocs::tom::ConstList<ocs::tom::HR> href_list) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (href_list && this_list) {
      for (const auto href : href_list) {
         const char *name = ocs::tom::get<HR_name>(href);

         if (ocs::is_hgroup_name(name)) {
            if (!hgroup_list_locate(this_list, name)) {
               ret = false;
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_DOESNOTEXIST_SS, "host group", name);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               break;
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief hgroup_list_exists() for an untyped caller
 *
 * @param this_list HGRP_Type list, checked; may be nullptr
 * @param answer_list AN_Type
 * @param href_list HR_Type list, checked; may be nullptr
 *
 * @return as hgroup_list_exists()
 */
bool hgroup_list_exists(const lList *this_list, lList **answer_list, const lList *href_list) {
   return hgroup_list_exists(ocs::tom::ConstList<ocs::tom::HGRP>(this_list), answer_list,
                             ocs::tom::ConstList<ocs::tom::HR>(href_list));
}

/**
 * @brief Finds hostnames
 *
 * Selects all hostgroups of "this_list" which match the pattern
 * "hgroup_pattern". All hostnames which are directly or indirectly
 * referenced will be added to "used_hosts", each host once also when it is
 * a member of several matching groups.
 *
 * @param this_list HGRP_Type list, may be an empty handle
 * @param answer_list AN_Type
 * @param hgroup_pattern fnmatch pattern
 * @param used_hosts in-out: HR_Type list, or nullptr
 *
 * @return error state true  - Success false - Error
 */
bool hgroup_list_find_matching_and_resolve(const ocs::tom::ConstList<ocs::tom::HGRP> this_list, lList **answer_list,
                                           const char *hgroup_pattern,
                                           ocs::tom::ListPtr<ocs::tom::HR> *used_hosts) {
   DENTER(HGROUP_LAYER);

   bool ret = true;

   if (this_list && hgroup_pattern != nullptr) {
      const bool hgroup_pattern_is_expression = ocs::is_expression(hgroup_pattern);

      for (const auto hgroup : this_list) {
         const char *hgroup_name = ocs::tom::get<HGRP_name>(hgroup);

         /* use hostgroup expression */
         if (!sge_eval_expression(ocs::CEntry::Type::HOST, hgroup_pattern, hgroup_name, nullptr, true,
                                  hgroup_pattern_is_expression)) {
            ocs::tom::ListPtr<ocs::tom::HR> tmp_used_hosts;

            ret = hgroup_find_all_references(hgroup, nullptr, this_list, &tmp_used_hosts, nullptr);

            // a host may belong to several of the matching groups; it is collected once
            for (const auto tmp_href : *tmp_used_hosts) {
               if (used_hosts != nullptr) {
                  href_list_add(*used_hosts, answer_list, ocs::tom::get<HR_name>(tmp_href));
               }
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief hgroup_list_find_matching_and_resolve() for an untyped caller
 *
 * @param this_list HGRP_Type list, checked; may be nullptr
 * @param answer_list AN_Type
 * @param hgroup_pattern fnmatch pattern
 * @param used_hosts in-out: HR_Type list, or nullptr
 *
 * @return as hgroup_list_find_matching_and_resolve()
 */
bool hgroup_list_find_matching_and_resolve(const lList *this_list, lList **answer_list, const char *hgroup_pattern,
                                           lList **used_hosts) {
   ocs::UntypedHrefList typed_used_hosts(used_hosts);

   return hgroup_list_find_matching_and_resolve(ocs::tom::ConstList<ocs::tom::HGRP>(this_list), answer_list,
                                                hgroup_pattern, typed_used_hosts.get());
}

/**
 * @brief Find hgroups which match pattern
 *
 * Selects all hostgroups of "this_list" which match the pattern
 * "hgroup_pattern". All matching hostgroup names will be added to
 * "href_list"
 *
 * @param this_list HGRP_Type list, may be an empty handle
 * @param answer_list AN_Type list
 * @param hgroup_pattern hostgroup pattern
 * @param[in,out] href_list HR_Type list receiving the matching names, created if needed; or nullptr
 *
 * @return error state true  - success false - error
 */
bool hgroup_list_find_matching(const ocs::tom::ConstList<ocs::tom::HGRP> this_list, lList **answer_list,
                               const char *hgroup_pattern, ocs::tom::ListPtr<ocs::tom::HR> *href_list) {
   DENTER(HGROUP_LAYER);
   bool ret = true;

   if (this_list && hgroup_pattern != nullptr) {
      const bool hgroup_pattern_is_expression = ocs::is_expression(hgroup_pattern);

      for (const auto hgroup : this_list) {
         const char *hgroup_name = ocs::tom::get<HGRP_name>(hgroup);

         /* use hostgroup expression */
         if (!sge_eval_expression(ocs::CEntry::Type::HOST, hgroup_pattern, hgroup_name, nullptr, true,
                                  hgroup_pattern_is_expression)) {
            if (href_list != nullptr) {
               ocs_href_list_emplace(*href_list, hgroup_name);
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief hgroup_list_find_matching() for an untyped caller
 *
 * @param this_list HGRP_Type list, checked; may be nullptr
 * @param answer_list AN_Type list
 * @param hgroup_pattern hostgroup pattern
 * @param[in,out] href_list HR_Type list receiving the matching names, checked; created if needed; or nullptr
 *
 * @return as hgroup_list_find_matching()
 */
bool hgroup_list_find_matching(const lList *this_list, lList **answer_list, const char *hgroup_pattern,
                               lList **href_list) {
   ocs::UntypedHrefList typed_href_list(href_list);

   return hgroup_list_find_matching(ocs::tom::ConstList<ocs::tom::HGRP>(this_list), answer_list, hgroup_pattern,
                                    typed_href_list.get());
}
