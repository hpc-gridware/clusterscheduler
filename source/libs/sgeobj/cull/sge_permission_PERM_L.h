#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/PERM.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Object used to request given permissions of user and host.
*
* Object used to request given permissions of user and host.
*
*    SGE_BOOL(PERM_is_manager) - true if manager
*    true if user has manager role
*
*    SGE_BOOL(PERM_is_operator) - true if operator
*    true if user has operator role
*
*    SGE_BOOL(PERM_is_admin_host) - true if admin host
*    true if the specified host is an admin host
*
*    SGE_BOOL(PERM_is_submit_host) - true if submit host
*    true if the specified host is a submit host
*
*    SGE_HOST(PERM_host) - hostname
*    name of the host with the specified permissions
*
*    SGE_STRING(PERM_username) - username
*    username with the specified permissions
*
*/

enum {
   PERM_is_manager = PERM_LOWERBOUND,
   PERM_is_operator,
   PERM_is_admin_host,
   PERM_is_submit_host,
   PERM_host,
   PERM_username
};

LISTDEF(PERM_Type)
   SGE_BOOL(PERM_is_manager, CULL_DEFAULT)
   SGE_BOOL(PERM_is_operator, CULL_DEFAULT)
   SGE_BOOL(PERM_is_admin_host, CULL_DEFAULT)
   SGE_BOOL(PERM_is_submit_host, CULL_DEFAULT)
   SGE_HOST(PERM_host, CULL_DEFAULT)
   SGE_STRING(PERM_username, CULL_DEFAULT)
LISTEND

NAMEDEF(PERMN)
   NAME("PERM_is_manager")
   NAME("PERM_is_operator")
   NAME("PERM_is_admin_host")
   NAME("PERM_is_submit_host")
   NAME("PERM_host")
   NAME("PERM_username")
NAMEEND

#define PERM_SIZE sizeof(PERMN)/sizeof(char *)


