#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/*
 * This code was generated from file source/libs/sgeobj/json/HGRP.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Host Group: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of HGRP, Host Group
 *
 * The enumeration is the type of every field number of HGRP, so a parameter
 * of this type takes the field numbers of HGRP only.
 */
enum HGRP_Field {
   /**
    * @brief Name
    *
    * Name of the hostgroup. Always starts with '@', e.g. `@allhosts`.
    */
   HGRP_name = HGRP_LOWERBOUND,
   /**
    * @brief Host List
    *
    * List of hosts and/or other host groups which are referenced by the host group.
    */
   HGRP_host_list,
   /**
    * @brief Cluster Queue List
    *
    * Temporary list of cluster queues referencing a host group
    * while processing creation or modification requests for host groups.
    */
   HGRP_cqueue_list,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   HGRP_joker,
   /**
    * @brief Cached Resolved Host List
    *
    * CS-2451: the fully resolved host list of this host group - every host
    * reachable through the nested references of host_list, flattened.
    *  
    * Maintained by qmaster whenever a host group changes and distributed with
    * the object through the normal event mechanism, so a membership test is a
    * hash lookup on HR_name instead of a walk of the nested tree.
    *  
    * Only valid when cache_version is non-zero - see there.
    */
   HGRP_cached_hosts,
   /**
    * @brief Cache Validity
    *
    * CS-2451: 0 means cached_hosts has not been computed and every consumer
    * must fall back to walking the nested references. Any other value means
    * cached_hosts is authoritative.
    *  
    * This is a PRESENCE FLAG, not a stamp to compare against anything. The
    * field is needed because cull cannot distinguish an empty list from a
    * missing one - both are nullptr - so an empty cached_hosts would otherwise
    * be indistinguishable from an uncomputed one.
    *  
    * Staleness is prevented by construction, not detected here: qmaster
    * recomputes every affected group before sending its events, and the mirror
    * replaces the element wholesale on MOD, so a distributed element is never
    * partially stale. Do not add logic that compares two version values - a
    * consumer has nothing to compare against.
    */
   HGRP_cache_version,
   /**
    * @brief Matching Cache
    *
    * CS-2680: the hosts a matcher of this group has admitted.
    *  
    * The membership test asks the resolved host list first and this cache
    * second. It exists because a host admitted through a matcher need not be
    * in any object of the cluster, so it is never in the resolved list and
    * every one of its requests would otherwise repeat the matcher walk.
    *  
    * Unlike the resolved host list this one does NOT leave the process. It is
    * neither spooled nor packed nor delivered as event payload: a reader
    * asking for all fields receives it empty, and so does an event
    * subscriber, external or one of the qmaster's own mirrors. That is what
    * NO_TRANSFER says, and this field is its first user.
    *  
    * Two reasons, the second being the sharper one. A receiver decides for
    * itself whether it may keep what it had, so anything handed over is
    * waste - and an execution host movement produces one event per
    * referencing group, which multiplies that waste by their number.
    *  
    * Invalidation happens when an event is merged: the cache is carried over
    * to the new element only if the matcher set did not change, and is
    * discarded otherwise. A hit in it confers, so the completeness of that
    * invalidation is a security property rather than a question of
    * performance.
    */
   HGRP_match_cache,
};

namespace ocs::tom {

   struct CQ;
   struct HM;
   struct HR;
   struct VA;

   /**
    * @brief Host Group
    *
    * HGRP_Type elements are used to define groups of hosts. Each group
    * will be identified by a unique name.
    * Each hostgroup might refer to none, one or multiple hosts and/or
    * hostgroups. This object makes it possible to define a network of
    * hostgroups and hosts.
    *  
    *                   --------------
    *                   |            |
    *                   V            | 0:x
    *             ------------- -----|
    *             | HGRP_Type |
    *             ------------- ------------> | hostname |
    *                                0:x      ------------
    *  
    * Example:
    * Following diagram shows a network of 9 hostgroups (A; B; C;
    * D; E; G; H; I). Each of those groups references one host
    * (A -> a; B -> b; C -> c; ...). Additionally some of those
    * hostgroups refer to one (A -> C; B -> C; C -> E; ...) or two
    * hostgroups (E -> F,G; F -> H,I) The connections are all
    * uni-directional, you have to read the diagram from the left
    * to the right.
    *  
    *             -----                           -----
    *             | A | -- a                      | H | -- h
    *             ----- \                       / -----
    *                     -----           -----
    *                     | C | -- c      | F | -- f
    *                     -----         / -----
    *             ----- /       \ -----         \ -----
    *             | B | -- b      | E | -- e      | I | -- i
    *             -----           -----           -----
    *                     ----- /       \ -----
    *                     | D | -- d      | G | -- g
    *                     -----           -----
    * Several functions exist to create such networks and to find
    * certain sets of hosts and hostgroups within such a network:
    * hgroup_find_all_references("E", &answer, master_list, &hosts, &groups)
    *    hosts -> e, f, g, h, i
    *    groups -> F, G, H, I
    *  
    * hgroup_find_referencees("E", &answer, master_list, &groups)
    *    groups -> C, D
    *  
    * hgroup_find_all_referencees("E", &answer, master_list, &groups)
    *    groups -> A, B, C, D
    *  
    * @see hgroup_list_locate()
    * @see hgroup_create()
    * @see hgroup_add_references()
    * @see hgroup_find_all_references()
    * @see hgroup_find_all_referencees()
    * @see hgroup_find_referencees()
    * @see hgroup_list_exists()
    */
   struct HGRP {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {HGRP_name, "HGRP_name", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {HGRP_host_list, "HGRP_host_list", FieldType::List, Flag::Spool},
         {HGRP_cqueue_list, "HGRP_cqueue_list", FieldType::List, Flag::None},
         {HGRP_joker, "HGRP_joker", FieldType::List, Flag::Spool},
         {HGRP_cached_hosts, "HGRP_cached_hosts", FieldType::List, Flag::None},
         {HGRP_cache_version, "HGRP_cache_version", FieldType::Uint32, Flag::None},
         {HGRP_match_cache, "HGRP_match_cache", FieldType::List, Flag::NoTransfer},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of HGRP belongs to HGRP */
   template <>
   struct TypeOf<HGRP_Field> {
      using type = HGRP;   ///< the object type
   };

   /** @brief HGRP_host_list holds elements of HR */
   template <>
   struct SubType<HGRP_host_list> {
      using type = HR;   ///< the sub-type
   };

   /** @brief HGRP_cqueue_list holds elements of CQ */
   template <>
   struct SubType<HGRP_cqueue_list> {
      using type = CQ;   ///< the sub-type
   };

   /** @brief HGRP_joker holds elements of VA */
   template <>
   struct SubType<HGRP_joker> {
      using type = VA;   ///< the sub-type
   };

   /** @brief HGRP_cached_hosts holds elements of HR */
   template <>
   struct SubType<HGRP_cached_hosts> {
      using type = HR;   ///< the sub-type
   };

   /** @brief HGRP_match_cache holds elements of HM */
   template <>
   struct SubType<HGRP_match_cache> {
      using type = HM;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of HGRP for the list library: a synonym for ocs::tom::HGRP::descriptor */
inline constexpr const lDescr *HGRP_Type = ocs::tom::HGRP::descriptor.e;
