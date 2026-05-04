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
 * This code was generated from file source/libs/sgeobj/json/RL.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief RBAC Role
*
* An RBAC Role defines a named and centrally managed collection of Permissions that specifies
* which actions are authorized on which resource types and under which conditions.
* Roles are assigned to users via user access lists (user_list) and may inherit Permissions
* from other Roles through a directed acyclic role hierarchy (parent_role_list).
* Authorization decisions are derived dynamically at runtime from the effective Permission set
* of the Role, including all transitively inherited Permissions, and are enforced by the
* qmaster GDI request handler according to the default-deny principle.
*
*    SGE_STRING(RL_name) - Role Name
*    The unique name of the Role as defined for object_name in sge_types(1).
*    The name is used as argument to the -role command-line switch available for all
*    GCS commands, and may alternatively be specified via the SGE_ROLE environment variable.
*    Three built-in role names are reserved and cannot be deleted: manager, operator, user.
*
*    SGE_BOOL(RL_enabled) - Enabled
*    Controls whether the Role may be used directly to authorize incoming requests.
*    When set to false, requests that specify this Role (via -role, SGE_ROLE, or default_role)
*    are rejected. A disabled Role that appears in another Role's parent_role_list still
*    contributes its Permissions to that child Role via inheritance.
*    This allows base Roles to be defined that are only used through inheritance and never
*    directly. The built-in manager Role cannot be disabled.
*
*    SGE_LIST(RL_user_list) - User List
*    List of GCS user access list names (US_Type) whose members are authorized to use
*    this Role to submit requests, provided the Role is enabled.
*    A user must be a member of at least one of the referenced access lists to assume the Role.
*    If set to NONE (the default), no user is authorized to use the Role.
*    Unlike other configuration objects, Roles do not support an xuser_list parameter.
*    Access must be explicitly granted; it is never implicitly allowed and later revoked.
*
*    SGE_LIST(RL_parent_role_list) - Parent Role List
*    List of Role names (ST_Type) from which this Role inherits all Permissions transitively.
*    Inheritance is transitive: a Role acquires the Permissions of all ancestor Roles in the
*    hierarchy. The Role Hierarchy must form a directed acyclic graph (DAG); cyclic references
*    are not permitted and are rejected during validation.
*    Permissions are merged in depth-first, left-to-right order. The first matching Permission
*    rule encountered during evaluation (starting from the Role itself, then parent Roles in
*    declaration order) determines the authorization outcome.
*    If set to NONE (the default), no Permissions are inherited.
*
*    SGE_STRING(RL_perm_list) - Permission List
*    Comma-separated list of RBAC permission rules that define the access rights granted by
*    this Role. Each rule consists of exactly six colon-separated characteristics:
*      <source_of_request>:<origin_of_request>:<operation_type>:<object_type>:<object_key>:<object_value_constraint>
*    A request is authorized if at least one rule in the list matches all six characteristics.
*    The first matching rule determines the outcome; no further rules are evaluated.
*    The keyword NONE defines an empty rule set that does not match any request.
*    Unlike other configuration objects, Roles do not support a negation (xperm_list);
*    Permissions must be explicitly granted and cannot be implicitly revoked.
*
*    SGE_LIST(RL_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   RL_name = RL_LOWERBOUND,
   RL_enabled,
   RL_user_list,
   RL_parent_role_list,
   RL_perm_list,
   RL_joker
};

LISTDEF(RL_Type)
   SGE_STRING(RL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_BOOL(RL_enabled, CULL_SPOOL)
   SGE_LIST(RL_user_list, US_Type, CULL_SPOOL)
   SGE_LIST(RL_parent_role_list, ST_Type, CULL_SPOOL)
   SGE_STRING(RL_perm_list, CULL_SPOOL)
   SGE_LIST(RL_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(RLN)
   NAME("RL_name")
   NAME("RL_enabled")
   NAME("RL_user_list")
   NAME("RL_parent_role_list")
   NAME("RL_perm_list")
   NAME("RL_joker")
NAMEEND

#define RL_SIZE sizeof(RLN)/sizeof(char *)


