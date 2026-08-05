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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

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
* hgroup_find_references("E", &answer, master_list, &hosts, &groups)
*    hosts -> e
*    groups -> F, G
*  
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
* @see hgroup_list_get_master_list()
* @see hgroup_list_locate()
* @see hgroup_create()
* @see hgroup_add_references()
* @see hgroup_find_all_references()
* @see hgroup_find_references()
* @see hgroup_find_all_referencees()
* @see hgroup_find_referencees()
* @see hgroup_list_exists()
*
*    SGE_HOST(HGRP_name) - Name
*    Name of the hostgroup. Always starts with '@', e.g. '@allhosts'.
*
*    SGE_LIST(HGRP_host_list) - Host List
*    List of hosts and/or other host groups which are referenced by the host group.
*
*    SGE_LIST(HGRP_cqueue_list) - Cluster Queue List
*    Temporary list of cluster queues referencing a host group
*    while processing creation or modification requests for host groups.
*
*    SGE_LIST(HGRP_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*    SGE_LIST(HGRP_cached_hosts) - Cached Resolved Host List
*    CS-2451: the fully resolved host list of this host group - every host
*    reachable through the nested references of host_list, flattened.
*     
*    Maintained by qmaster whenever a host group changes and distributed with
*    the object through the normal event mechanism, so a membership test is a
*    hash lookup on HR_name instead of a walk of the nested tree.
*     
*    Only valid when cache_version is non-zero - see there.
*
*    SGE_ULONG(HGRP_cache_version) - Cache Validity
*    CS-2451: 0 means cached_hosts has not been computed and every consumer
*    must fall back to walking the nested references. Any other value means
*    cached_hosts is authoritative.
*     
*    This is a PRESENCE FLAG, not a stamp to compare against anything. The
*    field is needed because cull cannot distinguish an empty list from a
*    missing one - both are nullptr - so an empty cached_hosts would otherwise
*    be indistinguishable from an uncomputed one.
*     
*    Staleness is prevented by construction, not detected here: qmaster
*    recomputes every affected group before sending its events, and the mirror
*    replaces the element wholesale on MOD, so a distributed element is never
*    partially stale. Do not add logic that compares two version values - a
*    consumer has nothing to compare against.
*
*/

enum {
   HGRP_name = HGRP_LOWERBOUND,
   HGRP_host_list,
   HGRP_cqueue_list,
   HGRP_joker,
   HGRP_cached_hosts,
   HGRP_cache_version
};

LISTDEF(HGRP_Type)
   SGE_HOST(HGRP_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_LIST(HGRP_host_list, HR_Type, CULL_SPOOL)
   SGE_LIST(HGRP_cqueue_list, CQ_Type, CULL_DEFAULT)
   SGE_LIST(HGRP_joker, VA_Type, CULL_SPOOL)
   SGE_LIST(HGRP_cached_hosts, HR_Type, CULL_DEFAULT)
   SGE_ULONG(HGRP_cache_version, CULL_DEFAULT)
LISTEND

NAMEDEF(HGRPN)
   NAME("HGRP_name")
   NAME("HGRP_host_list")
   NAME("HGRP_cqueue_list")
   NAME("HGRP_joker")
   NAME("HGRP_cached_hosts")
   NAME("HGRP_cache_version")
NAMEEND

#define HGRP_SIZE sizeof(HGRPN)/sizeof(char *)


